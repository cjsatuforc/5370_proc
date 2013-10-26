/*
 * HPIB
 * Simulates (partially) how the HPIB hardware works so the firmware can run unchanged.
 *
 * TODO
 * 1. For fast mode, check the assumption that the pre-added n1n2 + n0 really always fits in 2 bytes without overflow.
 * 2. Lots of the HPIB commands are untested.
 *
 * BUGS
 * 1. ST9 only outputs "TI=..., " and then stops. Must be waiting for some sort of signal to continue.
 * 
 */

//#define DISABLE_HPIB
//#define HPIB_DECODE

#include "boot.h"
#include "sim.h"
#include "5370.h"
#include "hpib.h"
#include "net.h"
#include "chip.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#ifdef HPIB_SIM_DEBUG
	#define D_HPIB(x) x;
#else
	#define D_HPIB(x)
#endif

#ifdef DISABLE_HPIB
	static bool hpib_remind = FALSE;
#endif

#ifdef HPIB_SIM
	static u1_t hpib_sim(u2_t addr, u1_t data);
#endif

#if defined(HPIB_DECODE) || defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
	static int mread[4], mwrite[4];
	static int initr[4], lastr[4];
	static int initw[4], lastw[4];
	
	static char hpib_regs[8][16] = {
		"R0_data_in", "R1_state", "R2_state", "R3_status", "W0_data_out", "W1_status", "W2_ctrl", "W3_not_used"
	};
#endif

#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)

typedef struct {
	u4_t iCount;
	u2_t rPC;
	u1_t n_irq;
	u4_t irq_masked;
} t_hr_stamp;

typedef struct {
	t_hr_stamp stamp;
	u2_t addr;
	u1_t data;
	u1_t rwn;
	u4_t dup;
} t_hr_buf;

#define NHP 1024

t_hr_buf hr_buf[NHP], *hp = hr_buf;
int nhp = 0;
t_hr_stamp hr_stamp;
u4_t last_iCount = 0;

// stamp the recorded hpib bus cycles with additional info
void hpib_stamp(int write, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
{
	hr_stamp.iCount = iCount;
	hr_stamp.rPC = rPC;
	hr_stamp.n_irq = n_irq;
	hr_stamp.irq_masked = irq_masked;
	
	// record interrupt events
	if (write && n_irq) {
		if (nhp < NHP) {
			hp->stamp = hr_stamp;
			hp++;
			nhp++;
		}
	}
}

#endif

#define TST(lbl) \
	if (i || ((d & (H_##lbl)) != (l & (H_##lbl)))) { \
		printf(#lbl "->%d ", (d & (H_##lbl))? 1:0); \
	}
	
#define SAME(lbl) \
	if (!i && ((d & (H_##lbl)) == (l & (H_##lbl)))) { \
		printf(#lbl "=%d ", (d & (H_##lbl))? 1:0); \
	}
	
#define TST_F(lbl) \
	if (i || ((d & (H_##lbl)) != (l & (H_##lbl)))) { \
		printf(#lbl "->%d ", d & (H_##lbl)); \
	}
	
#define SAME_F(lbl) \
	if (!i && ((d & (H_##lbl)) == (l & (H_##lbl)))) { \
		printf(#lbl "=%d ", d & (H_##lbl)); \
	}
	
#define TSTV() \
	if (i || (d != l)) { \
		printf("0x%02x->0x%02x ", l, d); \
	}

#if defined(HPIB_DECODE) || defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)

static void hpib_rdecode(u2_t addr, u1_t d, u1_t count_dups)
{
	u1_t i, l, same = 0;

	if (count_dups) {
		if (initr[addr] && (d == lastr[addr])) {
			mread[addr]++;
			return;
			//same = 1;
			//goto simple_r;
		}
		
		printf("%04d ", mread[addr]);
		mread[addr] = 0;
	}
	
	if (initr[addr] == 0)
		i = 1;
	else
		i = 0;
	
	initr[addr] = 1;
	l = lastr[addr];
	lastr[addr] = d;

	switch (addr) {
	
	case R3_status:
		printf("R3_status=0x%02x ", d);
		if (same) { printf("n/c "); break; }
		printf("0x%02x->0x%02x ", l, d);
		TST(HRFD_i);
		TST(ATN);
		TST(rmt);
		TST(poll);
		TST(talk);
		TST(listen);
		printf(" | ");
		SAME(HRFD_i);
		SAME(ATN);
		SAME(rmt);
		SAME(poll);
		SAME(talk);
		SAME(listen);
		break;
		
	case R2_state:
		printf("R2_state=0x%02x ", d);
		if (same) { printf("n/c "); break; }
		printf("0x%02x->0x%02x ", l, d);
		TST(Nlisten);
		TST_F(cmdROM);
		printf(" | ");
		SAME(Nlisten);
		SAME_F(cmdROM);
		break;
		
	case R1_state:
		printf("R1_state=0x%02x ", d);
		if (same) { printf("n/c "); break; }
		printf("0x%02x->0x%02x ", l, d);
		TST(IRQ);
		TST(EOI);
		TST(com);
		TST(rmt_qual);
		TST(rdy);
		TST(data);
		printf(" | ");
		SAME(IRQ);
		SAME(EOI);
		SAME(com);
		SAME(rmt_qual);
		SAME(rdy);
		SAME(data);
		break;
		
	case R0_data_in:
		printf("R0_data_in=0x%02x ", d);
		if ((d >= 0x20) && (d <= 0x7e)) {
			printf ("\"%c\" ", d);
		}
		if (same) { printf("n/c "); break; }
		TSTV();
		break;
	}

	return;
}
#endif

u1_t handler_dev_hpib_read(u2_t addr)
{
	u1_t d;

#ifdef DISABLE_HPIB
	if (!hpib_remind) {
		printf("REMINDER: HPIB configured for no real bus transactions\n");
		hpib_remind = TRUE;
	}

	return 0;
#endif

#ifdef HPIB_SIM
	d = hpib_sim(addr, 0);
#else
	d = bus_read(addr);
#endif

#ifdef DEBUG
	if (iDump) {
		printf("[HPIB read @ 0x%x 0x%x]\n", addr, d);
	}
#endif
	
#ifdef HPIB_RECORD
	if (nhp < NHP) {
		if ((nhp > 0) && (addr == (hp-1)->addr) && (d == (hp-1)->data) && ((hp-1)->rwn == 1)) {
			(hp-1)->dup++;
		} else {
			hp->stamp = hr_stamp;
			hp->addr = addr;
			hp->rwn = 1;
			hp->data = d;
			hp->dup = 1;
			hp++;
			nhp++;
		}
	}
	if (nhp == NHP) {
		printf("R NHP %d\n", NHP);
		nhp++;
	}
#endif

#ifdef HPIB_DECODE
	hpib_rdecode(addr, d);
	printf("\n");
#endif

	return d;
}


#if defined(HPIB_DECODE) || defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)

static void hpib_wdecode(u2_t addr, u1_t d, u1_t count_dups)
{
	u1_t i, l, same = 0;

	if (count_dups) {
		if (initw[addr] && (d == lastw[addr])) {
			mwrite[addr]++;
			return;
			//same = 1;
		}
		
		printf("%04d ", mwrite[addr]);
		mwrite[addr] = 0;
	}

	if (initw[addr] == 0)
		i = 1;
	else
		i = 0;
	
	initw[addr] = 1;
	l = lastw[addr];
	lastw[addr] = d;

	switch (addr+4) {

	case W2_ctrl:
		printf("W2_ctrl=0x%02x ", d);
		if (same) { printf("n/c "); break; }
		printf("0x%02x->0x%02x ", l, d);
		TST(rdy_sw);
		TST(EOI_o);
		TST(ren_sw);
		TST(LNMI_ENL);
		TST(LIRQ_ENL);
		TST(ifc_sw);
		printf(" | ");
		SAME(rdy_sw);
		SAME(EOI_o);
		SAME(ren_sw);
		SAME(LNMI_ENL);
		SAME(LIRQ_ENL);
		SAME(ifc_sw);
		break;

	case W1_status:
		printf("W1_status=0x%02x ", d);
		if (same) { printf("n/c "); break; }
		TSTV();
		TST(LSRQ_o);
		printf(" | ");
		SAME(LSRQ_o);
		break;
		
	case W0_data_out:
		printf("W0_data_out=0x%02x ", d);
		if ((d >= 0x20) && (d <= 0x7e)) {
			printf ("\"%c\" ", d);
		}
		if (same) { printf("n/c "); break; }
		TSTV();
		break;

	default:
		printf("[HPIB write @ 0x%x = 0x%x] ", addr, d);
		panic("bad addr");
	}
}
#endif

void handler_dev_hpib_write(u2_t addr, u1_t d)
{

#ifdef DISABLE_HPIB
	return;
#endif

#ifdef DEBUG
	if (iDump) {
		printf("[HPIB write @ 0x%x 0x%x]\n", addr, d);
	}
#endif
	
#ifdef HPIB_SIM
	TEST3_SET();
	hpib_sim(addr + 4, d);
	TEST3_CLR();
	return;
#else
	bus_write(addr, d);
#endif
	
#ifdef HPIB_RECORD
	if (nhp < NHP) {
		if ((nhp > 0) && (addr == (hp-1)->addr) && (d == (hp-1)->data) && ((hp-1)->rwn == 0)) {
			(hp-1)->dup++;
		} else {
			hp->stamp = hr_stamp;
			hp->addr = addr;
			hp->rwn = 0;
			hp->data = d;
			hp->dup = 1;
			hp++;
			nhp++;
		}
	}
	if (nhp == NHP) {
		printf("W NHP %d\n", NHP);
		nhp++;
	}
#endif

#ifdef HPIB_DECODE
	hpib_wdecode(addr, d, 1);
	printf("\n");
#endif

}


#ifdef HPIB_RECORD

void hpib_dump()
{
	t_hr_buf *p;
	
	for (p = hr_buf; p != hp; p++) {
		printf("%07d +%5d @%04x%c #%4d ", p->stamp.iCount, p->stamp.iCount - last_iCount, p->stamp.rPC,
			p->stamp.irq_masked? '*':' ', p->dup);
		last_iCount = p->stamp.iCount;
		if (p->stamp.n_irq) {
			printf("---- IRQ ----\n");
		} else {
			if (p->rwn)
				hpib_rdecode(p->addr, p->data, 0);
			else
				hpib_wdecode(p->addr, p->data, 0);
			
			printf("\n");
		}
	}
}

#endif


#ifdef HPIB_SIM

#ifdef HPIB_SIM_DEBUG
	#define CK_REG(r) \
		if (addr != (r)) { \
			printf("expecting %s not %s\n", hpib_regs[r], hpib_regs[addr]); \
			printf("state %d\n", state); \
			panic("hpib_sim"); \
		}
	
	#define RTN_REG(r, rv) \
		case r: \
		rdata = (rv); \
		if (hps) hpib_rdecode(r, rv, 0); \
		break;
	
	#define RTN_REGx(r, rv, x) \
		case r: \
		rdata = (rv); \
		if (hps) hpib_rdecode(r, rv, 0); \
		x; \
		break;
	
	#define R_REG(rv) \
		if (hps) hpib_rdecode(addr, rv, 0); \
	
	#define W_REG(wv) \
		if (hps) hpib_wdecode(addr-4, wv, 0); \
	
	#define CK_WVAL(r, wv) \
		if (addr != (r)) { \
			printf("expecting %s not %s\n", hpib_regs[r], hpib_regs[addr]); \
			printf("state %d\n", state); \
			panic("hpib_sim"); \
		} \
		if (wdata != (wv)) { \
			printf("state %d\n", state); \
			printf("wrote 0x%02x expecting 0x%02x\n", wdata, wv); \
			panic("hpib_sim"); \
		} \
		if (hps) printf("%s=0x%02x ", hpib_regs[r], wv);
	
	#define CHK_WVAL(v) \
		if (wdata != (v)) { \
			printf("state %d\n", state); \
			printf("wrote 0x%02x expecting 0x%02x\n", wdata, v); \
			panic("hpib_sim"); \
		} \
		if (hps) printf("%s=0x%02x ", hpib_regs[r], v);
	
	#define CHK_WREG(r, v, x) \
		case r: \
		if (wdata != (v)) { \
			printf("%s: %s\n", states[state], hpib_regs[r]); \
			printf("wrote 0x%02x expecting 0x%02x\n", wdata, v); \
			panic("hpib_sim"); \
		} \
		if (hps) hpib_wdecode(r-4, v, 0); \
		x; \
		break;
	
	#define CHK_WREG_EXPR(r, ex, x) \
		case r: \
		if (!(ex)) { \
			printf("%s: %s\n", states[state], hpib_regs[r]); \
			printf("wrote 0x%02x expr failed\n", wdata); \
			panic("hpib_sim"); \
		} \
		if (hps) hpib_wdecode(r-4, ex, 0); \
		x; \
		break;
	
	#define ANY_WREG(r, x) \
		case r: \
		if (hps) hpib_wdecode(r-4, wdata, 0); \
		x; \
		break;
	
	#define UNEXPECT_VAL(r, x) \
		printf("%s: %s\n", states[state], hpib_regs[r]); \
		printf("unexpected data 0x%02x\n", wdata); \
		panic("hpib_sim"); \
		x;
	
	#define BAD_REG() \
		default: \
		printf("\n\%s: %s\n", states[state], hpib_regs[addr]); \
		panic("hpib_sim bad reg"); \
		break;
#else
	#define CK_REG(r) \
		if (addr != (r)) { \
			panic("hpib_sim1"); \
		}
	
	#define RTN_REG(r, rv) \
		case r: \
		rdata = (rv); \
		break;
	
	#define RTN_REGx(r, rv, x) \
		case r: \
		rdata = (rv); \
		x; \
		break;
	
	#define R_REG(rv)
	
	#define W_REG(wv)
	
	#define CK_WVAL(r, wv) \
		if (addr != (r)) { \
			panic("hpib_sim2"); \
		} \
		if (wdata != (wv)) { \
			panic("hpib_sim3"); \
		}
	
	#define CHK_WVAL(v) \
		if (wdata != (v)) { \
			panic("hpib_sim4"); \
		} \
	
	#define CHK_WREG(r, v, x) \
		case r: \
		if (wdata != (v)) { \
			panic("hpib_sim5"); \
		} \
		x; \
		break;
	
	#define CHK_WREG_EXPR(r, ex, x) \
		case r: \
		if (!(ex)) { \
			panic("hpib_sim6"); \
		} \
		x; \
		break;
	
	#define ANY_WREG(r, x) \
		case r: \
		x; \
		break;
	
	#define UNEXPECT_VAL(r, x) \
		panic("hpib_sim7"); \
		x;
	
	#define BAD_REG() \
		default: \
		panic("hpib_sim8"); \
		break;
#endif

typedef enum { S_INIT, S_IDLE, S_LISTEN_INIT, S_LISTEN, S_TALK_INIT, S_TALKING, NSTATES } states_e;

#ifdef HPIB_SIM_DEBUG
	char *states[NSTATES] = { "init", "idle", "listen_init", "listen", "talk_init", "talking" };
#endif


char *hip = "";			// set to point to interface input commands
bool hpib_causeIRQ;		// ask simulator for an IRQ

// show full register accesses else just the HPIB data is sent
//bool hps = TRUE;		
bool hps = FALSE;

static int state;
static bool init;
static int loopct;
static u1_t r3 = 0;
static u1_t irqFF = 0;
static bool input_done;
static bool irq_en, nmi_en;

// for binary mode
static bool binary_mode;
static u1_t tb1 = 0;
static bool fast_mode;

static u1_t hpib_sim(u2_t addr, u1_t wdata)
{
	u1_t rdata = 0;

#ifdef HPIB_SIM_DEBUG
	if (hps) printf("%07d +%4d @%04x%c ",
		hr_stamp.iCount, hr_stamp.iCount - last_iCount, hr_stamp.rPC,
		hr_stamp.irq_masked? '*':' ');
	last_iCount = hr_stamp.iCount;
#endif

	D_HPIB(if (hps) printf("%s ", states[state]));
	
	switch (state) {
	
	case S_INIT:
		if (!init) {
			hpib_fast_binary_init();
			init = TRUE;
		}
		
		switch (addr) {
			RTN_REG(R0_data_in, 0x00);
			RTN_REG(R1_state, H_rdy);
			RTN_REG(R2_state, H_cmdROM);
			RTN_REG(R3_status, H_HRFD_i);
			ANY_WREG(W1_status, ; );
			CHK_WREG(W2_ctrl, 0, ; );
			BAD_REG();
		}

		// simulate startup behavior of hpib card
		if (loopct++ > 20) {
			state = S_IDLE;
		}
		break;
		
	case S_IDLE:
idle:
		assert ((r3 & H_HRFD_i) == 0);

		switch (addr) {

		RTN_REG(R1_state, H_rdy);

		case R3_status:
			if (*hip) {	// LISTEN
				r3 &= ~H_talk;
				r3 |= H_listen | H_rmt;
				rdata = r3;
				R_REG(rdata);
				irqFF = H_IRQ;
				hpib_causeIRQ = TRUE;
				tb1 = 0;
				binary_mode = FALSE;
				fast_mode = FALSE;
				state = S_LISTEN;
			} else {
				rdata = r3;
				R_REG(rdata);
			}
			break;

		case W1_status:
			if (wdata & H_LSRQ_o) {	// TALK
				W_REG(wdata);
				r3 &= ~H_listen;
				r3 |= H_HRFD_i | H_talk | H_rmt;
				state = S_TALK_INIT;
			} else {
				W_REG(wdata);
			}
			break;

		case W2_ctrl:
			irq_en = (wdata & H_LIRQ_ENL)? FALSE:TRUE;
			nmi_en = (wdata & H_LNMI_ENL)? FALSE:TRUE;

			if (wdata == H_ren_sw) {
				//printf("remote button pushed\n");
			} else {
				W_REG(wdata);
			}
			break;

		BAD_REG();
		}
		break;
		
	case S_LISTEN:		// H_HRFD_i=0
		assert (! ((addr == R0_data_in) && input_done));

		switch (addr) {

		case R1_state:
			rdata = H_rdy | irqFF;
			irqFF = 0;

			if (input_done) {
				input_done = FALSE;
				
				if (tb1 == 5) {
					// just finished receiving a "TB1" (goto binary mode)
					// immediately switch to talk
					r3 &= ~H_listen;
					r3 |= H_HRFD_i | H_talk | H_rmt;
					binary_mode = TRUE;
					loopct = 0;
					hip = "";		// terminate current input
					state = S_TALK_INIT;
					goto talk_init;
				} else {
					// wait for talk to be initiated with W1_status=H_LSRQ_o
					state = S_IDLE;
				}
			} else {
				rdata |= H_data;
			}
			R_REG(rdata);
			break;

		case R0_data_in:
			rdata = *hip;
			D_HPIB(if (hps) hpib_rdecode(addr, rdata, 0));

#ifdef HPIB_ECHO_INPUT
			if (hps) {
				printf("[%c] ", *hip);
			} else {
				printf("%c", *hip);
			}
#endif

			switch (*hip) {
			case 'T':
			case 't':
				tb1 = 1;
				break;
			case 'B':
			case 'b':
				if (tb1 == 1) {
					tb1 = 2;
				}
				break;
			case '1':
			case '2':
				if (tb1 == 2) {
					if (rdata == '2') {		// "tb2" is a virtual cmd for fast mode
						rdata = '1';
						fast_mode = TRUE;
					}
					tb1 = 3;
				}
				break;
			case 0xd:
				if (tb1 == 3) {
					tb1 = 4;
				}
				break;
			case 0xa:
				if (tb1 == 4) {
					tb1 = 5;
				}
				if (tb1 == 3) {		// accept \n without preceding \r
					tb1 = 5;
				}
				break;
			default:
				tb1 = 0;
				break;
			}

			hip++;
			if (*hip == 0) {
				input_done = TRUE;
			}
			break;

		case R3_status:
			if (input_done) {
				rdata = r3 | H_HRFD_i;
			} else {
				rdata = r3;
			}
			R_REG(rdata);
			break;

		ANY_WREG(W1_status, ; );

		case W2_ctrl:
			W_REG(wdata);
			irq_en = (wdata & H_LIRQ_ENL)? FALSE:TRUE;
			nmi_en = (wdata & H_LNMI_ENL)? FALSE:TRUE;

			if (wdata == H_LNMI_ENL) {	// XXX but this doesn't always preceed an irq?
				//panic("cirq2?");
				irqFF = H_IRQ;
				hpib_causeIRQ = TRUE;
			}
			break;

		BAD_REG();
		}
		break;
		
	case S_TALK_INIT:	// allows R3_status to read once with H_HRFD_i=1
talk_init:
		assert (r3 == (H_talk | H_HRFD_i | H_rmt));

		switch (addr) {

		RTN_REGx(R3_status, r3, if (hps) printf("TALK: "); r3 &= ~H_HRFD_i; state = S_TALKING; );

		RTN_REGx(R1_state, H_rdy | irqFF, irqFF = 0; );

		ANY_WREG(W1_status, ; );

		case W2_ctrl:
			W_REG(wdata);
			irq_en = (wdata & H_LIRQ_ENL)? FALSE:TRUE;
			nmi_en = (wdata & H_LNMI_ENL)? FALSE:TRUE;

			if (wdata == H_LNMI_ENL) {	// xxx but this doesn't always preceed an irq?
				//panic("cirq?");
				irqFF = H_IRQ;
				hpib_causeIRQ = TRUE;
			}
			break;

		BAD_REG();
		}
		break;
		
	case S_TALKING:		// seq: W0_data_out, irq, R1_state
		assert (r3 == (H_talk | H_rmt));

		switch (addr) {

		case R1_state:
			rdata = H_rdy | irqFF;
			
			// Hack: Only the read of R1 from the interrupt routine resets irqFF because there is a delay
			// delivering the interrupt when using inlined code (must wait for the next branch).
			// Other reads of R1 from non-interrupt code that result from the delay must not
			// prematurely reset irqFF.
			if (!hpib_causeIRQ) irqFF = 0;
			break;

		case W0_data_out:
#ifdef HPIB_ENET_IO
			if (binary_mode) {
	#ifdef HPIB_SIM_DEBUG
				if (HPIB_TELNET_TCP_PORT) {	// send as ascii
					char buf[8];
					sprintf(buf, "%d-", wdata);
					net_send(buf, strlen(buf), FALSE, FALSE);
				} else
	#endif
				{	// HPIB_TCP_PORT, send in binary as usual

					if (fast_mode) {
						u4_t *bp, nb;
						bp = net_send(0, 0, FALSE, FALSE);	// get buffer pointer
	
						while (1) {		// send data as fast as possible
							nb = hpib_fast_binary((s2_t *) bp, HPIB_MEAS_PER_FAST_PKT);		// enough data for a full packet
							bp = net_send((char *) bp, nb, /* no copy */ TRUE, TRUE);
							if (bp == 0) break;
							{
								u4_t now;
								static u4_t last, meas;
								now = sys_now();
								if (time_diff(now, last) >= 1000) {
									printf("%d meas/sec\n", meas);
									last = now;
									meas = 0;
								}
								meas += HPIB_MEAS_PER_FAST_PKT;
							}
						}
					} else {

						// inefficient, but net_send() accumulates data in buffer before sending packet anyway
						net_send((char*) &wdata, 1, FALSE, FALSE);

						u4_t now;
						static u4_t last, meas;
						now = sys_now();
						if (time_diff(now, last) >= 1000) {
							meas /= 5;
							printf("%d meas/sec", meas);
						#ifdef MEAS_IP_HPIB_MEAS
							{
								static u4_t iCount, last_iCount;
								iCount = sim_insn_count();
								printf(", %d insn/meas", (iCount-last_iCount)/meas);
								last_iCount = iCount;
							}
						#endif
							printf("\n");
							last = now;
							meas = 0;
						}
						meas++;
						if (meas & 1) {
							TEST1_SET();
						} else {
							TEST1_CLR();
						}
					}
				}
			} else {
				net_send((char*) &wdata, 1, FALSE, (wdata=='\n')? TRUE:FALSE);
			}
#else
			if (binary_mode) {
				printf("%d-", wdata);
			} else {
				if (hps) printf("W0_data_out=<%c> %02x ", wdata, wdata); else printf("%c", wdata);
			}
#endif
			irqFF = H_IRQ;
			if (irq_en) hpib_causeIRQ = 1;
			break;

		case R3_status:
			if (!binary_mode) {
				state = S_IDLE;
				goto idle;
			}

			// in binary mode from here
#ifdef HPIB_ENET_IO
	#ifdef HPIB_SIM_DEBUG
			if (HPIB_TELNET_TCP_PORT) {
				net_send("\n", 1, FALSE, TRUE);
			} else
	#endif
			{	// HPIB_TCP_PORT
				if (++loopct >= HPIB_MEAS_PER_PKT) {	// accumulate a full packet before sending
					net_send(0, 0, FALSE, TRUE);
					loopct = 0;
				}
			}
#else
			if (!hps) printf("\n");
#endif
			if (*hip) {		// break out of binary mode if new input available (typically a "tb0")
				D_HPIB(printf("exit binary mode receiving: %s", hip));
#ifdef HPIB_ENET_IO
				net_send(0, 0, FALSE, TRUE);		// flush any partial output
#endif
				state = S_IDLE;
				goto idle;
			}

			rdata = r3;
			r3 |= H_HRFD_i;
			state = S_TALK_INIT;
			R_REG(rdata);
			break;

		ANY_WREG(W1_status, ; );

		case W2_ctrl:
			W_REG(wdata);
			irq_en = (wdata & H_LIRQ_ENL)? FALSE:TRUE;
			nmi_en = (wdata & H_LNMI_ENL)? FALSE:TRUE;
			break;

		BAD_REG();
		}
		break;
		
	default:
		D_HPIB(printf("\nstate=%d addr=%d\n", state, addr));
		panic("hpib_sim9");
		break;
	}
	
	D_HPIB(if (hps) printf("\n"));
	return rdata;
}

void hpib_input(char *buf)
{
	hip = buf;
}

#endif
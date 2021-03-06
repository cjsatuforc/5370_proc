#ifndef _SIM_H_
#define _SIM_H_

#include "config.h"

// how many times through the simulator loop before input poll check
// -1 because used as a count compare mask, e.g.
// if ((iCount & SIM_POLL_INPUT_COUNT) == 0) ...
#define SIM_POLL_INPUT_COUNT	(1024-1)

extern u1_t sim_key, sim_key_intr;
extern bool iDump;
extern int iSnap, iTrace;
extern bool background_mode;

#define N_PROFILE	16
extern char conf_profile[N_PROFILE];

void sim_reset();
void sim_init();
void sim_args(bool cmd_line, int argc, char *argv[]);
void sim_main();
void sim_proc_reset();
void sim_processor();
u4_t sim_insn_count();
char *sim_input();

#ifdef DEBUG
 #define	PF(...)		tr_printf(sprintf(tr_s(), __VA_ARGS__))
 #define	PFC(c, ...)	if (c) tr_printf(sprintf(tr_s(), __VA_ARGS__))
#else
 #define	PF(...)		printf(__VA_ARGS__)
 #define	PFC(c, ...)	if (c) printf(__VA_ARGS__)
#endif

void trace(u4_t iCount, u2_t rPC, u2_t nPC, u1_t taken, u1_t IRQ, u1_t IRQs, u1_t a, u1_t b, u2_t x, u2_t sp, u1_t C, u1_t VNZ, u1_t tU8, u2_t tU16);
void trace_iSnap(int _iSnap);
void trace_iDump(int _iDump);
void trace_init();
void trace_dump();
void trace_clear();
void trace_on();
char *tr_s();
void tr_printf(int ignore);

#endif

#ifndef _5370_REGS_H_
#define _5370_REGS_H_

#include "5370.h"

// interpretation of the registers and signals from the manual & schematics
// corrections and improvements welcome
//
// registers are either read-only or write-only
// [signal] is an inter-board signal name called out on the schematics
//
// boards:
// A11 = display
// A16 = arming
// A17 = count chain
// A18 = DAC, N0 logic
//
// on the A17 schematic the address signals driving the muxes are shown as: A0, LEN0, LEN1, LEN2
// the correct labeling should be:  LEN0, LEN1, LEN2, A0


// read-only registers
#define RREG_KEY_SCAN		ADDR_DSP(0)

#define	RREG_LDACSR			ADDR_ARM(0)
#define	DSR_TRIG_LVL_STOP	0x8
#define	DSR_TRIG_LVL_START	0x4
#define	DSR_SPARE			0x2			// loopback of DCW_SPARE
#define	DSR_VOK				0x1			// power supply voltages check

#define	RREG_A16_SWITCHES	ADDR_ARM(2)	// 2,3 A16-U21-U15

#define	RREG_I1				ADDR_ARM(3)
#define I1_IRQ				0x80		// interrupt request
#define I1_RTI_MASK			0x40		// loopback of O1_RTI_MASK
#define I1_RST_TEST			0x20		// loopback of O3_RST_TEST
#define I1_SRATE			0x10		// sample rate
#define	I1_LRMT				0x08		// loopback of O1_LRM_MASK
#define I1_LRTL				0x04		// front pnl rmt/lcl key [rtn to local]
#define I1_MAN_ARM			0x02		// front pnl man arm key [man arm]
#define I1_IO_FLO			0x01		// driven from O2_FLAG and A11 [flag] (wire or)

#define RREG_ST				ADDR_ARM(4)	// 4,5 A17-U9-U11
#define	N0ST_LOVEN			0x80		// oven temp status [loven]
#define	N0ST_HEXT			0x40		// ext freq std [hext]

#define RREG_N0ST			ADDR_ARM(5)
#define N0ST_MASK			0xfc
#define	N0ST_EVT_RNG		0x80		// event counter range (ovfl) flag [hn3or]
#define	N0ST_EVT_RNG_GPIO	BUS_LD7
#define	N0ST_EOM			0x40		// end-of-measurement [lproc]
#define N0ST_EOM_GPIO		BUS_LD6
#define N0ST_N0_POS			0x20		// N0 sign [sign]
#define N0ST_N0_POS_GPIO	BUS_LD5
#define N0ST_ARMED			0x10		// armed flag [ermd]
#define N0ST_PLL_OOL		0x08		// PLL out-of-lock, latched [lool]
#define N0ST_PLL_OOL_GPIO	BUS_LD3
#define N0ST_N0_OVFL		0x04		// N0 overflow
#define N0ST_N0_OVFL_GPIO	BUS_LD2
#define N0ST_N1N2			0x03		// N1N2 bits <17,16>
#define N0ST_N1_GPIO		BUS_LD1
#define N0ST_N2_GPIO		BUS_LD0

// N0 is incorrectly labeled "N3 counter" on A17 schematic
// N3 is used on the schematics as another name for the event counter (see below)
#define RREG_N0H			ADDR_ARM(6)		// 16-bit N0 counter A17-U6-U5
#define RREG_N0L			ADDR_ARM(7)

#define RREG_N1N2H			ADDR_ARM(8)		// N1N2 18-bit signed, A17-U12-U8
#define RREG_N1N2L			ADDR_ARM(9)		// bits <17,16> are in N0ST <1,0>

#define RREG_N3H			ADDR_ARM(0xa)	// 16-bit event counter (AKA N3), A16-U19-U17
#define RREG_N3L			ADDR_ARM(0xb)


// write-only registers
#define WREG_SPARE			ADDR_ARM(0)

#define WREG_LDACCW			ADDR_ARM(1)
#define	DCW_START_DAC_OE	0x80
#define	DCW_STOP_DAC_OE		0x40
#define	DCW_RELAY			0x20
#define	DCW_LOCK_FIX		0x10		// [lock fix]
#define	DCW_SPARE			0x08
#define	DCW_HRMT_SLOPE		0x04		// [hrmt slope]
#define	DCW_FWD_REV_START	0x02
#define	DCW_FWD_REV_STOP	0x01

#define WREG_LDACSTART		ADDR_ARM(2)
#define WREG_LDACSTOP		ADDR_ARM(3)

// most of these are signals going to the A22 arming assembly
#define WREG_O2				ADDR_ARM(4)		// A16-U3-U9
#define O2_FLAG				0x80		// [flag]
#define O2_SRATE_EN			0x40		// sample rate enable
#define O2_HTDGL			0x20		// [htdgl]
#define O2_LGATE			0x10		// [lgate]
#define O2_HARMCT3			0x08		// [larmct3]
#define O2_LARMCT2			0x04		// [larmct2]
#define O2_HMNRM			0x02		// [hmnrm]
#define O2_HRWEN_LRST		0x01		// [hrmen/lrst]

// most of these are signals going to the A22 arming assembly
#define WREG_O1				ADDR_ARM(5)		// A16-U2-U7
#define O1_LRM_MASK			0x80		// [lrm mask]
#define O1_RTI_MASK			0x40		// 1 = enable RTI/IRQ (interrupt)
#define O1_LHLDEN			0x20		// [lhlden]
#define O1_STOPSW			0x10		// [stopsw]
#define O1_STARTSW			0x08		// [startsw]
#define O1_HSET2			0x04		// [hset2]
#define O1_HSET1			0x02		// [hset1]
#define O1_HSTD				0x01		// [hstd]

#define WREG_O3				ADDR_ARM(6)		// A16-U5-U11
#define O3_SELF_CLR			0x80		// clear this reg on next phase-2 clk
#define O3_RST_TEST			0x40		// loopback to I1_RST_TEST
#define O3_LARMRST			0x20		// [larmrst] arm assy master reset (called preset on A22)
#define O3_LOLRST			0x10		// [a16:lpolrst, a17:lolrst] PLL loss-of-lock FF reset
#define O3_CLR_EVT_OVFL		0x08		// clr event ctr ovfl flag on A16, the output of which is [hn3or]
#define O3_LORST			0x04		// [a16:lpor0st, a17:lorst] pulse overrange N0 reset, happens when bit 15 of N0 is set. ovfl cnt kept in software.
#define O3_HN3RST			0x02		// [hn3rst] event ctr (N3) reset A16 and A22
#define O3_LCRST			0x01		// [a16:lpcrst, a17:lcrst] pulse counter reset, resets all ctrs on A17: N0, N1, N2

#endif

#ifndef _CONFIG_H_
#define _CONFIG_H_

// firmware version
#define	FIRMWARE_VER_MAJ	3
#define	FIRMWARE_VER_MIN	0


// these defines are set by the makefile:
// {HP5370A, HP5370B, HP5359A}
// SETUP_*
// CHIP_INCLUDE


// configuration options
//#define BUS_TEST
//#define INTERRUPTS				// real interrupts instead of polling (not fully working yet)
//#define DBG_INTRS
//#define FASTER_INTERRUPTS
#define HPIB_SIM				// simulates the HPIB hardware board so the unmodified firmware can continue to work
#define HPIB_SIM_DEBUG
#define HPIB_ENET_IO			// HPIB i/o redirected to tcp connection
//#define HPIB_ECHO_INPUT
#define HPIB_TELNET_TCP_PORT 0
//#define MEAS_IP_HPIB_MEAS		// measure instructions-per-HPIB-measurement
//#define MEAS_IPS				// measure instructions-interpreted-per-second
//#define RECORD_IO_HIST
//#define HPIB_RECORD			// record HPIB bus cycles for use in developing HPIB_SIM


#ifdef HP5370A
	#define ROM_CODE_H		"rom_5370a.h"
	#define INST_STR		"5370a"
	#define INST_NAME		"HP 5370A Universal Time Interval Counter"
#endif

#ifdef HP5370B
	#define ROM_CODE_H		"rom_5370b.h"
	#define INST_STR		"5370b"
	#define INST_NAME		"HP 5370B Universal Time Interval Counter"
#endif

#ifdef HP5359A
	#define ROM_CODE_H		"rom_5359a.h"
	#define INST_STR		"5359a"
	#define INST_NAME		"HP 5359A Time Synthesizer"
#endif

#endif
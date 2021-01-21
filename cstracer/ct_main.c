
/*--------------------------------------------------------------------*/
/*--- A Valgrind tool to generate traces for ChampSim    ct_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   Copyright (C) 2020 Siddharth Jayashankar

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

// This tool creates instructions traces for use with ChampSim

/*	 Following Taken from Lackey, a sample Valgrind Tool 	*/

// Lackey with --trace-mem gives good traces, but they are not perfect, for
// the following reasons:
//
// - It does not trace into the OS kernel, so system calls and other kernel
//   operations (eg. some scheduling and signal handling code) are ignored.
//
// - It could model loads and stores done at the system call boundary using
//   the pre_mem_read/post_mem_write events.  For example, if you call
//   fstat() you know that the passed in buffer has been written.  But it
//   currently does not do this.
//
// - Valgrind replaces some code (not much) with its own, notably parts of
//   code for scheduling operations and signal handling.  This code is not
//   traced.
//
// - There is no consideration of virtual-to-physical address mapping.
//   This may not matter for many purposes.
//
// - Valgrind modifies the instruction stream in some very minor ways.  For
//   example, on x86 the bts, btc, btr instructions are incorrectly
//   considered to always touch memory (this is a consequence of these
//   instructions being very difficult to simulate).
//
// - Valgrind tools layout memory differently to normal programs, so the
//   addresses you get will not be typical.  Thus Lackey (and all Valgrind
//   tools) is suitable for getting relative memory traces -- eg. if you
//   want to analyse locality of memory accesses -- but is not good if
//   absolute addresses are important.
//
// Despite all these warnings, Lackey's results should be good enough for a
// wide range of purposes.  For example, Cachegrind shares all the above
// shortcomings and it is still useful.
//
// For further inspiration, you should look at cachegrind/cg_main.c which
// uses the same basic technique for tracing memory accesses, but also groups
// events together for processing into twos and threes so that fewer C calls
// are made and things run faster.
//


// undef this to generate traces for the normal form of ChampSim
#define TRACE_MEM_VALUES


// Set to 1 to for for debugging
#define DEBUG_CT 0
#define PRINT_INST 0
#define PRINT_ERROR 1


#include "pub_tool_basics.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_machine.h" // VG_(fnptr_to_fnentry)
#include "pub_tool_options.h"
#include "pub_tool_threadstate.h" // VG_(get_running_tid)()
#include "pub_tool_tooliface.h"

#if defined(VGP_arm64_linux)
#include "arm64regs.h" //contains the registers
#else
#include "x86-64regs.h" //contains the register enum
#endif

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>


/*------------------------------------------------------------*/
/*--- Command line options                                 ---*/
/*------------------------------------------------------------*/

// Trace file name --trace-file=tracefile
static const HChar *t_fname = "tracefile";

// Num of Instructions to skip --skip=
static unsigned long long int skip = 0;

// Num of Instructions to trace --trace=
static unsigned long long int trace_instrs = 1000;

// Print superblock Info
static Bool trace_superblocks = False;


// Print superblock Info
static Bool exit_after_tracing = True;

static Bool ct_process_cmd_line_option(const HChar *arg) {
	if
		VG_STR_CLO(arg, "--trace-file", t_fname) {}
	else if
		VG_INT_CLO(arg, "--skip", skip) {}
	else if
		VG_INT_CLO(arg, "--trace", trace_instrs) {}
	else if
		VG_BOOL_CLO(arg, "--superblocks", trace_superblocks) {}
	else if
		VG_BOOL_CLO(arg, "--exit-after", exit_after_tracing) {}
	else
		return False;

	tl_assert(t_fname);
	tl_assert(t_fname[0]);
	return True;
}

static void ct_print_usage(void) {
	VG_(printf)
	("    --trace-file=<file>        Trace File Name\n"
	 "    --trace=<num>        	Number of Instructions to Trace\n"
	 "    --skip=<num>        	Number of Instructions to Skip\n"
	 "    --superblocks=<yes|no> Print Superblock Information\n"
	 "    --exit-after=<yes|no> Exit after tracing completes\n");
}

static void ct_print_debug_usage(void) { VG_(printf)(" (none)\n"); }

/*------------------------------------------------------------*/
/*--- Tracing                                              ---*/
/*------------------------------------------------------------*/

static Bool tracing	= False;
static Bool tracing_done = False;

static unsigned long long int superblocks  = 0;
static unsigned long long int instructions = 0;

static int fd;
static UInt pid;
typedef IRExpr IRAtom;

#define MAX_DSIZE 512

/* For ChampSim Traces*/
#define NUM_INSTR_DESTINATIONS 4
#define NUM_INSTR_SOURCES 4
#define CACHE_POW 6
#define CACHE_LINE_SIZE 64

typedef struct {
	uint64_t encode_key;
	uint64_t ip; // instruction pointer (program counter) value

#ifdef TRACE_MEM_VALUES
	uint32_t is_branch;	// is this branch
	uint32_t branch_taken; // if so, is this taken
#endif

#ifndef TRACE_MEM_VALUES
	uint32_t is_branch;	// is this branch
	uint32_t branch_taken; // if so, is this taken
#endif

	uint8_t destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
	uint8_t source_registers[NUM_INSTR_SOURCES];		   // input registers

	uint64_t destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
	uint64_t source_memory[NUM_INSTR_SOURCES];			 // input memory

#ifdef TRACE_MEM_VALUES
	uint8_t d_valid[NUM_INSTR_DESTINATIONS];
	uint8_t d_value[NUM_INSTR_DESTINATIONS]
				   [CACHE_LINE_SIZE]; // data in cache block
									  // to which store took place
	uint8_t s_valid[NUM_INSTR_SOURCES];
	uint8_t s_value[NUM_INSTR_SOURCES][CACHE_LINE_SIZE]; // data in cache block
#endif
} trace_instr_format_t;

static trace_instr_format_t inst;


static VG_REGPARM(1) void trace_superblock(Addr addr) {

	// VG_(printf)("cstracer: SB %llu : Addr %08lx | Ins : %llu\n", superblocks,
	// addr, instructions);
	VG_(printf)
	("==%u== cstracer: Addr %08lx | Ins : %llu\n", pid, addr, instructions);
	superblocks++;
}

static VG_REGPARM(2) void trace_instr(Addr iaddr, SizeT size) {
	if (!tracing)
		return;
	inst.ip = iaddr;
	if (DEBUG_CT) {
		VG_(printf)("I  %08lx,%lu\n", iaddr, size);
	}
}

static VG_REGPARM(2) void trace_load(Addr addr, SizeT size) {
	if (!tracing)
		return;
	Int already_found = 0;
	for (Int i = 0; i < NUM_INSTR_SOURCES; i++) {
		if (inst.source_memory[i] == ((uint64_t)addr)) {
			already_found = 1;
			break;
		}
	}
	if (already_found == 0) {
		for (int i = 0; i < NUM_INSTR_SOURCES; i++) {
			if (inst.source_memory[i] == 0) {
				inst.source_memory[i] = (uint64_t)addr;

#ifdef TRACE_MEM_VALUES
				char *a	= (char *)((addr >> CACHE_POW) << CACHE_POW);
				inst.s_valid[i] = 1;
				for (Int j = 0; j < CACHE_LINE_SIZE; j++) {
					inst.s_value[i][j] = (uint8_t)a[j];
				}
#endif
				if (DEBUG_CT) {
					VG_(printf)(" Load %08lx : ", addr);

#ifdef TRACE_MEM_VALUES
					for (Int j = 0; j < CACHE_LINE_SIZE; j++) {
						VG_(printf)("%d ", (uint8_t)a[j]);
					}
#endif
					VG_(printf)("\n");
				}
				break;
			}
		}
	}
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size) {
	if (!tracing) { return; }
	Int already_found = 0;
	for (Int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
		if (inst.destination_memory[i] == ((uint64_t)addr)) {
			already_found = 1;
			break;
		}
	}
	if (already_found == 0) {
		for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
			if (inst.destination_memory[i] == 0) {
				inst.destination_memory[i] = (uint64_t)addr;

#ifdef TRACE_MEM_VALUES
				char *a			= (char *)((addr >> CACHE_POW) << CACHE_POW);
				inst.d_valid[i] = 1;
				for (Int j = 0; j < CACHE_LINE_SIZE; j++) {
					inst.d_value[i][j] = (uint8_t)a[j];
				}
#endif
				break;

				if (DEBUG_CT) {
					VG_(printf)(" Load %08lx : ", addr);
#ifdef TRACE_MEM_VALUES
					for (Int j = 0; j < CACHE_LINE_SIZE; j++) {
						VG_(printf)("%d ", (uint8_t)a[j]);
					}
#endif
					VG_(printf)("\n");
				}
			}
		}
	}
}

static VG_REGPARM(1) void trace_reg_read(Int r) {
	if (!tracing) { return; }
	Int already_found = 0;
	for (Int i = 0; i < NUM_INSTR_SOURCES; i++) {
		if (inst.source_registers[i] == ((unsigned char)r)) {
			already_found = 1;
			break;
		}
	}
	if (already_found == 0) {
		for (int i = 0; i < NUM_INSTR_SOURCES; i++) {
			if (inst.source_registers[i] == 0) {
				inst.source_registers[i] = (unsigned char)r;
				break;
			}
		}
	}
	if (DEBUG_CT) {
		VG_(printf)(" RegRead %d\n", r);
	}
}

static VG_REGPARM(1) void trace_reg_write(Int r) {
	if (!tracing) { return; }
	Int already_found = 0;
	for (Int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
		if (inst.destination_registers[i] == ((unsigned char)r)) {
			already_found = 1;
			break;
		}
	}
	if (already_found == 0) {
		for (int i = 0; i < NUM_INSTR_SOURCES; i++) {
			if (inst.destination_registers[i] == 0) {
				inst.destination_registers[i] = (unsigned char)r;
				break;
			}
		}
	}
	if (DEBUG_CT) {
		VG_(printf)(" RegWrite %d\n", r);
	}
}

static VG_REGPARM(1) void trace_branch_conditional(Bool ci, Bool guard) {
	if (!tracing) { return; }
	inst.is_branch = 1;

	/* Hack : Valgrind doesn't support implicit register reading, 		*
	 * so we manually specify the registers to make it compatible 		*
	 * with ChampSim's branch logic. This may not always be exact,		*
	 * but it works good enough. */
#if defined(VGP_arm64_linux)
	inst.destination_registers[0] = REG_PC;
	inst.source_registers[0]	  = REG_PC;
	/*All conditional branches don't read the flag instructions,
	 * This has been done as of now for simplicity */
	inst.source_registers[1] = REG_FLAGS;

#else
	inst.destination_registers[0] = REG_RIP;
	inst.source_registers[0]	  = REG_RIP;
	inst.source_registers[1]	  = REG_RFLAGS;
#endif
	if (guard)
		inst.branch_taken = !ci;
	else
		inst.branch_taken = ci;
	if (DEBUG_CT) {
		if (guard)
			VG_(printf)(" Branch Taken\n");
		else
			VG_(printf)(" Branch NotTaken\n");
	}
}

static VG_REGPARM(1) void trace_branch_direct(IRJumpKind jk) {
	if (!tracing) { return; }

	if (DEBUG_CT) {
		VG_(printf)(" Direct Branch\n");
	}
	inst.is_branch	= 1;
	inst.branch_taken = 1;
	/* Hack : See note above */
	if (jk == Ijk_Call) {
#if defined(VGP_arm64_linux)
		inst.destination_registers[0] = REG_PC;
		inst.source_registers[0]	  = REG_PC;
#else
		 inst.destination_registers[0] = REG_RIP;
		 inst.destination_registers[1] = REG_RSP;
		 inst.source_registers[0]	  = REG_RIP;
		 inst.source_registers[1]	  = REG_RSP;
#endif
		if (DEBUG_CT) {
			VG_(printf)(" Direct Call\n");
		}
	} else if (jk == Ijk_Ret) {
#if defined(VGP_arm64_linux)
		// VG_(printf)(" We shouldn't be here - Direct Ijk_Ret\n");
		inst.destination_registers[0] = REG_PC;
		inst.destination_registers[1] = REG_XSP;
		inst.source_registers[0]	  = REG_X30;
#else
		 inst.destination_registers[0] = REG_RIP;
		 inst.destination_registers[1] = REG_RSP;
		 inst.source_registers[0]	  = REG_RSP;
#endif
		if (DEBUG_CT) {
			VG_(printf)(" Return\n");
		}
	} else if (jk == Ijk_Boring) {
#if defined(VGP_arm64_linux)
		inst.destination_registers[0] = REG_PC;
#else
		 inst.destination_registers[0] = REG_RIP;
#endif
		if (DEBUG_CT) {
			VG_(printf)(" Jump \n");
		}
	} else {
		/* Shouldn't be possible to reach here */
		tl_assert(0);
	}
}

static VG_REGPARM(1) void trace_branch_indirect(IRJumpKind jk) {
	if (!tracing) { return; }

	inst.is_branch	= 1;
	inst.branch_taken = 1;
	if (DEBUG_CT) {
		VG_(printf)(" Indirect Branch\n");
	}
	/* Hack : See note above */
	if (jk == Ijk_Call) {
#if defined(VGP_arm64_linux)
		inst.destination_registers[0] = REG_PC;
		inst.destination_registers[1] = REG_X30;
		inst.source_registers[0]	  = REG_PC;
		/* It could read from any register, we always mark it as REG_X30 for
		 * simplicity */
		inst.source_registers[2] = REG_X30;
#else
		 inst.destination_registers[0] = REG_RIP;
		 inst.destination_registers[1] = REG_RSP;
		 inst.source_registers[0]	  = REG_RIP;
		 inst.source_registers[1]	  = REG_RSP;
		 inst.source_registers[2]	  = REG_RAX;
#endif
		if (DEBUG_CT) {
			VG_(printf)(" Indirect Call\n");
		}
	} else if (jk == Ijk_Ret) {
#if defined(VGP_arm64_linux)
		inst.destination_registers[0] = REG_PC;
		inst.source_registers[0]	  = REG_X30;
#else
		 inst.destination_registers[0] = REG_RIP;
		 inst.destination_registers[1] = REG_RSP;
		 inst.source_registers[0]	  = REG_RSP;
		 inst.source_registers[2]	  = REG_RAX;
#endif
		if (DEBUG_CT) {
			VG_(printf)(" Indirect Return\n");
		}
	} else if (jk == Ijk_Boring) {
#if defined(VGP_arm64_linux)
		inst.destination_registers[0] = REG_PC;
		inst.source_registers[0]	  = REG_X30;
#else
		 inst.destination_registers[0] = REG_RIP;
		 inst.source_registers[2]	  = REG_RAX;
#endif
		if (DEBUG_CT) {
			VG_(printf)(" Indirect Branch \n");
		}
	} else {
		/* Shouldn't be possible to reach here */
		tl_assert(0);
	}
}

/* Set the inst structure to zero*/
static VG_REGPARM(0) void zero_inst(void) {
	if (!tracing) { return; }
	inst.ip			  = 0;
	inst.is_branch	= 0;
	inst.branch_taken = 0;
	for (Int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
		inst.destination_registers[i] = 0;
		inst.destination_memory[i]	= 0;
#ifdef TRACE_MEM_VALUES
		inst.d_valid[i] = 0;
		for (Int j = 0; j < CACHE_LINE_SIZE; j++) {

			inst.d_value[i][j] = 0;
		}
#endif
	}

	for (Int i = 0; i < NUM_INSTR_SOURCES; i++) {
		inst.source_registers[i] = 0;
		inst.source_memory[i]	= 0;

#ifdef TRACE_MEM_VALUES
		inst.s_valid[i] = 0;
		for (Int j = 0; j < CACHE_LINE_SIZE; j++) {
			inst.s_value[i][j] = 0;
		}
#endif
	}
}

static VG_REGPARM(0) void write_inst_to_file(void) {
	if (!tracing) { return; }
	/* Don't Print Empty Instruction*/
	if (inst.ip == 0)
		return;
	uint8_t buffer[1152];
	uint32_t index  = 0;
	inst.encode_key = 0;
	VG_(memcpy)(buffer + index, &inst, 32);
	index += 32;
	for (int i = 0; i < 4; i++) {
		if (inst.d_valid[i]) {
			VG_(memcpy)(buffer + index, &inst.destination_memory[i], 8);
			index += 8;
			VG_(memcpy)(buffer + index, &inst.d_value[i], 64);
			index += 64;
			inst.encode_key += ((0xfULL) << (32 + 4 * i));
		}
	}
	for (int i = 0; i < 4; i++) {
		if (inst.s_valid[i]) {
			VG_(memcpy)(buffer + index, &inst.source_memory[i], 8);
			index += 8;
			VG_(memcpy)(buffer + index, &inst.s_value[i], 64);
			index += 64;
			inst.encode_key += ((0xfULL) << (48 + 4 * i));
		}
	}
	inst.encode_key = (((index - 8) & 0xffffffffULL) | inst.encode_key);
	VG_(memcpy)(buffer, &inst.encode_key, 8);
	if (tracing) {
		VG_(write)(fd, buffer, index);
	}
}

static VG_REGPARM(0) void print_inst(void) {

	if (!PRINT_INST)
		return;

	if (!tracing) { return; }
	/* Don't Print Empty Instruction*/
	if (inst.ip == 0)
		return;

	VG_(printf)("INSTR :");
	VG_(printf)(" %08llx :", inst.ip);
	VG_(printf)(" %d :", inst.is_branch);
	VG_(printf)(" %d :", inst.branch_taken);
	for (Int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
		VG_(printf)(" %d :", inst.destination_registers[i]);
	}
	for (Int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
		VG_(printf)(" %08llx :", inst.destination_memory[i]);
#ifdef TRACE_MEM_VALUES
		/*if( inst.destination_memory[i] != 0) {
			for(Int j = 0; j < CACHE_LINE_SIZE; j++)
			{
				VG_(printf)(" %u-%u",inst.d_valid[i][j],inst.d_value[i][j]);
			}
			VG_(printf)(" :");
		}*/
#endif
	}

	for (Int i = 0; i < NUM_INSTR_SOURCES; i++) {
		VG_(printf)(" %d :", inst.source_registers[i]);
	}
	for (Int i = 0; i < NUM_INSTR_SOURCES; i++) {
		VG_(printf)(" %08llx :", inst.source_memory[i]);
#ifdef TRACE_MEM_VALUES
		/*if( inst.source_memory[i] != 0) {
			for(Int j = 0; j < CACHE_LINE_SIZE; j++)
			{
				VG_(printf)(" %u-%u",inst.s_valid[i][j],inst.s_value[i][j]);
			}
			VG_(printf)(" :");
		}*/
#endif
	}

	VG_(printf)("\n");
}


static VG_REGPARM(0) void inc_inst(void) {
	//if (!tracing) { return; }
	instructions++;
	if (instructions == skip) {
		tracing = True;
		VG_(printf)
		("==%u== cstracer: Skipped %llu instructions\n", pid, instructions);
		VG_(printf)("==%u== cstracer: Starting Tracing\n", pid);
	}
	if (instructions == (skip + trace_instrs + 1)) {
		tracing = False;
		if (!tracing_done) {
			/* end tracing */
			tracing_done = True;
			VG_(printf)("==%u== cstracer: Tracing Completed\n", pid);
			VG_(printf)
			("==%u== cstracer: Instructions = %llu\n", pid, instructions - 1);

			VG_(close)(fd);

			/* Valgrind is slow at executing the program, 	*
			 * so we don't run the program to completion		*
			 * and exit once tracing is done to save time.	*/
			if (exit_after_tracing) {
				VG_(printf)("==%u== cstracer: Halting Execution\n", pid);
				VG_(printf)("==%u== cstracer: Bye!\n", pid);
				VG_(exit)(0);
			}
		}
	}
}


static void instrument_superblock(IRSB *sb, IRAtom *addr) {

	if (trace_superblocks) {

		IRDirty *di;
		di = unsafeIRDirty_0_N(0, "trace_superblock",
							   VG_(fnptr_to_fnentry)(&trace_superblock),
							   mkIRExprVec_1(mkIRExpr_HWord((HWord)addr)));
		addStmtToIRSB(sb, IRStmt_Dirty(di));
	}
}


static void instrument_instruction(IRSB *sb, IRAtom *iaddr, UInt isize) {

	tl_assert((VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB) ||
			  VG_CLREQ_SZB == isize);
	IRExpr **argv;
	IRDirty *di;

	di = unsafeIRDirty_0_N(0, "inc_inst", VG_(fnptr_to_fnentry)(inc_inst),
						   mkIRExprVec_0());
	addStmtToIRSB(sb, IRStmt_Dirty(di));

	di = unsafeIRDirty_0_N(0, "write_inst",
						   VG_(fnptr_to_fnentry)(write_inst_to_file),
						   mkIRExprVec_0());
	addStmtToIRSB(sb, IRStmt_Dirty(di));

	//	di = unsafeIRDirty_0_N( 0, "print_inst",
	//			VG_(fnptr_to_fnentry)( print_inst ),
	//			mkIRExprVec_0() );
	//	addStmtToIRSB( sb, IRStmt_Dirty(di) );

	di = unsafeIRDirty_0_N(0, "zero_inst", VG_(fnptr_to_fnentry)(zero_inst),
						   mkIRExprVec_0());
	addStmtToIRSB(sb, IRStmt_Dirty(di));

	argv = mkIRExprVec_2(iaddr, mkIRExpr_HWord(isize));
	di = unsafeIRDirty_0_N(2, "trace_instr", VG_(fnptr_to_fnentry)(trace_instr),
						   argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static IRExpr *cache_block_addr(const IRAtom *a) {
	tl_assert(isIRAtom(a));
	if (a->tag == Iex_RdTmp) {
		return mkIRExpr_HWord((a->Iex.RdTmp.tmp >> CACHE_POW) << CACHE_POW);
	} else if (a->tag == Iex_Const) {
		Addr ad = (sizeof(Addr) == 4) ? a->Iex.Const.con->Ico.U32
									  : a->Iex.Const.con->Ico.U64;
		return mkIRExpr_HWord((ad >> CACHE_POW) << CACHE_POW);
	} else {
		tl_assert(0);
		return NULL;
	}
}

static void instrument_load(IRSB *sb, IRAtom *daddr, Int dsize, IRAtom *guard) {
	tl_assert(isIRAtom(daddr));
	tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

	IRDirty *di_mem = emptyIRDirty();
#ifdef TRACE_MEM_VALUES
	di_mem->mFx   = Ifx_Read;
	di_mem->mAddr = cache_block_addr(daddr);
	di_mem->mSize = CACHE_LINE_SIZE;
#endif
	IRExpr **argv_mem = mkIRExprVec_2(daddr, mkIRExpr_HWord(dsize));
	di_mem->args	  = argv_mem;
	di_mem->cee =
		mkIRCallee(2, "trace_load", VG_(fnptr_to_fnentry)(trace_load));
	di_mem->guard = IRExpr_Const(IRConst_U1(True));

	// Predicated Load. Track all loads if commented
	/*
 if (guard) {
	  di_mem->guard = guard;
 }*/
	addStmtToIRSB(sb, IRStmt_Dirty(di_mem));
}

static void instrument_store(IRSB *sb, IRAtom *daddr, Int dsize,
							 IRAtom *guard) {
	tl_assert(isIRAtom(daddr));
	tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

	IRDirty *di_mem = emptyIRDirty();
#ifdef TRACE_MEM_VALUES
	di_mem->mFx   = Ifx_Read;
	di_mem->mAddr = cache_block_addr(daddr);
	di_mem->mSize = CACHE_LINE_SIZE;
#endif
	IRExpr **argv_mem = mkIRExprVec_2(daddr, mkIRExpr_HWord(dsize));
	di_mem->args	  = argv_mem;
	di_mem->cee =
		mkIRCallee(2, "trace_store", VG_(fnptr_to_fnentry)(trace_store));
	di_mem->guard = IRExpr_Const(IRConst_U1(True));

	if (guard) {
		di_mem->guard = guard;
	}

	addStmtToIRSB(sb, IRStmt_Dirty(di_mem));
}


#if defined(VGP_arm64_linux)
static ARM64_REG offset_to_arm64_register(Int offset) {

	tl_assert(offset >= 0);

	if (DEBUG_CT) {

		VG_(printf)(" REGISTER_OFFSET : Register Offset =  %d\n", offset);
	}

	if (offset < 16) {
		/* Pseudo Registers Used by Valgrind, we don't trace these*/
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	}

	else if (offset < 256) {
		return REG_X0 + (offset - 16) / 8;
	} else if (offset < 264) {
		return REG_X30;
	} else if (offset < 272) {
		return REG_XSP;
	} else if (offset < 280) {
		return REG_PC;
	} else if (offset < 312) {
		return REG_FLAGS;
	}

	else if (offset < 320) {
		/* User Space Thread Register */
		return REG_UTHRD;
	}

	else if (offset < 832) {
		return (offset - 312) / 16 + REG_Q0;
	}

	else if (offset < 848) {
		return REG_QFLAGS;
	}

	/* Pseudo Registers Used by Valgrind, we don't trace these*/
	else if (offset < 896) {
		return 0;
	} else {

		VG_(printf)(" ERROR:Register Offset =  %d\n", offset);
		tl_assert(0);
		return -1;
	}

	VG_(printf)(" ERROR:Register Offset =  %d\n", offset);
	tl_assert(0);
	return -1;
}

#elif defined(VGP_x86_linux) 

static PIN_REG offset_to_x86_64_register(Int offset, Int sz) {
	tl_assert(offset >= 0);
	return 0;
}

#else

/* TODO This function is incomplete for now. Needs to be enhanced to support all
 * registers */
static PIN_REG offset_to_x86_64_register(Int offset, Int sz) {
	tl_assert(offset >= 0);

	if (offset < 16) {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	} else if (offset < 208) {
		switch ((offset - 16) / 8) {
		case 0:
			return REG_RAX;
		case 1:
			return REG_RCX;
		case 2:
			return REG_RDX;
		case 3:
			return REG_RBX;
		case 4:
			return REG_RSP;
		case 5:
			return REG_RBP;
		case 6:
			return REG_RSI;
		case 7:
			return REG_RDI;
		case 8:
			return REG_R8;
		case 9:
			return REG_R9;
		case 10:
			return REG_R10;
		case 11:
			return REG_R11;
		case 12:
			return REG_R12;
		case 13:
			return REG_R13;
		case 14:
			return REG_R14;
		case 15:
			return REG_R15;
		}
	} else if (offset < 176) {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	} else if (offset < 184) {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 25;
	} else if (offset < 192) {
		return REG_RIP;
	} else if (offset < 200) {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	} else if (offset < 208) {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	} else if (offset < 216) {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	} else if (offset < 728) {
		switch ((offset - 216) / 32) {
		case 0:
			return REG_YMM0;
			break;
		case 1:
			return REG_YMM1;
			break;
		case 2:
			return REG_YMM2;
			break;
		case 3:
			return REG_YMM3;
			break;
		case 4:
			return REG_YMM4;
			break;
		case 5:
			return REG_YMM5;
			break;
		case 6:
			return REG_YMM6;
			break;
		case 7:
			return REG_YMM7;
			break;
		case 8:
			return REG_YMM8;
			break;
		case 9:
			return REG_YMM9;
			break;
		case 10:
			return REG_YMM10;
			break;
		case 11:
			return REG_YMM11;
			break;
		case 12:
			return REG_YMM12;
			break;
		case 13:
			return REG_YMM13;
			break;
		case 14:
			return REG_YMM14;
			break;
		case 15:
			return REG_YMM15;
			break;
		case 16:
			VG_(printf)("ASSERT : Fake register YMM16\n");
			tl_assert(0);
		default:
			VG_(printf)("ASSERT : Something is Terribly Wrong\n");
			tl_assert(0);
		}
		return 0;
	} else {
		if (PRINT_ERROR) {
			VG_(printf)(" ERROR:Register Not Yet supported %d\n", offset);
		}
		return 0;
	}

	return 0;
}

#endif

static void instrument_reg_read(IRSB *sb, Int offset, Int sz) {
	int p;
#if defined(VGP_arm64_linux)
	p = offset_to_arm64_register(offset);
	if (p == 0 || p == REG_PC)
		return;
#else
	p = offset_to_x86_64_register(offset, sz);
#endif

	IRExpr **argv = mkIRExprVec_1(mkIRExpr_HWord(p));
	IRDirty *di   = unsafeIRDirty_0_N(
		  1, "trace_reg_read", VG_(fnptr_to_fnentry)(trace_reg_read), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static void instrument_reg_write(IRSB *sb, Int offset, Int sz) {
	int p;
#if defined(VGP_arm64_linux)
	p = offset_to_arm64_register(offset);
	if (p == 0 || p == REG_PC)
		return;
#else
	p = offset_to_x86_64_register(offset, sz);
#endif
	IRExpr **argv = mkIRExprVec_1(mkIRExpr_HWord(p));
	IRDirty *di   = unsafeIRDirty_0_N(
		  1, "trace_reg_write", VG_(fnptr_to_fnentry)(trace_reg_write), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static void instrument_branch_conditional(IRSB *sb, Bool ci, IRExpr *guard) {
	IRType hWordTy = integerIRTypeOfSize(sizeof(Addr));
	IRTemp guard1  = newIRTemp(sb->tyenv, Ity_I1);
	IRTemp guardW  = newIRTemp(sb->tyenv, hWordTy);
	IROp widen	 = hWordTy == Ity_I32 ? Iop_1Uto32 : Iop_1Sto64;

	addStmtToIRSB(sb, IRStmt_WrTmp(guard1, guard));
	addStmtToIRSB(
		sb, IRStmt_WrTmp(guardW, IRExpr_Unop(widen, IRExpr_RdTmp(guard1))));

	IRAtom *guard2 = IRExpr_RdTmp(guardW);
	tl_assert(isIRAtom(guard2));
	IRExpr **argv = mkIRExprVec_2(mkIRExpr_HWord(ci), guard2);
	IRDirty *di   = unsafeIRDirty_0_N(
		  2, "trace_branch_conditional",
		  VG_(fnptr_to_fnentry)(trace_branch_conditional), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static void instrument_branch_direct(IRSB *sb, IRJumpKind jk) {

	IRExpr **argv = mkIRExprVec_1(mkIRExpr_HWord(jk));
	IRDirty *di =
		unsafeIRDirty_0_N(1, "trace_branch_direct",
						  VG_(fnptr_to_fnentry)(trace_branch_direct), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static void instrument_branch_indirect(IRSB *sb, IRJumpKind jk) {
	IRExpr **argv = mkIRExprVec_1(mkIRExpr_HWord(jk));
	IRDirty *di =
		unsafeIRDirty_0_N(1, "trace_branch_indirect",
						  VG_(fnptr_to_fnentry)(trace_branch_indirect), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

/*------------------------------------------------------------*/
/*--- Basic tool functions                                 ---*/
/*------------------------------------------------------------*/

static void ct_post_clo_init(void) {

	pid = VG_(getpid)();

	char str[128];
	VG_(sprintf)(str, "%s_%u", t_fname, pid);
	VG_(printf)("==%u== cstracer: inst struct size : %u\n", pid, sizeof(inst));
	VG_(printf)("==%u== cstracer: Tracefile : %s\n", pid, str);
	VG_(printf)("==%u== cstracer: Skip : %llu\n", pid, skip);
	VG_(printf)("==%u== cstracer: Trace : %llu\n", pid, trace_instrs);

	fd = VG_(fd_open)(str, VKI_O_WRONLY | VKI_O_TRUNC | VKI_O_CREAT, 00644);
	tl_assert(fd != -1);
}

static IRSB *ct_instrument(VgCallbackClosure *closure, IRSB *sbIn,
						   const VexGuestLayout *layout,
						   const VexGuestExtents *vge,
						   const VexArchInfo *archinfo_host, IRType gWordTy,
						   IRType hWordTy) {
	IRDirty *di;
	Int i;
	IRSB *sbOut;
	IRTypeEnv *tyenv		= sbIn->tyenv;
	Addr iaddr				= 0, dst;
	UInt ilen				= 0;
	Bool condition_inverted = False;

	if (gWordTy != hWordTy) {
		/* We don't currently support this case. */
		VG_(tool_panic)("host/guest word size mismatch");
	}

	/* Set up SB */
	sbOut = deepCopyIRSBExceptStmts(sbIn);

	// Copy verbatim any IR preamble preceding the first IMark
	i = 0;
	while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
		addStmtToIRSB(sbOut, sbIn->stmts[i]);
		i++;
	}

	instrument_superblock(sbOut, mkIRExpr_HWord((HWord)vge->base[0]));

	for (/*use current i*/; i < sbIn->stmts_used; i++) {

		IRStmt *st = sbIn->stmts[i];
		if ( !st || st->tag == Ist_NoOp )
			continue;
		switch (st->tag) {

		case Ist_IMark:
			/* Needed to be able to check for inverted condition in Ist_Exit */
			iaddr = st->Ist.IMark.addr;
			ilen  = st->Ist.IMark.len;
			instrument_instruction(
				sbOut, mkIRExpr_HWord((HWord)st->Ist.IMark.addr), ilen);
			addStmtToIRSB(sbOut, st);
			break;

		case Ist_NoOp:
		case Ist_AbiHint:
		case Ist_PutI: // TODO: Add support
			/*if ( PRINT_ERROR ) {
				VG_(printf)("ERROR: PutI Not Yet Supported\n");
			}*/
		case Ist_MBE:
			addStmtToIRSB(sbOut, st);
			break;

		case Ist_Put: {
			IRType type = typeOfIRExpr(tyenv, st->Ist.Put.data);
			instrument_reg_write(sbOut, st->Ist.Put.offset, sizeofIRType(type));
			addStmtToIRSB(sbOut, st);
			break;
		}

		case Ist_WrTmp: {
			addStmtToIRSB(sbOut, st);

			/*Instrument After*/
			IRExpr *data = st->Ist.WrTmp.data;
			if (data->tag == Iex_Load) {
				instrument_load(sbOut, data->Iex.Load.addr,
								sizeofIRType(data->Iex.Load.ty), NULL);
			}

			switch (data->tag) {
			case Iex_Get:
				instrument_reg_read(sbOut, data->Iex.Get.offset,
									sizeofIRType(data->Iex.Get.ty));
				break;
			case Iex_GetI:
				if (PRINT_ERROR) {
					VG_(printf)("ERROR: GetI Not Yet Supported\n");
				}
				/*Not supported as of now*/
				ppIRStmt(st);
				//tl_assert(0);
				break;
			default:
				break;
			}
			break;
		}

		case Ist_Store: {
			addStmtToIRSB(sbOut, st);
			IRExpr *data = st->Ist.Store.data;
			IRType type  = typeOfIRExpr(tyenv, data);
			tl_assert(type != Ity_INVALID);
			instrument_store(sbOut, st->Ist.Store.addr, sizeofIRType(type),
							 NULL);
			break;
		}

		case Ist_StoreG: {
			addStmtToIRSB(sbOut, st);
			IRStoreG *sg = st->Ist.StoreG.details;
			IRExpr *data = sg->data;
			IRType type  = typeOfIRExpr(tyenv, data);
			tl_assert(type != Ity_INVALID);
			instrument_store(sbOut, sg->addr, sizeofIRType(type), sg->guard);
			break;
		}

		case Ist_LoadG: {
			addStmtToIRSB(sbOut, st);
			IRLoadG *lg		= st->Ist.LoadG.details;
			IRType type		= Ity_INVALID; /* loaded type */
			IRType typeWide = Ity_INVALID; /* after implicit widening */
			typeOfIRLoadGOp(lg->cvt, &typeWide, &type);
			tl_assert(type != Ity_INVALID);
			instrument_load(sbOut, lg->addr, sizeofIRType(type), lg->guard);
			break;
		}

		case Ist_Dirty: {
			addStmtToIRSB(sbOut, st);
			Int dsize;
			IRDirty *d = st->Ist.Dirty.details;
			if (d->mFx != Ifx_None) {
				// This dirty helper accesses memory.  Collect the details.
				tl_assert(d->mAddr != NULL);
				tl_assert(d->mSize != 0);
				dsize = d->mSize;
				if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
					instrument_load(sbOut, d->mAddr, dsize, NULL);
				if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
					instrument_store(sbOut, d->mAddr, dsize, NULL);
			} else {
				tl_assert(d->mAddr == NULL);
				tl_assert(d->mSize == 0);
			}
			break;
		}

		case Ist_CAS: {
			/* We treat it as a read and a write of the location.  I
			think that is the same behaviour as it was before IRCAS
			was introduced, since prior to that point, the Vex
			front ends would translate a lock-prefixed instruction
			into a (normal) read followed by a (normal) write. */
			addStmtToIRSB(sbOut, st);
			Int dataSize;
			IRType dataTy;
			IRCAS *cas = st->Ist.CAS.details;
			tl_assert(cas->addr != NULL);
			tl_assert(cas->dataLo != NULL);
			dataTy   = typeOfIRExpr(tyenv, cas->dataLo);
			dataSize = sizeofIRType(dataTy);
			if (cas->dataHi != NULL)
				dataSize *= 2; /* since it's a doubleword-CAS */
			instrument_load(sbOut, cas->addr, dataSize, NULL);
			instrument_store(sbOut, cas->addr, dataSize, NULL);
			break;
		}

		case Ist_LLSC: {
			addStmtToIRSB(sbOut, st);
			IRType dataTy;
			if (st->Ist.LLSC.storedata == NULL) {
				/* LL */
				dataTy = typeOfIRTemp(tyenv, st->Ist.LLSC.result);
				instrument_load(sbOut, st->Ist.LLSC.addr, sizeofIRType(dataTy),
								NULL);
			} else {
				/* SC */
				dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
				instrument_store(sbOut, st->Ist.LLSC.addr, sizeofIRType(dataTy),
								 NULL);
			}
			break;
		}

		case Ist_Exit:
			// VG_(printf)("EXIT: ");
			// ppIRSB( sbIn );
			// VG_(printf)("\n");

			// The condition of a branch was inverted by VEX if a taken
			// branch is in fact a fall trough according to client address
			tl_assert(iaddr != 0);
			dst = (sizeof(Addr) == 4) ? st->Ist.Exit.dst->Ico.U32
									  : st->Ist.Exit.dst->Ico.U64;
			condition_inverted = (dst == iaddr + ilen);

			// instrument only if it is branch in guest code
			if ((st->Ist.Exit.jk == Ijk_Boring) ||
				(st->Ist.Exit.jk == Ijk_Call) || (st->Ist.Exit.jk == Ijk_Ret)) {
				instrument_branch_conditional(sbOut, condition_inverted,
											  st->Ist.Exit.guard);
			}

			addStmtToIRSB(sbOut, st); // Original statement

			break;

		default:
			ppIRStmt(st);
			tl_assert(0);
		}
	}

	if ((sbIn->jumpkind == Ijk_Boring) || (sbIn->jumpkind == Ijk_Call) ||
		(sbIn->jumpkind == Ijk_Ret)) {
		if (0) {
			ppIRExpr(sbIn->next);
			VG_(printf)("\n");
		}
		switch (sbIn->next->tag) {
		// TODO : This classification isn't perfect;
		case Iex_Const:
			/*branch to known address */
			instrument_branch_direct(sbOut, sbIn->jumpkind);
			break;
		case Iex_RdTmp:
			/* an indirect branch (branch to unknown) */
			instrument_branch_indirect(sbOut, sbIn->jumpkind);
			break;
		default:
			/* shouldn't happen - if the incoming IR is properly
				flattened, should only have tmp and const cases to
				consider. */
			tl_assert(0);
		}
	}
	return sbOut;
}

static void ct_fini(Int exitcode) {
	VG_(printf)("==%u== cstracer: Program Completed\n", pid);
	VG_(printf)("==%u== cstracer: Instructions = %llu\n", pid, instructions);

	if (!tracing_done) {
		VG_(close)(fd);
	}
	/* end tracing */
}

static void ct_pre_clo_init(void) {
	VG_(details_name)("ChampSimTracer");
#if defined(VGP_arm64_linux)
	VG_(details_description)("generate Traces for Data ChampSim : arm64");
#else
	VG_(details_description)("generate Traces for Data ChampSim : x86-64");
#endif
	VG_(details_copyright_author)
	("Copyright (C) 2020, and GNU GPL'd, by Siddharth Jayashankar.");
	VG_(details_bug_reports_to)(VG_BUGS_TO);
	VG_(details_avg_translation_sizeB)(200);

	VG_(basic_tool_funcs)(ct_post_clo_init, ct_instrument, ct_fini);
	VG_(needs_command_line_options)
	(ct_process_cmd_line_option, ct_print_usage, ct_print_debug_usage);
}

VG_DETERMINE_INTERFACE_VERSION(ct_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                ct_main.c ---*/
/*--------------------------------------------------------------------*/

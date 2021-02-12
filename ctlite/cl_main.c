
/*--------------------------------------------------------------------*/
/*--- A Valgrind tool to generate traces for ChampSim    cl_main.c ---*/
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

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>


#define IMAP_SIZE 1024
#define MMAP_SIZE 1024
#define MAX_DSIZE 512


/*------------------------------------------------------------*/
/*--- Command line options                                 ---*/
/*------------------------------------------------------------*/

// Trace file name --trace-file=tracefile
static const HChar *t_fname = "tracefile";

// Memory Size to Trace --mem-size=
static unsigned int MShiftSize = 22; // 4M -> 22 - 10
static unsigned long long int MMask = (1 << 12) - 1;

// Code Size to Trace --code-size=
static unsigned int IShiftSize = 22; // 4M -> 22 - 10
static unsigned long long int IMask = (1 << 12) - 1;

// Print Heartbeat after --heartbeat= instructions
static unsigned long long int heartbeat = 100000000;

static unsigned int Imap[IMAP_SIZE];
static unsigned int Mmap[MMAP_SIZE];

static Bool cl_process_cmd_line_option(const HChar *arg) {
	if
		VG_STR_CLO(arg, "--trace-file", t_fname) {}
	else if
		VG_INT_CLO(arg, "--mem-size", MShiftSize) {}
	else if
		VG_INT_CLO(arg, "--code-size", IShiftSize) {}
	else if
		VG_INT_CLO(arg, "--heartbeat", heartbeat) {}
	else
		return False;


	
	tl_assert(t_fname);
	tl_assert(t_fname[0]);
	tl_assert(MShiftSize >= 10);
	tl_assert(IShiftSize >= 10);
	return True;
}

static void cl_print_usage(void) {
	VG_(printf)
	("    --trace-file=<file>        Trace File Name\n"
	 "    --mem-size=<num>        	Log Size of Memory Region To Track\n"
	 "    --code-size=<num>        	Log Size of Code Region To Track\n");
}

static void cl_print_debug_usage(void) { VG_(printf)(" (none)\n"); }

/*------------------------------------------------------------*/
/*--- Tracing                                              ---*/
/*------------------------------------------------------------*/
static unsigned long long int instructions = 0;
static unsigned int TBranches = 0;
static unsigned int UBranches = 0;

static int fd;
static UInt pid;
typedef IRExpr IRAtom;

static VG_REGPARM(2) void trace_instr(Addr iaddr, SizeT size) {
	instructions++;
	iaddr = iaddr >> IShiftSize;
	Imap[iaddr % IMAP_SIZE]++;
	if(instructions % heartbeat == 0) {
		VG_(printf)
		("==%u== ctlite: Heartbeat : %llu instructions\n", pid, instructions);
		VG_(write)(fd, &instructions, sizeof(instructions));
		VG_(write)(fd, Mmap, MMAP_SIZE*sizeof(unsigned int));
		VG_(write)(fd, Imap, IMAP_SIZE*sizeof(unsigned int));
		VG_(write)(fd, &TBranches, sizeof(TBranches));
		VG_(write)(fd, &UBranches, sizeof(UBranches));
		
		VG_(memset)( Mmap, 0, MMAP_SIZE*sizeof(unsigned int));
		VG_(memset)( Imap, 0, IMAP_SIZE*sizeof(unsigned int));
		VG_(memset)( &TBranches, 0, sizeof(TBranches));
		VG_(memset)( &UBranches, 0, sizeof(UBranches));
	}

}

static VG_REGPARM(2) void trace_load(Addr addr, SizeT size) {
	addr = addr >> MShiftSize;
	Mmap[addr % MMAP_SIZE]++;
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size) {
	addr = addr >> MShiftSize;
	Mmap[addr % MMAP_SIZE]++;
}


static VG_REGPARM(2) void trace_branch_conditional(Bool ci, Bool guard) {
	if (guard) {
		if(ci)
			UBranches++;
		else
			TBranches++;
	} else {
		if(ci)
			TBranches++;
		else
			UBranches++;
	}
}


static void instrument_instruction(IRSB *sb, IRAtom *iaddr, UInt isize) {

	tl_assert((VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB) ||
			  VG_CLREQ_SZB == isize);
	IRExpr **argv;
	IRDirty *di;

	argv = mkIRExprVec_2(iaddr, mkIRExpr_HWord(isize));
	di = unsafeIRDirty_0_N(2, "trace_instr", VG_(fnptr_to_fnentry)(trace_instr),
						   argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static void instrument_load(IRSB *sb, IRAtom *daddr, Int dsize, IRAtom *guard) {
	tl_assert(isIRAtom(daddr));
	tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

	IRDirty *di_mem = emptyIRDirty();
	IRExpr **argv_mem = mkIRExprVec_2(daddr, mkIRExpr_HWord(dsize));
	di_mem->args	  = argv_mem;
	di_mem->cee =
		mkIRCallee(2, "trace_load", VG_(fnptr_to_fnentry)(trace_load));
	di_mem->guard = IRExpr_Const(IRConst_U1(True));
	addStmtToIRSB(sb, IRStmt_Dirty(di_mem));
}

static void instrument_store(IRSB *sb, IRAtom *daddr, Int dsize,
							 IRAtom *guard) {
	tl_assert(isIRAtom(daddr));
	tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

	IRDirty *di_mem = emptyIRDirty();
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


/*------------------------------------------------------------*/
/*--- Basic tool functions                                 ---*/
/*------------------------------------------------------------*/

static void cl_post_clo_init(void) {

	pid = VG_(getpid)();
	
	MShiftSize -= 10;
	IShiftSize -= 10;
	MMask = (1 << MShiftSize) - 1;
	IMask = (1 << IShiftSize) - 1;

	char str[128];
	VG_(sprintf)(str, "%s_%u", t_fname, pid);
	VG_(printf)("==%u== ctlite: sizes : %u %u\n", pid, sizeof(unsigned int), sizeof(instructions));
	VG_(printf)("==%u== ctlite: Tracefile : %s\n", pid, str);
	VG_(printf)("==%u== ctlite: mem-size : %u\n", pid, MShiftSize + 10);
	VG_(printf)("==%u== ctlite: code-size : %u\n", pid, IShiftSize + 10);

	fd = VG_(fd_open)(str, VKI_O_WRONLY | VKI_O_TRUNC | VKI_O_CREAT, 00644);
	tl_assert(fd != -1);
}

static IRSB *cl_instrument(VgCallbackClosure *closure, IRSB *sbIn,
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
		case Ist_MBE:
		case Ist_Put:
			addStmtToIRSB(sbOut, st);
			break;

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
			case Iex_GetI:
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
#if 0
	if ((sbIn->jumpkind == Ijk_Boring) || (sbIn->jumpkind == Ijk_Call) ||
		(sbIn->jumpkind == Ijk_Ret)) {
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
#endif
	return sbOut;
}

static void cl_fini(Int exitcode) {
	VG_(printf)("==%u== ctlite: Program Completed\n", pid);
	VG_(printf)("==%u== ctlite: Instructions = %llu\n", pid, instructions);

	VG_(close)(fd);
	/* end tracing */
}

static void cl_pre_clo_init(void) {
	VG_(details_name)("ChampSimTracer-Lite");
#if defined(VGP_arm64_linux)
	VG_(details_description)("generate Traces for Data ChampSim : arm64");
#else
	VG_(details_description)("generate Traces for Data ChampSim : x86-64");
#endif
	VG_(details_copyright_author)
	("Copyright (C) 2020, and GNU GPL'd, by Siddharth Jayashankar.");
	VG_(details_bug_reports_to)(VG_BUGS_TO);
	VG_(details_avg_translation_sizeB)(200);

	VG_(basic_tool_funcs)(cl_post_clo_init, cl_instrument, cl_fini);
	VG_(needs_command_line_options)
	(cl_process_cmd_line_option, cl_print_usage, cl_print_debug_usage);
}

VG_DETERMINE_INTERFACE_VERSION(cl_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                cl_main.c ---*/
/*--------------------------------------------------------------------*/

/*
 * Copyright (C) 2008-2009 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*
 *  Copyright (C) 2007, 2008, 2009. PathScale, LLC. All Rights Reserved.
 */

/*
 *  Copyright (C) 2006, 2007. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2002, 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

   Path64 is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Path64 is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Path64; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Special thanks goes to SGI for their continued support to open source

*/

/* ====================================================================
 *
 * Module: cgemit.c
 * $Revision: 1.193 $
 * $Date: 05/11/17 16:20:11-08:00 $
 * $Author: gautam@jacinth.keyresearch $
 * $Source: be/cg/SCCS/s.cgemit.cxx $
 *
 * Description:
 *
 * Emit object code and/or assembly instructions from compiler.
 *
 * ====================================================================
 * ====================================================================
 */


#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "elf_defines.h"
#ifdef KEY /* Mac port */
#include "dwarf_stuff.h"
#endif /* KEY Mac port */
#include <elfaccess.h>
#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdlib.h>
#include <cmplrs/rcodes.h>
#include <stamp.h>
#include <vector>
#include <sstream>
#if defined(BUILD_OS_DARWIN)
#include <set>
#include <string>
#endif /* defined(BUILD_OS_DARWIN) */

#define	USE_STANDARD_TYPES 1
#include "defs.h"
#include "cxx_template.h"
#include "config.h"
#include "config_asm.h"
#include "config_targ_opt.h"
#include "config_debug.h"
#ifdef TARG_ST
#include "libiberty.h"
#include "file_util.h" // [CL] for New_Extension
#endif
#include "targ_const.h"
#include "strtab.h"
#include "symtab.h"
#include "wn.h"

#include "erglob.h"
#include "erlib.h"
#include "ercg.h"
#include "file_util.h"
#include "flags.h"
#include "glob.h"
#include "xstats.h"
#include "tracing.h"
#include "cgir.h"
#include "import.h"
#include "opt_alias_interface.h"	/* for Print_alias_info */
#include "anl_driver.h"			/* for Anl_File_Path */
#include "ti_asm.h"
#include "ti_errors.h"
#include "targ_proc_properties.h"
#include "targ_abi_properties.h"
#include "targ_isa_print.h"
#include "targ_isa_enums.h"
#include "targ_isa_pack.h"
#include "targ_isa_bundle.h"
#include "targ_isa_operands.h"

#include "cg.h"				/* for Alias_Manager */
#include "const.h"
#include "whirl2ops.h"			/* for Get_WN_From_Memory_OP */
#include "stblock.h"
#include "data_layout.h"
#include "sections.h"
#include "dwarf_DST_mem.h"

#include "calls.h"
#include "cgemit.h"
#include "cg_minir.h"
#include "cgtarget.h"
#include "irbdata.h"
#include "em_elf.h"
#include "cgdwarf.h"
#include "cgdwarf_targ.h"
#include "em_dwarf.h"
#include "tn_set.h"
#include "iface_scn.h"
#include "config_targ.h"
#include "config_list.h"
#include "note.h"
#include "cgexp.h"
#include "eh_region.h"
#include "cg_flags.h"
#include "region_util.h"
#include "cg_region.h"
#include "targ_isa_lits.h"
#include "freq.h"
#include "vstring.h"
#include "label_util.h"
#include "cgemit_targ.h"
#include "tag.h"

#ifdef TARG_ST
/* (cbr) demangler interface */
#include <cxxabi.h>

/* [SC] For ST_has_inito */
#include "be_symtab.h"
#endif
#ifdef TARG_IA64
  #include "cg_swp.h"
#else
  #include "cg_swp_driver.h"
#endif

#ifdef KEY
#include "config_lno.h" // for LNO_Run_Simd
#include "config_opt.h" // for OPT_Cyg_Instrument
#include "be_util.h"    // for Current_PU_Count
#endif

#include "main_defs.h"

std::ostringstream sections_MINIR, objects_MINIR, functions_MINIR;

#include <fstream>
#include <iostream>
static std::ofstream MiniR_Ofstream; /* MiniR file */

// Huge ifdef here /!\ BEGIN_TARG_ST_IFDEF
#ifdef TARG_ST
BE_EXPORTED extern void Early_Terminate (INT status);

#define PAD_SIZE_LIMIT	2048	/* max size to be padded in a section */

/* c++ mangled names can be any arbitrary length,
 * so this is just a first guess */ 
#define LBUF_LEN	(OP_MAX_FIXED_OPNDS*1024)

/* Instructions go into one of two ELF text sections depending on
 * whether the BB is in a hot or cold region. Hot BBs go into
 * ".text" (or ".text<pu-name>" for -TENV:section_for_each_function).
 * Cold BBs go into ".text.cold". As a result, numerous cgemit
 * routines need to track the PC of both sections, and 2 element
 * arrays are typically used. Here we define the indices for
 * the arrays, with the values chosen to coincide with the result
 * of BB_Is_Cold.
 */
enum { IHOT=FALSE, ICOLD=TRUE };

/* Overload BB flag <local_flag1> to indicate if a BB is cold or hot --
 * it's less overhead than BB_Is_Cold.
 */
#define BB_cold		BB_local_flag1
#define Set_BB_cold	Set_BB_local_flag1
#define Reset_BB_cold	Reset_BB_local_flag1

#ifndef _NO_WEAK_SUPPORT_
extern const char __Release_ID[];
#else
BE_EXPORTED extern const char **__Release_ID_p;
#define __Release_ID (*__Release_ID_p)
#endif

/* ====================================================================
 *    Global data
 * ====================================================================
 */
BOOL CG_emit_asm_dwarf    = FALSE;
#ifdef TARG_ST // [CL] do not emit unwind info by default
BOOL CG_emit_unwind_info  = FALSE;
#else
BOOL CG_emit_unwind_info  = TRUE;
#endif
BOOL CG_emit_unwind_directives = FALSE;

#ifdef TARG_STxP70
INT32 CG_itstackalign_val = 8;
BOOL  CG_itstackalign_val_overridden = FALSE;
#endif

/* ====================================================================
 *    Local data
 * ====================================================================
 */

static BOOL generate_dwarf = FALSE;
static BOOL generate_elf_symbols = FALSE;

/* Local phase trace flags: */
static BOOL Trace_Init    = FALSE;	/* Data initialization trace */
static BOOL Trace_Inst    = FALSE;	/* Noop-insertion trace */
static BOOL Trace_Elf     = FALSE;	/* Miscellaneous ELF trace */
#ifdef TARG_ST
static BOOL Trace_Sched   = FALSE;	// Trace issue dates and bundle count. */
#endif
// static BOOL Trace_Longbr  = FALSE;	/* trace handling of long branches */

static FILE *Output_File;               /* write to Asm or Lai file */

BOOL Use_Page_Zero = FALSE;
static BOOL Use_Prefetch = FALSE;

// For dwarf output in assembly we need to keep track of the 
// offset of the current instruction from a known label.
// If Last_Label is LABEL_IDX_ZERO, it is assumed to be invalid.
static LABEL_IDX Last_Label = LABEL_IDX_ZERO;
static INT32 Offset_From_Last_Label = 0;

static LABEL_IDX Initial_Pu_Label;

/* NOTE: PCs are actually a bundle address and slot number pair. The 
 * slot number is in the low bits. 
 */
static INT32 PC = 0;			/* PC for current text section */
static INT32 text_PC = 0;		/* PC for hot text */
static INT32 cold_PC = 0;		/* PC for cold text */

static ST *PU_base = NULL;		/* base for current text section */
static ST *text_base = NULL;		/* base for hot text section */
static ST *cold_base = NULL;		/* base for cold text section */

static pSCNINFO PU_section = NULL;	/* current text section */
static pSCNINFO text_section = NULL;	/* hot text section */
static pSCNINFO cold_section = NULL;	/* cold text section */

static INT current_rid = 0;	/* current rid id */

typedef struct {
  pSCNINFO scninfo;
  Elf64_Word scn_ofst;	/* [CG] keeps track of current section offset. */
  ST *sym;
#ifdef TARG_ST100
  char *label;     // we need to label sections on some targets that
                   // do not have section relative relocations
#endif
} AUX_SCN;

static AUX_SCN *em_scn;
static INT last_scn = 0;	
static INT max_scn = 0;
/* 0 index to em_scn will be null values */

static ST *cur_section = NULL;

static PU_IDX current_pu = 0;

#ifdef TARG_ST
static int Bundle_Count;
// [SC]: Symbol_Def_After_Group is a symbol that should
// be defined immediately after the current group.
static ST *Symbol_Def_After_Group;
#endif

/* ====================================================================
 *    Use_Separate_PU_Section
 *
 *    put PU in separate elf text section? (pu could be null if no PUs)
 * ====================================================================
 */
#define Use_Separate_PU_Section(pu,st) \
	((pu != (PU_IDX) NULL) \
	&& (Section_For_Each_Function || PU_in_elf_section(pu)) \
	&& (ST_class(st) == CLASS_BLOCK) \
	&& (strcmp(ST_name(st), ELF_TEXT) == 0))


static char ism_name[40];

/* ====================================================================
 *    Get_Ism_Name ()
 *
 *    get ism name from __Release_ID 
 *    TODO: same code as in ../cg/cgemit.cxx -- factorize ?
 * ====================================================================
 */
static void
Get_Ism_Name (void)
{
  char *s = strchr(__Release_ID, ':');
  char *p;

  if (s == NULL) {
    ism_name[0] = '\0';
    return;
  }
  s++;	/* skip : */
  s = strchr(s, ':');
  if (s == NULL) {
    ism_name[0] = '\0';
    return;
  }
  s++;	/* skip : */
  p = strchr(s, ' ');
  if (p == NULL) {
    ism_name[0] = '\0';
    return;
  }
  strncpy (ism_name, s, p-s);
  ism_name[p-s] = '\0';

  return;
}

/*
 * For the elf indexes, we can reuse the temp field for local STs,
 * Need to map from ST's to the elf index for the ST.
 * So create array of index arrays, where the ST_level
 * gets you to the index array, and then the ST_index 
 * gets you to the elf index for that ST.
 */

/*
 * Maximum nesting depth includes these scopes, in the deepest case:
 *  0 : not used
 *  1 : global scope
 *  2 : top-level PU
 *  3 : F90 internal procedure
 *  4 : MP PU from internal procedure
 *  5 : Fortran I/O in MP PU
 */

#define MAX_SYMTAB_DEPTH	6

static INT32 *elf_index_array[MAX_SYMTAB_DEPTH] =
  {NULL,NULL,NULL,NULL,NULL,NULL};
static INT max_elf_index[MAX_SYMTAB_DEPTH] = {0,0,0,0,0,0};

/* ====================================================================
 *   Allocate_Elf_Index_Space
 * ====================================================================
 */
static void
Allocate_Elf_Index_Space (
  UINT level
)
{
  INT num_sts = ST_Table_Size(level);

  if (elf_index_array[level] == NULL) {
    max_elf_index[level] = num_sts + 100;
    /* for the size take the #st's + extra space for new symbols */
    elf_index_array[level] = 
          (INT32*) Src_Alloc(sizeof(INT32) * max_elf_index[level]);
  } else if (max_elf_index[level] < num_sts + 10) {  
    /* # globals has grown, so realloc. */
    /* we realloc even if close and not yet overflowed,
     * because may alloc new symbols as part of cg. */
    elf_index_array[level] = TYPE_MEM_POOL_REALLOC_N (INT32, 
		    &MEM_src_pool, elf_index_array[level], 
			      max_elf_index[level], num_sts + 100);
    max_elf_index[level] = num_sts + 100;
  }
}

/* ====================================================================
 *   Init_ST_elf_index
 * ====================================================================
 */
static void
Init_ST_elf_index (
  UINT stab
)
{
  UINT level = stab;
  INT i;

  Allocate_Elf_Index_Space(GLOBAL_SYMTAB);
  Is_True((level < MAX_SYMTAB_DEPTH), ("Init_ST_elf_index overflow"));
  if (level > GLOBAL_SYMTAB && elf_index_array[level] != NULL) {
    /* need to clear the values in case leftover 
     * from previous uplevel */
    for (i = 0; i < max_elf_index[level]; i++) {
      elf_index_array[level][i] = 0;
    }
  }
  Allocate_Elf_Index_Space(stab);
}

/* ====================================================================
 *   ST_elf_index
 * ====================================================================
 */
static INT32
ST_elf_index (
  ST *st
)
{
  INT level = ST_level(st);

  Is_True((level < MAX_SYMTAB_DEPTH), ("ST_elf_index overflow"));
  Is_True((ST_index(st) < max_elf_index[level]), 
                                      ("ST_elf_index overflow"));
  return elf_index_array[level][ST_index(st)];
}

/* ====================================================================
 *   Set_ST_elf_index
 * ====================================================================
 */
static void
Set_ST_elf_index (
  ST *st, 
  INT32 v
)
{
  INT level = ST_level(st);

  Is_True((level < MAX_SYMTAB_DEPTH), ("Set_ST_elf_index overflow"));
  Is_True((ST_index(st) < max_elf_index[level]),
		                      ("Set_ST_elf_index overflow"));
  elf_index_array[level][ST_index(st)] = v;
}

/* ====================================================================
 *   ST_is_gp_relative
 *
 *   Determine if the symbol has a base of gp 
 * ====================================================================
 */
static BOOL
ST_is_gp_relative (
  ST *st
)
{
  ST *base_st = Base_Symbol (st);

  return ((ST_class(base_st) == CLASS_BLOCK || 
	   ST_class(base_st) == CLASS_VAR) && 
	  ST_gprel(base_st));
}


static unsigned char
st_other_for_sym (ST *sym)
{
  unsigned char symother;

  switch (ST_export(sym)) {
    case EXPORT_HIDDEN:
      symother = STO_HIDDEN;
      break;
    case EXPORT_PROTECTED:
      symother = STO_PROTECTED;
      break;
    case EXPORT_INTERNAL:
      symother = STO_INTERNAL;
      break;
    case EXPORT_OPTIONAL:
      symother = STO_OPTIONAL;
      break;
    default:
      symother = STO_DEFAULT;
      break;
  }
  return symother;
}

static INT64
Get_Offset_From_Full (ST *sym)
{
	/* full-split symbols have names of form "full_.offset"
	 * we need to extract the offset and pass that in value field.
	 * An alternative would be to search type structure for
	 * matching fields, but this is probably faster.
	 */
	char *offset_string;
        offset_string = strrchr (ST_name(sym), '.');
        FmtAssert(offset_string != NULL, ("Get_Offset_From_Full unexpected name format (%s)", ST_name(sym)));
        offset_string++;       /* skip the period */
	/* Remove atoll to favor cross-compilability: Except for behavior on  error, atoll() is equivalent to strtoll(str, (char **)NULL, 10) */
	return strtoll(offset_string, (char **)NULL, 10) ;
}

/***********************************************************************
 *
 * Instruction-PC utilities.
 *
 * On architectures that support bundling, we use a PC representation
 * that enables us to point at instructions within the bundle.
 * We accomplish this by using the low bits of the bundle address
 * (which are normally zero because of alignment constraints) to
 * hold the instruction offset, i.e. slot number, within the bundle.
 *
 * The following utility routines provide for updating and accessing
 * the composite PC values.
 *
 ***********************************************************************
 */

/* Given a composite PC, return the bundle address component.
 */
inline INT32 PC_Bundle(INT32 pc)
{
#ifdef TARG_ST
  // Previous implementation not working if
  // ISA_MAX_INST_BYTES is not a power of 2.
  INT32 slot = pc % ISA_MAX_INST_BYTES;
  return (pc - slot);
#else
  return pc & ~(ISA_MAX_INST_BYTES - 1);
#endif
}

/* Given a composite PC, return the slot number component.
 */
inline INT32 PC_Slot(INT32 pc)
{
#ifdef TARG_ST
  // Previous implementation not working if
  // ISA_MAX_INST_BYTES is not a power of 2.
  return (pc % ISA_MAX_INST_BYTES);
#else
  return pc & (ISA_MAX_INST_BYTES - 1);
#endif
}

#ifdef TARG_ST
/* Given a composite PC, return the corresponding physical address */
inline INT32 PC2Addr(INT32 pc)
{
  return PC_Bundle(pc) + PC_Slot(pc)*ISA_MAX_INST_BYTES/ISA_MAX_SLOTS;
}

/* Given a physical address, return the corresponding composite PC */
inline INT32 Addr2PC(INT32 addr)
{
  // Note that there is no equivalence with the previous tranformation
  INT32 bundle = PC_Bundle(addr);
  INT32 slot = ((addr - bundle) * ISA_MAX_SLOTS) / ISA_MAX_INST_BYTES;
  return bundle + slot;
}
#endif

/* ====================================================================
 *   PC_Incr
 *
 *   Increment a composite PC by 1. The slot number is incremented, 
 *   and if it overflows, the bundle address is adjusted as well.
 * ====================================================================
 */
inline INT32 
PC_Incr (
  INT32 pc
)
{
  ++pc;

  if (PC_Slot(pc) == ISA_MAX_SLOTS) {
    pc += ISA_MAX_INST_BYTES - ISA_MAX_SLOTS;
  }

  return pc;
}

/* ====================================================================
 *   PC_Incr_N
 *
 *   Same as PC_Incr except the increment is an arbitrary constant.
 * ====================================================================
 */
inline INT32 
PC_Incr_N (
  INT32 pc, 
  UINT32 incr
)
{
  UINT slots = PC_Slot(pc) + incr;
  UINT bundles = slots / ISA_MAX_SLOTS;
  pc = PC_Bundle(pc) + (bundles * ISA_MAX_INST_BYTES) + (slots % ISA_MAX_SLOTS);
  return pc;
}

#ifdef TARG_ST
/* ====================================================================
 *   PC_Align_N
 *
 *   Align the PC to the given byte boundary
 * ====================================================================
 */
inline INT32 
PC_Align_N (
  INT32 pc, 
  UINT32 align
)
{
  INT32 addr = PC2Addr(pc);
  INT32 new_addr = (addr+align-1)/align*align;
  pc = Addr2PC(new_addr);
  return pc;
}
#endif

/* ====================================================================
 *    r_qualified_name
 * ====================================================================
 */
static void
r_qualified_name (
  ST *st,
  vstring *buf       /* buffer to format it into */
)
{
#ifdef TARG_ST
  // [CG]: Call support function into targ_cgemit.cxx
  CGEMIT_Qualified_Name(st, buf);
#else
  if (ST_name(st) && *(ST_name(st)) != '\0') {
    *buf = vstr_concat(*buf, ST_name(st));

#if defined (TARG_X8664) || defined (TARG_ST200)
// This name is already unique, don't change it.
	if (!strncmp (ST_name(st), ".range_table.", strlen(".range_table.")))
		return;
#endif // TARG_X8664

    if (ST_is_export_local(st) && ST_class(st) == CLASS_VAR) {
      // local var, but being written out.
      // so add suffix to help .s file distinguish names.
      // assume that statics in mult. pu's will 
      // get moved to global symtab, so don't need pu-num
      if (ST_level(st) == GLOBAL_SYMTAB) {
	vstr_sprintf (buf, vstr_len(*buf), 
                    "%s%d", Label_Name_Separator, ST_index(st));
      } else {
	vstr_sprintf (buf, vstr_len(*buf), 
		      "%s%d%s%d", Label_Name_Separator, ST_pu(Get_Current_PU_ST()),
		           Label_Name_Separator, ST_index(st) );
      }
    }
    else if (*Symbol_Name_Suffix != '\0') {
      *buf = vstr_concat(*buf, Symbol_Name_Suffix);
    }
  } else {
    vstr_sprintf (buf, vstr_len(*buf), 
                 "%s %+lld", ST_name(ST_base(st)), ST_ofst(st));
  }
#endif
}


/* ====================================================================
 *    EMT_Write_Qualified_Name (FILE *f, ST *st)
 * ====================================================================
 */
void
EMT_Write_Qualified_Name (
  FILE *f, 
  ST *st
)
{
  vstring buf = vstr_begin(LBUF_LEN);
  r_qualified_name (st, &buf);
  fprintf (f, "%s", vstr_str(buf));
  vstr_end(buf);
}



#ifdef TARG_ST
// [CL] helper functions to generate anonymous constants
/* ====================================================================
 *    r_qualified_tcon_name
 * ====================================================================
 */
static void
r_qualified_tcon_name (
  ST *st,
  vstring *buf       /* buffer to format it into */
)
{
  if (ST_level(st) == GLOBAL_SYMTAB) {
    vstr_sprintf (buf, vstr_len(*buf), 
                 "%s_UNNAMED_CONST_%s_%d%s%d", Local_Label_Prefix,
		  Ipa_Label_Suffix,
		 ST_tcon(st), Label_Name_Separator, ST_index(st));
  } else {
    vstr_sprintf (buf, vstr_len(*buf), 
                 "%s_UNNAMED_CONST_%s_%d%s%d%s%d", Local_Label_Prefix,
		  Ipa_Label_Suffix,
		  ST_tcon(st), Label_Name_Separator,
		  ST_pu(Get_Current_PU_ST()),
		  Label_Name_Separator, ST_index(st));
  }
}
#endif

/* ====================================================================
 *    EMT_Write_Qualified_Name (FILE *f, ST *st)
 * ====================================================================
 */
void
EMT_Write_Qualified_Tcon_Name (
  FILE *f, 
  ST *st
)
{
  vstring buf = vstr_begin(LBUF_LEN);
  r_qualified_tcon_name (st, &buf);
  fprintf (f, "%s", vstr_str(buf));
  vstr_end(buf);
}


/* ====================================================================
 *    Trace_Init_Loc
 * ====================================================================
 */
static void
Trace_Init_Loc ( INT scn_idx, Elf64_Xword scn_ofst, INT32 repeat)
{
  /* Emit the section/offset/repeat as a line prefix -- the caller will
   * add context-specific information:
   */
  fprintf(TFile, "<init>: Section %s (offset %4lld x%d): ",
	    ST_name(em_scn[scn_idx].sym), scn_ofst, repeat );

  return;
}



#ifdef KEY
static Elf64_Xword
Generate_Exception_Table_Header (INT scn_idx,
                                 Elf64_Xword scn_ofst,
                                 LABEL_IDX *);
#endif // KEY


#ifdef KEY
static int
num_digits (int in)
{
    int num=1;
    while (in/10 != 0)
    {
        num++;
        in = in/10;
    }
    return num;
}

static Elf64_Xword
Generate_Exception_Table_Header (INT scn_idx, Elf64_Xword scn_ofst, LABEL_IDX *labels)
{
    // Generate LPStart pointer, currently constant
    TCON lpstart = Host_To_Targ (MTYPE_I1, 0xff);
    scn_ofst = Write_TCON (&lpstart, scn_idx, scn_ofst, 1);

    // Generate TType format
    TCON ttype;
    if (Gen_PIC_Call_Shared || Gen_PIC_Shared)
    	ttype = Host_To_Targ (MTYPE_I1, 0x9b);
    else
    	ttype = Host_To_Targ (MTYPE_I1, 0);
    scn_ofst = Write_TCON (&ttype, scn_idx, scn_ofst, 1);

    // Generate TType base offset
    int id_size = num_digits (exception_table_id);
    LABEL_IDX ttype_base;
    New_LABEL (CURRENT_SYMTAB, ttype_base);
    char *ttype_base_name = (char *)malloc (strlen (lsda_ttype_base) +
                                id_size + 1);
    sprintf (ttype_base_name, "%s%d", lsda_ttype_base, exception_table_id);
    Set_LABEL_name_idx (Label_Table[ttype_base], Save_Str (ttype_base_name));

    LABEL_IDX ttype_disp;
    New_LABEL (CURRENT_SYMTAB, ttype_disp);
    char *ttype_disp_name = (char *)malloc (strlen (lsda_ttype_disp) +
                                id_size + 1);
    sprintf (ttype_disp_name, "%s%d", lsda_ttype_disp, exception_table_id);
    Set_LABEL_name_idx (Label_Table[ttype_disp], Save_Str (ttype_disp_name));
    /*
    ST * csb = New_ST (CURRENT_SYMTAB);
    char *csb_name = (char *)malloc ( strlen (lsda_cs_begin) +
                                id_size + 1);
    sprintf (csb_name, "%s%d", lsda_cs_begin, exception_table_id);
    ST_Init (csb, Save_Str (csb_name), CLASS_BLOCK, 
                SCLASS_EH_REGION, EXPORT_LOCAL, MTYPE_TO_TY_array[MTYPE_U4]);

    ST * ttype_disp = New_ST (CURRENT_SYMTAB);
    char *ttype_disp_name = (char *)malloc ( strlen (lsda_ttype_disp) +
                                id_size + 1);
    sprintf (ttype_disp_name, "%s%d", lsda_ttype_disp, exception_table_id);
    ST_Init (ttype_disp, Save_Str (ttype_disp_name), CLASS_BLOCK, 
                SCLASS_EH_REGION, EXPORT_LOCAL, MTYPE_TO_TY_array[MTYPE_U4]);
    Set_ST_base_idx (*ttype_disp, ST_st_idx (*csb));
    */
    scn_ofst = Write_Diff (ttype_base, ttype_disp, scn_idx, scn_ofst);
    fprintf ( Asm_File, "%s:\n", LABEL_name(ttype_disp));
#ifdef TARG_ST
    // [CL]
    Set_LABEL_emitted(ttype_disp);
#endif

    // Generate call site format
    TCON csf = Host_To_Targ (MTYPE_I1, 1);
    scn_ofst = Write_TCON (&csf, scn_idx, scn_ofst, 1);

    LABEL_IDX csb;
    New_LABEL (CURRENT_SYMTAB, csb);
    char *csb_name = (char *)malloc (strlen (lsda_cs_begin) +
                                id_size + 1);
    sprintf (csb_name, "%s%d", lsda_cs_begin, exception_table_id);
    Set_LABEL_name_idx (Label_Table[csb], Save_Str (csb_name));

    LABEL_IDX cse;
    New_LABEL (CURRENT_SYMTAB, cse);
    char *cse_name = (char *)malloc (strlen (lsda_cs_end) +
                                id_size + 1);
    sprintf (cse_name, "%s%d", lsda_cs_end, exception_table_id);
    Set_LABEL_name_idx (Label_Table[cse], Save_Str (cse_name));
    scn_ofst = Write_Diff (cse, csb, scn_idx, scn_ofst);
    labels[0] = cse;
    labels[1] = ttype_base;

    fprintf ( Asm_File, "%s:\n", LABEL_name(csb));
#ifdef TARG_ST
    // [CL]
    Set_LABEL_emitted(csb);
#endif
    return scn_ofst;
}
#endif // KEY

#ifdef TARG_ST
/* ====================================================================
 * Set_Section_Offset.
 *
 * [CG] Set the new offset for a section.
 * ====================================================================
 */
static void
Set_Section_Offset(ST *base, INT64 ofst)
{
  em_scn[STB_scninfo_idx(base)].scn_ofst = ofst;
}

/* ====================================================================
 * Add_To_Section_Offset.
 *
 * [CG] Inform that an object has been emitted into the section.
 * Thus the current section offset must be updated.
 * For each data emitted do:
 * Change_Section_Origin(st_sect, ofst);
 * Check_Section_Alignement(st_sect, object_align)
 * ... emit object ...
 * Add_To_Section_Offset(st_sect, object_size);
 * ====================================================================
 */
static void
Add_To_Section_Offset (ST *base, INT64 size) 
{
  em_scn[STB_scninfo_idx(base)].scn_ofst += size;
}		       

static void
Check_Section_Alignment (ST *base, int align)
{
  INT64 current = em_scn[STB_scninfo_idx(base)].scn_ofst;
  INT64 aligned = align == 0 ? current : 
    ((current + (align - 1)) / align) * align;
  FmtAssert(aligned == current,
	    ("object alignment mismatch offset in section %s: offset is %lld, alignment is %d", ST_name(base), current, align));
}
#endif

/* ====================================================================
 *    Change_Section_Origin (base, ofst)
 *
 *    change to a new section and new origin 
 * ====================================================================
 */
static void
Change_Section_Origin (
  ST *base, 
  INT64 ofst
)
{
  if (Assembly || Lai_Code) {
    if (base != cur_section) {
      /* switch to new section. */
      fprintf (Output_File, "\n\t%s %s\n", AS_SECTION, ST_name(base));
    }
    
#ifdef TARG_ST
    // IA64 generates .org so next data is placed at this offset
    // within the section
    // GHS assembler interprets .org as an absolute address
    // We must use use .align instead that follows.
#else
    // generate .org so next data is placed at this offset
    fprintf (Output_File, "\t%s 0x%llx\n", AS_ORIGIN, ofst);
#endif

    /* [CG]: We should not insert alignement, offsets have already alignment
       into account. The alignment should only appear at the start of the section. */
#if 0
    /* generate a '.align 0' to make sure we don't autoalign */
#ifdef GHS_BUG_FIXED
    // I can't for now since GHS assembler does not recognize
    // .align 0
    fprintf (Output_File, "\t%s\t0\n", AS_ALIGN);
#else
    // Arthur: we are aligning at 4 bytes for now.
    // TODO: parametrize, so aligns at the strongest required
    //       alignment on this target
    fprintf (Output_File, "\t%s 4\n", AS_ALIGN);
#endif
#endif

#ifdef TARG_ST
    // 
    // Emit this section's label before offset 0
    //
    if (ofst == 0) {
      /* [CG] We set current section offset to 0. */
      em_scn[STB_scninfo_idx(base)].scn_ofst = 0;

#ifdef TARG_ST100
  /* (cbr) section relative offset using labels only on st100 */
      fprintf(Asm_File, "%s:\n", em_scn[STB_scninfo_idx(base)].label);
      if (Trace_Init) {
	fprintf(TFile, "<init>: Emitting label %s for section %s\n",
		em_scn[STB_scninfo_idx(base)].label, ST_name(base));
      }
#endif
    }
#endif

#ifdef TARG_ST
    {
      /* [CG] We must pad to new ofst. */
      INT64 current_ofst = em_scn[STB_scninfo_idx(base)].scn_ofst;
      if (current_ofst <= ofst) {
	INT64 padding = ofst - current_ofst;
	if (padding > 0) {
	  fprintf(Output_File, "\t%s %lld\n", AS_SPACE, padding);
	}
      } else {
	FmtAssert(0,
		  ("object overlaps in section %s: expected offset is %lld, current offset is %lld", ST_name(base), ofst, current_ofst));
      }
      em_scn[STB_scninfo_idx(base)].scn_ofst = ofst;
    }
#endif

    cur_section = base;
  }

#if 0
  /* for nobits, add final size at end because we don't write any data. */
  if (Object_Code && !STB_nobits(base)) {
    Em_Change_Section_Origin (em_scn[STB_scninfo_idx(base)].scninfo, ofst);
  }
#endif

  return;
}

/* ====================================================================
 *    section_lt (s1, s2)
 * ====================================================================
 */
inline bool section_lt (ST *s1, ST* s2) 
{ 
  // order doesn't really matter, just that grouped by base section
  return Base_Symbol(s1) < Base_Symbol(s2); 
}

/* ====================================================================
 *    offset_lt (s1, s2)
 * ====================================================================
 */
inline bool offset_lt (ST *s1, ST* s2) 
{ 
#ifdef TARG_ST
  return Base_Offset (s1) < Base_Offset (s2);
#else
  return Offset_From_Base_Symbol(s1) < Offset_From_Base_Symbol(s2); 
#endif
}

/* ====================================================================
 *    size_lt (s1, s2)
 * ====================================================================
 */
inline bool size_lt (ST *s1, ST* s2) 
{ 
  // This is only needed so that we get silly 0-sized structs
  // at the correct offset.
  return TY_size(ST_type(s1)) < TY_size(ST_type(s2)); 
}

#if 0
/* ====================================================================
 *    LAI_print_section
 * ====================================================================
 */
static void
LAI_print_section (
  ST *st, 
  Elf64_Word scn_type, 
  Elf64_Word scn_flags,
  Elf64_Xword scn_entsize, 
  ST *cur_section
)
{
  const char *scn_name = ST_name(st);
  Elf64_Word  scn_align = STB_align(st);
  const char *cur_scn_name = (cur_section != NULL) ? 
                                         ST_name(cur_section) : NULL;
  char scn_flags_string[5];
  char scn_type_string[10];  // min of strlen("progbits") + 1
  char *p = &scn_flags_string[0];

  fprintf (Lai_File, "\n\t%s \"%s\",", AS_SECTION, scn_name);

  if (scn_flags & SHF_WRITE) *p++ = 'w';
  if (scn_flags & SHF_ALLOC) *p++ = 'a';
  if (scn_flags & SHF_EXECINSTR) *p++ = 'x';

  *p = '\0'; // null terminate the string.
  fprintf (Lai_File, " \"%s\"", scn_flags_string);

  /*
  fprintf (Lai_File, " \"%s\",", scn_flags_string);

  p = &scn_type_string[0];
  if (scn_type == SHT_NOBITS) {
    strcpy(p, "nobits");
  }
  else if (scn_type == SHT_PROGBITS) {
    strcpy(p, "progbits");
  }
  else {
    DevWarn("Intel assembler definition inadequate for "
	    "ELF section type 0x%lx; using \"progbits\"", scn_type);
    strcpy(p, "progbits");
  }
  fprintf (Lai_File, " \"%s\"\n", scn_type_string);
  */

  fprintf (Lai_File, "\n");
  fprintf (Lai_File, "\t%s\t%d\n", AS_ALIGN, scn_align);  

  return;
}
#endif


/* ====================================================================
 *    Process_Distr_Array ()
 * 
 *    Write all distr_array INITOs from the global symtab
 * ====================================================================
 */
static void
Process_Distr_Array ()
{
  for (UINT i = 1; i < INITO_Table_Size(GLOBAL_SYMTAB); ++i) {
    INITO* ino = &Inito_Table(GLOBAL_SYMTAB,i);
    ST* st = INITO_st(ino);
    if (!ST_is_not_used(st) &&
        ST_sclass(st) == SCLASS_DISTR_ARRAY) {
      INT64 ofst;
      ST* base;
      Base_Symbol_And_Offset(st, &base, &ofst);
      FmtAssert(ST_class(base) == CLASS_BLOCK && STB_section(base),
                ("inito (%s) not allocated?", ST_name(st)));
      Init_Section(base);
      Change_Section_Origin(base, ofst);
#ifdef TARG_ST
      Check_Section_Alignment (base, TY_align(ST_type(st)));
#endif
      ofst = Write_INITO(ino, STB_scninfo_idx(base), ofst);
#ifdef TARG_ST
      Set_Section_Offset(base, ofst);
#endif

    }
  }

  return;
}

/* ====================================================================
 *    Process_Bss_Data (stab)
 *
 *    iterate through the global symbols and write (s)bss symbols 
 *    to sections 
 * ====================================================================
 */
static void
Process_Bss_Data (
  SYMTAB_IDX stab
)
{
  /* 
   * To guarantee in increasing order, create std::vector of st*, 
   * then stable_sort on section, then stable_sort on offset. 
   */
  std::vector< ST* > bss_list;
  std::vector< ST* >::iterator bssp;

  // This routine can be called multiple times for the global symtab;
  // we do this so that objects are emitted in order.
  // For each section, some objects are in local symtab, and some in global.
  // We keep track of the last global symbol that has been processed
  // so that we only process new symbols.

  static UINT last_global_index = 1;
  UINT first_index = (stab == GLOBAL_SYMTAB ? last_global_index : 1);
  
  for (UINT i = first_index; i < ST_Table_Size(stab); ++i) {
    ST* sym = &St_Table(stab,i);
    if (ST_class(sym) == CLASS_BLOCK)
      continue;	// not a leaf symbol
    if (!Has_Base_Block(sym))
      continue;	// not a data symbol
#ifdef TARG_ST
    // [CG] If it is an alias, we don't emit it.
    // The alias generation is done elsewhere
    if (Has_Base_Block(sym) && ST_class(ST_base(sym)) != CLASS_BLOCK
	&& ST_emit_symbol(sym))
      continue;
#endif
    if (ST_sclass(sym) == SCLASS_UGLOBAL ||
        ST_sclass(sym) == SCLASS_FSTATIC ||
        ST_sclass(sym) == SCLASS_PSTATIC) {
      bss_list.push_back (sym);	// bss symbol
    }
  }

  if (stab == GLOBAL_SYMTAB) {
    last_global_index = ST_Table_Size(GLOBAL_SYMTAB);
  }

  // sort in lexicographic order section, offset, size
  std::stable_sort (bss_list.begin(), bss_list.end(), size_lt);
  std::stable_sort (bss_list.begin(), bss_list.end(), offset_lt);
  std::stable_sort (bss_list.begin(), bss_list.end(), section_lt);

  ST*   sym;
  ST*   next_sym;
  ST*   base;
  ST*   next_base;
  INT64 ofst;
  INT64 next_ofst;
  INT64 size;

#ifdef TARG_ST
  //
  // can have overlapping symbols that are equivalenced.
  // assume here that if multiple symbols with same offset,
  // are sorted so that largest size is last.
  //
  // align must come before the label. But if multiple labels
  // with the same offset overlap, need to align at the
  // strongest alignment. Thus, Arthur's using a list to
  // accumulate the labels while they refer to the equivalenced
  // symbols and looking for the strongest alignment.
  //
  std::list<ST*> equivalenced_symbols;
  INT32 align = 0;
#endif

  for (bssp = bss_list.begin(); bssp != bss_list.end(); ++bssp) {

    sym = *bssp;
    Base_Symbol_And_Offset(sym, &base, &ofst);
    if (ST_class(base) != CLASS_BLOCK || !STB_section(base))
      continue;	/* not allocated */
    if (!STB_nobits(base))
      continue;	/* not a bss symbol */

#ifdef TARG_ST
    if (ST_assigned_to_dedicated_preg(sym))
      continue;
#endif

#ifdef TARG_ST
    // [CG]: Ensure section is initialized
    Init_Section(base); 
#endif

    Change_Section_Origin (base, ofst);
    if (Assembly || Lai_Code) {
      size = TY_size(ST_type(sym));
#ifdef TARG_ST
      // Save alignment before continuing
      if (TY_align(ST_type(sym)) > align) {
	align = TY_align(ST_type(sym));
      }
      // Do not print the label yet
#else
      Print_Label (Output_File, &objects_MINIR, sym, size);
#endif

      // before emitting space,
      // first check whether next symbol has same offset.
      // can have overlapping symbols that are equivalenced.
      if (bssp+1 != bss_list.end()) {
	next_sym = *(bssp+1);
	Base_Symbol_And_Offset(next_sym, &next_base, &next_ofst);
#ifdef TARG_ST
	if (size > 0 && next_base == base && next_ofst == ofst) {
#else
	if (next_base == base && next_ofst == ofst) {
#endif
	  // skip to next iteration
	  // so label printed before space emitted.
#ifdef TARG_ST
	  // Save label in the equvalenced symbol list before
	  // continueing
	  equivalenced_symbols.push_back(sym);
#endif
	  continue;
	}
	else if (next_base == base && next_ofst < (ofst+size)) {
	  // have label in middle of structure,
	  // so don't emit space for full structure.
	  size = next_ofst - ofst;
	}
      }

#ifdef TARG_ST
      // assume here that if multiple symbols with same offset,
      // are sorted so that largest size is last.
      // [SC] Care: we can have genuinely zero-sized symbols, and
      // there may be one at the end of a section.
	// [CG]: Don't emit alignement, the padding was already inserted
	// by the Change_Section_Origin()
      std::list<ST*>::iterator esyms;
      // Print the previously eqivalenced symbols
      for (esyms = equivalenced_symbols.begin();
	   esyms != equivalenced_symbols.end();
	   esyms++) {
	Print_Label (Output_File, NULL, *esyms, TY_size(ST_type(*esyms)));
      }
      // Print the last/current one
      Print_Label (Output_File, NULL, sym, size);
      
      Check_Section_Alignment (base, align);
      
      // clear the list
      equivalenced_symbols.clear();
      // reset align
      align = 0;
#endif
      // assume here that if multiple symbols with same offset,
      // are sorted so that largest size is last.
      if (size > 0) {
#ifndef TARG_ST
	fprintf(Output_File, "\t%s %d\n", AS_ALIGN, TY_align(ST_type(sym)));
#endif
	if (Assembly)
	  ASM_DIR_SKIP(Asm_File, (INT32)size);
	if (Lai_Code)
	  fprintf(Lai_File, "\t%s %lld\n", AS_SPACE, size);
	  /*      ASM_DIR_SKIP(Lai_File, size); */
      }
#ifdef TARG_ST
      Add_To_Section_Offset(base, size);
#endif
    }

    if (generate_elf_symbols && !ST_is_export_local(sym)) {
      (void)EMT_Put_Elf_Symbol (sym);
    }
  }
}

/* ====================================================================
 *   Branch_Skips_First_Op
 *
 *   Check if <pred> is a branch to <succ> that skips the first instruction
 *   in the <succ>. This typically happens after filling the branch delay
 *   slot with the first instruction in the target basic block.
 * ====================================================================
 */
static BOOL
Branch_Skips_First_Op (
  BB *pred, 
  BB *succ
) 
{
  OP *op;

  if (BB_next(pred) == succ) return FALSE;

  op = BB_branch_op (pred);
  if (op != NULL) {
    INT i;
    for (i = 0; i < OP_opnds(op); i++) {
      TN *opnd_tn = OP_opnd(op,i);
      if (TN_is_label(opnd_tn) && TN_offset(opnd_tn) != 0) return TRUE;
    }
  }
  return FALSE;
}

#ifdef TARG_ST
/* ====================================================================
 *   Check_If_Should_Align_BB
 *
 *   Check if bb should be aligned, 
 *   and return number of bytes the bb should be aligned with.
 *   Or 0 if no alignment is requested.
 * ====================================================================
 */
// [CG] This function interface has changed and returns the alignment
// in bytes. 
// The options that may define a bb alignment are in priority
// order:
// -OPT:align_loops
// -OPT:align_loopends
// -OPT:align_jumps
// -OPT:align_labels
// -OPT:align_instructions
//
INT32
Check_If_Should_Align_BB (
  BB *bb
)
{
  BOOL bb_is_loop = FALSE;
  BOOL bb_has_jump_to = FALSE;

  if (EMIT_space) return 0;

  /* Detect bbs that are loop head. */
  if (BB_loop_head_bb(bb) == bb &&
      !BB_unrolled_fully(bb)) bb_is_loop = TRUE;
  if (bb_is_loop && Align_Loops) return Align_Loops;

  /* Treat targets of jumps.*/
  if ((BB_preds_len(bb) >= 2) ||
      (BB_preds_len(bb) == 1 &&
       BB_prev(bb) != BB_First_Pred(bb))) bb_has_jump_to = TRUE;
  if (bb_has_jump_to && Align_Jumps) return Align_Jumps;

  /* If Align_Labels is specified all BB are aligned. */
  if (Align_Labels) return Align_Labels;
  
  /* Is Align instructions is specified, check BB that should be aligned. */
  if (Align_Instructions) {
    /* No predecessor, don't align */
    if (BB_prev(bb) == NULL)
      return 0;	
    /* Not frequent enough, so don't align */
    if (BB_freq(bb) <= 1.0)
      return 0;	
#define FREQUENT_BB_DIFF 5.0
    /* Not frequent enough compared to predecessor. */
    if (BB_freq(bb) / BB_freq(BB_prev(bb)) < FREQUENT_BB_DIFF)
      return 0;
    return Align_Instructions;
  }
  return 0;
}
#else

/* ====================================================================
 *   Check_If_Should_Align_BB
 *
 *   Check if bb should be aligned, 
 *   and return number of instructions it should be aligned with.
 * ====================================================================
 */
INT32
Check_If_Should_Align_BB (
  BB *bb, 
  INT32 curpc
)
{
  BBLIST *preds;
  INT32 targpc;
  float skip_freq = 0.01;		/* not zero so can divide */
  float noskip_freq = 0.01;
  INT targ_alignment;
  INT num_of_ops = 0; 		/* zero to begin with */
#define FREQUENT_BB_DIFF 5.0

  /*
   * Align loops for best processor efficiency.
   * Originally checked if bb_loophead, but now just
   * check frequency of block (in case doesn't look like proper loop,
   * but still has high-frequency).
   */
  if (EMIT_space)
    return 0;	/* don't align */
  if (BB_freq(bb) <= 1.0)
    return 0;	/* not frequent enough, so don't align */
  if (BB_prev(bb) == NULL) 
    return 0;	/* nowhere to put nops */
  /* don't want to add nops to previous bb 
   * unless current bb is significantly more frequent. 
   */
  if (BB_freq(bb) / BB_freq(BB_prev(bb)) < FREQUENT_BB_DIFF)
    return 0;

  /* first check whether target is always label+4 */
  FOR_ALL_BB_PREDS (bb, preds) {
    if (Branch_Skips_First_Op (BBLIST_item(preds), bb)) {
      skip_freq += BB_freq(BBLIST_item(preds));
    } else {
      noskip_freq += BB_freq(BBLIST_item(preds));
    }
  }
  if ((skip_freq / noskip_freq) > FREQUENT_BB_DIFF)
    targpc = PC_Incr(curpc);
  else if ((noskip_freq / skip_freq) > FREQUENT_BB_DIFF)
    targpc = curpc;
  else {
    /* mixture of skip and noskip branches, 
     * or just not frequent enough,
     * so don't align */
    return 0;
  }

  if (Trace_Inst) {
#pragma mips_frequency_hint NEVER
    fprintf(TFile, "align_bb %d:  bb_freq = %f, prev_bb_freq = %f, skip_freq = %f, noskip_freq = %f\n", BB_id(bb), BB_freq(bb), BB_freq(BB_prev(bb)), skip_freq, noskip_freq);
  }

  if (Align_Instructions)
    targ_alignment = Align_Instructions;
  else
    targ_alignment = CGTARG_Text_Alignment();
  targ_alignment /= ISA_MAX_INST_BYTES;	/* so word-sized */
  targpc /= ISA_MAX_INST_BYTES;		/* so word-sized */

  //  fprintf(TFile, "  targ_alignment %d\n", targ_alignment);
  //  fprintf(TFile, "  targpc %d\n", targpc);

  if (Get_Trace(TP_EMIT, 0x400)) {
    /* To do this right for R10K, 
     * need to check whether paths/bb-chains
     * cross cache lines.  An easier check, which is also
     * compatible with beast and tfp is to just do the
     * quad-word alignment, which guarantees at least that
     * the cache-line crossing will be quad-aligned.
     * So leave this off by default.
     */
    /* align to cache line size (16 words) */
    /* only do it if block would no longer cross cache line */
#define R10K_PRIMARY_ICACHE_LINE_WORDS	16
    INT targ_cachesize = R10K_PRIMARY_ICACHE_LINE_WORDS;
    UINT ops_till_cacheline = (targ_cachesize - (targpc % targ_cachesize));
    INT aligned_line_crossings = BB_length(bb) / targ_cachesize;
    INT orig_line_crossings = (BB_length(bb) + targ_cachesize - ops_till_cacheline)  / targ_cachesize;
    if (aligned_line_crossings < orig_line_crossings &&
	ops_till_cacheline <= 4) {
      num_of_ops = ops_till_cacheline;
    }
  }
  else {
    num_of_ops = ((targ_alignment - (targpc % targ_alignment)) % targ_alignment);
  }

  return num_of_ops;
}
#endif

#ifdef TARG_ST
/* ====================================================================
 *   Check_If_Should_Align_PU
 * ====================================================================
 */
// [CG]: Changed this function interface.
// This function now returns the requested alignment for the PU if
// any.
// This is an optional feature activated in priority order by:
// -OPT:align_functions,
// -OPT:align_instructions.
// If no specific option is given, 0 must be returned and the function
// will be aligned by the DEFAULT_FUNCTION_ALIGNMENT value.
static INT
Check_If_Should_Align_PU (
  ST *pu
)
{
  INT align;

  if (EMIT_space) return 0;

  if (Align_Functions)
    align = Align_Functions;
  else if (Align_Instructions)
    align = Align_Instructions;
  else 
    align = 0;
  return align;
}
#else
/* ====================================================================
 *   Check_If_Should_Align_PU
 * ====================================================================
 */
static INT
Check_If_Should_Align_PU (
  INT curpc
)
{
  INT q;

  if (Align_Instructions) {
    q = Align_Instructions;
  }
  else if (EMIT_space) {
    return 0;
  }
  else {
    q = CGTARG_Text_Alignment();
  }
  q /= ISA_MAX_INST_BYTES;	                   /* so word-sized */
  return (q - ((curpc/ISA_MAX_INST_BYTES) % q)) % q;
}
#endif


static LABEL_IDX       prev_pu_last_label  = LABEL_IDX_ZERO;
static Dwarf_Unsigned  prev_pu_base_elfsym = 0;
static PU_IDX          prev_pu_pu_idx      = (PU_IDX) 0;
static Dwarf_Unsigned  prev_pu_end_offset_from_last_label = 0;
static char           *prev_pu_last_label_name = NULL;
static Dwarf_Unsigned  prev_pu_last_offset = 0;

static void
cache_last_label_info(LABEL_IDX      label_idx,
                      Dwarf_Unsigned base_elf_idx,
                      PU_IDX         pu_idx,
                      Dwarf_Unsigned end_offset)
{
  prev_pu_last_label  = label_idx;
  prev_pu_base_elfsym = base_elf_idx;
  prev_pu_pu_idx      = pu_idx;
  prev_pu_end_offset_from_last_label = end_offset;
  prev_pu_last_label_name = LABEL_name(label_idx);
  prev_pu_last_offset = Get_Label_Offset(label_idx);
#ifdef TARG_ST
  // [CL]
  Set_LABEL_emitted(label_idx);
#endif
}

static void
end_previous_text_region(pSCNINFO scninfo,
                         INT      end_offset
#ifdef TARG_ST
			 // By default, immediate offset will be used.
			 // Symbolic offset is used only if end_label!=0
			 , Dwarf_Unsigned end_label=LABEL_IDX_ZERO
#endif
			 )
{
#ifdef TARG_ST
  // CL: convert composite PC to actual PC address
  Em_Dwarf_End_Text_Region_Semi_Symbolic(scninfo,
					 PC2Addr(end_offset),
					 Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
							       prev_pu_last_label,
							       prev_pu_base_elfsym,
							       prev_pu_pu_idx,
							       prev_pu_last_label_name,
							       prev_pu_last_offset),
					 end_label,
					 prev_pu_end_offset_from_last_label);
#else
  Em_Dwarf_End_Text_Region_Semi_Symbolic(scninfo,
                                         end_offset,
                                         Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
                                                               prev_pu_last_label,
                                                               prev_pu_base_elfsym,
                                                               prev_pu_pu_idx,
                                                               prev_pu_last_label_name,
                                                               prev_pu_last_offset),
                                         prev_pu_end_offset_from_last_label);
#endif
}

#ifdef TARG_ST

// [CL] create a new label for a new source line
// code borrowed from Gen_Label_For_BB
static LABEL_IDX
Gen_Label_For_Source_Line() {
  char *buf;
  LABEL_IDX lab;
  LABEL *label;
#define  EXTRA_NAME_LEN  32
  static PU_IDX current_debug_pu = 0;
  static int debug_lab_id=0;

  /* Reset debug label id at each new PU */
  if (current_debug_pu != current_pu) {
    current_debug_pu = current_pu;
    debug_lab_id = 0;
  }

  /* Name this label: */
  buf = (char *)alloca(strlen(Cur_PU_Name) + EXTRA_NAME_LEN);
  sprintf(buf, Source_Line_Label_Format, debug_lab_id, Cur_PU_Name);

  label = &New_LABEL(CURRENT_SYMTAB, lab);
  LABEL_Init(*label, Save_Str(buf), LKIND_DEFAULT);

  debug_lab_id ++;
  return lab;
}

// [CL] Create a new label for each new source line
void New_Debug_Line_Set_Label(INT code_address,
			      BOOL last_label_of_PU)
{
#ifdef TARG_STxP70
    // Do not emitted label line in non -g mode.
    // It is important to do nothing, otherwise the created label may be
    // referenced in debug frame information (which is always emitted) to
    // specify function bound (first line to last line label)
    if(!Full_Debug_Info)
        {
            return;
        }
#endif

  BOOL delayed_symb_end = FALSE;
  if (Last_Label > 0)
  {
    cache_last_label_info (Last_Label,
			   Em_Create_Section_Symbol(PU_section),
			   current_pu,
			   PC2Addr(Offset_From_Last_Label));

    // [TTh] Switching to symbolic offset: need to create end label
    // before closing the text region
    if (Dwarf_Require_Symbolic_Offsets()) {
      delayed_symb_end = TRUE;
    }
    else {
      pSCNINFO old_PU_section = em_scn[STB_scninfo_idx(PU_base)].scninfo;
      end_previous_text_region(old_PU_section, 
			       //  Em_Get_Section_Offset(old_PU_section));
			       Addr2PC(code_address));
    }
  }

  Last_Label = Gen_Label_For_Source_Line ();
  Offset_From_Last_Label = 0;



  if (Assembly || Lai_Code) {
    fprintf (Output_File, "%s:\n", 
	     LABEL_name(Last_Label));
    // [CL]
    Set_LABEL_emitted(Last_Label);
  }

  Dwarf_Unsigned new_label;
  new_label = Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
				    Last_Label,
				    ST_elf_index(text_base));

  if (delayed_symb_end) {
    pSCNINFO old_PU_section = em_scn[STB_scninfo_idx(PU_base)].scninfo; 
    end_previous_text_region(old_PU_section, 
			     Addr2PC(code_address),
			     new_label /* first label of new section used as last label of previous section */
			     );
  }

  if (!last_label_of_PU) {
    Em_Dwarf_Start_Text_Region_Semi_Symbolic (PU_section, code_address,
					      new_label,
					      PC2Addr(Offset_From_Last_Label));
  }
}
#endif

/* ====================================================================
 *   Setup_Text_Section_For_PU
 *
 *   Setup, and create if necessary, the text section of the PU.  
 * ====================================================================
 */
static void
Setup_Text_Section_For_PU (
  ST *pu
)
{
  static ST *orig_text_base = NULL;
  ST        *old_base = PU_base;
  INT        i;

  if (text_base == NULL) {
    text_base = SEC_block(_SEC_TEXT);
  }
  orig_text_base = text_base;

  current_pu = ST_pu(pu);

  Initial_Pu_Label = LABEL_IDX_ZERO;

  if (! Object_Code && generate_elf_symbols) {
    // didn't actually write instructions,
    // but want the offset to be up-to-date.
    Em_Change_Section_Origin (
		em_scn[STB_scninfo_idx(orig_text_base)].scninfo,
		text_PC);
  }

  if (Section_For_Each_Function || PU_in_elf_section(current_pu)) {
    /* create new text section */
    text_base = Copy_ST(orig_text_base);
    Set_ST_blk(text_base, Copy_BLK(ST_blk(orig_text_base)));
#if 0
    if (Get_Trace ( TP_EMIT,128 )) {
      // use same .text name for each section
      Set_ST_name (text_base, ST_name_idx(orig_text_base));
    }
    else {
      char buf[16];
      sprintf(buf, "%s.", ELF_TEXT);
      Set_ST_name (text_base, Save_Str2(buf, ST_name(pu)));
    }
#else
    // use same .text name for each section
    char buf[16];
    sprintf(buf, "%s.", ELF_TEXT);
    Set_ST_name (text_base, Save_Str2(buf, ST_name(pu)));
#endif

    Set_STB_size (text_base, 0);
    Set_STB_scninfo_idx(text_base, 0);
    Set_STB_section_idx(text_base, STB_section_idx(orig_text_base));
    Init_Section (text_base);
    text_PC = 0;	/* starting new offsets */
  }
  else if (ST_base(pu) != text_base) {
    // e.g. for section attributes.
    text_base = ST_base(pu);
    text_PC = STB_size(text_base);
  }

  Set_STB_scninfo_idx(SP_Sym, STB_scninfo_idx(text_base));
  Set_STB_scninfo_idx(FP_Sym, STB_scninfo_idx(text_base));

#ifdef TARG_ST
  // [CG] Setup cold section from here as we need text_base
  Create_Cold_Text_Section();
#endif

  cur_section = text_base;
  if (generate_elf_symbols) {
    text_section = em_scn[STB_scninfo_idx(text_base)].scninfo;
    PU_section = text_section;
    i = Em_Get_Section_Offset (PU_section);
    Is_True(i == text_PC, ("Setup_Text_Section_For_PU: PC doesn't match"));
    text_PC = i;
  }

  if (Assembly) fprintf (Asm_File, "\t%s %s\n", AS_SECTION, ST_name(text_base));
  if (Lai_Code) fprintf (Lai_File, "\n\t%s %s\n", AS_SECTION, ST_name(text_base));

#ifdef TEMPORARY_STABS_FOR_GDB
  // This is an ugly hack to enable basic debugging for IA-32 target
  if (Assembly && Debug_Level > 0) {
    static BOOL marked_text_start = FALSE;
    if (!marked_text_start) {
      fprintf(Asm_File, ".Ltext0:\n");
      marked_text_start = TRUE;
    }
  }
#endif

#ifdef TARG_ST
  // [CG] Check_If_Should_Align_PU() is now used from EMT_Emit_PU()
#else
  i = Check_If_Should_Align_PU (text_PC);
  if (i != 0) {
    if (Assembly || Lai_Code) {
      UINT32 tmp, power;
      power = 0;
      for (tmp = STB_align(text_base); tmp > 1; tmp >>= 1) power++;
      if (Assembly) ASM_DIR_ALIGN(power, text_base);
      if (Lai_Code) fprintf(Lai_File, "\t%s %d\n", 
                                    AS_ALIGN,STB_align(text_base));
    }
#if 0
    if (Object_Code) {
      // these bytes will never be executed so just insert 0's and
      // then we don't have to worry about how to generate a nop for
      // the target arch.
      Em_Add_Zeros_To_Scn (PU_section, i * ISA_MAX_INST_BYTES, 1);
    }
#endif
    // increment text_PC by 'num' bundles
    text_PC = text_PC + (i * ISA_MAX_INST_BYTES);
  }
#endif

  // hack for supporting dwarf generation in assembly (suneel)
#ifndef TARG_ST
  // [CL] The label generated here is NOT to be used for
  // debug. Last_Label has been reset in EMT_Emit_PU after
  // emission of the previous PU
  // Initial_Pu_Label has been reset above
  Last_Label = Gen_Label_For_BB (REGION_First_BB);
  Offset_From_Last_Label = 0;
  if (Initial_Pu_Label == LABEL_IDX_ZERO) {
    Initial_Pu_Label = Last_Label;
  }
#else
  Initial_Pu_Label = Gen_Label_For_BB (REGION_First_BB);
#endif

  /* CL: Start a new "text region" for Dwarf at each
     function start (instead of only when changing
     sections */
#ifndef TARG_ST
  /* check if we are changing sections. */
  if (text_base != old_base) {
#endif
    if (generate_elf_symbols && old_base != NULL) {
      pSCNINFO old_section = em_scn[STB_scninfo_idx(old_base)].scninfo;
      // Arange is symbolic; line number entries (if used) are not.
#ifndef TARG_ST
      // [CL] this is already performed in New_Debug_Line_Set_Label
      end_previous_text_region(old_section, 
                                   Em_Get_Section_Offset(old_section));
#endif
    }
#ifndef TARG_ST
    // [CL] new regions are started in New_Debug_Line_Set_Label
    if (generate_dwarf) {
    	Em_Dwarf_Start_Text_Region_Semi_Symbolic (PU_section, text_PC,
			Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
			Last_Label,
			ST_elf_index(text_base)),
			Offset_From_Last_Label);
    }
  }
#endif

  PC = text_PC;
  PU_base = text_base;

  return;
}

/* ====================================================================
 *   put_TN_comment
 *
 *   Add comment about the TN to comment buffer.
 * ====================================================================
 */
static void
put_TN_comment (
  TN *t, 
  BOOL add_name, 
  vstring *comment
)
{
  if (!add_name) return;	// don't duplicate name
  if (!TN_is_constant(t)) return;

  INT64 val = TN_offset(t);

  if (TN_is_symbol(t)) {
    if (ST_class(TN_var(t)) == CLASS_CONST) {
      *comment = vstr_concat (*comment, 
		   Targ_Print (NULL, ST_tcon_val(TN_var(t))) );
    }
    else {
#ifdef TARG_ST
      if (PU_src_lang(Get_Current_PU()) == PU_CXX_LANG) {
	/* (cbr) demangle name */
	int status_cp_name ;
	char *cp_name =  abi::__cxa_demangle(ST_name(TN_var(t)), NULL, NULL, &status_cp_name) ;
	if (!status_cp_name && cp_name) {
	  *comment = vstr_concat(*comment, cp_name);
	  ::free(cp_name) ;
	}
      }
      else
#endif
      *comment = vstr_concat (*comment, ST_name(TN_var(t)));
    }
    if (TN_offset(t) != 0) {
      vstr_sprintf (comment, vstr_len(*comment), "%+lld", val);
    }
  }
  else if (TN_is_label(t) && val != 0) {
    *comment = vstr_concat (*comment, LABEL_name(TN_label(t)));
    vstr_sprintf (comment, vstr_len(*comment), "%+lld", val); 
#ifdef TARG_ST
    // [CL]
    Set_LABEL_emitted(TN_label(t));
#endif
  }
}

/* ====================================================================
 *   r_apply_l_const
 *
 *   Format the given constant TN's "value" into the given buffer.
 *   Return whether the symbol name should be added to comments.
 * ====================================================================
 */
static BOOL
r_apply_l_const (
  OP *op,		/* OP with constant operand */
  INT opidx,		/* OP index of constant TN of some sort */
  vstring *buf		/* A buffer to format it into */
)
{
  TN *t = OP_opnd(op, opidx);
  INT paren = 0;
  BOOL hexfmt = FALSE;
  BOOL print_TN_offset = TRUE;
  BOOL add_name = FALSE;
  ST *st;
  INT64 val;

#ifdef TARG_ARM
  /* Special case of register list. This should become generic 
     when register will be supported by targinfo and TNs */
  if(CGEMIT_Register_List(op,opidx,buf)) {
    return TRUE;
  }
#endif

  /* special case for stack symbols */
  if (TN_is_symbol(t)) {
    ST *base_st;
    INT64 base_ofst;

    st = TN_var(t);
#ifdef TARG_ST
    base_st = Base_Symbol (st);
#else
    Base_Symbol_And_Offset (st, &base_st, &base_ofst);
#endif
    if (base_st == SP_Sym || base_st == FP_Sym) {
#ifdef TARG_ST
      base_ofst = Base_Offset (st);
#endif
        if (Pointer_Size <= 4) {
            INT32 val32 = CGTARG_TN_Value (t, base_ofst);
            vstr_sprintf (buf, 
                          vstr_len(*buf), 
                          "%d",
                          val32);
        } else { 
            INT64 val64 = CGTARG_TN_Value (t, base_ofst);
            vstr_sprintf (buf, 
                          vstr_len(*buf), 
                          "%lld",
                          val64);
        }
      return TRUE;
    }
  }

  val = TN_offset(t);

  if (TN_is_symbol(t)) { // not SP or FP relative
    st = TN_var(t);

    if (ST_class(st) == CLASS_CONST) {
#ifdef TARG_ST
      //
      // Arthur: you can not always address a constant as an offset
      //         from the section
      //

      TCON tc = ST_tcon_val(st);
      //
      // For floats make a bit pattern, others - symbolic name
      //
      if (MTYPE_is_float(TCON_ty(tc)) && !CG_floating_const_in_memory) {

	union {
	  INT32 i;
	  float f;
	} val;

#if __GNUC__ < 3
	// [CL] copying floats on x86 with gcc-2.95 implies conversions
	// which may change the copied value (especially in our case where
	// the lo/hi parts of a double are not generally "plain" floats
	// They are likely to be NaNs and as such candidate to conversions
	// which should apply. It would be much cleaner to split a double
	// into 2 parts which are *not* float and have special types for
	/// the compiler
	// In addition, the union above implies that
	// sizeof(INT32) == sizeof(float)
	memcpy(&val.f, &tc.vals.f.fval, sizeof(val.f));
#else
	val.f = TCON_fval(tc);
#endif
	vstr_sprintf (buf, vstr_len(*buf), "%#x", val.i);

      }
      else {
	//
	// Print out a symbolic name
	//
	if (TN_relocs(t) != 0) {

	  hexfmt = TRUE;
	  print_TN_offset = TRUE;
	  // add name to comments in case was confused by gp_disp.
	  add_name = TRUE;
	  paren = TN_Relocs_In_Asm (t, st, buf, &val);
	  if(TN_Reloc_has_parenthesis(TN_relocs(t))) {
	    *buf = vstr_concat (*buf, "(" );
	  }
	}
#ifndef TARG_ST
	vstr_sprintf (buf, vstr_len(*buf), "%s_UNNAMED_CONST_%d", Local_Label_Prefix, ST_tcon(st));
#else
	r_qualified_tcon_name(st, buf);
#endif
	//	// call put_symbol so that we emit .type info, once per symbol
	// (void) EMT_Put_Elf_Symbol (st);
      }
#else
      *buf = vstr_concat(*buf, ST_name(st));
      if (*Symbol_Name_Suffix != '\0')
	*buf = vstr_concat(*buf, Symbol_Name_Suffix);
#endif
    }
    else {
      if (ST_sclass(st) != SCLASS_COMMON) {
	// call put_symbol so that we emit .type info, once per symbol
	(void)EMT_Put_Elf_Symbol (st);
      }

      if (TN_relocs(t) != 0) {
#if 0
	// use base if referring to current pu or local data
	if (CGEMIT_Use_Base_ST_For_Reloc (TN_relocs(t), st)) {
	  ST *base_st;
	  INT64 base_ofst;
	  Base_Symbol_And_Offset (st, &base_st, &base_ofst);
	  val += base_ofst;
	  st = base_st;
	}
#endif
	if (Use_Separate_PU_Section(current_pu,st)) {
	  /* use PU text section rather than generic one */
	  st = PU_base;
	}

	hexfmt = TRUE;
	print_TN_offset = TRUE;
	// add name to comments in case was confused by gp_disp.
	add_name = TRUE;
	paren = TN_Relocs_In_Asm (t, st, buf, &val);
	if(TN_Reloc_has_parenthesis(TN_relocs(t))) {
	  *buf = vstr_concat (*buf, "(" );
	}
	r_qualified_name (st, buf);
      } 
      else {
	r_qualified_name (st, buf);
      }
    } /* not CLASS_CONST */
  }
  else if ( TN_is_label(t) ) {
    if (val != 0) {
      // don't use "." cause that can have varying meaning
      // when have multiple instruction slots.
      // Instead just do label+offset.
      *buf = vstr_concat (*buf, LABEL_name(TN_label(t)));
#ifdef TARG_ST
      // [CL]
      Set_LABEL_emitted(TN_label(t));
#endif
      vstr_sprintf (buf, vstr_len(*buf), "%+lld", val); 
    }
    else {
      *buf = vstr_concat(*buf, LABEL_name(TN_label(t)));
#ifdef TARG_ST
      // [CL]
      Set_LABEL_emitted(TN_label(t));
#endif
      if (isdigit(LABEL_name(TN_label(t))[0])) {
      	*buf = vstr_concat(*buf, 
                (PC > Get_Label_Offset(TN_label(t))) ? "b" : "f");
      }
    }
    print_TN_offset = FALSE;
  }
  else if (TN_is_tag(t)) {
    *buf = vstr_concat(*buf, LABEL_name(TN_label(t)));
#ifdef TARG_ST
    // [CL]
    Set_LABEL_emitted(TN_label(t));
#endif
    print_TN_offset = FALSE;
  }
  else if (TN_is_enum(t)) {
    if (TN_enum(t)<ISA_ECV_STATIC_MAX) {  /* fix [vcdv] */
      vstr_sprintf (buf, vstr_len(*buf), "%s", ISA_ECV_Name(TN_enum(t)) );
    }
    print_TN_offset = FALSE;	/* because value used instead */
  }
  else if ( TN_has_value(t) ) {
    long long value = TN_value(t);
#ifdef TARG_ST
    /* [VCdV] implementation of NEGATIVE offset literal class. asm output
       for a negative offset is switch to its oppposite value */
    if (opidx < OP_fixed_opnds(op)) {
      ISA_LIT_CLASS lc = OP_opnd_lit_class(op, opidx);
      if (ISA_LC_Is_Negative(lc)  ) {
        value=-value;
      }
    }
#endif

    if(!hexfmt) { hexfmt = CGEMIT_TN_Value_In_Hexa_Format(op,t); }
    if ( TN_size(t) <= 4 )
      vstr_sprintf (buf, vstr_len(*buf), 
		(hexfmt ? "0x%x" : "%d"), (mINT32)value );
    else
      vstr_sprintf (buf, vstr_len(*buf), 
      		(hexfmt ? "0x%llx" : "%lld"), value );
    print_TN_offset = FALSE;	/* because value used instead */
  }
  else {
    #pragma mips_frequency_hint NEVER
    FmtAssert (FALSE, ("r_apply_l_const: illegal constant TN"));
  }

  if (print_TN_offset && (val != 0)) {
    vstr_sprintf (buf, vstr_len(*buf), "%+lld", val );
  }

  while ( paren > 0 ) {
    *buf = vstr_concat(*buf, ")");
    --paren;
  }
  return add_name;
}

/* ====================================================================
 *   print_prefetch_info(char *comment, WN *wn)
 *
 *   add prefetch info comments to the assembler listing.
 * ====================================================================
 */
static void 
print_prefetch_info(
  vstring *comment, 
  WN *wn
)
{

  if (!wn) return;

  if (WN_pf_stride_1L (wn) != 0) {
    *comment = vstr_concat(*comment, "L1");
    return;
  }

  if (WN_pf_stride_2L (wn) != 0 ) {
    *comment = vstr_concat(*comment, "L2");
  }
}

/* ====================================================================
 *   r_value
 *
 *   Returns TN value depending of its type.
 * ====================================================================
 */
static INT64
r_value( TN *tn )
{
  if(TN_is_constant(tn)) {
    if(TN_has_value(tn)) { return TN_value(tn); }
    if(TN_is_symbol(tn)) { return TN_offset(tn); }
    if(TN_is_label(tn))  { return TN_offset(tn); }
    if(TN_is_enum(tn))   { return TN_enum(tn); }
  }
  if(TN_is_register(tn)) { return TN_register(tn)-1; }
  return 0;
}

/* ====================================================================
 *   r_assemble_opnd
 * ====================================================================
 */
static char*
r_assemble_opnd (
  OP      *op,
  INT      i,
  vstring *buf,
  BOOL    *add_name
)
{
  char  vname[10];
  INT   start = vstr_len(*buf);	// start of operand string
  TN   *t = OP_opnd(op,i);

  if (TN_is_constant(t)) {
    *add_name |= r_apply_l_const (op, i, buf);
  }
  else {
    const char *rname;
    ISA_REGISTER_CLASS rc = TN_register_class(t);
    REGISTER reg = TN_register(t);

    // If register is physical, print its real name, otherwise
    // virtual:
    if (reg != REGISTER_UNDEFINED) {

      ISA_REGISTER_SUBCLASS sc = OP_opnd_reg_subclass(op,i);

#ifdef TARG_ST
      // [TTh] Raise internal error if current operand received an unallowed register
      FmtAssert((sc == ISA_REGISTER_SUBCLASS_UNDEFINED ||
		 REGISTER_SET_MemberP(REGISTER_SUBCLASS_members(sc), reg)),
		("r_assemble_opnd: illegal register <%s> for register subclass <%s>\n",
		 REGISTER_name(rc, reg), REGISTER_SUBCLASS_name(sc)));
#endif
      if (REGISTER_SET_MemberP(REGISTER_SUBCLASS_members(sc), reg)
	      && REGISTER_SUBCLASS_reg_name(sc, reg)) {
	rname = REGISTER_SUBCLASS_reg_name(sc, reg);
      } 
      else if (List_Software_Names) {
	rname = ABI_PROPERTY_Reg_Name(rc, REGISTER_machine_id(rc, reg));
      } 
      else {
	rname = REGISTER_name(rc, reg);
      }
    }
    else {
      sprintf(vname, "%s%d", ISA_REGISTER_CLASS_Symbol(rc), TN_number(t));
      rname = vname;
    }

#if defined( TARG_ST200 )
  /* (cbr) predicate is a regular operand format on st235 */
    if (0) {
#elif defined( TARG_STxP70 )
    if ( (OP_cond(op) && i == OP_find_opnd_use(op,OU_condition)) || 
	 (OP_has_predicate(op) && i == OP_find_opnd_use(op,OU_predicate)) ||
	 (OP_fixed_results(op) > 0 && OP_result_reg_class(op,0) == ISA_REGISTER_CLASS_gr && i == 0 &&
	  !OP_load(op))) {
#else
    if (OP_has_predicate(op) && i == OP_find_opnd_use(op,OU_predicate)) {
#endif

#ifdef TARG_IA64
      vstr_sprintf(buf, start, ISA_PRINT_PREDICATE, rname);
#elif defined( TARG_STxP70 )
      // [JV] Now treated in isa_print.
      vstr_sprintf(buf, start, "%s", rname);
#else
      if (TOP_is_guard_t(OP_code(op)))
	vstr_sprintf(buf, start, True_Predicate_Format, rname);
      else if (TOP_is_guard_f(OP_code(op)))
	vstr_sprintf(buf, start, False_Predicate_Format, rname);
#endif
    }
    else {
      *buf = vstr_concat(*buf, rname);
    }
  }

  // need end-of-string between each operand
  *buf = vstr_append(*buf, '\0');

  return vstr_str(*buf)+start;
}

/* ====================================================================
 *   r_assemble_result
 * ====================================================================
 */
static char*
r_assemble_result (
  OP *op,
  INT i,
  vstring *buf
)
{
  char  vname[10];
  INT start = vstr_len(*buf);	// start of operand string
  TN *t = OP_result(op,i);

  const char *rname;
  ISA_REGISTER_SUBCLASS sc = OP_result_reg_subclass(op,i);
  ISA_REGISTER_CLASS rc = TN_register_class(t);
  REGISTER reg = TN_register(t);

  FmtAssert (t != NULL && !TN_is_constant(t),
	                     ("r_assemble_list: illegal result tn"));

  if (reg != REGISTER_UNDEFINED) {

#ifdef TARG_ST
      // [TTh] Raise internal error if current result received an unallowed register
      FmtAssert((sc == ISA_REGISTER_SUBCLASS_UNDEFINED ||
		 REGISTER_SET_MemberP(REGISTER_SUBCLASS_members(sc), reg)),
		("r_assemble_result: illegal register <%s> for register subclass <%s>\n",
		 REGISTER_name(rc, reg), REGISTER_SUBCLASS_name(sc)));
#endif
    if (REGISTER_SET_MemberP(REGISTER_SUBCLASS_members(sc), reg)
	  && REGISTER_SUBCLASS_reg_name(sc, reg)) {
      rname = REGISTER_SUBCLASS_reg_name(sc, reg);
    } 
    else if (List_Software_Names) {
      rname = ABI_PROPERTY_Reg_Name(rc, REGISTER_machine_id(rc, reg));
    } 
    else {
      rname = REGISTER_name(rc, reg);
    }
  }
  else {
    sprintf(vname, "%s%d", ISA_REGISTER_CLASS_Symbol(rc), TN_number(t));
    rname = vname;
  }

  *buf = vstr_concat(*buf, rname);
  *buf = vstr_append(*buf, '\0');	// increment vstr length
  return vstr_str(*buf)+start;
}

/* ====================================================================
 *   CGEMIT_Print_Intrinsic_OP
 *
 *   TODO: target-dependent.
 *         On a given target, a TOP_intrncall format must be known.
 *         For example, ST100 must maintain the following convension:
 *         TOP_intrncall -- var_opnds;
 *         opnd[0]       -- predicate
 *         opnd[1]       -- symbolic TN with intrinsic name
 *
 * ====================================================================
 */
static INT
CGEMIT_Print_Intrinsic_OP (
  OP *op,
  ISA_PRINT_OPND_INFO *result,
  ISA_PRINT_OPND_INFO *opnd,
  FILE *f
)
{
  INT i;
  INT st;

  // TODO: add description of intrinsic predicated or not.
  //       add description of printing format ?
  //
  // For now assume all intrinsics are predicated ? opnd[0]
  //
  st = fprintf (f, "%s\t", opnd[0].name);

  //
  // print name
  //
  st += fprintf (f, "%s\t", opnd[1].name);

  //
  // print results
  //
  for (i = 0; i < OP_results(op); i++) {
    st += fprintf (f, "%s, ", result[i].name);
  }

  //
  // print opnds
  //
  for (i = 2; i < OP_opnds(op)-1; i++) {
    st += fprintf (f, "%s, ", opnd[i].name);
  }
  if (OP_opnds(op) > 2) {
    st += fprintf (f, "%s", opnd[OP_opnds(op)-1].name);
  }

  if (st == -1) {
    FmtAssert(FALSE,("CGEMIT_Print_Intrinsic_OP: fprintf failed"));
  }
  else return st;
}

/* ====================================================================
 *   r_assemble_list
 *
 *   Emit a pseudo-assembly listing for the given OP.
 * ====================================================================
 */
static void 
r_assemble_list (
  OP *op,		/* The operation being listed */
  BB *bb
)
{
  ISA_PRINT_OPND_INFO result[ISA_OPERAND_max_results];
  ISA_PRINT_OPND_INFO opnd[ISA_OPERAND_max_operands];
  vstring buf = vstr_begin(LBUF_LEN);
  INT i;
  INT lc = 0;
  BOOL add_name = FALSE;

#if 0
  Emit_Unwind_Directives_For_OP(op, Asm_File);
#endif

#ifdef GAS_TAGS_WORKED
// un-ifdef this when gas can handle tags inside explicit bundle
#ifdef TARG_ST
  if (Get_OP_Tag(op)) {
#else
  if (OP_has_tag(op)) {
#endif
	fprintf(Asm_File, "%s:\n", LABEL_name(Get_OP_Tag(op)));
#ifdef TARG_ST
	// [CL]
	Set_LABEL_emitted(Get_OP_Tag(op));
#endif
  }
#endif

#if defined(TARG_ST100) && !defined(__ASM_SUPPORTS_REGMASK_OPERAND)
extern BOOL Hack_For_Printing_Push_Pop (OP *op, FILE *file);
// Arthur: the GHS assembler isn't able to take
//         a register mask as operand. However, push/pop
//         is described in the architecture as having a 
//         mask as an operand. So, I need this hack ...
  if (Assembly && Hack_For_Printing_Push_Pop (op, Asm_File)) {
    return ;
  }
#endif

  for (i = 0; i < OP_opnds(op); i++) {
    TN   *t = OP_opnd(op,i);
    opnd[i].name = r_assemble_opnd(op, i, &buf, &add_name);
    opnd[i].value = r_value(t);
    
  }

  for (i = 0; i < OP_results(op); i++) {
    TN *t = OP_result(op,i);
    result[i].name = r_assemble_result (op, i, &buf);
    result[i].value = r_value(t);
  }

  if (Assembly) {
    lc = TI_ASM_Print_Inst(OP_code(op), result, opnd, Asm_File);
  }
  if (Lai_Code) {
    fputc ('\t', Lai_File);
    if (OP_code(op) == TOP_intrncall) {
      lc = CGEMIT_Print_Intrinsic_OP(op, result, opnd, Lai_File);
    }
    else {
      lc = TI_ASM_Print_Inst(OP_code(op), result, opnd, Lai_File);
    }
  }
  FmtAssert (lc != TI_RC_ERROR, ("%s", TI_errmsg));
  vstr_end(buf);

#if 0
  if (OP_end_group(op) && Assembly) {
    lc += fprintf(Asm_File, " %s", ISA_PRINT_END_GROUP);
  }
#endif

  while (lc < 30) {
    if (Assembly || Lai_Code) fputc (' ', Output_File);
    lc++;
  }

  /* emit TN comments: */
  vstring comment = vstr_begin(LBUF_LEN);

#ifdef TARG_ST
  char cycle_info[30];
  cycle_info[0] = '\0';
#endif

  for (i = 0; i < OP_results(op); i++) {
    TN *tn = OP_result(op, i);
    if (TN_is_save_reg(tn)) {
      vstr_sprintf (&comment, vstr_len(comment), "(sv:%s)", 
                  REGISTER_name(TN_save_rclass(tn), TN_save_reg(tn)));
    }
  }
  for (i = 0; i < OP_opnds(op); i++) {
    put_TN_comment (OP_opnd(op,i), add_name, &comment);
  }

#ifndef TARG_ST
  /* SC: Do this only if we have a comment.  We do not know
     yet, so do it later.
  */
  if (Assembly || Lai_Code) fprintf (Output_File, "\t%s", ASM_CMNT);
#endif
  if (vstr_len(comment) == 0) {
    WN *wn = Get_WN_From_Memory_OP (op);
    if (wn && Alias_Manager) {
      char tbuf[LBUF_LEN];
      tbuf[0] = '\0';
      Print_alias_info (tbuf, Alias_Manager, wn);
      comment = vstr_concat(comment, tbuf);
    }
    if (wn && OP_prefetch(op) && vstr_len(comment) == 0) {
      print_prefetch_info(&comment, wn);
    }
  }

  /* Add target PU name as comment for normal call and tail call OPs.
   */
  if ((OP_call(op) || OP_tail_call(op)) && vstr_len(comment) == 0) {
    ANNOTATION *ant = ANNOT_Get (BB_annotations(bb), ANNOT_CALLINFO);
    if (ant != NULL) {
	ST *call_sym = CALLINFO_call_st(ANNOT_callinfo(ant));
	if (call_sym != NULL) {
#ifdef TARG_ST
      if (PU_src_lang(Get_Current_PU()) == PU_CXX_LANG) {
	/* (cbr) demangle name */
	int status_cp_name ;
	char *cp_name =  abi::__cxa_demangle(ST_name(call_sym), NULL, NULL, &status_cp_name) ;
	if (!status_cp_name && cp_name) {
	  comment = vstr_concat(comment, cp_name);
	  ::free(cp_name) ;
	}
      }
#endif
            comment = vstr_concat(comment, ST_name(call_sym));
	}
    }
  }

#ifdef TARG_ST
  if (OP_ssa_move(op) && vstr_len(comment) == 0) {
    comment = vstr_concat(comment, "ssa_move");
  }
#endif

#ifdef TARG_ST
  if (Assembly || Lai_Code) {
    if (Trace_Sched && (OP_scycle(op) >= 0)) {
      sprintf (cycle_info, "(cycle %d)  ", OP_scycle(op));
    }
    if ((OP_scycle(op) >= 0) &&
	(OP_scycle(op) < Bundle_Count) &&
	(!OP_xfer(op) || Trace_Sched))
      DevWarn("BB %d, bundle %d: operation %s was scheduled at cycle %d",
	      BB_id(bb), Bundle_Count, TOP_Name(OP_code(op)), OP_scycle(op));
  }
#endif

#ifdef TARG_ST
  if (! CGEMIT_NewLine_Before_ISA_PRINT_END_GROUP() &&
      OP_end_group(op) && Assembly) {
    fprintf(Output_File, "\t %s", ISA_PRINT_END_GROUP);
  }

  if ((Assembly || Lai_Code)
      && List_Notes
      && (vstr_len(comment) > 0 || strlen(cycle_info) > 0)) {
      fprintf (Output_File, "\t %s %s%s", ASM_CMNT, cycle_info, vstr_str(comment));
  }
  fputc ('\n', Output_File);

  if (CGEMIT_NewLine_Before_ISA_PRINT_END_GROUP() &&
      OP_end_group(op) && Assembly) {
    fprintf(Output_File, "\t %s\n", ISA_PRINT_END_GROUP);
  }

#else
    fprintf (Output_File, "  %s\n", vstr_str(comment));
#endif

  vstr_end(comment);

  return;
}

/* ====================================================================
 *   Verify_Operand
 *
 *   Verify that the specified operand is valid for the particular
 *   instruction operand.
 * ====================================================================
 */
static void 
Verify_Operand (
  const ISA_OPERAND_INFO *oinfo,
  OP *op, 
  INT opnd, 
  BOOL is_result
)
{
  const ISA_OPERAND_VALTYP *vtype =   is_result 
				    ? ISA_OPERAND_INFO_Result(oinfo, opnd)
				    : ISA_OPERAND_INFO_Operand(oinfo, opnd);
  TN *tn = is_result ? OP_result(op, opnd) : OP_opnd(op, opnd);
  const char * const res_or_opnd = is_result ? "result" : "operand";

  if (ISA_OPERAND_VALTYP_Is_Register(vtype)) {
    REGISTER_SET class_regs;
    ISA_REGISTER_SUBCLASS sc = ISA_OPERAND_VALTYP_Register_Subclass(vtype);
    ISA_REGISTER_CLASS rc = ISA_OPERAND_VALTYP_Register_Class(vtype);
    REGISTER reg = TN_register(tn);

    if (!TN_is_register(tn)) goto incorrect_register;
    if (TN_register_class(tn) != rc) goto incorrect_register;
    if (reg != REGISTER_UNDEFINED) {
      class_regs =   (sc == ISA_REGISTER_SUBCLASS_UNDEFINED)
	? REGISTER_CLASS_universe(rc)
	: REGISTER_SUBCLASS_members(sc);
      if (!REGISTER_SET_MemberP(class_regs, reg)) goto incorrect_register;
    }
#ifdef TARG_ST
    if (sc != ISA_REGISTER_SUBCLASS_UNDEFINED) {
      const ISA_REGISTER_SUBCLASS_INFO *info = ISA_REGISTER_SUBCLASS_Info (sc);
      if (ISA_REGISTER_SUBCLASS_INFO_Count (info) == 1) {
	// Register TN for a singleton subclass must use a
	// dedicated register.
	if (! TN_is_dedicated(tn)) goto incorrect_register;
      }
    }

#endif
    return;
  incorrect_register:
#if 0
     {
      int i;
      int opndnum = OP_opnds(op);
      int resnum = OP_results(op);
      fprintf(TFile, "Incorrect register operand in:\n");
      fprintf(TFile, "BB:%d  %s ",
	      BB_id(OP_bb(op)),TOP_Name(OP_code(op)));
      for (i = 0; i < resnum; i++) {
	fprintf(TFile," ");
	Print_TN(OP_result(op, i),TRUE);
      }
      fprintf(TFile,"=");
      for (i = 0; i < opndnum; i++) {
	fprintf(TFile," ");
	Print_TN(OP_opnd(op, i),TRUE);
      }
      fprintf(TFile,"\n");
    }
#endif
    FmtAssert(0,
	      ("incorrect register for %s %d in opcode %s", 
	       res_or_opnd, opnd, TOP_Name(OP_code(op))));
  } else if (ISA_OPERAND_VALTYP_Is_Literal(vtype)) {
#if Is_True_On
    /* More verbose error in debug mode. */
    if(!TN_is_constant(tn)){
      int i;
      int opndnum = OP_opnds(op);
      int resnum = OP_results(op);
      fprintf(TFile, "Incorrect register operand in:\n");
      fprintf(TFile, "BB:%d  %s ",
	      BB_id(OP_bb(op)),TOP_Name(OP_code(op)));
      for (i = 0; i < resnum; i++) {
	fprintf(TFile," ");
	Print_TN(OP_result(op, i),TRUE);
      }
      fprintf(TFile,"=");
      for (i = 0; i < opndnum; i++) {
	fprintf(TFile," ");
	Print_TN(OP_opnd(op, i),TRUE);
      }
      fprintf(TFile,"\n");
    }
#endif
    FmtAssert(TN_is_constant(tn),
	     ("%s %d is not a constant", res_or_opnd, opnd));

    if (TN_has_value(tn)) {
      ISA_LIT_CLASS lc = ISA_OPERAND_VALTYP_Literal_Class(vtype);
      INT64 imm = TN_value(tn);

       if ((TFile != stdout) && !ISA_LC_Value_In_Class(imm, lc)) {
         Print_OP_No_SrcLine (op);
       }
#if Is_True_On
       /* More verbose error in debug mode. */
       if (!ISA_LC_Value_In_Class(imm, lc)) {
         fprintf(TFile,  "in operation:");
         Print_OP_No_SrcLine(op);
       }
#endif
       FmtAssert(ISA_LC_Value_In_Class(imm, lc),
                 ("literal %lld for %s %d is not in range for top: %s", imm, res_or_opnd, opnd, TOP_Name(OP_code(op))));
    } else if (TN_is_label(tn)) {

#ifndef TARG_ST
      // [CG]: This check is wrong, does not handle variable bundles
#if Is_True_On
      LABEL_IDX lab = TN_label(tn);
      INT64 offset = TN_offset(tn);
      INT64 val = Get_Label_Offset(lab) + offset;
      if (CG_opt_level > 0 && CFLOW_Enable && !OP_likely(op)) {
	INT nextpc = PC + sizeof(ISA_BUNDLE);
	if (PROC_has_branch_delay_slot()) nextpc += sizeof(ISA_PACK_INST);

	if (val == nextpc) {
	  DevWarn("branch to next instruction at PC=0x%x", PC);

	  Print_OP_No_SrcLine(op);
	  fprintf(TFile, " PC = %d; TN_offset = %lld\n", PC, offset);
	  fprintf(TFile, " val = %lld; nextpc = %d\n", val, nextpc);

	}
      }
      if (Get_Label_Offset(lab) == 0 && offset == 0) {
	BBLIST *item;
	BB *label_bb = Get_Label_BB(lab);
	BOOL okay = FALSE;
	FOR_ALL_BB_SUCCS(OP_bb(op), item) {
	  if (BBLIST_item(item) == label_bb) okay = TRUE;
	}
	if (!okay) {
	  DevWarn("branch to 0? (possible bug at PC=0x%x, label %s)", 
		   PC, LABEL_name(lab));
	}
      }
#endif
#endif
    }
  } else if (ISA_OPERAND_VALTYP_Is_Enum(vtype)) {
    ISA_ENUM_CLASS_VALUE ecv;
    ISA_ENUM_CLASS ec = ISA_OPERAND_VALTYP_Enum_Class(vtype);
    ISA_ENUM_CLASS_VALUE first_ecv = ISA_EC_First_Value(ec);
    ISA_ENUM_CLASS_VALUE last_ecv = ISA_EC_Last_Value(ec);

    FmtAssert(TN_is_enum(tn),
	      ("%s %d is not an enum", res_or_opnd, opnd));

    ecv = TN_enum(tn);
    FmtAssert(ecv >= first_ecv && ecv <= last_ecv,
	      ("enum for %s %d is not in range", res_or_opnd, opnd));
  } else {
    FmtAssert(FALSE, ("unhandled vtype in Verify_Operand"));
  }
}

#ifdef TARG_ST
/* ====================================================================
 *   Verify_Instruction_Simple
 *
 *   Verify that the specified OP contains valid information for
 *   the instruction it represents.
 * ====================================================================
 */
void 
Verify_Instruction_Simple (
  OP *op
)
{
  INT i;
  const ISA_OPERAND_INFO *oinfo;
  TOP top = OP_code(op);


  if (TOP_is_dummy(top)  || TOP_is_simulated(top))
    return;

  // ??? check for valid topcode?

  FmtAssert(ISA_SUBSET_LIST_Member(ISA_SUBSET_List, top),
	    ("%s is a member of available ISA subsets", 
	     TOP_Name(top)));

  oinfo = ISA_OPERAND_Info(top);

  INT results = OP_results(op);
  if (results != OP_fixed_results(op)) {
    FmtAssert((TOP_is_var_opnds(top) || OP_call(op)) && results > OP_fixed_results(op),
	      ("wrong number of results (%d) for %s",
	       results,
	       TOP_Name(top)));
    results = OP_fixed_results(op); // can only verify fixed results
  }
  for (i = 0; i < results; ++i) {
    Verify_Operand(oinfo, op, i, TRUE);
  }

  INT opnds = OP_opnds(op);
  if (opnds != OP_fixed_opnds(op)) {
    FmtAssert((TOP_is_var_opnds(top) || OP_call(op)) && opnds > OP_fixed_opnds(op),
	      ("wrong number of operands (%d) for %s",
	       opnds,
	       TOP_Name(top)));
    opnds = OP_fixed_opnds(op); // can only verify fixed operands
  }
  for (i = 0; i < opnds; ++i) {
    Verify_Operand(oinfo, op, i, FALSE);
  }

}
#endif

/* ====================================================================
 *   Verify_Instruction
 *
 *   Verify that the specified OP contains valid information for
 *   the instruction it represents.
 * ====================================================================
 */
void 
Verify_Instruction (
  OP *op
)
{
  INT i;
  const ISA_OPERAND_INFO *oinfo;
  TOP top = OP_code(op);

  // ??? check for valid topcode?

  FmtAssert(ISA_SUBSET_LIST_Member(ISA_SUBSET_List, top),
	    ("%s is a member of available ISA subsets", 
	     TOP_Name(top)));

  oinfo = ISA_OPERAND_Info(top);

  INT results = OP_results(op);
  if (results != OP_fixed_results(op)) {
    FmtAssert((TOP_is_var_opnds(top) || OP_call(op)) && results > OP_fixed_results(op),
	      ("wrong number of results (%d) for %s",
	       results,
	       TOP_Name(top)));
    results = OP_fixed_results(op); // can only verify fixed results
  }
  for (i = 0; i < results; ++i) {
    Verify_Operand(oinfo, op, i, TRUE);
  }

  INT opnds = OP_opnds(op);
  if (opnds != OP_fixed_opnds(op)) {
    FmtAssert((TOP_is_var_opnds(top) || OP_call(op)) && opnds > OP_fixed_opnds(op),
	      ("wrong number of operands (%d) for %s",
	       opnds,
	       TOP_Name(top)));
    opnds = OP_fixed_opnds(op); // can only verify fixed operands
  }
  for (i = 0; i < opnds; ++i) {
    Verify_Operand(oinfo, op, i, FALSE);
  }

#if Is_True_On
  // [CG]: Test cond/jump to next bb
  //if (CG_opt_level > 0 && CFLOW_Enable && !OP_likely(op)) {
    if (OP_jump(op) || OP_cond(op)) {
      int idx = TOP_Find_Operand_Use(OP_code(op), OU_target);
      TN *dest = OP_opnd(op, idx);
      BB *bb = OP_bb(op);
      BB *bb_next = BB_next(bb);
      //FdF: TN_is_label(dest) may not be true after tail_call optimization
      // DevAssert(TN_is_label(dest), ("expected label on branch at BB:%d", BB_id(bb)));
      if (TN_is_label(dest) &&
	  bb_next && BB_cold(bb) == BB_cold(bb_next) &&
	  Is_Label_For_BB(TN_label(dest), bb_next) &&
	  TN_offset(dest) == 0) {
	DevWarn("branch to next BB at BB:%d", BB_id(bb));
      }
    }
    //}
#endif

#ifdef TARG_ST
    // [SC] Verify that TOP_asm is in a BB that is marked BB_asm.
    if (top == TOP_asm) {
      BB *bb = OP_bb(op);
      FmtAssert(BB_asm(bb),
	       ("TOP_asm in BB %ld, which is not BB_asm", (long)BB_id(bb)));
    }
#endif

}

/* ====================================================================
 *   Assemble_OP
 *
 *   Write out the 'op' into the object file and/or into the assembly 
 *   file. see r_assemble_op() in cg/cgemit.cxx
 * ====================================================================
 */
static INT
Assemble_OP (
  OP *op, 
  BB *bb, 
  ISA_BUNDLE *bundle,
  INT slot
)
{
  INT words;
  // INT i;

  if (Trace_Inst) {
    fprintf(TFile, "<cgemit> ");
    Print_OP(op);
  }

#ifdef TARG_STxP70
  CGEMIT_Special_Register_Function_Fixup( op );
#endif

  Verify_Instruction(op);

  if (OP_prefetch(op)) Use_Prefetch = TRUE;


#ifdef TARG_ST
  if (ISA_MAX_SLOTS <= 1) {
    // CL: with variable-length bundles
    //     we call Cg_Dwarf_Add_Line_Entry
    //     only at bundle starts in
    //     Assemble_Bundles
    Cg_Dwarf_Add_Line_Entry (PC, OP_srcpos(op));
  }
#else
  Cg_Dwarf_Add_Line_Entry (PC, OP_srcpos(op));
#endif

  
  if (Assembly || Lai_Code) {
    r_assemble_list ( op, bb );
    if (!Object_Code) words = ISA_EXEC_Unit_Slots(OP_code(op));

  }

  if (OP_end_group(op) && Assembly) {
#ifdef TARG_ST
    if (Trace_Sched && List_Notes) {
      fprintf(Asm_File, " %s (bundle %d)\n", ASM_CMNT, Bundle_Count);
      Bundle_Count ++;
    }
#else
    fprintf(Asm_File, "\t %s", ISA_PRINT_END_GROUP);
    fputc('\n', Asm_File);
#endif
#ifdef TARG_ST
    // [SC]: Output symbol after the group.
    if (Symbol_Def_After_Group) {
      EMT_Write_Qualified_Name (Asm_File, Symbol_Def_After_Group);
      fprintf (Asm_File, ":\n");
      Symbol_Def_After_Group = NULL;
    }
#endif
  }

  
#if 0
  if (Object_Code) {
    ISA_PACK_INST inst[ISA_PACK_MAX_INST_WORDS];
    words = r_assemble_binary ( op, bb, inst );
    for (i = 0; i < words; ++i) {
      ISA_BUNDLE_PACK_COMP slot_comp = (ISA_BUNDLE_PACK_COMP)
	  (ISA_BUNDLE_PACK_COMP_slot + slot++);
      TI_ASM_Set_Bundle_Comp(bundle, slot_comp, inst[i]);
      if (slot == ISA_MAX_SLOTS) {
	slot = 0;
	++bundle;
      }
    }

    /* May need to add information about this call (or tail call) site */
    if (OP_call(op) || OP_tail_call(op)) {
      CGEMIT_Add_Call_Information (op, bb, PC, PU_section);
    }
if (Get_Trace ( TP_EMIT,0x100 )) {
/* don't do this till decide on format of EK_SWITCH */
    if (OP_ijump(op) && !OP_call(op) && !BB_exit(bb)) {
      ANNOTATION *ant = ANNOT_Get(BB_annotations(bb), ANNOT_SWITCH);
      if (ant != NULL) {
	ST *jumptable = ANNOT_switch(ant);
	BOOL gprel = ST_is_gp_relative(jumptable);
	INT num_entries = TY_AR_ubnd_val(ST_type(jumptable),0)+1;
	Elf_Event_Kind event_type;
	event_type = (Use_32_Bit_Pointers ? EK_SWITCH_32 : EK_SWITCH_64);

	Em_Add_New_Event (event_type, PC, 
		gprel, EMT_Put_Elf_Symbol(jumptable), num_entries, PU_section);
      }
    }
}
  }
#endif

  PC = PC_Incr_N(PC, words);

  // hack to keep track of last label and offset for assembly dwarf (suneel)
  // If the current op is a TOP_asm, mark the Last_Label as invalid.
  if (OP_code(op) == TOP_asm) {
    Last_Label = LABEL_IDX_ZERO;
  }
  else {
    Offset_From_Last_Label = PC_Incr_N(Offset_From_Last_Label, words);
  }

  return words;
}

/* ====================================================================
 *   Generate_Asm_String
 * ====================================================================
 */
static char*
Generate_Asm_String (
  OP* asm_op,
  BB *bb
)
{
  UINT i;
  ASM_OP_ANNOT* asm_info = (ASM_OP_ANNOT*) OP_MAP_Get(OP_Asm_Map, asm_op);
  char* asm_string = strdup(WN_asm_string(ASM_OP_wn(asm_info)));

  for (i = 0; i < OP_results(asm_op); i++) {
#ifdef TARG_ST
    // [TTh] Results corresponding to subparts of Multi-registers TNs
    //       are not referenced in asm string
    if (ASM_OP_result_position(asm_info)[i] != ASM_OP_position_UNDEF) {
      //TB add subclass parameter to CGTARG_Modify_Asm_String to handle
      //registers that have different name depending on their subclass
      ISA_REGISTER_SUBCLASS sc = ASM_OP_result_subclass(asm_info)[i];
      asm_string = CGTARG_Modify_Asm_String(asm_string, 
					    ASM_OP_result_position(asm_info)[i], 
					    ASM_OP_result_memory(asm_info)[i], 
					    OP_result(asm_op, i),
					    sc);
    }
#else
    asm_string = CGTARG_Modify_Asm_String(asm_string, 
                                   ASM_OP_result_position(asm_info)[i], 
                                   ASM_OP_result_memory(asm_info)[i], 
				   OP_result(asm_op, i))
#endif
  }

  for (i = 0; i < OP_opnds(asm_op); i++) {
#ifdef TARG_ST
    // [TTh] Operands corresponding to subparts of Multi-registers TNs
    //       are not referenced in asm string
    if (ASM_OP_opnd_position(asm_info)[i] != ASM_OP_position_UNDEF) {
      //TB add subclass parameter to CGTARG_Modify_Asm_String to handle
      //registers that have different name depending on thier subclass
      ISA_REGISTER_SUBCLASS sc = ASM_OP_opnd_subclass (asm_info)[i];
      asm_string = CGTARG_Modify_Asm_String(asm_string, 
					    ASM_OP_opnd_position(asm_info)[i], 
					    ASM_OP_opnd_memory(asm_info)[i], 
					    OP_opnd(asm_op, i),
					    sc);
    }
#else
    asm_string = CGTARG_Modify_Asm_String(asm_string, 
                                   ASM_OP_opnd_position(asm_info)[i], 
                                   ASM_OP_opnd_memory(asm_info)[i], 
				   OP_opnd(asm_op, i));
#endif
  }
#if 0
  asm_string = CGTARG_Postprocess_Asm_String(asm_string);
#endif
  return asm_string;
}

/* ====================================================================
 *   Assemble_Simulated_OP
 *
 *   Assemble a simulated OP.
 * ====================================================================
 */
#ifdef TARG_ST
/* [SC] if TARG_ST, Assemble_Simulated_OP has two choices:
 *      1. it may output ops directly (as in the original open64 code), or
 *      2. it may return ops to be inserted in the current bundle.
 *         This allows the target to fully bundle simulated ops if
 *      desired.
 */
#endif
static void
Assemble_Simulated_OP (
  OP *op, 
  BB *bb
#ifdef TARG_ST
  , OPS *ops
#endif
)
{
  /* ASM is an odd case so we handle it specially. It doesn't expand
   * into a series of OPs, but rather we just emit the asm string into
   * the assembly output.
   */
  if (OP_code(op) == TOP_asm) {
    FmtAssert((Assembly || Lai_Code) && !Object_Code,
	      ("can't emit object code when ASM"));
#ifdef TARG_ST
    Verify_Instruction (op);
#endif
    if (Assembly) {
#ifndef TARG_ST
      // [CG] We don't emit stop bit before asm
      if (AS_STOP_BIT && 
	  (EMIT_stop_bits_for_asm ||
	   (EMIT_stop_bits_for_volatile_asm && OP_volatile(op)) ) ) {
	fprintf(Asm_File, "\t%s\n", AS_STOP_BIT);
      }
#endif
#ifdef TARG_ST
      // [SC] We need to emit directives just before the asm string
      CGEMIT_Asm_String_Prefix(op, &PC);
      // [CL] debug_frame support
      Emit_Unwind_Directives_For_OP(op, Asm_File, FALSE, FALSE);
#endif
      fprintf(Asm_File, "\t%s\n", Generate_Asm_String(op, bb));
      if (AS_STOP_BIT && 
	  (EMIT_stop_bits_for_asm ||
	   (EMIT_stop_bits_for_volatile_asm && OP_volatile(op)) ) ) {
	fprintf(Asm_File, "\t%s\n", AS_STOP_BIT);
      }
#ifdef TARG_ST
      // [SC] We need to emit directives just after the asm string
      CGEMIT_Asm_String_Suffix(op, &PC);
      // [CL] debug_frame support
      Emit_Unwind_Directives_For_OP(op, Asm_File, TRUE, FALSE);
#endif
#ifdef TARG_ST200
      // [CG] Align end of asm to 8 bytes boundary such that 
      // for ST200 the alignment is correctly tracked
      // Currently the criterion is DEFAULT_FUNCTION_ALIGNMENT
      if (DEFAULT_FUNCTION_ALIGNMENT) {
	// [CG] Work around the align restriction in ST200 gas
	// .align does not work use balignl instead
	//fprintf(Asm_File, "\t%s %d\n", AS_ALIGN, DEFAULT_FUNCTION_ALIGNMENT);
	fprintf(Asm_File, "\t.balignl %d, 0x80000000\n", DEFAULT_FUNCTION_ALIGNMENT);
	// Align current PC
	PC = PC_Align_N(PC, DEFAULT_FUNCTION_ALIGNMENT);
      }
      // [SC] We do not know the size of the asm, so we no longer know
      // our offset from the Last_Label, so set it to unknown.
      Last_Label = LABEL_IDX_ZERO;
#endif
    }
    if (Lai_Code) {
      fprintf(Lai_File, "\t%s\n", Generate_Asm_String(op, bb));
    }
    return;
  }

#ifdef TARG_ST
  // Arthur: IA64 does not generate any such OPs so I don't really
  //         know what these are ??
  //         ST100 should only generate TOP_intrncall when generating
  //         the Lai_Code, and in this case we just emit a string
  //         for this intrinsic function much like we do for the
  //         TOP_asm.
  if (OP_code(op) == TOP_intrncall) {
    ISA_BUNDLE bundle[ISA_PACK_MAX_INST_WORDS];
    INT words;

    FmtAssert(Lai_Code,("Assemble_Simulated_OP: shouldn't have generated an TOP_intrncall"));

    ASM_DIR_NOTRANSFORM();
    words = Assemble_OP (op, bb, bundle, 0);
    //    r_assemble_list (op, bb);
    ASM_DIR_TRANSFORM();
    return;
  }

  if (Trace_Inst) {
    fprintf (TFile, "<cgemit> transform simulated OP: ");
    Print_OP (op);
  }

  Exp_Simulated_Op (op, ops, PC, &Symbol_Def_After_Group);

  if (Trace_Inst) {
    fprintf (TFile, "... to: ");
    Print_OPS (ops);
  }

#else
  bool is_intrncall = OP_code(op) == TOP_intrncall;
  OPS ops = OPS_EMPTY;

  if (Trace_Inst) {
    fprintf (TFile, "<cgemit> transform simulated OP: ");
    Print_OP (op);
  }

  Exp_Simulated_Op (op, &ops, PC);

  if (is_intrncall && Object_Code) {
    Em_Add_New_Content (CK_NO_XFORM, PC, OPS_length(&ops)*4, 0, PU_section);
  }

  if (Trace_Inst) {
    fprintf (TFile, "... to: ");
    Print_OPS (&ops);
  }

  if (is_intrncall && Assembly) {
    ASM_DIR_NOTRANSFORM();
  }

  FOR_ALL_OPS_OPs_FWD (&ops, op) {
    ISA_BUNDLE bundle[ISA_PACK_MAX_INST_WORDS];
    INT words;
    Perform_Sanity_Checks_For_OP(op, FALSE);
    words = Assemble_OP (op, bb, bundle, 0);
    if (Object_Code) {
      /* write out the instruction. */
      Em_Add_Bytes_To_Scn (PU_section, (char *)&bundle,
			   ISA_MAX_INST_BYTES * words, ISA_MAX_INST_BYTES);
    }
  }

  if (is_intrncall && (Assembly || Lai_Code)) {
    ASM_DIR_TRANSFORM();
  }
#endif /* TARG_ST100 */

  return;
}

/* ====================================================================
 *   Assemble_Ops
 *
 *   Assemble the OPs in a BB an OP at a time.
 * ====================================================================
 */
static void
Assemble_Ops (
  BB *bb
)
{
  OP *op;

#ifdef TARG_ST
  BOOL first_op = TRUE;
  BOOL previous_op_is_prologue = BB_entry(bb);

  Bundle_Count = 0;
  if ((BB_first_op(bb) != NULL) && (OP_scycle(BB_first_op(bb)) != -1))
    Bundle_Count = OP_scycle(BB_first_op(bb));
  Symbol_Def_After_Group = NULL;
#endif

  FmtAssert(ISA_MAX_SLOTS == 1 || Lai_Code || !LOCS_Enable_Bundle_Formation,
	    ("Assemble_Ops shouldn't have been called"));

  FOR_ALL_BB_OPs_FWD(bb, op) {
    ISA_BUNDLE bundle[ISA_PACK_MAX_INST_WORDS];
    INT words;

    if (OP_dummy(op)) continue;		// these don't get emitted

#ifdef TARG_ST
    // Fix for bug #49902: Force the generation of a new line entry
    // at the end of the prologue.
    // Equivalent fix is present in Assemble_Bundles()
    if (first_op) {
      previous_op_is_prologue = OP_prologue(op);
      first_op = FALSE;
    }
    else if (previous_op_is_prologue && !OP_prologue(op)) {
      Cg_Dwarf_Add_Line_Entry (PC, OP_srcpos(op), TRUE);
      previous_op_is_prologue = FALSE;
    }
#endif

    if (OP_simulated(op)) {
#ifdef TARG_ST
      OPS new_ops = OPS_EMPTY;
      Assemble_Simulated_OP(op, bb, &new_ops);
      BB_Insert_Ops_After (bb, op, &new_ops);
#else
      Assemble_Simulated_OP(op, bb);
#endif
      continue;
    }
// TODO POST MERGE: sync with st200. Why is it STxP70 specific?
// CQ1: I do not know why this is not done for all kind of operation at the
// begining and the end of the loop. Currently each assemble function does this
// this work!
#ifdef TARG_STxP70
    Emit_Unwind_Directives_For_OP(op, Asm_File, FALSE, FALSE);
#endif
    // Perform_Sanity_Checks_For_OP(op, TRUE);
    words = Assemble_OP (op, bb, bundle, 0);
#ifdef TARG_STxP70
    Emit_Unwind_Directives_For_OP(op, Asm_File, TRUE, FALSE);
#endif
#if 0
    if (Object_Code) {
      Em_Add_Bytes_To_Scn(PU_section, (char *)bundle,
			  ISA_MAX_INST_BYTES * words, ISA_MAX_INST_BYTES);
    }
#endif
  }

  return;
}

/* ====================================================================
 *   Assemble_Bundles
 *
 *   Assemble the OPs in a BB a bundle at a time.
 * ====================================================================
 */
static void
Assemble_Bundles(BB *bb)
{
  OP *op;

#ifdef TARG_ST
  Bundle_Count = 0;
  if ((BB_first_op(bb) != NULL) && (OP_scycle(BB_first_op(bb)) != -1))
    Bundle_Count = OP_scycle(BB_first_op(bb));
  Symbol_Def_After_Group = NULL;
#endif

  FmtAssert(Assembly,
	    ("Assemble_Bundles shouldn't have been called"));

  FmtAssert(ISA_MAX_SLOTS > 1,
	    ("Assemble_Bundles shouldn't have been called (%d)",
	     ISA_MAX_SLOTS));

  if (Trace_Inst) {
    fprintf(TFile, "<cgemit> Assemble_Bundles BB%d:\n", BB_id(bb));
    //Print_BB(bb);
  }

#ifdef TARG_ST
  // [CL] track prologue and epilogue
  BOOL first_bundle = TRUE;
  BOOL bundle_has_prologue;
  BOOL bundle_has_epilogue;
  BOOL previous_bundle_has_prologue=FALSE;
  BOOL epilogue_has_started=FALSE;
#endif

#ifdef TARG_ST
    // [CL] handle simulated OPs and unwind information
    BOOL is_inserted_op[ISA_MAX_SLOTS];
    UINT n_inserted_ops = 0;
#endif

#ifdef TARG_ST
  for (op = BB_first_op(bb);op != NULL;) {
#else
  for (op = BB_first_op(bb);;) {
#endif
    ISA_BUNDLE bundle;
#ifdef TARG_ST
    INT n_slot_ops;
    ISA_EXEC_MASK slot_mask;
#else
    UINT64 slot_mask;
#endif
    UINT stop_mask;
    INT slot;
    OP *slot_op[ISA_MAX_SLOTS];
    INT ibundle;
#ifdef TARG_ST
    // CL: track bundle stop because we have variable length bundles
    int seen_end_group=0;
    if (OP_code(op) == TOP_asm) {
      FmtAssert(!Object_Code, ("can't emit asm in object code")); 
      Cg_Dwarf_Add_Line_Entry (PC2Addr(PC), OP_srcpos(op));
      Assemble_Simulated_OP(op, bb, NULL);
      op = OP_next(op);
      continue;
    }
#endif

    /* Gather up the OPs for the bundle.
     */
    stop_mask = 0;
#ifdef TARG_ST
    memset(&slot_mask, 0, sizeof(slot_mask));
#else
    slot_mask = 0;
#endif
#ifdef TARG_ST
    for (slot = 0; op && !seen_end_group; op = OP_next(op)) {
#else
    for (slot = 0; op && slot < ISA_MAX_SLOTS; op = OP_next(op)) {
#endif

#ifdef TARG_ST
      INT instr_slots;
      INT instr_slot;
#else
      INT words;
      INT w;
#endif

#ifdef TARG_ST
      seen_end_group = OP_end_group(op);
      FmtAssert(!(OP_dummy(op) && OP_end_group(op)),
                ("Dummy op should not have end group marker"));
#endif

      if (OP_dummy(op)) continue;		// these don't get emitted

      if (OP_simulated(op)) {
#ifdef TARG_ST
	OPS new_ops = OPS_EMPTY;
	Assemble_Simulated_OP(op, bb, &new_ops);
	BB_Insert_Ops_After (bb, op, &new_ops);

	// [TTh] Fix for bug #44381
	// For TOP_asm, OP_Real_Ops(op) returns 0.
	UINT real_ops = OP_Real_Ops(op);
	if (real_ops > 0) {
	  n_inserted_ops += real_ops - 1;
	}

	if (OPS_first(&new_ops)) {
	  seen_end_group = FALSE;
	}
#else
	FmtAssert(slot == 0, ("can't bundle a simulated OP in BB:%d (op %s).",
			      BB_id(bb), TOP_Name(OP_code(op))));
	Assemble_Simulated_OP(op, bb);
#endif
	continue;
      }

      if (OP_prologue(op)) {
	bundle_has_prologue = TRUE;
      } else {
	bundle_has_prologue = FALSE;
      }
      if (OP_epilogue(op)) {
	bundle_has_epilogue = TRUE;
      } else {
	bundle_has_epilogue = FALSE;
      }

#ifdef TARG_ST
      instr_slots = ISA_EXEC_Unit_Slots(OP_code(op));
      for (instr_slot = 0; instr_slot < instr_slots; ++instr_slot) {
#else
      words = ISA_PACK_Inst_Words(OP_code(op));
      for (w = 0; w < words; ++w) {
#endif
	FmtAssert(slot < ISA_MAX_SLOTS, 
		  ("multi-word inst extends past end of bundle in BB:%d.",
		   BB_id(bb)));
#ifdef TARG_ST
	// [CL] handle simulated OPs and unwind information
	if (n_inserted_ops) {
	  is_inserted_op[slot] = TRUE;
	  n_inserted_ops--;
	} else {
	  is_inserted_op[slot] = FALSE;
	}
#endif

        slot_op[slot++] = op;
#ifdef TARG_ST
	slot_mask = TI_BUNDLE_Set_Slot_Mask_Property(slot_mask, slot - 1,
						     ISA_EXEC_Unit_Prop(OP_code(op)));
#else
        slot_mask = (slot_mask << ISA_TAG_SHIFT) | ISA_EXEC_Unit_Prop(OP_code(op));
#endif
        stop_mask = stop_mask << 1;
      }
#ifdef TARG_ST
      stop_mask |= (seen_end_group != 0);
#endif

#ifndef GAS_TAGS_WORKED
// remove this when gas can handle tags inside explicit bundle
#ifdef TARG_ST
      if (Get_OP_Tag(op)) {
#else
      if (OP_has_tag(op)) {
#endif
	fprintf(Asm_File, "\n%s:\n", LABEL_name(Get_OP_Tag(op)));
#ifdef TARG_ST
	// [CL]
	Set_LABEL_emitted(Get_OP_Tag(op));
#endif
      }
#endif

    }
#ifdef TARG_ST
      // clarkes 030910: to allow other ops in a bb after
      // a simulated op
      if (slot == 0) {
	  continue;
      }
      // [SC] Care: Previous code used the OP_end_group(op) marker
      // as a sentinel to mark the last entry of slot_op[].  This is okay
      // for some targets, but not for ia64, where the last operation
      // of a bundle is not necessarily OP_end_group.  Instead, use
      // n_slot_ops as a count of the number of entries in slot_op[].
      n_slot_ops = slot;
#else
    if (slot == 0) break;
#endif

#ifdef TARG_ST
    // CL: recalibrate to ISA_MAX_SLOTS bundle length
    for(int w=slot; w<ISA_MAX_SLOTS; w++) {
      // [SC] removed. slot_mask <<= ISA_TAG_SHIFT; 
      stop_mask <<= 1;
    }
#endif

#ifndef TARG_ST
    // CL: now, bundles have variable length

    // Emit the warning only when bundle formation phase is enabled (ON by
    // default).
    if (LOCS_Enable_Bundle_Formation) {
      FmtAssert(slot == ISA_MAX_SLOTS, ("not enough OPs for bundle in BB:%d\n",BB_id(bb)));
    }
#endif

    /* Determine template.
     */
#ifdef TARG_ST
    // [SC] The ops are not guaranteed to be in the correct order
    // in slot[] and slot_mask.  To find the correct bundle
    // we need to try each permutation of the ops.
    // The ops can be permuted between stop bits only.
    // Multi-slot ops must be handled carefully to ensure
    // that they are not split into non-adjacent slots.
    // We also need to take alignment into account to find the
    // correct bundle.
    // That is all very hard to do, and all we achieve is an internal
    // consistency check that the previous bundling was correct.
    // So I give up and fix ibundle here.
    BOOL bundle_order_changed = FALSE;
    ibundle = CGTARG_Detect_Bundle_Id(slot_mask, &bundle_order_changed);

#else
    for (ibundle = 0; ibundle < ISA_MAX_BUNDLES; ++ibundle) {
      UINT64 this_slot_mask = ISA_EXEC_Slot_Mask(ibundle);
      UINT32 this_stop_mask = ISA_EXEC_Stop_Mask(ibundle);
      if (   (slot_mask & this_slot_mask) == this_slot_mask 
             && stop_mask == this_stop_mask) break;
    }
#endif

    if (Trace_Inst) {
      fprintf(TFile, "<cgemit> Bundle:\n");
      for (int i = 0; i < slot; i++) {
	Print_OP_No_SrcLine (slot_op[i]);
      }
    }

    // Emit the warning only when bundle formation phase is enabled (ON by
    // default).
    if (LOCS_Enable_Bundle_Formation) {
      //      if (Trace_Inst && ibundle == ISA_MAX_BUNDLES) {
      //      if (Trace_Inst) {
      //        fprintf(TFile, "<cgemit> Bundle:\n");
      //        for (slot = 0; slot < ISA_MAX_SLOTS; slot++) {
      //          Print_OP_No_SrcLine (slot_op[slot]);
      //        }
      //      }
#ifdef TARG_ST
      FmtAssert(ibundle != ISA_MAX_BUNDLES,
	       ("couldn't find bundle for slot mask=0x%0llx, stop mask=0x%x in BB:%d\n",
	        slot_mask.v[0], stop_mask, BB_id(bb)));
#else
      FmtAssert(ibundle != ISA_MAX_BUNDLES,
	       ("couldn't find bundle for slot mask=0x%0llx, stop mask=0x%x in BB:%d\n",
	        slot_mask, stop_mask, BB_id(bb)));
#endif
    }

    /* Bundle prefix
     */
    if (Assembly && EMIT_explicit_bundles) {
      fprintf(Asm_File, ISA_PRINT_BEGIN_BUNDLE, ISA_EXEC_AsmName(ibundle));
      fprintf(Asm_File, "\n");
    }


    /* Assemble the bundle.
     */
    OP *sl_op;
#ifdef  TARG_ST
    BOOL force_dwarf=FALSE;
    SRCPOS line;

    // [CL] The bundler has already taken care of prologue/epilogue
    // concerns when forming bundles:
    // - we force a new bundle after prologue
    // - we force a new bundle where epilogue begins

    // [CL] If this bundle has epilogue code, and the previous one
    // hadn't, then we consider that epilogue starts here,
    // and thus we force output of debug info for this bundle
    if (bundle_has_epilogue && !epilogue_has_started) {
      force_dwarf = TRUE;
    }

    // [CL] If the prologue ended in the previous bundle,
    // force output of debug info for this bundle.
    if (!bundle_has_prologue && previous_bundle_has_prologue) {
      force_dwarf = TRUE;
    }

    // [CL] Don't output debug info within prologue
    if (bundle_has_prologue) {
      force_dwarf = FALSE;
    }

    line = OP_srcpos(slot_op[0]);

    if ( (!force_dwarf || (line == 0)) && (Opt_Level > 0) ) {
      slot = 0;
      do {
	sl_op = slot_op[slot];
	if ((OP_srcpos(sl_op) != 0) && (!OP_prologue(sl_op)) && (!OP_epilogue(sl_op))){
	  force_dwarf = TRUE;
	  line = OP_srcpos(sl_op);
	  break;
	}
	slot += ISA_EXEC_Unit_Slots(OP_code(sl_op));
      } while (slot < n_slot_ops);
    }

    /* Generate debug info for the 1st op of the bundle */
    Cg_Dwarf_Add_Line_Entry (PC2Addr(PC), line, force_dwarf);
#endif

#ifdef TARG_ST
    // [CL] support for debug_frame information
    // loop over the bundle to check if we need to emit special labels
  slot = 0;
  do {
      sl_op = slot_op[slot];

      Emit_Unwind_Directives_For_OP(sl_op, Asm_File, FALSE,
				    is_inserted_op[slot]);

      slot += ISA_EXEC_Unit_Slots(OP_code(sl_op));
  } while (slot < n_slot_ops);
#endif

  if (bundle_order_changed) {
    CGTARG_FixBundle(slot_mask, slot_op, ibundle);
  }

    slot = 0;
    do {
      sl_op = slot_op[slot];
      //      Perform_Sanity_Checks_For_OP(sl_op, TRUE);
      slot += Assemble_OP(sl_op, bb, &bundle, slot);
#ifdef TARG_ST
    } while (slot < n_slot_ops);
#else
    } while (slot < ISA_MAX_SLOTS);
#endif

    /* Bundle suffix
     */
#if 0
    if (Object_Code) {
      TI_ASM_Set_Bundle_Comp(&bundle,
			     ISA_BUNDLE_PACK_COMP_template, 
			     ibundle);

      Em_Add_Bytes_To_Scn (PU_section, (char *)&bundle, 
                                        ISA_MAX_INST_BYTES, ISA_MAX_INST_BYTES);
    }
#endif
    if (Assembly) {
      if (EMIT_explicit_bundles) {
	fprintf(Asm_File, "%s", ISA_PRINT_END_BUNDLE);
#ifdef TARG_ST
	fprintf(Asm_File, "\n");
      }
#else
      }
      fprintf(Asm_File, "\n");
#endif
    }

#ifdef TARG_ST
    // [CL] loop over the bundle to check if we need
    // to emit special labels after the bundle
    slot = 0;
    do {
      sl_op = slot_op[slot];
      
      Emit_Unwind_Directives_For_OP(sl_op, Asm_File, TRUE,
				    is_inserted_op[slot]);

      slot += ISA_EXEC_Unit_Slots(OP_code(sl_op));
    } while (slot < n_slot_ops);
#endif

#ifdef TARG_ST
    // [CL]
    if (first_bundle) {
      previous_bundle_has_prologue = bundle_has_prologue;
      first_bundle = FALSE;
    }
    else if (!bundle_has_prologue) {
      previous_bundle_has_prologue = bundle_has_prologue;
    }
    if (bundle_has_epilogue) {
      epilogue_has_started = TRUE;
    }
#endif
  }
#ifndef TARG_ST
  if (Assembly) {
    fprintf(Asm_File, "\n");
  }
#endif
}

/* ====================================================================
 *   Emit_Loop_Note
 *
 *   Emit a loop note to the .s file, anl file, etc.
 * ====================================================================
 */
static void
Emit_Loop_Note (
  BB *bb, 
  FILE *file
)
{
  //  BOOL anl_note = file == anl_file;
  BB *head = BB_loop_head_bb(bb);
  UINT16 unrollings = BB_unrollings(bb);
  ANNOTATION *info_ant = ANNOT_Get(BB_annotations(head), ANNOT_LOOPINFO);
  LOOPINFO *info = info_ant ? ANNOT_loopinfo(info_ant) : NULL;
  BOOL unroll_pragma = FALSE;
  ANNOTATION *unroll_ant = ANNOT_Get(BB_annotations(head), ANNOT_PRAGMA);

#ifdef TARG_ST
  UINT16 kunroll = info ? LOOPINFO_kunroll(info) : 0;
  if (kunroll > 1)
    unrollings /= kunroll;
#endif

  while (unroll_ant && WN_pragma(ANNOT_pragma(unroll_ant)) != WN_PRAGMA_UNROLL)
    unroll_ant = ANNOT_Get(ANNOT_next(unroll_ant), ANNOT_PRAGMA);
  if (unroll_ant) {
    WN *wn = ANNOT_pragma(unroll_ant);
    if (WN_pragma_arg1(wn) > 1) {
      if (WN_pragma_arg1(wn) == unrollings)
	unroll_pragma = TRUE;
#ifdef TARG_ST
      else if (BB_innermost(bb) && (bb == head))
#else
      else if (BB_innermost(bb))
#endif
	{
	  char buffer[128];
	  sprintf(buffer,
		  "loop unrolled %d times but pragma says to unroll %d times",
		  unrollings, WN_pragma_arg1(wn));
	  ErrMsg(EC_CG_Generic_Warning, buffer);
	}
    }
  }

  if (bb == head) {
    SRCPOS srcpos = BB_Loop_Srcpos(bb);
    INT32 lineno = SRCPOS_linenum(srcpos);

#if 0
    if (anl_note) {
      INT32 fileno = SRCPOS_filenum(srcpos);
      INT32 colno = SRCPOS_column(srcpos);
      fprintf (anl_file,
	       "\nmsg loop lines [%d %d %d]",
	       fileno,
	       lineno,
	       colno);
    } else {
      fprintf (file, "%s<loop> Loop body line %d", ASM_CMNT_LINE, lineno);
    }
#else
    fprintf (file, "%s<loop> Loop body line %d", ASM_CMNT_LINE, lineno);
#endif

    // [CR] Add minitercount and maxitercount information
    ANNOTATION *min_annot;
    INT32 minitercount = -1;
    /* FdF 20060913: The information is now set in LOOPINFO. */
    if (info)
      minitercount = LOOPINFO_trip_min(info);
#if 0
    for (min_annot = ANNOT_Get(BB_annotations(head), ANNOT_PRAGMA);
	 min_annot != NULL;
	 min_annot = ANNOT_Get(ANNOT_next(min_annot), ANNOT_PRAGMA)) {
      if (WN_pragma(ANNOT_pragma(min_annot)) == WN_PRAGMA_LOOPMINITERCOUNT) {
	WN *pragma = ANNOT_pragma(min_annot);
	minitercount = (INT32)WN_pragma_arg1(pragma);
	break;
      } 
    }
#endif
    ANNOTATION *max_annot;
    INT32 maxitercount = -1;
    for (max_annot = ANNOT_Get(BB_annotations(head), ANNOT_PRAGMA);
	 max_annot != NULL;
	 max_annot = ANNOT_Get(ANNOT_next(max_annot), ANNOT_PRAGMA)) {
      if (WN_pragma(ANNOT_pragma(max_annot)) == WN_PRAGMA_LOOPMAXITERCOUNT) {
	WN *pragma = ANNOT_pragma(max_annot);
	maxitercount = (INT32)WN_pragma_arg1(pragma);
	break;
      } 
    }

    if (info) {
      WN *wn = LOOPINFO_wn(info);
#ifdef TARG_ST
      TN *trip_tn = LOOPINFO_exact_trip_count_tn(info);
#else
      TN *trip_tn = LOOPINFO_trip_count_tn(info);
#endif
      BOOL constant_trip = trip_tn && TN_is_constant(trip_tn);
      INT depth = WN_loop_depth(wn);
      const char *estimated = constant_trip ? "" : "estimated ";
      INT64 trip_count = constant_trip ? TN_value(trip_tn) :
					 (INT64)WN_loop_trip_est(wn);
#if 0
      const char *fmt =   anl_note
			? " \"nesting depth: %d, %siterations: %lld\""
			: ", nesting depth: %d, %siterations: %lld";
#else
      const char *fmt = ", nesting depth: %d, %siterations: %lld";
#endif
      fprintf (file, fmt, depth, estimated, trip_count);
    }

    if (minitercount != -1) {
      const char *fmt = "\n%s<loop>  pragma minitercount :%ld";
      fprintf (file, fmt, ASM_CMNT_LINE, minitercount);
    }
    if (maxitercount != -1) {
      const char *fmt = "\n%s<loop>  pragma minitercount :%ld";
      fprintf (file, fmt, ASM_CMNT_LINE, minitercount);
    }
    fprintf (file, "\n");
  } 
#if 0
  else if (anl_note) {

    /* Only interested in loop head messages for anl file
     */
    return;
  } 
#endif
  else {
    ANNOTATION *lbl_ant = ANNOT_Get(BB_annotations(head), ANNOT_LABEL);
    DevAssert(lbl_ant, ("loop head BB:%d has no label", BB_id(head)));
    fprintf(file,
	    "%s<loop> Part of loop body line %d"
	    ", head labeled %s\n",
	    ASM_CMNT_LINE, BB_Loop_Lineno(head), LABEL_name(ANNOT_label(lbl_ant)));
  }

  if (unrollings > 1) {
#if 0
    if (anl_note) {
      fprintf(anl_file, "\"unrolled %d times%s%s\"\n", unrollings,
	      BB_unrolled_fully(bb) ? " (fully)" : "",
	      unroll_pragma ? " (pragma)" : "");
    } else {
      fprintf(file, "%s<loop> unrolled %d times%s%s\n", 
	      ASM_CMNT_LINE,
	      unrollings,
	      BB_unrolled_fully(bb) ? " (fully)" : "",
	      unroll_pragma ? " (pragma)" : "");
    }
#else
      fprintf(file, "%s<loop> unrolled %d times%s%s\n", 
	      ASM_CMNT_LINE,
	      unrollings,
	      BB_unrolled_fully(bb) ? " (fully)" : "",
	      unroll_pragma ? " (pragma)" : "");
#endif
  }
#ifdef TARG_ST
  if (kunroll > 1) {
    fprintf(file, "%s<loop> kernel unrolling %d times\n",
	    ASM_CMNT_LINE,
	    kunroll);
  }
#endif
}

#ifdef BCO_ENABLED /* Thierry */
static BB_NUM PU_BB_Count_for_profile;	// Number of BB with profile info
static BB_NUM PU_BB_current_idx_for_profile = 0; /* Current index of BB being treated */ 

/* Add profil_info commands */
#define PI_FUNCBEGIN 0x01 
#define PI_FUNCEND 0x02 

#define PIA_FUNCBEGIN 0x01 
#define PIA_FUNCEND 0x02 
#define PIA_BBBEGIN 0x03 
#define PIA_BBEND 0x04

/* ====================================================================
 *   get_char_from_float
 *
 * Convert a float into 4 bytes for a little endian target 
 * Use for .profile_info whose data is little endian
 *
 * ====================================================================
 */
static void 
get_char_from_float(float *f, char *ptr)
{
  union u {
    float f;
    char b[4];
  } p;
  p.f = *f;
#if HOST_IS_LITTLE_ENDIAN
  *((union u *)ptr) = p;
#elif HOST_IS_BIG_ENDIAN
  ptr[0] = p.b[3];
  ptr[1] = p.b[2];
  ptr[2] = p.b[1];
  ptr[3] = p.b[0];
#else
#error "HOST_IS_LITTLE_ENDIAN or HOST_IS_BIG_ENDIAN need to be defined"
#endif  
}

/* ====================================================================
 *   EMT_ProfileInfo_BB
 *
 *   Emit .profile_info for this BB.
 *   Emit frequency info about BB.
 *
 * ====================================================================
 */
static void
EMT_ProfileInfo_BB ( 
  BB *bb, 
  WN *rwn 
)
{
  char * label_name;
  ANNOTATION *ant;
  LABEL_IDX lab;
  FILE *file = Asm_File;
  char buf[4];
  float f;
  const char *prefix = file == Asm_File ? ASM_CMNT : "";

  if (!FREQ_Frequencies_Computed() && !CG_PU_Has_Feedback) return;
  FmtAssert(Assembly && CG_emit_bb_freqs, ("EMT_ProfileInfo_BB calls only with -CG:emit-bb-freqs"));

  /* first get a label attached to this BB */

  ant = ANNOT_First (BB_annotations(bb), ANNOT_LABEL);
  assert(ant != NULL);
  lab = ANNOT_label(ant);
  label_name = LABEL_name(lab);
    
  PU_BB_current_idx_for_profile++;
  FmtAssert( PU_BB_current_idx_for_profile <= PU_BB_Count_for_profile, ("Current BB index is over BB count!"));
    
  /* Convert BB frequency into 4 bytes */
  f = BB_freq(bb);
  get_char_from_float(&f, buf);

  /* Dump profile info */
  fprintf(file, "\t%s\t%s, 0x%x\n", AS_WORD,
	  label_name, PU_BB_current_idx_for_profile);
#ifdef TARG_ST
  // [CL]
  Set_LABEL_emitted(lab);
#endif
  
  fprintf(file, "\t%s\t0x%x, 0x%x, 0x%x, 0x%x\n", AS_BYTE,
	  buf[0], buf[1], buf[2], buf[3]);
}

/* ====================================================================
 *   EMT_ProfileInfoArc_BB
 *
 *   Emit .profile_info_arc for this BB.
 *   Emit frequency info about BB.
 *
 * ====================================================================
 */
static int
EMT_ProfileInfoArc_BB ( 
  BB *bb, 
  WN *rwn 
)
{
  // BB begin, size of the bb section, address, number, nb of succs, flags, freq
  // SUCCINFO:
  // succ1, proba, flag
  // succ2, proba, flag
  // ....
  // END_BB
  char * label_name;
  ANNOTATION *ant;
  LABEL_IDX lab;
  FILE *file = Asm_File;
  char buf[4];
  float f;
  const char *prefix = file == Asm_File ? ASM_CMNT_LINE : "";
  BBLIST *bl;
  int bbflag = BB_flag(bb);
#define BBM_ICALL          0x400000
  //Add icall flag to the bb
  if (BB_call(bb)) {
    ANNOTATION *ant;
    CALLINFO *call_info;
    ST *call_st;
    ant = ANNOT_Get(BB_annotations(bb), ANNOT_CALLINFO);
    call_info = ANNOT_callinfo(ant);
    call_st = CALLINFO_call_st(call_info);
    if (call_st == NULL)
      bbflag |= BBM_ICALL;
  }
#undef BBM_ICALL
  if (!FREQ_Frequencies_Computed() && !CG_PU_Has_Feedback) return 0;
  FmtAssert(Assembly && CG_emit_bb_freqs_arcs, ("EMT_ProfileInfoArc_BB calls only with -CG:emit-bb-freqs"));

  /* first get a label attached to this BB */

  ant = ANNOT_First (BB_annotations(bb), ANNOT_LABEL);
  assert(ant != NULL);
  lab = ANNOT_label(ant);
  label_name = LABEL_name(lab);
    
  /* Convert BB frequency into 4 bytes */
  f = BB_freq(bb);
  get_char_from_float(&f, buf);

  // Get BB number of succs
  int BB_count_succ = 0;
  FOR_ALL_BB_SUCCS (bb, bl) {
    ++BB_count_succ;
  }

  int  bb_section_size = 8 * 4 + BB_count_succ*(3*4);
  /* Dump profile info */
  fprintf(file, "\t%s\t0x%x, 0x%x, %s, 0x%x, 0x%x, 0x%x\n", AS_WORD,
	  PIA_BBBEGIN, bb_section_size, label_name, BB_id(bb), BB_count_succ, bbflag);
#ifdef TARG_ST
  // [CL]
  Set_LABEL_emitted(lab);
#endif
  
  fprintf(file, "\t%s\t0x%x, 0x%x, 0x%x, 0x%x\n", AS_BYTE,
	  buf[0], buf[1], buf[2], buf[3]);
  //Dump SUCC info
  FOR_ALL_BB_SUCCS (bb, bl) {
    /* Convert arc proba into 4 bytes */
    f = BBLIST_prob(bl);
    get_char_from_float(&f, buf);
    fprintf(file, "\t%s\t0x%x\n", AS_WORD, BB_id(BBLIST_item(bl)));
    fprintf(file, "\t%s\t0x%x, 0x%x, 0x%x, 0x%x\n", AS_BYTE,
	  buf[0], buf[1], buf[2], buf[3]);
    int flagsucc=0;
    fprintf(file, "\t%s\t0x%x\n", AS_WORD, flagsucc);
  }
  fprintf(file, "\t%s\t0x%x\n", AS_WORD,
	  PIA_BBEND);
  return bb_section_size;
}


/* ====================================================================
 *   EMT_ProfileInfo_Header_PU
 *
 *   Emit .profile_info header for this PU.
 * ====================================================================
 */
static void
EMT_ProfileInfo_PU ( 
  ST      *pu,
  DST_IDX  pu_dst, 
  WN      *rwn
)
{
  BB *bb; 
  int total_count_succ = 0;
  if (!FREQ_Frequencies_Computed() && !CG_PU_Has_Feedback) return;
  FmtAssert( Assembly && CG_emit_bb_freqs, ("EMT_ProfileInfo_Header_PU calls only with -CG:emit-bb-freqs"));
  /* Emit header of .profile_info for this PU */
  // HEADER
  /* Reset current BB index to 0 */
  PU_BB_current_idx_for_profile = 0;
  
  /* compute PU_BB_Count because PU_BB_Count is wrong here??? */
  PU_BB_Count_for_profile = 0;
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    PU_BB_Count_for_profile++;
    BBLIST *bl;
    FOR_ALL_BB_SUCCS (bb, bl) {
      ++total_count_succ;
    }
  }
  fprintf (Asm_File, "\t%s %s\n", AS_SECTION, ".profile_info");
  fprintf (Asm_File, "\t%s\t0x%x, %s, 0x%x \n",AS_WORD,
	   PI_FUNCBEGIN, ST_name(pu), PU_BB_Count_for_profile);
  // BBS
  /* Emit profile_info for each basic block in the PU */
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    EMT_ProfileInfo_BB (bb, rwn);
  }

  // FOOTER
  fprintf(Asm_File, "\t%s\t0x%x\n", AS_WORD, PI_FUNCEND);
  if (CG_emit_bb_freqs_arcs) {
    //TB: Create a new .profile_info_section with control flow info
    // Format:
    // FUNCTION begin, size of the function section, address, number, nb of BB, flags
    // BBINFO
    //END_FUNCTION
    int size = 0;
    int function_section_size = 6 * 4 + PU_BB_Count_for_profile * (8*4) + total_count_succ*(3*4);
    fprintf (Asm_File, "\t%s %s\n", AS_SECTION, ".profile_info_arc");
    fprintf (Asm_File, "\t%s\t0x%x, 0x%x, %s, 0x%x, 0x%x\n",AS_WORD,
	     PIA_FUNCBEGIN, function_section_size, ST_name(pu), PU_BB_Count_for_profile, 0x0);
    size += 5*4;
    // BBS
    /* Emit profile_info_section for each basic block in the PU */
    for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
      size += EMT_ProfileInfoArc_BB (bb, rwn);
    }
    // FOOTER
    fprintf(Asm_File, "\t%s\t0x%x\n", AS_WORD, PIA_FUNCEND);
    size += 4;
    FmtAssert(size == function_section_size ,
	      ("size of .profile_info_arc section mismatches"));
  }
}
#endif /* BCO_Enabled Thierry */

/* ====================================================================
 *   EMT_Assemble_BB
 *
 *   Assemble the contents of the given BB.
 * ====================================================================
 */
static void
EMT_Assemble_BB ( 
  BB *bb, 
  WN *rwn 
)
{
  ST *st;
  ANNOTATION *ant;
  RID *rid = BB_rid(bb);

  if (Trace_Inst) {
    #pragma mips_frequency_hint NEVER
    fprintf(TFile, "assemble BB %d\n", BB_id(bb));
  }

  if (Assembly || Lai_Code) {
    if (rid != NULL && RID_cginfo(rid) != NULL) {
      if (current_rid == RID_id(rid)) {
	/* same, so do nothing */
      }
      else if (current_rid > 0) {
	/* changing regions */
	fprintf (Output_File, "%s END REGION %d\n",
				    ASM_CMNT_LINE, current_rid);
	if (RID_id(rid) > 0 && !RID_is_glue_code(rid)) {
	  fprintf (Output_File, "%s BEGIN REGION %d\n",
				    ASM_CMNT_LINE, RID_id(rid));
	  current_rid = RID_id(rid);
	}
	else if (RID_is_glue_code(rid)) {
	  current_rid = 0;	/* pretend not a region */
	}
	else {
	  current_rid = RID_id(rid);
	}
      }
      else if (RID_id(rid) > 0 && !RID_is_glue_code(rid)) {
	/* beginning a region */
	fprintf (Output_File, "%s BEGIN REGION %d\n", 
				   ASM_CMNT_LINE, RID_id(rid));
	current_rid = RID_id(rid);
      }
    }
    else if (current_rid > 0) {
      /* ending a region */
      fprintf (Output_File, "%s END REGION %d\n", ASM_CMNT_LINE, current_rid);
      current_rid = 0;	/* no rid */
    }
  }

#ifdef TARG_ST
  INT align_bb = Check_If_Should_Align_BB(bb);
  if (align_bb != 0) {
    fprintf(Asm_File, "\t%s %d\n", AS_ALIGN, align_bb);
    // Align current PC
    PC = PC_Align_N(PC, align_bb);
  }
#endif

  if (BB_entry(bb)) {
    char *entry_name;
    ST *entry_sym; 
    ENTRYINFO *ent;
    SRCPOS entry_srcpos;
    ant = ANNOT_Get (BB_annotations(bb), ANNOT_ENTRYINFO);
    ent = ANNOT_entryinfo(ant);
    entry_srcpos = ENTRYINFO_srcpos(ent);
    entry_sym = ENTRYINFO_name(ent);
    entry_name = ST_name(entry_sym);

    /* Set an initial line number so that if the first inst in the BB
     * has no srcpos, then we'll be ok.
     */
#ifdef TARG_ST
    // [CL] force generation of line information for entry_BB
    // useful for C++ when several functions are generated
    // in sequence for the same line number (generated constructors
    // for instance)
    if (entry_srcpos)
      Cg_Dwarf_Add_Line_Entry (PC2Addr(PC), entry_srcpos, TRUE);
#else
    if (entry_srcpos)
      Cg_Dwarf_Add_Line_Entry (PC, entry_srcpos);
#endif

    if (ST_is_not_used(entry_sym)) {
      // don't emit alt-entry if marked as not-used
      DevWarn("CG reached entry marked as unused; will ignore it (%s)\n", 
		entry_name);
    }
    else {
      Set_ST_ofst(entry_sym, PC);
      if (strcmp(Cur_PU_Name, entry_name) != 0) {
	// alt-entry
	if (Assembly || Lai_Code) {
	  fprintf (Output_File, "\t%s\t%s\n", AS_AENT, entry_name);
	  Print_Label (Output_File, NULL, entry_sym, 0 );
	}
	(void)EMT_Put_Elf_Symbol (entry_sym);
#if 0
	if ( Object_Code ) {
	  Em_Define_Symbol (
	       EMT_Put_Elf_Symbol(entry_sym), PC, 0, PU_section);
	  Em_Add_New_Event (EK_ENTRY, PC, 0, 0, 0, PU_section);
	}
#endif
      }
#if 0
      if (Object_Code) {
	if ( EMIT_interface_section && !BB_handler(bb))
	  Interface_Scn_Add_Def( entry_sym, rwn );
      }
#endif
    }
  }

#ifdef TARG_STxP70
  // Emits a cross-end directives in first block of HWLoops
  CGEMIT_Loop_CrossEnd_Directive(bb);

  // Emit an align directive on call return addresses.
  CGEMIT_CallReturns_Directive(bb);
  
  // [TTh] quick fix to align call returns
#endif

  /* List labels attached to BB: */
  for (ant = ANNOT_First (BB_annotations(bb), ANNOT_LABEL);
       ant != NULL;
       ant = ANNOT_Next (ant, ANNOT_LABEL)) {
    LABEL_IDX lab = ANNOT_label(ant);
    if (Assembly || Lai_Code) {
#ifdef TARG_ST
      if (List_Notes)
	fprintf (Output_File, "%s:\t%s 0x%llx\n", 
		 LABEL_name(lab), ASM_CMNT_LINE, Get_Label_Offset(lab) );
      else
	fprintf (Output_File, "%s:\n", LABEL_name(lab));
#else
      fprintf (Output_File, "%s:\t%s 0x%llx\n", 
		   LABEL_name(lab), ASM_CMNT_LINE, Get_Label_Offset(lab) );
#endif
#ifdef TARG_ST
      // [CL]
      Set_LABEL_emitted(lab);
#endif
    }
#ifndef TARG_IA64

    // Arthur: this is probably just the MIPS thing ??
#ifdef TARG_MIPS
    if (Get_Label_Offset(lab) != PC) {
	DevWarn ("label %s offset %lld doesn't match PC %d", 
		LABEL_name(lab), Get_Label_Offset(lab), PC);
    }
#endif
#endif
  }

  // hack to keep track of last label and offset for assembly dwarf (suneel)
  if (generate_dwarf && ! (CG_emit_asm_dwarf
#ifdef TARG_STxP70
                           && Full_Debug_Info
#endif
                           )) {
    if (Last_Label == LABEL_IDX_ZERO) {
      Last_Label = Gen_Label_For_BB (bb);
      Offset_From_Last_Label = 0;
      if (Initial_Pu_Label == LABEL_IDX_ZERO) {
        Initial_Pu_Label = Last_Label;
      }
    }
  }

  st = BB_st(bb);
  if (st) {
    if (Assembly || Lai_Code) {
#ifdef TARG_ST
      if (List_Notes) {
	fprintf (Output_File, "%s:\t%s 0x%llx\n", ST_name(st), ASM_CMNT_LINE, ST_ofst(st));
      } else {
	fprintf (Output_File, "%s:\n", ST_name(st));
      }
#else
      fprintf (Output_File, "%s:\t%s 0x%llx\n", ST_name(st), ASM_CMNT_LINE, ST_ofst(st));
#endif
    }

    Is_True (ST_ofst(st) == PC, ("st %s offset %lld doesn't match PC %d", 
	ST_name(st), ST_ofst(st), PC));
    Is_True (!Has_Base_Block(st) 
	     || (ST_base(st) == (BB_cold(bb) ? cold_base : text_base)),
	     ("sym %s base doesn't match BB:%d",
	     ST_name(st), BB_id(bb)));
    FmtAssert(ST_is_export_local(st),
	      ("ST for BB:%d not EXPORT_LOCAL", BB_id(bb)));
  }

  /* write out all the notes for this BB */
  if (Assembly && List_Notes) {
    if (BB_loop_head_bb(bb)) {
      Emit_Loop_Note(bb, Asm_File);
    }
#if 0
    if (BB_annotations(bb) && 
	ANNOT_Get(BB_annotations(bb), ANNOT_ROTATING_KERNEL))
      Emit_SWP_Note(bb, Asm_File);
#endif
    if (BB_has_note(bb)) {
      NOTE_BB_Act (bb, NOTE_PRINT_TO_FILE, Asm_File);
    }

    FREQ_Print_BB_Note(bb, Asm_File);
  }
  if (Lai_Code && List_Notes) {
    if (BB_loop_head_bb(bb)) {
      Emit_Loop_Note(bb, Lai_File);
    }
#if 0
    if (BB_annotations(bb) && 
	ANNOT_Get(BB_annotations(bb), ANNOT_ROTATING_KERNEL))
      Emit_SWP_Note(bb, Lai_File);
#endif
    if (BB_has_note(bb)) {
      NOTE_BB_Act (bb, NOTE_PRINT_TO_FILE, Lai_File);
    }

    FREQ_Print_BB_Note(bb, Lai_File);
  }

#if 0
  if (Run_prompf) {
    if (BB_loop_head_bb(bb)) {
      Emit_Loop_Note(bb, anl_file);
    }

    if (BB_has_note(bb)) {
      NOTE_BB_Act (bb, NOTE_PRINT_TO_ANL_FILE, anl_file);
    }
  }
#endif

#if Is_True_On
  /*  Init_Sanity_Checking_For_BB (); */
#endif
  
  if ((ISA_MAX_SLOTS > 1) && LOCS_Enable_Bundle_Formation) {
    Assemble_Bundles(bb);
  } else {
    Assemble_Ops(bb);
  }

#if 0
  if (Object_Code && BB_exit(bb)) {
    Em_Add_New_Event (EK_EXIT, PC - 2*ISA_MAX_INST_BYTES, 0, 0, 0, PU_section);
  }
#endif

#ifdef TARG_STxP70
  CGEMIT_Loop_End_Directive(bb);
#endif

  return;
}

/* ====================================================================
 *   Recompute_Label_Offset
 * ====================================================================
 */
static void
Recompute_Label_Offset (
  INT32 pcs[2]
)
{
  BB *bb;
  INT32 cur_pcs[2];

  /* recompute the addresses of all labels in the procedure. */
  cur_pcs[IHOT] = text_PC;
  cur_pcs[ICOLD] = cold_PC;
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    ANNOTATION *ant;
    OP *op;
    INT isect = BB_cold(bb) ? ICOLD : IHOT;
    INT32 cur_pc = cur_pcs[isect];
    LABEL_IDX lab;
    ST *st = BB_st(bb);
    if (st) {
      Set_ST_ofst(st, cur_pc);
      Set_ST_base(st, BB_cold(bb) ? cold_base : text_base);
    }
    for (ant = ANNOT_First (BB_annotations(bb), ANNOT_LABEL);
         ant != NULL;
         ant = ANNOT_Next (ant, ANNOT_LABEL))
    {
      lab = ANNOT_label(ant);
      Set_Label_Offset(lab, cur_pc);
    }
    for (op = BB_first_op(bb); op; op = OP_next(op)) {
#ifdef TARG_ST
      lab = Get_OP_Tag(op);
      if (lab) {
#else
      if (OP_has_tag(op)) {
	lab = Get_OP_Tag(op);
#endif
      	Set_Label_Offset(lab, cur_pc);
      }
#ifdef TARG_ST
      INT num_slots = OP_Real_Unit_Slots (op);
      cur_pc = PC_Incr_N(cur_pc, num_slots);
#else
      INT num_inst_words = OP_Real_Inst_Words (op);
      cur_pc = PC_Incr_N(cur_pc, num_inst_words);
#endif
    }
    cur_pcs[isect] = cur_pc;
  }

  Is_True(cur_pcs[IHOT] == pcs[IHOT],
	 ("Recompute_Label_Offsets: hot region PC mismatch (%d vs %d).",
	  cur_pcs[IHOT], pcs[IHOT]));
  Is_True(cur_pcs[ICOLD] == pcs[ICOLD],
	 ("Recompute_Label_Offsets: cold region PC mismatch (%d vs %d).",
	  cur_pcs[ICOLD], pcs[ICOLD]));
}

/* ====================================================================
 *   Fixup_Long_Branches
 * ====================================================================
 */
static void
Fixup_Long_Branches (
  INT32 *hot_size, 
  INT32 *cold_size
)
{
  FmtAssert(FALSE,
	    ("Fixup_Long_Branches: not implemented"));
}

/* ====================================================================
 *   Pad_BB_With_Noops
 *
 *   When we have bundles, 'num' is the number of bundles, not
 *   insructions.
 * ====================================================================
 */
static void 
Pad_BB_With_Noops (
  BB *bb, 
  INT num
)
{
  OP *new_op;
  OPS new_ops = OPS_EMPTY;

  if (ISA_MAX_SLOTS > 1) {
    INT ibundle;
#ifdef TARG_ST
    ISA_EXEC_MASK slot_mask;
#else
    UINT64 slot_mask;
#endif

    /* Choose a bundle for the nops. For now we just pick the first
     * bundle without a stop bit. We could chose more smartly based
     * on the previous contents of the BB.
     */
    for (ibundle = 0; ibundle < ISA_MAX_BUNDLES; ++ibundle) {
      UINT32 stop_mask = ISA_EXEC_Stop_Mask(ibundle);
      slot_mask = ISA_EXEC_Slot_Mask(ibundle);
      if (stop_mask == 0) break;
    }

    do {
      INT slot = ISA_MAX_SLOTS - 1;
      do {
	ISA_EXEC_UNIT_PROPERTY unit;
#ifdef TARG_ST
	unit = TI_BUNDLE_Get_Slot_Mask_Property (slot_mask, slot);
#else
	unit =  (ISA_EXEC_UNIT_PROPERTY)(
		  (slot_mask >> (ISA_TAG_SHIFT * slot)) 
		& ((1 << ISA_TAG_SHIFT) - 1));
#endif
	Exp_Noop(&new_ops);
	new_op = OPS_last(&new_ops);
	OP_scycle(new_op) = OP_scycle(BB_last_op(bb));
	OP_Change_Opcode(new_op, CGTARG_Noop_Top(unit));
	BB_Append_Op (bb, new_op);
#ifdef TARG_ST
	slot -= OP_Real_Unit_Slots(new_op);
#else
	slot -= OP_Real_Inst_Words(new_op);
#endif
      } while (slot >= 0);
    } while (--num);
  } else {
    do {
      Exp_Noop(&new_ops);
      new_op = OPS_last(&new_ops);
      OP_scycle(new_op) = OP_scycle(BB_last_op(bb));
      BB_Append_Op (bb, new_op);
    } while (--num);
  }
}

/* ====================================================================
 *   R_Resolve_Branches
 *
 *   Assign addresses to all local labels. Fixup long branches.
 * ====================================================================
 */
static void
R_Resolve_Branches (
  ST *pu_st
)
{
  BB *bb;
  BB *prev_nonempty_bbs[2] = { NULL, NULL };
  INT32 curpcs[2];
  INT32 hot_size, cold_size;

  /* check for T5 workarounds */
  Hardware_Workarounds();

  curpcs[IHOT] = text_PC;
  curpcs[ICOLD] = cold_PC;
  for (bb = REGION_First_BB; bb; bb = BB_next(bb)) {
    INT32 bb_start_pc;
    OP *op;
    INT32 isect = BB_cold(bb) ? ICOLD : IHOT;
    INT32 curpc = curpcs[isect];
    BB *prev_nonempty_bb = prev_nonempty_bbs[isect];

#ifndef TARG_ST
    // [CG] Removed this code. Alignment of BB is handled in EMT_Emit_BB.
    /* need prev bb to align */
    if (prev_nonempty_bb != NULL) {
      INT32 num = Check_If_Should_Align_BB (bb, curpc);
      if (num != 0) {
	if (Trace_Inst) {
	  #pragma mips_frequency_hint NEVER
	  fprintf(TFile, "insert %d noops at bb %d\n", num, BB_id(bb));
	}
	// increment curpc by 'num' bundles
	curpc += ISA_MAX_INST_BYTES * num;
	Pad_BB_With_Noops(prev_nonempty_bb, num);
      }
    }
#endif

    bb_start_pc = curpc;

    /* If there is no label, make one: */
    Gen_Label_For_BB ( bb );

    for (op = BB_first_op(bb); op; op = OP_next(op)) {
#ifdef TARG_ST
      INT num_slots = OP_Real_Unit_Slots (op);
      curpc = PC_Incr_N(curpc, num_slots);
#else
      INT num_inst_words = OP_Real_Inst_Words (op);
      curpc = PC_Incr_N(curpc, num_inst_words);
#endif
    }
    
    if (curpc != bb_start_pc) prev_nonempty_bbs[isect] = bb;

    curpcs[isect] = curpc;
  }

  Recompute_Label_Offset(curpcs);

  hot_size = curpcs[IHOT] - text_PC;
  cold_size = curpcs[ICOLD] - cold_PC;

  // if large text size or has branch predicts (which are limited in size),
  // then check for long branches.
  if (hot_size >= MIN_BRANCH_DISP || cold_size >= MIN_BRANCH_DISP
    || CGTARG_Has_Branch_Predict()) 
  {
    Fixup_Long_Branches (&hot_size, &cold_size);
  }

  if (generate_elf_symbols) {
    ;
  }

  if (Trace_Inst) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "INSTS IN PROCEDURE: %d\n", hot_size + cold_size);
  }
}

/* ====================================================================
 *    emit_global_symbols
 *
 *    initialize any new global sections.
 *    We don't do this in EMT_Begin_File because may not have done
 *    data layout at that point.
 * ====================================================================
 */
// removed static for MiniR FAB 
void
emit_global_symbols ()
{
  INT i;
  static UINT last_global_index = 1;

  for (i = last_global_index; i < ST_Table_Size(GLOBAL_SYMTAB); ++i) {
    ST* sym = &St_Table(GLOBAL_SYMTAB,i);

    if (ST_class(sym) == CLASS_BLOCK && STB_section(sym)) {
      Init_Section(sym);
    }

    // emit commons here so order is preserved for datapools
    if (ST_sclass(sym) == SCLASS_COMMON) {
      if (ST_is_not_used (sym)) continue;
      (void)EMT_Put_Elf_Symbol (sym);
    }
  }
  last_global_index = ST_Table_Size(GLOBAL_SYMTAB);

  if (Assembly)
    fprintf(Asm_File,"\n");
  if (Lai_Code)
    fprintf(Lai_File,"\n");

  return;
}

/* ====================================================================
 *    emit_function_prototypes
 * ====================================================================
 */
static void
emit_function_prototypes ()
{
  INT i, j;
  static UINT last_global_index = 1;

  for (i = last_global_index; i < ST_Table_Size(GLOBAL_SYMTAB); ++i) {
    ST* sym = &St_Table(GLOBAL_SYMTAB,i);

    if (ST_is_not_used(sym))
      continue;

    if (ST_class(sym) == CLASS_FUNC) {
      ISA_REGISTER_CLASS rc;

      TY_IDX prototype = PU_prototype(Pu_Table[ST_pu(sym)]);

      if (!TY_has_prototype(prototype)) continue;

      FmtAssert(prototype != 0,
		("emit_function_prototypes: empty function prototype"));

      fprintf(Lai_File,"\t.proto\t%s", ST_name(sym));

      // parameters:
      TYLIST_ITER fn_iter = Make_tylist_iter(TY_parms(prototype));
      while (*fn_iter) {
	rc = Register_Class_For_Mtype(TY_mtype(Ty_Table[*fn_iter]));
	fprintf(Lai_File,", %s", ISA_REGISTER_CLASS_ASM_Name (rc));
	*fn_iter++;
      }
      if (TY_is_varargs(prototype))
	fprintf(Lai_File,", ...");


      fprintf(Lai_File," => "); //, ST_name(sym));
      // return info:
      RETURN_INFO return_info = Get_Return_Info(TY_ret_type(prototype),
                                              Complex_Not_Simulated);
      for (j = 0; j < RETURN_INFO_count(return_info)-1; j++) {
	rc = Register_Class_For_Mtype(RETURN_INFO_mtype (return_info, j));
	fprintf(Lai_File,"%s, ", ISA_REGISTER_CLASS_ASM_Name (rc));
      }

      if (RETURN_INFO_count(return_info)) {
	rc = Register_Class_For_Mtype(RETURN_INFO_mtype (return_info, 
                                      RETURN_INFO_count(return_info)-1));
	fprintf(Lai_File,"%s\n", ISA_REGISTER_CLASS_ASM_Name (rc));
      }
      else {
	fprintf(Lai_File,"void\n");
      }
    }
  }
  last_global_index = ST_Table_Size(GLOBAL_SYMTAB);

  return;
}

/* ====================================================================
 *    emit_input_directives
 * ====================================================================
 */
static void
emit_input_directives ()
{
  INT    i;
  ST    *sym;
  ST    *base;
  INT64  ofst;

  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
    if (ST_is_not_used(sym)) continue;
    if (ST_sclass(sym) == SCLASS_FORMAL) {
      Base_Symbol_And_Offset (sym, &base, &ofst);
      /*
      base = ST_base(sym);
      ofst = ST_ofst(sym);
      */

      fprintf (Lai_File, "\t%s\t.input %s at %s, %lld\n", 
                   ASM_CMNT_LINE, ST_name(sym), ST_name(base), ofst);
    }
  }

  return;
}

/* ====================================================================
 *    emit_output_directives
 * ====================================================================
 */
static void
emit_output_directives ()
{
  BB *bb;
  OP *op;

  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    if (!BB_exit(bb)) continue;
    FOR_ALL_BB_OPs_FWD(bb, op) {
	TN *tn = OP_opnd(op, 0);
	ISA_REGISTER_CLASS rc = TN_register_class(tn);
	fprintf(Lai_File, "\t.output %s%d\t\t\t\t%s\n",
             ISA_REGISTER_CLASS_Symbol(rc), TN_number(tn), ASM_CMNT);
    }
  }

  return;
}

/* ====================================================================
 *    emit_virtual_registers
 *
 *    Check the TN_Vec[] to see what TNs exist.
 * ====================================================================
 */
static void
emit_virtual_registers ()
{
  INT i, j;
  TN *tn;
  ISA_REGISTER_CLASS rc;
  INT tct[ISA_REGISTER_CLASS_MAX+1];
  TN *tns[ISA_REGISTER_CLASS_MAX+1][Last_TN - First_Regular_TN + 1];

  if (Trace_Inst) {
    Print_TNs ();
  }

  FOR_ALL_ISA_REGISTER_CLASS(rc) {
    tct[rc] = 0;
  }

  for (i = First_Regular_TN; i <= Last_TN; i++) {
    tn = TNvec(i);

    if (TN_is_register(tn)) {
      rc = TN_register_class(tn);
      tns[rc][tct[rc]] = tn;
      tct[rc] += 1;
    }
  }

  FOR_ALL_ISA_REGISTER_CLASS(rc) {

    if (tct[rc] == 0) continue;

    //    fprintf(Lai_File, "\t.%s\t", ISA_REGISTER_CLASS_ASM_Name (rc));
    for (j = 0; j < tct[rc]; j++) {
      tn = tns[rc][j];

      if (j % 6 == 0)
	fprintf(Lai_File, "\n\t.%s\t %s%d",
           ISA_REGISTER_CLASS_ASM_Name (rc),
		ISA_REGISTER_CLASS_Symbol(rc), TN_number(tn));
      else
	fprintf(Lai_File, ", %s%d",
                     ISA_REGISTER_CLASS_Symbol(rc), TN_number(tn));
    }
    fprintf(Lai_File, "\n");
    //    tn = tns[rc][tct[rc]-1];
    //    fprintf(Lai_File, "%s%d \n",
    //                     ISA_REGISTER_CLASS_Symbol(rc), TN_number(tn));
  }

  return;
}

/* ====================================================================
 *    EMT_Begin_File
 *
 *    TODO: unify with making the Object and Assembly !
 * ====================================================================
 */
void
EMT_Begin_File (
  char *process_name,	/* Back end process name */
  char *options)	/* Option string used for compilation */
{
  char *buff;

  /* Initialize: */
  Trace_Elf	= Get_Trace ( TP_EMIT, 2 );
  Trace_Init	= Get_Trace ( TP_EMIT, 4 );
#ifdef TARG_ST
  Trace_Sched   = Get_Trace ( TP_SCHED, 2 );
#endif

  // Which one to write into ?
  if (Assembly) Output_File = Asm_File;
  if (Lai_Code) Output_File = Lai_File;

#if 0
  // Enumerate all instructions:
  if (Get_Trace (TP_CG, 0x100)) Enumerate_Insts();
#endif

  text_PC = 0;
  cold_PC = 0;
  Get_Ism_Name();
  if (FILE_INFO_gp_group (File_info)) {
        Has_GP_Groups = TRUE;
  }

  if (Object_Code || CG_emit_asm_dwarf) {
	generate_dwarf = TRUE;
	generate_elf_symbols = TRUE;
  }

#ifdef TARG_ST
  // (cbr) we enter here either for debug dwarf emission or exceptions frame dwarf unwinding 
  if (CXX_Exceptions_On) {
	generate_dwarf = TRUE;
	generate_elf_symbols = TRUE;
  }
#endif

  if ( generate_elf_symbols ) {
#ifdef TARG_ST
    // [CL] when generating .s file, do not
    // overwrite/remove existing .o file
    if ( ! Object_Code) {
      Obj_File_Name = make_temp_file("cctpo");
    }
#endif
 
    Obj_File = fdopen (Em_Begin_File (
			    	Obj_File_Name, 
			    	FALSE, 
			    	!Use_32_Bit_Pointers, 
				FALSE /* old_abi */,
				(INT) Target_ISA,
				(Target_Byte_Sex == BIG_ENDIAN),
				Gen_PIC_Shared, 
				Gen_PIC_Call_Shared,
                		!Guaranteed_Small_GOT, 
				Has_GP_Groups,
			    	Trace_Elf),
		       "r+");
    if (Obj_File == NULL) return;

    // If we are meant to be generating only a .s file but that .s
    // file needs to have dwarf information represented in it, we
    // currently use the em_elf and libelf routines to maintain
    // section indices, symbol indices, etc. for libdwarf. In such a
    // situation, we generate the object file, but we unlink it here
    // so it never shows up after the compilation is done.
    if ( ! Object_Code) {
#ifdef TARG_ST
      // [CL] under Win32, we must close the file before calling unlink()
      fclose(Obj_File);
      if (unlink(Obj_File_Name)) {
	ErrMsg ( EC_Obj_Close, Obj_File_Name, errno );
      }
      Obj_File = NULL;
#else
      unlink(Obj_File_Name);
#endif
    }

    buff = (char *) alloca (strlen("be") + sizeof(INCLUDE_STAMP) + 
			    strlen(ism_name) + strlen(Obj_File_Name) + 4);
    if (*ism_name != '\0')
	sprintf(buff, "be::%s-%s:%s", INCLUDE_STAMP, ism_name, Obj_File_Name);
    else
	sprintf(buff, "be::%s:%s", INCLUDE_STAMP, Obj_File_Name);
    Em_Add_Comment (buff);
    if ( ! DEBUG_Optimize_Space) {
    	buff = (char *) alloca (strlen("be-options") + strlen(options) + 4);
    	sprintf(buff, "be-options:::%s", options);
    	Em_Add_Comment (buff);
    }

#if 0
    if ( EMIT_interface_section ) Interface_Scn_Begin_File();
#endif
  }

  Init_ST_elf_index(GLOBAL_SYMTAB);
  if (generate_dwarf) {
    Cg_Dwarf_Begin (!Use_32_Bit_Pointers);
  }
  Cg_Dwarf_Gen_Asm_File_Table ();

  if (Assembly) {
    ASM_DIR_NOREORDER();
    ASM_DIR_NOAT();
    fprintf ( Asm_File, "\t%s  %s::%s\n", ASM_CMNT_LINE, process_name, 
			    INCLUDE_STAMP );
    if (*ism_name != '\0')
    	fprintf ( Asm_File, "\t%s%s\t%s\n", ASM_CMNT_LINE, "ism", ism_name);
    List_Compile_Options ( Asm_File, "\t"ASM_CMNT_LINE, FALSE, TRUE, TRUE );
    /* TODO: do we need to do .interface stuff for asm files? */
  }

  if (Lai_Code) {
    // These are for the .lai processing:
    //    LAI_Begin_File (process_name, options);


    fprintf (Lai_File, "\t%s  %s::%s\n", ASM_CMNT_LINE, process_name, 
			    INCLUDE_STAMP);
    if (*ism_name != '\0')
      fprintf (Lai_File, "\t%s%s\t%s\n", ASM_CMNT_LINE, "ism", ism_name);

    List_Compile_Options (Lai_File, "\t"ASM_CMNT_LINE, FALSE, TRUE, TRUE);
  }

  //  Init_Tcon_Info ();

#ifdef TARG_ST
  // Target dependent begin file.
  if (Assembly) CGEMIT_Begin_File_In_Asm ();
#endif

  return;
}


/* ====================================================================
 *    EMT_Emit_PU (pu, pu_dst, rwn)
 * ====================================================================
 */
void
EMT_Emit_PU (
  ST      *pu,
  DST_IDX  pu_dst, 
  WN      *rwn
)
{
  ST *sym;
  ST *base;
  BB *bb;
  INT Initial_Pu_PC;
  INT64 ofst;
  INT i;

  // bugs 2178, 2152
  // fix bug OSP_115
  // "__inline__" is a reserved keyword in C so it's 
  // safe(ish) to use in headers, including sys/*.h
  // when PU is marked extern _inline_ in C, 
  // needn't emit anything.
  if ( PU_is_extern_inline (Pu_Table[ST_pu(pu)]) ) {
    return;
  }

  Trace_Inst	= Get_Trace ( TP_EMIT,1 );
  BOOL trace_unwind = Get_Trace (TP_EMIT, 64);

  if (Trace_Inst) {
    fprintf(TFile, "%s CFG before cgemit \n%s", DBar, DBar);
    Print_All_BBs ();
  }

// [VL-HMP] 
#ifdef TARG_STxP70
  CGEMIT_Task_Asm_Macro();
#endif

  Init_Unwind_Info (trace_unwind);

#if 0
  /* In the IA-32 case, we need to convert fp register references
   * so that they reference entries in the fp register stack with the
   * proper offset.
   */
  STACK_FP_Fixup_PU();

  if ( Run_prompf ) {
    const char *path = Anl_File_Path();
    anl_file = fopen(path, "a");
    fprintf(anl_file, "\n");
  }
#endif

  Init_ST_elf_index(CURRENT_SYMTAB);

  cur_section = NULL;

  if (Lai_Code) {
    // I cannot do this while it is constantly changing !
    // When they learn to work out the specs and abide by them,
    // and communicate, I might enable this again, Arthur.
    //  emit_function_prototypes ();
  }

  // initialize any new global sections:
  // We don't do this in EMT_Begin_File because may not have done
  // data layout at that point.
#if 0 // for debug FAB
  if (Assembly) fprintf( Asm_File, "## %s:%d before global syms\n", __FILE__, __LINE__ );
#endif
  emit_global_symbols ();

  // emit global bss first so .org is correct
  Process_Bss_Data (GLOBAL_SYMTAB);

  /* Initialize any sections that might have been created by the backend. */
  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
    base = Base_Symbol(sym);
    if (ST_class(base) == CLASS_BLOCK && STB_section(base)) {
      Init_Section(base);
    }
  }

#ifdef TARG_ST
  // [CG]: Create_Cold_Text_Section() into Setup_Text_Section_For_PU()
  Setup_Text_Section_For_PU (pu);
#else
  Create_Cold_Text_Section();

  Setup_Text_Section_For_PU (pu);
#endif

  Initial_Pu_PC = PC;
  Set_ST_ofst(pu, PC);

  /* 
   * Assign addresses to all local labels. Fixup long branches.
   * Perform branch target optimization.
   */
  R_Resolve_Branches (pu);

#if 0
  if (Object_Code) {
    Em_Add_New_Event (EK_ENTRY, PC, 0, 0, 0, PU_section);
  }
#endif
  if ( Assembly ) {
#ifdef TARG_ST
    if ( List_Notes ) {
#endif
#ifdef TARG_ST
      /* (cbr) demangle name */
      if (PU_src_lang(Get_Current_PU()) == PU_CXX_LANG) {
	int status_cp_name ;
	char *cp_name =  abi::__cxa_demangle(ST_name(pu), NULL, NULL, &status_cp_name) ;
	if (!status_cp_name && cp_name) {
	  fprintf ( Asm_File, "\n\t%s Program Unit: %s\n", ASM_CMNT_LINE, cp_name );
	  ::free(cp_name) ;
	}
      }
      else
#endif
        fprintf ( Asm_File, "\n\t%s Program Unit: %s\n", ASM_CMNT_LINE, ST_name(pu) );
    }
#ifdef TARG_ST
    //[TB] Add optimization level info
    if (PU_size_opt(Get_Current_PU()) != PU_OPTLEVEL_UNDEF)
        fprintf ( Asm_File, "\t%s Size optimization level: %d\n", ASM_CMNT, PU_size_opt(Get_Current_PU()) );
//     if (PU_perf_opt(Get_Current_PU()) != PU_OPTLEVEL_UNDEF)
//         fprintf ( Asm_File, "\n\t%s Performance optimization level: %d\n", ASM_CMNT, PU_perf_opt(Get_Current_PU()) );

    // [CL] force alignment when starting a new function
    // (as alignment constraints in hb_hazards.cxx:Make_Bundles()
    // are reset for each new PU, we must reset alignment here)
    // [CG] Added call to Check_If_Should_Align_PU()
    INT pu_align = Check_If_Should_Align_PU(pu);
    if (pu_align < DEFAULT_FUNCTION_ALIGNMENT) pu_align = DEFAULT_FUNCTION_ALIGNMENT;
    if (pu_align) {
      fprintf(Asm_File, "\t%s %d\n", AS_ALIGN, pu_align);
    }
#endif
    if (AS_ENT) CGEMIT_Prn_Ent_In_Asm (pu);
#ifdef TEMPORARY_STABS_FOR_GDB
    // This is an ugly hack to enable basic debugging for IA-32 target
    if (Debug_Level > 0) {
      fprintf(Asm_File, ".stabs \"%s:F(0,1)\",36,0,0,%s\n", ST_name(pu), ST_name(pu));
      fprintf(Asm_File, "\t%s\t%s,%s\n", AS_TYPE, ST_name(pu), AS_TYPE_FUNC);
    }
#endif

    Print_Label (Asm_File, NULL, pu, 0);
    // .fframe is only used for unwind info,
    // and we plan on emitting that info directly.
    //    CGEMIT_Gen_Asm_Frame (Frame_Len);
  }

  if (Lai_Code) {
    fprintf (Lai_File, "\n\t%s Program Unit: %s\n", ASM_CMNT_LINE, ST_name(pu) );
    if (AS_ENT) CGEMIT_Prn_Ent_In_Asm (pu);

    Print_Label (Lai_File, NULL, pu, 0);
    // .fframe is only used for unwind info,
    // and we plan on emitting that info directly.
    //    CGEMIT_Gen_Asm_Frame (Frame_Len);
  }

  if (Assembly) {
    if (cur_section != PU_base) {
	/* reset to text section */ 
   	fprintf (Asm_File, "\n\t%s %s\n", AS_SECTION, ST_name(PU_base));
	cur_section = PU_base;
    }
  }
  if (Lai_Code) {
    if (cur_section != PU_base) {
      /* reset to text section */
      fprintf (Lai_File, "\n\t%s %s\n", AS_SECTION, ST_name(PU_base));
      cur_section = PU_base;
    }
    emit_input_directives ();
  }

  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
    if (ST_is_not_used(sym)) continue;
    if (ST_sclass(sym) == SCLASS_COMMON) {
      (void)EMT_Put_Elf_Symbol (sym);
    }

    /* put all extern symbols into the elf symbol table. */
    if ((PU_src_lang(Get_Current_PU()) == PU_F77_LANG 
	|| PU_src_lang(Get_Current_PU()) == PU_F90_LANG) &&
	ST_sclass(sym) == SCLASS_EXTERN && 
	! ST_is_export_local(sym)) {
#ifdef TARG_ST
      if (Assembly) {
	fprintf(Asm_File, "\t%s\t", AS_GLOBAL);
	EMT_Write_Qualified_Name(Asm_File, sym);
	fprintf ( Asm_File, "\n");
	EMT_Visibility (Asm_File, NULL, ST_export(sym), sym);
      }
      if (Lai_Code) {
	fprintf(Lai_File, "\t%s\t", AS_GLOBAL);
	EMT_Write_Qualified_Name(Lai_File, sym);
	fprintf(Lai_File, "\n");
	EMT_Visibility (Lai_File, NULL, ST_export(sym), sym);
      }
#else
      if (Assembly) fprintf (Asm_File, "\t%s\t %s\n", AS_GLOBAL, ST_name(sym));
      if (Lai_Code) fprintf (Lai_File, "\t%s\t %s\n", AS_GLOBAL, ST_name(sym));
#endif
      if (Object_Code) EMT_Put_Elf_Symbol (sym);
    }

    if (Assembly) {
      if (ST_class(sym) == CLASS_VAR && ST_sclass(sym) == SCLASS_AUTO) {
	if (Has_Base_Block(sym)) {
	  Base_Symbol_And_Offset(sym, &base, &ofst);
	  if (List_Notes) {
	    fprintf ( Asm_File, "\t%s %s = %lld\n",
		      ASM_CMNT_LINE, ST_name(sym), ofst);
	  }
	}
      }
    }
    if (Lai_Code) {
      if (ST_class(sym) == CLASS_VAR && ST_sclass(sym) == SCLASS_AUTO) {
	if (Has_Base_Block(sym)) {
	  Base_Symbol_And_Offset(sym, &base, &ofst);
	  fprintf (Lai_File, "\t%s\t.auto %s at %s, %lld\n", 
                        ASM_CMNT_LINE, ST_name(sym), ST_name(base), ofst);
	}
      }
    }
  }

  if (Lai_Code) {
    /* emit vertual register declarations */
    emit_virtual_registers ();
  }

  /* Assemble each basic block in the PU */
  BB *bb_last = NULL;
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    Setup_Text_Section_For_BB(bb);
    EMT_Assemble_BB (bb, rwn);
    bb_last = bb;

  }


#ifdef TARG_ST
  // [CL] make sure at least one debug label is emitted: for
  // compiler-generated PUs, we have no line information, so we may
  // have no debug_line label generated which confuses
  // Cg_Dwarf_Process_PU below

  // [CL] #58127
  // But be careful not to enter it in the Dwarf symtab: we should not
  // generate any line info for such PUs.
  if (CG_emit_asm_dwarf) {
    bool No_Label = FALSE;
    if (Last_Label == LABEL_IDX_ZERO) {
      No_Label = TRUE;
    }
    if (No_Label || Dwarf_Require_Symbolic_Offsets()) {
      New_Debug_Line_Set_Label(PC2Addr(PC), TRUE);
    }
    if (!Dwarf_Require_Symbolic_Offsets() && (No_Label == FALSE)) {
      cache_last_label_info (Last_Label,
			     Em_Create_Section_Symbol(PU_section),
			     current_pu,
			     PC2Addr(Offset_From_Last_Label));
      pSCNINFO old_PU_section = em_scn[STB_scninfo_idx(PU_base)].scninfo;
      end_previous_text_region(old_PU_section, 
			       PC);
    }
  }
#endif

#ifdef SPLIT_BCO_ENABLED /* Thierry */
  Gen_EndSplit_Label(Asm_File);  
#endif /* BCO_Enabled Thierry */


  /* Revert back to the text section to end the PU. */
  Setup_Text_Section_For_BB(REGION_First_BB);
  Is_True(PU_base == text_base, ("first region BB was not in text section"));

  /* Emit the stuff needed at the end of the PU. */
  if (AS_END) {
    CGEMIT_Exit_In_Asm(pu);
  }

#ifdef BCO_ENABLED /* Thierry */
  /* Emit the profile_info associated with this PU. */
  if (Assembly && CG_emit_bb_freqs) 
    EMT_ProfileInfo_PU(pu, pu_dst, rwn);
#endif /* BCO_Enabled Thierry */

  /* Emit the initialized data associated with this PU. */
  Process_Initos_And_Literals (CURRENT_SYMTAB);
  Process_Bss_Data (CURRENT_SYMTAB);

  if (generate_dwarf) {
    Elf64_Word symindex;
    INT eh_offset=0;
    BOOL has_exc_scopes = PU_has_exc_scopes(ST_pu(pu));
#if 0
    if (Object_Code)
    	Em_Add_New_Event (EK_PEND, PC - ISA_MAX_INST_BYTES, 0, 0, 0, PU_section);
#endif
    /* get exception handling info */ 
    if (!CXX_Exceptions_On && has_exc_scopes) {
      eh_offset = symindex = (Elf64_Word)DW_DLX_EH_OFFSET_UNAVAILABLE;
    }
    else if (!has_exc_scopes) {
	eh_offset = symindex = (Elf64_Word)DW_DLX_NO_EH_OFFSET;
    }
    else {
      sym = EH_Get_PU_Range_ST();
      if (sym != NULL) {
	Base_Symbol_And_Offset (sym, &base, &ofst);
	Init_Section(base);	/* make sure eh_region is inited */
#ifdef TARG_ST
        /* (cbr) lsda entry is private range_table */
	eh_offset = 0;
	symindex = EMT_Put_Elf_Symbol (sym);
#else
	eh_offset = ofst;
	symindex = ST_elf_index(base);
#endif
      } else {
	eh_offset = symindex = (Elf64_Word)DW_DLX_NO_EH_OFFSET;
      }
    }
    // Cg_Dwarf_Process_PU (PU_section, Initial_Pu_PC, PC, pu, pu_dst, symindex, eh_offset);
#ifdef TARG_ST
    Cg_Dwarf_Process_PU (
		Em_Create_Section_Symbol(PU_section),
		Initial_Pu_Label,
		Last_Label, PC2Addr(Offset_From_Last_Label),
		pu, pu_dst, symindex, eh_offset,
		// The following two arguments need to go away
		// once libunwind provides an interface that lets
		// us specify ranges symbolically.
		PC2Addr(Initial_Pu_PC), PC2Addr(PC));

    // [CL] reset labels after emission of a PU, so that
    // next PU starts from a clean state
    Last_Label = LABEL_IDX_ZERO;
    Offset_From_Last_Label = 0;
#else
    Cg_Dwarf_Process_PU (
		Em_Create_Section_Symbol(PU_section),
		Initial_Pu_Label,
		Last_Label, Offset_From_Last_Label,
		pu, pu_dst, symindex, eh_offset,
		// The following two arguments need to go away
		// once libunwind provides an interface that lets
		// us specify ranges symbolically.
		Initial_Pu_PC, PC);
#endif
  }

  PU_Size = PC - Initial_Pu_PC;
  Set_STB_size (PU_base, PC);
  text_PC = PC;

#ifndef TARG_ST // [CL] replaced by end_previous_text_region(PU_section, Addr2PC(PC)) above
  if (generate_dwarf) {
    // The final label in this PU is liable to get used in computing
    // arguments to Em_Dwarf_End_Text_Region_Semi_Symbolic, so we need
    // to squirrel away information about it.
    cache_last_label_info (Last_Label,
		Em_Create_Section_Symbol(PU_section),
		ST_pu(pu),
		PC2Addr(Offset_From_Last_Label));
  }
#endif

  Finalize_Unwind_Info();

#if 1 // for debug
  fprintf( Asm_File, "## %s:%d, %s end\n", __FILE__, __LINE__, __FUNCTION__ );
#endif
  return;
}

#else // END_TARG_ST_IFDEF
#ifdef TARG_X8664
extern void EETARG_Emit_IP_Calc_Func(void);
#endif

extern void Early_Terminate (INT status);

#define PAD_SIZE_LIMIT	2048	/* max size to be padded in a section */

/* c++ mangled names can be any arbitrary length,
 * so this is just a first guess */ 
#define LBUF_LEN	(OP_MAX_FIXED_OPNDS*1024)

/* Instructions go into one of two ELF text sections depending on
 * whether the BB is in a hot or cold region. Hot BBs go into
 * ".text" (or ".text<pu-name>" for -TENV:section_for_each_function).
 * Cold BBs go into ".text.cold". As a result, numerous cgemit
 * routines need to track the PC of both sections, and 2 element
 * arrays are typically used. Here we define the indices for
 * the arrays, with the values chosen to coincide with the result
 * of BB_Is_Cold.
 */
enum { IHOT=FALSE, ICOLD=TRUE };

/* Overload BB flag <local_flag1> to indicate if a BB is cold or hot --
 * it's less overhead than BB_Is_Cold.
 */
#define BB_cold		BB_local_flag1
#define Set_BB_cold	Set_BB_local_flag1
#define Reset_BB_cold	Reset_BB_local_flag1

extern const char __Release_ID[];
#ifdef KEY
extern BOOL profile_arcs;
extern BOOL PU_has_trampoline;  // defined in wn_lower.cxx
#endif

/* ====================================================================
 *
 * Global data
 *
 * ====================================================================
 */

#ifndef ORIGINAL_SGI_CODE
BOOL CG_emit_asm_dwarf    = TRUE;
BOOL CG_emit_unwind_info  = TRUE;
#ifdef TARG_IA64
BOOL CG_emit_unwind_directives = TRUE;
#else
BOOL CG_emit_unwind_info_Set = FALSE;
BOOL CG_emit_unwind_directives = FALSE;
#endif
#else
BOOL CG_emit_asm_dwarf    = FALSE;
BOOL CG_emit_unwind_info  = FALSE;
BOOL CG_emit_unwind_directives = FALSE;
#endif
#ifdef KEY
BOOL CG_emit_non_gas_syntax = FALSE;
BOOL CG_inhibit_size_directive = FALSE;

#ifdef ICE9A_HW_WORKAROUND
static int muld_shadow = 0;	// Workaround mul.d hardware bug.  Bug 13027.

static OP *round_hazard_op = NULL;	// Workaround fp hardware bug.
enum reg_state {OK, PENDING, WAW}	// Bug 13481.
  fp_reg_state[33];
#endif
#endif

static BOOL generate_dwarf = FALSE;
static BOOL generate_elf_symbols = FALSE;

/* Local phase trace flags: */
static BOOL Trace_Init    = FALSE;	/* Data initialization trace */
static BOOL Trace_Inst    = FALSE;	/* Noop-insertion trace */
static BOOL Trace_Elf     = FALSE;	/* Miscellaneous ELF trace */
static BOOL Trace_Longbr  = FALSE;	/* trace handling of long branches */

BOOL Use_Page_Zero = FALSE;
static BOOL Use_Prefetch = FALSE;


// For dwarf output in assembly we need to keep track of the 
// offset of the current instruction from a known label.
// If Last_Label is LABEL_IDX_ZERO, it is assumed to be invalid.
static LABEL_IDX Last_Label = LABEL_IDX_ZERO;
static INT32 Offset_From_Last_Label = 0;

static LABEL_IDX Initial_Pu_Label;

/* NOTE: PCs are actually a bundle address and slot number pair. The 
 * slot number is in the low bits. 
 */
static INT32 PC = 0;			/* PC for current text section */
static INT32 text_PC = 0;		/* PC for hot text */
static INT32 cold_PC = 0;		/* PC for cold text */

static ST *PU_base = NULL;		/* base for current text section */
static ST *text_base = NULL;		/* base for hot text section */
static ST *cold_base = NULL;		/* base for cold text section */

static pSCNINFO PU_section = NULL;	/* current text section */
static pSCNINFO text_section = NULL;	/* hot text section */
static pSCNINFO cold_section = NULL;	/* cold text section */


static INT current_rid = 0;	/* current rid id */

typedef struct {
  pSCNINFO scninfo;
  Elf64_Word scn_ofst;
  ST *sym;
#ifdef TARG_ST100
  char *label;     // we need to label sections on some targets that
                   // do not have section relative relocations
#endif
} AUX_SCN;

static AUX_SCN *em_scn;
static INT last_scn = 0;	
static INT max_scn = 0;
/* 0 index to em_scn will be null values */

static ST *cur_section = NULL;

static PU_IDX current_pu = 0;
#ifdef TARG_ST
static int Bundle_Count;
// [SC]: Symbol_Def_After_Group is a symbol that should
// be defined immediately after the current group.
static ST *Symbol_Def_After_Group;
#endif

// put PU in separate elf text section?
// (pu could be null if no PUs)
#define Use_Separate_PU_Section(pu,st) \
	((pu != (PU_IDX) NULL) \
	&& (Section_For_Each_Function || PU_in_elf_section(pu)) \
	&& (ST_class(st) == CLASS_BLOCK) \
	&& (strcmp(ST_name(st),ELF_TEXT) == 0))

static FILE *anl_file;

/*
 * For the elf indexes, we can reuse the temp field for local STs,
 * Need to map from ST's to the elf index for the ST.
 * So create array of index arrays, where the ST_level
 * gets you to the index array, and then the ST_index 
 * gets you to the elf index for that ST.
 */

/*
 * Maximum nesting depth includes these scopes, in the deepest case:
 *  0 : not used
 *  1 : global scope
 *  2 : top-level PU
 *  3 : F90 internal procedure
 *  4 : MP PU from internal procedure
 *  5 : Fortran I/O in MP PU
 */

#define MAX_SYMTAB_DEPTH	6

static INT32 *elf_index_array[MAX_SYMTAB_DEPTH] =
  {NULL,NULL,NULL,NULL,NULL,NULL};
static INT max_elf_index[MAX_SYMTAB_DEPTH] = {0,0,0,0,0,0};

#if defined(BUILD_OS_DARWIN)
/*
 * Emit ".section" directive, mapping names onto the syntax that the Darwin
 * assember expects to see after the token ".section", and suppressing the
 * ".bss" directive entirely since Darwin doesn't support it
 * base		Symbol for section
 */
static void
emit_section_directive(ST *base) {
  char *name = ST_name(base);
  if (strcmp(name, BSS_RAW_NAME)) {
    fprintf ( Asm_File, "\n\t%s %s\n", AS_SECTION, map_section_name(name));
  }
}

/*
 * Transform an external symbol name by prepending an underscore (the usual
 * case), or synthesizing the jump-target label used to implement an indirect
 * call via a stub, or synthesizing the label used to implement a non-lazy
 * pointer to an external symbol.
 *
 * name		symbol name as it would appear in C source (e.g. "x")
 * which	(optional) how to transform name
 * returns	pointer to static underscorify_buffer containing the transformed
 *		name
 */
static char *
underscorify(char *name, darwin_indirect_t which = DO_UNDERSCORE) {
  static char *underscorify_buffer = 0;
  static unsigned underscorify_len = 0;

  /* Don't mess with symbols like ".rodata" */
  if (*name == '.') {
    return name;
  }

  unsigned name_len = strlen(name);
  const char *pattern = "%s"; /* Just in case */
  switch (which) {
    case DO_UNDERSCORE:
      pattern = "_%s";
      break;
    case DO_STUB:
      pattern = "L_%s$stub";
      break;
    case DO_NON_LAZY_PTR:
      pattern = "L_%s$non_lazy_ptr";
      break;
    }
  unsigned int sizeof_pattern = strlen(pattern) + sizeof '\0';
  if (name_len + sizeof_pattern > underscorify_len) {
    free(underscorify_buffer);
    underscorify_buffer = (char *) malloc(2 * name_len + sizeof_pattern);
  }
  sprintf(underscorify_buffer, pattern, name);
  return underscorify_buffer;
}

/*
 * Generate __IMPORT,__jump_table entry for symbol name the first time we
 * need it; or generate __IMPORT,__pointers entry for symbol "name" the
 * first time we need it
 * name		symbol name as it would appear in C source
 * which	which kind of entry to generate
 */
static void
gen_indirect(char *name, darwin_indirect_t which) {
  static std::set<std::string> already_generated_jump;
  static std::set<std::string> already_generated_ptr;
  std::string sname(name);

  if (DO_STUB == which) {
    if (!already_generated_jump.insert(sname).second) {
      return;
      }
    fprintf(Asm_File, "\t.section __IMPORT,__jump_table,symbol_stubs,"
      "self_modifying_code+pure_instructions,5\n");
    fprintf(Asm_File, "%s:\n", underscorify(name, which));
    fprintf(Asm_File, "\t.indirect_symbol %s\n", underscorify(name,
      DO_UNDERSCORE));
    fprintf(Asm_File, "\thlt; hlt; hlt; hlt; hlt;\n");
  } else if (DO_NON_LAZY_PTR == which) {
    if (!already_generated_ptr.insert(sname).second) {
      return;
      }
    fprintf(Asm_File, "\t.section __IMPORT,__pointers,"
      "non_lazy_symbol_pointers\n");
    fprintf(Asm_File, "%s:\n", underscorify(name, which));
    fprintf(Asm_File, "\t.indirect_symbol %s\n", underscorify(name,
      DO_UNDERSCORE));
    fprintf(Asm_File, "\t.long 0\n");
  }
  fprintf(Asm_File, "\t%s __TEXT,__text\n", AS_SECTION);
}

/* Return log base 2 of an integer (use for .align on systems which expect
 * the argument to be the log of the alignment) */
int
logtwo(int value)
{
  int result = 0;
  for (; value > 1; value >>= 1) {
    result++;
  }
  return result;
}

/*
 * Darwin doesn't permit direct manipulation of the ".bss" section via
 * directives like ".section", ".org", ".skip", ".align". The only access
 * is via the ".lcomm" directive. So in Process_Bss_Data(), we make a list
 * of the symbols that are supposed to be in ".bss", computing the maximum
 * offset (size, actually) and alignment. At the end of compilation, we
 * emit ".lcomm .bss,<size>,<alignment>" followed by a
 * ".set <name>,.bss+<offset>" for each symbol.
 */
struct darwin_remember {
  char *name;
  UINT64 offset;

  darwin_remember(char *name_, UINT64 offset_): name(strdup(name_)),
    offset(offset_) {}
  ~darwin_remember() { free(name); }
  private:
    darwin_remember(darwin_remember &) {}
  };

class darwin_bss {
  static vector< darwin_remember * > revisit_bss_list;
  static UINT64 revisit_bss_size;
  static unsigned revisit_bss_maxalign;
  static vstring &qualified_name(vstring &, ST *);

  static UINT64 update_bss_size(ST *, UINT64 *, unsigned * = 0);

public:

  static void remember_bss_symbol(ST *);
  static void finish_bss_symbols(FILE *);
};

vector< darwin_remember * > darwin_bss::revisit_bss_list;
UINT64 darwin_bss::revisit_bss_size;
unsigned darwin_bss::revisit_bss_maxalign;

// Modeled upon EMT_Write_Qualified_Name()
vstring &
darwin_bss::qualified_name(vstring &v, ST *st) {
  // No name (can only be referred to as ".bss+offset")
  if (0 == ST_name(st) || 0 == *(ST_name(st))) {
    return v;
  }
  vstr_concat(v, underscorify(ST_name(st)));
  // This name is already unique, don't change it.
  if (!strncmp (ST_name(st), ".range_table.", strlen(".range_table."))) {
    return v;
  }
  // See comment in EMT_Write_Qualified_Name
  if ( ST_is_export_local(st) && ST_class(st) == CLASS_VAR) {
    if (ST_level(st) == GLOBAL_SYMTAB)
	vstr_sprintf(&v, vstr_len(v), "%s%d",
	  Label_Name_Separator, ST_index(st));
    else
	vstr_sprintf(&v, vstr_len(v), "%s%d%s%d", Label_Name_Separator, 
	    ST_pu(Get_Current_PU_ST()), Label_Name_Separator, ST_index(st) );
  }
  else if (*Symbol_Name_Suffix) {
    vstr_concat(v, Symbol_Name_Suffix);
  }
  return v;
}

/*
 * sym		.bss symbol 
 * accum_size	Input: size of .bss prior to this symbol. Output: size
 *		following this symbol
 * maxalign	Input: Maximum alignment required prior to this symbol.
 *		Output: Maximum alignment following this symbol. Optional
 * returns	Offset from .bss to this particular symbol
 */
UINT64
darwin_bss::update_bss_size(ST *sym, UINT64 *accum_size, unsigned *maxalign) {
  UINT64 save_accum_size = *accum_size;
  UINT64 size = TY_size(ST_type(sym));
  unsigned align = TY_align(ST_type(sym));
  unsigned modalign = save_accum_size % align;
  unsigned pad = modalign ? (align - modalign) : 0;
  if (maxalign) {
    *maxalign = MAX(align, (*maxalign));
  }
  *accum_size += (size + modalign);
  return save_accum_size + pad;
}

/*
 * Add a symbol to the list of ".bss" symbols which we must revisit at the
 * end of compilation
 * sym		Symbol in ".bss" section
 */
void
darwin_bss::remember_bss_symbol(ST *sym) {
  /* TBD: Might need other stuff from Print_Label? */
  vstring v = vstr_begin(64);
  char *name = vstr_str(qualified_name(v, sym));
  revisit_bss_list.push_back(new darwin_remember(name,
    update_bss_size(sym, &revisit_bss_size, &revisit_bss_maxalign)));
  vstr_end(v);
}

/*
 * After seeing the entire list of .bss symbols from all calls to
 * Process_Bss_Data(), emit a ".lcomm .bss" directive to reserve space for all
 * of them, and then emit ".set" directives to define each one as an offset
 * from the beginning of ".lcomm".
 */
void
darwin_bss::finish_bss_symbols(FILE *asm_file) {
  fprintf(asm_file, "\t.lcomm %s, %" SCNd64 ", %d\n",
    BSS_RAW_NAME, revisit_bss_size, logtwo(revisit_bss_maxalign));
  UINT64 accum_size = 0;
  for (vector< darwin_remember * >::iterator bi = revisit_bss_list.begin();
    bi != revisit_bss_list.end(); ++bi) {
    darwin_remember *r = *bi;
    fprintf(asm_file, "\t.set %s, %s+%" SCNd64 "\n",
      r->name, BSS_RAW_NAME, r->offset);
    delete r;
  }
  revisit_bss_size = 0;
  revisit_bss_maxalign = 0;
  revisit_bss_list.clear();
}
#endif /* defined(BUILD_OS_DARWIN) */

static void
Allocate_Elf_Index_Space (UINT level)
{
	INT num_sts = ST_Table_Size(level);

	if (elf_index_array[level] == NULL) {
		max_elf_index[level] = num_sts + 100;
		/* for the size take the #st's + extra space for new symbols */
		elf_index_array[level] = (INT32*) Src_Alloc(
			sizeof(INT32) * max_elf_index[level]);
	} else if (max_elf_index[level] < num_sts + 10) {  
		/* # globals has grown, so realloc. */
		/* we realloc even if close and not yet overflowed,
		 * because may alloc new symbols as part of cg. */
		elf_index_array[level] = TYPE_MEM_POOL_REALLOC_N (INT32, 
			&MEM_src_pool, elf_index_array[level], 
			max_elf_index[level], num_sts + 100);
		max_elf_index[level] = num_sts + 100;
	}
}

static void
Init_ST_elf_index (UINT stab)
{
	UINT level = stab;
	INT i;
	Allocate_Elf_Index_Space(GLOBAL_SYMTAB);
	Is_True((level < MAX_SYMTAB_DEPTH), 
		("Init_ST_elf_index overflow"));
	if (level > GLOBAL_SYMTAB && elf_index_array[level] != NULL) {
		/* need to clear the values in case leftover 
	 	 * from previous uplevel */
		for (i = 0; i < max_elf_index[level]; i++) {
			elf_index_array[level][i] = 0;
		}
	}
	Allocate_Elf_Index_Space(stab);
}

static INT32
ST_elf_index (ST *st)
{
	INT level = ST_level(st);
	Is_True((level < MAX_SYMTAB_DEPTH), 
		("ST_elf_index overflow"));
	Is_True((ST_index(st) < max_elf_index[level]), 
		("ST_elf_index overflow"));
	return elf_index_array[level][ST_index(st)];
}

static void
Set_ST_elf_index (ST *st, INT32 v)
{
	INT level = ST_level(st);
	Is_True((level < MAX_SYMTAB_DEPTH), 
		("Set_ST_elf_index overflow"));
	Is_True((ST_index(st) < max_elf_index[level]),
		("Set_ST_elf_index overflow"));
	elf_index_array[level][ST_index(st)] = v;
}

/* Determine if the symbol has a base of gp */
static BOOL
ST_is_gp_relative(ST *st)
{
  ST *base_st = Base_Symbol (st);

  return ((ST_class(base_st) == CLASS_BLOCK || 
	   ST_class(base_st) == CLASS_VAR) && 
	  ST_gprel(base_st));
}


#define Is_Text_Section(st) (STB_exec(st) && strncmp(ST_name(st), ELF_TEXT,5)==0)

const char* SHF_String (INT f)
{
  switch (f) {
  case SHF_WRITE:
    return "writable";
  case SHF_ALLOC:
    return "allocatable";
  case SHF_EXECINSTR:
    return "executable";
  case SHF_MERGE:
    return "to-be-merged";
  case SHF_STRINGS:
    return "string";
  case SHF_INFO_LINK:
    return "info-link";
  case SHF_LINK_ORDER:
    return "link-order";
  case SHF_OS_NONCONFORMING:
    return "os-nonconforming";
  case SHF_GROUP:
    return "section-group-member";
  case SHF_TLS:
    return "thread-local-storage";
  case SHF_MASKOS:
    return "mask-os";
  case SHF_MASKPROC:
    return "mask-proc";
  default:
    return "unknown";
  }
}

const char *SHFmask_String ( Elf64_Word m )
{
  Elf64_Word f;
  std::string sep="";
  std::ostringstream os;

  for (f=0x1; f; f<<=1) {
    if (f & m) {
      os << sep << SHF_String(f);
      sep=", ";
    }
  }

  return (os.str()).c_str();
}

const char* SHT_String (INT t)
{
  switch (t) {
  case SHT_NULL:
    return "undefined";
  case SHT_PROGBITS:
    return "program";
  case SHT_SYMTAB:
    return "symbol-table";
  case SHT_STRTAB:
    return "string-table";
  case SHT_RELA:
    return "rela-relocation";
  case SHT_HASH:
    return "hash-table";
  case SHT_DYNAMIC:
    return "dynamic-linking";
  case SHT_NOTE:
    return "file-marking";
  case SHT_NOBITS:
    return "unallocated";
  case SHT_REL:
    return "rel-relocation";
  case SHT_SHLIB:
    return "reserved";
  case SHT_DYNSYM:
    return "dynamic-symbol";
  case SHT_INIT_ARRAY:
    return "init-function-pointer-array";
  case SHT_FINI_ARRAY:
    return "exit-function-pointer-array";
  case SHT_PREINIT_ARRAY:
    return "preinit-function-pointer-array";
  case SHT_GROUP:
    return "group";
  case SHT_SYMTAB_SHNDX:
    return "symtab-extended-index";
  case SHT_LOOS:
    return "os-specific-lowbound";
  case SHT_HIOS:
    return "os-specific-highbound";
  case SHT_LOPROC:
    return "proc-specific-lowbound";
  case SHT_HIPROC:
    return "proc-specific-highbound";
  case SHT_LOUSER:
    return "user-specific-lowbound";
  case SHT_HIUSER:
    return "user-specific-highbound";
  default:
    return "unknown";
  }
}

/* ====================================================================
 *    Init_Section (st)
 *
 *    Initialize a section.
 * ====================================================================
 */

static void
Init_Section (ST *st)
{
	Elf64_Word scn_type;
	Elf64_Word scn_flags;
	Elf64_Xword scn_entsize;
	char sname[32];

	if (ST_elf_index(st) != 0) {
		/* already created */
		return;
	}
	if (last_scn >= (max_scn-1)) {
		/* allocate new block of sections */
		max_scn += 30;
		if (em_scn == NULL)
			em_scn = (AUX_SCN *)Src_Alloc(sizeof(AUX_SCN)*max_scn);
		else
			em_scn = TYPE_MEM_POOL_REALLOC_N (AUX_SCN, 
				&MEM_src_pool, em_scn, (max_scn-30), max_scn);
	}
	last_scn++;
	Set_STB_scninfo_idx(st, last_scn);

	/* hack for .text section */
#ifdef TARG_ST
  // (cbr) exectutable sections should be text aligned
  // [CG] Minimal alignment of text segment is DEFAULT_TEXT_ALIGNMENT.
  if (STB_exec (st)) {
    int text_align;
    if (Align_Instructions) 
      text_align = Align_Instructions;
    else if (EMIT_space)
      text_align = DEFAULT_TEXT_ALIGNMENT;
    else 
      text_align = CGTARG_Text_Alignment();
    if (text_align < DEFAULT_TEXT_ALIGNMENT) text_align = DEFAULT_TEXT_ALIGNMENT;
    Set_STB_align(st, text_align);
  }
#else
	if (Is_Text_Section(st)) {
		if (Align_Instructions) 
			Set_STB_align(st, Align_Instructions);
		else if (OPT_Space)
			Set_STB_align(st, INST_BYTES);
		else
			Set_STB_align(st, CGTARG_Text_Alignment());
	}
#endif
#ifdef TARG_ST
  // [CG]: For data sections we may have an alignment
  // Currently set it to DEFAULT_DATA_ALIGNMENT
  else {
    if (STB_align(st) == 0) {
      /* (cbr) if the section is not used, no alignement has been set */
      if (STB_size(st))
        DevWarn("default alignment not set for section %s\n", ST_name(st));
      Set_STB_align(st, DEFAULT_DATA_ALIGNMENT);
    }
  }
#endif

	em_scn[last_scn].sym = st;	/* save symbol for later reference */

#ifdef TARG_ST100
  /* (cbr) section relative offset using labels only on st100 */

  // Make a label:
  sprintf(sname, "%s_SECTION_%d", Local_Label_Prefix, last_scn);
  em_scn[last_scn].label = strdup(sname);

  if (Trace_Init) {
    fprintf(TFile, "<init>: Section %s: labeling %s\n", 
	    ST_name(em_scn[last_scn].sym), em_scn[last_scn].label);
  }
#endif
	/* assume st is CLASS_BLOCK */
	scn_type = Get_Section_Elf_Type(STB_section_idx(st));
	scn_flags = Get_Section_Elf_Flags(STB_section_idx(st));
	if (Is_Text_Section(st) && 
		current_pu != (PU_IDX) NULL && PU_in_elf_section(current_pu)) 
	{
		scn_flags |= SHF_MIPS_NODUPE;
	}
	
	scn_entsize = Get_Section_Elf_Entsize(STB_section_idx(st));

	if (generate_elf_symbols) {
		em_scn[last_scn].scninfo = Em_New_Section (ST_name(st), 
			scn_type, scn_flags, scn_entsize, STB_align(st));

		/* initialize elf data buffer. */
		if (!STB_nobits(st)) {
			Em_New_Data_Buffer (em_scn[last_scn].scninfo, 
				STB_size(st) + 100, 1);
		}
		Set_ST_elf_index(st,
			Em_Create_Section_Symbol (em_scn[last_scn].scninfo));
	}
	else {
		/* set dummy value just so don't redo this */
		Set_ST_elf_index(st, 1);
	}

	if (Assembly) {
#if defined(TARG_MIPS) || defined(TARG_X8664)
	  CGEMIT_Prn_Scn_In_Asm(st, cur_section);
#else
	  CGEMIT_Prn_Scn_In_Asm(st, scn_type, scn_flags, scn_entsize, cur_section);
#endif
#if defined(TARG_MIPS)
	  UINT32 tmp, power;
	  power = 0;
	  for (tmp = STB_align(st); tmp > 1; tmp >>= 1) power++;
	  fprintf(Asm_File, "\t%s\t%d\n", AS_ALIGN, power);
#endif
	}
	if (MiniR_Code) {
	  sections_MINIR << "  - label: " << ST_name(st) << "\n";
	  if (scn_flags) 
	    sections_MINIR << "    flags: [ " << SHFmask_String(scn_flags) << " ]\n";
	  if (scn_type)
	    sections_MINIR << "    type: " << SHT_String(scn_type) << std::endl;
	  if (!strcmp (ST_name(st), ".eh_frame")) fprintf (MiniR_File, "\n.EHCIE:");
	  sections_MINIR << "    align: " << STB_align(st) << std::endl;  
	}
	
  return;

}

static unsigned char
st_other_for_sym (ST *sym)
{
  unsigned char symother;

  switch (ST_export(sym)) {
    case EXPORT_HIDDEN:
      symother = STO_HIDDEN;
      break;
    case EXPORT_PROTECTED:
      symother = STO_PROTECTED;
      break;
    case EXPORT_INTERNAL:
      symother = STO_INTERNAL;
      break;
    case EXPORT_OPTIONAL:
      symother = STO_OPTIONAL;
      break;
    default:
      symother = STO_DEFAULT;
      break;
  }
  return symother;
}

static INT64
Get_Offset_From_Full (ST *sym)
{
	/* full-split symbols have names of form "full_.offset"
	 * we need to extract the offset and pass that in value field.
	 * An alternative would be to search type structure for
	 * matching fields, but this is probably faster.
	 */
	char *offset_string;
        offset_string = strrchr (ST_name(sym), '.');
        FmtAssert(offset_string != NULL, ("Get_Offset_From_Full unexpected name format (%s)", ST_name(sym)));
        offset_string++;       /* skip the period */
	return atoll(offset_string);
}



static void
add_reloc_type (Elf64_AltRela *preloc, unsigned char reloc_type, pSCNINFO scn)
{
  if (REL64_type3(*preloc) != 0) {
    Elf64_Addr sv_offset = REL_offset(*preloc);
    Em_Add_New_Composite_Rela (preloc, scn);
    memset (preloc, 0, sizeof (Elf64_Rela));
    REL_offset(*preloc) = sv_offset;
    REL64_type(*preloc) = reloc_type;
  }
  else if (REL64_type2(*preloc) != 0) {
    REL64_type3(*preloc) = reloc_type;
  }
  else if (REL64_type(*preloc) != 0) {
    REL64_type2(*preloc) = reloc_type;
  }
  else {
    REL64_type(*preloc) = reloc_type;
  }
}

/***********************************************************************
 *
 * Instruction-PC utilities.
 *
 * On architectures that support bundling, we use a PC representation
 * that enables us to point at instructions within the bundle.
 * We accomplish this by using the low bits of the bundle address
 * (which are normally zero because of alignment constraints) to
 * hold the instruction offset, i.e. slot number, within the bundle.
 *
 * The following utility routines provide for updating and accessing
 * the composite PC values.
 *
 ***********************************************************************
 */


/* Given a composite PC, return the bundle address component.
 */
inline INT32 PC_Bundle(INT32 pc)
{
  return pc & ~(INST_BYTES - 1);
}

/* Given a composite PC, return the slot number component.
 */
inline INT32 PC_Slot(INT32 pc)
{
  return pc & (INST_BYTES - 1);
}

/* Increment a composite PC by 1. The slot number is incremented, 
 * and if it overflows, the bundle address is adjusted as well.
 */
inline INT32 PC_Incr(INT32 pc)
{
  ++pc;

  if (PC_Slot(pc) == ISA_MAX_SLOTS) {
    pc += INST_BYTES - ISA_MAX_SLOTS;
  }

  return pc;
}

/* Same as PC_Incr except the increment is an arbitrary constant.
 */
inline INT32 PC_Incr_N(INT32 pc, UINT32 incr)
{
  UINT slots = PC_Slot(pc) + incr;
  UINT bundles = slots / ISA_MAX_SLOTS;
  pc = PC_Bundle(pc) + (bundles * INST_BYTES) + (slots % ISA_MAX_SLOTS);
  return pc;
}

/* ====================================================================
 *    EMT_Visibility
 *
 * If the assembler can support the ELF visibilities (i.e. if
 * AS_HIDDEN et al are defined) then use them.
 *
 * ====================================================================
 */
std::string
EMT_Get_Visibility ( ST_EXPORT eclass )
{
  const char *dir;

  switch (eclass)
    {
#ifdef AS_INTERNAL
    case EXPORT_INTERNAL:
      return MiniR_Code ? "internal" : AS_INTERNAL;
#endif
#ifdef AS_HIDDEN
    case EXPORT_HIDDEN:
      return MiniR_Code ? "hidden" : AS_HIDDEN;
#endif
#ifdef AS_PROTECTED
    case EXPORT_PROTECTED:
      return MiniR_Code ? "protected" : AS_PROTECTED;
#endif
    default:
      return "";
    }
}

void EMT_Visibility (
  FILE *f,			
  std::ostringstream *pstream,
  ST_EXPORT eclass,
  ST *st
  )
{
  const char *dir;

  if (EMT_Get_Visibility(eclass).empty())
    return;

  if (Assembly) {
    fprintf (f, "\t%s\t", EMT_Get_Visibility(eclass).c_str());
    EMT_Write_Qualified_Name (f, st);
    fprintf (f, "\n");
  }
  else if (MiniR_Code) 
    *pstream << "    visibility: " << EMT_Get_Visibility(eclass) << "\n";
}



std::string
EMT_Get_Qualified_Name (ST *st)
{
  std::ostringstream name;

  if (ST_name(st) && *(ST_name(st)) != '\0') {
#if defined(BUILD_OS_DARWIN)
    name << underscorify(ST_name(st));
#else /* defined(BUILD_OS_DARWIN) */
    name << ST_name(st);
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef KEY
// This name is already unique, don't change it.
	if (!strncmp (ST_name(st), ".range_table.", strlen(".range_table.")))
		return name.str();
#endif // KEY
	// KEY bug 14894: if the symbol name has a . then it has already
	// been made unique, no need to qualify it again 
	// KEY bug 14926 shows the above should not be applied for fortran
	if (ST_is_export_local(st) && ST_class(st) == CLASS_VAR) {
		// local var, but being written out.
		// so add suffix to help .s file distinguish names.
		// assume that statics in mult. pu's will 
		// get moved to global symtab, so don't need pu-num
		if (ST_level(st) == GLOBAL_SYMTAB) {
#ifdef KEY
		  // bug 14517
		  if (Emit_Global_Data || (ST_sclass(st) == SCLASS_PSTATIC &&
		                           !strchr (ST_name(st), '.')))
#endif
            name << Label_Name_Separator << ST_index(st);
		}
		else
#ifdef KEY
		// bug 14969: do not qualify fstatic for non-ipa C
		if (!PU_c_lang(Get_Current_PU()) || // bug 14926
		    Read_Global_Data || // -ipa?
		    (ST_sclass(st) == SCLASS_PSTATIC &&
		     !strchr (ST_name(st), '.')))
#endif
            name << Label_Name_Separator << ST_pu(Scope_tab[ST_level(st)].st)
                 << Label_Name_Separator << ST_index(st);
	}
	else if (*Symbol_Name_Suffix != '\0') {
        name << Symbol_Name_Suffix;
	}
  } else {
    name << ST_name(ST_base(st)) << " " << ST_ofst(st) << SCNd64;
  }
  return name.str();
}


void
EMT_Write_Qualified_Name (FILE *f, ST *st)
{
  fputs(EMT_Get_Qualified_Name(st).c_str(), f);
}

/* ====================================================================
 *    Print_Dynsym (pfile, st)
 *
 *    print the internal, hidden or protected attributes if present 
 * ====================================================================
 */
static void Print_Dynsym (FILE *pfile, ST *st)
{
  const char *x = AS_DYNSYM;

  if (x) {
    fprintf (pfile, "\t%s\t", x);
    EMT_Write_Qualified_Name (pfile, st);
    switch (ST_export(st)) {
      case EXPORT_INTERNAL:
	fputs ("\tsto_internal\n", pfile);
	break;
      case EXPORT_HIDDEN:
	fputs ("\tsto_hidden\n", pfile);
	break;
      case EXPORT_PROTECTED:
	fputs ("\tsto_protected\n", pfile);
	break;
      case EXPORT_OPTIONAL:
	fputs ("\tsto_optional\n", pfile);
	break;
      default:
	fputs ("\tsto_default\n", pfile);
	break;
    }
  }
}

/* ====================================================================
 *    Print_Label (pfile, pstreal, st, size)
 * ====================================================================
 */
static void Print_Label (FILE *pfile, std::ostringstream *pstream, ST *st, INT64 size)
{
    ST *base_st;
    INT64 base_ofst;

    FmtAssert(!MiniR_Code || pstream!=NULL, ("Print_Label called with NULL file")); 

    if (MiniR_Code)
      *pstream << "  - label: " << EMT_Get_Qualified_Name(st) << "\n";
    if (ST_is_weak_symbol(st)) {
      if (Assembly) {
	fprintf ( pfile, "\t%s\t", AS_WEAK);
	EMT_Write_Qualified_Name(pfile, st);
	fputc ('\n', pfile);
      }	else if (MiniR_Code) {
	*pstream << "    linkage: weak\n";
      }
      EMT_Visibility (pfile, pstream, ST_export(st), st);
    }
    else if (!ST_is_export_local(st)) {
      if (Assembly) {
	fprintf ( pfile, "\t%s\t", AS_GLOBAL);
	EMT_Write_Qualified_Name(pfile, st);
	fputc ('\n', pfile);
	EMT_Visibility (pfile, NULL, ST_export(st), st);
      } else if (MiniR_Code) {
	*pstream << "    linkage: global\n";
      }
      EMT_Visibility (pfile, pstream, ST_export(st), st);
    }
#ifdef TARG_ST
    // [CL] handle the case of generation of symtab.s
    // in IPA mode. Although local, such symbols must
    // be 'promoted' to global so that they can be
    // accessed by the module they belong to
    else if (ST_is_export_local(st) && Emit_Global_Data) {
      if (Assembly) {
	fprintf (pfile, "\t%s\t", AS_GLOBAL);
	EMT_Write_Qualified_Name(pfile, st);
	fprintf(pfile, "\n");
      } else if (MiniR_Code) {
	*pstream << "    linkage: global\n";
      }
      // [CL] output as hidden/internal so that we are close to the
      // export_local meaning
#ifdef AS_INTERNAL
      if (ST_export(st) == EXPORT_LOCAL_INTERNAL) {
	if (Assembly) {
	  fprintf (pfile, "\t%s\t", AS_INTERNAL);
	  EMT_Write_Qualified_Name(pfile, st);
	  fprintf(pfile, "\n");
	} else if (MiniR_Code) 
	  *pstream << "    visibility: internal\n";
      } else
#endif
#ifdef AS_HIDDEN
	{
	  if (Assembly) {
	    fprintf (pfile, "\t%s\t", AS_HIDDEN);
	    EMT_Write_Qualified_Name(pfile, st);
	    fprintf(pfile, "\n");
	  } else if (MiniR_Code) 
	    *pstream << "    visibility: hidden\n";
	}
#endif
    }
#endif
#if 1 /* defined(BUILD_OS_DARWIN) */
	// Bug 1275 and 4351
	// Always emit the function type
	// But Mach-O as 1.38 doesn't support .type
	if (ST_class(st) == CLASS_FUNC && Assembly) {
	  fprintf ( pfile, "\t%s\t", AS_TYPE);
	  EMT_Write_Qualified_Name(pfile, st);
	  fprintf ( pfile, ", %s\n", AS_TYPE_FUNC);
	}
#endif
    if (ST_class(st) == CLASS_VAR
#if defined(BUILD_OS_DARWIN)
	&& 0 // Mach-O as 1.38 doesn't support .type
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef TARG_MIPS
	&& !CG_emit_non_gas_syntax
#endif
	&& Assembly ) {
    	fprintf (pfile, "\t%s\t", AS_TYPE);
    	EMT_Write_Qualified_Name (pfile, st);
#ifdef TARG_ST
#  ifdef AS_MOVEABLE
	BOOL moveable;

	if (Emit_Global_Data) {
	  /* TB: Tell this symbol is moveable when not in Emit_Global_Data (ipa) */
	  moveable = FALSE;
	}
	else if (ST_is_initialized (st)) {
	  /* [SC]: Not moveable if initializer is larger than symbol size */
	  INITV_IDX inito_idx = ST_has_inito (st);
	  if (inito_idx != 0 && Get_INITO_Size (inito_idx) > size)
	    moveable = FALSE;
	  else
	    moveable = TRUE;
	}
	else {
	  moveable = TRUE;
	}
	
	if (Assembly) fprintf (pfile, ", %s", AS_TYPE_OBJECT);
	if (moveable && Assembly) {
	  fprintf (pfile, ", %s", AS_MOVEABLE);
	}
#  ifdef AS_USED
	/* TB: add gnu used  attibute when needed */
	if (ST_is_used(st) && Assembly) {
	  fprintf (pfile, ", %s", AS_USED);
	}
#endif
	if (Assembly) fprintf (pfile, "\n");
#  else //AS_MOVEABLE
	if (Assembly) fprintf (pfile, ", %s\n", AS_TYPE_OBJECT);
#  endif
#else // TARG_ST
	if (Assembly) fprintf (pfile, ", %s\n", AS_TYPE_OBJECT);
#endif    
    }
#if ! defined(BUILD_OS_DARWIN)
    if (size != 0 && !CG_inhibit_size_directive) {
	/* if size is given, then emit value for asm */
      if (Assembly) {
      	fprintf ( pfile, "\t%s\t", AS_SIZE);
	EMT_Write_Qualified_Name(pfile, st);
	fprintf ( pfile, ", %" SCNd64 "\n", size);
      } else if (MiniR_Code) 
	*pstream << "    size: " << size << "\n";
    }
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef TARG_ST
    if (Base_Offset_Is_Known (st)) {
      Base_Symbol_And_Offset (st, &base_st, &base_ofst);
    } else {
      base_st = st; base_ofst = 0;
    }
#else
    Base_Symbol_And_Offset (st, &base_st, &base_ofst);
#endif
    if (Assembly) {
      EMT_Write_Qualified_Name (pfile, st);
#ifdef TARG_ST
      if (List_Notes) {
	fprintf (pfile, ":\t%s 0x%llx\n", ASM_CMNT_LINE, base_ofst);
      } else {
	fprintf (pfile, ":\n");
      }
#else
      fprintf (pfile, ":\t%s 0x%llx\n", ASM_CMNT_LINE, base_ofst);
#endif
      Print_Dynsym (pfile, st);
    }
}

/* ====================================================================
 *    Print_Common
 * ====================================================================
 */
static void
Print_Common (FILE *pfile, ST *st)
{
  ST    *base_st;
#ifdef TARG_ST
  base_st = Base_Symbol (st);
#else
  INT64  base_ofst;

  Base_Symbol_And_Offset (st, &base_st, &base_ofst);
#endif
  if (st != base_st && ST_sclass(base_st) == SCLASS_COMMON) {
	// use base common
	if (ST_elf_index(base_st) == 0) {
		Print_Common (pfile, base_st);
	}
	return;
  }
  
  if (MiniR_Code) {
    Print_Label(NULL, &objects_MINIR, st, TY_size(ST_type(st))); 
    objects_MINIR << "    align: " << TY_align(ST_type(st)) << "\n";
    //    fprintf(MiniR_File, "    section: .bss\n");
    objects_MINIR << "    attr: common\n";
    return;
  }

  if (TY_size(ST_type(st)) > 0)
    {
    if (ST_is_weak_symbol(st)) {
	fprintf ( pfile, "\t%s\t", AS_WEAK);
	EMT_Write_Qualified_Name(pfile, st);
	fputc ('\n', pfile);
    }
    EMT_Visibility (pfile, NULL, ST_export(st), st);
    fprintf ( pfile, "\t%s\t", AS_COM);
    EMT_Write_Qualified_Name(pfile, st);
#ifdef TARG_X8664
#if defined(BUILD_OS_DARWIN) /* .comm alignment arg not allowed */
    fprintf ( pfile, ", %" SCNd64 "\n", TY_size(ST_type(st)));
#else /* defined(BUILD_OS_DARWIN) */
    if (LNO_Run_Simd && Simd_Align && TY_size(ST_type(st)) >= 16)
      fprintf ( pfile, ", %" SCNd64 ", 16\n", TY_size(ST_type(st)));
    else
      fprintf ( pfile, ", %" SCNd64 ", %d\n", 
 	TY_size(ST_type(st)), TY_align(ST_type(st)));
#endif /* defined(BUILD_OS_DARWIN) */
#else
    fprintf ( pfile, ", %" SCNd64 ", %d\n", 
		TY_size(ST_type(st)), TY_align(ST_type(st)));
#endif
    Print_Dynsym (pfile, st);

#ifdef TARG_ST
#  ifdef AS_MOVEABLE
    /* TB: Tell this symbol is moveable when not in Emit_Global_Data (ipa) */
    if (!Emit_Global_Data) {
      fprintf (pfile, "\t%s\t", AS_TYPE);
      EMT_Write_Qualified_Name (pfile, st);
      fprintf (pfile, ", %s, %s\n", AS_TYPE_OBJECT, AS_MOVEABLE);
    }
#endif
#  ifdef AS_USED
    /* TB: add gnu used  attibute when needed */
    if (ST_is_used(st)) {
      fprintf (pfile, "\t%s\t", AS_TYPE);
      EMT_Write_Qualified_Name (pfile, st);
      fprintf (pfile, ", %s, %s\n", AS_TYPE_OBJECT, AS_USED);
    }
#endif
#endif
    // this is needed so that we don't emit commons more than once
    if (!generate_elf_symbols) Set_ST_elf_index(st, 1);
  }
#ifdef KEY
  else {
    // Emit symbol even though type size is 0, in order to avoid undefined
    // symbol in C++.  Bug 3739.
    //
    // For ipa's symtab.I, can't use PU_src_lang(Get_Current_PU()) to check for
    // C++ because the PU isn't defined.  So, add the 1 byte for all languages.
    // Bug 3923.
    fprintf ( pfile, "\t%s\t", AS_COM);
    EMT_Write_Qualified_Name(pfile, st);
#if defined(BUILD_OS_DARWIN) /* .comm alignment arg not allowed */
    fputs (", 1\n", pfile);
#else /* defined(BUILD_OS_DARWIN) */
    fputs (", 1, 1\n", pfile);
#endif /* defined(BUILD_OS_DARWIN) */
  }
#endif
}


/* ====================================================================
 *
 *EMT_Put_Elf_Symbol
 *
 * Add a symbol to the ELF symbol table if it hasn't been added already.
 * ====================================================================
 */

#if defined(BUILD_OS_DARWIN)
/*
 * sym			symbol to add
 * which		(optional) if present and not DO_UNDERSCORE, then this
 *			symbol must be accessed indirectly to permit dynamic
 *			linking on Darwin
 */
mINT32 EMT_Put_Elf_Symbol (ST *sym, darwin_indirect_t which)
#else /* defined(BUILD_OS_DARWIN) */
mINT32 EMT_Put_Elf_Symbol (ST *sym)
#endif /* defined(BUILD_OS_DARWIN) */
{
  unsigned char symbind;
  unsigned char symother;
  Elf64_Word symindex;
  TY_IDX sym_type;
  ST *base_st;
  INT64 base_ofst = 0;
  ST_SCLASS sclass;

  symindex = ST_elf_index(sym);

  /* check if symbol is already there. */
#ifndef KEY 
  if (symindex != 0) return symindex;
#else
  if (symindex != 0 && 
      (!LNO_Run_Simd || !Simd_Align))
    return symindex;
#endif

  if ( Trace_Elf ) {
    #pragma mips_frequency_hint NEVER
    fprintf ( TFile, "EMT_Put_Elf_Symbol:\n" );
    Print_ST ( TFile, sym, FALSE );
  }
  if ( ! generate_elf_symbols) {
    // if only .s file, then just do dummy mark that we have
    // seen this symbol and emitted any type info for it.
    if (ST_class(sym) == CLASS_FUNC && !MiniR_Code
#if defined(BUILD_OS_DARWIN)
	&& 0 // Mach-O as 1.38 doesn't support .type
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef TARG_MIPS
	&& !CG_emit_non_gas_syntax
#endif
	) {
#ifdef TARG_ST
      // (cbr)
      /***** FUNCTION  *******/
      if (ST_sclass(sym) == SCLASS_EXTERN) 
	{
	  if (ST_is_weak_symbol(sym)) {
	    fprintf ( Asm_File, "\t%s\t", AS_WEAK);
	    EMT_Write_Qualified_Name(Asm_File, sym);
	    fprintf ( Asm_File, "\n");
	  }
	  else 
	    {
	      fprintf(Asm_File, "\t%s\t", AS_GLOBAL);
	      EMT_Write_Qualified_Name(Asm_File, sym);
	      fprintf ( Asm_File, "\n");
	    }
	  // clarkes 090307
	  EMT_Visibility (Asm_File, NULL, ST_export(sym), sym);
	}
#endif
	fprintf (Asm_File, "\t%s\t", AS_TYPE);
	EMT_Write_Qualified_Name (Asm_File, sym);
	fprintf (Asm_File, ", %s\n", AS_TYPE_FUNC);
      }
    else if (ST_class(sym) == CLASS_VAR && 
	     ST_sclass(sym) == SCLASS_COMMON) {
      /********* COMMON VAR *********/
      if (Assembly || Lai_Code || MiniR_Code) {
	Print_Common (MiniR_Code ? Asm_File : MiniR_File, sym);
      }
    }
#ifdef TARG_ST
    // (cbr) need to emit .weak for extern symbols as well
    /********** EXTERN VAR ********/
    else if (ST_class(sym) == CLASS_VAR &&
	     ST_sclass(sym) == SCLASS_EXTERN) {
      if (Assembly || Lai_Code || MiniR_Code) {
        if (ST_is_weak_symbol(sym)) {
          fprintf ( Asm_File, "\t%s\t", AS_WEAK);
          EMT_Write_Qualified_Name( Asm_File, sym);
          fprintf ( Asm_File, "\n");
        }
	else {
	  fprintf(Asm_File, "\t%s\t", AS_GLOBAL);
	  EMT_Write_Qualified_Name(Asm_File, sym);
	  fprintf ( Asm_File, "\n");
	}
	// clarkes 090307
	EMT_Visibility ( Asm_File, NULL, ST_export(sym), sym);
      }
    }
#endif
    Set_ST_elf_index(sym, 1);
    return 0;
  }
  #ifdef KEY 
  #ifdef Is_True_On
    if (! profile_arcs || strncmp(ST_name(sym), "LPBX", 4) != 0)
  #endif
  #endif
  Is_True (!ST_is_not_used(sym) || ST_emit_symbol(sym), 
	      ("Reference to not_used symbol (%s)", ST_name(sym)));

  /* set the symbol binding. */
  if (ST_is_weak_symbol(sym)) {
    symbind = STB_WEAK;
  }
  else if (ST_is_export_local(sym)) {
    symbind = STB_LOCAL;
  }
  else {
    symbind = STB_GLOBAL;
  }

  symother = st_other_for_sym (sym);
#ifdef TARG_ST
  base_st = Base_Symbol (sym);
  if (Base_Offset_Is_Known (sym)) {
    base_ofst = Base_Offset (sym);
  }
#else  
  Base_Symbol_And_Offset (sym, &base_st, &base_ofst);
#endif
  // check if base is new section symbol that is not initialized yet
  if (ST_class(base_st) == CLASS_BLOCK && STB_section(base_st)
	&& ST_elf_index(base_st) == 0)
  {
	Init_Section(base_st);
  }

  if (ST_is_weak_symbol(sym) && ST_sclass(base_st) == SCLASS_EXTERN) {
	// ipa can cause it to be based on global extern,
	// in which case treat it as an extern
	sclass = ST_sclass(base_st);
  }
  else {
	sclass = ST_sclass(sym);
  }
  switch (ST_class(sym)) {
    case CLASS_VAR:
      sym_type = ST_type(sym);	// only valid for VARs
      switch (sclass) {
	case SCLASS_FSTATIC:
	case SCLASS_DGLOBAL:
	case SCLASS_UGLOBAL:
#if defined (TARG_X8664) || defined (TARG_ST200) || defined (KEY)
	case SCLASS_EH_REGION:
#endif // TARG_X8664
#ifdef TARG_ST
        case SCLASS_PSTATIC:
	      // [CL] take into account the right renaming of static variables.
              // Symbols with no base are not emitted: as they have not been
              // allocated, it means they are not used.
	  if (Has_Base_Block(sym)) {
	      FmtAssert(ST_name(sym) && *(ST_name(sym)) != '\0', ("%s: unexpected symbol with no name", __FUNCTION__));
	      vstring buf = vstr_begin(LBUF_LEN);
	      r_qualified_name (sym, &buf);

	      symindex = Em_Add_New_Symbol (
		  vstr_str(buf), base_ofst, TY_size(sym_type), 
		  symbind,
		  (ST_is_thread_private(sym) ? STT_TLS : STT_OBJECT),
		  symother,
		  Em_Get_Section_Index (em_scn[STB_scninfo_idx(base_st)].scninfo));

	      vstr_end(buf);
	  }	
#else
	  symindex = Em_Add_New_Symbol (
			ST_name(sym), base_ofst, TY_size(sym_type), 
			symbind, STT_OBJECT, symother,
			Em_Get_Section_Index (em_scn[STB_scninfo_idx(base_st)].scninfo));
#endif
	  break;
	case SCLASS_EXTERN:
#ifdef TARG_ST
          symindex = Em_Add_New_Symbol (
                      ST_name(sym), 0, TY_size(sym_type),
                      symbind,
                      ST_is_thread_private(sym) ? STT_TLS : STT_OBJECT,
                      symother,
                      ST_is_gp_relative(sym) ? SHN_MIPS_SUNDEFINED : SHN_UNDEF);
#else
	  symindex = Em_Add_New_Symbol (
		      ST_name(sym), 0, TY_size(sym_type),
		      symbind, STT_OBJECT, symother,
		      ST_is_gp_relative(sym) ? SHN_MIPS_SUNDEFINED : SHN_UNDEF);
#endif
	  if (Assembly || MiniR_Code) {
	    if (ST_is_weak_symbol(sym)) {
	      fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", AS_WEAK);
	      EMT_Write_Qualified_Name( MiniR_Code ? MiniR_File : Asm_File, sym);
	      fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\n");
	    }
	    else {
#ifdef TARG_ST
	      fprintf(MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", AS_GLOBAL);
	      EMT_Write_Qualified_Name(MiniR_Code ? MiniR_File : Asm_File, sym);
	      fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\n");
#else
#if defined(BUILD_OS_DARWIN)
	      /* Indirect calls use jump table instead */
	      if (DO_UNDERSCORE != which) {
		fprintf(Asm_File, "\t%s\t%s\n", AS_GLOBAL,
		  underscorify(ST_name(sym)));
	      }
#else /* defined(BUILD_OS_DARWIN) */
	      fprintf(Asm_File, "\t%s\t%s\n", AS_GLOBAL, ST_name(sym));
#endif /* defined(BUILD_OS_DARWIN) */
#endif //TARG_ST
	    }
#ifdef TARG_ST
	    EMT_Visibility ( Asm_File, NULL,  ST_export(sym), sym);
#endif
	  }
	  break;
	case SCLASS_COMMON:
	  if (sym != base_st && ST_sclass(base_st) == SCLASS_COMMON) {
		// use base common
		return EMT_Put_Elf_Symbol (base_st);
	  }
	  if (Assembly || MiniR_Code) {
	    Print_Common (MiniR_Code ? MiniR_File: Asm_File, sym);
	  }
	  if (generate_elf_symbols) {
	    if (ST_is_split_common(sym)) {
		symbind = STB_SPLIT_COMMON;
		symother = STO_SC_ALIGN_UNUSED;
	  	symindex = Em_Add_New_Symbol (
			ST_name(sym), Get_Offset_From_Full(sym), TY_size(sym_type),
			symbind, STT_OBJECT, symother,
			EMT_Put_Elf_Symbol (ST_full(sym)) );
	    }
	    else {
		Elf64_Half symshndx;	/* sym section index */
		if (ST_is_thread_private(sym)) symshndx = SHN_MIPS_LCOMMON;
		else if (ST_is_gp_relative(sym)) symshndx = SHN_MIPS_SCOMMON;
		else symshndx = SHN_COMMON;
	  	symindex = Em_Add_New_Symbol (
			ST_name(sym), TY_align(sym_type), TY_size(sym_type),
			symbind, STT_OBJECT, symother, symshndx);
	    }
	  }
	  break;
	case SCLASS_UNKNOWN:
	default:
	  break;
      }
      break;

    case CLASS_NAME:
      if (ST_emit_symbol(sym)) {
	/* emit it even though it's an unknown local (C++) */
	symindex = Em_Add_New_Symbol (
		      ST_name(sym), 0, 0,
		      STB_LOCAL, STT_NOTYPE, symother, SHN_UNDEF);

      }
      break;

    case CLASS_FUNC:
      if (sclass == SCLASS_EXTERN) {
        symindex = Em_Add_New_Undef_Symbol (
			ST_name(sym), symbind, STT_FUNC, symother);
	if (Assembly || MiniR_Code) {
	  if (ST_is_weak_symbol(sym)) {
	    fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", AS_WEAK);
	    EMT_Write_Qualified_Name(MiniR_Code ? MiniR_File : Asm_File, sym);
	    fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\n");
	  }
	  else {
#ifdef TARG_ST
	    fprintf(MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", AS_GLOBAL);
	    EMT_Write_Qualified_Name(MiniR_Code ? MiniR_File : Asm_File, sym);
	    fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\n");
#else
#if defined(BUILD_OS_DARWIN)
	    /* Indirect calls use jump table instead */
	    if (DO_UNDERSCORE != which) {
	      fprintf(Asm_File, "\t%s\t%s\n", AS_GLOBAL,
		underscorify(ST_name(sym)));
	    }
#else /* defined(BUILD_OS_DARWIN) */
	    fprintf(Asm_File, "\t%s\t%s\n", AS_GLOBAL, ST_name(sym));
#endif /* defined(BUILD_OS_DARWIN) */
#endif // TARG_ST
	  }
#ifdef TARG_ST
	  EMT_Visibility (Asm_File, NULL, ST_export(sym), sym);
#endif
	}
      }
      else 
	symindex = Em_Add_New_Symbol (
			ST_name(sym), base_ofst, 0,
			symbind, STT_FUNC, symother,
			Em_Get_Section_Index (em_scn[STB_scninfo_idx(base_st)].scninfo));
#ifdef TARG_ST
      if (Assembly) {
	      // [CL] be consistent with the case when we
	      // don't generate elf symbols above
	  fprintf (Asm_File, "\t%s\t", AS_TYPE);
	  EMT_Write_Qualified_Name (Asm_File, sym);
	  fprintf (Asm_File, ", %s\n", AS_TYPE_FUNC);
      }
	
#endif
      break;

    case CLASS_BLOCK:
      if (STB_section(sym)) {
	Init_Section(sym);
	return ST_elf_index(sym);
      }
      // may be global binding (IPA global extern symbols)
      symindex = Em_Add_New_Undef_Symbol (
      				ST_name(sym), symbind, STT_OBJECT, symother);
      break;
    case CLASS_UNK:
    case CLASS_CONST:
    default:
      symindex = Em_Add_New_Undef_Symbol (
      				ST_name(sym), STB_LOCAL, STT_OBJECT, symother);
      break;
  }
  Set_ST_elf_index(sym, symindex);
  return symindex;
}

extern void
EMT_Change_Symbol_To_Undefined (ST *sym)
{
  Elf64_Word symindex = ST_elf_index(sym);
  if (symindex == 0) 
	/* if not emitted yet, can wait till somebody refers to it */
	return;
  Em_Undefine_Symbol (symindex);
}

extern void
EMT_Change_Symbol_To_Weak (ST *sym)
{
  Elf64_Word symindex = ST_elf_index(sym);
  if (symindex == 0) 
	/* if not emitted yet, can wait till somebody refers to it */
	return;
  Em_Set_Symbol_Binding (symindex, STB_WEAK);
}


// if TN is symbol TN, add info about it to comment buffer
static void
put_TN_comment (TN *t, BOOL add_name, vstring *comment)
{
  if (!add_name) return;	// don't duplicate name
  if ( ! TN_is_constant(t) ) return;
  INT64 val = TN_offset(t);
  if ( TN_is_symbol(t) ) {
	if (ST_class(TN_var(t)) == CLASS_CONST) {
		*comment = vstr_concat (*comment, 
			Targ_Print(NULL, ST_tcon_val(TN_var(t))) );
	}
	else {
		*comment = vstr_concat (*comment, ST_name(TN_var(t)));
	}
	if (TN_offset(t) != 0) {
		vstr_sprintf (comment, vstr_len(*comment), "%+lld", val);
	}
  }
  else if ( TN_is_label(t) && val != 0) {
	*comment = vstr_concat (*comment, LABEL_name(TN_label(t)));
	vstr_sprintf (comment, vstr_len(*comment), "%+lld", val); 
  }
}

/* ====================================================================
 *
 * r_apply_l_const
 *
 * Format the given constant TN's "value" into the given buffer.
 * Return whether the symbol name should be added to comments.
 *
 * ====================================================================
 */

static BOOL
r_apply_l_const (
  OP *op,		/* OP with constant operand */
  INT opidx,		/* OP index of constant TN of some sort */
  vstring *buf)		/* A buffer to format it into */
{
  TN *t = OP_opnd(op, opidx);
  INT paren = 0;
  BOOL hexfmt = FALSE;
  BOOL print_TN_offset = TRUE;
  BOOL add_name = FALSE;
  ST *st;
  INT64 val;

  /* special case for stack symbols */
  if ( TN_is_symbol(t) ) {
    ST *base_st;
    INT64 base_ofst;

    st = TN_var(t);
    Base_Symbol_And_Offset (st, &base_st, &base_ofst);
    if (base_st == SP_Sym || base_st == FP_Sym) {
      val = base_ofst + TN_offset(t);

      if ( TN_is_reloc_neg(t) ) {
	val = -val;
      }
      if ( TN_is_reloc_low16(t) ) {
	val = val & 0xffff;
	hexfmt = TRUE;
      } else if ( TN_is_reloc_high16(t) ) {
	val = ( ( val - (short)val ) >> 16) & 0xffff;
	hexfmt = TRUE;
      } else if ( TN_is_reloc_higher(t) ) {
	val = ( ( val + 0x80008000LL ) >> 32 ) & 0xffff;
	hexfmt = TRUE;
      } else if ( TN_is_reloc_highest(t) ) {
	val = ( ( val + 0x800080008000LL ) >> 48 ) & 0xffff;
	hexfmt = TRUE;
      }
#ifndef KEY
      vstr_sprintf (buf, vstr_len(*buf), (hexfmt ? "0x%" SCNx64 "" : "%" SCNd64 ""), val );
#else
      if ( TN_is_reloc_low16(t) )
	vstr_sprintf (buf, vstr_len(*buf), "%hd", (signed short)val );
      else {
	vstr_sprintf (buf, vstr_len(*buf), (hexfmt ? "0x%" SCNx64 "" : "%" SCNd64 ""), val );
      }
#endif
      return TRUE;
    }
  }
  val = TN_offset(t);

  if ( TN_is_reloc_neg(t) ) {
    *buf = vstr_concat (*buf, "-");
  }

  if ( TN_is_symbol(t) ) {
#if defined(BUILD_OS_DARWIN)
    darwin_indirect_t indirect = DO_UNDERSCORE;
#endif /* defined(BUILD_OS_DARWIN) */
    st = TN_var(t);
    // call put_symbol so that we emit .type info, once per symbol
    if( (ST_class(st) != CLASS_CONST)
#ifdef TARG_X8664
	&& ( TN_relocs(t) != TN_RELOC_IA32_GLOBAL_OFFSET_TABLE )
#endif
	){
#if defined(BUILD_OS_DARWIN)
      /* Darwin dynamic linking requires that we emit an indirect call
       * through a jump table. System and C library functions are supported
       * only via dynamic linking. Since we can't know whether a function
       * outside this compilation is available via static linking, we must
       * assume that it might require dynamic linking. */
      if (Is_Target_32bit() && ST_class(st) == CLASS_FUNC &&
        ST_sclass(st) == SCLASS_EXTERN) {
	if (OP_code(op) == TOP_call) {
	  indirect = DO_STUB;
	}
	/* Need other instructions as well? */
	else if (OP_code(op) == TOP_ldc32) {
	  indirect = DO_NON_LAZY_PTR;
	  op->opr = TOP_ld32_n32;
	}
      }
      (void) EMT_Put_Elf_Symbol (st, indirect);
      gen_indirect(EMT_Get_Qualified_Name(st).c_str(), indirect);
#else /* defined(BUILD_OS_DARWIN) */
      (void) EMT_Put_Elf_Symbol (st);
#endif /* defined(BUILD_OS_DARWIN) */
    }
    if (TN_relocs(t) != 0) {
	// use base if referring to current pu or local data
	if (CGEMIT_Use_Base_ST_For_Reloc (TN_relocs(t), st)) {
		ST *base_st;
		INT64 base_ofst;
		Base_Symbol_And_Offset (st, &base_st, &base_ofst);
		val += base_ofst;
		st = base_st;
    	}
    	if (Use_Separate_PU_Section(current_pu,st)) {
		/* use PU text section rather than generic one */
		st = PU_base;
    	}
	hexfmt = TRUE;
	print_TN_offset = TRUE;
	// add name to comments in case was confused by gp_disp.
	add_name = TRUE;
	paren = CGEMIT_Relocs_In_Asm (t, st, buf, &val);
    } 
    else {
#if defined(BUILD_OS_DARWIN)
	*buf = vstr_concat(*buf, underscorify(
                                 EMT_Get_Qualified_Name(st).c_str(), indirect));
#else /* defined(BUILD_OS_DARWIN) */
#ifdef TARG_X8664
	if (ST_sym_class(st) == CLASS_CONST) {
	  char name[32];
	  sprintf (name, TCON_Label_Format, ST_IDX_index(ST_st_idx(st)));
	  *buf = vstr_concat(*buf, name);
	}
	else
#endif
	*buf = vstr_concat(*buf, EMT_Get_Qualified_Name(st).c_str());
#endif /* defined(BUILD_OS_DARWIN) */
	if (*Symbol_Name_Suffix != '\0')
		*buf = vstr_concat(*buf, Symbol_Name_Suffix);
    }
  }
  else if ( TN_is_label(t) ) {
    if (val != 0) {
	// don't use "." cause that can have varying meaning
	// when have multiple instruction slots.
	// Instead just do label+offset.
	*buf = vstr_concat (*buf, LABEL_name(TN_label(t)));
	vstr_sprintf (buf, vstr_len(*buf), "%+lld", val); 
    }
    else {
      *buf = vstr_concat(*buf, LABEL_name(TN_label(t)));
      if (isdigit(LABEL_name(TN_label(t))[0])) {
      	*buf = vstr_concat(*buf, (PC > Get_Label_Offset(TN_label(t))) ? "b" : "f");
      }
    }
    print_TN_offset = FALSE;
  }
  else if (TN_is_tag(t)) {
	*buf = vstr_concat(*buf, LABEL_name(TN_label(t)));
    	print_TN_offset = FALSE;
  }
  else if (TN_is_enum(t)) {
    if (ISA_PRINT_Operand_Is_Part_Of_Name(OP_code(op), opidx)) {
      vstr_sprintf (buf, vstr_len(*buf), "%s", ISA_ECV_Name(TN_enum(t)) );
    } else {
      vstr_sprintf (buf, vstr_len(*buf), "%d", ISA_ECV_Intval(TN_enum(t)) );
    }
    print_TN_offset = FALSE;	/* because value used instead */
  }
  else if ( TN_has_value(t) ) {
#ifdef TARG_X8664
    vstr_sprintf (buf, vstr_len(*buf),  (hexfmt ? "0x%" SCNx64 "" : "%" SCNd64 ""), TN_value(t) );
#else
    if ( TN_size(t) <= 4 )
      vstr_sprintf (buf, vstr_len(*buf), 
		(hexfmt ? "0x%x" : "%d"), (mINT32)TN_value(t) );
    else
      vstr_sprintf (buf, vstr_len(*buf),  (hexfmt ? "0x%" SCNx64 "" : "%" SCNd64 ""), TN_value(t) );
#endif /* TARG_X8664 */

    print_TN_offset = FALSE;	/* because value used instead */
  }
  else {
    #pragma mips_frequency_hint NEVER
    FmtAssert (FALSE, ("r_apply_l_const: illegal constant TN"));
  }

  if (print_TN_offset && (val != 0)) {
      vstr_sprintf (buf, vstr_len(*buf), "%+lld", val );
  }

  while ( paren > 0 ) {
    *buf = vstr_concat(*buf, ")");
    --paren;
  }
  return add_name;
}

/* ====================================================================
 * print_prefetch_info(char *comment, WN *wn)
 *   add prefetch info comments to the assembler listing.
 *
 * ====================================================================
 */
static void print_prefetch_info(
  vstring *comment, 
  WN *wn)
{

  if ( !wn ) return;

  if ( WN_pf_stride_1L( wn ) != 0 ) {
    *comment = vstr_concat(*comment, "L1");
    return;
  }

  if ( WN_pf_stride_2L( wn ) != 0  ) {
    *comment = vstr_concat(*comment, "L2");
  }
}


/* ====================================================================
 *
 * r_assemble_list
 *
 * Emit a pseudo-assembly listing for the given OP.
 *
 * ====================================================================
 */

static void r_assemble_list (
  OP *op,		/* The operation being listed */
  BB *bb)
{
  const char *result[ISA_OPERAND_max_results];
  const char *opnd[ISA_OPERAND_max_operands];
  vstring buf = vstr_begin(LBUF_LEN);
  INT i;
  INT lc = 0;
  BOOL add_name = FALSE;

  Emit_Unwind_Directives_For_OP(op, Asm_File);

#ifdef GAS_TAGS_WORKED
// un-ifdef this when gas can handle tags inside explicit bundle
  if (OP_has_tag(op)) {
	fprintf(Asm_File, "%s:\n", LABEL_name(Get_OP_Tag(op)));
  }
#endif
  for (i = 0; i < OP_opnds(op); i++) {
    INT start = vstr_len(buf);	// start of operand string
    TN *t = OP_opnd(op,i);

    if ( TN_is_constant(t) ) {
      add_name |= r_apply_l_const (op, i, &buf);
    }
    else {
      const char *rname;
      ISA_REGISTER_CLASS rc = TN_register_class(t);
      REGISTER reg = TN_register(t);
#ifdef HAS_STACKED_REGISTERS
      if (ABI_PROPERTY_Is_stacked(rc, REGISTER_machine_id(rc, reg))
	&& ! BB_rotating_kernel(bb)	// in case rot-regs overlap output regs
	&& REGISTER_Is_Stacked_Output(rc, reg) )
      {
	reg = REGISTER_Translate_Stacked_Output(reg);
      }
#endif
      FmtAssert (reg != REGISTER_UNDEFINED, 
		 ("r_assemble_list: illegal operand tn%d", TN_number(t)));

      if (!TN_is_true_pred(t)) {
	ISA_REGISTER_SUBCLASS sc = OP_opnd_reg_subclass(op,i);
	if (   REGISTER_SET_MemberP(REGISTER_SUBCLASS_members(sc), reg)
	    && REGISTER_SUBCLASS_reg_name(sc, reg))
	{
	  rname = REGISTER_SUBCLASS_reg_name(sc, reg);
	} else if (List_Software_Names) {
	  rname = ABI_PROPERTY_Reg_Name(rc, REGISTER_machine_id(rc, reg));
	} else {
	  rname = REGISTER_name(rc, reg);
	}
	if (i == OP_PREDICATE_OPND && OP_has_predicate(op)) {
	  vstr_sprintf(&buf, start, ISA_PRINT_PREDICATE, rname);
	} else {
	  buf = vstr_concat(buf, rname);
	}
      }
    }
    // need end-of-string between each operand
    buf = vstr_append(buf, '\0');
    opnd[i] = vstr_str(buf)+start;
  }

  for (i = 0; i < OP_results(op); i++) {
    const char *rname;
    INT start = vstr_len(buf);	// start of operand string
    TN *t = OP_result(op,i);
    ISA_REGISTER_SUBCLASS sc = OP_result_reg_subclass(op,i);
    ISA_REGISTER_CLASS rc = TN_register_class(t);
    REGISTER reg = TN_register(t);
    FmtAssert (t != NULL && !TN_is_constant(t),
	       ("r_assemble_list: illegal result tn"));
#ifdef HAS_STACKED_REGISTERS
    if (ABI_PROPERTY_Is_stacked(rc, REGISTER_machine_id(rc, reg))
	&& ! BB_rotating_kernel(bb)	// in case rot-regs overlap output regs
	&& REGISTER_Is_Stacked_Output(rc, reg) )
    {
	reg = REGISTER_Translate_Stacked_Output(reg);
    }
#endif
    FmtAssert (reg != REGISTER_UNDEFINED, 
	       ("r_assemble_list: illegal result tn%d", TN_number(t)));
    if (   REGISTER_SET_MemberP(REGISTER_SUBCLASS_members(sc), reg)
	&& REGISTER_SUBCLASS_reg_name(sc, reg))
    {
      rname = REGISTER_SUBCLASS_reg_name(sc, reg);
    } else if (List_Software_Names) {
      rname = ABI_PROPERTY_Reg_Name(rc, REGISTER_machine_id(rc, reg));
    } else {
      rname = REGISTER_name(rc, reg);
    }
    buf = vstr_concat(buf, rname);
    buf = vstr_append(buf, '\0');	// increment vstr length
    result[i] = vstr_str(buf)+start;
  }

  fputc ('\t', Asm_File);
#ifdef KEY
  lc = CGEMIT_Print_Inst( op, result, opnd, Asm_File );
#else
  lc = TI_ASM_Print_Inst( OP_code(op), result, opnd, Asm_File );
#endif
  FmtAssert (lc != TI_RC_ERROR, ("%s", TI_errmsg));
  vstr_end(buf);

  if (OP_end_group(op)) lc += fprintf(Asm_File, " %s", ISA_PRINT_END_GROUP);

  while (lc < 30) {
    fputc (' ', Asm_File);
    lc++;
  }

  vstring comment = vstr_begin(LBUF_LEN);
  for (i = 0; i < OP_opnds(op); i++) {
	put_TN_comment (OP_opnd(op,i), add_name, &comment);
  }

  fprintf (Asm_File, "\t%s", ASM_CMNT);
  if (OP_scycle(op) >= 0)
    if (BB_rotating_kernel(bb)) {
      ANNOTATION *annot = ANNOT_Get(BB_annotations(bb), ANNOT_ROTATING_KERNEL);
      ROTATING_KERNEL_INFO *info = ANNOT_rotating_kernel(annot);
      INT ii = info->ii;
      fprintf (Asm_File, " [%d*II+%d]", OP_scycle(op) / ii, OP_scycle(op) % ii);
    }
    else if (BB_scheduled(bb)) 
      fprintf (Asm_File, " [%d]", OP_scycle(op));
  if (vstr_len(comment) == 0) {
    WN *wn = Get_WN_From_Memory_OP (op);
    if (wn && Alias_Manager) {
      char tbuf[LBUF_LEN];
      tbuf[0] = '\0';
      Print_alias_info (tbuf, Alias_Manager, wn);
      comment = vstr_concat(comment, tbuf);
    }
    if (wn && OP_prefetch(op) && vstr_len(comment) == 0) {
      print_prefetch_info(&comment,wn);
    }
  }

  /* Add target PU name as comment for normal call and tail call OPs.
   */
  if ((OP_call(op) || OP_tail_call(op)) && vstr_len(comment) == 0) {
    ANNOTATION *ant = ANNOT_Get (BB_annotations(bb), ANNOT_CALLINFO);
    if (ant != NULL) {
	ST *call_sym = CALLINFO_call_st(ANNOT_callinfo(ant));
	if (call_sym != NULL) {
		comment = vstr_concat(comment, ST_name(call_sym));
	}
    }
  }
  fprintf (Asm_File, "  %s\n", vstr_str(comment));
  vstr_end(comment);
}

/* ====================================================================
 *
 * Verify_Operand
 *
 * Verify that the specified operand is valid for the particular
 * instruction operand.
 *
 * ====================================================================
 */
static void Verify_Operand(
  const ISA_OPERAND_INFO *oinfo,
  OP *op, 
  INT opnd, 
  BOOL is_result)
{
  const ISA_OPERAND_VALTYP *vtype =   is_result 
				    ? ISA_OPERAND_INFO_Result(oinfo, opnd)
				    : ISA_OPERAND_INFO_Operand(oinfo, opnd);
  TN *tn = is_result ? OP_result(op, opnd) : OP_opnd(op, opnd);
  const char * const res_or_opnd = is_result ? "result" : "operand";

  if (ISA_OPERAND_VALTYP_Is_Register(vtype)) {
    ISA_REGISTER_SUBCLASS sc = ISA_OPERAND_VALTYP_Register_Subclass(vtype);
    ISA_REGISTER_CLASS rc = ISA_OPERAND_VALTYP_Register_Class(vtype);
    REGISTER reg = TN_register(tn);

    FmtAssert(TN_is_register(tn),
	      ("%s %d is not a register", res_or_opnd, opnd));

#ifdef TARG_X8664
    if( tn != Rip_TN() )
#endif
    FmtAssert(TN_register_class(tn) == rc, ("incorrect register class for %s %d %d", res_or_opnd, opnd, op->opr));

    FmtAssert(reg != REGISTER_UNDEFINED,
	      ("undefined register for %s %d", res_or_opnd, opnd));

    REGISTER_SET class_regs = (sc == ISA_REGISTER_SUBCLASS_UNDEFINED)
      ? REGISTER_CLASS_universe(rc)
      : REGISTER_SUBCLASS_members(sc);

    FmtAssert(REGISTER_SET_MemberP(class_regs, reg),
	      ("incorrect register for %s %d", res_or_opnd, opnd));

  } else if (ISA_OPERAND_VALTYP_Is_Literal(vtype)) {
    FmtAssert(TN_is_constant(tn),
	     ("%s %d is not a constant", res_or_opnd, opnd));

    if (TN_has_value(tn)) {
      ISA_LIT_CLASS lc = ISA_OPERAND_VALTYP_Literal_Class(vtype);
      INT64 imm = TN_value(tn);

      if ((TFile != stdout) && !ISA_LC_Value_In_Class(imm, lc)) {
        Print_OP_No_SrcLine (op);
      }
#ifdef TARG_X8664
      if( TN_size(tn) == 4 ){
        imm = (INT32) imm;
      }
#endif /* TARG_X8664 */
      FmtAssert(ISA_LC_Value_In_Class(imm, lc),
	        ("literal for %s %d is not in range", res_or_opnd, opnd));
    } else if (TN_is_label(tn)) {
#if Is_True_On
      LABEL_IDX lab = TN_label(tn);
      INT64 offset = TN_offset(tn);
      INT64 val = Get_Label_Offset(lab) + offset;
      if (CG_opt_level > 0 && CFLOW_Enable && !OP_likely(op)) {
	INT nextpc = PC + sizeof(ISA_BUNDLE);
	if (PROC_has_branch_delay_slot()) nextpc += sizeof(ISA_PACK_INST);

	if (val == nextpc) {
	  DevWarn("branch to next instruction at PC=0x%x", PC);
	}
      }
#ifdef KEY
      // If OP was created by Assemble_Simulated_OP, it might not have a valid
      // OP_bb because OP_bb is not needed this late in CG.
      if (OP_bb(op) != NULL)
#endif
      if (Get_Label_Offset(lab) == 0 && offset == 0) {
	BBLIST *item;
	BB *label_bb = Get_Label_BB(lab);
	BOOL okay = FALSE;
	FOR_ALL_BB_SUCCS(OP_bb(op), item) {
	  if (BBLIST_item(item) == label_bb) okay = TRUE;
	}
	if (!okay) {
	  DevWarn("branch to 0? (possible bug at PC=0x%x, label %s)", 
		   PC, LABEL_name(lab));
	}
      }
#endif
    }
  } else if (ISA_OPERAND_VALTYP_Is_Enum(vtype)) {
    ISA_ENUM_CLASS_VALUE ecv;
    ISA_ENUM_CLASS ec = ISA_OPERAND_VALTYP_Enum_Class(vtype);
    ISA_ENUM_CLASS_VALUE first_ecv = ISA_EC_First_Value(ec);
    ISA_ENUM_CLASS_VALUE last_ecv = ISA_EC_Last_Value(ec);

    FmtAssert(TN_is_enum(tn),
	      ("%s %d is not an enum", res_or_opnd, opnd));

    ecv = TN_enum(tn);
    FmtAssert(ecv >= first_ecv && ecv <= last_ecv,
	      ("enum for %s %d is not in range", res_or_opnd, opnd));
  } else {
    FmtAssert(FALSE, ("unhandled vtype in Verify_Operand"));
  }
}

/* ====================================================================
 *
 * Verify_Instruction
 *
 * Verify that the specified OP contains valid information for
 * the instruction it represents.
 *
 * ====================================================================
 */
static void Verify_Instruction(OP *op)
{
  INT i;
  const ISA_OPERAND_INFO *oinfo;
  TOP top = OP_code(op);

  // ??? check for valid topcode?

  FmtAssert(ISA_SUBSET_Member(ISA_SUBSET_Value, top),
	    ("%s is a member of ISA %s", 
	     TOP_Name(top), 
	     ISA_SUBSET_Name(ISA_SUBSET_Value)));

#ifdef TARG_X8664
  if( OP_x86_style( op ) ){
    if( TN_register_and_class( OP_opnd( op, 0 ) ) !=
	TN_register_and_class( OP_result( op , 0 ) ) )
      FmtAssert( false, ("Result and the first opnd use different register.") );
  }

  if( OP_reads_rflags( op ) ){
    OP* prev = OP_prev( op );
    while( prev != NULL ){
#ifdef TARG_X8664
      if( TOP_is_change_rflags( OP_code(prev)) 
                ||OP_code(prev) == TOP_ptest
				||OP_code(prev) == TOP_pcmpestri 
				||OP_code(prev) == TOP_pcmpestrm
				||OP_code(prev) == TOP_pcmpistri 
				||OP_code(prev) == TOP_pcmpistrm)
#else
      if( TOP_is_change_rflags( OP_code(prev) ) )
#endif
	break;
      prev = OP_prev( prev );
    }

    // Don't complain if jnp appears in a BB without a compare OP.  This is
    // because Expand_Ordered_Select_Compare can generate a compare followed by
    // 2 conditional branches, the second of which is jnp.
    FmtAssert(prev != NULL || OP_code(op) == TOP_jnp,
	      ("set_rflags op is missing") );
  }

  /* Make sure the index register is not %rsp. */
  int index = TOP_Find_Operand_Use( top, OU_index );
  if( index >= 0 ){
    REGISTER reg = TN_register( OP_opnd(op,index) );
    FmtAssert( reg != RSP, ("%rsp can not serve as index register in lea") );
  }
#endif

  oinfo = ISA_OPERAND_Info(top);

  INT results = OP_results(op);
  if (results != TOP_fixed_results(top)) {
    FmtAssert(TOP_is_var_opnds(top) && results > TOP_fixed_results(top),
	      ("wrong number of results (%d) for %s",
	       results,
	       TOP_Name(top)));
    results = TOP_fixed_results(top); // can only verify fixed results
  }
  for (i = 0; i < results; ++i) {
    Verify_Operand(oinfo, op, i, TRUE);
  }

  INT opnds = OP_opnds(op);
  if (opnds != TOP_fixed_opnds(top)) {
    FmtAssert(TOP_is_var_opnds(top) && opnds > TOP_fixed_opnds(top),
	      ("wrong number of operands (%d) for %s",
	       opnds,
	       TOP_Name(top)));
    opnds = TOP_fixed_opnds(top); // can only verify fixed operands
  }
  for (i = 0; i < opnds; ++i) {
    Verify_Operand(oinfo, op, i, FALSE);
  }

#ifdef KEY
  // Check for unique operand and result registers.
  if (OP_uniq_res(op)) {
    for (i = 0; i < results; i++) {
      int j;
      TN *result_tn = OP_result(op, i);
      for (j = 0; j < opnds; j++) {
	TN *opnd_tn = OP_opnd(op, j);
	if (TN_is_register(opnd_tn)) {
	  FmtAssert(!TNs_Are_Equivalent(opnd_tn, result_tn),
		    ("need unique registers for %s operand and result",
		     TOP_Name(top)));
	}
      }
    }
  }
#endif
}

/* ====================================================================
 *
 * r_assemble_binary
 *
 * Generate an instruction word(s) for the given OP.
 *
 * ====================================================================
 */
static INT r_assemble_binary ( OP *op, BB *bb, ISA_PACK_INST *pinst )
{
  INT    i;
  INT    words;
  TOP    opcode = OP_code(op);
  INT64  result[ISA_OPERAND_max_results];
  INT64  opnd[ISA_OPERAND_max_operands];

  for (i = 0; i < OP_opnds(op); i++) {
    INT64 val = 0;
    TN *t = OP_opnd(op, i);

    if (TN_is_constant(t)) {
      if (TN_is_label(t)) {

	// get the target address 
	val = Get_Label_Offset(TN_label(t)) + TN_offset(t);

	// get pc-relative offset if necessary
	if (OP_opnd_is_pcrel(op, i)) {
	  val -= PC_Bundle(PC);
	  if (PROC_has_branch_delay_slot()) val -= sizeof(ISA_PACK_INST);
	}

	FmtAssert (ISA_LC_Value_In_Class(val, OP_opnd_lit_class(op, i)),
		   ("branch offset %" SCNd64 " doesn't fit; try recompiling with "
		    "-CG:longbranch_limit=[try values smaller than %d]", 
		    val, EMIT_Long_Branch_Limit));
      } 
      else if (TN_is_tag(t)) {
	// get the target address 
	val = Get_Label_Offset(TN_label(t));
      }
      else if (TN_is_symbol(t)) {
	ST *base_st;
	INT64 base_ofst;
	ST *st = TN_var(t);

	val = TN_offset(t);
	if (ST_class(st) == CLASS_VAR && ST_is_export_local(st)) {
	    FmtAssert(Has_Base_Block(st),
		("relocation of local symbol %s that hasn't been allocated?", ST_name(st)));
	}
	Base_Symbol_And_Offset (st, &base_st, &base_ofst);

	if (OP_jump(op)) {
	  Em_Add_New_Rel (EMT_Put_Elf_Symbol (st), R_MIPS_26, PC, 
			  PU_section);
	  val = 0;
	}
	else if (base_st == SP_Sym || base_st == FP_Sym) {
	  val += base_ofst;
	  if ( TN_is_reloc_neg(t)) val = -val;

	  if (TN_is_reloc_low16(t)) {
	    val = (val << 48) >> 48;
	  } else if (TN_is_reloc_high16(t)) {
	    val = ( (val - (short)val) << 32) >> 48;
	  } else if (TN_is_reloc_higher(t)) {
	    val = ( (val + 0x80008000LL) << 16) >> 48;
	  } else if (TN_is_reloc_highest(t)) {
	    val = ( (val + 0x800080008000LL) << 0) >> 48;
	  }
	}
	else if (TN_relocs(t) != 0) {
	  if (CGEMIT_Use_Base_ST_For_Reloc (TN_relocs(t), st)) {
		val += base_ofst;
		st = base_st;
    	  }
	  if (Use_Separate_PU_Section(current_pu,st)) {
		/* use PU text section rather than generic one */
		st = PU_base;
	  }
	  CGEMIT_Relocs_In_Object (t, st, PC, PU_section, &val);
	}
        else if (ST_is_gp_relative(st)) {
	  DevWarn("is gp relative (%s)", ST_name(st));
	  if (ST_class(st) == CLASS_CONST || ST_is_export_local(st)) {
      	    val -= GP_DISP;
	  }
#ifdef TARG_IA64
	  if (!ISA_LC_Value_In_Class(val, LC_i16)) {
#elif TARG_X8664
	  if (!ISA_LC_Value_In_Class(val, LC_simm32)) {
#else
	  if (!ISA_LC_Value_In_Class(val, LC_simm16)) {
#endif
		/* 
		 * Put in addend instead of in instruction,
		 * and put gprel in rela section.
		 * This can happen for large common or array/struct in 
		 * gprel section.  Because it is gprel, IPA is guaranteeing
		 * that the final offset will fit in 16 bits.
		 * Usually, when val is small, 
		 * we put val in instruction and use .rel.
		 */
	  	Em_Add_New_Rela (EMT_Put_Elf_Symbol (st), R_MIPS_GPREL, PC, 
			val, PU_section);
	      	val = 0;
	  }
	  else {
#ifdef TARG_IA64
	    FmtAssert (ISA_LC_Value_In_Class(val, LC_i16),
		("immediate value %" SCNd64 " too large for GPREL relocation", val));
#elif TARG_X8664
	    FmtAssert (ISA_LC_Value_In_Class(val, LC_simm32),
		("immediate value %" SCNd64 " too large for GPREL relocation", val));
#else
	    FmtAssert (ISA_LC_Value_In_Class(val, LC_simm16),
		("immediate value %" SCNd64 " too large for GPREL relocation", val));
#endif
	    Em_Add_New_Rel (EMT_Put_Elf_Symbol (st), R_MIPS_GPREL, PC,
			  PU_section);
	  }
	}
	ISA_LIT_CLASS lc = OP_opnd_lit_class(op, i);
	FmtAssert(ISA_LC_Value_In_Class(val, lc),
		  ("r_assemble_binary: illegal immediate value %" SCNd64 " for %s",
		   val, ISA_LC_Name(lc)));
      } 
      else if (TN_is_enum(t)) {
        val = ISA_ECV_Intval(TN_enum(t));
      }
      else if (TN_has_value(t)) {
        val = TN_value(t);
      } 
      else {
	#pragma mips_frequency_hint NEVER
	FmtAssert (FALSE, ("r_assemble_binary: illegal constant TN"));
      }
    }
    else {
      ISA_REGISTER_CLASS rc = TN_register_class(t);
      REGISTER reg = TN_register(t);
#ifdef HAS_STACKED_REGISTERS
      if (ABI_PROPERTY_Is_stacked(rc, REGISTER_machine_id(rc, reg))
	&& ! BB_rotating_kernel(bb)	// in case rot-regs overlap output regs
	&& REGISTER_Is_Stacked_Output(rc, reg) )
      {
	reg = REGISTER_Translate_Stacked_Output(reg);
      }
#endif
      val = REGISTER_machine_id (rc, reg);
    }

    opnd[i] = val;
  }

  for (i = 0; i < OP_results(op); ++i) {
    INT32 val;
    TN *t = OP_result(op,i);
    ISA_REGISTER_CLASS rc = TN_register_class(t);
    REGISTER reg = TN_register(t);

#ifdef HAS_STACKED_REGISTERS
    if (ABI_PROPERTY_Is_stacked(rc, REGISTER_machine_id(rc, reg))
	&& ! BB_rotating_kernel(bb)	// in case rot-regs overlap output regs
	&& REGISTER_Is_Stacked_Output(rc, reg) )
    {
	reg = REGISTER_Translate_Stacked_Output(reg);
    }
#endif
    val = REGISTER_machine_id (rc, reg);

    result[i] = val;
  }

  words = TI_ASM_Pack_Inst(opcode, result, opnd, pinst);
  FmtAssert (words > 0, ("%s", TI_errmsg));
  return words;
}


#if Is_True_On

static REGISTER_SET defined_regs[ISA_REGISTER_CLASS_MAX+1];
static INT defining_pcs[ISA_REGISTER_CLASS_MAX+1][REGISTER_MAX+1];
static OP *defining_ops[ISA_REGISTER_CLASS_MAX+1][REGISTER_MAX+1];

static void
Init_Sanity_Checking_For_BB (void)
{
  ISA_REGISTER_CLASS cl;
  /* Intialize checking code for block. */
  FOR_ALL_ISA_REGISTER_CLASS(cl)
    defined_regs[cl] = REGISTER_SET_EMPTY_SET;
}

static void
Perform_Sanity_Checks_For_OP (OP *op, BOOL check_def)
{
  TN *tn;
  ISA_REGISTER_CLASS cl;
  REGISTER reg;
  INT i;
  BOOL predicated = OP_has_predicate(op) != 0;

#if 0	// wait until srcpos are more robust
  if (!OP_noop(op) && SRCPOS_linenum(OP_srcpos(op)) == 0) {
	DevWarn("0 linenum on op at PC 0x%x", PC);
  }
#endif
  for (i = 0; i < OP_opnds(op); i++) {
    tn = OP_opnd (op, i);
    if (TN_is_register(tn)) {
      cl = TN_register_class(tn);
      reg = TN_register(tn);
      defined_regs[cl] = REGISTER_SET_Difference1 (defined_regs[cl], reg);
    }
  }
  if (check_def) {
    for (i = 0; i < OP_results(op); i++) {
      tn = OP_result(op, i);
      cl = TN_register_class(tn);
      reg = TN_register(tn);
      if (REGISTER_SET_MemberP(defined_regs[cl], reg)) {
	OP *def_op = defining_ops[cl][reg];
	if ((CG_opt_level > 0) && // How can we clean up at level 0? (see pv #769596)
	    !OP_has_implicit_interactions(def_op) && 
	    !OP_cond_def(def_op) && 
	    !OP_cond_def(op) &&
	    OP_results(def_op) == 1 && // a hack for multi-result ops
            (!OP_has_predicate(def_op) || (OP_opnd(def_op,OP_PREDICATE_OPND) == True_TN)) &&
            (!OP_has_predicate(op) || (OP_opnd(op,OP_PREDICATE_OPND) == True_TN))) 
	{
	  DevWarn("Unused definition in %sBB:%d (PC=0x%x)",
	          OP_bb(op) && BB_rotating_kernel(OP_bb(op)) ? "SWPd " : "",
	          OP_bb(op) ? BB_id(OP_bb(op)) : -1, defining_pcs[cl][reg]);
	  if (TFile != stdout) {	/* only print to .t file */
	    Print_OP_No_SrcLine (def_op);
	  }
	}
      }
      defined_regs[cl] = REGISTER_SET_Union1(defined_regs[cl], reg);
      defining_pcs[cl][reg] = PC;
      defining_ops[cl][reg] = op;
    }
  }

  if (((tn = CGTARG_Copy_Operand_TN(op)) != NULL) &&
#ifdef KEY
      TN_is_register(tn) &&
#endif
      (!OP_has_predicate(op) || (OP_opnd(op,OP_PREDICATE_OPND) == True_TN))) {
    if (TN_register(OP_result(op,0)) == TN_register(tn)) {
      DevWarn("Redundant Copy instruction in %sBB:%d (PC=0x%x)",
	      OP_bb(op) && BB_rotating_kernel(OP_bb(op)) ? "SWPd " : "",
	      OP_bb(op) ? BB_id(OP_bb(op)) : -1, PC);
      if (TFile != stdout) {	/* only print to .t file */
        Print_OP_No_SrcLine (op);
      }
    }
  } else {
    if (OP_ixor(op) &&
	  (TN_is_zero(OP_opnd(op,0+predicated)) || TN_is_zero(OP_opnd(op,1+predicated))) )
    {
      DevWarn ("Redundant XOR instruction in %sBB:%d (PC=0x%x)",
	       BB_rotating_kernel(OP_bb(op)) ? "SWPd " : "",
	       BB_id(OP_bb(op)), PC);
      if (TFile != stdout) {	/* only print to .t file */
        Print_OP_No_SrcLine (op);
      }
    }

    if (CGTARG_Is_Right_Shift_Op (op)) {
      if (TN_register(OP_opnd(op,0+predicated)) == REGISTER_zero) {
        DevWarn ("Redundant shift instruction in %sBB:%d (PC=0x%x)",
	         BB_rotating_kernel(OP_bb(op)) ? "SWPd " : "",
	         BB_id(OP_bb(op)), PC);
        if (TFile != stdout) {	/* only print to .t file */
          Print_OP_No_SrcLine (op);
	}
      }
    }
  }

#if defined(TARG_MIPS)
  if (OP_unalign_ld(op)) {
	Is_True(TN_is_zero_reg(OP_opnd(op,2)) 
		|| (TN_register(OP_opnd(op,2)) == TN_register(OP_result(op,0))),
		("unaligned load last-result doesn't match new result "
		 "in BB:%d", OP_bb(op) ? BB_id(OP_bb(op)) : -1));
  }
#endif

  /* Make sure we're not emitting mov.[ds] of integer values. */
  if ((tn = CGTARG_Copy_Operand_TN(op)) != NULL 
	&& TN_is_float(OP_result(op,0)) ) 
  {
	Is_True( ! TN_is_fpu_int(OP_opnd(op,0+predicated)),
	  ("Illegal FP mov of integer TN%d in BB:%d", 
	   TN_number(tn), OP_bb(op) ? BB_id(OP_bb(op)) : -1));
  }

  if (OP_code(op) == TOP_noop) {
    DevWarn("Noop not removed in BB:%d (PC=0x%x)", BB_id(OP_bb(op)), PC);
    if (TFile != stdout) {	/* only print to .t file */
      Print_OP_No_SrcLine(op);
    }
  }
}
#else
#define Perform_Sanity_Checks_For_OP(op, check_def)
#endif

#ifdef KEY
//********************************************************
//Bug 11034: implement dymanic allocation of pu tables to
//various information for dwarf
//********************************************************
static INT32 num_pus = 64; /*initialized as 64*/
static BOOL dw_pu_tables_allocated = FALSE;
LABEL_IDX *Label_pushbp=NULL;
LABEL_IDX *Label_movespbp=NULL;
LABEL_IDX *Label_adjustsp=NULL;
LABEL_IDX *Label_Callee_Saved_Reg=NULL;
LABEL_IDX *Label_First_BB_PU_Entry=NULL;
LABEL_IDX *Label_Last_BB_PU_Entry=NULL;

#if defined(TARG_MIPS)
static INT32 new_cfa_offset = 0;
// We want to generate DWARF for only the first save of GP and return
// register in a PU (bug 12493).
static BOOL gp_stored_for_pu = FALSE;
static BOOL ra_stored_for_pu = FALSE;
#endif

#define ALLOCATE_LABEL_ENTRY                              \
    (TYPE_MEM_POOL_ALLOC_N(LABEL_IDX,&MEM_src_pool,num_pus))

#define DOUBLE_LABEL_ENTRY(label)                         \
    (label=TYPE_MEM_POOL_REALLOC_N(LABEL_IDX,&MEM_src_pool,label,num_pus, 2*num_pus))

static void init_dwarf_pu_tables()
{
   if(!dw_pu_tables_allocated){ // allocate memory 
     Label_pushbp=ALLOCATE_LABEL_ENTRY;
     Label_movespbp=ALLOCATE_LABEL_ENTRY;
     Label_adjustsp=ALLOCATE_LABEL_ENTRY;
     Label_Callee_Saved_Reg=ALLOCATE_LABEL_ENTRY;
     Label_First_BB_PU_Entry=ALLOCATE_LABEL_ENTRY;
     Label_Last_BB_PU_Entry=ALLOCATE_LABEL_ENTRY;
     dw_pu_tables_allocated = TRUE;
   } 
   for(INT i=0; i<num_pus; i++){ //initialization
     Label_pushbp[i] = LABEL_IDX_ZERO;
     Label_movespbp[i] =LABEL_IDX_ZERO;
     Label_adjustsp[i] = LABEL_IDX_ZERO;
     Label_Callee_Saved_Reg[i] = LABEL_IDX_ZERO;
     Label_First_BB_PU_Entry[i] = LABEL_IDX_ZERO;
     Label_Last_BB_PU_Entry[i] = LABEL_IDX_ZERO;
   }
}
    
static void double_dwarf_pu_tables()
{
   FmtAssert(dw_pu_tables_allocated && Label_pushbp, 
                ("double_dwarf_pu_tables: pu_tables were not allocated."));
   DOUBLE_LABEL_ENTRY(Label_pushbp);
   DOUBLE_LABEL_ENTRY(Label_movespbp);
   DOUBLE_LABEL_ENTRY(Label_adjustsp);
   DOUBLE_LABEL_ENTRY(Label_Callee_Saved_Reg);
   DOUBLE_LABEL_ENTRY(Label_First_BB_PU_Entry);
   DOUBLE_LABEL_ENTRY(Label_Last_BB_PU_Entry); 
   num_pus = 2*num_pus;
}

static INT32 pu_entries = 0;
static INT32 pu_entry_count = -1;
#endif // KEY

#ifdef ICE9A_HW_WORKAROUND
// Find a target register for the movn.d instruction such that (1) the movn.d
// will not introduce a WAW hazard, and (2) the register is not one of the
// operands of OP (so writing the register won't delay the execution of OP).
static int
find_movn_dest (OP *op) {
  int reg, i;

  for (reg = 1; reg < 33; reg++) {
    if (fp_reg_state[reg] != OK)
      continue;
    for (i = 0; i < OP_opnds(op); i++) {
      if (TN_is_register(OP_opnd(op, i)) &&
	  TN_register_class(OP_opnd(op, i)) == ISA_REGISTER_CLASS_float &&
	  TN_register(OP_opnd(op, i)) == reg) {
	goto next;
      }
    }
    return reg;
  next:
    continue;
  }
  FmtAssert(FALSE, ("find_movn_dest: cannot find dest reg for movn.d"));
  return 0;	// unreachable
}

// Insert movn.d to clear WAW states to workaround fp stall bug.
static void
clear_WAW_states (OP *op, BB *bb, int movn_dest_reg) {
  int reg;

  for (reg = 1; reg < 33; reg++) {
    if (fp_reg_state[reg] == WAW) {
      if (movn_dest_reg == -1)
	 movn_dest_reg = find_movn_dest(op);
      fprintf(Asm_File,
	      "\tmovn.d %s,%s,$0\t\t# fp stall workaround\n",
	      REGISTER_name(ISA_REGISTER_CLASS_float, movn_dest_reg),
	      REGISTER_name(ISA_REGISTER_CLASS_float, reg));
      fp_reg_state[reg] = OK;
    }
  }
}

static void
workaround_WAR (OP *op)
{
  if (OP_code(op) == TOP_mfc1 ||
      OP_code(op) == TOP_dmfc1) {
    fprintf(Asm_File,
	    "\tdaddu\t$0,$0,%s\t\t# fp stall workaround\n",
	      REGISTER_name(ISA_REGISTER_CLASS_integer,
			    TN_register(OP_result(op, 0))));
    fprintf(Asm_File,
	    "\tssnop\t\t\t\t# fp stall workaround\n");
  }
}

static int fp_lru_clock = 0;
static int fp_reg_modification_time[33];

static void
reset_lru_fp_regs()
{
  int reg;

  fp_lru_clock = 0;
  for (reg = 1; reg < 33; reg++) {
    fp_reg_modification_time[reg] = -1;
  }
}

static void
update_lru_fp_reg (int reg)
{
  fp_reg_modification_time[reg] = fp_lru_clock;
}

static int
lru_fp_reg()
{
  int reg;
  int oldest_reg = 1;
  int oldest_time = fp_reg_modification_time[1];

  for (reg = 2; reg < 33; reg++) {
    // 24 is f23, which is reserved as the dest of the mov.d workaround
    // instruction.  Don't choose it because it would not be the LRU reg among
    // the back-to-back mov.d instructions.
    if (reg == 24)
      continue;

    // If REG was modified more than 10 clock cycles ago, then that's good
    // enough as we are approximating LRU.
    if (fp_lru_clock - fp_reg_modification_time[reg] > 10)
      return reg;

    if (fp_reg_modification_time[reg] < oldest_time) {
      oldest_time = fp_reg_modification_time[reg];
      oldest_reg = reg;
    }
  }
  return oldest_reg;
}
#endif

/* Write out the 'op' into the object file and/or into the assembly file.
 */
static INT
r_assemble_op(OP *op, BB *bb, ISA_BUNDLE *bundle, INT slot)
{
  INT words;
  INT i;

  if (Trace_Inst) {
    Print_OP(op);
  }

  Verify_Instruction(op);

  if (OP_prefetch(op)) Use_Prefetch = TRUE;

#ifdef KEY
  static INT32 label_adjustsp_pu = -1;
  BOOL adjustsp_instr = FALSE;

  // Determine if OP is the first OP in the BB.  Cannot rely on
  // "BB_first_op(bb) == op" because Assemble_Simulated_OP does not update
  // BB_first_op when it expands the first BB OP into the current OP.
  // Bug 14305.
  static BB *prev_bb = NULL;
  static INT32 prev_pu_count = -1;
  INT32 cur_pu_count = Current_PU_Count();
  BOOL is_first_bb_op = ((cur_pu_count != prev_pu_count) || (bb != prev_bb));
  Is_True(((op != BB_first_op(bb)) || is_first_bb_op),
	  ("r_assemble_op: is_first_bb_op should be TRUE for first BB OP"));
  prev_bb = bb;
  prev_pu_count = cur_pu_count;

  if ( BB_entry(bb) && CG_emit_unwind_info ) {
    // Replace uses of entry_name with uses of ST_name, since inserting
    // new elements into StrTab can realloc it and this pointer would be
    // invalid: bug 4222
    // char* entry_name;

    ST *entry_sym; 
    ENTRYINFO *ent;
    ANNOTATION *ant;
    ant = ANNOT_Get (BB_annotations(bb), ANNOT_ENTRYINFO);
    ent = ANNOT_entryinfo(ant);
    entry_sym = ENTRYINFO_name(ent);

#ifdef TARG_X8664
    if (OP_code(op) == TOP_pushq || 
	OP_code(op) == TOP_pushl &&
	Debug_Level > 0) {
      char* buf;
      LABEL* label;
      buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
      		/* EXTRA_NAME_LEN */ 32);
      sprintf(buf, ".LEH_pushbp_%s", ST_name(entry_sym));
      label = &New_LABEL(CURRENT_SYMTAB, Label_pushbp[pu_entries]);
      LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
      fprintf( Asm_File, "%s:\n", LABEL_name(Label_pushbp[pu_entries]));
    } else if ((OP_code(op) == TOP_mov64 || 
    		OP_code(op) == TOP_mov32) &&
	       OP_result(op, 0) == FP_TN && 
	       OP_opnd(op, 0) == SP_TN) {
      char* buf;
      LABEL* label;
      buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
      		/* EXTRA_NAME_LEN */ 32);
      sprintf(buf, ".LEH_movespbp_%s", ST_name(entry_sym));
      label = &New_LABEL(CURRENT_SYMTAB, Label_movespbp[pu_entries]);
      LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
      fprintf( Asm_File, "%s:\n", LABEL_name(Label_movespbp[pu_entries]));
    } else if ((OP_code(op) == TOP_addi64 || 
    		OP_code(op) == TOP_addi32 ||
		OP_code(op) == TOP_sub64 ||
		OP_code(op) == TOP_sub32) &&
	       label_adjustsp_pu != pu_entry_count &&
	       OP_result(op, 0) == SP_TN) {
      char* buf;
      LABEL* label;
      buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
      		/* EXTRA_NAME_LEN */ 32);
      sprintf(buf, ".LEH_adjustsp_%s", ST_name(entry_sym));
      label = &New_LABEL(CURRENT_SYMTAB, Label_adjustsp[pu_entries]);
      LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
      fprintf( Asm_File, "%s:\n", LABEL_name(Label_adjustsp[pu_entries]));
      label_adjustsp_pu = pu_entry_count;
      adjustsp_instr = TRUE;
      // Bug 2929 - The exact position of the last Callee Saved Register
      // spill does not matter. So, we could emit the csr label immediately
      // after adjustsp label (this order is important). The positions of
      // the callee saved register(s) on the stack will be emitted in the 
      // concerned dwarf sections using this csr label. At higher optimization
      // levels, the adjustsp and csr saves can be moved around by EBO and this
      // is a work-around.
      if (Cgdwarf_Num_Callee_Saved_Regs()) { 
	buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
			       /* EXTRA_NAME_LEN */ 32);
	sprintf(buf, ".LEH_csr_%s", ST_name(entry_sym));
	label = &New_LABEL(CURRENT_SYMTAB, Label_Callee_Saved_Reg[pu_entries]);
	LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
	fprintf( Asm_File, "%s:\n", LABEL_name(Label_Callee_Saved_Reg[pu_entries]));
      }
    }
#elif defined(TARG_MIPS)
#if 1 //def KEY_DISABLE_13597_FIX
    static INT32 label_csr_pu = -1;
#endif // KEY_DISABLE_13597_FIX
    if ((OP_code(op) == TOP_addiu || 
         OP_code(op) == TOP_daddiu) &&
         label_adjustsp_pu != pu_entry_count &&
         OP_result(op, 0) == SP_TN) {
      char* buf;
      LABEL* label;
      TN * spadjustment;
      buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
      		/* EXTRA_NAME_LEN */ 32);
      sprintf(buf, ".LEH_adjustsp_%s", ST_name(entry_sym));
      label = &New_LABEL(CURRENT_SYMTAB, Label_adjustsp[pu_entries]);
      LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
      fprintf( Asm_File, "%s:\n", LABEL_name(Label_adjustsp[pu_entries]));

      spadjustment = OP_opnd(op, 1);
      FmtAssert (TN_is_constant(spadjustment) && TN_has_value(spadjustment),
                 ("stack adjustment by non-constant amount"));
      new_cfa_offset = TN_value(spadjustment);
      if (new_cfa_offset < 0)
        new_cfa_offset = -new_cfa_offset;
      label_adjustsp_pu = pu_entry_count;
      adjustsp_instr = TRUE;
    }
#if 1 //def KEY_DISABLE_13597_FIX
    else if (OP_code(op) == TOP_sd &&
             OP_opnd(op, 1) == SP_TN) {
      TN * tn = OP_opnd(op, 0);
      if (TN_is_dedicated(tn) ||
          (TN_register(tn) != REGISTER_UNDEFINED &&
           TN_register(tn) <=
	        REGISTER_CLASS_last_register(TN_register_class(tn)))) {
        PREG_NUM preg = TN_To_Assigned_PREG(tn);
        ISA_REGISTER_CLASS tn_class = TN_register_class(tn);

        if ((ABI_PROPERTY_Is_callee(tn_class, preg) ||
             (ABI_PROPERTY_Is_global_ptr(tn_class, preg) &&
              !gp_stored_for_pu) ||
             (ABI_PROPERTY_Is_ret_addr(tn_class, preg) &&
              !ra_stored_for_pu)) &&
            TN_is_symbol(OP_opnd(op, 2))) {

          static BOOL label_generated = FALSE;
          SAVE_REG_LOC sr;
          extern STACK<SAVE_REG_LOC> Saved_Callee_Saved_Regs;

          if (ABI_PROPERTY_Is_global_ptr(tn_class, preg))
            gp_stored_for_pu = TRUE;
          else if (ABI_PROPERTY_Is_ret_addr(tn_class, preg))
            ra_stored_for_pu = TRUE;

          sr.ded_tn = tn;
          sr.temp = TN_var(OP_opnd(op, 2));
          sr.user_allocated = FALSE;
          Saved_Callee_Saved_Regs.Push(sr);

          if (label_csr_pu != pu_entry_count)
          {
            char * buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
                                        /* EXTRA_NAME_LEN */ 32);
            sprintf(buf, ".LEH_csr_%s", ST_name(entry_sym));
            LABEL * label = &New_LABEL(CURRENT_SYMTAB,
	                               Label_Callee_Saved_Reg[pu_entries]);
            LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
            fprintf(Asm_File, "%s:\n",
	            LABEL_name(Label_Callee_Saved_Reg[pu_entries]));
            label_csr_pu = pu_entry_count;
          }
        }
      }
    }
#endif // KEY_DISABLE_13597_FIX
#endif // TARG_X8664 || TARG_MIPS
  }

#ifdef TARG_MIPS
#if 0 //ndef KEY_DISABLE_13597_FIX
  // bug 13597: Register saves may be present even in BBs that are not
  // entry BBs. Account for such saves in all BBs here.
  if (CG_emit_unwind_info &&
      OP_code(op) == TOP_sd &&
      OP_opnd(op, 1) == SP_TN) {
      TN * tn = OP_opnd(op, 0);
      if (TN_is_dedicated(tn) ||
          (TN_register(tn) != REGISTER_UNDEFINED &&
           TN_register(tn) <=
	        REGISTER_CLASS_last_register(TN_register_class(tn)))) {
        PREG_NUM preg = TN_To_Assigned_PREG(tn);
        ISA_REGISTER_CLASS tn_class = TN_register_class(tn);

        if ((ABI_PROPERTY_Is_callee(tn_class, preg) ||
             (ABI_PROPERTY_Is_global_ptr(tn_class, preg) &&
              !gp_stored_for_pu) ||
             (ABI_PROPERTY_Is_ret_addr(tn_class, preg) &&
              !ra_stored_for_pu)) &&
            TN_is_symbol(OP_opnd(op, 2))) {

          SAVE_REG_LOC sr;
          extern STACK<SAVE_REG_LOC> Saved_Callee_Saved_Regs;

          if (ABI_PROPERTY_Is_global_ptr(tn_class, preg))
            gp_stored_for_pu = TRUE;
          else if (ABI_PROPERTY_Is_ret_addr(tn_class, preg))
            ra_stored_for_pu = TRUE;

          sr.ded_tn = tn;
          sr.temp = TN_var(OP_opnd(op, 2));
          sr.user_allocated = FALSE;
          Saved_Callee_Saved_Regs.Push(sr);

          static INT32 label_csr_pu = -1;
          // did we already emit a csr label for this PU?
          if (label_csr_pu != pu_entry_count) {
            ST *entry_sym; 
            if (BB_entry(bb)) {
              ENTRYINFO *ent;
              ANNOTATION *ant;
              ant = ANNOT_Get (BB_annotations(bb), ANNOT_ENTRYINFO);
              ent = ANNOT_entryinfo(ant);
              entry_sym = ENTRYINFO_name(ent);
            }
            else entry_sym = Get_Current_PU_ST();

            Is_True (entry_sym, ("Current PU name unavailable"));
            char * buf = (char *)alloca(strlen(ST_name(entry_sym)) + 
                                        /* EXTRA_NAME_LEN */ 32);
            sprintf(buf, ".LEH_csr_%s", ST_name(entry_sym));
            LABEL * label = &New_LABEL(CURRENT_SYMTAB,
	                               Label_Callee_Saved_Reg[pu_entries]);
            LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
            fprintf(Asm_File, "%s:\n",
	            LABEL_name(Label_Callee_Saved_Reg[pu_entries]));
            label_csr_pu = pu_entry_count;
          }
        }
      }
    }
#endif // !KEY_DISABLE_13597_FIX
#endif // TARG_MIPS

  Cg_Dwarf_First_Op_After_Preamble_End = FALSE;
  if (Debug_Level > 0 && OP_first_after_preamble_end(op)) {
    Cg_Dwarf_First_Op_After_Preamble_End = TRUE;
    if (OP_srcpos(op) == 0 && op->next != 0)
      OP_srcpos(op) = OP_srcpos(OP_next(op));
  }

  Cg_Dwarf_BB_First_Op = FALSE; 
  if (Debug_Level > 0 && is_first_bb_op &&
      (BB_preds_len(bb) != 1 ||
       (BB_preds_len(bb) == 1 && 
        BB_prev(bb) && BB_First_Pred(bb) != BB_prev(bb))))
    Cg_Dwarf_BB_First_Op = TRUE; 
    // Does this make Cg_Dwarf_First_Op_After_Preamble_End redundant?
#endif // KEY
  Cg_Dwarf_Add_Line_Entry (PC, OP_srcpos(op));
  if (Assembly) {
#ifdef ICE9A_HW_WORKAROUND
    words = 0;

    // TRUE if the last OP written to Asm_File was a movn.d workaround insn.
    static BOOL inserted_movnd_for_round = FALSE;
    if (is_first_bb_op)
      inserted_movnd_for_round = FALSE;
      
    // CG_muld settings greater than 3 should have been converted to
    // CG_hw_round and CG_hw_stall.
    FmtAssert(CG_muld <= 3, ("r_assemble_op: CG_muld > 3"));

    // Emit assembly and insert NOPs to workaround mul.d hardware bug.
    // Bugs 13027, 13481.

    int movn_dest_reg = -1;
    switch (CG_muld) {
      case 0:
      case 2:
      case 3:
	break;	// do nothing
      case 1:
	if (OP_disrupts_muld(op)) {
	  for ( ; muld_shadow > 0; muld_shadow--) {
	    fprintf(Asm_File, "\tnop\t\t\t\t# mul.d bug\n");
	    words += 4;
	  }
	}
	break;
    }

    switch (CG_hw_round) {		// Bug 13481.
      case 0:
	break;	// do nothing
      case 1:
	if (OP_fp_arith(op)) {		// OP goes down the fp pipeline.
	  for (int i = 0; i < OP_opnds(op); i++) {
	    TN *opnd = OP_opnd(op, i);
	    if (TN_is_register(opnd) &&
		TN_register_class(opnd) == ISA_REGISTER_CLASS_float) {
	      fp_reg_state[TN_register(opnd)] = OK;
	      // Rounding hazard goes away if OP reads the result of the
	      // rounding hazard OP.
	      if (round_hazard_op != NULL &&
		  TNs_Are_Equivalent(opnd, OP_result(round_hazard_op, 0))) {
		round_hazard_op = NULL;
	      }
	    }
	  }
	}
	// Guard against rounding hazard before a round trigger OP, or before
	// end of BB.  If BB falls through to successor BB, then don't need
	// guard because the movn.d will be inserted before the first trigger
	// OP in the successor BB.
	if (round_hazard_op != NULL &&
	    (OP_fp_round_trigger(op) ||
	     OP_xfer(op))) {
	  TN *dest = OP_result(round_hazard_op, 0);
	  movn_dest_reg = find_movn_dest(op);

	  Is_True(TN_register_class(dest) == ISA_REGISTER_CLASS_float,
	    ("r_assemble_op: bad result reg class in fp rounding hazard OP"));
	  fprintf(Asm_File,
		  "\tmovn.d %s,%s,$0\t\t# fp rounding workaround\n",
		  REGISTER_name(ISA_REGISTER_CLASS_float, movn_dest_reg),
		  REGISTER_name(ISA_REGISTER_CLASS_float, TN_register(dest)));
	  round_hazard_op = NULL;
	  fp_reg_state[TN_register(dest)] = OK;
	}
	break;
    }

    switch (CG_hw_stall) {		// Bug 13481.
      case 0:
	break;	// do nothing
      case 1:
	// Insert movn.d to clear WAW hazards to workaround fp stall bug.
	if (OP_fp_stall_hazard(op))
	  clear_WAW_states(op, bb, movn_dest_reg);
	break;
      case 3:
	// Always insert 5 movn.d's before a stall hazard OP.  SiCortex 4376.
	{
	  static int stall_hazards = 0;
	  if (is_first_bb_op) {
	    reset_lru_fp_regs();
	    stall_hazards = 0;
	  }
	  if (OP_fp_stall_hazard(op)) {
	    int fp_reg = lru_fp_reg();
	    // Insert 5 movn.d.  Insert 6 if OP is first stall hazard in BB.
	    // Insert one less if one movn.d was already inserted to guard
	    // against an earlier fp round hazard.
	    int num = 5 + (stall_hazards == 0)
			- (inserted_movnd_for_round == TRUE);
	    for ( ; num > 0; num--) {
	      fprintf(Asm_File,
		      "\tmovn.d $f23,%s,$0\t\t# fp stall workaround\n",
		      REGISTER_name(ISA_REGISTER_CLASS_float, fp_reg));
	    }
	    stall_hazards++;
	  }
	}
	break;
    }

    r_assemble_list ( op, bb );
    if (!Object_Code) words += ISA_PACK_Inst_Words(OP_code(op));

    inserted_movnd_for_round = FALSE;

    switch (CG_muld) {
      case 0:			// No workaround.
	break;
      case 1:			// Insert NOPs.
	if (OP_is_like_muld(op)) {
	  muld_shadow = 3;
	} else if (OP_xfer(op)) {
	  // Branches can only single-issue, thus guaranteeing at least 1 cycle
	  // between mul.d and the disruptive OP.
	  muld_shadow = 0;
	} else {
	  muld_shadow--;
	}
	break;
      case 2:			// Always insert one SSNOP.
	if (OP_is_like_muld(op)) {
	  fprintf(Asm_File, "\tssnop\t\t\t\t# mul.d bug\n");
	  words += 4;
	}
	break;
      case 3:			// Always insert two SSNOPs.
	if (OP_is_like_muld(op)) {
	  fprintf(Asm_File, "\tssnop\t\t\t\t# mul.d bug\n");
	  fprintf(Asm_File, "\tssnop\t\t\t\t# mul.d bug\n");
	  words += 8;
	}
	break;
    }

    switch (CG_hw_round) {
      case 0:
	break;	// do nothing
      case 1:			// Insert one movn.d as needed.  Bug 13481.
	if (OP_fp_round_hazard(op))
	  round_hazard_op = op;
	break;
      case 2:			// Always insert one movn.d.  Bug 13481.
	if (OP_fp_round_hazard(op)) {
	  TN *dest = OP_result(op, 0);
	  fprintf(Asm_File,
		  "\tmovn.d $f23,%s,$0\t\t# fp rounding workaround\n",
		  REGISTER_name(TN_register_class(dest), TN_register(dest)));
	  fp_reg_state[TN_register(dest)] = OK;
	  inserted_movnd_for_round = TRUE;
	}
	break;
    }

    switch (CG_hw_stall) {
      case 0:
	break;	// do nothing
      case 1:			// Insert one movn.d as needed.  Bug 13481.
	// Track fp register states to detect WAW.
	if (OP_results(op) > 0 &&
	    TN_register_class(OP_result(op, 0)) == ISA_REGISTER_CLASS_float) {
	  int reg = TN_register(OP_result(op, 0));
	  if (fp_reg_state[reg] == OK) {
	    fp_reg_state[reg] = OP_fp_unpredictable_latency(op) ? PENDING : OK;
	  } else if (fp_reg_state[reg] == PENDING) {
	    fp_reg_state[reg] = WAW;
	  } else if (fp_reg_state[reg] == WAW) {
	    // State unchanged.
	  }
	}
	// Clear WAW states at end of BB.
	if ( // OP is last in BB, but not in branch delay slot.
	    (op == BB_last_op(bb) &&
	      !(OP_prev(op) && OP_xfer(OP_prev(op)))) ||
	    // OP is just before branch/call/return.
	    (OP_next(op) && OP_xfer(OP_next(op)))) {
	  clear_WAW_states(op, bb, -1);
	}
	workaround_WAR(op);
	break;
      case 2:			// Always insert one movn.d.  Bug 13481.
	if (OP_fp_unpredictable_latency(op) &&
	    OP_results(op) > 0 &&
	    TN_register_class(OP_result(op, 0)) == ISA_REGISTER_CLASS_float) {
	  TN *dest = OP_result(op, 0);
	  fprintf(Asm_File,
		  "\tmovn.d $f23,%s,$0\t\t# fp stall workaround\n",
		  REGISTER_name(TN_register_class(dest), TN_register(dest)));
	}
	workaround_WAR(op);
	break;
      case 3:			// Always insert 5 movn.d.  SiCortex 4376.
	// Track fp register modification time.
	if (OP_results(op) > 0 &&
	    TN_register_class(OP_result(op, 0)) == ISA_REGISTER_CLASS_float) {
	  int reg = TN_register(OP_result(op, 0));
	  update_lru_fp_reg(reg);
	}
	fp_lru_clock++;		// Increment clock.
	break;
    }
#else
    r_assemble_list ( op, bb );
    if (!Object_Code) words = ISA_PACK_Inst_Words(OP_code(op));
#endif
  }
#ifdef TARG_X8664
  if (BB_entry(bb) && CG_emit_unwind_info && Frame_Len == 0) {
    char* entry_name;
    ST *entry_sym; 
    ENTRYINFO *ent;
    ANNOTATION *ant;
    ant = ANNOT_Get (BB_annotations(bb), ANNOT_ENTRYINFO);
    ent = ANNOT_entryinfo(ant);
    entry_sym = ENTRYINFO_name(ent);
    entry_name = ST_name(entry_sym);
    
    // If the frame length is zero then spadjust would have been deleted
    // So, we need to generate another psuedo label here and attach it after 
    // the movespbp instruction.
    if ((OP_code(op) == TOP_mov64 || 
	 OP_code(op) == TOP_mov32) &&
	OP_result(op, 0) == FP_TN && 
	OP_opnd(op, 0) == SP_TN) {
      char* buf;
      LABEL* label;
      buf = (char *)alloca(strlen(entry_name) + 
      		/* EXTRA_NAME_LEN */ 32);
      sprintf(buf, ".LEH_adjustsp_%s", entry_name);
      label = &New_LABEL(CURRENT_SYMTAB, Label_adjustsp[pu_entries]);
      LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
      fprintf( Asm_File, "%s:\n", LABEL_name(Label_adjustsp[pu_entries]));
      // if we need to use entry_name here, we must use ST_name instead
      adjustsp_instr = TRUE;
    }
  }
#endif // TARG_X8664

#ifdef KEY
  // Bug 4204 - move the ctrl register setup after the preamble. This 
  // causes the debug information generated to let the debugger to stop
  // at the right spot for the main entry function. Otherwise, the parameters
  // would be displayed incorrectly. 
  if (adjustsp_instr) {
    // Assumption: REGION_First_BB is the first BB in the PU. If in future,
    // we start having multiple regions in a PU here, we need to change the 
    // following. 
    // In such a scenario, we should need to change many of the occurrences
    // of REGION_First_BB above.
    if( Assembly && (REGION_First_BB == bb) &&
	( strcmp( Cur_PU_Name, "MAIN__" ) == 0 ||
	  strcmp( Cur_PU_Name, "main" ) == 0 ) ){
      CGEMIT_Setup_Ctrl_Register( Asm_File );
    }
  }
#endif

  if (Object_Code) {
    ISA_PACK_INST inst[ISA_PACK_MAX_INST_WORDS];
    words = r_assemble_binary ( op, bb, inst );
    for (i = 0; i < words; ++i) {
      ISA_BUNDLE_PACK_COMP slot_comp = (ISA_BUNDLE_PACK_COMP)
	  (ISA_BUNDLE_PACK_COMP_slot + slot++);
      TI_ASM_Set_Bundle_Comp(bundle, slot_comp, inst[i]);
      if (slot == ISA_MAX_SLOTS) {
	slot = 0;
	++bundle;
      }
    }

    /* May need to add information about this call (or tail call) site */
    if (OP_call(op) || OP_tail_call(op)) {
      CGEMIT_Add_Call_Information (op, bb, PC, PU_section);
    }
if (Get_Trace ( TP_EMIT,0x100 )) {
/* don't do this till decide on format of EK_SWITCH */
    if (OP_ijump(op) && !OP_call(op) && !BB_exit(bb)) {
      ANNOTATION *ant = ANNOT_Get(BB_annotations(bb), ANNOT_SWITCH);
      if (ant != NULL) {
	ST *jumptable = ANNOT_switch(ant);
	BOOL gprel = ST_is_gp_relative(jumptable);
	INT num_entries = TY_AR_ubnd_val(ST_type(jumptable),0)+1;
	Elf_Event_Kind event_type;
	event_type = (Use_32_Bit_Pointers ? EK_SWITCH_32 : EK_SWITCH_64);

	Em_Add_New_Event (event_type, PC, 
		gprel, EMT_Put_Elf_Symbol(jumptable), num_entries, PU_section);
      }
    }
}
  }

  PC = PC_Incr_N(PC, words);

  // hack to keep track of last label and offset for assembly dwarf (suneel)
  // If the current op is a TOP_asm, mark the Last_Label as invalid.
  if (OP_code(op) == TOP_asm) {
    Last_Label = LABEL_IDX_ZERO;
  }
  else {
#ifndef KEY 
    // Bug 2468 - can not update offset/PC for x86 target (variable length)
    Offset_From_Last_Label = PC_Incr_N(Offset_From_Last_Label, words);
#endif
  }

  return words;
}


#ifdef KEY
#include "cxx_memory.h"

static char* 
Replace_Substring(char* in, char* from, const char* to)
{
  if (strcmp(from, to) == 0) return in;

  // Assume 'from' appears 5 times in 'in' to start with and later reallocate
  // memory if need be. Due to the context, this is more efficient than finding
  // out the number of occurrences of 'from' in 'in'.
  UINT  buflen = strlen(in) + 5*strlen(to) + 64;
  char* buf = CXX_NEW(char[buflen], &MEM_local_pool);
  char* tmp = CXX_NEW(char[buflen], &MEM_local_pool);
  char* cpy = in;
  char* p = NULL;
  char* q = NULL;
  char* leftover;
  INT times = 5;
  INT substitute = 1; // By default, substitutions ought to happen.

  strcpy(buf, ""); // initialize
  strcpy(tmp, ""); // initialize
  strcat(in, "\0");
  while ((p = strstr(in, from)) != NULL) {

    leftover = p + strlen(from);

    // Bug 7825 happened because in the earlier implementation, we used to 
    // replace the 'from' instance by 'to' even if the 'from' instance happened
    // to be a prefix of a larger instance of the token class that the incoming 
    // 'from' belonged to. 
    //
    // With the 2.x GCC series, the maximum number of operands permissible in
    // an asm() statement was 10, and as no pattern in %0 through %9 share a 
    // common prefix, this scheme worked just fine. Since the GCC 3.x series, 
    // asm() statements can have up to 30 operands. So, this scheme would no 
    // longer work: wrong substitutions happenned.
    // (e.g. %10 -> <'to' for 'from' %1>0 (Note trailing '0')).
    //
    // Using strstr(3) to skim over the 'in' (asm template) is fine, but does 
    // not suffice as the basis upon which to substitute 'from' by 'to' - we 
    // needed to ascertain that the 'from' instance happens to be a maximal 
    // instance of token class:
    //
    // %<number>
    //
    // Note: From the GCC 3.3.5 User Manual, we have the following excerpt:
    //
    // "As of GCC version 3.1, it is also possible to specify input and output 
    //  operands using symbolic names which can be referenced within the 
    //  assembler code."
    //
    // However, no two instances of the token class: %[<identifier>] can have a
    // common-prefix, so we are fine implementing our check for when the asm() 
    // operands are referred to per the older scheme viz. in a numbered manner.
    //
    // Note: It might seem like iterating the operand list in the descending
    // order of operands is a plausible workaround, but that might not be the
    // only context in which this function is invoked. The kludge would be exposed
    // when a single invocation involving a single numbered operand that shares a 
    // common prefix with another numbered operand in the operand list occurs.
    // So we rather do the right thing and fix things in the proper place.
    //
    // Hence the following code to implement a "maximal-munch" when we find that
    // the 'from' pattern is a numbered operand reference. (Admittedly, we're 
    // making this function specific to substituting references to operands in 
    // assembler templates, but then the other option would be to specify a
    // pattern (via regex(3)) for 'from' and that's adding dependencies and
    // complicating matters).

    if (strlen(from) >= 2 && 
        from[0] == '%' && 
        (isdigit(from[1]) || 
         isalpha(from[1]) || 
         from[1] == '*' || 
         from[1] == '&')) {

      // We've ensured that 'from' is a numbered operand reference possibly with
      // a modifier. (See Bug 3141 Additional Comment #7 for a more extensive 
      // list of modifiers in an operand reference that are possible in an asm()
      // assembler template but are not documented in the GCC User Manual).

      // That we have a 'from' instance in 'in' and that 'in' is '\0' 
      // terminated, implies that 'p + 2' is a valid address. Hence:

      q = p + 2;

      // Now scan ahead from this point onwards ensuring that we are within 
      // bounds (i.e. we have'nt yet hit '\0') cand a partially constructed 
      // number or identifier continues to be formed. 
      // Note that this weak check is is O.K. as long as 'from' is syntactically
      // well defined.

      while (*q != '\0' && (isdigit(*q) || isalpha(*q))) q++;

      // We already have proof that 'p' points to a prefix of 'from' in 'in'.
      // If the number of characters we've marched past 'p' upto 'q' above
      // non-inclusively, equals the length of 'from' - it's an exact match
      // and we allow the substitution to take place. Else, we want to 
      // prohibit the substitution.

      if ((q - p) == strlen(from)) 
        substitute = 1;
      else 
        substitute = 0;

    }
    else substitute = 1; // Preserve prior behaviour.

    strncpy(tmp, in, strlen(in) - strlen(p));
    tmp[strlen(in) - strlen(p)] = '\0'; // terminate the string
    strcat(buf, tmp);
    if (substitute)
      strcat(buf, to);
    else
      strcat(buf, from); // Because we are going to resume at 'from'.
    in = leftover;
    if (--times == 0) {
      buf = TYPE_MEM_POOL_REALLOC_N(char, &MEM_local_pool, buf,
				    buflen, buflen + 5*strlen(to) + 64);
      tmp = TYPE_MEM_POOL_REALLOC_N(char, &MEM_local_pool, tmp,
				    buflen, buflen + 5*strlen(to) + 64);
      buflen += (5*strlen(to) + 64);	
      times = 5; 
    }
  }
  CXX_DELETE(tmp, &MEM_local_pool);
  if (strcmp(buf, "") == 0) {
    CXX_DELETE(buf, &MEM_local_pool);
    return cpy;
  }  
  strcat(buf, in);
  return buf;
}
#else
static char* 
Replace_Substring(char* in, char* from, const char* to)
{
  UINT  buflen = strlen(in) + 64;
  char* buf = (char*) alloca(buflen);
  char* p;
  while ((p = strstr(in, from)) != NULL) {
    char* leftover = p + strlen(from);
    *p = '\0';
    while (strlen(in) + strlen(to) + strlen(leftover) >= buflen) {
      buflen = buflen * 2;
      buf = (char*) alloca(buflen);
    }
    sprintf(buf, "%s%s%s", in, to, leftover);
    in = strdup(buf);
  }
  return in;
}
#endif /* KEY */

#ifdef TARG_X8664
static const char* int_reg_names[5][16] = {
  /* BYTE_REG: low 8-bit */
  { "%al", "%bl", "%bpl", "%spl", "%dil", "%sil", "%dl", "%cl",
    "%r8b",  "%r9b",  "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b" },
  /* BYTE_REG: high 8-bit */
  { "%ah", "%bh", "%bph", "%sph", "%dih", "%sih", "%dh", "%ch",
    /* The following is a filler. There is no "high"  for these registers */
    "%r8b",  "%r9b",  "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b" },
  /* WORD_REG: 16-bit */
  { "%ax", "%bx", "%bp", "%sp", "%di", "%si", "%dx", "%cx",
    "%r8w",  "%r9w",  "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w" },
  /* DWORD_REG: 32-bit */
  { "%eax", "%ebx", "%ebp", "%esp", "%edi", "%esi", "%edx", "%ecx",
    "%r8d",  "%r9d",  "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d" },
  /* QWORD_REG: 64-bit */
  { "%rax", "%rbx", "%rbp", "%rsp", "%rdi", "%rsi", "%rdx", "%rcx",
    "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15" },
};
#endif  
static char* 
Modify_Asm_String (char* asm_string, UINT32 position, bool memory,
#ifdef KEY
		   TN* offset_tn,  /* for memory opnd */
#endif
		   TN* tn, BB *bb)
{
  char* name = NULL;
#ifdef TARG_X8664
  char st_reg_name_template[7] = {'%', 's', 't', '(', '\0', ')', '\0'};
#endif
  if( TN_is_register(tn) ){
    ISA_REGISTER_CLASS cl = TN_register_class(tn);
    REGISTER reg = TN_register(tn);
#ifdef HAS_STACKED_REGISTERS
    if (ABI_PROPERTY_Is_stacked(cl, REGISTER_machine_id(cl, reg))
	&& ! BB_rotating_kernel(bb)	// in case rot-regs overlap output regs
        && REGISTER_Is_Stacked_Output(cl, reg)) 
    {
      reg = REGISTER_Translate_Stacked_Output(reg);
    }
#endif
    name = (char*) REGISTER_name(cl, reg);
#ifdef TARG_X8664
    // Rename an integer register based on the size of the variable.
    if (cl == ISA_REGISTER_CLASS_integer) {
       name = (char*) CGTARG_Modified_Asm_Opnd_Name('r', tn, name);
    }
    // Check if you need to modify st0/st1 into parenthesised format.
    // Save a function call to strcmp(3).
    if (name[0] == '%' && 
        name[1] == 's' && 
        name[2] == 't' && 
        (name[3] == '0' || name[3] == '1') && 
        name[4] == '\0') { 
      st_reg_name_template[4] = name[3];
      name = st_reg_name_template;
    }
#endif
    if (memory) {
#if defined(TARG_MIPS)
      char* buf = (char*) alloca(strlen(name)+4);
      sprintf(buf, "0(%s)", name);
#else
      char* buf = (char*) alloca(strlen(name)+64);
#ifdef TARG_X8664      
      // ISA_REGISTER_CLASS_integer class renaming has happened above, 
      // so we just deal with the other classes out here. 
      if (Is_Target_32bit() && cl != ISA_REGISTER_CLASS_integer) {
	char modifier = 'r';
	name = (char*) CGTARG_Modified_Asm_Opnd_Name(modifier, tn, name);
      }
#endif
#if defined(TARG_IA32) || defined(TARG_X8664)
      if (offset_tn != NULL) {
	if (TN_is_symbol(offset_tn)) {
	  ST *base_st = NULL;
	  INT64 base_ofst = 0;
	  ST *offset_st = TN_var(offset_tn);
	  Base_Symbol_And_Offset(offset_st, &base_st, &base_ofst);
	  base_ofst += TN_offset(tn);

	  if (base_ofst == 0)
	    sprintf(buf, "(%s)", name);
	  else
	    sprintf(buf, "%d(%s)", (INT) base_ofst, name);
	} else if (TN_has_value(offset_tn)){
	  sprintf(buf, "%d(%s)", (INT)TN_value(offset_tn),name);
	} else {
	  FmtAssert(FALSE, ("Modify_Asm_String: unexpected offset_tn"));
	}
      } else {
	sprintf(buf, "(%s)", name);
      }
#else
      sprintf(buf, "[%s]", name);
#endif
#endif
      name = buf;
    }
  }
#ifdef TARG_X8664
  else if( TN_is_symbol(tn) ){
    ST* base_st = NULL;
    INT64 base_ofst = 0;
    TN* base_tn = NULL;

    ST* st = TN_var(tn);
    Base_Symbol_And_Offset( st, &base_st, &base_ofst );

    base_ofst += TN_offset(tn);

    char* buf = (char*)alloca(strlen(ST_name(st)) + /* EXTRA_NAME_LEN */ 64);

    if (ST_is_thread_local(st)) {
      name = ST_name(st);
      sprintf(buf, "%%fs:%s@TPOFF", name);
    } else if( base_st == SP_Sym || base_st == FP_Sym ){
      base_tn = base_st == SP_Sym ? SP_TN : FP_TN;
      name = (char*)REGISTER_name( TN_register_class(base_tn), TN_register(base_tn) );
      if( Is_Target_32bit() ){
	name = (char*)CGTARG_Modified_Asm_Opnd_Name( 'r', base_tn, name );
      }

      sprintf( buf, "%d(%s)", (int)base_ofst, name );

    } else {
      name = ST_name( st );

      if (!memory /* bug 14399 */ && !strcmp (name, ".rodata")) {
        // This is the address of a string constant, treat it similar
        // to a numeric constant. (bug 14390)
        if( base_ofst == 0 )
          sprintf( buf, "$%s", name );
        else
          sprintf( buf, "$%s+%d", name, (int)base_ofst );
      }
      else {
        if( base_ofst == 0 ){
	  if( Is_Target_32bit() )
	    sprintf( buf, "%s", name );
	  else
	    sprintf( buf, "%s(%%rip)", name );
        } else
	  if( Is_Target_32bit() )
	    sprintf( buf, "%s+%d", name, (int)base_ofst );
	  else
	    sprintf( buf, "%s+%d(%%rip)", name, (int)base_ofst );
       }
    }

    name = buf;
  }
  else {
    FmtAssert(!memory && TN_is_constant(tn) && TN_has_value(tn),
              ("ASM operand must be a register or a numeric constant"));
    char* buf = (char*) alloca(32);
    sprintf(buf, "$%" SCNd64 "",TN_value(tn));
    name = buf;
  }
#else
  else {
    FmtAssert(!memory && TN_is_constant(tn) && TN_has_value(tn),
              ("ASM operand must be a register or a numeric constant"));
    FmtAssert(!(TN_is_symbol(tn) && ST_is_thread_local(TN_var(tn))),
              ("Modify_Asm_String: thread-local ASM operand NYI"));
    char* buf = (char*) alloca(32);
    sprintf(buf, "%" SCNd64 "",TN_value(tn));
    name = buf;
  }
#endif
  
  char pattern[4];
  sprintf(pattern, "%%%d", position);
  
  asm_string =  Replace_Substring(asm_string, pattern, name);
#ifdef TARG_X8664
  if (TN_is_register(tn)) {
    char patternp[5];
    sprintf(patternp, "%%P%d", position);
    if (strstr(asm_string, patternp)) {
      asm_string =  Replace_Substring(asm_string, patternp, name);
    }
    // Bug 3141: Support the 'y' modifier in operand references within
    //           the asm template.
    // Note that we only modify %st0 into %st(0) [and %st1 into %st(1)];
    // so, we need to check for %st too (Which is what %y is about anyways).
    if (strcmp(name, "%st") == 0 || strcmp(name, "%st(0)") == 0) {
      sprintf(pattern, "%%y%d", position);
      asm_string = Replace_Substring(asm_string, pattern, "%st(0)");
    }
  }
  // "name" may have base as well as offset, so compare only the leading
  // half of the name.
  else if (TN_is_symbol(tn) && !strncmp(name, "$.rodata", 8)) { // bug 14390
    // Replace any %c<num> constraint with the immediate constant string
    if (strstr(asm_string, "%c")) {
      char replace[5];
      sprintf(replace, "%%c%d", position);
      // Drop the '$' in name.
      asm_string = Replace_Substring(asm_string, replace, name+1);
    }
  }
#endif

#ifdef TARG_MIPS
  // SiCortex 3443: Handle %z<num> constraint assuming no special meaning
  if (TN_is_register(tn)) {
    if (strstr(asm_string, "%z")) {
      char replace[5];
      sprintf(replace, "%%z%d", position);
      asm_string = Replace_Substring(asm_string, replace, name);
    }
  }
  // Replace any %x<num> constraint with the immediate value written in hex
  if (!TN_is_register(tn) && !TN_is_symbol(tn)) {
    if (strstr(asm_string, "%x")) {
      FmtAssert(!memory && TN_is_constant(tn) && TN_has_value(tn),
		("ASM operand for %x constraint must be numeric constant"));
      char* buf = (char*) alloca(32);
      sprintf(buf, "0x%" SCNx64 "",TN_value(tn));
      name = buf;
      char replace[5];
      sprintf(replace, "%%x%d", position);
      asm_string = Replace_Substring(asm_string, replace, name);
    }
  }
#endif
#ifdef TARG_X8664
  // Replace any %c<num> constraint with the immediate value
  // Replace any %n<num> constraint with the negated immediate value
  if (!TN_is_register(tn) /*&& !TN_is_symbol(tn)*/) {
    if (strstr(asm_string, "%c")) {
      char replace[5];
      sprintf(replace, "%%c%d", position);
      // Drop the '$' in name.  Bug 7406.
      asm_string = Replace_Substring(asm_string, replace, name+1);
    }
    if (strstr(asm_string, "%n")) {
      // Bug 3141: One of the problems found was that we did not support
      // %n<num> in the asm() assembler template.
      // The assembler downstream handles compile-time constant expressions as 
      // immediates in instructions, so we are fine just prefixing a '-' to the 
      // immediate value in 'name'.
      char replace[5];
      char tmp;
      sprintf(replace, "%%n%d", position);
      tmp = *name;
      *name = '-';
      asm_string = Replace_Substring(asm_string, replace, name);
      *name = tmp;
    }
    if (strstr(asm_string, "%P")) {
      char replace[5];
      sprintf(replace, "%%P%d", position);
      // Bug 14408: Treat %P like %c, and skip the initial "$" sign.
      asm_string = Replace_Substring(asm_string, replace, name+1);
    }
  }
  // Follow the zero dialect_number implementation as in 
  // gcc/final.c:output_asm_insn and handle {, } and | operators
  if (strchr(asm_string, '{')) {
    char buf[500];
    int next = 0;
    buf[0] = '\0';
    for (INT i = 0; i < strlen(asm_string); i ++) {
      if (asm_string[i] == '{' || asm_string[i] == '}') 
	continue;
      else if (asm_string[i] == '|') {
	INT j = i+1;
	BOOL found = TRUE;
	while (asm_string[j] != '}') {
	  j ++;
          if (j == strlen(asm_string)) {
	    found = FALSE;
	    break;
	  }
	}
	if (found) {
	  i = j;
	  continue; 
	} else {
	  FmtAssert(FALSE, 
	            ("Modify_Asm_String: Inline assembly %s has unmatched '{'",
		    asm_string));
	}
      } else {
        buf[next++] = asm_string[i]; 
	buf[next] = '\0';
      }
    }
    sprintf(asm_string, "%s", buf);
  }

  // Handle modifier between % and the operand number, such as %b0, %h0.  The
  // modifier controls how the register or instruction name is printed.
  {
    // Need to test bugs 504, 2455, 3141 
    // if any change is made in this function.
   
    // Handle x86 style asm operand constraints after all regular constraints.
    // This avoids replacing same register names multiple times.
    char x86pattern[5];
    const char *tmp_name;
    REGISTER reg;
    BOOL is_reg = TN_is_register(tn);

    if (is_reg) {
      reg = TN_register(tn);
    }
    
    // Handle any %b constraint
    tmp_name = is_reg ? int_reg_names[0][reg-REGISTER_MIN] : name;
    sprintf(x86pattern, "%%b%d", position);
    asm_string = Replace_Substring(asm_string, x86pattern, tmp_name);
    
    // Handle any %h constraint
    tmp_name = is_reg ? int_reg_names[1][reg-REGISTER_MIN] : name;
    sprintf(x86pattern, "%%h%d", position);
    asm_string = Replace_Substring(asm_string, x86pattern, tmp_name);
    
    // Handle any %w constraint
    tmp_name = is_reg ? int_reg_names[2][reg-REGISTER_MIN] : name;
    sprintf(x86pattern, "%%w%d", position);
    asm_string = Replace_Substring(asm_string, x86pattern, tmp_name);
    
    // Handle any %k constraint
    tmp_name = is_reg ? int_reg_names[3][reg-REGISTER_MIN] : name;
    sprintf(x86pattern, "%%k%d", position);
    asm_string = Replace_Substring(asm_string, x86pattern, tmp_name);

    // Handle any %q constraint
    tmp_name = is_reg ? int_reg_names[4][reg-REGISTER_MIN] : name;
    sprintf(x86pattern, "%%q%d", position);
    asm_string = Replace_Substring(asm_string, x86pattern, tmp_name);

    // Handle any %z modifier instance.
    const char *suffix = NULL;
    sprintf(x86pattern, "%%z%d", position);
    if (strstr(asm_string, x86pattern) != NULL) {
      UINT size;
      if (TN_is_symbol(tn)) {
	ST* st = TN_var(tn);
	size = ST_size(st);
      } else {
	size = TN_size(tn);
      }
      switch (size) {
        case 1: suffix = "b"; break;
        case 2: suffix = "w"; break;
        case 4: suffix = "l"; break;
        case 8: suffix = "q"; break;
        default: FmtAssert(FALSE, ("Opcode suffix requested for operand %d which has size %d", position, size)); break;
      }
      asm_string =  Replace_Substring(asm_string, x86pattern, suffix);
    }
  }
#endif	// TARG_X8664
  
  return asm_string;
}

static char*
Generate_Asm_String (OP* asm_op, BB *bb)
{
  UINT i;
  ASM_OP_ANNOT* asm_info = (ASM_OP_ANNOT*) OP_MAP_Get(OP_Asm_Map, asm_op);
  char* asm_string = strdup(WN_asm_string(ASM_OP_wn(asm_info)));

#ifdef KEY

  // Bug 5009: ASM: need to check operand number
  UINT n, index_count;
  char *p, *q, c;

  p = asm_string;

  // In the following loop, we identify an operand reference based on the 
  // following syntax: %<single-character-modifier>?<number> 
  while(*p != '\0') {

    // In case the '%' is followed by another one, consume it and continue.
    if (*p == '%' && *(p + 1) == '%') {
                                                                                                                                                  
      p += 2; 
      continue;
                                                                                                                                                  
    }
    else if (*p == '%') {
                                                                                                                                                  
      p++; // (Consume the '%').
                                                                                                                                                  
      // In case there is a modifer, get past it, taking care not to overshoot.
      if (*p != '\0' && !isdigit(*p)
#ifdef KEY // bug 11651
	  && *p != 'r' && *p != 'R'
#endif
	 ) p++;
                                                                                                                                                  
      // Save the start location and scan ahead, looking for a number.
      q = p;
      while (*p != '\0' && isdigit(*p)) p++;
                                                                                                                                                  
      // If we got an operand reference, warn if it is not within permissible 
      // bounds for this asm() statement.
      if (p - q > 0) {
                                                                                                                                                  
        c = *p;
        *p = '\0';
        n = atoi(q);
        *p = c;

        index_count = OP_opnds(asm_op) + OP_results(asm_op);
                                                                                                                                                  
        if (n >= index_count) {
                                                                                                                                                  
          char message[128]; 
          // Every time you change the text of any of the error message that's
          // going out - make sure the size of the message array is large 
          // enough!
                                                                                                                                                  
          if (index_count == 0)
            sprintf(message, "Not expecting to see any operand references");
          else if (index_count == 1)
            sprintf(message, "Only permissible reference can be to operand 0");
          else
            sprintf(message, 
                    "Expecting to see references to operands numbered"
                    " between 0 and %d inclusive",
                    index_count - 1);

          // Emit the diagnostic.
          ErrMsg(EC_ASM_Bad_Operand, Srcpos_To_Line(OP_srcpos(asm_op)), n,
                 message);
                                                                                                                                                  
        }
      }
    }
    else p++;
  }

  // Bug 2486
  static INT asm_unique_number = 111; // each asm with a %= has a unique number
  if (strstr(asm_string, "%=")) {
    char pattern[3];
    char name[10];
    strcpy(pattern, "%=");
    sprintf(name, "%d", asm_unique_number);
    asm_string = Replace_Substring(asm_string, pattern, name);
    asm_unique_number ++;
  }

  if (strcmp(asm_string, "__asm_builtin_apply_load") == 0) {
    FmtAssert(OP_opnds(asm_op) == 1, ("Has to have 1 operand"));
    TN *tn = OP_opnd(asm_op, 0);
    FmtAssert(TN_is_register(tn), ("opnd has to be in register"));
    ISA_REGISTER_CLASS cl = TN_register_class(tn);
    REGISTER reg = TN_register(tn);
    char name[256];
    char *asm_string;
    asm_string = (char *)malloc(sizeof(char)*1024);
    sprintf(name, "%s", REGISTER_name(cl, reg));
    sprintf(asm_string, "ld $4, 8(%s)\n", name);
    sprintf(asm_string, "%s\tld $5, 16(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tld $6, 24(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tld $7, 32(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tld $8, 40(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tld $9, 48(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tld $10, 56(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tld $11, 64(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f12, 72(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f13, 80(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f14, 88(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f15, 96(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f16, 104(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f17, 112(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f18, 120(%s)\n", asm_string, name);
    sprintf(asm_string, "%s\tldc1 $f19, 128(%s)", asm_string, name);
    return asm_string;
  }
#endif
  for (i = 0; i < OP_results(asm_op); i++) {
    asm_string = Modify_Asm_String(asm_string, 
                                   ASM_OP_result_position(asm_info)[i], 
                                   ASM_OP_result_memory(asm_info)[i], 
#ifdef KEY
				   NULL,
#endif
                                   OP_result(asm_op, i), bb);
  }

  for (i = 0; i < OP_opnds(asm_op); i++) {
    asm_string = Modify_Asm_String(asm_string, 
                                   ASM_OP_opnd_position(asm_info)[i], 
                                   ASM_OP_opnd_memory(asm_info)[i], 
#ifdef KEY
				   (TN*)ASM_OP_opnd_offset(asm_info)[i],
#endif
                                   OP_opnd(asm_op, i), bb);
  }

#ifndef TARG_X8664
  CGTARG_Postprocess_Asm_String(asm_string);
#else
  if (strstr(asm_string, "%%")) {
    char replace[3] = { '%', '%', '\0' };
    char name[2] = { '%', '\0' };
    asm_string = Replace_Substring(asm_string, replace, name);
  }
#endif

  return asm_string;
}


/* Assemble a simulated OP.
 */
// removed static for minir
void
Assemble_Simulated_OP(OP *op, BB *bb)
{
  /* ASM is an odd case so we handle it specially. It doesn't expand
   * into a series of OPs, but rather we just emit the asm string into
   * the assembly output.
   */

  const char *stop_bit = AS_STOP_BIT;
  
  if (OP_code(op) == TOP_asm) {
    FmtAssert(Assembly && !Object_Code,
	      ("can't emit object code when ASM"));
    if (stop_bit && (EMIT_stop_bits_for_asm
		     || (EMIT_stop_bits_for_volatile_asm && OP_volatile(op)) ) )
    {
	fprintf(Asm_File, "\t%s\n", stop_bit);
    }
#ifdef TARG_MIPS
    const char *asm_string = Generate_Asm_String(op, bb);
    if (!CG_emit_non_gas_syntax)
      if (asm_string)
	if (strstr(asm_string, ".set"))
          fprintf (Asm_File, "\t.set\tpush\n");
#endif
    fprintf(Asm_File, "\t%s\n", Generate_Asm_String(op, bb));
#ifdef TARG_MIPS
    if (!CG_emit_non_gas_syntax)
      if (asm_string)
	if (strstr(asm_string, ".set"))
          fprintf (Asm_File, "\t.set\tpop\n");
#endif
    if (stop_bit && (EMIT_stop_bits_for_asm
		     || (EMIT_stop_bits_for_volatile_asm && OP_volatile(op)) ) )
    {
	fprintf(Asm_File, "\t%s\n", stop_bit);
    }
    return;
  }

  bool is_intrncall = OP_code(op) == TOP_intrncall;
  OPS ops = OPS_EMPTY;

  if (Trace_Inst) {
    fprintf (TFile, "<cgemit> transform simulated OP: ");
    Print_OP (op);
  }

  Exp_Simulated_Op (op, &ops, PC);
  if (is_intrncall && Object_Code) {
    Em_Add_New_Content (CK_NO_XFORM, PC, OPS_length(&ops)*4, 0, PU_section);
  }

  if (Trace_Inst) {
    fprintf (TFile, "... to: ");
    Print_OPS (&ops);
  }

  if (is_intrncall && Assembly) {
    ASM_DIR_NOTRANSFORM();
  }
  FOR_ALL_OPS_OPs_FWD (&ops, op) {
    ISA_BUNDLE bundle[ISA_PACK_MAX_INST_WORDS];
    INT words;
    Perform_Sanity_Checks_For_OP(op, FALSE);
    words = r_assemble_op (op, bb, bundle, 0);
    if (Object_Code) {
      /* write out the instruction. */
      Em_Add_Bytes_To_Scn (PU_section, (char *)&bundle,
			   INST_BYTES * words, INST_BYTES);
    }
  }
  if (is_intrncall && Assembly) {
    ASM_DIR_TRANSFORM();
  }
}

/* Assemble the OPs in a BB a bundle at a time.
 */
static void
Assemble_Bundles(BB *bb)
{
  OP *op;

  FmtAssert(ISA_MAX_SLOTS > 1,
	    ("Assemble_Bundles shouldn't have been called"));

  for (op = BB_first_op(bb);;) {
    ISA_BUNDLE bundle;
    UINT64 slot_mask;
    UINT stop_mask;
    INT slot;
    OP *slot_op[ISA_MAX_SLOTS];
    INT ibundle;

    /* Gather up the OPs for the bundle.
     */
    stop_mask = 0;
    slot_mask = 0;
    for (slot = 0; op && slot < ISA_MAX_SLOTS; op = OP_next(op)) {
      INT words;
      INT w;

      if (OP_dummy(op)) continue;		// these don't get emitted

      if (OP_simulated(op)) {
	FmtAssert(slot == 0, ("can't bundle a simulated OP in BB:%d.",BB_id(bb)));
	Assemble_Simulated_OP(op, bb);
	continue;
      }

      words = ISA_PACK_Inst_Words(OP_code(op));
      for (w = 0; w < words; ++w) {
	FmtAssert(slot < ISA_MAX_SLOTS, 
		  ("multi-word inst extends past end of bundle in BB:%d.",
		   BB_id(bb)));
        slot_op[slot++] = op;
        slot_mask = (slot_mask << ISA_TAG_SHIFT) | ISA_EXEC_Unit_Prop(OP_code(op));
        stop_mask = stop_mask << 1;
      }
      stop_mask |= (OP_end_group(op) != 0);

#ifndef GAS_TAGS_WORKED
// remove this when gas can handle tags inside explicit bundle
	if (OP_has_tag(op)) {
		fprintf(Asm_File, "\n%s:\n", LABEL_name(Get_OP_Tag(op)));
	}
#endif
    }
    if (slot == 0) break;

    // Emit the warning only when bundle formation phase is enabled (ON by
    // default).
    if (LOCS_Enable_Bundle_Formation) {
      FmtAssert(slot == ISA_MAX_SLOTS, ("not enough OPs for bundle in BB:%d\n",BB_id(bb)));
    }

    /* Determine template.
     */
    for (ibundle = 0; ibundle < ISA_MAX_BUNDLES; ++ibundle) {
      UINT64 this_slot_mask = ISA_EXEC_Slot_Mask(ibundle);
      UINT32 this_stop_mask = ISA_EXEC_Stop_Mask(ibundle);
      if (   (slot_mask & this_slot_mask) == this_slot_mask 
	  && stop_mask == this_stop_mask) break;
    }

    // Emit the warning only when bundle formation phase is enabled (ON by
    // default).
    if (LOCS_Enable_Bundle_Formation) {
      if (Trace_Inst && ibundle == ISA_MAX_BUNDLES) {
        Print_OP_No_SrcLine (slot_op[0]);
        Print_OP_No_SrcLine (slot_op[1]);
        Print_OP_No_SrcLine (slot_op[2]);
      }
      FmtAssert(ibundle != ISA_MAX_BUNDLES,
	       ("couldn't find bundle for slot mask=0x%" SCNx64 ", stop mask=0x%x in BB:%d\n",
	        slot_mask, stop_mask, BB_id(bb)));
    }

    /* Bundle prefix
     */
    const char *begin_bundle = ISA_PRINT_BEGIN_BUNDLE;
    
    if (Assembly && EMIT_explicit_bundles) {
      fputc (' ', Asm_File);
      fprintf(Asm_File, begin_bundle, ISA_EXEC_AsmName(ibundle));
      fputc ('\n', Asm_File);
    }

    /* Assemble the bundle.
     */
    slot = 0;
    do {
      OP *sl_op = slot_op[slot];
      Perform_Sanity_Checks_For_OP(sl_op, TRUE);
      slot += r_assemble_op(sl_op, bb, &bundle, slot);
    } while (slot < ISA_MAX_SLOTS);

    /* Bundle suffix
     */
    if (Object_Code) {
      TI_ASM_Set_Bundle_Comp(&bundle,
			     ISA_BUNDLE_PACK_COMP_template, 
			     ibundle);

      Em_Add_Bytes_To_Scn (PU_section, (char *)&bundle, INST_BYTES, INST_BYTES);
    }
    if (Assembly && EMIT_explicit_bundles) {
      fprintf(Asm_File, " %s", ISA_PRINT_END_BUNDLE);
    }
  }
  if (Assembly) {
    fputc ('\n', Asm_File);
  }
}

/* Assemble the OPs in a BB an OP at a time.
 */
static void
Assemble_Ops(BB *bb)
{
  OP *op;

  FmtAssert(ISA_MAX_SLOTS == 1,
	    ("Assemble_Ops shouldn't have been called"));

  FOR_ALL_BB_OPs_FWD(bb, op) {
    ISA_BUNDLE bundle[ISA_PACK_MAX_INST_WORDS];
    INT words;

    if (OP_dummy(op)) continue;		// these don't get emitted

    if (OP_simulated(op)) {
      Assemble_Simulated_OP(op, bb);
      continue;
    }

    Perform_Sanity_Checks_For_OP(op, TRUE);
    words = r_assemble_op(op, bb, bundle, 0);

    if (Object_Code) {
      Em_Add_Bytes_To_Scn(PU_section, (char *)bundle,
			  INST_BYTES * words, INST_BYTES);
    }
  }
}

/* ====================================================================
 *
 * Emit_Loop_Note
 *
 * Emit a loop note to the .s file, anl file, etc.
 *
 * ====================================================================
 */
static void
Emit_Loop_Note(BB *bb, FILE *file)
{
  BOOL anl_note = file == anl_file;
  BB *head = BB_loop_head_bb(bb);
  UINT16 unrollings = BB_unrollings(bb);
  ANNOTATION *info_ant = ANNOT_Get(BB_annotations(head), ANNOT_LOOPINFO);
  LOOPINFO *info = info_ant ? ANNOT_loopinfo(info_ant) : NULL;
  BOOL unroll_pragma = FALSE;
  ANNOTATION *unroll_ant = ANNOT_Get(BB_annotations(head), ANNOT_PRAGMA);

  while (unroll_ant && WN_pragma(ANNOT_pragma(unroll_ant)) != WN_PRAGMA_UNROLL)
    unroll_ant = ANNOT_Get(ANNOT_next(unroll_ant), ANNOT_PRAGMA);
  if (unroll_ant) {
    WN *wn = ANNOT_pragma(unroll_ant);
    if (WN_pragma_arg1(wn) > 1) {
      if (WN_pragma_arg1(wn) == unrollings)
	unroll_pragma = TRUE;
      else if (BB_innermost(bb))
	DevWarn("BB:%d unrolled %d times but pragma says to unroll %d times",
		BB_id(bb), unrollings, WN_pragma_arg1(wn));
    }
  }

  if (bb == head) {
#ifdef TARG_X8664
    if (CG_p2align) 
      fputs ("\t.p2align 6,,7\n", file);
    else if (CG_loop32) {
      if (BB_innermost(bb) && Is_Target_Barcelona()) {
        fputs ("\t.p2align 5,,\n", file);
      }
    }
#endif
    SRCPOS srcpos = BB_Loop_Srcpos(bb);
    INT32 lineno = SRCPOS_linenum(srcpos);

    if (anl_note) {
      INT32 fileno = SRCPOS_filenum(srcpos);
      INT32 colno = SRCPOS_column(srcpos);
      fprintf (anl_file,
	       "\nmsg loop lines [%d %d %d]",
	       fileno,
	       lineno,
	       colno);
    } else {
      fprintf (file, "%s<loop> Loop body line %d", ASM_CMNT_LINE, lineno);
    }

    if (info) {
      WN *wn = LOOPINFO_wn(info);
      TN *trip_tn = LOOPINFO_trip_count_tn(info);
      BOOL constant_trip = trip_tn && TN_is_constant(trip_tn);
      INT depth = WN_loop_depth(wn);
      const char *estimated = constant_trip ? "" : "estimated ";
      INT64 trip_count = constant_trip ? TN_value(trip_tn) :
					 (INT64)WN_loop_trip_est(wn);
      const char * const fmt =   anl_note
			? " \"nesting depth: %d, %siterations: %" SCNd64 "\""
			: ", nesting depth: %d, %siterations: %" SCNd64 "";

      fprintf (file, fmt, depth, estimated, trip_count);
    }

    fputc ('\n', file);
  } else if (anl_note) {

    /* Only interested in loop head messages for anl file
     */
    return;
  } else {
    ANNOTATION *lbl_ant = ANNOT_Get(BB_annotations(head), ANNOT_LABEL);
    DevAssert(lbl_ant, ("loop head BB:%d has no label", BB_id(head)));
    fprintf(file,
	    "%s<loop> Part of loop body line %d"
	    ", head labeled %s\n",
	    ASM_CMNT_LINE, BB_Loop_Lineno(head), LABEL_name(ANNOT_label(lbl_ant)));
  }

  if (unrollings > 1) {
    if (anl_note) {
      fprintf(anl_file, "\"unrolled %d times%s%s\"\n", unrollings,
	      BB_unrolled_fully(bb) ? " (fully)" : "",
	      unroll_pragma ? " (pragma)" : "");
    } else {
      fprintf(file, "%s<loop> unrolled %d times%s%s\n", 
	      ASM_CMNT_LINE,
	      unrollings,
	      BB_unrolled_fully(bb) ? " (fully)" : "",
	      unroll_pragma ? " (pragma)" : "");
    }
  }
}

/* ====================================================================
 *
 * EMT_Assemble_BB
 *
 * Assemble the contents of the given BB.
 *
 * ====================================================================
 */

static void
EMT_Assemble_BB ( BB *bb, WN *rwn )
{
  ST *st;
  ANNOTATION *ant;
  RID *rid = BB_rid(bb);

#ifdef TARG_X8664 
  if (!BB_entry(bb)) {
    float fall_thru_freq = 0.0;
    float branch_in_freq = 0.0;
    BBLIST *edge;
    BB *fall_thru_pred = BB_Fall_Thru_Predecessor(bb);
    FOR_ALL_BB_PREDS(bb, edge) {
      BB* pred = BBLIST_item(edge);
      BBLIST *succ_edge = BB_Find_Succ(pred, bb);
      FmtAssert(succ_edge != NULL, ("EMT_Assemble_BB: succ bb not found"));
      if (pred == fall_thru_pred) {
	fall_thru_freq = BB_freq(pred) * BBLIST_prob(succ_edge);
      } else {
	branch_in_freq += BB_freq(pred) * BBLIST_prob(succ_edge);
      }
    }

    int max_skip_bytes = CG_p2align_max_skip_bytes;
    float branch_in_ratio = branch_in_freq / fall_thru_freq;
    bool add_p2align = FALSE;

    // bug 2191
    if (branch_in_freq > 100000000.0 &&
	branch_in_ratio > 50.0) {
      max_skip_bytes = 15;
      add_p2align = TRUE;
    }

    if (add_p2align ||
	(CG_p2align_freq > 0 &&
	 branch_in_freq > CG_p2align_freq &&
	 branch_in_ratio > 0.5)) {
      fprintf(Asm_File, "\t.p2align 4,,%d\n", max_skip_bytes);
    }
  }
#endif

  if (Trace_Inst) {
	#pragma mips_frequency_hint NEVER
	fprintf(TFile, "assemble BB %d\n", BB_id(bb));
  }
  if (Assembly) {
    if (rid != NULL && RID_cginfo(rid) != NULL) {
	if (current_rid == RID_id(rid)) {
		/* same, so do nothing */
	}
	else if (current_rid > 0) {
		/* changing regions */
		fprintf (Asm_File, "%s END REGION %d\n",
				    ASM_CMNT_LINE, current_rid);
		if (RID_id(rid) > 0 && !RID_is_glue_code(rid)) {
		    fprintf (Asm_File, "%s BEGIN REGION %d\n",
					ASM_CMNT_LINE, 	RID_id(rid));
		    current_rid = RID_id(rid);
		}
		else if (RID_is_glue_code(rid)) {
		    current_rid = 0;	/* pretend not a region */
		}
		else {
		    current_rid = RID_id(rid);
		}
	}
	else if (RID_id(rid) > 0 && !RID_is_glue_code(rid)) {
		/* beginning a region */
		fprintf (Asm_File, "%s BEGIN REGION %d\n", 
				   ASM_CMNT_LINE, RID_id(rid));
		current_rid = RID_id(rid);
	}
    }
    else if (current_rid > 0) {
	/* ending a region */
	fprintf (Asm_File, "%s END REGION %d\n", ASM_CMNT_LINE, current_rid);
	current_rid = 0;	/* no rid */
    }
  }
  if ( BB_entry(bb) ) {
#ifndef KEY
    // replace uses of entry_name with ST_name(entry_sym)
    char *entry_name;
#endif
    ST *entry_sym; 
    ENTRYINFO *ent;
    SRCPOS entry_srcpos;
    ant = ANNOT_Get (BB_annotations(bb), ANNOT_ENTRYINFO);
    ent = ANNOT_entryinfo(ant);
    entry_srcpos = ENTRYINFO_srcpos(ent);
    entry_sym = ENTRYINFO_name(ent);
#ifndef KEY
    // replace uses of entry_name with ST_name(entry_sym)
    entry_name = ST_name(entry_sym);
#endif

#ifdef KEY
    if (strcmp(ST_name(entry_sym), Cur_PU_Name) != 0 && 
	PU_ftn_lang(Get_Current_PU())) { // PU entry
      char *buf;
      LABEL *label;
      buf = (char *)alloca(strlen(ST_name(entry_sym)) + /* EXTRA_NAME_LEN */ 32);
      sprintf(buf, ".LDWend_%s", ST_name(entry_sym));
      label = &New_LABEL(CURRENT_SYMTAB, Label_Last_BB_PU_Entry[pu_entries+1]);
      LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
      if (pu_entries > 0) 
	fprintf( Asm_File, 
		 "%s:\n", LABEL_name(Label_Last_BB_PU_Entry[pu_entries]));
    }
#endif
    /* Set an initial line number so that if the first inst in the BB
     * has no srcpos, then we'll be ok.
     */
    if (entry_srcpos) Cg_Dwarf_Add_Line_Entry (PC, entry_srcpos);

    if ( ST_is_not_used(entry_sym)) {
    	// don't emit alt-entry if marked as not-used
	DevWarn("CG reached entry marked as unused; will ignore it (%s)\n", 
		ST_name(entry_sym)); // KEY
    }
    else {
    	Set_ST_ofst(entry_sym, PC);
    	if (strcmp( Cur_PU_Name, ST_name(entry_sym) ) != 0) /* KEY */{ 
		// alt-entry
      		if ( Assembly ) {
			fprintf ( Asm_File, "\t%s\t%s\n", AS_AENT, ST_name(entry_sym)); // KEY
			Print_Label (Asm_File, NULL, entry_sym, 0 );
      		}
		EMT_Put_Elf_Symbol (entry_sym);
      		if ( Object_Code ) {
        		Em_Define_Symbol (
			    EMT_Put_Elf_Symbol(entry_sym), PC, 0, PU_section);
			Em_Add_New_Event (EK_ENTRY, PC, 0, 0, 0, PU_section);
      		}
    	}
    	if (Object_Code) {
      		if ( EMIT_interface_section && !BB_handler(bb))
		   Interface_Scn_Add_Def( entry_sym, rwn );
    	}
    }
  }

  /* List labels attached to BB: */
  for (ant = ANNOT_First (BB_annotations(bb), ANNOT_LABEL);
       ant != NULL;
       ant = ANNOT_Next (ant, ANNOT_LABEL))
  {
    LABEL_IDX lab = ANNOT_label(ant);

    if ( Assembly ) {
#ifdef KEY

      // bug 14483: It's not clear why we need to avoid emitting labels
      // for ASM bbs, other than reducing the size of the assembly file.
      // So the code below is disabled. If we really need this optimization,
      // the rest of CG should say which labels are actually required,
      // and remove the rest.
#if 0 /* KEY */
      // We do not want to emit labels between consecutive asms.
      // And, we know each asm ends a BB.

      /* We need to emit label for bb that has a label used to mark the
	 beginning or ending of an exception-handling region or exception handler.
	 (bug#3068)
      */
      BOOL emit_label = TRUE;
      if( !BB_Has_Exc_Label(bb) ){
	if (/* Bug 3815 - this was probably added to avoid generating
	     * too many asm labels. But we don't need to worry about 
	     * it until we hit glibc kind of code. Avoid this under 
	     * 0 optimization level when we can have code snippets like
	     * jmp .Lt_0_19 followed by the BB labeled .Lt_0_19. 
	     * I don't remember why we need to avoid generating extra labels 
	     * in the first place.
	     */
	    CG_opt_level > 0 && 
	    BB_preds(bb) &&
	    !BB_entry(bb) &&
	    (BBlist_Len(BB_preds(bb)) == 1) &&
	    (BBLIST_item(BB_preds(bb)) == bb->prev) &&
	    (bb->ops.length == 1) &&
	    BB_asm(bb))
	  emit_label = FALSE;
      }

      if (emit_label)
#endif
        fprintf ( Asm_File, "%s:\n", LABEL_name(lab)); 
#else
      fprintf ( Asm_File, "%s:\t%s 0x%" SCNx64 "\n", 
			  LABEL_name(lab), ASM_CMNT, Get_Label_Offset(lab) );
#endif
    }
#ifdef KEY
    static BOOL seen_asm = FALSE;
    static WN *rwn_tmp = NULL;
    if (rwn_tmp != rwn) {
      rwn_tmp = rwn;
      seen_asm = FALSE;      
    }
    if (BB_asm(bb)) { 
      seen_asm = TRUE;
    }            
#endif
#ifndef TARG_IA64
    if (Get_Label_Offset(lab) != PC) {
#ifdef KEY
      if (!seen_asm)
#endif
	DevWarn ("label %s offset %" SCNd64 " doesn't match PC %d", 
		LABEL_name(lab), Get_Label_Offset(lab), PC);
    }
#endif
  }

  // hack to keep track of last label and offset for assembly dwarf (suneel)
  if (Last_Label == LABEL_IDX_ZERO) {
    Last_Label = Gen_Label_For_BB (bb);
    Offset_From_Last_Label = 0;
    if (Initial_Pu_Label == LABEL_IDX_ZERO) {
      Initial_Pu_Label = Last_Label;
    }
  }

  st = BB_st(bb);
  if (st) {
    if ( Assembly ) {
      fprintf ( Asm_File, "%s:\t%s 0x%" SCNx64 "\n", ST_name(st), ASM_CMNT, ST_ofst(st));
    }
    Is_True (ST_ofst(st) == PC, ("st %s offset %" SCNd64 " doesn't match PC %d", 
	ST_name(st), ST_ofst(st), PC));
    Is_True (   !Has_Base_Block(st) 
	     || (ST_base(st) == (BB_cold(bb) ? cold_base : text_base)),
	     ("sym %s base doesn't match BB:%d",
	     ST_name(st), BB_id(bb)));
    FmtAssert(ST_is_export_local(st),
	      ("ST for BB:%d not EXPORT_LOCAL", BB_id(bb)));
  }

  /* write out all the notes for this BB */
  if (Assembly && List_Notes) {
    if (BB_loop_head_bb(bb)) {
      Emit_Loop_Note(bb, Asm_File);
    }
    if (BB_annotations(bb) && 
	ANNOT_Get(BB_annotations(bb), ANNOT_ROTATING_KERNEL)){
#ifdef TARG_IA64
      Emit_SWP_Note(bb, Asm_File);
#endif
    }

    if (BB_has_note(bb)) {
      NOTE_BB_Act (bb, NOTE_PRINT_TO_FILE, Asm_File);
    }

    FREQ_Print_BB_Note(bb, Asm_File);
  }
  if (Run_prompf) {
    if (BB_loop_head_bb(bb)) {
      Emit_Loop_Note(bb, anl_file);
    }

    if (BB_has_note(bb)) {
      NOTE_BB_Act (bb, NOTE_PRINT_TO_ANL_FILE, anl_file);
    }
  }

#if Is_True_On
  Init_Sanity_Checking_For_BB ();
#endif

#ifdef ICE9A_HW_WORKAROUND
  // Initialize register states for fp stall bug workaround.  Bug 13481.
  for (int reg = 1; reg <= 32; reg++) {
    fp_reg_state[reg] = PENDING;
  }
  // Dedicate f23 as movn.d destination.  f23 is removed from the allocatable
  // set.
  fp_reg_state[24] = OK;	// 24 is f23
#endif

#ifdef KEY
// Assumption: REGION_First_BB is the first BB in the PU. If in future,
// we start having multiple regions in a PU here, we need to change the 
// following. 
// In such a scenario, we should need to change many of the occurrences
// of REGION_First_BB above.
  if(
#ifdef KEY
      !CG_emit_unwind_info &&
#endif
      Assembly && (REGION_First_BB == bb) &&
      ( strcmp( Cur_PU_Name, "MAIN__" ) == 0 ||
   	strcmp( Cur_PU_Name, "main" ) == 0 ) ){
      CGEMIT_Setup_Ctrl_Register( Asm_File );
  }
#endif

#ifdef KEY
  // bug 14724: Following initializations should be done only for entry
  // BBs that are not C++ exception handlers to prevent wrong increment of
  // pu_entry_count and overwriting original entry-BB label information.
  // Any SP adjustment in handler need not be tracked.
  if (BB_entry(bb) && CG_emit_unwind_info && !BB_handler(bb)) {
    ANNOTATION *ant = ANNOT_Get(BB_annotations(bb), ANNOT_ENTRYINFO);
    ENTRYINFO *ent = ANNOT_entryinfo(ant);
    ST *entry_sym = ENTRYINFO_name(ent);

    pu_entry_count++;
    if (!strcmp(ST_name(entry_sym), Cur_PU_Name)) {
      pu_entries = 0;			// reinitialize
    } else if (PU_ftn_lang(Get_Current_PU())) {
      pu_entries++;
    }

    //bug 11034 : dynamic allocation and reallocation if necessary
    if (!dw_pu_tables_allocated) {	// this should not happen
      Is_True(FALSE, ("EMT_Assemble_BB: dwarf pu tables not allocated."));
      init_dwarf_pu_tables();
    }
    if (pu_entries >= num_pus-1)	// not enough entry in the tables
      double_dwarf_pu_tables();		// double the size of pu_tables

    Label_First_BB_PU_Entry[pu_entries] = Gen_Label_For_BB(bb);
    FmtAssert(pu_entries < num_pus-1,
      ("EMT_Assemble_BB: Too many pu's, pu_tables should have been expanded."));
  }
#endif

  if (ISA_MAX_SLOTS > 1) {
    Assemble_Bundles(bb);
  } else {
    Assemble_Ops(bb);
  }

  if (Object_Code && BB_exit(bb)) {
    Em_Add_New_Event (EK_EXIT, PC - 2*INST_BYTES, 0, 0, 0, PU_section);
  }
} 


/* Routines to handle long branches. */

/* The farthest distance, in bytes, we will look back for a BB without 
 * a fallthrough
 */
#define FUDGE_ZONE 16000

static INT num_longb_inst_words;

typedef struct {
  BB                *bb_ptr;
  INT32              bb_inum;
  struct longb_info *bb_longb;	/* First long branch from this bb */
} BB_INFO;

static BB_INFO *bb_info;

typedef struct longb_info {
  INT bbindex;
  INT32 longb_inum;		// relative pc of long-branch
  INT32 distance;
  OP *op;
  UINT8 opnd;
  struct longb_info *next;
} LONGB_INFO;

typedef struct stub_info {
  struct stub_info *next;
  BB *stub;
  INT32 stub_loc;
  BB *targ;
  INT64 offset;
} STUB_INFO;


/* ====================================================================
 *
 * Find_Stub_BB
 *
 * Given a long branch, look at the already created stubs and determine
 * if we can just branch to one of them. To avoid some sticky problems,
 * we only consider stubs that are in the same direction as the
 * ultimate target is from this branch.
 *
 * ====================================================================
 */
static BB *
Find_Stub_BB (const LONGB_INFO *longb, const STUB_INFO *stubs)
{
  const STUB_INFO *stub;
  TN *tn = OP_opnd(longb->op, longb->opnd);
  BB *targ = Get_Label_BB(TN_label(tn));
  INT64 offset = TN_offset(tn);

  for (stub = stubs; stub; stub = stub->next) {
    INT32 stub_distance;
    if (stub->targ != targ || stub->offset != offset) continue;
    stub_distance = stub->stub_loc - longb->longb_inum;
    if (stub_distance == 0) continue;
    if ((longb->distance ^ stub_distance) < 0) continue;
    if (stub_distance < 0) stub_distance = -stub_distance;
    if (stub_distance < EMIT_Long_Branch_Limit) return stub->stub;
  }

  return NULL;
}


static BB *
Create_Stub_BB (LONGB_INFO *longb, INT *stub_loc, BOOL fwd_branch)
{
  INT save_idx;
  BB *next_bb = NULL;
  INT idx = longb->bbindex;
  BB *orig_bb = bb_info[idx].bb_ptr;
  BB *stub_bb = Gen_BB_Like (orig_bb);

  if (BB_cold(orig_bb)) Set_BB_cold(stub_bb);

  if (Trace_Longbr) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "Create_Stub_BB: idx: %d, stub_loc: %d, fwd:%d\n", 
			  idx, *stub_loc, fwd_branch);
  }

  /* Search for a BB in the fudge zone that does not fall through to
   * its successor. If we find one, we insert there and we don't have to
   * worry about keeping anyone from falling through to the inserted
   * BB. The search is done in such a way as to always check the
   * first candidate BB even if it is outside of the fudge zone
   * (which would happen it its size was >= FUDGE_ZONE). We do this
   * because this is where we will insert the stub and we still want
   * to know if a branch around the stub is necessary.
   */
  if (fwd_branch) {
    while (bb_info[++idx].bb_inum < *stub_loc);
    save_idx = --idx;
    do {
      BB *bb = bb_info[idx].bb_ptr;
      BB *bb_prev = BB_prev(bb);

      /* BB_prev(bb_info[idx].bb_ptr) is expected never to be NULL,
       * but there's really no choice of what to do if it
       * is. Therefore rather than assert, we go ahead and check for
       * it, doing the right thing if we happen to find NULL.
       */
      if (bb_prev != NULL && !BB_in_preds(bb, bb_prev)) {
	next_bb = bb;
	save_idx = idx;
	break;
      }
    } while (--idx >= 0 && bb_info[idx].bb_inum > (*stub_loc - FUDGE_ZONE));
  }
  else {
    while (bb_info[--idx].bb_inum > *stub_loc); 
    save_idx = ++idx;
    do {
      BB *bb = bb_info[idx].bb_ptr;
      BB *bb_prev = BB_prev(bb);

      /* BB_prev(bb_info[idx].bb_ptr) is expected never to be NULL,
       * but there's really no choice of what to do if it
       * is. Therefore rather than assert, we go ahead and check for
       * it, doing the right thing if we happen to find NULL.
       */
      if (bb_prev != NULL && !BB_in_preds(bb, bb_prev)) {
	next_bb = bb;
	save_idx = idx;
	break;
      }
    } while (   ++idx < PU_BB_Count
	     && bb_info[idx].bb_inum < (*stub_loc + FUDGE_ZONE));
  }
  if (Trace_Longbr) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "next_bb: %p, save_idx: %d\n", next_bb, save_idx);
  }
  if (next_bb == NULL) {

    /* No blocks were found in the fudge zone where we could avoid
     * placing a branch around the stub. So insert the stub as far
     * away as possible and insert a branch around the stub
     * BB to avoid falling through to it.
     */
    BB *branch_around_bb;
    BBLIST *item;
    TN *label_tn;
    ANNOTATION *ant;
    float prob;
    OP *new_op;
    OPS new_ops = OPS_EMPTY;

    next_bb = bb_info[save_idx].bb_ptr;

    Is_True(BB_prev(next_bb) && BB_in_preds(next_bb, BB_prev(next_bb)),
	    ("branch around stub not required"));

    branch_around_bb = Gen_BB_Like (orig_bb);
    if (BB_cold(orig_bb)) Set_BB_cold(branch_around_bb);

    if (Trace_Longbr) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "Inserting a branch around the stub bb\n");
      fprintf (TFile, "Looking for BB:%d among preds of BB:%d\n",
	       BB_id(BB_prev(next_bb)), BB_id(next_bb));
      Print_BB_Header(next_bb, TRUE, FALSE);
    }

    /* Set branch_around_bb's frequency to the frequency of the edge
     * from its predecessor to its successor.
     */
    FOR_ALL_BB_PREDS(next_bb, item) {
      if (BBLIST_item(item) == BB_prev(next_bb)) {
	prob = BBLIST_prob(item);
	BB_freq(branch_around_bb) = BB_freq(BBLIST_item(item)) * prob;
        break;
      }
    }
    FmtAssert(item != NULL,
              ("Can't insert branch_around_bb; "
               "incomplete fallthrough pred/succ"));

    if (BB_freq_fb_based(BB_prev(next_bb)))
      Set_BB_freq_fb_based(branch_around_bb);
    Insert_BB (branch_around_bb, BB_prev(next_bb));

    /* The following is somewhat inefficient, considering that to do
     * part of the work, we could simply go through the appropriate
     * lists and update the BBLIST_item fields in the relevant BBLIST
     * nodes. But it's better to stick to the common interface.
     */
    Unlink_Pred_Succ(BB_prev(branch_around_bb), next_bb);
    Link_Pred_Succ_with_Prob(BB_prev(branch_around_bb), branch_around_bb, prob);
    Link_Pred_Succ_with_Prob(branch_around_bb, next_bb, 1.0);

    ant = ANNOT_Get (BB_annotations(next_bb), ANNOT_LABEL);
    label_tn = Gen_Label_TN (ANNOT_label(ant), 0);
    Exp_OP1 (OPC_GOTO, NULL, label_tn, &new_ops);
    new_op = OPS_last(&new_ops);
    OP_scycle(new_op) = 0;
    BB_Append_Op(branch_around_bb, new_op);
    num_longb_inst_words += OP_Real_Inst_Words(new_op);
    if (PROC_has_branch_delay_slot()) {
      Exp_Noop(&new_ops);
      new_op = OPS_last(&new_ops);
      OP_scycle(new_op) = 1;
      BB_Append_Op(branch_around_bb, new_op);
      num_longb_inst_words += OP_Real_Inst_Words(new_op);
    }

    if (Trace_Longbr) {
      #pragma mips_frequency_hint NEVER
      fprintf(TFile, "Previous BB:\n");
      Print_BB_Header(BB_prev(branch_around_bb), TRUE, FALSE);
      fprintf(TFile, "Inserted branch_around_bb:\n");
      Print_BB_Header(branch_around_bb, TRUE, FALSE);
      fprintf(TFile, "Next BB:\n");
      Print_BB_Header(next_bb, TRUE, FALSE);
      fprintf(TFile, "====== end of branch-around insertion ========\n");
    }
  }
  *stub_loc = bb_info[save_idx].bb_inum;
  if (Trace_Longbr) {
    #pragma mips_frequency_hint NEVER
    ANNOTATION *ant = ANNOT_Get (BB_annotations(next_bb), ANNOT_LABEL);
    fprintf (TFile, "Inserting stub bb before bb: %s\n", 
		LABEL_name(ANNOT_label(ant)));
  }
  Insert_BB (stub_bb, BB_prev(next_bb));
  return stub_bb;
}

static void
Recompute_Label_Offset(INT32 pcs[2])
{
  BB *bb;
  INT32 cur_pcs[2];

  /* recompute the addresses of all labels in the procedure. */
  cur_pcs[IHOT] = text_PC;
  cur_pcs[ICOLD] = cold_PC;
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    ANNOTATION *ant;
    OP *op;
    INT isect = BB_cold(bb) ? ICOLD : IHOT;
    INT32 cur_pc = cur_pcs[isect];
    LABEL_IDX lab;
    ST *st = BB_st(bb);
    if (st) {
      Set_ST_ofst(st, cur_pc);
      Set_ST_base(st, BB_cold(bb) ? cold_base : text_base);
    }
    for (ant = ANNOT_First (BB_annotations(bb), ANNOT_LABEL);
         ant != NULL;
         ant = ANNOT_Next (ant, ANNOT_LABEL))
    {
      lab = ANNOT_label(ant);
      Set_Label_Offset(lab, cur_pc);
    }
    for (op = BB_first_op(bb); op; op = OP_next(op)) {
      if (OP_has_tag(op)) {
	lab = Get_OP_Tag(op);
      	Set_Label_Offset(lab, cur_pc);
      }
      INT num_inst_words = OP_Real_Inst_Words (op);
      cur_pc = PC_Incr_N(cur_pc, num_inst_words);
    }
    cur_pcs[isect] = cur_pc;
  }

  Is_True(cur_pcs[IHOT] == pcs[IHOT],
	 ("Recompute_Label_Offsets: hot region PC mismatch (%d vs %d).",
	  cur_pcs[IHOT], pcs[IHOT]));
  Is_True(cur_pcs[ICOLD] == pcs[ICOLD],
	 ("Recompute_Label_Offsets: cold region PC mismatch (%d vs %d).",
	  cur_pcs[ICOLD], pcs[ICOLD]));
}

/* Takes a pointer to the bb_info array, the array length, and the
 * index of the (unique) element that could be out of place. Restores
 * the sort by bb_inum.
 */

static INT
BB_Info_Maintain_Sort (BB_INFO *bb_info,
		       INT      n_bb_info,
		       INT      bbindex,
		       INT      stub_len)

{
  INT i = bbindex + 1;

  FmtAssert(bbindex < n_bb_info,
	    ("Reference to BB:%2d out of range", bbindex));

  /* If we wanted to be really fancy, the following loop would be
   * binary search.
   */
  for ( ;
       i < n_bb_info &&
       bb_info[bbindex].bb_inum > bb_info[i].bb_inum;
       ++i) ;
  /* If we need to move the bbindex'th element upward, its destination
   * is just to the left of i.
   *
   * If we don't need to move upward, i == bbindex + 1.
   */
  if (i == bbindex + 1) {
    for ( ;
	 i > 0 &&
	 bb_info[bbindex].bb_inum <= bb_info[i - 1].bb_inum;
	 --i) ;
    ++i;
  }
  /* Now the bbindex'th element's destination is just to the left of
   * i, regardless of anything else.
   */
  if (i != bbindex + 1) {
    BB_INFO            temp = bb_info[bbindex];
    LONGB_INFO *longb_list;

    while (i <= bbindex) {
      Is_True(bb_info[bbindex - 1].bb_inum >= temp.bb_inum,
	      ("Moving too low a BB upward"));
      bb_info[bbindex] = bb_info[bbindex - 1];
      if ((longb_list = bb_info[bbindex - 1].bb_longb) != NULL) {
	for (Is_True(longb_list->bbindex == bbindex - 1,
		     ("Rotate right: longb_list->bbindex == %d; should be %d",
		      longb_list->bbindex, bbindex - 1));
	     (longb_list != NULL) && (longb_list->bbindex == bbindex - 1);
	     longb_list = longb_list->next) {
	  ++(longb_list->bbindex);
	}
      }
      --bbindex;
    }
    bb_info[bbindex] = temp;
    while (i > bbindex + 1) {
      Is_True(bb_info[bbindex + 1].bb_inum < temp.bb_inum,
	      ("Moving too high a BB downward"));
      bb_info[bbindex] = bb_info[bbindex + 1];
      if ((longb_list = bb_info[bbindex + 1].bb_longb) != NULL) {
	for (Is_True(longb_list->bbindex == bbindex + 1,
		     ("Rotate left: longb_list->bbindex == %d; should be %d",
		      longb_list->bbindex, bbindex + 1));
	     (longb_list != NULL) && (longb_list->bbindex == bbindex + 1);
	     longb_list = longb_list->next) {
	  --(longb_list->bbindex);
	}
      }
      ++bbindex;
    }
    bb_info[bbindex] = temp;
  }

  /* Adjust the bb_inum of those entries pushed upward by inserting
   * the stub BB.
   */
  for (i = bbindex + 1; i < n_bb_info; ++i) {
    bb_info[i].bb_inum += stub_len;
  }
  return bbindex;
}


/* ====================================================================
 *
 * Compare_Long_Branches
 *
 * qsort comparison function for sorting long branches.  The goal of the 
 * sort is to group all branches with the same target together, with the 
 * branches spanning the largest distance coming first. In addition, the 
 * top level sorting critera is the direction of the branch. All forward
 * branches come first.
 *
 * ====================================================================
 */
static INT
Compare_Long_Branches(const void *p1, const void *p2)
{
  /* qsort claims to sort things in ascending order, but in reality
   * it is sorting based on how this comparison function classifies
   * the relationship. Define some constants that make that clear.
   */
  enum {
    sort_1_before_2 = -1, 
    sort_1_after_2  = 1, 
    sort_1_same_2   = 0 
  };
  LONGB_INFO *lb1 = *(LONGB_INFO **)p1;
  LONGB_INFO *lb2 = *(LONGB_INFO **)p2;
  INT32 dist1 = lb1->distance;
  INT32 dist2 = lb2->distance;
  INT32 targ1 = lb1->longb_inum + dist1;
  INT32 targ2 = lb2->longb_inum + dist2;

  if ((dist1 ^ dist2) < 0) {
    /* one forward and one backward branch
     */
    return dist1 < 0 ? sort_1_after_2 : sort_1_before_2;
  } else if (dist1 < 0) {
    /* two backward branches
     */
    if (targ1 > targ2) {
      return sort_1_after_2;
    } else if (targ1 < targ2) {
      return sort_1_before_2;
    } else {
      Is_True(dist1 != dist2, ("two branches from the same address???"));
      return dist1 > dist2 ? sort_1_before_2 : sort_1_after_2;
    }
  } else {
    /* two forward branches
     */
    if (targ1 > targ2) {
      return sort_1_before_2;
    } else if (targ1 < targ2) {
      return sort_1_after_2;
    } else {
      Is_True(dist1 != dist2, ("two branches from the same address???"));
      return dist1 > dist2 ? sort_1_before_2 : sort_1_after_2;
    }
  }
}


/* ====================================================================
 *
 * Sort_Long_Branches
 *
 * Sort long branches to help streamline things when we have multiple
 * long branches to the same target. See Compare_Long_Branches for
 * description of the sorted order.
 *
 * ====================================================================
 */
static LONGB_INFO *
Sort_Long_Branches(INT num_longb, LONGB_INFO *longb_list)
{
  LONGB_INFO *longb;
  INT i;
  LONGB_INFO **vec = (LONGB_INFO **)alloca(sizeof(LONGB_INFO *) * num_longb);

  /* Get the linked list into a vector we can sort.
   */
  longb = longb_list;
  for (i = 0; i < num_longb; ++i) {
    vec[i] = longb;
    longb = longb->next;
  }

  qsort(vec, num_longb, sizeof(LONGB_INFO *), Compare_Long_Branches);

  /* Re-create the linked list from the sorted vector.
   */
  longb = NULL;
  for (i = num_longb - 1; i >= 0; --i) {
    LONGB_INFO *vec_i = vec[i];
    vec_i->next = longb;
    longb = vec_i;
  }
  return longb;
}


/* ====================================================================
 *
 * Print_Long_Branches
 *
 * Print the info for the long branches to the trace file.
 *
 * ====================================================================
 */
static void
Print_Long_Branches(LONGB_INFO *longb_list)
{
  LONGB_INFO *longb;
  for (longb = longb_list; longb; longb = longb->next) {
    fprintf(TFile, "  bbindex=%d (BB:%d)"
		   ", longb_inum=%d"
		   ", distance=%d"
		   ", target=%d\n",
		   longb->bbindex, BB_id(bb_info[longb->bbindex].bb_ptr),
		   longb->longb_inum,
		   longb->distance,
		   longb->longb_inum + longb->distance);
  }
}


static void
Fixup_Long_Branches (INT32 *hot_size, INT32 *cold_size)
{
  BB *bb;
  OP *op;
  INT32 cur_pcs[2];
  INT32 start_pcs[2];
  TN *t;
  INT i;
  INT num_bbs;
  INT num_longb;
  LONGB_INFO *longb_list, *new_longb;
  STUB_INFO *stubs;

  Trace_Longbr = Get_Trace (TP_EMIT, 0x800);
  num_longb_inst_words = 0;
  bb_info = (BB_INFO *) alloca ((PU_BB_Count + 2) * sizeof (*bb_info));
  num_bbs = 0;
  num_longb = 0;
  longb_list = NULL;
  cur_pcs[IHOT] = start_pcs[IHOT] = text_PC;
  cur_pcs[ICOLD] = start_pcs[ICOLD] = cold_PC;
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    INT isect = BB_cold(bb) ? ICOLD : IHOT;
    INT32 cur_pc = cur_pcs[isect];
    INT32 start_pc = start_pcs[isect];
    bb_info[num_bbs].bb_ptr = bb;
    bb_info[num_bbs].bb_inum = cur_pc - start_pc;
    bb_info[num_bbs].bb_longb = NULL;
    for (op = BB_first_op(bb); op; op = OP_next(op)) {
      INT num_inst_words = OP_Real_Inst_Words (op);
      if (OP_xfer(op)) {
        for (i = 0; i < OP_opnds(op); i++) {
	  t = OP_opnd(op,i);
	  if (TN_is_label(t) && OP_opnd_is_pcrel(op,i)) {
	    LABEL_IDX lab = TN_label(t);
	    INT64 val = Get_Label_Offset(lab) + TN_offset(t) - PC_Bundle(cur_pc);
	    if (PROC_has_branch_delay_slot()) val -= sizeof(ISA_PACK_INST);
	    if (val > -EMIT_Long_Branch_Limit && val < EMIT_Long_Branch_Limit) break;
	    if (Trace_Longbr) {
	      #pragma mips_frequency_hint NEVER
	      fprintf (TFile, "Found a long branch to %s, ", LABEL_name(lab));
	      fprintf (TFile, "location: %d, distance: %" SCNd64 "\n", 
		      cur_pc - start_pc, val);
	    }
	    new_longb = (LONGB_INFO *)alloca (sizeof(LONGB_INFO));
	    new_longb->bbindex = num_bbs;
	    new_longb->longb_inum = cur_pc - start_pc;
	    new_longb->distance = val;
	    new_longb->op = op;
	    new_longb->opnd = i;
	    new_longb->next = longb_list;
	    longb_list = new_longb;
	    bb_info[num_bbs].bb_longb = new_longb;
	    ++num_longb;
	  }
	}
      }
      else if (OP_branch_predict(op)) {
	// for branch predicts with one or more pc-relative operands,
	// if the target is too far away to fit in the instruction
	// then since not needed for correctness, just replace with a nop.
        for (i = 0; i < OP_opnds(op); i++) {
	  t = OP_opnd(op,i);
	  if ((TN_is_label(t) || TN_is_tag(t)) && OP_opnd_is_pcrel(op,i)) {
            LABEL_IDX lab = TN_label(t);
            INT64 val = Get_Label_Offset(lab) + TN_offset(t) - PC_Bundle(cur_pc);
	    if ( ! ISA_LC_Value_In_Class (val, OP_opnd_lit_class(op, i))) {
	      // replace brp with nop
	      OP *new_op;
	      OPS new_ops;
	      OPS_Init(&new_ops);
	      DevWarn("%s label %s doesn't fit; replace with nop",
		      TOP_Name(OP_code(op)), LABEL_name(lab));
	      Exp_Noop(&new_ops);
	      new_op = OPS_last(&new_ops);
	      OP_scycle(new_op) = OP_scycle(op);
	      // now make a specific nop (if has multiple nops)
	      OP_Change_Opcode(new_op, 
			CGTARG_Noop_Top (ISA_EXEC_Unit_Prop(OP_code(op))) );
	      BB_Insert_Op_After (bb, op, new_op);
	      BB_Remove_Op (bb, op);
	      op = new_op;	// so op->next is correct
	      break;
	    }
	  }
	}
      }
      cur_pc = PC_Incr_N(cur_pc, num_inst_words);
    }
    cur_pcs[isect] = cur_pc;
    num_bbs++;
  }

  longb_list = Sort_Long_Branches(num_longb, longb_list);
  if (Trace_Longbr) {
    #pragma mips_frequency_hint NEVER
    fprintf(TFile, "\nLong branches after sorting:\n");
    Print_Long_Branches(longb_list);
  }

  stubs = NULL;
  for (; longb_list != NULL; longb_list = longb_list->next) {
    BOOL fwd_branch = (longb_list->distance > 0);
    INT br_limit = (fwd_branch ? EMIT_Long_Branch_Limit : -EMIT_Long_Branch_Limit);
    INT stub_loc = longb_list->longb_inum + br_limit;
    INT tgt_loc = longb_list->longb_inum + longb_list->distance;
    TN *tgt_tn = OP_opnd(longb_list->op, longb_list->opnd);
    BB *tgt_bb = Get_Label_BB(TN_label(tgt_tn));
    while ((fwd_branch && stub_loc < tgt_loc) ||
	   (!fwd_branch && stub_loc > tgt_loc))
    {
      BBLIST *item;
      STUB_INFO *stub_info;
      BB *longb_bb;
      BB *stub_bb;
      TN *label_tn;
      TN *old_label_tn;
      BB *old_tgt_bb;
      OP *new_op;
      OPS new_ops;
      INT stub_inst_words;
      INT stub_bytes;
      float longb_prob;
      INT pc_idx;

      if (Trace_Longbr) {
	#pragma mips_frequency_hint NEVER
	fprintf(TFile, "========= Creating a stub BB ========\n");
	fprintf(TFile, " BB with long branch:\n");
	Print_BB_Header(bb_info[longb_list->bbindex].bb_ptr, TRUE, FALSE);
      }
      old_label_tn = OP_opnd(longb_list->op, longb_list->opnd);
      old_tgt_bb = Get_Label_BB(TN_label(old_label_tn));
      longb_bb = bb_info[longb_list->bbindex].bb_ptr;

      /* Get the probability that the long branch is taken.
       */
      if (Trace_Longbr) {
	#pragma mips_frequency_hint NEVER
	fprintf(TFile, "Looking for branch target BB (labeled %s)\n",
		LABEL_name(TN_label(old_label_tn)));
      }
      FOR_ALL_BB_SUCCS(longb_bb, item) {
	if (old_tgt_bb == BBLIST_item(item)) {
	  longb_prob = BBLIST_prob(item);
	  if (Trace_Longbr) {
	    #pragma mips_frequency_hint NEVER
	    fprintf(TFile, "Found BB:%2d labeled %s\n",
		    BB_id(BBLIST_item(item)),
		    LABEL_name(TN_label(old_label_tn)));
	  }
	  break;
	}
      }
      FmtAssert(item != NULL,
		("Didn't find branch target BB among successors!"));

      /* If there is an existing stub that branches to the final target,
       * then change this long branch to branch to it and we've fixed
       * it without addding any additional code.
       */
      stub_bb = Find_Stub_BB(longb_list, stubs);
      if (stub_bb) {
	if (Trace_Longbr) {
	  #pragma mips_frequency_hint NEVER
          fprintf (TFile, "Targetting existing stub-bb: BB:%d\n", BB_id(stub_bb));
	}
	BB_freq(stub_bb) += longb_prob * BB_freq(longb_bb);

	label_tn = Gen_Label_TN (Gen_Label_For_BB (stub_bb), 0);
	Set_OP_opnd(longb_list->op, longb_list->opnd, label_tn);

        Unlink_Pred_Succ(longb_bb, old_tgt_bb);
        Link_Pred_Succ_with_Prob(longb_bb, stub_bb, longb_prob);

	break;
      }

      /* Keep track of how many instruction words we add for this stub.
       */
      stub_inst_words = num_longb_inst_words;
      stub_bb = Create_Stub_BB (longb_list, &stub_loc, fwd_branch);

      /* Keep track of the location of this stub so that we might be
       * able to branch to it from another long branch. Note that
       * since the long branches are sorted by target, we delete
       * the list if the final target is different -- this speeds up
       * the searching in Find_Stub_BB. (Note that we could do some
       * better memory management and recycle the list members).
       */
      if (stubs && stubs->targ != tgt_bb) stubs = NULL;
      stub_info = (STUB_INFO *)alloca(sizeof(STUB_INFO));
      stub_info->stub = stub_bb;
      stub_info->stub_loc = stub_loc;
      stub_info->targ = tgt_bb;
      stub_info->offset = TN_offset(tgt_tn);
      stub_info->next = stubs;
      stubs = stub_info;

      /* Set the frequency of the new BB.
       */
      BB_freq(stub_bb) = longb_prob * BB_freq(longb_bb);

      Unlink_Pred_Succ(longb_bb, old_tgt_bb);
      Link_Pred_Succ_with_Prob(longb_bb, stub_bb, longb_prob);
      Link_Pred_Succ_with_Prob(stub_bb, old_tgt_bb, 1.0);

      if (Trace_Longbr) {
	#pragma mips_frequency_hint NEVER
        fprintf (TFile, "Location of stub-bb: %d\n", stub_loc);
      }
      label_tn = Gen_Label_TN (Gen_Label_For_BB (stub_bb), 0);
      Set_OP_opnd(longb_list->op, longb_list->opnd, label_tn);
      OPS_Init(&new_ops);
      Exp_OP1 (OPC_GOTO, NULL, old_label_tn, &new_ops);
      new_op = OPS_last(&new_ops);
      /* Are the following two lines right? */
      longb_list->op = new_op;
      longb_list->opnd = OP_find_opnd_use(new_op, OU_target);
      longb_list->longb_inum = stub_loc;

      /* If the above two lines are right, the following line should
       * be OK, too, and something like it is required now that we
       * update successor and predecessor edges.
       */
      bb_info[longb_list->bbindex].bb_ptr = stub_bb;

      /* Now we have a problem. bb_info[longb_list->bbindex].bb_inum
       * refers to the position of the BB with the original long
       * branch in it, but longb_list->* and
       * bb_info[longb_list->bbindex].bb_ptr refer to the new
       * (potentially long) branch. This is bad because bb_inum is the
       * criterion used to decide whether to place a new stub, but if
       * the criterion is met, the stub gets placed according to
       * bb_ptr. The final insult is added by the fact that the
       * bb_info entries are sorted by bb_inum, so we can't just
       * update bb_inum to reflect the position of the (new,
       * potentially long) branch and have everything just work.
       *
       * We take the low-performance solution, and hope we don't get
       * hurt too bad: Figure out the bb_inum of the new potentially
       * long branch, and grimpse the bb_info array up or down to put
       * the updated bb_info record in the right place. Argh. Shoulda
       * used the Standard Template Library for bb_info, and then this
       * would all be transparent and fast...
       *
       * Issues: Should we update longb_list->longb_inum? I don't
       *         think so, since it doesn't get used.
       *         What to do about the fact that bb_inum fields aren't
       *         updated to reflect the insertion of the stub_bb's
       *         instructions? Should we update it as we shift the
       *         bb_info array?
       *
       * There remains a slight incorrectness in that even though
       * BB_Info_Maintain_Sort updates the bb_inum for each BB
       * containing a long branch to reflect the stub insertion, we do
       * not update the longb_inum fields of the longb_list
       * entries. This can lead to the user having to manually set
       * longbranch_limit. Ugh.
       *
       * To counter this problem, we should redefine the longb_inum
       * field to be an offset (in bytes) from the beginning of
       * the BB. Then the updating of the bb_inum fields will take
       * care of us, and calculations that once involved
       * longb_list->longb_inum will now involve
       * bb_info[longb_list->bb_ptr].bb_inum + longb_list->longb_inum.
       */
      bb_info[longb_list->bbindex].bb_inum = stub_loc;

      /* Add the new instructions so we can count their length
       */
      OP_scycle(new_op) = 0;
      BB_Append_Op(stub_bb, new_op);
      num_longb_inst_words += OP_Real_Inst_Words(new_op);
      if (PROC_has_branch_delay_slot()) {
	Exp_Noop(&new_ops);
	new_op = OPS_last(&new_ops);
	OP_scycle(new_op) = 1;
	BB_Append_Op(stub_bb, new_op);
	num_longb_inst_words += OP_Real_Inst_Words(new_op);
      }

      stub_inst_words = num_longb_inst_words - stub_inst_words;
      stub_bytes = stub_inst_words * INST_BYTES;

      pc_idx = BB_cold(stub_bb) ? ICOLD : IHOT; 
      cur_pcs[pc_idx] = PC_Incr_N(cur_pcs[pc_idx], stub_inst_words);

      longb_list->bbindex = BB_Info_Maintain_Sort(bb_info,
						  num_bbs,
						  longb_list->bbindex,
						  stub_bytes);

      /* Adjust the start address of the stubs in case they moved.
       */
      for (stub_info = stubs->next; stub_info; stub_info = stub_info->next) {
	if (stub_info->stub_loc >= stub_loc) {
	  stub_info->stub_loc += stub_bytes;
	}
      }

      if (Trace_Longbr) {
	#pragma mips_frequency_hint NEVER
	INT num_longb_inst_bytes = num_longb_inst_words * INST_BYTES;
	fprintf (TFile, "Number of instruction bytes in branch stubs: %d\n", 
		num_longb_inst_bytes * INST_BYTES);
        if (num_longb_inst_bytes > ((MIN_BRANCH_DISP - EMIT_Long_Branch_Limit)))
	  fprintf (TFile, "ERROR: too many long branches\n");
	fprintf (TFile, "New stub BB:\n");
	Print_BB_Header(stub_bb, TRUE, FALSE);
      }
      stub_loc += br_limit;
    }
    if (Trace_Longbr) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "--- Resolved\n");
    }
  }

  Recompute_Label_Offset(cur_pcs);
  *hot_size = cur_pcs[IHOT] - text_PC;
  *cold_size = cur_pcs[ICOLD] - cold_PC;
}


/* Check if <pred> is a branch to <succ> that skips the first instruction
 * in the <succ>. This typically happens after filling the branch delay
 * slot with the first instruction in the target basic block.
 */
static BOOL
Branch_Skips_First_Op (BB *pred, BB *succ) 
{
  OP *op;

  if (BB_next(pred) == succ) return FALSE;

  op = BB_branch_op (pred);
  if (op != NULL) {
    INT i;
    for (i = 0; i < OP_opnds(op); i++) {
      TN *opnd_tn = OP_opnd(op,i);
      if (TN_is_label(opnd_tn) && TN_offset(opnd_tn) != 0) return TRUE;
    }
  }
  return FALSE;
}

/* 
 * Check if bb should be aligned, 
 * and return number of instructions it should be aligned with.
 */
INT
Check_If_Should_Align_BB (BB *bb, INT32 curpc)
{
  	BBLIST *preds;
	INT32 targpc;
	float skip_freq = 0.01;		/* not zero so can divide */
	float noskip_freq = 0.01;
	INT targ_alignment;
	INT num_of_ops = 0; 		/* zero to begin with */
#define FREQUENT_BB_DIFF 5.0

	/*
	 * Align loops for best processor efficiency.
	 * Originally checked if bb_loophead, but now just
	 * check frequency of block (in case doesn't look like proper loop,
	 * but still has high-frequency).
	 */
	if (OPT_Space)
		return 0;	/* don't align */
	if (BB_freq(bb) <= 1.0)
		return 0;	/* not frequent enough, so don't align */
	if (BB_prev(bb) == NULL) 
		return 0;	/* nowhere to put nops */
	/* don't want to add nops to previous bb 
	 * unless current bb is significantly more frequent. */
	if (BB_freq(bb) / BB_freq(BB_prev(bb)) < FREQUENT_BB_DIFF)
		return 0;

	/* first check whether target is always label+4 */
        FOR_ALL_BB_PREDS (bb, preds) {
		if (Branch_Skips_First_Op (BBLIST_item(preds), bb)) {
			skip_freq += BB_freq(BBLIST_item(preds));
		} else {
			noskip_freq += BB_freq(BBLIST_item(preds));
		}
	}
	if ((skip_freq / noskip_freq) > FREQUENT_BB_DIFF)
		targpc = PC_Incr(curpc);
	else if ((noskip_freq / skip_freq) > FREQUENT_BB_DIFF)
		targpc = curpc;
	else {
		/* mixture of skip and noskip branches, 
		 * or just not frequent enough,
		 * so don't align */
		return 0;
	}
	if (Trace_Inst) {
		#pragma mips_frequency_hint NEVER
		fprintf(TFile, "align_bb %d:  bb_freq = %f, prev_bb_freq = %f, skip_freq = %f, noskip_freq = %f\n",
			BB_id(bb), BB_freq(bb), BB_freq(BB_prev(bb)), 
			skip_freq, noskip_freq);
	}
	if (Align_Instructions)
		targ_alignment = Align_Instructions;
	else
		targ_alignment = CGTARG_Text_Alignment();
	targ_alignment /= INST_BYTES;	/* so word-sized */
	targpc /= INST_BYTES;		/* so word-sized */
	if (Get_Trace(TP_EMIT, 0x400)) {
		/* To do this right for R10K, 
		 * need to check whether paths/bb-chains
		 * cross cache lines.  An easier check, which is also
		 * compatible with beast and tfp is to just do the
		 * quad-word alignment, which guarantees at least that
		 * the cache-line crossing will be quad-aligned.
		 * So leave this off by default.
		 */
		/* align to cache line size (16 words) */
		/* only do it if block would no longer cross cache line */
#define R10K_PRIMARY_ICACHE_LINE_WORDS	16
		INT targ_cachesize = R10K_PRIMARY_ICACHE_LINE_WORDS;
		UINT ops_till_cacheline = 
			(targ_cachesize - (targpc % targ_cachesize));
		INT aligned_line_crossings = BB_length(bb) / targ_cachesize;
		INT orig_line_crossings = (BB_length(bb) + targ_cachesize - ops_till_cacheline)  / targ_cachesize;
		if (aligned_line_crossings < orig_line_crossings
		    && ops_till_cacheline <= 4)
		{
			num_of_ops = ops_till_cacheline;
		}
	}
	else {
		num_of_ops = ((targ_alignment - (targpc % targ_alignment)) % targ_alignment);
	}
	return num_of_ops;
}

// When we have bundles, 'num' is the number of bundles, not
// insructions.
static void Pad_BB_With_Noops(BB *bb, INT num)
{
  OP *new_op;
  OPS new_ops = OPS_EMPTY;

  if (ISA_MAX_SLOTS > 1) {
    INT ibundle;
    UINT64 slot_mask;

    /* Choose a bundle for the nops. For now we just pick the first
     * bundle without a stop bit. We could chose more smartly based
     * on the previous contents of the BB.
     */
    for (ibundle = 0; ibundle < ISA_MAX_BUNDLES; ++ibundle) {
      UINT32 stop_mask = ISA_EXEC_Stop_Mask(ibundle);
      slot_mask = ISA_EXEC_Slot_Mask(ibundle);
      if (stop_mask == 0) break;
    }

    do {
      INT slot = ISA_MAX_SLOTS - 1;
      do {
	ISA_EXEC_UNIT_PROPERTY unit;
	unit =  (ISA_EXEC_UNIT_PROPERTY)(
		  (slot_mask >> (ISA_TAG_SHIFT * slot)) 
		& ((1 << ISA_TAG_SHIFT) - 1));
	Exp_Noop(&new_ops);
	new_op = OPS_last(&new_ops);
	OP_scycle(new_op) = OP_scycle(BB_last_op(bb));
	OP_Change_Opcode(new_op, CGTARG_Noop_Top(unit));
	BB_Append_Op (bb, new_op);
	slot -= OP_Real_Inst_Words(new_op);
      } while (slot >= 0);
    } while (--num);
  } else {
    do {
      Exp_Noop(&new_ops);
      new_op = OPS_last(&new_ops);
      OP_scycle(new_op) = OP_scycle(BB_last_op(bb));
      BB_Append_Op (bb, new_op);
    } while (--num);
  }
}

/* ====================================================================
 *
 * R_Resolve_Branches
 *
 * Assign addresses to all local labels. Fixup long branches.
 *
 * ====================================================================
 */

static void
R_Resolve_Branches (ST *pu_st)
{
  BB *bb;
  BB *prev_nonempty_bbs[2] = { NULL, NULL };
  INT32 curpcs[2];
  INT32 hot_size, cold_size;

  /* check for T5 workarounds */
  Hardware_Workarounds();

  curpcs[IHOT] = text_PC;
  curpcs[ICOLD] = cold_PC;
  for ( bb = REGION_First_BB; bb; bb = BB_next(bb) ) {
    INT32 bb_start_pc;
    OP *op;
    INT32 isect = BB_cold(bb) ? ICOLD : IHOT;
    INT32 curpc = curpcs[isect];
    BB *prev_nonempty_bb = prev_nonempty_bbs[isect];

    /* need prev bb to align */
    if (prev_nonempty_bb != NULL) {
      INT32 num = Check_If_Should_Align_BB (bb, curpc);
      if (num != 0) {
	if (Trace_Inst) {
	  #pragma mips_frequency_hint NEVER
	  fprintf(TFile, "insert %d noops at bb %d\n", num, BB_id(bb));
	}
	// increment curpc by 'num' bundles
	curpc += INST_BYTES * num;
	Pad_BB_With_Noops(prev_nonempty_bb, num);
      }
    }

    bb_start_pc = curpc;

    /* If there is no label, make one: */
    Gen_Label_For_BB ( bb );

    for (op = BB_first_op(bb); op; op = OP_next(op)) {
      INT num_inst_words = OP_Real_Inst_Words (op);
      curpc = PC_Incr_N(curpc, num_inst_words);
    }
    
    if (curpc != bb_start_pc) prev_nonempty_bbs[isect] = bb;

    curpcs[isect] = curpc;
  }

  Recompute_Label_Offset(curpcs);

  hot_size = curpcs[IHOT] - text_PC;
  cold_size = curpcs[ICOLD] - cold_PC;

  // if large text size or has branch predicts (which are limited in size),
  // then check for long branches.
  if (hot_size >= MIN_BRANCH_DISP || cold_size >= MIN_BRANCH_DISP
    || CGTARG_Has_Branch_Predict()) 
  {
    Fixup_Long_Branches (&hot_size, &cold_size);
  }

  if (generate_elf_symbols) {
    Em_Define_Symbol (EMT_Put_Elf_Symbol(pu_st), 
		      text_PC, hot_size, text_section);
  }

  if ( Trace_Inst ) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "INSTS IN PROCEDURE: %d\n", hot_size + cold_size);
  }
}

/* ====================================================================
 * ====================================================================
 *
 * Data Initialization
 *
 * ====================================================================
 * ====================================================================
 */

static void
Trace_Init_Loc ( INT scn_idx, Elf64_Xword scn_ofst, INT32 repeat)
{
  /* Emit the section/offset/repeat as a line prefix -- the caller will
   * add context-specific information:
   */
  fprintf ( TFile, "<init>: Section %s (offset %4" SCNd64 " x%d): ",
	    ST_name(em_scn[scn_idx].sym), scn_ofst, repeat );
}


#ifdef TARG_ST
/* (cbr) DDTSst24451. add support for label diffs initializers */
/* ====================================================================
 *    Write_Labdiff (lab1, lab2, scn_idx, scn_ofst, repeat, size)
 * ====================================================================
 */
static Elf64_Word
Write_Labdiff (
  LABEL_IDX lab1,	/* left symbol */
  LABEL_IDX lab2,       /* right symbol */
  INT scn_idx,		/* Section to emit it in */
  Elf64_Word scn_ofst,	/* Section offset to emit it at */
  INT32	repeat,		/* Repeat count */
  INT size		/* 2 or 4 bytes */
)
{
  INT32 i;
  ST *basesym1;
  ST *basesym2;
  INT64 base1_ofst = 0;
  INT64 base2_ofst = 0;
  pSCNINFO scn = em_scn[scn_idx].scninfo;
  INT32 val;

  if ( Trace_Init ) {
    #pragma mips_frequency_hint NEVER
    Trace_Init_Loc (scn_idx, scn_ofst, repeat);
    fprintf ( TFile, "LABDIFF " );
    fprintf ( TFile, "%s - %s\n", LABEL_name(lab1), LABEL_name(lab2));
  }

  /* symbols must have an address */
  Is_True (lab1, ("cgemit: Labdiff lab1 is null"));
  Is_True (lab2, ("cgemit: Labdiff lab2 is null"));

  basesym1 = BB_cold(Get_Label_BB(lab1)) ? cold_base : text_base;
  basesym2 = BB_cold(Get_Label_BB(lab2)) ? cold_base : text_base;
  base1_ofst = Get_Label_Offset(lab1);
  base2_ofst = Get_Label_Offset(lab2);

  if (Use_Separate_PU_Section(current_pu,basesym2)) {
	/* use PU text section rather than generic one */
	basesym2 = PU_base;
  }
  Is_True (basesym1 == basesym2, ("cgemit: Labdiff bases not same"));
  val = base1_ofst - base2_ofst;
  if (val < 0) val = 0;
  if (size == 2) {
	if (val > INT16_MAX) {
		DevWarn("symdiff value not 16-bits; will try recompiling with -TENV:long_eh_offsets");
		Early_Terminate (RC_OVERFLOW_ERROR);
	}
	val = val << 16;	/* for Add_Bytes */
  }

  for ( i = 0; i < repeat; i++ ) {
    if (Assembly || MiniR_Code) {
      fprintf (MiniR_Code ? MiniR_file : Output_File, "\t%s\t", (size == 2 ? AS_HALF : AS_WORD));
      fprintf((MiniR_Code ? MiniR_file : Output_File, "%s", LABEL_name(lab1));
      fprintf ((MiniR_Code ? MiniR_file : Output_File, "-");
      fprintf((MiniR_Code ? MiniR_file : Output_File, "%s", LABEL_name(lab2));
      fprintf ((MiniR_Code ? MiniR_file : Output_File, "\n");
    }
    if (Object_Code) {
      Em_Add_Bytes_To_Scn (scn, (char *) &val, size, 1);
    }
    scn_ofst += size;
  }
#ifdef TARG_ST
  // [CL]
  Set_LABEL_emitted(lab1);
  Set_LABEL_emitted(lab2);
#endif
  return scn_ofst;
}
#endif

#ifdef KEY
#include <map>
std::map<const ST*, const ST*> st_to_pic_st;
// Emit PIC version of a symbol, here, a typeinfo symbol
static Elf64_Word
Emit_PIC_version (ST * st, Elf64_Word scn_ofst)
{
  if (st_to_pic_st.find (st) == st_to_pic_st.end())
  {
    ST * pic_st = New_ST (GLOBAL_SYMTAB);
    STR_IDX name = Save_Str2 ("DW.ref.", ST_name (st));
    ST_Init(pic_st, name, CLASS_VAR, SCLASS_DGLOBAL, EXPORT_HIDDEN, MTYPE_TO_TY_array[MTYPE_U8]);
    Set_ST_is_weak_symbol (pic_st);
    Set_ST_is_initialized (pic_st);
    ST_ATTR_IDX st_attr_idx;
    ST_ATTR&    st_attr = New_ST_ATTR (GLOBAL_SYMTAB, st_attr_idx);
    ST_ATTR_Init (st_attr, ST_st_idx (pic_st), ST_ATTR_SECTION_NAME, Save_Str2 (".gnu.linkonce.d.", ST_name (pic_st)));
                                                                                
    INITV_IDX iv = New_INITV();
    INITV_Init_Symoff (iv, st, 0, 1);
    New_INITO (ST_st_idx (pic_st), iv);

    Assign_ST_To_Named_Section (pic_st, ST_ATTR_section_name (st_attr));
    st_to_pic_st [st] = pic_st;
  }

#ifdef TARG_ST
  fprintf (MiniR_Code ? MiniR_File : Asm_File, "\t%s\tDW.ref.%s-.\n", AS_WORD, ST_name (st));
#else
  fprintf (MiniR_Code ? MiniR_File : Asm_File, "\t.long\tDW.ref.%s-.\n", ST_name (st));
#endif

  return scn_ofst + 4;
}
#endif


#ifdef KEY
static int exception_table_id=0;
const char *lsda_ttype_base = ".LSDATTYPEB";
const char *lsda_ttype_disp = ".LSDATTYPED";
const char *lsda_cs_begin = ".LSDACSB";
const char *lsda_cs_end = ".LSDACSE";

static Elf64_Word Write_Diff (LABEL_IDX, LABEL_IDX, INT, Elf64_Word);

static Elf64_Xword Handle_EH_Region_Length (LABEL_IDX l, 
				INT scn_idx, Elf64_Xword scn_ofst)
{
    static bool first_label_seen = false;
    static LABEL_IDX first_label=0;

    // process exception region length
    if (!first_label_seen)
    {
	FmtAssert (first_label==0,("EH table processing error"));
	first_label_seen = true;
	first_label = l;
    }
    else
    {// this is the 2nd label
	FmtAssert (first_label!=0,("EH table processing error"));
	scn_ofst = Write_Diff (first_label, l, scn_idx, scn_ofst);
	// reset
	first_label_seen = false;
	first_label = 0;
    }
    return scn_ofst;
}
#endif // KEY

/* ====================================================================
 *
 * Write_TCON
 *
 * Emit a TCON (constant) value to the assembly/object file.
 *
 * ====================================================================
 */

static Elf64_Word
Write_TCON (
  TCON	*tcon,		/* Constant to emit */
  INT scn_idx,		/* Section to emit it into */
  Elf64_Xword scn_ofst,	/* Section offset to emit it at */
  INT32	repeat		/* Repeat count */
#ifdef KEY
  , bool etable = 0
  , int format = 0
#endif // KEY
   )
{
  BOOL add_null = TCON_add_null(*tcon);
  pSCNINFO scn = em_scn[scn_idx].scninfo;

  if (Trace_Init) {
    #pragma mips_frequency_hint NEVER
    Trace_Init_Loc ( scn_idx, scn_ofst, repeat);
    fprintf (TFile, "TCON: >>%s<<\n", Targ_Print (NULL, *tcon));
  }
  if (Assembly || MiniR_Code) {
    INT32 scn_ofst32 = (INT32)scn_ofst;
    FmtAssert(scn_ofst32 == scn_ofst, ("section offset exceeds 32 bits: 0x%" SCNx64 "",
				       (INT64)scn_ofst));
#ifdef KEY
    if (etable)
      Targ_Emit_EH_Const ( MiniR_Code ? MiniR_File : Asm_File, *tcon, add_null, repeat, scn_ofst32, format );
    else
#endif // KEY
      Targ_Emit_Const ( MiniR_Code ? MiniR_File : Asm_File, *tcon, add_null, repeat, scn_ofst32 );
  } 
  if (Object_Code) {
    Em_Targ_Emit_Const ( scn, *tcon, add_null, repeat );
  }

  if ( TCON_ty(*tcon) == MTYPE_STRING )
    scn_ofst += (Targ_String_Length (*tcon) + (add_null ? 1 : 0)) * repeat;
  else
    scn_ofst += TY_size(Be_Type_Tbl(TCON_ty(*tcon))) * repeat;

  return scn_ofst;
}

static INT
AsmType_Mtype (const char *at)
{
  switch (at[1]) {
  case 'w': // ".word"
    return MTYPE_I4;
  case 'h': // ".hword"
    return MTYPE_I2;
  case 'd': // ".dword":
    return MTYPE_U4;
  case 'q': // ".quad":
    return MTYPE_I8;
  default:
    objects_MINIR << "unknown type: " << at << "\n";
    return MTYPE_UNKNOWN;
  }
}

/* ====================================================================
 *
 * Write_Symbol
 *
 * Emit a symbol value to the assembly/object file.
 *
 * ====================================================================
 */

static Elf64_Word
Write_Symbol (
  ST * sym,		/* Emit the address of this symbol */
  Elf64_Sxword sym_ofst,/*   ... plus this offset */
  INT scn_idx,		/* Section to emit it in */
  Elf64_Word scn_ofst,	/* Section offset to emit it at */
  INT32	repeat		/* Repeat count */
#ifdef TARG_ST
  /* (cbr) support for half address relocation */
  ,  BOOL halfword = 0  /* Output size */
#endif
)
{
  INT32 i;
  ST *basesym;
  INT64 base_ofst = 0;
  pSCNINFO scn = em_scn[scn_idx].scninfo;
#ifdef TARG_ST
  /* (cbr) support for half address relocation */
  INT address_size = halfword ? 2 : ((Use_32_Bit_Pointers) ? 4 : 8);
#else
  INT address_size = ((Use_32_Bit_Pointers) ? 4 : 8);
#endif

  if ( Trace_Init ) {
    #pragma mips_frequency_hint NEVER
    Trace_Init_Loc (scn_idx, scn_ofst, repeat);
    fprintf ( TFile, "SYM " );
    fprintf ( TFile, "%s %+" SCNd64 "\n", ST_name(sym), sym_ofst );
  }

  /* make sure is allocated */
  if (!Has_Base_Block(sym)) {
	Allocate_Object(sym);
	/* if did allocate on local stack, that messes up frame length */
	Is_True(!Has_Base_Block(sym) || Is_Global_Symbol(Base_Symbol(sym)),
		("Write_Symbol:  too late to allocate object on stack"));
  }

  /* If the symbol is a local label that has not been assigned an address, it is
   * a label in a basic block that has been deleted. Emit zeros for this case 
   * instead of the address.
   */
  if (ST_sclass(sym) == SCLASS_TEXT && !Has_Base_Block(sym)) {
    INT32 padding;
    padding = repeat * address_size;
    if ((Assembly || MiniR_Code) && padding > 0) {
#ifdef TARG_MIPS
      if (CG_emit_non_gas_syntax)
	fprintf(MiniR_Code ? MiniR_File : Asm_File, "\t%s %" SCNd64 "\n", ".space", (INT64)padding);
      else
#endif
      ASM_DIR_ZERO(MiniR_Code ? MiniR_File : Asm_File, padding);
    }
    if (Object_Code) {
      Em_Add_Zeros_To_Scn (scn, padding, 1);
    }
    scn_ofst += padding;
    return scn_ofst;
  }

  /* For local static symbols that do not have their own elf entry,
   * use the base symbol; funcs always have own elf entry. */
  basesym = sym;
  if (Has_Base_Block(sym) && 
      ST_is_export_local(sym) && 
#ifdef TARG_ST
      Base_Offset_Is_Known (sym) &&
#endif
      ST_class(sym) != CLASS_FUNC) {
    Base_Symbol_And_Offset (sym, &basesym, &base_ofst);
  }
  if (Use_Separate_PU_Section (current_pu, basesym)) {
	/* use PU text section rather than generic one */
	basesym = PU_base;
  }
  base_ofst += sym_ofst;

  if (Object_Code && repeat != 0) {
    if (Use_32_Bit_Pointers) {
      Em_Add_New_Content (CK_SADDR_32, scn_ofst, 4*repeat, 0, scn);
    }
    else {
      Em_Add_New_Content (CK_SADDR_64, scn_ofst, 8*repeat, 0, scn);
    }
  }

  for ( i = 0; i < repeat; i++ ) {
    // do object code first so base section initialized
    if (Object_Code) {
	if (ST_sclass(sym) == SCLASS_EH_REGION_SUPP) {
      		Em_Add_Displacement_To_Scn (scn, EMT_Put_Elf_Symbol (basesym),
			base_ofst, 1);
	} else {
      		Em_Add_Address_To_Scn (scn, EMT_Put_Elf_Symbol (basesym), 
			base_ofst, 1);
	}
    }
    const char *fptr = AS_FPTR;
    if (Assembly || MiniR_Code) {
#ifdef TARG_MIPS
      if (CG_emit_non_gas_syntax)  
	if (MiniR_Code)
	  objects_MINIR << "      - " << Mtype_String(AsmType_Mtype(Use_32_Bit_Pointers ? ".word" : ".dword")) << ": [";
	else 
	  fprintf(Asm_File, "\t%s\t", Use_32_Bit_Pointers ? ".word" : ".dword");
      else
#elsif TARG_ST
      /* (cbr) support for half address relocation */
	if (halfword)
	  if (MiniR_Code)
	    objects_MINIR << "      - " << Mtype_String(AsmType_Mtype(AS_HALF)) << ": [";
	  else
	    fprintf (Asm_File, "\t%s\t",  AS_HALF);
	else
#endif
	  if (MiniR_Code) 
	    objects_MINIR << "      - " << Mtype_String(AsmType_Mtype((scn_ofst % address_size) == 0 ? AS_ADDRESS : AS_ADDRESS_UNALIGNED)) << ": [";
	  else 
	    fprintf (Asm_File, "\t%s\t", 
		     (scn_ofst % address_size) == 0 ? 
		     AS_ADDRESS : AS_ADDRESS_UNALIGNED);
      if (ST_class(sym) == CLASS_CONST) {
	  if (MiniR_Code)
	    if (base_ofst)
	      objects_MINIR << EMT_Get_Qualified_Name(basesym) << ", '" << std::showpos << base_ofst << "']\n";
	    else 
	      objects_MINIR << EMT_Get_Qualified_Name(basesym) << "]\n";
	  else {
	    EMT_Write_Qualified_Name (Asm_File, basesym);
	    fprintf (Asm_File, " %+" SCNd64 "\n", base_ofst);
	  }
	}
	else if (ST_class(sym) == CLASS_FUNC && fptr) {
		fprintf (MiniR_Code ? MiniR_File : Asm_File, " %s(", fptr);
		EMT_Write_Qualified_Name (MiniR_Code ? MiniR_File : Asm_File, sym);
		fprintf (MiniR_Code ? MiniR_File : Asm_File, " %+" SCNd64 ")\n", sym_ofst);
	}
	else {
		if (MiniR_Code)
		  if (sym_ofst)
		    objects_MINIR << EMT_Get_Qualified_Name(basesym) << ", '" << std::showpos << sym_ofst << "']\n";
		  else 
		    objects_MINIR << EMT_Get_Qualified_Name(basesym) << "]\n";
		else if (Assembly) {
		  EMT_Write_Qualified_Name (Asm_File, sym);
		  fprintf (Asm_File, " %+" SCNd64 "\n", sym_ofst);
		}
	}
#ifdef TARG_ST
	// [SC] Make sure the weak attribute is emitted, in case
	// this is the only reference to the symbol.
	if (ST_is_weak_symbol(sym)) {
	  fprintf (MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", AS_WEAK);
	  EMT_Write_Qualified_Name(MiniR_Code ? MiniR_File : Asm_File, sym);
	  fprintf(MiniR_Code ? MiniR_File : Asm_File, "\n");
	}
#endif
 	if (ST_class(sym) == CLASS_FUNC
#if defined(BUILD_OS_DARWIN)
	&& 0 // Mach-O as 1.38 doesn't support .type
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef TARG_MIPS
	&& !CG_emit_non_gas_syntax
#endif
		) {
		fprintf (MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", AS_TYPE);
		EMT_Write_Qualified_Name (MiniR_Code ? MiniR_File : Asm_File, sym);
		fprintf (MiniR_Code ? MiniR_File : Asm_File, ", %s\n", AS_TYPE_FUNC);
	}
    } 
    scn_ofst += address_size;
  }
  return scn_ofst;
}


/* ====================================================================
 *
 * Write_Label
 *
 * Emit a label value to the assembly/object file.
 *
 * ====================================================================
 */

static Elf64_Word
Write_Label (
  LABEL_IDX lab,	/* Emit the address of this label */
  Elf64_Sxword lab_ofst,/*   ... plus this offset */
  INT scn_idx,		/* Section to emit it in */
  Elf64_Word scn_ofst,	/* Section offset to emit it at */
  INT32	repeat)		/* Repeat count */
{
  INT32 i;
  ST *basesym;
  Elf64_Sxword base_ofst = 0;
  pSCNINFO scn = em_scn[scn_idx].scninfo;
  INT address_size = ((Use_32_Bit_Pointers) ? 4 : 8);

  if ( Trace_Init ) {
    #pragma mips_frequency_hint NEVER
    Trace_Init_Loc (scn_idx, scn_ofst, repeat);
    fprintf ( TFile, "LAB (%d) ", (INT)lab );
    fprintf ( TFile, "%s %+" SCNd64 "\n", LABEL_name(lab), lab_ofst );
  }

#ifdef TODO
  // how to tell that label has been deleted?
  // for now, emit 0's.

  /* If the symbol is a local label that has not been assigned an address, it is
   * a label in a basic block that has been deleted. Emit zeros for this case 
   * instead of the address.
   */
  if (!Has_Base_Block(sym)) {
    INT32 padding;
    padding = repeat * address_size;
    if ((Assembly || MiniR_Code) && padding > 0) {
#ifdef TARG_MIPS
      if (CG_emit_non_gas_syntax)
	fprintf(Minir_Code ? MiniR_File : Asm_File, "\t%s %" SCNd64 "\n", ".space", (INT64)padding);
      else
#endif
      ASM_DIR_ZERO(Minir_Code ? MiniR_File : Asm_File, padding);
    }
    if (Object_Code) {
      Em_Add_Zeros_To_Scn (scn, padding, 1);
    }
    scn_ofst += padding;
    return scn_ofst;
  }
#endif

  // Labels are local and do not have own elf entry, 
  // so use base symbol.
  FmtAssert (Get_Label_BB(lab), ("label %d doesn't have defining bb?", lab));
  basesym = BB_cold(Get_Label_BB(lab)) ? cold_base : text_base;
  if (Use_Separate_PU_Section (current_pu, basesym)) {
	/* use PU text section rather than generic one */
	basesym = PU_base;
  }
  base_ofst = Get_Label_Offset(lab) + lab_ofst;

  if (Object_Code && repeat != 0) {
    if (Use_32_Bit_Pointers) {
      Em_Add_New_Content (CK_SADDR_32, scn_ofst, 4*repeat, 0, scn);
    }
    else {
      Em_Add_New_Content (CK_SADDR_64, scn_ofst, 8*repeat, 0, scn);
    }
  }

  for ( i = 0; i < repeat; i++ ) {
    if (Assembly || MiniR_Code) {
#ifdef TARG_MIPS
	if (CG_emit_non_gas_syntax)
	  fprintf(Minir_Code ? MiniR_File : Asm_File, "\t%s\t", Use_32_Bit_Pointers ? ".word" : ".dword");
	else
#endif
	fprintf (MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", 
		(scn_ofst % address_size) == 0 ? 
		AS_ADDRESS : AS_ADDRESS_UNALIGNED);
	fputs (LABEL_name(lab), MiniR_Code ? MiniR_File : Asm_File);
	if (lab_ofst != 0)
		fprintf (MiniR_Code ? MiniR_File : Asm_File, " %+" SCNd64 , lab_ofst);
	fputc ('\n', MiniR_Code ? MiniR_File : Asm_File);
    } 
    if (Object_Code) {
    	Em_Add_Address_To_Scn (scn, EMT_Put_Elf_Symbol (basesym), base_ofst, 1);
    }
    scn_ofst += address_size;
  }
#ifdef TARG_ST
  // [CL]
  Set_LABEL_emitted(lab);
#endif
  return scn_ofst;
}

/* ====================================================================
 *    Write_Symdiff (lab1, sym2idx, scn_idx, scn_ofst, repeat, size)
 * ====================================================================
 */
static Elf64_Word
Write_Symdiff (
  LABEL_IDX lab1,	/* left symbol */
  ST_IDX sym2idx,	/* right symbol */
  INT scn_idx,		/* Section to emit it in */
  Elf64_Word scn_ofst,	/* Section offset to emit it at */
  INT32	repeat,		/* Repeat count */
  INT size		/* 2 or 4 bytes */
#ifdef KEY
  , bool etable = 0
#endif // KEY
  )
{
  INT32 i;
  ST *basesym1;
  ST *basesym2;
  INT64 base1_ofst = 0;
  INT64 base2_ofst = 0;
  pSCNINFO scn = em_scn[scn_idx].scninfo;
  ST *sym2 = &St_Table[sym2idx];
  INT32 val;

  if ( Trace_Init ) {
    #pragma mips_frequency_hint NEVER
    Trace_Init_Loc (scn_idx, scn_ofst, repeat);
    fprintf ( TFile, "SYMDIFF " );
    fprintf ( TFile, "%s - %s\n", LABEL_name(lab1), ST_name(sym2));
  }

  /* symbols must have an address */
  Is_True (lab1, ("cgemit: Symdiff lab1 is null"));
  Is_True (sym2, ("cgemit: Symdiff sym2 is null"));
  Is_True (Has_Base_Block(sym2), ("cgemit: Symdiff sym2 not allocated"));

  BB *labb = Get_Label_BB(lab1);
  basesym1 = BB_cold(Get_Label_BB(lab1)) ? cold_base : text_base;
  base1_ofst = Get_Label_Offset(lab1);
#ifdef TARG_ST
  if (Object_Code) {
    FmtAssert (Base_Offset_Is_Known (sym2), ("Unknown offset in Write_Symdiff"));
    
#endif
  Base_Symbol_And_Offset (sym2, &basesym2, &base2_ofst);
  if (Use_Separate_PU_Section(current_pu,basesym2)) {
	/* use PU text section rather than generic one */
	basesym2 = PU_base;
  }
  Is_True (basesym1 == basesym2, ("cgemit: Symdiff bases not same"));
  val = base1_ofst - base2_ofst;
  if (val < 0) val = 0;
  if (size == 2) {
	if (val > INT16_MAX) {
		DevWarn("symdiff value not 16-bits; will try recompiling with -TENV:long_eh_offsets");
		Early_Terminate (RC_OVERFLOW_ERROR);
	}
	val = val << 16;	/* for Add_Bytes */
  }
#ifdef TARG_ST
  } /* Object_Code */
#endif
  for ( i = 0; i < repeat; i++ ) {
    if (Assembly || MiniR_Code) {
#ifdef KEY
      if (etable)
      	fputs ("\t.uleb128\t", MiniR_Code ? MiniR_File : Asm_File);
      else
#endif // KEY
      fprintf (MiniR_Code ? MiniR_File : Asm_File, "\t%s\t", (size == 2 ? AS_HALF : AS_WORD));
      fputs (LABEL_name(lab1), MiniR_Code ? MiniR_File : Asm_File);
      fputc ('-', MiniR_Code ? MiniR_File : Asm_File);
      EMT_Write_Qualified_Name (MiniR_Code ? MiniR_File : Asm_File, sym2);
      fputc ('\n', MiniR_Code ? MiniR_File : Asm_File);
    } 
    if (Object_Code) {
      Em_Add_Bytes_To_Scn (scn, (char *) &val, size, 1);
    }
    scn_ofst += size;
  }
#ifdef TARG_ST
  // [CL]
  Set_LABEL_emitted(lab1);
#endif
  return scn_ofst;
}


/* ====================================================================
 *
 * Write_INITV
 *
 * Emit an initial value record to the assembly/object file.
 *
 * ====================================================================
 */

static Elf64_Word
#ifdef KEY
Write_INITV (INITV_IDX invidx, INT scn_idx, Elf64_Word scn_ofst, bool etable=0, int format=0)
#else
Write_INITV (INITV_IDX invidx, INT scn_idx, Elf64_Word scn_ofst)
#endif // KEY
{
  INT32 i;
  INITV_IDX ninv;
  INITV inv = Initv_Table[invidx];
  LABEL_IDX lab;
  TCON tcon;
  ST *st;
  pSCNINFO scn = em_scn[scn_idx].scninfo;

#ifdef KEY
  static bool emit_typeinfo=false;
#endif

  switch ( INITV_kind(inv) ) {
    case INITVKIND_ZERO:
      tcon = Host_To_Targ (INITV_mtype (inv), 0);
#ifdef KEY
      if (!emit_typeinfo)
	if (Assembly)
	  scn_ofst = Write_TCON (&tcon, scn_idx, scn_ofst, INITV_repeat2 (inv),
				 etable, format);
	else if (MiniR_Code) {
	  objects_MINIR << "      - " << Mtype_String(INITV_mtype(inv)) << ": ";
	  if (INITV_repeat2(inv) > 1)
	    objects_MINIR << "[ 0, x" << INITV_repeat2 (inv) << " ]\n";
	  else  
	    objects_MINIR << "0\n";
	}
#ifdef TARG_X8664
      else if (Gen_PIC_Call_Shared || Gen_PIC_Shared) // emit_typeinfo
      {
  	fputs ("\t.long\t0\n", Asm_File);
	scn_ofst += 4;
      }
#endif
      else
#endif // KEY
      scn_ofst = Write_TCON (&tcon, scn_idx, scn_ofst, INITV_repeat2 (inv));
      break;

    case INITVKIND_ONE:
      tcon = Host_To_Targ (INITV_mtype (inv), 1);
      if (Assembly)
#ifdef KEY
	scn_ofst = Write_TCON (&tcon, scn_idx, scn_ofst, INITV_repeat2 (inv),
			       etable, format);
#else
        scn_ofst = Write_TCON (&tcon, scn_idx, scn_ofst, INITV_repeat2 (inv));
#endif // KEY
      else if (MiniR_Code) {
	objects_MINIR << "      - " << Mtype_String(INITV_mtype(inv)) << ": ";
	if (INITV_repeat2(inv) > 1)
	  objects_MINIR << "[ 1, x" << INITV_repeat2 (inv) << " ]\n";
	else  
	  objects_MINIR << "1\n";
      }
      break;
    case INITVKIND_VAL:
      tcon = INITV_tc_val(inv);
      if (Assembly)
#ifdef KEY
      scn_ofst = Write_TCON (&INITV_tc_val(inv), scn_idx, scn_ofst,
			     INITV_repeat2(inv), etable, format);
#else
      scn_ofst = Write_TCON (&INITV_tc_val(inv), scn_idx, scn_ofst, 
			     INITV_repeat2(inv));
#endif // KEY
      else if (MiniR_Code) {
	objects_MINIR << "      - " << Mtype_String(TCON_ty(tcon)) << ": ";
	if (INITV_repeat2(inv) > 1)
	  objects_MINIR << "[ " << Targ_Print (NULL, INITV_tc_val(inv)) << ", x" << INITV_repeat2 (inv) << " ]\n";
	else  
	  objects_MINIR << Targ_Print (NULL, INITV_tc_val(inv)) << "\n";
      }
      break;

    case INITVKIND_SYMOFF:
      st = &St_Table[INITV_st(inv)];
#if defined TARG_ST || defined TARG_X8664
      if ((Gen_PIC_Call_Shared || Gen_PIC_Shared) && emit_typeinfo)
      { // handle it differently
	scn_ofst = Emit_PIC_version (st, scn_ofst);
      	break;
      }
#endif
      switch (ST_sclass(st)) {
	case SCLASS_AUTO:
	{ /* EH stack variable */
	  tcon = Host_To_Targ(MTYPE_I4, Offset_from_FP(st));
	  scn_ofst = Write_TCON (&tcon, scn_idx, scn_ofst, INITV_repeat1(inv));
	  break;
	}

	case SCLASS_FORMAL:
	{ /* EH this-pointer */
	  ST * base = ST_base(st);
	  INT  ofst = Offset_from_FP(base) + ST_ofst(st);
	  tcon = Host_To_Targ(MTYPE_I4, ofst);
	  scn_ofst = Write_TCON (&tcon, scn_idx, scn_ofst, INITV_repeat1(inv));
	  break;
	}
      
	default:
          scn_ofst = Write_Symbol ( st, INITV_ofst(inv),
	  			      scn_idx, scn_ofst, INITV_repeat1(inv));
	   break;
      }
      break;

#ifdef TARG_ST
      /* (cbr) support for half address relocation */
    case INITVKIND_SYMOFF16:
      st = &St_Table[INITV_st(inv)];
      switch (ST_sclass(st)) {
	case SCLASS_AUTO:
	case SCLASS_FORMAL:
          DevAssert(FALSE, ("wrong class symoff16"));
	  break;
          
	default:
          scn_ofst = Write_Symbol ( st, INITV_ofst(inv),
                                    scn_idx, scn_ofst, INITV_repeat1(inv), true);
	   break;
      }
      break;
#endif

    case INITVKIND_LABEL:
	lab = INITV_lab(inv);
#ifdef KEY
	if (etable)
	{
	    scn_ofst = Handle_EH_Region_Length (lab, scn_idx, scn_ofst);
	    break;
	}
#endif // KEY
	scn_ofst = Write_Label (lab, 0, scn_idx, scn_ofst, INITV_repeat1(inv));
	break;

#ifdef TARG_ST
/* (cbr) DDTSst24451. add support for label diffs initializers */
 case INITVKIND_LABDIFF:
      scn_ofst = Write_Labdiff ( INITV_labd0(inv), INITV_labd1(inv),
				scn_idx, scn_ofst, INITV_repeat1(inv), 4);
      break;
#endif

    case INITVKIND_SYMDIFF:
#ifdef KEY
      scn_ofst = Write_Symdiff ( INITV_lab1(inv), INITV_st2(inv),
			scn_idx, scn_ofst, INITV_repeat1(inv), 4, etable);
#else
      scn_ofst = Write_Symdiff ( INITV_lab1(inv), INITV_st2(inv),
				scn_idx, scn_ofst, INITV_repeat1(inv), 4);
#endif // KEY
      break;
    case INITVKIND_SYMDIFF16:
#ifdef KEY
      scn_ofst = Write_Symdiff ( INITV_lab1(inv), INITV_st2(inv),
			scn_idx, scn_ofst, INITV_repeat1(inv), 2, etable);
#else
      scn_ofst = Write_Symdiff ( INITV_lab1(inv), INITV_st2(inv),
				scn_idx, scn_ofst, INITV_repeat1(inv), 2);
#endif // KEY
      break;

    case INITVKIND_BLOCK:
#ifdef KEY
      if (INITV_flags(inv) == INITVFLAGS_TYPEINFO)
	emit_typeinfo = true;
#endif
      for (i = 0; i < INITV_repeat1(inv); i++) {
	for (ninv = INITV_blk(inv); ninv; ninv = INITV_next(ninv)) {
#ifdef KEY
          scn_ofst = Write_INITV (ninv, scn_idx, scn_ofst, etable, format);
#else
          scn_ofst = Write_INITV (ninv, scn_idx, scn_ofst);
#endif // KEY
	}
      }
#ifdef KEY
      if (emit_typeinfo)
	emit_typeinfo = false;
#endif
      break;

    case INITVKIND_PAD:
      if ((Assembly || MiniR_Code) && (INITV_pad(inv)*INITV_repeat1(inv) > 0)) {
#ifdef TARG_MIPS
	if (CG_emit_non_gas_syntax)
	  if (Assembly)
	    fprintf(Asm_File, "\t%s %" SCNd64 "\n", ".space", 
		  (INT64)(INITV_pad(inv) * INITV_repeat1(inv)));
	else
#endif
	  if (MiniR_Code)
	    objects_MINIR << "      - space: " << (INT64)(INITV_pad(inv) * INITV_repeat1(inv)) << "\n";
	  else if (Assembly)
	    ASM_DIR_ZERO(Asm_File, INITV_pad(inv) * INITV_repeat1(inv));
      }
      if (Object_Code) {
	Em_Add_Zeros_To_Scn (scn, INITV_pad(inv) * INITV_repeat1(inv), 1);
      }
      scn_ofst += INITV_pad(inv) * INITV_repeat1(inv);
      break;

    default:
      break;
  }
  return scn_ofst;
}

#ifdef KEY
static Elf64_Xword
Generate_Exception_Table_Header (INT scn_idx,
                                 Elf64_Xword scn_ofst,
                                 LABEL_IDX *);
#endif // KEY

#define ST_type_string(st) ( ST_class(st) == CLASS_FUNC ? "function" : ((ST_class(st) == CLASS_VAR ? "object": "unknown")) )


/* ====================================================================
 *    Write_INITO (inop, scn_idx, scn_ofst)
 *
 *    Emit the initialized object to the object file 
 * ====================================================================
 */
static Elf64_Word
Write_INITO (
  INITO* inop,		/* Constant to emit */
  INT scn_idx,		/* Section to emit it into */
  Elf64_Xword scn_ofst)	/* Section offset to emit it at */
{
  pSCNINFO scn = em_scn[scn_idx].scninfo;
  Elf64_Xword inito_ofst;
  ST *sym;
  ST *base;
  INITO ino = *inop;

  if ( Trace_Init ) {
    #pragma mips_frequency_hint NEVER
    Trace_Init_Loc (scn_idx, scn_ofst, 0);
    fprintf ( TFile, "INITO: " );
    Print_INITO (ino);
  }

    Base_Symbol_And_Offset(INITO_st(ino), &base, (INT64*)&inito_ofst);

    if (inito_ofst > scn_ofst) {
      if (Assembly) {
#ifdef TARG_MIPS
	if (CG_emit_non_gas_syntax)
	  fprintf(Asm_File, "\t%s %" SCNd64 "\n", AS_SPACE, 
		  (INT64)(inito_ofst - scn_ofst));
	else
#endif
	  ASM_DIR_ZERO(Asm_File, (INT32)(inito_ofst - scn_ofst));
      }
      if (Object_Code) {
	Em_Add_Zeros_To_Scn ( scn, inito_ofst - scn_ofst, 1 );
      }
      scn_ofst = inito_ofst;
    } else {
      FmtAssert ( inito_ofst >= scn_ofst, 
	("Write_INITO: DATA overlap 1, inito ofst @ %" SCNd64 ", scn ofst @ %" SCNd64 "",
	  inito_ofst, scn_ofst));
    }

    sym = INITO_st(ino);
    if (Assembly || MiniR_Code) {
        char *name = ST_name(sym);
        if (name != NULL && *name != 0) {
	  Print_Label (Asm_File, &objects_MINIR, sym, TY_size(ST_type(sym)));
        }
	if (MiniR_Code) {
	  if (inito_ofst > scn_ofst)
	    objects_MINIR << "    pad: " << (INT32)(inito_ofst - scn_ofst) << "\n";
	  objects_MINIR << "    section: " << ST_name(em_scn[scn_idx].sym) << "\n";
	  objects_MINIR << "    align: " << TY_align (ST_type (sym)) << "\n";
	}
    }
    if (Object_Code && ! ST_is_export_local(sym)) {
        EMT_Put_Elf_Symbol (sym);
    }

    /* If there's no initial value, this should be a constant symbol,
     * and the represented constant is the initial value:
     */
    if ( INITO_val(ino) == (INITO_IDX) NULL ) {
      if ( ST_class(sym) == CLASS_CONST ) {
	TCON *tc=&ST_tcon_val(sym);
	if (Assembly)
	  scn_ofst = Write_TCON (tc, scn_idx, scn_ofst, 1);
	else if (MiniR_Code) {
	  objects_MINIR << "    init:\n";
	  objects_MINIR << "      - " << Mtype_String(TCON_ty(*tc)) << ": " << Targ_Print (NULL, *tc) << "\n";
	}
      }
    } else {
	INITV_IDX inv;
#ifdef KEY
        char *name = ST_name (sym);
        bool range_table = (name != NULL) &&
                (!strncmp (".range_table.", name,strlen(".range_table."))) &&
                (ST_sclass (sym) == SCLASS_EH_REGION);
        LABEL_IDX labels[2];
        if (range_table)
        {
            exception_table_id++;
            scn_ofst = Generate_Exception_Table_Header (scn_idx, scn_ofst, labels);
        }
        bool action_table_started = false;
	bool type_label_emitted = false;
#endif // KEY
	FOREACH_INITV (INITO_val(ino), inv) {
#ifdef KEY
            if (range_table && !action_table_started &&
                  (INITV_flags (Initv_Table[inv]) == INITVFLAGS_ACTION_REC))
            {
                action_table_started = true;
		if (Assembly)
		  fprintf ( Asm_File, "%s:\n", LABEL_name(labels[0]));
#ifdef TARG_ST
		Set_LABEL_emitted(labels[0]);
#endif
            }
	    if (range_table && !type_label_emitted && 
	    	INITV_flags(Initv_Table[inv]) == INITVFLAGS_EH_SPEC)
	    {
	    	type_label_emitted = true;
		if (Assembly)
		  fprintf ( Asm_File, "%s:\n", LABEL_name(labels[1]));
#ifdef TARG_ST
		Set_LABEL_emitted(labels[1]);
#endif
	    }
	    if (MiniR_Code)
	      objects_MINIR << "    init:\n";
            scn_ofst = Write_INITV (inv, scn_idx, scn_ofst, range_table,
	    		range_table ? INITV_flags (Initv_Table[inv]) : 0);
	    if (range_table && !type_label_emitted)
	    {
// Emit the type label IF we are emitting typeids AND this is the last
// typeid, OR if we are beginning the eh-spec table (i.e. had an empty
// typeid table) as above
	    	if ((INITV_flags(Initv_Table[inv]) == INITVFLAGS_TYPEINFO) &&
	      		(!INITV_next(inv) || 
			!INITV_flags(Initv_Table[INITV_next(inv)])))
	    	{
	    	    type_label_emitted = true;
		if (Assembly) 
            	    fprintf (Asm_File, "%s:\n", LABEL_name(labels[1]));
#ifdef TARG_ST
		Set_LABEL_emitted(labels[1]);
#endif
	    	}
	    }
#else
	    if (MiniR_Code)
	      objects_MINIR << "    init:\n";
	    scn_ofst = Write_INITV (inv, scn_idx, scn_ofst);
#endif // KEY
        }
#ifdef KEY
        if (range_table && !type_label_emitted) {
	  if (Assembly)
	    fprintf ( Asm_File, "%s:\n", LABEL_name(labels[1]));
#ifdef TARG_ST
	  Set_LABEL_emitted(labels[1]);
#endif
	}
#endif // KEY
#ifdef TARG_ST
	if (range_table && !action_table_started) {
	  if (Assembly)
	    fprintf ( Asm_File, "%s:\n", LABEL_name(labels[0]));
	  Set_LABEL_emitted(labels[0]);
	}
#endif
    }
    if (Assembly) {
    	fprintf ( Asm_File, "\t%s end of initialization for %s\n", ASM_CMNT, ST_name(sym) );
    }

  return scn_ofst;
}

#ifdef KEY
static Elf64_Word
Write_Diff (
  LABEL_IDX lab1,       /* left symbol */
  LABEL_IDX lab2,       /* right symbol */
  INT scn_idx,          /* Section to emit it in */
  Elf64_Word scn_ofst)  /* Section offset to emit it at */
{
      fputs ("\t.uleb128\t", MiniR_Code ? MiniR_File : Asm_File);
      fputs (LABEL_name(lab1), MiniR_Code ? MiniR_File : Asm_File);
      fputc ('-', MiniR_Code ? MiniR_File : Asm_File);
      fputs (LABEL_name(lab2), MiniR_Code ? MiniR_File : Asm_File);
      fputc ('\n', MiniR_Code ? MiniR_File : Asm_File);

#ifdef TARG_ST
      // [CL]
      Set_LABEL_emitted(lab1);
      Set_LABEL_emitted(lab2);
#endif
      return scn_ofst+4;
}

static int
num_digits (int in)
{
    int num=1;
    while (in/10 != 0)
    {
        num++;
        in = in/10;
    }
    return num;
}

static Elf64_Xword
Generate_Exception_Table_Header (INT scn_idx, Elf64_Xword scn_ofst, LABEL_IDX *labels)
{
    // Generate LPStart pointer, currently constant
    TCON lpstart = Host_To_Targ (MTYPE_I1, 0xff);
    scn_ofst = Write_TCON (&lpstart, scn_idx, scn_ofst, 1);

    // Generate TType format
    TCON ttype;
#ifdef TARG_X8664
    if (Gen_PIC_Call_Shared || Gen_PIC_Shared)
    	ttype = Host_To_Targ (MTYPE_I1, 0x9b);
    else
#endif
    	ttype = Host_To_Targ (MTYPE_I1, 0);
    scn_ofst = Write_TCON (&ttype, scn_idx, scn_ofst, 1);

    // Generate TType base offset
    int id_size = num_digits (exception_table_id);
    LABEL_IDX ttype_base;
    New_LABEL (CURRENT_SYMTAB, ttype_base);
    char *ttype_base_name = (char *)malloc (strlen (lsda_ttype_base) +
                                id_size + 1);
    sprintf (ttype_base_name, "%s%d", lsda_ttype_base, exception_table_id);
    Set_LABEL_name_idx (Label_Table[ttype_base], Save_Str (ttype_base_name));

    LABEL_IDX ttype_disp;
    New_LABEL (CURRENT_SYMTAB, ttype_disp);
    char *ttype_disp_name = (char *)malloc (strlen (lsda_ttype_disp) +
                                id_size + 1);
    sprintf (ttype_disp_name, "%s%d", lsda_ttype_disp, exception_table_id);
    Set_LABEL_name_idx (Label_Table[ttype_disp], Save_Str (ttype_disp_name));
    /*
    ST * csb = New_ST (CURRENT_SYMTAB);
    char *csb_name = (char *)malloc ( strlen (lsda_cs_begin) +
                                id_size + 1);
    sprintf (csb_name, "%s%d", lsda_cs_begin, exception_table_id);
    ST_Init (csb, Save_Str (csb_name), CLASS_BLOCK, 
                SCLASS_EH_REGION, EXPORT_LOCAL, MTYPE_TO_TY_array[MTYPE_U4]);

    ST * ttype_disp = New_ST (CURRENT_SYMTAB);
    char *ttype_disp_name = (char *)malloc ( strlen (lsda_ttype_disp) +
                                id_size + 1);
    sprintf (ttype_disp_name, "%s%d", lsda_ttype_disp, exception_table_id);
    ST_Init (ttype_disp, Save_Str (ttype_disp_name), CLASS_BLOCK, 
                SCLASS_EH_REGION, EXPORT_LOCAL, MTYPE_TO_TY_array[MTYPE_U4]);
    Set_ST_base_idx (*ttype_disp, ST_st_idx (*csb));
    */
    scn_ofst = Write_Diff (ttype_base, ttype_disp, scn_idx, scn_ofst);
    fprintf ( Asm_File, "%s:\n", LABEL_name(ttype_disp));

    // Generate call site format
    TCON csf = Host_To_Targ (MTYPE_I1, 1);
    scn_ofst = Write_TCON (&csf, scn_idx, scn_ofst, 1);

    LABEL_IDX csb;
    New_LABEL (CURRENT_SYMTAB, csb);
    char *csb_name = (char *)malloc (strlen (lsda_cs_begin) +
                                id_size + 1);
    sprintf (csb_name, "%s%d", lsda_cs_begin, exception_table_id);
    Set_LABEL_name_idx (Label_Table[csb], Save_Str (csb_name));

    LABEL_IDX cse;
    New_LABEL (CURRENT_SYMTAB, cse);
    char *cse_name = (char *)malloc (strlen (lsda_cs_end) +
                                id_size + 1);
    sprintf (cse_name, "%s%d", lsda_cs_end, exception_table_id);
    Set_LABEL_name_idx (Label_Table[cse], Save_Str (cse_name));
    scn_ofst = Write_Diff (cse, csb, scn_idx, scn_ofst);
    labels[0] = cse;
    labels[1] = ttype_base;

    fprintf ( Asm_File, "%s:\n", LABEL_name(csb));
    return scn_ofst;
}
#endif // KEY

/* change to a new section and new origin */
static void
Change_Section_Origin (ST *base, INT64 ofst)
{
	if (Assembly) {
		if (base != cur_section) {
			/* switch to new section. */
#if defined(BUILD_OS_DARWIN)
			emit_section_directive(base);
#else /* defined(BUILD_OS_DARWIN) */
			fprintf ( Asm_File, "\n\t%s %s\n", AS_SECTION,
			ST_name(base)
			);
#endif /* defined(BUILD_OS_DARWIN) */
		}
#if !defined(TARG_MIPS) && !defined(TARG_MVP)
#ifdef TARG_X8664
		if (strcmp(ST_name(base), ".except_table"))
#endif // TARG_X8664
		if (!CG_file_scope_asm_seen)
		fprintf (Asm_File, "\t%s 0x%" SCNx64 "\n", AS_ORIGIN, ofst);
		/* generate a '.align 0' to make sure we don't autoalign */
#ifndef KEY
		fprintf ( Asm_File, "\t%s\t0\n", AS_ALIGN );
#endif // KEY
#endif // TARG_MIPS
		cur_section = base;
#if defined(TARG_MIPS) || defined(TARG_MVP)
		CGEMIT_Change_Origin_In_Asm(base, ofst);
#endif
	}
	/* for nobits, add final size at end because we don't write any data. */
  	if (Object_Code && !STB_nobits(base)) {
		Em_Change_Section_Origin (em_scn[STB_scninfo_idx(base)].scninfo, ofst);
	}
}


inline bool section_lt (ST *s1, ST* s2) 
{ 
  // order doesn't really matter, just that grouped by base section
  return Base_Symbol(s1) < Base_Symbol(s2); 
}


inline bool offset_lt (ST *s1, ST* s2) 
{ 
  return Offset_From_Base_Symbol(s1) < Offset_From_Base_Symbol(s2); 
}


inline bool size_lt (ST *s1, ST* s2) 
{ 
  // This is only needed so that we get silly 0-sized structs
  // at the correct offset.
  return TY_size(ST_type(s1)) < TY_size(ST_type(s2)); 
}

#ifdef Is_True_On
static void
Print_ST_List(vector<ST*>& st_list, const char* header)
{
  printf("%s\n", header);

  vector<ST*>::iterator st_iter;
  for (st_iter = st_list.begin(); st_iter != st_list.end(); ++st_iter) {
    ST* st = *st_iter;
    printf("%-25s%-15s%10" SCNu64 "%10" SCNd64 "\n",
           (ST_class(st) == CLASS_CONST ? "<constant>" : ST_name(st)), 
           (ST_class(Base_Symbol(st)) == CLASS_CONST ? "<constant>" :
            ST_name(Base_Symbol(st))),
           TY_size(ST_type(st)),
           Offset_From_Base_Symbol(st));
  }
}
#endif
int INITV_Removable(INITV & inv) {
  INT32 i;
  INITV_IDX ninv;
  if(INITV_kind(inv) == INITVKIND_BLOCK) {
    for (i = 0; i < INITV_repeat1(inv); i++) 
	  for (ninv = INITV_blk(inv); ninv; ninv = INITV_next(ninv)) 
        if(!INITV_Removable(Initv_Table[ninv]))
          return FALSE;    
  }
  else if(INITV_kind(inv) == INITVKIND_LABEL)  {
    LABEL_IDX lab = INITV_lab(inv); 
    if(!Get_Label_BB(lab)) 
      return TRUE;
    else
      return FALSE;
  }   
  else 
    return FALSE; 

  return TRUE;
}

//This routine can judge whether Inito print into asm file or not.  
int Inito_Removable(INITO* inop) {
  INITO ino = *inop;
  INITV_IDX inv;
  FOREACH_INITV (INITO_val(ino), inv) {
    INITV invp = Initv_Table[inv];
    if(!INITV_Removable(invp)) 
      return FALSE;
  }
  return TRUE;
}

/* ====================================================================
 *    Process_Initos_And_Literals (stab)
 *
 *    This routine can be called multiple times for the global symtab;
 *    we do this so that objects are emitted in order.
 *    For each section, some objects are in local symtab, and some in global.
 *    We keep track of the last global symbol that has been processed
 *    so that we only process new symbols.
 *
 * ====================================================================
 */
void
Process_Initos_And_Literals (
  SYMTAB_IDX stab
)
{
  static std::vector<bool> st_processed;
  if (st_processed.size() != ST_Table_Size(GLOBAL_SYMTAB)) {
    st_processed.resize(ST_Table_Size(GLOBAL_SYMTAB), false);
  }

  std::vector<ST*> st_list;
  std::vector<ST*>::iterator st_iter;

  typedef 
  hash_map < ST_IDX, INITO*, __gnu_cxx::hash<ST_IDX>, std::equal_to<ST_IDX> > ST_INITO_MAP;
  ST_INITO_MAP st_inito_map;

  UINT i;
  static UINT last_inito = 1;
  
  // First walk the INITOs from the global table
  for (i = last_inito; i < INITO_Table_Size(GLOBAL_SYMTAB); ++i) {
    INITO* ino = &Inito_Table(GLOBAL_SYMTAB,i);
    ST* st = INITO_st(ino);
    /* don't emit initialization if st not used or extern */
    if (ST_is_not_used(st) ||
        ST_sclass(st) == SCLASS_EXTERN ||
        ST_sclass(st) == SCLASS_DISTR_ARRAY) {
      continue;
    }
    st_list.push_back(st);
    st_inito_map[ST_st_idx(st)] = ino;
  }

  last_inito = INITO_Table_Size(GLOBAL_SYMTAB);

  // Then walk the INITOs from the local table
  if (stab != GLOBAL_SYMTAB) {
    for (i = 1; i < INITO_Table_Size(stab); ++i) {
      INITO* ino = &Inito_Table(stab,i);
      ST* st = INITO_st(ino);
      /* don't emit initialization if st not used or extern */
      if (ST_is_not_used(st) ||
          ST_sclass(st) == SCLASS_EXTERN) {
        continue;
      }
	  if(Inito_Removable(ino))
 	    continue;
      st_list.push_back(st);
      st_inito_map[ST_st_idx(st)] = ino;
    }
  }

#ifdef KEY
  // Don't search GLOBAL_SYMTAB for unprocessed constants if there aren't any.
  // Bug 8756.
  static int prev_global_ST_Table_Size = -1;
  static BOOL unprocessed_global_constants = TRUE;
  if (prev_global_ST_Table_Size == ST_Table_Size(GLOBAL_SYMTAB) &&
      !unprocessed_global_constants) {
    goto skip_global_constants;
  }
  prev_global_ST_Table_Size = ST_Table_Size(GLOBAL_SYMTAB);
  unprocessed_global_constants = FALSE;
#endif

  // Then walk the CONSTANTs from the global table
  for (i = 1; i < ST_Table_Size(GLOBAL_SYMTAB); ++i) {
    ST* st = &St_Table(GLOBAL_SYMTAB,i);
    if (ST_class(st) == CLASS_CONST && !st_processed[ST_index(st)]) {
#ifdef KEY
      unprocessed_global_constants = TRUE;
#endif
#ifdef TARG_ST
      ST *base = Base_Symbol (st);
#else
      INT64 ofst;
      ST* base;
      Base_Symbol_And_Offset(st, &base, &ofst);
#endif
      if (ST_class(base) != CLASS_BLOCK || !STB_section(base)) {
        continue; // not allocated
      }
      if (Emit_Global_Data && SEC_is_merge(STB_section_idx(base))) {
        continue; // allocate in each .o
      }
      st_list.push_back(st);
    }
  }

#ifdef KEY
  skip_global_constants:
#endif

  // lexicographical sort: section, offset, size
  std::stable_sort (st_list.begin(), st_list.end(), size_lt);
  std::stable_sort (st_list.begin(), st_list.end(), offset_lt);
  std::stable_sort (st_list.begin(), st_list.end(), section_lt);

  for (st_iter = st_list.begin(); st_iter != st_list.end(); ++st_iter) {

    INT64 ofst;
    ST* base;
    ST* st = *st_iter;

#ifdef KEY // for PUs with nested functions, the INITO is used only to record
	   // the list of nested functions only, since the STs of the nested
	   // functions can only be entered in the global symbol table
     if (ST_class(st) == CLASS_PREG && 
         strncmp(ST_name(st), ".nested_functions", 17) == 0)
       continue;
#endif

    ST_INITO_MAP::iterator st_inito_entry = st_inito_map.find(ST_st_idx(st));

    if (st_inito_entry != st_inito_map.end()) {
      INITO* ino = (*st_inito_entry).second;
      Base_Symbol_And_Offset(st, &base, &ofst);
      if (ST_sclass(base) == SCLASS_EXTERN) {
        // ipa can cause it to be based on global extern,
        // in which case it is already emitted. 
        continue;
      }
      FmtAssert(ST_class(base) == CLASS_BLOCK && STB_section(base),
                ("inito (%s) not allocated? ", ST_name(st)));
      Init_Section(base); //make sure base is inited 
      // may need padding between objects in same section,
      // so always change origin
      Change_Section_Origin (base, ofst);
      if (!MiniR_Code) // alignment dumped in write_inito
#ifdef KEY
	if (CG_file_scope_asm_seen &&
	    TY_align (ST_type (st)) > 1)
	  fprintf( Asm_File, "\t%s\t%d\n", AS_ALIGN, 
#if defined(BUILD_OS_DARWIN)
                 logtwo (TY_align (ST_type (st)))
#else /* defined(BUILD_OS_DARWIN) */
		 TY_align (ST_type (st))
#endif /* defined(BUILD_OS_DARWIN) */
		   );
	else
	  fprintf ( Asm_File, "\t%s\t0\n", AS_ALIGN );
#endif
      Write_INITO (ino, STB_scninfo_idx(base), ofst);
#ifdef TARG_ST
      ofst = Write_INITO (ino, STB_scninfo_idx(base), ofst);
      Set_Section_Offset(base, ofst);
#endif      
    }

    else {
      st_processed[ST_index(st)] = TRUE;
      Base_Symbol_And_Offset(st, &base, &ofst);
      Init_Section(base); // make sure base is inited
      // we cannot assume that constants are sequentially ordered
      // by offset, because they are allocated on the fly as we
      // expand the whirl nodes.  So always reset the origin.
#ifdef KEY
      // Bug 617 - do not emit zero length strings 
      // (data_layout assigns same offset to two different constants
      // when one of them is a zero length constant).
      if ((TCON_ty(ST_tcon_val(st)) != MTYPE_STR &&
	   TCON_ty(ST_tcon_val(st)) != MTYPE_STRING) ||
	  TCON_str_len(ST_tcon_val(st)) != 0)
#endif
      Change_Section_Origin (base, ofst);
#ifdef KEY
      if (CG_file_scope_asm_seen &&
          TY_align (ST_type (st)) > 1)
        fprintf( MiniR_Code ? MiniR_File : Asm_File, "\t%s\t%d\n", AS_ALIGN, 
#if defined(BUILD_OS_DARWIN)
                 logtwo (TY_align (ST_type (st)))
#else /* defined(BUILD_OS_DARWIN) */
                 TY_align (ST_type (st))
#endif /* defined(BUILD_OS_DARWIN) */
               );
      else
        fprintf ( MiniR_Code ? MiniR_File : Asm_File, "\t%s\t0\n", AS_ALIGN );
#endif

#ifdef TARG_ST
      // [CL] handle the case of generation of symtab.s
      // in IPA mode. Although local, such symbols must
      // be 'promoted' to global so that they can be
      // accessed by the module they belong to
      if (Emit_Global_Data) {
	fprintf(Output_File, "\t%s\t", AS_GLOBAL);
	EMT_Write_Qualified_Tcon_Name(Output_File, st);
	fprintf(Output_File, "\n");
	// [CL] output as hidden/internal so that we are close to the
	// export_local meaning
#ifdef AS_INTERNAL
	if (ST_export(st) == EXPORT_LOCAL_INTERNAL) {
	  fprintf (Output_File, "\t%s\t", AS_INTERNAL);
	  EMT_Write_Qualified_Tcon_Name(Output_File, st);
	  fprintf(Output_File, "\n");
	} else
#endif
#ifdef AS_HIDDEN
	  {
	    fprintf (Output_File, "\t%s\t", AS_HIDDEN);
	    EMT_Write_Qualified_Tcon_Name(Output_File, st);
	    fprintf(Output_File, "\n");
	  }
#endif
      }
      fprintf (Output_File, "\t%s\t", AS_TYPE);
      EMT_Write_Qualified_Tcon_Name (Output_File, st);
#  ifdef AS_MOVEABLE
      BOOL moveable;
      int size = TY_size(ST_type(st));
      
      if (Emit_Global_Data) {
	/* TB: Tell this symbol is moveable when not in Emit_Global_Data (ipa) */
	moveable = FALSE;
      }
      else if (ST_is_initialized (st)) {
	/* [SC]: Not moveable if initializer is larger than symbol size */
	INITV_IDX inito_idx = ST_has_inito (st);
	if (inito_idx != 0 && Get_INITO_Size (inito_idx) > size)
	  moveable = FALSE;
      else
	  moveable = TRUE;
      }
      else {
	moveable = TRUE;
      }

      fprintf (Output_File, ", %s", AS_TYPE_OBJECT);
      if (moveable) {
	fprintf (Output_File, ", %s", AS_MOVEABLE);
      }
#  ifdef AS_USED
      /* TB: add gnu used  attibute when needed */
      if (ST_is_used(st)) {
	fprintf (Output_File, ", %s", AS_USED);
      }
#endif
      fprintf (Output_File, "\n");
#  else //AS_MOVEABLE
      fprintf (Output_File, ", %s\n", AS_TYPE_OBJECT);
#endif
#ifdef AS_SIZE
      {
	TCON tcon = ST_tcon_val(st);
	BOOL add_null = TCON_add_null(tcon);
	int size = 0;
	if ( TCON_ty(tcon) == MTYPE_STRING )
	  size = (Targ_String_Length (tcon) + (add_null ? 1 : 0));
	else
	  size = TY_size(Be_Type_Tbl(TCON_ty(tcon)));
	if (size != 0) {
	  /* if size is given, then emit value for asm */
	  fprintf (Output_File, "\t%s\t", AS_SIZE);
	  EMT_Write_Qualified_Tcon_Name(Output_File, st);
	  fprintf (Output_File, ", %d\n", size);
    }
  }
#endif
      EMT_Write_Qualified_Tcon_Name(Output_File, st);
      fprintf(Output_File, ":\n");
#else //Not TARG_ST
#ifdef KEY
#ifdef TARG_X8664 // limit to x86_64 for now
      if (!Emit_Global_Data) // not for symbols in ipa symtab
        fprintf ( MiniR_Code ? MiniR_File : Asm_File, TCON_Label_Format ":\n",
                  ST_IDX_index(ST_st_idx(st)) );
#endif  
#endif
#endif
      ofst = Write_TCON (&ST_tcon_val(st), STB_scninfo_idx(base), ofst, 1);
#ifdef TARG_ST
      Set_Section_Offset(base, ofst);
#endif
    }
  }

  return;
}



// Write all distr_array INITOs from the global symtab
static void
Process_Distr_Array ()
{
  for (UINT i = 1; i < INITO_Table_Size(GLOBAL_SYMTAB); ++i) {
    INITO* ino = &Inito_Table(GLOBAL_SYMTAB,i);
    ST* st = INITO_st(ino);
    if (!ST_is_not_used(st) &&
        ST_sclass(st) == SCLASS_DISTR_ARRAY) {
      INT64 ofst;
      ST* base;
      Base_Symbol_And_Offset(st, &base, &ofst);
      FmtAssert(ST_class(base) == CLASS_BLOCK && STB_section(base),
                ("inito (%s) not allocated?", ST_name(st)));
      Init_Section(base);
      Change_Section_Origin(base, ofst);
#ifdef KEY
      if (CG_file_scope_asm_seen &&
          TY_align (ST_type (st)) > 1)
        fprintf( Asm_File, "\t%s\t%d\n", AS_ALIGN, 
#if defined(BUILD_OS_DARWIN)
                 logtwo (TY_align (ST_type (st)))
#else /* defined(BUILD_OS_DARWIN) */
                 TY_align (ST_type (st))
#endif /* defined(BUILD_OS_DARWIN) */
               );
      else
        fprintf ( Asm_File, "\t%s\t0\n", AS_ALIGN );
#endif
      Write_INITO(ino, STB_scninfo_idx(base), ofst);
    }
  }
}


/* iterate through the global symbols and write (s)bss symbols to sections */
static void
Process_Bss_Data (SYMTAB_IDX stab)
{
  // To guarantee in increasing order,
  // create vector of st*, then stable_sort on section,
  // then stable_sort on offset. 
  vector< ST* > bss_list;
  vector< ST* >::iterator bssp;

  // This routine can be called multiple times for the global symtab;
  // we do this so that objects are emitted in order.
  // For each section, some objects are in local symtab, and some in global.
  // We keep track of the last global symbol that has been processed
  // so that we only process new symbols.

  static UINT last_global_index = 1;
  UINT first_index = (stab == GLOBAL_SYMTAB ? last_global_index : 1);
  
  for (UINT i = first_index; i < ST_Table_Size(stab); ++i) {
    ST* sym = &St_Table(stab,i);
    if (ST_class(sym) == CLASS_BLOCK)
      continue;	// not a leaf symbol
    if (!Has_Base_Block(sym))
      continue;	// not a data symbol
#ifdef KEY // bug 3182: to avoid .org backwards with cray pointers
    if (ST_sclass(sym) == SCLASS_PSTATIC &&
	(&Get_Current_PU() != NULL) && PU_ftn_lang(Get_Current_PU()) && 
	ST_class(ST_base(sym)) != CLASS_BLOCK &&
	TY_size(ST_type(ST_base(sym))) == Pointer_Size &&
	ST_ofst(sym) == 0 && TY_size(ST_type(sym)) > Pointer_Size) {
      DevWarn("Process_Bss_Data: skipped symbol %s because based on cray pointer", ST_name(sym));
      continue; // is based on a cray pointer
    }

    // bug 14507: sym will be replaced by dedicated reg, so don't emit sym
    if (ST_assigned_to_dedicated_preg(sym))
      continue;
#endif
    if (ST_sclass(sym) == SCLASS_UGLOBAL ||
        ST_sclass(sym) == SCLASS_FSTATIC ||
        ST_sclass(sym) == SCLASS_PSTATIC) {
      bss_list.push_back (sym);	// bss symbol
    }
  }

  if (stab == GLOBAL_SYMTAB) {
    last_global_index = ST_Table_Size(GLOBAL_SYMTAB);
  }

  std::stable_sort (bss_list.begin(), bss_list.end(), size_lt);
  std::stable_sort (bss_list.begin(), bss_list.end(), offset_lt);
  std::stable_sort (bss_list.begin(), bss_list.end(), section_lt);

  ST*   sym;
  ST*   next_sym;
  ST*   base;
  ST*   next_base;
  INT64 ofst;
  INT64 next_ofst;
  INT64 size;
  INT64 size_to_skip;
  INT64 not_yet_skip_amt = 0;	// bug 10678
#ifdef KEY
  ST*	last_base = NULL;	// bug 10678
  PU	*pu = &Get_Current_PU();

  // bug 13829: if not inside any PU, get 1st PU
  if (pu == NULL && Pu_Table.Size() > 1)
    pu = &(Pu_Table[(PU_IDX)1]);
#endif

  for (bssp = bss_list.begin(); bssp != bss_list.end(); ++bssp) {

	sym = *bssp;
	Base_Symbol_And_Offset(sym, &base, &ofst);
#if defined(BUILD_OS_DARWIN)
        if (!strcmp(ST_name(base), BSS_RAW_NAME)) {
	  darwin_bss::remember_bss_symbol(sym);
	  continue;
	}
#endif  /* defined(BUILD_OS_DARWIN) */
#ifdef KEY // bug 10678
	if (base != last_base) {
	  if (last_base != NULL && not_yet_skip_amt > 0)
	    ASM_DIR_SKIP(Asm_File, not_yet_skip_amt);
	  // initialize for new base
	  last_base = base;
	  not_yet_skip_amt = 0;
	}
#endif
	if (ST_class(base) != CLASS_BLOCK || !STB_section(base))
		continue;	/* not allocated */
	if (!STB_nobits(base))
		continue;	/* not a bss symbol */

#ifdef KEY
	// Compute SIZE now.  The x86-64 code below relies on SIZE to determine
	// if Change_Section_Origin is needed.  Bug 13863.
	size = TY_size(ST_type(sym));
	// C++ requires empty classes to have unique addresses.
	if (size == 0 && (pu == NULL || PU_cxx_lang(*pu)/*bug 13826*/))
	  size = 1;
#endif

#ifdef TARG_X8664
	// Fix bug 617
	// Do not emit .org for any symbols with section attributes.
	if (Assembly) {
	  ST* tmp_base = sym;
	  BOOL has_named_section = FALSE;
	  if ( ST_base(tmp_base) == base && ST_has_named_section (tmp_base)) {
#if defined(BUILD_OS_DARWIN)
	    emit_section_directive(base);
#else /* defined(BUILD_OS_DARWIN) */
	    if (Assembly)
	      fprintf ( Asm_File, "\n\t%s %s\n", AS_SECTION, ST_name(base));
#endif /* defined(BUILD_OS_DARWIN) */
	    has_named_section = TRUE;
	  }
	  while ( ST_base(tmp_base) != base ) {
	    tmp_base = ST_base(tmp_base);	  
	    if ( ST_has_named_section (tmp_base) ) {
#if defined(BUILD_OS_DARWIN)
	      emit_section_directive(base);
#else /* defined(BUILD_OS_DARWIN) */
	      fprintf ( Asm_File, "\n\t%s %s\n", AS_SECTION, ST_name(base));
#endif /* defined(BUILD_OS_DARWIN) */
	      has_named_section = TRUE;
	      break;
	    }
	  }
	  if (!has_named_section && size != 0) {
	    Change_Section_Origin (base, ofst);	    
	    if ( !Simd_Reallocate_Objects ) {
              // If Simd_Reallocate_Objects, we already emitted alignment 
	      // inside Change_Section_Origin - may be screwed up but we did
	      if ( STB_align(base) == 16 ) {
		// IPA probably realigned a FSTATIC variable destined for 
		// this EXTERN variable 
		// Get the alignment from the ST_type
		fprintf( Asm_File, "\t%s\t%d\n", AS_ALIGN, 
#if defined(BUILD_OS_DARWIN)
			logtwo(TY_align( ST_type ( sym ) ))
#else /* defined(BUILD_OS_DARWIN) */
			TY_align( ST_type ( sym ) )
#endif /* defined(BUILD_OS_DARWIN) */
			);
	      } else
		fprintf( Asm_File, "\t%s\t0\n", AS_ALIGN );
#ifdef KEY
	    } else if ( CG_file_scope_asm_seen &&
	                TY_align( ST_type ( sym ) ) > 1 ) {
	      fprintf( Asm_File, "\t%s\t%d\n", AS_ALIGN, 
#if defined(BUILD_OS_DARWIN)
			logtwo(TY_align( ST_type ( sym ) ))
#else /* defined(BUILD_OS_DARWIN) */
			TY_align( ST_type ( sym ) )
#endif /* defined(BUILD_OS_DARWIN) */
			);
#endif /* KEY */
	    } else
		fprintf(Asm_File, "\t%s\t0\n", AS_ALIGN );
	  }
	} 
#else
	Change_Section_Origin (base, ofst);
#endif
	if (Assembly || MiniR_Code) {
#ifdef KEY // bug 11593: do not generate a definition for aliased variables
		if (ST_class(sym) == CLASS_VAR &&
		    ST_base_idx(sym) != ST_st_idx(sym) &&
		    !ST_is_equivalenced(sym) &&
		    ST_class(ST_base(sym)) != CLASS_BLOCK &&
      		    pu != NULL && ! PU_ftn_lang(*pu) /* bug 13585 */)
		  goto skip_definition;
#endif

#ifndef KEY	// SIZE computed above.
		size = TY_size(ST_type(sym));
#endif
		Print_Label (Asm_File, &objects_MINIR, sym, size);
		if (MiniR_Code) {
		  objects_MINIR << "    section: " << ST_name(base) << "\n";
		  if (ST_sclass(sym) == SCLASS_FSTATIC ||
		      ST_sclass(sym) == SCLASS_PSTATIC)
		    objects_MINIR << "    attr: static\n";
		  if (ST_sclass(sym) == SCLASS_UGLOBAL)
		    objects_MINIR << "    attr: global\n";
		}
		size_to_skip = size;
		// before emitting space,
		// first check whether next symbol has same offset.
		// can have overlapping symbols that are equivalenced.
		if (bssp+1 != bss_list.end()) {
			next_sym = *(bssp+1);
			Base_Symbol_And_Offset(next_sym, &next_base, &next_ofst);
			if (next_base == base && next_ofst == ofst) {
				// skip to next iteration
				// so label printed before space emitted.
				continue;
			}
			else if (next_base == base && next_ofst < (ofst+size)) {
				// have label in middle of structure,
				// so don't emit space for full structure.
				not_yet_skip_amt = MAX(not_yet_skip_amt, size); // bug 10678
				size_to_skip = next_ofst - ofst;
			}
			else if (next_base == base && next_ofst > (ofst+size))
			  size_to_skip = MAX(size_to_skip, next_ofst - ofst);
		}
		// assume here that if multiple symbols with same offset,
		// are sorted so that largest size is last.
		if (size_to_skip > 0 && Assembly) {
#ifdef TARG_MIPS
		    if (CG_emit_non_gas_syntax)
		      fprintf(Asm_File, "\t%s %" SCNd64 "\n", ".space", (INT64)size_to_skip);
		    else
#endif
			ASM_DIR_SKIP(Asm_File, size_to_skip);
		    not_yet_skip_amt = MAX(0, not_yet_skip_amt-size_to_skip); // bug 10678
		}
	}
skip_definition:
	if (generate_elf_symbols && ! ST_is_export_local(sym)) {
		EMT_Put_Elf_Symbol (sym);
	}
  }
#ifdef KEY // bug 10678
  if (bss_list.size() > 0) {
    if (not_yet_skip_amt > 0) {
      ASM_DIR_SKIP(Asm_File, not_yet_skip_amt);
    }
  }
#endif
}


static INT
Check_If_Should_Align_PU (INT curpc)
{
  INT q;
  if (Align_Instructions) {
	q = Align_Instructions;
  }
  else if (OPT_Space) {
	return 0;
  }
  else {
	q = CGTARG_Text_Alignment();
  }
  q /= INST_BYTES;	/* so word-sized */
  return (q - ((curpc/INST_BYTES) % q)) % q;
}

/* ====================================================================
 *   Create_Cold_Text_Section
 *
 *    Scan the BBs in the region looking for cold BBs. If one is found,
 *    create the cold text section if it hasn't already been created.
 *    Also set the BB_cold BB flag accordingly.
 * ====================================================================
 */
static void
Create_Cold_Text_Section (void)
{
  BB *bb;

  //  FmtAssert(text_base != NULL, ("emit: text_base is null"));
  for (bb = REGION_First_BB; bb; bb = BB_next(bb)) {
#ifdef TARG_ST
    // [CG] hot/cold placement for .text section only
    if (Is_Text_Section(text_base) &&
#else 
    if (
#endif
	EMIT_use_cold_section && BB_Is_Cold(bb)) {
      if (cold_base == NULL) {
	ST *st = Copy_ST(text_base);
	Set_ST_blk(st, Copy_BLK(ST_blk(text_base)));
	Set_ST_name (st, Save_Str2(ELF_TEXT, ".cold"));
	Set_STB_size (st, 0);
	Set_STB_scninfo_idx(st, 0);
	Set_STB_section_idx(st, STB_section_idx(text_base));
	Init_Section(st);
	cold_base = st;

	if (generate_elf_symbols) {
	  cold_section = em_scn[STB_scninfo_idx(cold_base)].scninfo;
	}
      }

      /* Check the remaining BBs in the region to verify they are
       * are cold. cgemit doesn't require this attribute, but currently
       * this is how the cold region is generated, so it's helpful
       * to verify it since someone forgetting to set the rid on a
       * new BB will cause trouble.
       */
      do {
	FmtAssert(BB_Is_Cold(bb),
		  ("cgemit: hot BB:%d found in cold region", BB_id(bb)));
	Set_BB_cold(bb);
      } while (bb = BB_next(bb));

      return;
    }

    Reset_BB_cold(bb);
  }
}


/* ====================================================================
 *   Setup_Text_Section_For_BB
 *
 *   Set PU_base, PU_section and PC according to whether <bb> is
 *   in the hot or cold region.
 * ====================================================================
 */
static void
Setup_Text_Section_For_BB (BB *bb)
{
  BOOL cold_bb = BB_cold(bb);
  PU_base = cold_bb ? cold_base : text_base;
  if (cur_section != PU_base) {
#if defined(BUILD_OS_DARWIN)
    emit_section_directive(PU_base);
#else /* defined(BUILD_OS_DARWIN) */
    if (Assembly) {
      fprintf (Asm_File, "\n\t%s %s\n", AS_SECTION, ST_name(PU_base));
#ifdef TARG_ST
      // [CL] force alignment if we have just switched to cold
      // section for the current PU (as alignment constraints in
      // hb_hazards.cxx:Make_Bundles() are reset for each new
      // PU, we must reset alignment here)
      if ( (PU_base == cold_base) && (DEFAULT_FUNCTION_ALIGNMENT) ) {
	fprintf(Asm_File, "\t%s %d\n", AS_ALIGN, DEFAULT_FUNCTION_ALIGNMENT);
      }
#endif
    }
#endif /* defined(BUILD_OS_DARWIN) */
    if (cold_bb) {
      PU_section = cold_section;
      text_PC = PC;
      PC = cold_PC;
    } else {
      PU_section = text_section;
      cold_PC = PC;
      PC = text_PC;
    }
    cur_section = PU_base;
  }
}

static LABEL_IDX       prev_pu_last_label  = LABEL_IDX_ZERO;
static Dwarf_Unsigned  prev_pu_base_elfsym = 0;
static PU_IDX          prev_pu_pu_idx      = (PU_IDX) 0;
static Dwarf_Unsigned  prev_pu_end_offset_from_last_label = 0;
static char           *prev_pu_last_label_name = NULL;
static Dwarf_Unsigned  prev_pu_last_offset = 0;

static void
cache_last_label_info(LABEL_IDX      label_idx,
		      Dwarf_Unsigned base_elf_idx,
		      PU_IDX         pu_idx,
		      Dwarf_Unsigned end_offset)
{
  prev_pu_last_label  = label_idx;
  prev_pu_base_elfsym = base_elf_idx;
  prev_pu_pu_idx      = pu_idx;
  prev_pu_end_offset_from_last_label = end_offset;
  prev_pu_last_label_name = LABEL_name(label_idx);
  prev_pu_last_offset = Get_Label_Offset(label_idx);
}

static void
end_previous_text_region(pSCNINFO scninfo,
			 INT      end_offset)
{
  Em_Dwarf_End_Text_Region_Semi_Symbolic(scninfo,
					 end_offset,
					 Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
							       prev_pu_last_label,
							       prev_pu_base_elfsym,
							       prev_pu_pu_idx,
							       prev_pu_last_label_name,
							       prev_pu_last_offset),
					 prev_pu_end_offset_from_last_label);
}

/* Setup, and create if necessary, the text section of the PU.  */
static void
Setup_Text_Section_For_PU (ST *pu)
{
  static ST *orig_text_base = NULL;
  ST *old_base = PU_base;
  INT i;
  if (text_base == NULL) {
	text_base = SEC_block(_SEC_TEXT);
  }
  orig_text_base = text_base;

  current_pu = ST_pu(pu);

  Initial_Pu_Label = LABEL_IDX_ZERO;

  if ( ! Object_Code && generate_elf_symbols) {
  	// didn't actually write instructions,
	// but want the offset to be up-to-date.
	Em_Change_Section_Origin (
		em_scn[STB_scninfo_idx(orig_text_base)].scninfo,
		text_PC);
  }

  if (Section_For_Each_Function || PU_in_elf_section(current_pu)) {
	/* create new text section */
	text_base = Copy_ST(orig_text_base);
	Set_ST_blk(text_base, Copy_BLK(ST_blk(orig_text_base)));
	if (Get_Trace ( TP_EMIT,128 )) {
		// use same .text name for each section
		Set_ST_name (text_base, ST_name_idx(orig_text_base));
	}
	else {
		char buf[16];
		sprintf(buf, "%s.", ELF_TEXT);
		Set_ST_name (text_base, Save_Str2(buf, ST_name(pu)));
	}
	Set_STB_size (text_base, 0);
	Set_STB_scninfo_idx(text_base, 0);
	Set_STB_section_idx(text_base, STB_section_idx(orig_text_base));
	Init_Section (text_base);
	text_PC = 0;	/* starting new offsets */
  }
  else if (ST_base(pu) != text_base) {
	// e.g. for section attributes.
	text_base = ST_base(pu);
	text_PC = STB_size(text_base);
  }

  Set_STB_scninfo_idx(SP_Sym, STB_scninfo_idx(text_base));
  Set_STB_scninfo_idx(FP_Sym, STB_scninfo_idx(text_base));

  cur_section = text_base;
  if (generate_elf_symbols) {
    text_section = em_scn[STB_scninfo_idx(text_base)].scninfo;
    PU_section = text_section;
    i = Em_Get_Section_Offset (PU_section);
    Is_True(i == text_PC, ("Setup_Text_Section_For_PU: PC doesn't match"));
    text_PC = i;
  }
#if defined(BUILD_OS_DARWIN)
  emit_section_directive(text_base);
#else /* defined(BUILD_OS_DARWIN) */
  if (Assembly) fprintf (Asm_File, "\t%s %s\n", AS_SECTION, ST_name(text_base));
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef TEMPORARY_STABS_FOR_GDB
  // This is an ugly hack to enable basic debugging for IA-32 target
  if (Assembly && Debug_Level > 0) {
    static BOOL marked_text_start = FALSE;
    if (!marked_text_start) {
      fputs (".Ltext0:\n", Asm_File);
      marked_text_start = TRUE;
    }
  }
#endif

  i = Check_If_Should_Align_PU (text_PC);
  if (i != 0) {
    if (Assembly) {
      UINT32 tmp, power;
      power = 0;
      for (tmp = STB_align(text_base); tmp > 1; tmp >>= 1) power++;
#ifdef KEY
#if defined(TARG_X8664) && ! defined(BUILD_OS_DARWIN)
      fprintf (Asm_File, "\t%s\t%d\n", AS_ALIGN, 1 << power );
#else
      fprintf (Asm_File, "\t%s\t%d\n", AS_ALIGN, power);
#endif
#else
      ASM_DIR_ALIGN(power, text_base);
#endif
    }
    if (Object_Code) {
      // these bytes will never be executed so just insert 0's and
      // then we don't have to worry about how to generate a nop for
      // the target arch.
      Em_Add_Zeros_To_Scn (PU_section, i * INST_BYTES, 1);
    }
    // increment text_PC by 'num' bundles
    text_PC = text_PC + (i * INST_BYTES);
  }

#ifndef KEY
  // hack for supporting dwarf generation in assembly (suneel)
  Last_Label = Gen_Label_For_BB (REGION_First_BB);

  Offset_From_Last_Label = 0;
  if (Initial_Pu_Label == LABEL_IDX_ZERO) {
    Initial_Pu_Label = Last_Label;
  }
#else
  {
    // The original code would create a label (if required) for the first BB
    // and then use that label in DW_TAG_low_pc and DW_TAG_high_pc. To set 
    // DW_TAG_high_pc, the original mechanism used to count instruction bytes. 
    // This is hard to do. Hence we would generate a new label (Last_Label) 
    // and attach it to the end of the PU.
    char *buf;
    LABEL *label;
    buf = (char *)alloca(strlen(Cur_PU_Name) + /* EXTRA_NAME_LEN */ 32);
    sprintf(buf, ".LDWend_%s", Cur_PU_Name);
    label = &New_LABEL(CURRENT_SYMTAB, Last_Label);
    LABEL_Init (*label, Save_Str(buf), LKIND_DEFAULT);
  }

  Offset_From_Last_Label = 0;
  if (Initial_Pu_Label == LABEL_IDX_ZERO) {
    Initial_Pu_Label = Gen_Label_For_BB (REGION_First_BB);
  }
#endif

  /* check if we are changing sections. */
  if (text_base != old_base) {
    if (generate_elf_symbols && old_base != NULL) {
      pSCNINFO old_section = em_scn[STB_scninfo_idx(old_base)].scninfo;
      // Arange is symbolic; line number entries (if used) are not.
      end_previous_text_region(old_section, Em_Get_Section_Offset(old_section));
    }
    if (generate_dwarf) {
#ifdef KEY
      // Bug 2468 - use the appropriate labels for the debug_aranges
      LABEL_IDX Text_Label = LABEL_IDX_ZERO;
      Text_Label = Gen_Label_For_BB (REGION_First_BB);
      Em_Dwarf_Start_Text_Region_Semi_Symbolic (
        PU_section, text_PC,
        Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
                              Text_Label,
                              ST_elf_index(text_base)),
        Offset_From_Last_Label);
#else    	
	Em_Dwarf_Start_Text_Region_Semi_Symbolic (PU_section, text_PC,
					      Cg_Dwarf_Symtab_Entry(CGD_LABIDX,
								    Last_Label,
								    ST_elf_index(text_base)),
					      Offset_From_Last_Label);
#endif
	}
  }

  PC = text_PC;
  PU_base = text_base;
}

/* ====================================================================
 * EMT_Emit_PU
 *
 * Assemble a program unit.
 * ====================================================================
*/
void
EMT_Emit_PU ( ST *pu, DST_IDX pu_dst, WN *rwn )
{
  ST *sym;
  ST *base;
  BB *bb;
  INT Initial_Pu_PC;
  INT64 ofst;
  INT i;

#ifdef KEY
  // bugs 2178, 2152
  if ( PU_is_extern_inline (Pu_Table[ST_pu(pu)]) ) {
    if ( OPT_Cyg_Instrument >= 4 && ! PU_no_instrument(Pu_Table[ST_pu(pu)]) ) {
      // Bug 13801
      DevWarn( "Keeping %s for -finstrument-functions", ST_name(pu) );
    } else return;
  }
  // Notes on bugs 2839 and 2934: CG_emit_asm_dwarf controls generate_dwarf
  // and generate_elf_symbols. generate_dwarf and generate_elf_symbols are
  // used interchangeably in cgemit for generating dwarf code as well as to 
  // emit other useful info such as "weak" symbols. This is why we can not
  // turn off dwarf generation (for non -g/C++ compilation).
#endif

  Trace_Inst	= Get_Trace ( TP_EMIT,1 );
  BOOL trace_unwind = Get_Trace (TP_EMIT, 64);

  Init_Unwind_Info (trace_unwind);
#ifdef KEY
  // bug 3031, 4814: initialize these unconditionally
  // bug 11034 : dynamic memory allocation
  init_dwarf_pu_tables();
#endif // KEY

#if defined(TARG_MIPS) || defined(TARG_MVP)
  gp_stored_for_pu = FALSE;
  ra_stored_for_pu = FALSE;
#endif

#ifdef ICE9A_HW_WORKAROUND
  muld_shadow = 0;
  round_hazard_op = NULL;
#endif

  /* In the IA-32 case, we need to convert fp register references
   * so that they reference entries in the fp register stack with the
   * proper offset.
   */
  STACK_FP_Fixup_PU();

  if ( Run_prompf ) {
    const char *path = Anl_File_Path();
    anl_file = fopen(path, "a");
    fputc ('\n', anl_file);
  }

  Init_ST_elf_index(CURRENT_SYMTAB);

  cur_section = NULL;

  // initialize any new global sections
  // We don't do this in EMT_Begin_File because may not have done
  // data layout at that point.
  static UINT last_global_index = 1;
#ifdef KEY
  static BOOL CG_LNO_Alignment_Overridden = FALSE;

  // Bug 2028 - If the source language is Fortran, we always generate
  // 16 byte alignment, so we do not have to look at the 
  // CG_LNO_Alignment_Overridden flag now. In general, it is okay to emit
  // alignment directive for common blocks as many times as long as the new
  // alignments are equal or more restrictive than previous versions.
  // For bug 2028 though, there is a user local variable with the same name as
  // a common block in a different PU and that causes the assembler to complain
  // about the alignment re-directives. For other languages we should not have 
  // this naming clash and that is the convenience to check for F77 or F90 here.
  if (LNO_Run_Simd && Simd_Align && !CG_LNO_Alignment_Overridden &&
      PU_ftn_lang(Get_Current_PU()))
    CG_LNO_Alignment_Overridden = TRUE;

  if (LNO_Run_Simd && Simd_Align && !CG_LNO_Alignment_Overridden) {
    // If LNO succeeds in vectorization, emit alignment attributes again.
    last_global_index = 1;
    CG_LNO_Alignment_Overridden = TRUE;
  }
#endif
  for (i = last_global_index; i < ST_Table_Size(GLOBAL_SYMTAB); ++i) {
	ST* sym = &St_Table(GLOBAL_SYMTAB,i);
	if (ST_class(sym) == CLASS_BLOCK && STB_section(sym)) {
		Init_Section(sym);
	}
	// emit commons here so order is preserved for datapools
    	if (ST_sclass(sym) == SCLASS_COMMON) {
		if (ST_is_not_used (sym)) continue;
#ifdef KEY  // bug 14507: sym will be replaced by dedicated reg, so don't emit sym
		if (ST_assigned_to_dedicated_preg(sym)) continue;
#endif
      		EMT_Put_Elf_Symbol (sym);
    	}
  }
  last_global_index = ST_Table_Size(GLOBAL_SYMTAB);

  // emit global bss first so .org is correct
  Process_Bss_Data (GLOBAL_SYMTAB);
#ifdef KEY
  if (LNO_Run_Simd)
    Simd_Align = FALSE;
#endif

  /* Initialize any sections that might have been created by the backend. */
  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
	base = Base_Symbol(sym);
	if (ST_class(base) == CLASS_BLOCK && STB_section(base)) {
		Init_Section(base);
	}
  }

  Create_Cold_Text_Section();

  Setup_Text_Section_For_PU (pu);

  Initial_Pu_PC = PC;
  Set_ST_ofst(pu, PC);

  /* Assign addresses to all local labels. Fixup long branches.
     Perform branch target optimization.
  */
  R_Resolve_Branches (pu);

  if (Object_Code) {
    Em_Add_New_Event (EK_ENTRY, PC, 0, 0, 0, PU_section);
  }
  if ( Assembly ) {
#ifdef TARG_X8664
    if (CG_p2align) 
      fputs ("\t.p2align 4,,15\n", Asm_File);
    else if (PU_src_lang (Get_Current_PU()) & PU_CXX_LANG) {
      // g++ requires a minimum alignment because it uses the least significant
      // bit of function pointers to store the virtual bit.
      fputs ("\t.p2align 1,,\n", Asm_File);
    }
#endif
    fprintf ( Asm_File, "\n\t%s Program Unit: %s\n", ASM_CMNT, ST_name(pu) );
    if (AS_ENT &&
#ifdef KEY
	! CG_inhibit_size_directive
#endif
	    ) CGEMIT_Prn_Ent_In_Asm (pu);
#if !defined(TARG_MIPS) && !defined(TARG_MVP)
#ifdef TEMPORARY_STABS_FOR_GDB
    // This is an ugly hack to enable basic debugging for IA-32 target
    if (Debug_Level > 0) {
      fprintf(Asm_File, ".stabs \"%s:F(0,1)\",36,0,0,%s\n", ST_name(pu), ST_name(pu));
      fprintf(Asm_File, "\t%s\t%s,%s\n", AS_TYPE, ST_name(pu), AS_TYPE_FUNC);
    }
#endif
#else
    if (!CG_emit_non_gas_syntax) {
      fprintf (Asm_File, "\t%s\t", AS_TYPE);
      EMT_Write_Qualified_Name (Asm_File, pu);
      fprintf (Asm_File, ", %s\n", AS_TYPE_FUNC);
    }
#endif
    Print_Label (Asm_File, NULL, pu, 0);
    CGEMIT_Gen_Asm_Frame (Frame_Len);
  }

  if (Assembly) {
    if (cur_section != PU_base) {
	/* reset to text section */
#if defined(BUILD_OS_DARWIN)
    	emit_section_directive(PU_base);
#else /* defined(BUILD_OS_DARWIN) */
    	fprintf (Asm_File, "\n\t%s %s\n", AS_SECTION, ST_name(PU_base));
#endif /* defined(BUILD_OS_DARWIN) */
	cur_section = PU_base;
    }
  }
  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
    if (ST_is_not_used(sym)) continue;
    if (ST_sclass(sym) == SCLASS_COMMON) {
      EMT_Put_Elf_Symbol (sym);
    }
    /* put all extern symbols into the elf symbol table. */
    if ((PU_src_lang(Get_Current_PU()) == PU_F77_LANG 
	|| PU_src_lang(Get_Current_PU()) == PU_F90_LANG) &&
	ST_sclass(sym) == SCLASS_EXTERN && 
	! ST_is_export_local(sym)) 
    {
      if (Assembly) fprintf (Asm_File, "\t%s\t %s\n", AS_GLOBAL,
#if defined(BUILD_OS_DARWIN)
        underscorify(ST_name(sym))
#else /* defined(BUILD_OS_DARWIN) */
        ST_name(sym)
#endif /* defined(BUILD_OS_DARWIN) */
	);
      if (Object_Code) EMT_Put_Elf_Symbol (sym);
    }

    if (Assembly) {
      if (ST_class(sym) == CLASS_VAR && ST_sclass(sym) == SCLASS_AUTO) {
	if (Has_Base_Block(sym)) {
	  Base_Symbol_And_Offset(sym, &base, &ofst);
	  fprintf ( Asm_File, "\t%s %s = %" SCNd64 "\n",
		    ASM_CMNT, ST_name(sym), ofst);
	}
      }
    }
  }

#if 0
// This code has been moved to EMT_Assemble_BB with the aim of emitting
// it at the start of the 1st BB in the PU.
#ifdef TARG_X8664
  if( Assembly &&
      ( strcmp( Cur_PU_Name, "MAIN__" ) == 0 ||
	strcmp( Cur_PU_Name, "main" ) == 0 ) ){
    CGEMIT_Setup_Ctrl_Register( Asm_File );
  }
#endif  
#endif

  /* Assemble each basic block in the PU */
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    Setup_Text_Section_For_BB(bb);
    EMT_Assemble_BB (bb, rwn);
  }

#ifdef KEY
  // Emit Last_Label at the end of the PU to guide Dwarf DW_AT_high_pc
  fprintf( Asm_File, "%s:\n", LABEL_name(Last_Label));
  Label_Last_BB_PU_Entry[pu_entries] = Last_Label;
#if ! defined(BUILD_OS_DARWIN)
  // Mach-O as 1.38 doesn't support .size
  // Bug 1275
  fprintf( Asm_File, "\t.size %s, %s-%s\n", 
	   ST_name(pu), LABEL_name(Last_Label), ST_name(pu));
#endif /* defined(BUILD_OS_DARWIN) */
#endif
  /* Revert back to the text section to end the PU. */
  Setup_Text_Section_For_BB(REGION_First_BB);
  Is_True(PU_base == text_base, ("first region BB was not in text section"));

  /* Emit the stuff needed at the end of the PU. */
  const char *end = AS_END;
  if (Assembly) {
#ifdef TARG_MIPS
    if (CG_emit_non_gas_syntax) {
      fprintf ( Asm_File, "\t%s\t", AS_END);
      EMT_Write_Qualified_Name(Asm_File, pu);
      fputc ( '\n', Asm_File);
    }
    else
#endif
    if (end
#ifdef KEY
	&& ! CG_inhibit_size_directive
#endif
	    ) {
      fprintf ( Asm_File, "\t%s\t", end);
      EMT_Write_Qualified_Name(Asm_File, pu);
      fputc ( '\n', Asm_File);
    }
  }
  
  /* Emit the initialized data associated with this PU. */
  Process_Initos_And_Literals (CURRENT_SYMTAB);
  Process_Bss_Data (CURRENT_SYMTAB);
#ifdef KEY
  Simd_Reallocate_Objects = FALSE;
#endif

  if (generate_dwarf) {
    Elf64_Word symindex;
    INT eh_offset;
    BOOL has_exc_scopes = PU_has_exc_scopes(ST_pu(pu));
    if (Object_Code)
    	Em_Add_New_Event (EK_PEND, PC - INST_BYTES, 0, 0, 0, PU_section);
    /* get exception handling info */ 
    if (!CXX_Exceptions_On && has_exc_scopes) {
      eh_offset = symindex = (Elf64_Word)DW_DLX_EH_OFFSET_UNAVAILABLE;
    }
    else if (!has_exc_scopes) {
	eh_offset = symindex = (Elf64_Word)DW_DLX_NO_EH_OFFSET;
    }
    else {
      sym = EH_Get_PU_Range_ST();
      if (sym != NULL) {
	Base_Symbol_And_Offset (sym, &base, &ofst);
	eh_offset = ofst;
	Init_Section(base);	/* make sure eh_region is inited */
#ifdef KEY
// emit the begin label instead of the section name, since the section name
// is same for different PUs, the label is different.
	symindex = EMT_Put_Elf_Symbol (sym);
// the above calculated offset is w.r.t the base, since we are not using the
// base, the offset is 0.
	eh_offset = 0;
#else
	symindex = ST_elf_index(base);
#endif // KEY
      } else {
	eh_offset = symindex = (Elf64_Word)DW_DLX_NO_EH_OFFSET;
      }
    }
    // Cg_Dwarf_Process_PU (PU_section, Initial_Pu_PC, PC, pu, pu_dst, symindex, eh_offset);
#ifdef TARG_X8664
    Cg_Dwarf_Process_PU (
		Em_Create_Section_Symbol(PU_section),
		Initial_Pu_Label,
		// For Opteron, we generate a last label at the end of the PU
		Last_Label, 
		&Label_pushbp[0],
		&Label_movespbp[0],
		&Label_adjustsp[0],
		&Label_Callee_Saved_Reg[0],
		&Label_First_BB_PU_Entry[0],
		&Label_Last_BB_PU_Entry[0],
		pu_entries,
		0,
		pu, pu_dst, symindex, eh_offset,
		// The following two arguments need to go away
		// once libunwind provides an interface that lets
		// us specify ranges symbolically.
		Initial_Pu_PC, PC);
#elif defined(TARG_MIPS) || defined(TARG_MVP)
    Cg_Dwarf_Process_PU (
		Em_Create_Section_Symbol(PU_section),
		Initial_Pu_Label,
		Last_Label,
		&Label_adjustsp[0],
		&Label_Callee_Saved_Reg[0],
		new_cfa_offset,
		0,
		pu, pu_dst, symindex, eh_offset,
		// The following two arguments need to go away
		// once libunwind provides an interface that lets
		// us specify ranges symbolically.
		Initial_Pu_PC, PC);
#else
    Cg_Dwarf_Process_PU (
		Em_Create_Section_Symbol(PU_section),
		Initial_Pu_Label,
		Last_Label, Offset_From_Last_Label,
		pu, pu_dst, symindex, eh_offset,
		// The following two arguments need to go away
		// once libunwind provides an interface that lets
		// us specify ranges symbolically.
		Initial_Pu_PC, PC);
#endif // TARG_X8664
  }

  if (Run_prompf) {
    fputc ('\n', anl_file);
    fclose(anl_file);
  }

  PU_Size = PC - Initial_Pu_PC;
  Set_STB_size (PU_base, PC);
  text_PC = PC;

  if (generate_dwarf) {
    // The final label in this PU is liable to get used in computing
    // arguments to Em_Dwarf_End_Text_Region_Semi_Symbolic, so we need
    // to squirrel away information about it.
    cache_last_label_info (Last_Label,
		Em_Create_Section_Symbol(PU_section),
		ST_pu(pu),
		Offset_From_Last_Label);
  }

  Finalize_Unwind_Info();
}


void
MINIR_Dump_PU( ST *pu, DST_IDX pu_dst, WN *rwn )
{
  ST *sym;
  ST *base;
  BB *bb;
  INT Initial_Pu_PC;
  const char *sep;
  const char *pfx;
  const char *bb_pfx;
  static int function_num;
  int i;

  FmtAssert(REGION_First_BB != NULL, ("Precondition on function failed"));
  FmtAssert(Cur_PU_Name != NULL, ("Precondition on function failed"));

  if ( PU_is_extern_inline (Pu_Table[ST_pu(pu)]) ) {
    return;
  }
  Init_ST_elf_index(CURRENT_SYMTAB);
//  cur_section = NULL;

  static UINT last_global_index = 1;
  bool has_sections = false;
  for (i = last_global_index; i < ST_Table_Size(GLOBAL_SYMTAB); ++i) {
	ST* sym = &St_Table(GLOBAL_SYMTAB,i);
	if (ST_class(sym) == CLASS_BLOCK && STB_section(sym)) {
	  if (!has_sections) {
	    has_sections=true;
	  }
	  Init_Section(sym);
	}
	// emit commons here so order is preserved for datapools
    	if (ST_sclass(sym) == SCLASS_COMMON) {
		if (ST_is_not_used (sym)) continue;
		if (ST_assigned_to_dedicated_preg(sym)) continue;
		if (ST_elf_index(sym)==0) { //EMT_Put_Elf_Symbol 
		  if (ST_class(sym) == CLASS_FUNC) {
		    fprintf (MiniR_File, "\t%s\t", AS_TYPE);
		    EMT_Write_Qualified_Name (MiniR_File, sym);
		    fprintf (MiniR_File, ", %s\n", AS_TYPE_FUNC);
		  } else if (ST_class(sym) == CLASS_VAR) {
		    Print_Common (MiniR_File, sym);
		  } 
		  Set_ST_elf_index(sym, 1);
		}
    	}
  }
  last_global_index = ST_Table_Size(GLOBAL_SYMTAB);

  // emit global bss first so .org is correct
  Process_Bss_Data (GLOBAL_SYMTAB);

  /* Initialize any sections that might have been created by the backend. */
  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
	base = Base_Symbol(sym);
	if (ST_class(base) == CLASS_BLOCK && STB_section(base)) {
		Init_Section(base);
	}
  }

  Create_Cold_Text_Section();

  //  Setup_Text_Section_For_PU (pu);

  Initial_Pu_PC = PC;
  Set_ST_ofst(pu, PC);

  Print_Label(NULL, &functions_MINIR, pu, 0);
  pfx = "    ";
#  ifdef AS_USED
  functions_MINIR << pfx << "used: " << (ST_is_used(pu) ? "yes\n" : "no\n"); 
#endif
  functions_MINIR << pfx << "entries: [";
  sep = "";
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    if (BB_entry(bb)) {
      functions_MINIR << sep << BB_Name(bb);
      sep = ", ";
    }
  }
  functions_MINIR << "]\n";
  functions_MINIR << pfx << "exits: [";
  sep = "";
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    if (BB_exit(bb)) {
      functions_MINIR << sep << BB_Name(bb);
      sep = ", ";
    }
  }
  functions_MINIR << "]\n";
  if (cur_section != PU_base) {
    /* reset to text section */
    fprintf (MiniR_File, "%s\n\t%s %s\n", pfx, AS_SECTION, ST_name(PU_base));
    cur_section = PU_base;
  }
  FOREACH_SYMBOL (CURRENT_SYMTAB, sym, i) {
    if (ST_is_not_used(sym)) continue;
    if (ST_sclass(sym) == SCLASS_COMMON) {
      EMT_Put_Elf_Symbol (sym);
    }
    /* put all extern symbols into the elf symbol table. */
    if ((PU_src_lang(Get_Current_PU()) == PU_F77_LANG 
	|| PU_src_lang(Get_Current_PU()) == PU_F90_LANG) &&
	ST_sclass(sym) == SCLASS_EXTERN && 
	! ST_is_export_local(sym)) 
    { 
      fprintf (MiniR_File, "%s%s\t %s\n", pfx, AS_GLOBAL, ST_name(sym)); 
    }

    if (ST_class(sym) == CLASS_VAR && ST_sclass(sym) == SCLASS_AUTO) {
      if (Has_Base_Block(sym)) {
	INT64 ofst;
	Base_Symbol_And_Offset(sym, &base, &ofst);
	fprintf ( MiniR_File, "\t%s %s = %" SCNd64 "\n",
		  ASM_CMNT, ST_name(sym), ofst);
      }
    }
  }
 
  functions_MINIR << pfx << "bbs:\n";

  bb_pfx = "        ";
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    Setup_Text_Section_For_BB(bb);
    // Assemble_BB
    functions_MINIR << pfx << "  - ";
    CG_Dump_Minir_BB(bb, bb_pfx, &functions_MINIR);
  }
  Setup_Text_Section_For_BB(REGION_First_BB);

  Process_Initos_And_Literals ( CURRENT_SYMTAB );
  Process_Bss_Data (CURRENT_SYMTAB);
//  MINIR_Dump_Bss_Data( CURRENT_SYMTAB );
}



static INT format_operand(
  char *buf, 
  INT n,
  BOOL qp,
  const ISA_OPERAND_VALTYP *vtype)
{
  INT len;

  if (ISA_OPERAND_VALTYP_Is_Register(vtype)) {
    const char *reg;
    ISA_REGISTER_SUBCLASS sc = ISA_OPERAND_VALTYP_Register_Subclass(vtype);
    ISA_REGISTER_CLASS rc = ISA_OPERAND_VALTYP_Register_Class(vtype);
    const ISA_REGISTER_CLASS_INFO *rcinfo = ISA_REGISTER_CLASS_Info(rc);
    const char * const fmt = qp ? ISA_PRINT_PREDICATE : "%s";
    INT regnum = 2 + n;
    if (sc == ISA_REGISTER_SUBCLASS_UNDEFINED) {
      INT first_reg = ISA_REGISTER_CLASS_INFO_First_Reg(rcinfo);
      INT last_reg = ISA_REGISTER_CLASS_INFO_Last_Reg(rcinfo);
      INT count = last_reg - first_reg + 1;
      regnum = ((regnum - first_reg) % count) + first_reg;
      reg = ISA_REGISTER_CLASS_INFO_Reg_Name(rcinfo, regnum);
    } else {
      const ISA_REGISTER_SUBCLASS_INFO *scinfo = ISA_REGISTER_SUBCLASS_Info(sc);
      INT count = ISA_REGISTER_SUBCLASS_INFO_Count(scinfo);
      regnum = ISA_REGISTER_SUBCLASS_INFO_Member(scinfo, regnum % count);
      reg = ISA_REGISTER_SUBCLASS_INFO_Reg_Name(scinfo, regnum % count);
      if (reg == NULL) reg = ISA_REGISTER_CLASS_INFO_Reg_Name(rcinfo, regnum);
    }
    len = sprintf(buf, fmt, reg) + 1;
  } else if (ISA_OPERAND_VALTYP_Is_Literal(vtype)) {
    INT64 imm = n + 1;
    ISA_LIT_CLASS lc = ISA_OPERAND_VALTYP_Literal_Class(vtype);
    if (!ISA_LC_Value_In_Class(imm, lc)) imm = ISA_LC_Min(lc);
    len = sprintf(buf, "%" SCNd64 "", imm) + 1;
  } else if (ISA_OPERAND_VALTYP_Is_Enum(vtype)) {
    ISA_ENUM_CLASS ec = ISA_OPERAND_VALTYP_Enum_Class(vtype);
    len = sprintf(buf, "%d", ISA_ECV_Intval(ISA_EC_First_Value(ec))) + 1;
  } else {
    FmtAssert(FALSE, ("unhandled vtype in format_operand"));
  }

  return len;
}

static void Enumerate_Insts(void)
{
  TOP top;
  char lab[25];
  INT labnum = 0;
  INT ninsts = 0;

  fprintf(TFile, "%s Instruction enumeration for %s ISA\n\n",
		 ASM_CMNT_LINE, ISA_SUBSET_Name(ISA_SUBSET_Value));

  for (top = (TOP)0; top < TOP_UNDEFINED; top = (TOP)(top + 1)) {

    if (   TOP_is_simulated(top) 
	|| TOP_is_dummy(top)
	|| !ISA_SUBSET_Member(ISA_SUBSET_Value, top)) continue;

    char buf[1024];
    const char *result[ISA_OPERAND_max_results];
    const char *opnd[ISA_OPERAND_max_operands];
    ISA_ENUM_CLASS_VALUE ev[ISA_OPERAND_max_operands];
    ISA_ENUM_CLASS ec[ISA_OPERAND_max_operands];
    INT ei[ISA_OPERAND_max_operands];
    INT i;
    const ISA_OPERAND_INFO *oinfo = ISA_OPERAND_Info(top);
    INT nopnds = ISA_OPERAND_INFO_Operands(oinfo);
    INT nresults = ISA_OPERAND_INFO_Results(oinfo);
    INT nenums = 0;
    INT cursor = 0;

    for (i = 0; i < nopnds; ++i) {
      const ISA_OPERAND_VALTYP *vtype = ISA_OPERAND_INFO_Operand(oinfo, i);
      if (   ISA_OPERAND_VALTYP_Is_Enum(vtype)
	  && ISA_PRINT_Operand_Is_Part_Of_Name(top, i))
      {
	ec[nenums] = ISA_OPERAND_VALTYP_Enum_Class(vtype);
	ev[nenums] = ISA_EC_First_Value(ec[nenums]);
	ei[nenums] = i;
	++nenums;
      } else {
        BOOL qp = (i == OP_PREDICATE_OPND && TOP_is_predicated(top));
        opnd[i] = buf + cursor;
        cursor += format_operand(buf + cursor, i, qp, vtype);
      }
    }

    for (i = 0; i < nresults; ++i) {
      const ISA_OPERAND_VALTYP *vtype = ISA_OPERAND_INFO_Result(oinfo, i);
      result[i] = buf + cursor;
      cursor += format_operand(buf + cursor, nopnds + i, FALSE, vtype);
    }

    do {
      INT orig_cursor = cursor;

      if ((ninsts % (10 * ISA_MAX_SLOTS)) == 0) {
	sprintf(lab, "lab%d", labnum);
	fprintf(TFile, "%s:\n", lab);
	++labnum;
      }
      ++ninsts;

      for (i = 0; i < nenums; ++i) {
	opnd[ei[i]] = buf + cursor;
	cursor += sprintf(buf + cursor, "%s", ISA_ECV_Name(ev[i])) + 1;
      }

      for (i = 0; i < nopnds; ++i) {
	const ISA_OPERAND_VALTYP *vtype = ISA_OPERAND_INFO_Operand(oinfo, i);
	if (ISA_OPERAND_VALTYP_Is_PCRel(vtype)) opnd[i] = lab;
      }

      fputc ('\t', TFile);
      TI_ASM_Print_Inst(top, result, opnd, TFile);
      fputc ('\n', TFile);

      cursor = orig_cursor;

      for (i = 0; i < nenums; ++i) {
	if (ev[i] == ISA_EC_Last_Value(ec[i])) {
	  ev[i] = ISA_EC_First_Value(ec[i]);
	} else {
	  ev[i] = (ISA_ENUM_CLASS_VALUE)(ev[i] + 1);
	  break;
	}
      }

    } while (i < nenums);
  }
}

static char ism_name[40];

/* get ism name from __Release_ID */
static void
Get_Ism_Name (void)
{
	const char *s = strchr(__Release_ID, ':');
	const char *p;
	if (s == NULL) {
		ism_name[0] = '\0';
		return;
	}
	s++;	/* skip : */
	s = strchr(s, ':');
	if (s == NULL) {
		ism_name[0] = '\0';
		return;
	}
	s++;	/* skip : */
	p = strchr(s, ' ');
	strncpy (ism_name, s, p-s);
	ism_name[p-s] = '\0';
}

void
EMT_Begin_File (
  char *process_name,	/* Back end process name */
  char *options)	/* Option string used for compilation */
{
  char *buff;

  /* Initialize: */
  Trace_Elf	= Get_Trace ( TP_EMIT, 2 );
  Trace_Init	= Get_Trace ( TP_EMIT, 4 );

  // Enumerate all instructions:
  if (Get_Trace (TP_CG, 0x100)) Enumerate_Insts();

  text_PC = 0;
  cold_PC = 0;
  Get_Ism_Name();
  if (FILE_INFO_gp_group (File_info)) {
        Has_GP_Groups = TRUE;
  }

  if (Object_Code || CG_emit_asm_dwarf) {
	generate_dwarf = TRUE;
	generate_elf_symbols = TRUE;
  }

  if ( generate_elf_symbols ) {
    Obj_File = fdopen (Em_Begin_File (
			    	Obj_File_Name, 
			    	FALSE, 
			    	!Use_32_Bit_Pointers, 
				FALSE /* old_abi */,
				(INT) Target_ISA,
				(!Target_Is_Little_Endian),
				Gen_PIC_Shared, 
				Gen_PIC_Call_Shared,
                		!Guaranteed_Small_GOT, 
				Has_GP_Groups,
			    	Trace_Elf),
		       "r+");
    if (Obj_File == NULL) return;

    // If we are meant to be generating only a .s file but that .s
    // file needs to have dwarf information represented in it, we
    // currently use the em_elf and libelf routines to maintain
    // section indices, symbol indices, etc. for libdwarf. In such a
    // situation, we generate the object file, but we unlink it here
    // so it never shows up after the compilation is done.
    if ( ! Object_Code) {
      unlink(Obj_File_Name);
    }

    buff = (char *) alloca (strlen("be") + sizeof(INCLUDE_STAMP) + 
			    strlen(ism_name) + strlen(Obj_File_Name) + 4);
    if (*ism_name != '\0')
	sprintf(buff, "be::%s-%s:%s", INCLUDE_STAMP, ism_name, Obj_File_Name);
    else
	sprintf(buff, "be::%s:%s", INCLUDE_STAMP, Obj_File_Name);
    Em_Add_Comment (buff);
    if ( ! DEBUG_Optimize_Space) {
    	buff = (char *) alloca (strlen("be-options") + strlen(options) + 4);
    	sprintf(buff, "be-options:::%s", options);
    	Em_Add_Comment (buff);
    }

    if ( EMIT_interface_section ) Interface_Scn_Begin_File();
  }

  Init_ST_elf_index(GLOBAL_SYMTAB);
  if (generate_dwarf) {
    Cg_Dwarf_Begin (!Use_32_Bit_Pointers);
  }
  Cg_Dwarf_Gen_Asm_File_Table ();
#ifdef KEY
  Cg_Dwarf_Gen_Macinfo ();
#endif

  if (Assembly) {
    ASM_DIR_NOREORDER();
    ASM_DIR_NOAT();
#ifdef TARG_MIPS
    if (! CG_emit_non_gas_syntax)
      fputs ( "\t.set\tnomacro\n", Asm_File );
#endif
    fprintf ( Asm_File, "\t%s  %s::%s\n", ASM_CMNT, process_name, 
			    INCLUDE_STAMP );
    if (*ism_name != '\0')
    	fprintf ( Asm_File, "\t%s%s\t%s\n", ASM_CMNT, "ism", ism_name);
    List_Compile_Options ( Asm_File, "\t"ASM_CMNT, FALSE, TRUE, TRUE );

#ifdef KEY
    if (! CG_emit_non_gas_syntax)
      Print_Directives_For_All_Files();
#endif

    /* TODO: do we need to do .interface stuff for asm files? */
  }
}

BOOL pad_data_section = FALSE;


static void
Em_Options_Scn(void)
{
    Elf64_Word info_mask = 0;
    Elf64_Half size = 0;
    Elf_Options_Hw hwand_buf = {0,0};
    UINT8 gp_group;
    gp_group = FILE_INFO_gp_group (File_info);

    if (gp_group != 0) {
	Elf64_Xword identifier = Ipa_Ident_Number;
	Em_Add_New_Option (ODK_IDENT, 0, gp_group, 
		(char*) &identifier, sizeof(identifier));
    }

    if ( Use_Page_Zero) 
      info_mask |= OEX_PAGE0;
    if (Force_SMM)
	info_mask |= OEX_SMM;
    if (Force_FP_Precise_Mode)
      info_mask |= OEX_PRECISEFP;
    if (Force_Memory_Dismiss)
      info_mask |= OEX_DISMISS;

    info_mask |= (FP_Exception_Enable_Max << 8) | FP_Exception_Enable_Min;
    
    Em_Add_New_Option (ODK_EXCEPTIONS, SHN_UNDEF, info_mask, NULL, 0);
    
    if (Use_Prefetch)
	Em_Add_New_Option (ODK_HWPATCH, 0, OHW_R8KPFETCH, NULL, 0);

    /* always generate empty HWAND and HWOR descriptors */
    Em_Add_New_Option (ODK_HWAND, 0, 0, (char*) &hwand_buf, sizeof(hwand_buf));

    info_mask = 0;
    if (Get_Trace ( TP_EMIT,0x100 )) {
      // fprintf ( TFile, "Misaligned_Cnt= %d\n", Misaligned_Cnt);
      DevWarn ("Misaligned_Cnt= %d\n", Misaligned_Cnt);
    }
    if (DEBUG_Alignment_Fixade && Misaligned_Cnt>0)
      info_mask = OHWO0_FIXADE;
    Em_Add_New_Option (ODK_HWOR, 0, info_mask, (char*) &hwand_buf, sizeof(hwand_buf));

    /* I don't have a case for PREFIX pad yet */
    /* We don't handle per symbol pad yet */
    if (pad_data_section) {
      /* pad all data sections */
      Elf64_Word pad_size[2];
     
      size = PAD_SIZE_LIMIT;
      pad_size[0] = (short) size;
      pad_size[1] = 0;
      Em_Add_New_Option(ODK_PAD, 0, 0, &pad_size[0], sizeof(Elf64_Xword));
    }
    else {
#if TODO_MONGOOSE
      INT32 i;
      for (i = 1; i <= Sec_last; i++) {
	if (SEC_pad_size(i) || pad_data_section) {
	  if (SEC_used(i) && SEC_static(i) && (!SEC_exec(i))) {
	    Elf64_Word pad_size[2];
	    size_t scn = Em_Get_Section_Index (em_scn[i].scninfo);
	    size = pad_data_section ? PAD_SIZE_LIMIT : SEC_pad_size(i);
	    pad_size[0] = (short)size;
	    pad_size[1] = 0;
	    Em_Add_New_Option(ODK_PAD, scn, 0, &pad_size[0], sizeof(Elf64_Xword));
	  }
	}
      }
#endif
    }
}


#ifdef KEY
// stores the command line arguments passed to 'be'
extern char ** be_command_line_args;
extern INT be_command_line_argc;

#ifdef TARG_X8664
// similar to Targ_Name, but return all lower-case names
static const char *
Target_Name (TARGET_PROCESSOR t)
{
  switch (t)
  {
    case TARGET_opteron: return "opteron";
    case TARGET_barcelona: return "barcelona";
    case TARGET_athlon64: return "athlon64";
    case TARGET_athlon: return "athlon";
    case TARGET_em64t: return "em64t";
    case TARGET_core: return "core";
    case TARGET_wolfdale: return "wolfdale";
    case TARGET_pentium4: return "pentium4";
    case TARGET_xeon: return "xeon";
    case TARGET_anyx86: return "anyx86";
    default: Fail_FmtAssertion ("Add support for %s", Targ_Name (t));
  }
  return NULL;
}
#endif // TARG_X8664

// Emit compilation options and ABI/processor details into assembly file
static void
Emit_Options (void)
{
  // don't emit anything for symtab.s
  if (!Emit_Global_Data)
  {
#if defined(VENDOR_DEFINED)
    // TODO : Move this to being a configure option
    fputs ("\t.ident\t\"#Compiler Version " PSC_FULL_VERSION " :", Asm_File);
#else
    fputs ("\t.ident\t\"#PathScale Compiler Version " PSC_FULL_VERSION " :", Asm_File);
#endif
    fprintf (Asm_File, " %s compiled with : ", Src_File_Name);
    // 0th is 'be', (be_command_line_argc-1)th is filename
    for (INT cmds=1; cmds < be_command_line_argc-1; ++cmds)
    {
      char * option = be_command_line_args[cmds];
      // exclude lots of options
      if (!strncmp ("-PHASE", option, strlen ("-PHASE")) ||
          !strncmp ("-INTERNAL", option, strlen ("-INTERNAL")) ||
          !strncmp ("-G8", option, strlen ("-G8")) ||
          !strncmp ("-LANG", option, strlen ("-LANG")) ||
          !strncmp ("-LIST", option, strlen ("-LIST")) ||
          !strncmp ("-TARG", option, strlen ("-TARG")))
        continue;

      if (!strcmp ("-show", option) ||
          !strcmp ("-v", option) ||
          !strcmp ("-c", option) ||
          !strcmp ("-s", option) ||
          !strncmp ("-fB,", option, strlen ("-fB,")) ||
          !strncmp ("-fs,", option, strlen ("-fs,")) ||
	  // feedback option
	  !strncmp ("-ff,", option, strlen ("-ff,")) ||
	  // all the trace options
	  !strncmp ("-ti", option, strlen ("-ti")) ||
	  !strncmp ("-tr", option, strlen ("-tr")) ||
	  !strncmp ("-ts", option, strlen ("-ts")) ||
	  !strncmp ("-tt", option, strlen ("-tt")) ||
	  !strncmp ("-tf", option, strlen ("-tf")))
        continue;

      // check if IPA, then exclude some internal options
      if (Read_Global_Data &&
          (!strcmp ("-OPT:procedure_reorder=on", option) ||
          !strcmp ("-CG:enable_feedback=off", option) ||
          !strncmp ("-TENV:ipa_ident", option,
                    strlen ("-TENV:ipa_ident")) ||
          !strncmp ("-TENV:read_global_data", option,
                    strlen ("-TENV:read_global_data")) ||
          !strncmp ("-TENV:CPIC", option,
                    strlen ("-TENV:CPIC"))))
        continue;

      fprintf (Asm_File, "%s ", option);
    }
    // ipa ?
    if (Read_Global_Data) fputs ("-ipa ", Asm_File);
#ifdef TARG_X8664
    fprintf (Asm_File, "-march=%s ", Target_Name (Target));

    if (Is_Target_SSE2()) fputs ("-msse2 ", Asm_File);
    else fputs ("-mno-sse2 ", Asm_File);

    if (Is_Target_SSE3()) fputs ("-msse3 ", Asm_File);
    else fputs ("-mno-sse3 ", Asm_File);

    if (Is_Target_3DNow()) fputs ("-m3dnow ", Asm_File);
    else fputs ("-mno-3dnow ", Asm_File);
   
    if (Is_Target_SSE4a()) fputs ("-msse4a ", Asm_File);
    else fputs ("-mno-sse4a ", Asm_File);

    if (Is_Target_64bit()) fputs ("-m64", Asm_File);
    else fputs ("-m32", Asm_File);
#endif // TARG_X8664

    fputs ("\"\n", Asm_File);
  }
}
#endif // KEY

/* ====================================================================
 *    EMT_End_File ()
 * ====================================================================
 */
void
EMT_End_File( void )
{
  INT16 i;
  ST *sym;

#ifdef TARG_X8664
  EETARG_Emit_IP_Calc_Func();
#endif
  cur_section = NULL;
  Init_ST_elf_index(GLOBAL_SYMTAB);

  /* make sure all global symbols are initialized */
  FOREACH_SYMBOL (GLOBAL_SYMTAB, sym, i) {
	if (ST_class(sym) == CLASS_BLOCK && STB_section(sym)) {
		if (Emit_Global_Data && SEC_is_merge(STB_section_idx(sym)) )
			continue;	// merge sections go in each .o
		Init_Section(sym);
	}
        // emit commons here so order is preserved for datapools
        if (ST_sclass(sym) == SCLASS_COMMON) {
                if (ST_is_not_used (sym)) continue;
#ifdef KEY // bug 14507: sym will be replaced by dedicated reg, so don't emit sym
                if (ST_assigned_to_dedicated_preg(sym)) continue;
#endif
                EMT_Put_Elf_Symbol (sym);
        }
#ifdef KEY
	/* Emit .weak directive for the used weak symbol. (bug#3202)
	 */
	if( ST_is_weak_symbol(sym) &&
	    !ST_is_not_used(sym) ){
	  EMT_Put_Elf_Symbol(sym);
	}
#endif
  }

  if (Emit_Global_Data) {
	char *newname;
	// create dummy symbol to represent the section
	FOREACH_SYMBOL (GLOBAL_SYMTAB, sym, i) {
		if (ST_class(sym) != CLASS_BLOCK) continue;
		if (!STB_section(sym)) continue;
		// mergeable sections will be emitted into each .o
		if (SEC_is_merge(STB_section_idx(sym))) continue;
#ifdef TARG_ST
      // [CL] generate unique names
      UINT len = strlen ("_symbol_") + strlen(Ipa_Label_Suffix) + 1;
      char *new_str = (char *) alloca (len);
      strcpy (new_str, "_symbol_");
      strcat (new_str, Ipa_Label_Suffix);
      newname = Index_To_Str(Save_Str2(ST_name(sym), new_str));
#else
		newname = Index_To_Str(Save_Str2(ST_name(sym), "_symbol"));
#endif
#ifndef TARG_ST
		if (Object_Code) {
	  		(void) Em_Add_New_Symbol (
				newname,
				0 /* offset */, 
				0 /* size */,
				STB_GLOBAL, STT_OBJECT, STO_INTERNAL,
				Em_Get_Section_Index (em_scn[STB_scninfo_idx(sym)].scninfo));
		}
#endif

#ifdef TARG_ST
                // [CG]: Ensure section is initialized
                Init_Section(sym); 
#endif
		if (Assembly || MiniR_Code) {
		  char * sym_print_name = 
#if defined(BUILD_OS_DARWIN)
		    underscorify(newname);
#else /* defined(BUILD_OS_DARWIN) */
		    newname;
#endif /* defined(BUILD_OS_DARWIN) */

		  Change_Section_Origin (sym, 0);
#ifdef KEY
		  // TODO: Handle CG_file_scope_asm_seen
		  fprintf (MiniR_Code? MiniR_File :  Asm_File, "\t%s\t0\n", AS_ALIGN );
#endif
		  fprintf (MiniR_Code? MiniR_File : Asm_File, "\t%s\t%s\n", AS_GLOBAL, sym_print_name);

		  fprintf (MiniR_Code? MiniR_File : Asm_File, "\t%s\t%s\n", AS_INTERNAL, sym_print_name);
		  ASM_DIR_STOINTERNAL(newname);
		  fprintf (MiniR_Code? MiniR_File : Asm_File, "%s:\n", newname);
		}
	}
  }

  /* If there weren't any PUs, we may have data initialization
   * associated with file scope data here:
   */
  Process_Bss_Data (GLOBAL_SYMTAB);
#if defined(BUILD_OS_DARWIN)
  darwin_bss::finish_bss_symbols(Asm_File);
#endif /* defined(BUILD_OS_DARWIN) */
  Process_Initos_And_Literals (GLOBAL_SYMTAB);
  // We need two calls to  Process_Initos_And_Literals (GLOBAL_SYMTAB)
  // because while writing out INITOs new literals may be allocated
  Process_Initos_And_Literals (GLOBAL_SYMTAB);
  Process_Distr_Array ();

  FOREACH_SYMBOL (GLOBAL_SYMTAB, sym, i) {
    if (ST_class(sym) == CLASS_NAME) {
	if (ST_emit_symbol(sym)) {
		/* may be notype symbol */
		EMT_Put_Elf_Symbol(sym);
	}
#ifndef TARG_ST
    	else if (ST_sclass(sym) == SCLASS_COMMENT && Object_Code 
		&& ! Read_Global_Data	// just put once in symtab.o
		&& ! DEBUG_Optimize_Space)
	{
		char *buf = (char *) alloca (strlen("ident::: ") + strlen(ST_name(sym)));
    		sprintf(buf, "ident:::%s", ST_name(sym));
    		Em_Add_Comment (buf);
	}
#endif
    }

    if (ST_class(sym) == CLASS_VAR &&
        ST_is_fill_align(sym) &&
        !Has_Base_Block(sym)) {
      /* fill/align symbols are supposed to be allocated in be
       * but are not done if there were no PUs in the file.
       * Report that error here.
       */

      ErrMsg (EC_Is_Bad_Pragma_Abort, 
              "Fill/Align symbol",
              ST_name(sym),
              "requires the file to contain at least one function");
    }
	
    if (Has_Strong_Symbol(sym)) {
      ST *strongsym = ST_strong(sym);
      unsigned char symtype;

#ifndef KEY // the strong symbol can be another weak one linked to a strong one
      if ( ! Has_Base_Block(strongsym))
	continue;	/* strong not allocated, so ignore weak */
#endif

      if (ST_class(sym) == CLASS_FUNC) {
	symtype = STT_FUNC;
      }
      else {
	symtype = STT_OBJECT;
      }

      if (Assembly) {
	CGEMIT_Weak_Alias (sym, strongsym);
	Print_Dynsym (Asm_File, sym);
      }
#ifndef TARG_ST
      if (Object_Code) {
	Em_Add_New_Weak_Symbol (
	  ST_name(sym), 
	  symtype,
	  st_other_for_sym (sym),
	  EMT_Put_Elf_Symbol(strongsym));
      }
#endif
    }
    else if (Has_Base_Block(sym) && ST_class(ST_base(sym)) != CLASS_BLOCK
	&& ST_emit_symbol(sym))
    {
	// alias
	if (Assembly) {
	    if ( ! ST_is_export_local(sym)) {
	    	fprintf (Asm_File, "\t%s\t %s\n", AS_GLOBAL,
#if defined(BUILD_OS_DARWIN)
		  underscorify(ST_name(sym))
#else /* defined(BUILD_OS_DARWIN) */
		  ST_name(sym)
#endif /* defined(BUILD_OS_DARWIN) */
		  );
#ifdef TARG_ST
	  EMT_Visibility (Asm_File, NULL, ST_export(sym), sym);
#endif
	    }
	    CGEMIT_Alias (sym, ST_base(sym));
	}
    }
    else if (ST_class(sym) == CLASS_FUNC && ST_emit_symbol(sym)
	// possible to have local not-used emit_symbols,
	// cause mp does that to mark them as unused,
	// and don't want to emit those.
	&& ST_sclass(sym) == SCLASS_EXTERN) 
    {
	// some unreferenced fortran externs need to be emitted
	EMT_Put_Elf_Symbol(sym);
	if (Assembly) {
		fprintf (Asm_File, "\t%s\t %s\n", AS_GLOBAL,
#if defined(BUILD_OS_DARWIN)
		  underscorify(ST_name(sym))
#else /* defined(BUILD_OS_DARWIN) */
		  ST_name(sym)
#endif /* defined(BUILD_OS_DARWIN) */
		  );
#ifdef TARG_ST
	EMT_Visibility(Asm_File, NULL, ST_export(sym), sym);
#endif
	}
    }
  }

#ifdef TEMPORARY_STABS_FOR_GDB
  // This is an ugly hack to enable basic debugging for IA-32 target
  if (PU_base == NULL && Assembly && Debug_Level > 0) {
    fputs (".Ltext0:\n", Asm_File);
  }
#endif

#ifndef TARG_ST // [CL] previous text_region was closed it the end of EMT_Emit_PU()
  if (generate_elf_symbols && PU_section != NULL) {
    end_previous_text_region(PU_section, Em_Get_Section_Offset(PU_section));
  }
#endif

#ifndef TARG_ST
  if (Object_Code) {
    Em_Options_Scn();
  }
#endif
  if (generate_dwarf) {
    // must write out dwarf unwind info before text section is ended
    Cg_Dwarf_Finish (PU_section);
  }

  /* Write out the initialized data to the object file. */
  for (i = 1; i <= last_scn; i++) {
    sym = em_scn[i].sym;
#ifndef TARG_ST
    if (Object_Code) {

#ifdef PV_205345
	/* Data section alignment is initially set to the maximum
         * alignment required by the objects allocated to the section.
         * Whenever Find_Alignment is called for an object in a section
         * with smaller alignment than it's quantum of interest, it
         * updates the section alignment to guarantee that the
	 * determined alignment is valid.  This override can be enabled
	 * with -DPV_205345 to force alignment to at least 8 bytes.
         */
	if (STB_align(sym) < 8) Set_STB_align(sym, 8);
#endif /* PV_205345 */

	if (STB_nobits(sym)) {
	  /* For the .bss section, the size field should be set explicitly. */
	  Em_Add_Bytes_To_Scn (em_scn[i].scninfo, NULL,  
		STB_size(sym), STB_align(sym));
	}
	else {
	  Em_Change_Section_Alignment (em_scn[i].scninfo, STB_align(sym));
	}
        Em_End_Section (em_scn[i].scninfo);
      }
#endif

      if (Assembly) {
	UINT32 tmp, power;
	power = 0;
	for (tmp = STB_align(sym); tmp > 1; tmp >>= 1) power++;
#if defined(BUILD_OS_DARWIN)
	emit_section_directive(sym);
#else /* defined(BUILD_OS_DARWIN) */
	fprintf (Asm_File, "\t%s %s\n", AS_SECTION, ST_name(sym));
#endif /* defined(BUILD_OS_DARWIN) */
#ifdef KEY
#if defined(TARG_X8664) && ! defined(BUILD_OS_DARWIN)
	fprintf (Asm_File, "\t%s\t%d\n", AS_ALIGN, 1 << power );
#else
	fprintf (Asm_File, "\t%s\t%d\n", AS_ALIGN, power);
#endif
#else
	ASM_DIR_ALIGN(power, sym);
#endif
      }
  }

  INT dwarf_section_count = 0;

  if (generate_dwarf) {
    dwarf_section_count = Em_Dwarf_Prepare_Output ();
  }

  if (Assembly) {
#ifdef TARG_ST
    if (List_Notes)
      fprintf(Asm_File, "\t%s %s %d\n", ASM_CMNT_LINE, AS_GPVALUE, GP_DISP);
#else
    ASM_DIR_GPVALUE();
#endif
#ifdef TARG_MIPS
    if (! CG_emit_non_gas_syntax) {
#endif
#ifdef TARG_ST
  // (cbr) we enter here either for debug dwarf emission or exceptions frame dwarf unwinding 
  if (CG_emit_asm_dwarf || CXX_Exceptions_On) {
#else
  if (CG_emit_asm_dwarf) {
#endif
      Cg_Dwarf_Write_Assembly_From_Symbolic_Relocs(Asm_File,
						   dwarf_section_count,
						   !Use_32_Bit_Pointers);
    }
#ifdef KEY // bug 5561: mark stack as non-executable
#if defined(BUILD_OS_DARWIN)
    fprintf ( Asm_File, "\t%s\t.note.GNU-stack,\"\"\n", AS_SECTION);
#else /* defined(BUILD_OS_DARWIN) */
    fprintf ( Asm_File, "\t%s\t.note.GNU-stack,\"", AS_SECTION);
    if (PU_has_trampoline)
      fprintf ( Asm_File, "x");
    fprintf ( Asm_File, "\",@progbits\n");
#endif /* defined(BUILD_OS_DARWIN) */
    Emit_Options ();
#endif
#ifdef TARG_MIPS
    }
#endif
  }
#ifndef TARG_ST
  if (Object_Code && !CG_emit_asm_dwarf) {
    /* TODO: compute the parameters more accurately. For now we assume 
     * that all integer and FP registers are used. If we change the GP
     * value to be non-zero, make sure we adjust the addends for the 
     * GP_REL cases.
     */
    Em_Write_Reginfo (GP_DISP, 0xffffffff, 0xffffffff, Pure_ABI); 

    Em_Dwarf_Write_Scns (Cg_Dwarf_Translate_To_Elf);

    /* finalize .interfaces section (must be before Em_End_File) */
    if ( EMIT_interface_section )
      Interface_Scn_End_File();

    Em_End_File ();
    Em_Dwarf_End ();
    Em_Cleanup_Unwind ();
  }
#endif

  if (Emit_Global_Data) {
	// prepare block data to be written to .G file.
	// need to remove section info so can be reset when read.
	FOREACH_SYMBOL (GLOBAL_SYMTAB, sym, i) {
 		if (ST_class(sym) == CLASS_CONST
			&& SEC_is_merge(STB_section_idx(ST_base(sym))) )
		{
			// reallocate in each file
			Set_ST_base(sym,sym);	
			Set_ST_ofst(sym,0);	
		}
		else if (ST_class(sym) == CLASS_VAR 
			|| ST_class(sym) == CLASS_CONST
			|| ST_class(sym) == CLASS_FUNC)
		{
#ifdef TARG_ST
		  if (ST_is_weak_symbol(sym)
		      && (ST_sclass(sym) == SCLASS_DGLOBAL
			  || ST_sclass(sym) == SCLASS_UGLOBAL)) {
		    /* Weak definition becomes weak reference in the .G file */
		    Set_ST_sclass(sym, SCLASS_EXTERN);
		    Set_ST_base(sym, sym);
		    Set_ST_ofst(sym, 0);
		  }
#endif
		  if (ST_sclass (sym) != SCLASS_COMMON && !ST_is_weak_symbol(sym) ) {
		    Set_ST_sclass(sym, SCLASS_EXTERN);
		  }
		}
		if (ST_class(sym) != CLASS_BLOCK) continue;
		Set_STB_scninfo_idx(sym,0);
		Set_STB_compiler_layout(sym);
		if (STB_section(sym)) {
		  Reset_STB_section(sym);
		  Reset_STB_root_base(sym);
		  Set_STB_section_idx(sym,0);
#ifdef TARG_ST
		  // [CL] generate unique names
		  UINT len = strlen ("_symbol_") + strlen(Ipa_Label_Suffix) + 1;
		  char *new_str = (char *) alloca (len);
		  strcpy (new_str, "_symbol_");
		  strcat (new_str, Ipa_Label_Suffix);
		  Set_ST_name(sym, Save_Str2(ST_name(sym), new_str));
#else
		  Set_ST_name(sym, Save_Str2(ST_name(sym), "_symbol"));
#endif
		  Set_ST_sclass(sym, SCLASS_EXTERN);
		  Set_ST_export(sym, EXPORT_INTERNAL);
		}
		else {
		  // in case non-section block
		  Set_ST_sclass(sym, SCLASS_EXTERN);
		}
	}
  }

  // Finish off the TCON to symbolic names table:
  //  Fini_Tcon_Info ();

#ifdef TARG_ST
  // Target dependent end file.
  if (Assembly) CGEMIT_End_File_In_Asm ();
#endif    

  if (MiniR_Code) {
    fclose(MiniR_File);
    MiniR_Ofstream.open(MiniR_File_Name, std::ios::app);
    if (sections_MINIR.tellp()>0) {
      MiniR_Ofstream << "sections:\n"; 
      MiniR_Ofstream << sections_MINIR.str();
    }
    if (functions_MINIR.tellp()>0) {
      MiniR_Ofstream << "functions:\n"; 
      MiniR_Ofstream << functions_MINIR.str();
    }
    if (objects_MINIR.tellp()>0) {
      MiniR_Ofstream << "objects:\n";
      MiniR_Ofstream << objects_MINIR.str();
    }
    MiniR_Ofstream.close();
  }

  return;
}


#if defined(BUILD_OS_DARWIN)
typedef struct {
  int is_debug_section;	/* True if this is a debug section */
  const char *name;	/* Section name */
  const char *asm_dir_name;	/* String to use in asm ".section" directive */
  } map_section_t;

/* If a section name doesn't need to be transformed for use in a ".section"
 * directive, then this table can omit an entry for it. */
static map_section_t map_section_table[] = {
  { 1, DEBUG_ABBREV_SECTNAME, "__DWARF,__debug_abbrev,regular,debug" },
  { 1, DEBUG_ARANGES_SECTNAME, "__DWARF,__debug_aranges,regular,debug" },
  { 1, DEBUG_FRAME_SECTNAME, "__DWARF,__debug_frame,regular,debug" },
  { 1, DEBUG_INFO_SECTNAME, "__DWARF,__debug_info,regular,debug" },
  { 1, DEBUG_LINE_SECTNAME, "__DWARF,__debug_line,regular,debug" },
  { 1, DEBUG_LOC_SECTNAME, "__DWARF,__debug_loc,regular,debug" }, /* Need? */
  { 1, DEBUG_MACINFO_SECTNAME, "__DWARF,__debug_macinfo,regular,debug" }, /* Need? */
  { 1, DEBUG_PUBNAMES_SECTNAME, "__DWARF,__debug_pubnames,regular,debug" },
  { 1, DEBUG_STR_SECTNAME, "__DWARF,__debug_str,regular,debug" },
  { 0, EH_FRAME_SECTNAME,
    "__TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support" },
  { 0, ".data", "__DATA,__data" },
  { 0, ".lit4", "__TEXT,__literal4" },
  { 0, ".lit8", "__TEXT,__literal8" },
  { 0, ".rodata","__TEXT,__const" },
  { 0, ".text", "__TEXT,__text" }
};

static int
cmp_section_name(const void *a_arg, const void *b_arg) {
  map_section_t *a = (map_section_t *) a_arg;
  map_section_t *b = (map_section_t *) b_arg;
  return strcmp(a->name, b->name);
}

static map_section_t *
map_search(const char *section_name) {
  static int first_time = 1;
  if (first_time) {
    /* must sort map_section_table so bsearch works */
    first_time = 0;
    qsort(map_section_table,
      (sizeof map_section_table) / (sizeof map_section_table[0]),
      sizeof map_section_table[0],
      cmp_section_name);
  }
  map_section_t key;
  key.name = section_name;
  return (map_section_t *) bsearch(&key,
    map_section_table,
    (sizeof map_section_table) / (sizeof *map_section_table),
    sizeof *map_section_table,
    cmp_section_name);
}

/* section_name		Symbol name representing object file section
 * return		string to use in asm ".section" directive to select
 *			that section */
const char *
map_section_name(const char *section_name) {
  map_section_t *value = map_search(section_name);
  return value ? value->asm_dir_name : section_name;
}

/* section_name		Symbol name representing object file section
 * return		1 if section is a "debug" section which should be
 *			emitted for -g, 0 otherwise
 */
int
is_debug_section(const char *section_name) {
  map_section_t *value = map_search(section_name);
  return value ? value->is_debug_section : 0;
}
#endif /* defined(BUILD_OS_DARWIN) */
#endif

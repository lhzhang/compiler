/*
 * Copyright (C) 2008-2009 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*
 *  Copyright (C) 2007, 2008, 2009 PathScale LLC.  All Rights Reserved.
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
 * ====================================================================
 *
 * Module: cgdriver.c
 * $Source: be/cg/SCCS/s.cgdriver.cxx $
 *
 * Description:
 *
 * Main driver -- command line processing and file name manipulation --
 * for the code generator.
 *
 * ====================================================================
 * ====================================================================
 */

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "elf_defines.h"
#include <sys/elf_whirl.h>	    /* for WHIRL_REVISION */
#include <ctype.h>
#include "defs.h"
#include "config.h"
#include "config_debug.h"
#include "config_list.h"
#include "config_targ_opt.h"
#include "config_opt.h"
#include "cg_flags.h"
#include "controls.h"
#include "flags.h"
#include "erglob.h"
#include "erlib.h"
#include "errors.h"
#include "erauxdesc.h"
#include "file_util.h"
#include "glob.h"
#include "timing.h"
#include "tracing.h"
#include "util.h"
#include "mempool.h"

#include "wn.h"			    /* for WN */
#include "opt_alias_interface.h"    /* for ALIAS_MANAGER stuff */
#include "dwarf_DST_mem.h"

#include "bb.h"			    /* for cgemit.h */
#include "cg.h"			    /* CG_Initialize(), etc. */
#include "cgemit.h"		    /* R_Assemble_File() */
#include "cg_swp_options.h"         /* for SWP_Options */
#include "gra.h"                    /* for GRA_optimize_placement... */
#include "ebo.h"		    /* for EBO options */
#include "cgprep.h"		    /* for CGPREP knobs */
#include "cgtarget.h"		    /* target-dependent stuff */
#include "cg_dep_graph.h"	    /* for CG_DEP knobs */
#include "cg_dep_graph_update.h"    /* more CG_DEP knobs */
#include "cio.h"                    /* for rw, cicse etc ...*/
#include "cg_loop.h"                /* for unrolling */
#include "cg_loop_recur.h"	    /* recurrence fixing */
#include "loop_invar_hoist.h"	    /* For loop invariant hoisting */
#include "gcm.h"		    /* for GCM options */
#include "cg_sched_est.h"	    /* for CG_SCHED_EST options */
#include "targ_proc_properties.h"
#ifndef TARG_ST
#include "cgdriver_arch.h"
#endif
#include "cgdriver.h"
#include "register.h"
#include "pqs_cg.h"
#ifdef KEY
#include "cg_gcov.h"
#include "flags.h"
#include "cgdwarf.h"
#endif
#ifdef LAO_ENABLED
#include "lao_stub.h"               /* for lao_init()/lao_fini() */
#endif

#ifdef TARG_ST
// [TB] gcov coverage utilities  
#include "gcov_profile.h"

// [GS-DFGforISE] for exportation to DFGs
#include "ExportFromBackEnd.h"
#endif
#ifdef TARG_ST
#include "cg_ssa.h"                 /* for SSA flags */
#include "cg_outssa.h"
#include "cg_select.h"              /* for SELECT flags */
#include "cg_ivs.h"		    /* for packing flags */
#include "cg_affirm.h"		    /* for Affirm flags */
#endif

extern void Set_File_In_Printsrc(char *);	/* defined in printsrc.c */
extern void Configure_CG_Target(void);    /* defined in cg_flags_arch.cxx */
extern char *WHIRL_File_Name;

/* ====================================================================
 *
 * Back	end process-specific global data from glob.h.
 *
 * ====================================================================
 */

/* Output requested: */
BOOL Assembly =	FALSE;		/* Assembly code */
BOOL MiniR_Code = FALSE;        /* MiniR code */
BOOL Lai_Code = FALSE;          /* Lai code */
BOOL Object_Code = FALSE;	/* Object code */

/* Have	the OP_REGCOPY operations been translated? */
BOOL Regcopies_Translated = FALSE;

/* ====================================================================
 *
 * Local data.
 *
 * ====================================================================
 */

static char *Argv0;		    /* argv[0] from main */

/* Default file	extensions: */
#define	ASM_FILE_EXTENSION ".s"	/* Assembly code file */
#define MINIR_FILE_EXTENSION ".minir"        /* MiniR file */
#define LAI_FILE_EXTENSION ".lai"        /* LAI file */
#define	OBJ_FILE_EXTENSION ".o"	/* Relocatable object file */
#define DSTDUMP_FILE_EXTENSION ".be.dst" /* DST dump-file extension */

/* Internal flags: */
static BOOL cg_opt_level_overridden = FALSE;

static BOOL CG_tail_call_overridden = FALSE;
static BOOL CG_enable_prefetch_overridden = FALSE;
static BOOL CG_enable_z_conf_prefetch_overridden  = FALSE;
static BOOL CG_enable_nz_conf_prefetch_overridden = FALSE;
static BOOL CG_enable_pf_L1_ld_overridden = FALSE;
static BOOL CG_enable_pf_L1_st_overridden = FALSE;
static BOOL CG_enable_pf_L2_ld_overridden = FALSE;
static BOOL CG_enable_pf_L2_st_overridden = FALSE;
static BOOL CG_L1_ld_latency_overridden;
static BOOL CG_L2_ld_latency_overridden;
static BOOL CG_L1_pf_latency_overridden;
static BOOL CG_L2_pf_latency_overridden;
static BOOL CG_maxinss_overridden = FALSE;
static BOOL Enable_CG_Peephole_overridden = FALSE;
static BOOL EBO_Opt_Level_overridden = FALSE;
static BOOL Integer_Divide_By_Constant_overridden = FALSE;
static BOOL Integer_Divide_Use_Float_overridden = FALSE;
#ifdef KEY
static BOOL Integer_Multiply_By_Constant_overridden = FALSE;
#endif
static BOOL CG_DEP_Mem_Arc_Pruning_overridden = FALSE;
static BOOL clone_incr_overridden = FALSE;
static BOOL clone_min_incr_overridden = FALSE;
static BOOL clone_max_incr_overridden = FALSE;
static BOOL CFLOW_Enable_Clone_overridden = FALSE;
#ifdef TARG_ST
//TB
static BOOL CFLOW_Enable_Favor_Branches_Condition_overridden = FALSE;
static BOOL CFLOW_Space_overridden = FALSE;
static BOOL CFLOW_depgraph_use_overridden = FALSE;
static BOOL CFLOW_enable_last_pass_overridden = FALSE;
static BOOL EMIT_space_overridden = FALSE;
static BOOL CG_ifc_freq_overriden = FALSE;
static BOOL CG_ifc_cycles_overridden = FALSE;
static BOOL CG_ifc_allow_dup_overridden = FALSE;
static BOOL CG_ifc_space_overridden = FALSE;
static BOOL CG_select_spec_stores_overridden = FALSE;
static BOOL CG_enable_thr_overridden = FALSE;
static BOOL IPFEC_Enable_LICM_passes_overridden = FALSE;
static BOOL CG_COLOR_use_pref_regs_overridden = FALSE;
static BOOL CG_COLOR_pref_regs_priority_overridden = FALSE;
static BOOL CG_IV_offset_space_overridden = FALSE;
#endif
#ifdef TARG_X8664
BOOL cg_load_execute_overridden = FALSE;
#endif
static BOOL CG_enable_ssa_overridden = FALSE;
static BOOL CG_enable_select_overridden = FALSE;
static BOOL CG_enable_range_propagation_overridden = FALSE;
static BOOL CG_range_recompute_limit_overridden = FALSE;
static BOOL CG_enable_loop_optimizations_overridden = FALSE;
static BOOL CG_LOOP_unroll_multi_bb_overridden = FALSE;
static BOOL IPFEC_Enable_LICM_overridden = FALSE;
static BOOL CG_enable_cbpo_overriden = FALSE;
static BOOL CG_affirm_opt_overridden = FALSE;

static BOOL CG_LAO_activation_overridden = FALSE;
static BOOL CG_LAO_regiontype_overridden = FALSE;
static BOOL CG_LAO_conversion_overridden = FALSE;
static BOOL CG_LAO_coalescing_overridden = FALSE;
static BOOL CG_LAO_predication_overridden = FALSE;
static BOOL CG_LAO_scheduling_overridden = FALSE;
static BOOL CG_LAO_allocation_overridden = FALSE;
static BOOL CG_LAO_rcmssolving_overridden = FALSE;
static BOOL CG_LAO_preloading_overridden = FALSE;
static BOOL CG_LAO_l1missextra_overridden = FALSE;
static BOOL CG_LAO_compensation_overridden = FALSE;
static BOOL CG_LAO_speculation_overridden = FALSE;
static BOOL CG_LAO_relaxation_overridden = FALSE;
static BOOL CG_LAO_pipelining_overridden = FALSE;
static BOOL CG_LAO_logtimeout_overridden = FALSE;
static BOOL CG_LAO_renaming_overridden = FALSE;
static BOOL CG_LAO_boosting_overridden = FALSE;
static BOOL CG_LAO_aliasing_overridden = FALSE;
static BOOL CG_LAO_prepadding_overridden = FALSE;
static BOOL CG_LAO_postpadding_overridden = FALSE;
static BOOL CG_LAO_overrun_overridden = FALSE;
static BOOL CG_LAO_opslimit_overridden = FALSE;

static BOOL CG_split_BB_length_overridden = FALSE;

static BOOL CG_gen_callee_saved_regs_mask_overriden = FALSE;

#ifdef TARG_ST200
BOOL CG_NOPs_to_GOTO = FALSE;
static BOOL CG_NOPs_to_GOTO_overridden = FALSE;
BOOL CG_nop_insertion_directives = TRUE;
#endif
#ifdef TARG_ST
BOOL CG_LOOP_opt_iv_offset = TRUE;
BOOL CG_AutoMod = FALSE;
static BOOL CG_AutoMod_overridden = FALSE;
BOOL CG_AutoMod_RelaxPdom = FALSE;
static BOOL CG_AutoMod_RelaxPdom_overridden = FALSE;
BOOL CG_cbpo_optimize_load_imm = FALSE;
static BOOL CG_cbpo_optimize_load_imm_overridden = FALSE;
INT32 CG_cbpo_ratio = 50;
static BOOL CG_cbpo_ratio_overridden = FALSE;
INT CG_cbpo_block_method = CBPO_BLOCK_NONE;
static BOOL CG_cbpo_block_method_overridden = FALSE;
BOOL CG_cbpo_optimize_load_imm_cst = FALSE;
static BOOL CG_cbpo_optimize_load_imm_cst_overridden = FALSE;
BOOL CG_cbpo_facto_cst = FALSE;
static BOOL CG_cbpo_facto_cst_overridden = FALSE;

static BOOL GCM_PRE_Enable_Scheduling_overridden = FALSE;
static BOOL GCM_Forw_Circ_Motion_overridden = FALSE;

BOOL  CG_ifc_subpart = FALSE ;
static BOOL CG_ifc_subpart_overridden = FALSE;
INT32 CG_force_select=0;

BOOL  CG_safe_memmove = FALSE ;

#   ifdef TARG_STxP70
#      define DEFAULT_LOAD_IMM(opt_level, opt_space) TRUE
#      define DEFAULT_RATIO(opt_level, opt_space) opt_level <= 2? 50: 100;
#      define DEFAULT_BLOCK_METHOD(opt_level, opt_space) opt_space? CBPO_BLOCK_NONE: CBPO_BLOCK_GLOBAL_THEN_LOCAL;
#      define DEFAULT_FACTO_CST(opt_level, opt_space) opt_level >= 2? TRUE: FALSE;
// No else case, since currently not used for other targets.
#   endif

static BOOL CG_tailmerge_overridden = FALSE;
static BOOL CG_simp_flow_in_tailmerge_overridden = FALSE;
static BOOL LRA_minregs_overridden = FALSE;

#   ifdef TARG_STxP70
#      define DEFAULT_TAILMERGE(opt_level, opt_space) 1
#      define DEFAULT_SIMP_FLOW_TAILMERGE(opt_level, opt_space) 1
// No else case, since currently not used for other targets.
#   endif

#endif
#ifdef TARG_ST
static BOOL LOCS_POST_Scheduling_overriden;
static BOOL CG_enable_min_max_abs_overriden = FALSE;
/* 
 * TDR options controlled by mask, previously mixed with option -Wb,-tt...
 * See art #41947 
 */
INT32 CG_sched_mask = 0;
INT32 CG_LRA_mask = 0;
INT32 CG_sched_extract_earliest = SCHED_EXTRACT_EARLIEST_ON;

/* 
 * [GS] options for the exportation to DfgGraph and ISE identification.
 */
INT32 CG_dfg_ise_mask = 0;
INT32 CG_dfg_debug_mask = 0;
#endif
/* Keep	a copy of the command line options for assembly	output:	*/
static char *option_string;

/* Software pipelining options: */
static OPTION_DESC Options_CG_SWP[] = {
  /* General software pipelining options */
#ifdef TARG_ST
  { OVK_BOOL,	OV_INTERNAL,	FALSE, "enable", NULL,
    0, 0, 0,	&Enable_SWP, &Enable_SWP_overridden },
#else
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "", NULL,
    0, 0, 0,	&Enable_SWP, &Enable_SWP_overridden },

  { OVK_BOOL,	OV_INTERNAL,	FALSE, "verbose", "v",
    TRUE, 0, 0,	&SWP_Options.Enable_Verbose_Mode, NULL },
#endif
  { OVK_INT32,	OV_INTERNAL,	TRUE, "sched_direction", "sched_dir",
    0, 0, INT32_MAX,	&SWP_Options.Sched_Direction, NULL },
#if defined( TARG_IA64) || defined(TARG_ST)
  { OVK_INT32,	OV_INTERNAL,	TRUE, "heuristics", "heur",
    0, 0, INT32_MAX,	&SWP_Options.Heuristics, NULL },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "opt", "opt",
    0, 0, INT32_MAX,	&SWP_Options.Opt_Level, NULL },
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "while_loop", NULL,
    0, 0, 0,	&SWP_Options.Enable_While_Loop, NULL },
#endif
  { OVK_INT32,	OV_INTERNAL,	TRUE, "min_unroll_times", "min_unr",
    0, 0, INT32_MAX,	&SWP_Options.Min_Unroll_Times, &SWP_Options.Min_Unroll_Times_Set },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "max_unroll_times", "max_unr",
    0, 0, INT32_MAX,	&SWP_Options.Max_Unroll_Times, &SWP_Options.Max_Unroll_Times_Set },
#ifndef TARG_ST
  { OVK_BOOL,	OV_INTERNAL,	FALSE, "pow2_unroll_times", "pow2_unr",
    TRUE, 0, 0, &SWP_Options.Pow2_Unroll_Times, NULL },
#endif
#if defined( TARG_IA64) || defined(TARG_ST)
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "bundle", NULL,
    TRUE, 0, 0,	&SWP_Options.Enable_Bundling, NULL },
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "postincr", "posti",
    0, 0, 0,	&SWP_Options.Enable_Post_Incr, NULL },
#endif
  { OVK_INT32,	OV_INTERNAL,	TRUE, "start_ii", "start",
    0, 0, INT32_MAX,	&SWP_Options.Starting_II, NULL },
#if defined( TARG_IA64) || defined(TARG_ST)
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "workaround", "work", 
    0, 0, 0,	&SWP_Options.Enable_Workaround, NULL },
#endif
  { OVK_INT32,	OV_INTERNAL,	TRUE, "critical_threshold", "critical",
    0, 0, INT32_MAX,	&SWP_Options.Critical_Threshold, NULL },
#if defined( TARG_IA64) || defined(TARG_ST)
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "prep_only", "", 
    0, 0, 0,	&SWP_Options.Prep_Only, NULL },
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "min_retry", "", 
    0, 0, 0,	&SWP_Options.Min_Retry, NULL },
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "implicit_prefetch", "", 
    0, 0, 0,	&SWP_Options.Implicit_Prefetch, &SWP_Options.Implicit_Prefetch_Set },
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "predicate_promotion", "", 
    0, 0, 0,	&SWP_Options.Predicate_Promotion, NULL },
  { OVK_BOOL,	OV_INTERNAL,	TRUE, "enable_brp", "", 
    0, 0, 0,	&SWP_Options.Enable_BRP, NULL },
#endif
#ifndef TARG_ST
#ifndef TARG_IA64
  { OVK_INT32,	OV_INTERNAL,	TRUE, "budget", NULL,
    0, 0, INT32_MAX,	&SWP_Options.Budget, &SWP_Options.Budget_Set },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "min_trip_count", NULL,
    0, 0, INT32_MAX,	&SWP_Options.Min_Trip_Count, &SWP_Options.Min_Trip_Count_Set },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "max_ii", NULL,
    0, 0, INT32_MAX,	&SWP_Options.Max_II, 0 },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "linear_search_limit", "linear",
    0, 0, INT32_MAX,	&SWP_Options.Linear_Search_Limit, 0 },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "max_ii_factor", NULL,
    0, 0, INT32_MAX,	&SWP_Options.MaxII_Factor, 0 },
  { OVK_BOOL,	OV_INTERNAL,	FALSE, "jump_block", NULL, 
    0, 0, 0,	&SWP_Options.Enable_Jump_Block, NULL },
  { OVK_BOOL,	OV_INTERNAL,	FALSE, "offset_adjustment", "offset", 
    0, 0, 0,	&SWP_Options.Enable_Offset_Adjustment, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "offset_skip_before", NULL,
    0, 0, INT32_MAX, &SWP_Options.Offset_Skip_Before, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "offset_skip_after", NULL,
    0, 0, INT32_MAX, &SWP_Options.Offset_Skip_After, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "offset_skip_equal", NULL,
    0, 0, INT32_MAX, &SWP_Options.Offset_Skip_Equal, NULL }, 
#endif
#endif
  { OVK_COUNT }		/* List terminator -- must be last */
};

/* Global register allocator options */
static OPTION_DESC Options_GRA[] = {
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "optimize_placement", "",
    0,0,0,      &GRA_optimize_placement, NULL,
    "Enable/disable movement of spills and restores created during splitting [Default TRUE]."
  },
#ifdef TARG_X8664
  { OVK_INT32,	OV_INTERNAL, TRUE, "local_forced_max", "local_forced_max",
    4, 0, 16,	&GRA_local_forced_max, &GRA_local_forced_max_set,
    "How many locals to force allocate (out of the number requested by LRA) [Default 4]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "unspill", "",
    0,0,0,	&GRA_unspill_enable, NULL,
    "Enable/disable fusing of GRA spills and restores back to registers [Default FALSE]"
  },
#else
  { OVK_INT32,	OV_INTERNAL, TRUE, "local_forced_max", "",
    4, 0, 32,	&GRA_local_forced_max, NULL,
    "How many locals to force allocate (out of the number requested by LRA) [Default 4]"
  },
#endif // TARG_X8664
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "avoid_glue_references_for_locals", "",
    0,0,0,      &GRA_avoid_glue_references_for_locals,NULL,
    "If possible grant the forced locals from the set of registers not referenced for glue copies in the same block.  [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "split_entry_exit_blocks", "",
    0,0,0,	&GRA_split_entry_exit_blocks,NULL,
    "Enable/Disable splitting of entry/exit blocks for callee saved preferencing [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "split_lranges", "",
    0,0,0,      &GRA_split_lranges, NULL,
    "Turn on/off splitting of live ranges [Default TRUE]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "non_split_tn", "",
    4, 0, INT32_MAX,	&GRA_non_split_tn_id, NULL,
    "Turn off live range splitting for a given TN specified by its tn number (n).  [Default -1]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "non_preference_tn", "",
    4, 0, INT32_MAX,	&GRA_non_preference_tn_id, NULL,
    "Turn off preferencing for a given TN specified by its tn number (n). [Default -1]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "use_old_conflict", "",
    0,0,0,      &GRA_use_old_conflict, NULL,
    "Use old conflict graph algorithm ... not functioning at present."
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "shrink_wrap", "",
    0,0,0,      &GRA_shrink_wrap, NULL,
    "Turn on/off shrink wrapping (currently, only for callee saved regs) [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "loop_splitting", "",
    0,0,0,      &GRA_loop_splitting, NULL,
    "Turn on/off loop directed live range splitting [Default TRUE]",
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "home", "",
    0,0,0,      &GRA_home, NULL,
    "Turn on/off gra homing [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "remove_spills", "",
    0,0,0,      &GRA_remove_spills, NULL,
    "Turn on/off gra removal of spill instructions in Optimize_Placment [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "ensure_spill_proximity", "",
    0,0,0,      &GRA_ensure_spill_proximity, NULL,
    "Turn on/off gra placing spills close to use/def in block [Default TRUE]"
  },    
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "choose_best_split", "",
    0,0,0,      &GRA_choose_best_split, NULL,
    "Turn on/off gra choosing best/smallest interim split found [Default TRUE]"
  },    
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "use_stacked_regs", "",
    0,0,0,      &GRA_use_stacked_regs, NULL,
    "Turn on/off gra using stacked registers [Default TRUE]"
  },    
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "redo_liveness", "",
    0,0,0,      &GRA_redo_liveness, NULL,
    "Turn on/off recalculation of liveness [Default FALSE]"
  },    
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "preference_globals", "",
    0,0,0,      &GRA_preference_globals, NULL,
    "Turn on/off gra preferencing of global TNs (other than glue code) [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "preference_dedicated", "",
    0,0,0,      &GRA_preference_dedicated, NULL,
    "Turn on/off gra preferencing with dedicated TNs  [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "preference_glue", "",
    0,0,0,      &GRA_preference_glue, NULL,
    "Turn on/off gra preferencing in glue code [Default TRUE]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "preference_all", "",
    0,0,0,      &GRA_preference_all, NULL,
    "Turn on/off all gra preferencing [Default TRUE]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "non_home_low", "",
    4, 0, INT32_MAX,	&GRA_non_home_lo, NULL,
    "Turn off homing for a TN range specified by its tn numbers.  [Default INT32_MAX]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "non_home_hi", "",
    4, 0, INT32_MAX,	&GRA_non_home_hi, NULL,
    "Turn off homing for a TN range specified by its tn numbers.  [Default -1]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "recalc_liveness", "",
    0,0,0,      &GRA_recalc_liveness, NULL,
    "Turn on/off recomputation of global liveness info [Default FALSE]"
  },    
  { OVK_NAME,   OV_INTERNAL, TRUE,"call_split_freq", "",
#ifdef TARG_ST
    0, 0, 0,	&GRA_call_split_freq_string,
#else
    0, 0, 0,	&GRA_call_split_freq_string,
#endif
#ifdef KEY
    NULL,
#endif // KEY
    "Threshold frequency of block containing a call below which a caller saved register will be preferred and live ranges spanning it will be split [Default .1]"
  },    
  { OVK_NAME,   OV_INTERNAL, TRUE,"spill_count_factor", "",
#ifdef TARG_ST
    0, 0, 0,	&GRA_spill_count_factor_string,
#else
    0, 0, 0,	&GRA_spill_count_factor_string,
#endif
#ifdef KEY
    NULL,
#endif // KEY
    "Factor by which count of spills affects the priority of a split.  Only valid under OPT:space [Default 0.5]"
  },
#ifdef TARG_ST
  //TB:
  { OVK_BOOL,   OV_INTERNAL, TRUE,"size_spill_count_factor", "",
    0, 0, 0,    &GRA_spill_count_factor_for_size, &GRA_spill_count_factor_for_size_set,
    "Affects the priority of a split by count of spills . Default is true in Os",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"split_for_size", "",
    0, 0, 0,    &GRA_split_for_size, &GRA_split_for_size_set,
    "Take into account code size when choosing the best split in the GRA ",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"spill_min", "",
    0, 0, 0,    &GRA_spill_count_min, &GRA_spill_count_min_set,
    "Don't increase spill count ",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"use_rn_spill_metric", "",
    0, 0, 0,    &GRA_use_runeson_nystrom_spill_metric, NULL,
    "Use Runeson/Nystrom spill metric during simplify (only applicable when Runeson/Nystrom local colorability test is selected)",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"use_interprocedural_info", "",
    0, 0, 0,    &GRA_use_interprocedural_info, NULL,
    "Use interprocedural information during register allocation",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"spill_to_caller_save", "",
    0, 0, 0,    &GRA_spill_to_caller_save, NULL,
    "Use interprocedural information during register allocation",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"preference_subclass", "",
    0, 0, 0,    &GRA_preference_subclass, NULL,
    "Preference to subclass members",
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"use_subclass_register_request", "",
    0, 0, 0,    &GRA_use_subclass_register_request, NULL,
    "Use subclass information in the LRA register request",
  },
  { OVK_NAME,   OV_INTERNAL, TRUE,"local_spill_multiplier", "",
    0, 0, 0,	&GRA_local_spill_multiplier_string, NULL,
    "Multiplier for GRA estimate of cost of spilling a local live range [Default 1.0]"
  },    
  { OVK_BOOL,   OV_INTERNAL, TRUE,"overlay_spills", "",
    0, 0, 0,    &GRA_overlay_spills, NULL,
    "Overlay spill locations",
  },
#endif
#if defined( KEY) && !defined(TARG_ST)
  { OVK_BOOL,   OV_INTERNAL, TRUE,"exclude_saved_regs", "",
    0, 0, 0,	&GRA_exclude_callee_saved_regs, NULL,
    "If true, callee-saved registers are never used to allocate to variables by GRA"
  },    
  { OVK_BOOL,   OV_INTERNAL, TRUE,"eh_exclude_saved_regs", "",
    0, 0, 0,	&GRA_eh_exclude_callee_saved_regs, NULL,
    "If true, callee-saved registers are never used to allocate to variables in functions with exception handlers"
  },    
  { OVK_BOOL,   OV_INTERNAL, TRUE,"fp_exclude_saved_regs", "",
    0, 0, 0,	&GRA_fp_exclude_callee_saved_regs, NULL,
    "If true, floating-point callee-saved registers are never used to allocate to variables by GRA [Default FALSE]"
  },    
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "optimize_boundary", "",
    0,0,0,      &GRA_optimize_boundary, &GRA_optimize_boundary_set,
    "Enable/disable reuse of registers in live range boundary basic blocks [Default FALSE]."
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "prioritize_by_density", "",
    0,0,0,      &GRA_prioritize_by_density, &GRA_prioritize_by_density_set,
    "Enable/disable prioritizing live ranges by reference density [Default FALSE]."
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "reclaim", "",
    0,0,0,      &GRA_reclaim_register, &GRA_reclaim_register_set,
    "Enable/disable reclaiming of registers after they have been allocated [Default FALSE]."
  },
#endif // KEY
#ifdef TARG_X8664
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "grant_special_regs", "",
    0,0,0,      &GRA_grant_special_regs, NULL,
    "Force GRA to always grant rax/rcx/rdx, whether LRA needs them or not."
  },
#endif
#ifdef TARG_MIPS
  { OVK_NAME,	OV_INTERNAL, TRUE, "avoid_reg", "",
    0, 0, 0,	&GRA_avoid_reg, NULL,
    "Force GRA to avoid specific register.  Use avoid_reg=X for integer register X; use avoid_reg=fY for floating point register Y.  [Default none]"
  },
#endif
  { OVK_COUNT }		/* List terminator -- must be last */
};

static OPTION_DESC Options_CG[] = {

  // Generic CG options.

  { OVK_BOOL,	OV_INTERNAL, TRUE, "warn_bad_freqs", "",
    0, 0, 0,	&CG_warn_bad_freqs, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "skip_before", "skip_b",
    0, 0, INT32_MAX, &CG_skip_before, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "skip_after", "skip_a",
    0, 0, INT32_MAX, &CG_skip_after, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "skip_equal", "skip_e",
    0, 0, INT32_MAX, &CG_skip_equal, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "local_skip_before", "local_skip_b",
    0, 0, INT32_MAX, &CG_local_skip_before, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "local_skip_after", "local_skip_a",
    0, 0, INT32_MAX, &CG_local_skip_after, NULL }, 
  { OVK_INT32,	OV_INTERNAL, TRUE, "local_skip_equal", "local_skip_e",
    0, 0, INT32_MAX, &CG_local_skip_equal, NULL }, 
  { OVK_BOOL,	OV_INTERNAL, TRUE, "skip_local_hbf", "",
    0, 0, 0,	&CG_skip_local_hbf, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "skip_local_loop", "",
    0, 0, 0,	&CG_skip_local_loop, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "skip_local_swp", "",
    0, 0, 0,	&CG_skip_local_swp, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "skip_local_ebo", "",
    0, 0, 0,	&CG_skip_local_ebo, NULL },
#ifndef TARG_ST
  { OVK_BOOL,	OV_VISIBLE, TRUE, "cmp_peep", "",
    0, 0, 0,	&CG_cmp_load_exec, NULL },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE, "skip_local_sched", "",
    0, 0, 0,	&CG_skip_local_sched, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "optimization_level", "",
    0, 0, MAX_OPT_LEVEL,
                &CG_opt_level, &cg_opt_level_overridden },

  // EBO options:
  { OVK_BOOL,	OV_INTERNAL, TRUE, "peephole_optimize", "",
    0, 0, 0,	&Enable_CG_Peephole, &Enable_CG_Peephole_overridden },
  { OVK_BOOL, 	OV_INTERNAL, TRUE, "create_madds", "create_madd",
    0, 0, 0,  &CG_create_madds, NULL },
#ifdef KEY
  { OVK_BOOL, OV_INTERNAL, TRUE, "load_index", NULL,
    0, 0, 0,	&CG_enable_load_index, NULL },
  { OVK_BOOL, OV_INTERNAL, TRUE,  "test_coverage", "",
    0, 0, 0,    &flag_test_coverage, NULL},
  { OVK_LIST, OV_INTERNAL, FALSE, "profile_proc", "",
    0, 0, 0,    &Arc_Profile_Region, NULL},
  { OVK_LIST, OV_INTERNAL, FALSE, "profile_id1",  "",
    0, 0, 0,    &Arc_Profile_Region, NULL},
  { OVK_LIST, OV_INTERNAL, FALSE, "profile_id2", "",
    0, 0, 0,    &Arc_Profile_Region, NULL},
  { OVK_INT32,  OV_INTERNAL, FALSE,  "cse_regs", "",
    0, INT32_MIN, INT32_MAX,    &CG_cse_regs, NULL},
  { OVK_INT32,  OV_INTERNAL, FALSE,  "sse_cse_regs", "",
    0, INT32_MIN, INT32_MAX,    &CG_sse_cse_regs, NULL},
#endif
#ifdef TARG_X8664
  { OVK_INT32,  OV_INTERNAL, TRUE,  "sse_load_execute", "sse_load_exe",
    0, 0, INT32_MAX,    &CG_sse_load_execute, NULL},
  { OVK_INT32,	OV_INTERNAL, TRUE, "load_execute", "load_exe",
    0, 0, INT32_MAX,	&CG_load_execute, &cg_load_execute_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "loadbw_execute", "loadbw_exe",
    0, 0, 0,	&CG_loadbw_execute, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "valgrind_friendly", "valgrind",
    0, 0, 0,	&CG_valgrind_friendly, NULL },
#endif

  // CG Dependence Graph related options.

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ignore_lno", "",
    0, 0, 0,	&CG_DEP_Ignore_LNO, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "ignore_wopt", "",
    0, 0, 0,	&CG_DEP_Ignore_WOPT, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "addr_analysis", "",
    0, 0, 0,	&CG_DEP_Addr_Analysis, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "verify_mem_deps", "",
    0, 0, 0,	&CG_DEP_Verify_Mem_Deps, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "add_alloca_arcs", "",
    0, 0, 0,	&CG_DEP_Add_Alloca_Arcs, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "relax_xfer_depndnce", "",
    0, 0, 0,	&CG_DEP_Relax_Xfer_Dependence, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "adjust_ooo_latency", "adjust_ooo_latency",
    0, 0, 0,	&CG_DEP_Adjust_OOO_Latency, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "prune_mem", "",
    0, 0, INT32_MAX, &CG_DEP_Mem_Arc_Pruning,
    &CG_DEP_Mem_Arc_Pruning_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "prune_depndnce", "",
    0, 0, 0,	&CG_DEP_Prune_Dependence, NULL },
#ifdef KEY
  { OVK_BOOL,	OV_INTERNAL, TRUE, "ignore_loop_loadstore_dep", "",
    0, 0, 0,	&CG_DEP_Ignore_Loop_Loadstore_Dep,
    &CG_DEP_Ignore_Loop_Loadstore_Dep_Set,
    "Assume load/stores in innermost loops don't alias unless proven otherwise" },
#endif

  // Prefetching and load latency options.
 
  { OVK_BOOL,	OV_INTERNAL, TRUE,"prefetch", "",
    0, 0, 0, &CG_enable_prefetch, &CG_enable_prefetch_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"z_conf_prefetch", "",
    0, 0, 0, &CG_enable_z_conf_prefetch,
	     &CG_enable_z_conf_prefetch_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"nz_conf_prefetch", "",
    0, 0, 0, &CG_enable_nz_conf_prefetch,
	     &CG_enable_nz_conf_prefetch_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"pf_L1_ld", "",
    0, 0, 0, &CG_enable_pf_L1_ld, &CG_enable_pf_L1_ld_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"pf_L1_st", "",
    0, 0, 0, &CG_enable_pf_L1_st, &CG_enable_pf_L1_st_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"pf_L2_ld", "",
    0, 0, 0, &CG_enable_pf_L2_ld, &CG_enable_pf_L2_ld_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"pf_L2_st", "",
    0, 0, 0, &CG_enable_pf_L2_st, &CG_enable_pf_L2_st_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "exclusive_prefetch", "",
    0, 0, 0, &CG_exclusive_prefetch, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "L1_pf_latency", "",
    0, 0, INT32_MAX, &CG_L1_pf_latency, &CG_L1_pf_latency_overridden  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "L2_pf_latency", "",
    0, 0, INT32_MAX, &CG_L2_pf_latency, &CG_L2_pf_latency_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE, "L1_ld_latency", "",
    0, 0, INT32_MAX, &CG_L1_ld_latency, &CG_L1_ld_latency_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE, "L2_ld_latency", "",
    0, 0, INT32_MAX, &CG_L2_ld_latency, &CG_L2_ld_latency_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE, "z_conf_L1_ld_latency", "",
    0, 0, INT32_MAX, &CG_z_conf_L1_ld_latency, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "z_conf_L2_ld_latency", "",
    0, 0, INT32_MAX, &CG_z_conf_L2_ld_latency, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE, "ld_latency", "",
    0, 0, INT32_MAX, &CG_ld_latency, NULL },
#ifdef TARG_ST
  { OVK_BOOL,	OV_INTERNAL, TRUE, "warn_prefetch_padding", "",
    1, 0, 0, &CG_warn_prefetch_padding, NULL },
#endif

  // CGLOOP options.

  { OVK_BOOL,	OV_INTERNAL, TRUE, "loop_opt", "loop_opt",
    0, 0, 0,	&CG_enable_loop_optimizations, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "opt_non_trip_countable", "opt_non_trip",
    0, 0, 0,	&CG_LOOP_optimize_non_trip_countable, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "opt_lno_winddown_cache", NULL,
    0, 0, 0,	&CG_LOOP_optimize_lno_winddown_cache, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "opt_lno_winddown_reg", NULL,
    0, 0, 0,	&CG_LOOP_optimize_lno_winddown_reg, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "opt_non_innermost", "opt_non_inner",
    0, 0, 0,	&CG_LOOP_optimize_non_innermost, NULL },

  { OVK_BOOL,	OV_INTERNAL, TRUE,  "fix_recurrences", "",
    0, 0, 0,    &CG_LOOP_fix_recurrences,
		&CG_LOOP_fix_recurrences_specified },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "back_substitution", "",
    0, 0, 0,    &CG_LOOP_back_substitution,
		&CG_LOOP_back_substitution_specified },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "back_substitution_variant", "",
    0, 0, 0,    &CG_LOOP_back_substitution_variant,
		&CG_LOOP_back_substitution_variant_specified },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "interleave_reductions", "",
    0, 0, 0,    &CG_LOOP_interleave_reductions,
		&CG_LOOP_interleave_reductions_specified },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "interleave_posti", "",
    0, 0, 0,    &CG_LOOP_interleave_posti,
		&CG_LOOP_interleave_posti_specified },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "nested_interleave", "",
    0, 0, 0,	&CG_LOOP_nested_interleave,
		&CG_LOOP_nested_interleave_specified },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "reassociate", "reassoc",
    0, 0, 0,    &CG_LOOP_reassociate,
		&CG_LOOP_reassociate_specified },
  { OVK_INT32, OV_INTERNAL, TRUE, "recurrence_min_omega", "",
    0, 0, INT32_MAX, &CG_LOOP_recurrence_min_omega, NULL },
#ifdef KEY
  { OVK_INT32, OV_INTERNAL, TRUE, "recurrence_max_omega", "",
    0, 0, 16, &CG_LOOP_recurrence_max_omega, NULL },
  { OVK_INT32, OV_INTERNAL, TRUE, "loop_limit", "",
    INT32_MAX, 0, INT32_MAX, &CG_Enable_Loop_Opt_Limit, NULL },
#endif
#ifdef TARG_X8664
  { OVK_BOOL,	OV_INTERNAL, TRUE, "cloop", "",
    0, 0, 0,	&CG_LOOP_cloop, NULL },
#endif
#ifdef KEY
  { OVK_UINT32, OV_INTERNAL, TRUE, "post_remainder_loop", "",
    0, 0, 16, &CG_LOOP_post_remainder_loop, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "convert_prefetch_to_load", "",
    0, 0, 0, &CG_LOOP_convert_prefetch_to_load, NULL },
#endif

  // CG Unrolling options - see also OPT:unroll_times_max:unroll_size.

  { OVK_BOOL,	OV_INTERNAL, TRUE,"unroll_non_trip_countable", "unroll_non_trip",
    0, 0, 0, &CG_LOOP_unroll_non_trip_countable, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"unroll_fully", "unroll_full",
    0, 0, 0, &CG_LOOP_unroll_fully, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"unroll_remainder_fully", "unroll_remainder_full",
    0, 0, 0, &CG_LOOP_unroll_remainder_fully, NULL },
  { OVK_BOOL,	OV_VISIBLE, TRUE,"unroll_fb_req", "",
    0, 0, 0, &CG_LOOP_unroll_fb_required, NULL },
#ifdef TARG_ST
  { OVK_BOOL,	OV_INTERNAL, TRUE,"unroll_do_unwind", "",
    TRUE, 0, 0, &CG_LOOP_unroll_do_unwind, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"unroll_remainder_after", "",
    TRUE, 0, 0, &CG_LOOP_unroll_remainder_after, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"unroll_multi_bb", "",
    TRUE, 0, 0, &CG_LOOP_unroll_multi_bb, &CG_LOOP_unroll_multi_bb_overridden },
  { OVK_INT32,  OV_INTERNAL, TRUE, "unroll_heuristics", "",
    7, 0, 7,    &CG_LOOP_unroll_heuristics, NULL },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "licm_opt", "", 
    1, 0, 2,	&IPFEC_Enable_LICM, &IPFEC_Enable_LICM_overridden },
  /* [vcdv] this option controls which of the 3 passes of licm should
     run. the parameter is a 3bit mask */
  { OVK_INT32,	OV_INTERNAL,	TRUE, "licm_passes", "", 
    0x07, 0, 0x07,	&IPFEC_Enable_LICM_passes, &IPFEC_Enable_LICM_passes_overridden },
#ifdef TARG_ST200
  { OVK_INT32,	OV_INTERNAL,	TRUE, "packing_load_store", "", 
    0x25B, 0, 0x3FF,	&CG_LOOP_Packing_flags, NULL },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "packing_sizes", "", 
    0x25B, 0, 0x3FF,	&CG_LOOP_Packing_sizes, NULL },
#endif
  { OVK_INT32, OV_INTERNAL,	TRUE, "stream_align", "", 
    8, 0, 8,	&CG_LOOP_stream_align, NULL },
  { OVK_BOOL, OV_INTERNAL,	TRUE, "optimize_iv_offsets", "", 
    0, 0, 0, &CG_LOOP_opt_iv_offset, NULL,
    "activate optimization of references to induction variables with large offsets" },
  { OVK_BOOL, OV_INTERNAL,	TRUE, "cbpo_opt", "cbpo", 
    0, 0, 0, &CG_enable_cbpo, &CG_enable_cbpo_overriden,
    "activate common base pointer optimization" },
  { OVK_BOOL, OV_INTERNAL,	TRUE, "cbpo_optimize_load_imm", NULL, 
    0, 0, 0,	&CG_cbpo_optimize_load_imm,
    &CG_cbpo_optimize_load_imm_overridden,
    "activate commonalization of load immediate in common base pointer "
    "optimization" },
  { OVK_INT32, OV_INTERNAL,	TRUE, "cbpo_ratio", NULL, 
    50, 0, INT32_MAX,	&CG_cbpo_ratio, &CG_cbpo_ratio_overridden,
    "set minmum frequency ratio to allow commonalization in common base "
    "pointer optimization. The ratio is used in the following formula "
    "sum(freq(sources basic blocks)) / freq(dominator) > min_cbpo_ratio. Given"
    " value will be divided by 100.0 to allow more precise setting."
    " E.g. for 50 it means that the optimization will be done if "
    "sum(freq(sourcesBB) / freq(dominatorBB) > 0.5" },
  { OVK_INT32, OV_INTERNAL,	TRUE, "cbpo_block_method", NULL, 
    0, 0, 3,	&CG_cbpo_block_method, &CG_cbpo_block_method_overridden,
    "Specify what method should be used to block common base pointer "
    "optimization. Possible values are: 0 (none), 1 (local), 2 (global) or "
    "3 (global then local). The 'none' method never blocks the "
    "optimization. The 'local' method blocks the optimization, for a common "
    "base, when sum(freq(current sources bb)) / freq(current dominator) < "
    "cbpo_ratio. Then 'global' method blocks the optimization, for a common "
    "base, when sum(freq(all sources bb)) / freq(global dominator) < "
    "cbpo_ratio. Finally, the both method try the global estimation and if "
    "it fails, it tries the local."},
  { OVK_BOOL, OV_INTERNAL,	FALSE, "cbpo_optimize_load_imm_cst", "", 
    0, 0, 0,	&CG_cbpo_optimize_load_imm_cst,
    &CG_cbpo_optimize_load_imm_cst_overridden,
    "activate commonalization of load immediate constant (vs symbol) in common"
    " base pointer optimization. Unless cbpo_optimize_load_imm cbpo_facto_cst "
    " are set, it has no effect" },
  { OVK_BOOL, OV_INTERNAL,     FALSE, "cbpo_facto_cst", "", 
    0, 0, 0,	&CG_cbpo_facto_cst, &CG_cbpo_facto_cst_overridden,
    "activate special phase of load immediat commonalization" },
  { OVK_BOOL, 	OV_INTERNAL, TRUE, "automod", "",
    TRUE, 0, 0, &CG_AutoMod, &CG_AutoMod_overridden },
  { OVK_BOOL,   OV_INTERNAL, TRUE, "relax_automod_pdom", "",
    TRUE, 0, 0, &CG_AutoMod_RelaxPdom, &CG_AutoMod_RelaxPdom_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "tailmerge", "", 
    0, 0, 255,	&CG_tailmerge, &CG_tailmerge_overridden,
    "Activate tailmerge optimization for phases (mask)" },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "simp_flow_in_tailmerge", "", 
    0, 0, 255,	&CG_simp_flow_in_tailmerge,
    &CG_simp_flow_in_tailmerge_overridden, "Activate flow simplification"
    " in tailmerge for phases (mask). Note that it has an effect only for "
    "related activated tailmerge phases" },
#endif

#ifdef TARG_ST200
  { OVK_BOOL,	OV_INTERNAL, TRUE, "nop2goto", "",
    TRUE, 0, 0, &CG_NOPs_to_GOTO, &CG_NOPs_to_GOTO_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "nop_insertion_directives", "",
    TRUE, 0, 0, &CG_nop_insertion_directives, NULL,
    "control emission of assembler nop insertion directives" },
#endif /* TARG_ST200 */

  // Cross Iteration Loop Optimization options.

  { OVK_BOOL,	OV_INTERNAL, TRUE, "cio_invariant_copy_removal", "",
    0, 0, 0, &CIO_enable_invariant_copy_removal, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "cio_copy_removal", "",
    0, 0, 0, &CIO_enable_copy_removal, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "cio_read_removal", "",
    0, 0, 0, &CIO_enable_read_removal, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "cio_write_removal", "",
    0, 0, 0, &CIO_enable_write_removal, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "cio_cse_removal", "",
    0, 0, 0, &CIO_enable_cse_removal, NULL },
  { OVK_INT32, OV_INTERNAL, TRUE, "cio_rw_max_omega", "",
    8, 0, INT32_MAX, &CIO_rw_max_omega, NULL },
  { OVK_INT32,  OV_INTERNAL, TRUE, "cio_reserve_int", "",
    2, INT32_MIN, INT32_MAX, &CIO_reserve_int, NULL },
  { OVK_INT32,  OV_INTERNAL, TRUE, "cio_reserve_fp", "",
    2, INT32_MIN, INT32_MAX, &CIO_reserve_fp, NULL },


  // Control flow optimizations (CFLOW) options.

  { OVK_BOOL,	OV_INTERNAL, TRUE, "unique_exit", "",
    0, 0, 0,	&CG_unique_exit, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "tail_call", "",
    0, 0, 0,	&CG_tail_call, &CG_tail_call_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_before_cgprep", NULL,
    0, 0, 0, &CFLOW_opt_before_cgprep, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_after_cgprep", "cflow_after_cgprep",
    0, 0, 0, &CFLOW_opt_after_cgprep, NULL },
  { OVK_INT32,  OV_INTERNAL, TRUE,"ebo_level", "ebo",
    0, INT32_MIN, INT32_MAX, &EBO_Opt_Level, &EBO_Opt_Level_overridden },
#ifdef KEY
  { OVK_INT32,  OV_INTERNAL, TRUE,"ebo_opt_mask", "",
    0, INT32_MIN, INT32_MAX, &EBO_Opt_Mask, NULL },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow", NULL,
    0, 0, 0, &CFLOW_Enable, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow", NULL,
    0, 0, 0, &CFLOW_Enable, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_unreachable", "",
    0, 0, 0, &CFLOW_Enable_Unreachable, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_branch", "",
    0, 0, 0, &CFLOW_Enable_Branch, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_merge", "",
    0, 0, 0, &CFLOW_Enable_Merge, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_reorder", "",
    0, 0, 0, &CFLOW_Enable_Reorder, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_clone", "",
    0, 0, 0, &CFLOW_Enable_Clone, &CFLOW_Enable_Clone_overridden },
#ifdef KEY
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_freq_order", "cflow_freq_order",
    0, 0, 0, &CFLOW_Enable_Freq_Order, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_freq_order_on_heu", "cflow_freq_order_on_heu",
    0, 0, 0, &CFLOW_Enable_Freq_Order_On_Heuristics, NULL },
#else
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_freq_order", "",
    0, 0, 0, &CFLOW_Enable_Freq_Order, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_freq_order_on_heu", "",
    0, 0, 0, &CFLOW_Enable_Freq_Order_On_Heuristics, NULL },
#endif // KEY
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_opt_all_br_to_bcond", "",
    0, 0, 0, &CFLOW_opt_all_br_to_bcond, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"cflow_heuristic_tolerance", "",
    0, 0, 0, &CFLOW_heuristic_tolerance, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"cflow_feedback_tolerance", "",
    0, 0, 0, &CFLOW_feedback_tolerance, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE,"cflow_clone_incr", "cflow_clone_i",
    0, 0, 100, &CFLOW_clone_incr, &clone_incr_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE,"cflow_clone_min_incr", "cflow_clone_mi",
    0, 0, INT32_MAX, &CFLOW_clone_min_incr, &clone_min_incr_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE,"cflow_clone_max_incr", "cflow_clone_ma",
    0, 0, INT32_MAX, &CFLOW_clone_max_incr, &clone_max_incr_overridden },
  { OVK_NAME,	OV_INTERNAL, TRUE,"cflow_cold_threshold", "",
    0, 0, 0, &CFLOW_cold_threshold, NULL },
#ifdef TARG_ST
  // [CL]
  { OVK_BOOL,	OV_INTERNAL, FALSE,"cflow_favor_branches_condition", "",
    0, 0, 0, &CFLOW_Enable_Favor_Branches_Condition, &CFLOW_Enable_Favor_Branches_Condition_overridden },
  // [TB]
  { OVK_BOOL,	OV_INTERNAL, FALSE,"cflow_space", "",
    0, 0, 0, &CFLOW_Space, &CFLOW_Space_overridden },
  { OVK_BOOL,	OV_INTERNAL, FALSE,"cflow_depgraph_use", "",
    0, 0, 0, &CFLOW_depgraph_use, &CFLOW_depgraph_use_overridden },
  { OVK_BOOL,	OV_INTERNAL, FALSE,"cflow_enable_last_pass", "",
    0, 0, 0, &CFLOW_enable_last_pass, &CFLOW_enable_last_pass_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cflow_hoist_rts", "",
    0, 0, 0, &CFLOW_Enable_Hoist_rts, NULL },
#endif

  // Frequency heuristic/feedback options.

  { OVK_BOOL,	OV_INTERNAL, TRUE,"enable_frequency", "",
    0, 0, 0, &FREQ_enable, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"eh_freq", "",
    0, 0, 0, &FREQ_eh_freq, NULL },
#ifdef KEY
  { OVK_NAME,	OV_INTERNAL, TRUE,"non_local_targ_freq", "",
    0, 0, 0, &FREQ_non_local_targ_freq, NULL },
#endif
  { OVK_NAME,	OV_INTERNAL, TRUE,"freq_frequent_never_ratio", "",
    0, 0, 0, &FREQ_frequent_never_ratio, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "freq_view_cfg", "",
    0, 0, 0, &FREQ_view_cfg, NULL },

  // Whirl2ops / Expander options.

  { OVK_NAME,	OV_INTERNAL, TRUE,"fdiv_algorithm", "fdiv",
    0, 0, 0, &CGEXP_fdiv_algorithm, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"sqrt_algorithm", "sqrt",
    0, 0, 0, &CGEXP_sqrt_algorithm, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"use_copyfcc", "",
    0, 0, 0, &CGEXP_use_copyfcc, NULL },
#ifdef TARG_ST
  { OVK_INT32,	OV_INTERNAL, TRUE,"expconst", "",
    DEFAULT_CGEXP_CONSTANT, 0, INT32_MAX, &CGEXP_expandconstant, &CGEXP_expandconstant_set },
#else
  { OVK_INT32,	OV_INTERNAL, TRUE,"expconst", "",
    DEFAULT_CGEXP_CONSTANT, 0, INT32_MAX, &CGEXP_expandconstant, NULL },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE,"normalize_logical", "normalize",
    0, 0, 0, &CGEXP_normalize_logical, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"gp_prolog_call_shared", "gp_prolog",
    0, 0, 0, &CGEXP_gp_prolog_call_shared, NULL },
#ifdef KEY
  { OVK_BOOL,	OV_INTERNAL, TRUE,"reuse_tn", "",
    0, 0, 0, &CGEXP_reuse_tn, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"integer_multiply_by_constant", "integer_multiply_by_constant",
    0, 0, 0, &CGEXP_cvrt_int_mult_to_add_shift, &Integer_Multiply_By_Constant_overridden },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE,"integer_divide_by_constant", "integer_divide_by_constant",
    0, 0, 0, &CGEXP_cvrt_int_div_to_mult, &Integer_Divide_By_Constant_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"integer_divide_use_float", "integer_divide_use_float",
    0, 0, 0, &CGEXP_cvrt_int_div_to_fdiv, &Integer_Divide_Use_Float_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"fast_imul", "",
    0, 0, 0, &CGEXP_fast_imul, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"float_consts_from_ints", "",
    0, 0, 0, &CGEXP_float_consts_from_ints, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"float_div_by_const", "",
    0, 0, 0, &CGEXP_opt_float_div_by_const, NULL },

  { OVK_NAME,	OV_INTERNAL, TRUE,"lfhint_L1", "",
    0, 0, 0, &CGEXP_lfhint_L1, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"lfhint_L2", "",
    0, 0, 0, &CGEXP_lfhint_L2, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"ldhint_L1", "",
    0, 0, 0, &CGEXP_ldhint_L1, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"ldhint_L2", "",
    0, 0, 0, &CGEXP_ldhint_L2, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"sthint_L1", "",
    0, 0, 0, &CGEXP_sthint_L1, NULL },
  { OVK_NAME,	OV_INTERNAL, TRUE,"sthint_L2", "",
    0, 0, 0, &CGEXP_sthint_L2, NULL },

  { OVK_BOOL,	OV_INTERNAL, TRUE, "localize", "localize",
    0, 0, 0, &CG_localize_tns, &CG_localize_tns_Set},
#ifdef TARG_X8664
  { OVK_BOOL,	OV_INTERNAL, TRUE, "localize_x87", "localize_x87",
    0, 0, 0, &CG_localize_x87_tns, &CG_localize_x87_tns_Set,
    "Localize x87 floating point variables.  Has no effect on integer variables.  Default off."
  },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE, "localize_using_stacked_regs", "localize_using_stack",
    0, 0, 0, &LOCALIZE_using_stacked_regs, NULL },

  // Local Register Allocation (LRA) options.

  { OVK_BOOL,	OV_INTERNAL, TRUE,"rematerialize", "remat",
    0, 0, 0, &CGSPILL_Rematerialize_Constants, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"force_rematerialization", "force_remat",
    0, 0, 0, &CGSPILL_Enable_Force_Rematerialization, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"lra_reorder", "",
    0, 0, 0, &LRA_do_reorder, NULL },
#ifdef TARG_X8664
  { OVK_BOOL,	OV_INTERNAL, FALSE, "prefer_legacy_regs", "",
    0, 0, 0, &LRA_prefer_legacy_regs, NULL },
#endif
#ifdef KEY
  { OVK_INT32,	OV_INTERNAL, TRUE, "inflate_reg_request", "inflate_reg",
    0, 0, 100, &LRA_inflate_reg_request, &LRA_inflate_reg_request_Set,
    "Inflate LRA register request by this percentage for innermost loops [Default 0]"},
  { OVK_BOOL,	OV_INTERNAL, FALSE, "prefer_lru_reg", "",
    1, 0, 0, &LRA_prefer_lru_reg, &LRA_prefer_lru_reg_Set },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "min_spill_loc_size", "",
    0,0,0,      &CG_min_spill_loc_size, NULL,
    "Turn on/off minimize spill location size [Default FALSE]"
  },    
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "min_stack_size", "",
    0,0,0,      &CG_min_stack_size, NULL,
    "Turn on/off minimize stack size [Default TRUE]"
  }, 
#endif
#ifdef TARG_ST
  { OVK_BOOL,   OV_INTERNAL, TRUE,"lra_minregs", "",
    0, 0, 0, &LRA_minregs, &LRA_minregs_overridden,
    "minimize registers used in local register allocation" },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"lra_merge_extract", "",
    0, 0, 0, &LRA_merge_extract, NULL,
    "allocate extract sources early in local register allocation" },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"lra_resched_check", "",
    0, 0, 0, &LRA_resched_check, NULL,
    "check that the LRA rescheduling reduces register pressure" },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"lra_overlap_coalescing", "",
    0, 0, 0, &LRA_overlap_coalescing, NULL,
    "support coalescing of non-conflicting overlapping live ranges in local register allocation" },
  { OVK_BOOL,   OV_INTERNAL, TRUE, "lra_no_strict_check", "",
    0, 0, 0, &LRA_no_uninit_strict_check, NULL,
    "on an uninitialized use in local register allocation, raise a warning instead of an assertion" },
#endif /* TARG_ST (LRA) */

  // Global Code Motion (GCM) options.

  {OVK_BOOL,	OV_INTERNAL, TRUE, "gcm", "gcm",
    0, 0, 0, &GCM_Enable_Scheduling, NULL },
#ifdef TARG_ST
  {OVK_BOOL,	OV_INTERNAL, TRUE, "pre_gcm", "pre_gcm",
    0, 0, 0, &GCM_PRE_Enable_Scheduling, &GCM_PRE_Enable_Scheduling_overridden },
#else
  {OVK_BOOL,    OV_INTERNAL, TRUE, "pre_gcm", "pre_gcm",
    0, 0, 0, &GCM_PRE_Enable_Scheduling, NULL },
#endif
  {OVK_BOOL,	OV_INTERNAL, TRUE, "post_gcm", "post_gcm",
    0, 0, 0, &GCM_POST_Enable_Scheduling, NULL },
  {OVK_BOOL,	OV_INTERNAL, TRUE, "force_post_gcm", "force_post_gcm",
    0, 0, 0, &GCM_POST_Force_Scheduling, NULL },
  {OVK_BOOL,	OV_INTERNAL, TRUE, "cflow_after_gcm", "cflow_after_gcm",
    0, 0, 0, &GCM_Enable_Cflow, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "cross_call_motion", "",
    0, 0, 0, &GCM_Motion_Across_Calls, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "use_sched_est", "use_sched_est",
    0, 0, 0, &GCM_Use_Sched_Est, NULL},
  {OVK_BOOL,    OV_INTERNAL, TRUE, "pre_spec_loads", "",
    0, 0, 0, &GCM_PRE_Spec_Loads, NULL},
  {OVK_BOOL,    OV_INTERNAL, TRUE, "post_spec_loads", "",
    0, 0, 0, &GCM_POST_Spec_Loads, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "pointer_speculation", "",
    0, 0, 0, &GCM_Pointer_Spec, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "speculative_ptr_deref", "",
    0, 0, 0, &GCM_Eager_Ptr_Deref, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "speculative_loads", "",
    0, 0, 0, &GCM_Speculative_Loads, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "predicated_loads", "",
    0, 0, 0, &GCM_Predicated_Loads, NULL},
#ifdef TARG_ST
  {OVK_BOOL,	OV_INTERNAL, TRUE, "forw_circ_motion", "",
    0, 0, 0, &GCM_Forw_Circ_Motion, &GCM_Forw_Circ_Motion_overridden},
#else
  {OVK_BOOL,    OV_INTERNAL, TRUE, "forw_circ_motion", "",
    0, 0, 0, &GCM_Forw_Circ_Motion, NULL},
#endif
  {OVK_BOOL,	OV_INTERNAL, TRUE, "gcm_minimize_reg_usage", "",
    0, 0, 0, &GCM_Min_Reg_Usage, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "gcm_test", "",
    0, 0, 0, &GCM_Test, NULL},
  {OVK_BOOL,	OV_INTERNAL, TRUE, "skip_gcm", "",
    0, 0, 0, &CG_Skip_GCM, NULL},
  { OVK_INT32,	OV_INTERNAL, TRUE,"gcm_from_bb", "",
    0, 0, INT32_MAX, &GCM_From_BB, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE,"gcm_to_bb", "",
    0, 0, INT32_MAX, &GCM_To_BB, NULL },
  { OVK_INT32,	OV_INTERNAL, TRUE,"gcm_result_tn", "",
    0, 0, INT32_MAX, &GCM_Result_TN, NULL },
#ifdef KEY
  // Consider no more than this number of candidate target bb's.
  { OVK_INT32,	OV_INTERNAL, TRUE,"gcm_bb_limit", "",
    0, 0, INT32_MAX, &GCM_BB_Limit, NULL },
#endif

  // Local Scheduling (LOCS) and HyperBlock Scheduling (HBS) options.

  { OVK_BOOL,	OV_INTERNAL, TRUE,"local_scheduler", "local_sched",
    0, 0, 0, &LOCS_Enable_Scheduling, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"pre_local_scheduler", "pre_local_sched",
    0, 0, 0, &LOCS_PRE_Enable_Scheduling, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"post_local_scheduler", "post_local_sched",
    0, 0, 0, &LOCS_POST_Enable_Scheduling, NULL },
#ifndef TARG_ST
  { OVK_BOOL,	OV_INTERNAL, TRUE,"branch_likely", "branch_l",
    0, 0, 0, &CGTARG_Enable_Brlikely, NULL },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE,"fill_delay_slots", "fill_delay",
    0, 0, 0, &Enable_Fill_Delay_Slots, NULL },
  { OVK_NAME,   OV_INTERNAL, TRUE,"branch_taken_prob", "",
    0, 0, 0,	&CGTARG_Branch_Taken_Prob,
		&CGTARG_Branch_Taken_Prob_overridden},
  { OVK_BOOL,	OV_INTERNAL, TRUE,"locs_form_bundles", "locs_form_bundles",
    0, 0, 0, &LOCS_Enable_Bundle_Formation, NULL },
  {OVK_BOOL,	OV_INTERNAL, TRUE, "pre_hb_scheduler", "pre_hb_sched",
    0, 0, 0, &IGLS_Enable_PRE_HB_Scheduling, NULL },
  {OVK_BOOL,	OV_INTERNAL, TRUE, "post_hb_scheduler", "post_hb_sched",
    0, 0, 0, &IGLS_Enable_POST_HB_Scheduling, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"hb_scheduler", "hb_sched",
    0, 0, 0, &IGLS_Enable_HB_Scheduling, NULL },
#ifdef KEY
  { OVK_BOOL, OV_INTERNAL, TRUE, "local_fwd_scheduler", "local_fwd_sched",
    0, 0, 0, &LOCS_Fwd_Scheduling, &LOCS_Fwd_Scheduling_set },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"local_sched_algorithm", "local_sched_alg",
    0, 0, 2, &LOCS_Scheduling_Algorithm, &LOCS_Scheduling_Algorithm_set,
    "Select basic block instruction scheduling algorithm" },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"locs_best", "",
    0, 0, 0, &LOCS_Best, &LOCS_Best_set,
    "Select best schedule produced by different scheduling heuristics" },
  { OVK_BOOL, OV_INTERNAL, TRUE, "locs_shallow_depth", "",
    0, 0, 0, &LOCS_Shallow_Depth, &LOCS_Shallow_Depth_set },
  { OVK_BOOL, OV_INTERNAL, TRUE, "locs_balance_ready_types", "",
    0, 0, 0, &LOCS_Balance_Ready_Types, &LOCS_Balance_Ready_Types_set,
    "Enable heuristic to balance the number of int and fp OPs in the ready vector" },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"locs_balance_ready_int", "",
    0, 0, 100, &LOCS_Balance_Ready_Int, &LOCS_Balance_Ready_Int_set,
    "The ready vector should contain no more than this percentage of int OPs" },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"locs_balance_ready_fp", "",
    0, 0, 100, &LOCS_Balance_Ready_Fp, &LOCS_Balance_Ready_Fp_set,
    "The ready vector should contain no more than this percentage of fp OPs" },
  { OVK_BOOL, OV_INTERNAL, TRUE, "locs_balance_unsched_types", "",
    0, 0, 0, &LOCS_Balance_Unsched_Types, &LOCS_Balance_Unsched_Types_set,
    "Enable heuristic to balance the number of unscheduled int and fp OPs" },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"locs_balance_unsched_int", "",
    0, 0, 100, &LOCS_Balance_Unsched_Int, &LOCS_Balance_Unsched_Int_set,
    "The unsched OPs should contain no more than this percentage of int OPs" },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"locs_balance_unsched_fp", "",
    0, 0, 100, &LOCS_Balance_Unsched_Fp, &LOCS_Balance_Unsched_Fp_set,
    "The unsched OPs should contain no more than this percentage of fp OPs" },
  { OVK_BOOL, OV_INTERNAL, TRUE, "locs_retry", "",
    0, 0, 0, &LOCS_Retry, &LOCS_Retry_set,
    "Retry scheduling after analyzing the current schedule to see how it can be improved" },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "locs_reduce_prefetch", "",
    0, 0, 0, &LOCS_Reduce_Prefetch, &LOCS_Reduce_Prefetch_set,
    "Delete prefetches that cannot be scheduled in an unused issue slot" },
  { OVK_BOOL,   OV_INTERNAL, TRUE, "locs_renumber_cycles", "",
    0, 0, 0, &LOCS_Renumber_Cycles, &LOCS_Renumber_Cycles_set,
    "Renumber cycles from backward scheduling to match issue cycles in forward direction" },
  { OVK_BOOL, OV_INTERNAL, TRUE, "locs_add_copy", "",
    0, 0, 0, &LOCS_Add_Copy, &LOCS_Add_Copy_set,
    "Add copies to break scheduling constraints" },
#endif

  // Turns of all scheduling (LOCS, HBS, GCM) for triaging.
  { OVK_BOOL,	OV_INTERNAL, TRUE,"all_scheduler", "all_sched",
    0, 0, 0, &IGLS_Enable_All_Scheduling, NULL },

#ifdef TARG_ST
  { OVK_INT32,	OV_INTERNAL, TRUE,"post_scheduler", "post_sched",
    2, 0, 7, &LOCS_POST_Scheduling, &LOCS_POST_Scheduling_overriden },
#endif

  // Hyperblock formation (HB) options.

  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_formation", "",
    0,0,0,      &HB_formation, NULL,
    "Turn on/off hyperblock formation [Default ON]"
  },    
#ifdef KEY
  { OVK_INT32,	OV_INTERNAL, TRUE,  "ifc_cutoff", "",
    4,0,100,      &HB_if_conversion_cut_off, NULL,
    "What is the cut-off for doing If-conversion"
  },    
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_static_freq_heuristics", "",
    0,0,0,      &HB_static_freq_heuristics, NULL,
    "Turn on/off hyperblock formation's use of different heuristics in the presence of static frequency analysis [Default ON]"
  },    
  { OVK_INT32,	OV_INTERNAL, TRUE, "hb_max_blocks", "",
    4, 0, 100,	&HB_max_blocks, NULL,
    "How many blocks allowed in a hyperblock [Default architecturally dependent]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "hb_min_blocks", "",
    4, 0, 32,	&HB_min_blocks, NULL,
    "Minimum blocks allowed in a hyperblock [Default 2]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_tail_duplication", "",
    0,0,0,      &HB_allow_tail_duplication, NULL, 
    "Flag to control tail-duplication when forming hyperblocks"
  },   
  { OVK_NAME,	OV_INTERNAL, TRUE, "hb_max_sched_growth", "",
    0, 0, 0,	&HB_max_sched_growth, NULL,
    "Multiplier for max increase in HB sched height [Default:3.0]"
  },
  { OVK_NAME,	OV_INTERNAL, TRUE,"hb_min_path_priority_ratio", "",
    0, 0, 0,	&HB_min_path_priority_ratio, NULL,
    "Ratio to control relative size of paths included in hyperblock [Default: .1]"
  },
  { OVK_NAME,	OV_INTERNAL, TRUE,"hb_min_priority", "",
    0, 0, 0,	&HB_min_priority, NULL,
    "Minimum priority allowed for a hyperblock [Default: .1]"
  },
  { OVK_NAME,	OV_INTERNAL, TRUE,"hb_call_hazard_multiplier", "",
    0, 0, 0,	&HB_call_hazard_multiplier, NULL,
    "Factor by which to reduce path priority in presence of calls [Default: .25]"
  },
  { OVK_NAME,	OV_INTERNAL, TRUE,"hb_memory_hazard_multiplier", "",
    0, 0, 0,	&HB_memory_hazard_multiplier, NULL,
    "Factor by which to reduce path priority in presence of unresolvable memory stores [Default: 1.0]"
  },
  { OVK_NAME,	OV_INTERNAL, TRUE,"hb_base_probability_contribution", "",
    0, 0, 0,	&HB_base_probability_contribution, NULL,
    "Factor to ensure base contribution of path probability to priority [Default: 0.1]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_require_alias", "",
    0,0,0,      &HB_require_alias, NULL,
    "Turn on/off requirement that alias information be present for complex hyperblock formation [Default ON]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_complex_non_loop", "",
    0,0,0,      &HB_complex_non_loop, NULL,
    "Turn on/off complex hyperblock formation for non-loop regions [Default ON]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_simple_ifc", "",
    0,0,0,      &HB_simple_ifc, &HB_simple_ifc_set,
    "Turn on/off simple, always profitable hyperblock formation for non-loop regions [Default ON]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_general_use_pq", "",
    0,0,0,      &HB_general_use_pq, NULL,
    "Turn on/off using priority queue when following side paths in general region id for hyperblocks [Default OFF]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_general_from_top", "",
    0,0,0,      &HB_general_from_top, NULL,
    "Turn on/off following side paths from top of main path in general region id for hyperblocks [Default OFF]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_exclude_calls", "",
    0,0,0,      &HB_exclude_calls, NULL,
    "Disallow blocks with calls during hyperblock formation, temporary workaround before full support for predicate callee-register spilling is included [Default ON]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_exclude_pgtns", "",
    0,0,0,      &HB_exclude_pgtns, NULL,
    "Disallow forming hyperblocks if it consists of any global predicate TNs (PGTNS) [Default ON]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "hb_skip_hammocks", "",
    0,0,0,      &HB_skip_hammocks, NULL,
    "Skip forming hyperblocks on hammocks, cause later pass will do them [Default ON]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE, "loop_force_ifc", "",
    0, 0, 2,    &CG_LOOP_force_ifc, NULL },

  // Emit options
  { OVK_INT32,	OV_INTERNAL, TRUE,"longbranch_limit", "",
    DEFAULT_LONG_BRANCH_LIMIT, 0, INT32_MAX, &EMIT_Long_Branch_Limit, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"pjump_all", "pjump_all",
    0, 0, 0, &EMIT_pjump_all, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"use_cold_section", "use_cold_section",
    0, 0, 0, &EMIT_use_cold_section, NULL },
  { OVK_BOOL,   OV_INTERNAL, TRUE,  "emit_asm_dwarf", "",
    0,0,0,      &CG_emit_asm_dwarf, NULL,
    "Turn on/off emission of dwarf data into .s file [Default OFF]"
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,  "emit_unwind_directives", "",
    0,0,0,      &CG_emit_unwind_directives, NULL,
    "Turn on/off emission of unwind directives into .s file [Default OFF]"
  },
  { OVK_BOOL,   OV_INTERNAL, TRUE,  "emit_unwind_info", "",
#ifdef TARG_X8664
    0,0,0,      &CG_emit_unwind_info, &CG_emit_unwind_info_Set,
#else
    0,0,0,      &CG_emit_unwind_info, NULL,
#endif
    "Turn on/off emission of unwind into .s/.o file [Default OFF]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"volatile_asm_stop", "",
    0, 0, 0, &EMIT_stop_bits_for_volatile_asm, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"emit_stop_bits_for_asm", "",
    0, 0, 0, &EMIT_stop_bits_for_asm, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"emit_explicit_bundles", "",
    0, 0, 0, &EMIT_explicit_bundles, NULL },
#if defined( KEY) && !defined(TARG_ST)
  { OVK_BOOL,	OV_INTERNAL, TRUE, "enable_feedback", "",
    0, 0, 0,	&CG_enable_feedback, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"non_gas_syntax", "non_gas",
    0, 0, 0, &CG_emit_non_gas_syntax, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"inhibit_size_directive", "inhibit_size",
    0, 0, 0, &CG_inhibit_size_directive, NULL },
  { OVK_BOOL,   OV_INTERNAL, TRUE,  "prune_unused_debug_types", "",
    0, 0, 0, &CG_prune_unused_debug_types, NULL,
    "Do not produce debugging information for types that are not used in the compilation unit [Default OFF]." },
#endif
#ifdef TARG_X8664
  { OVK_BOOL,	OV_INTERNAL, TRUE,"use_movlpd", "",
    0, 0, 0, &CG_use_movlpd, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"use_setcc", "",
    0, 0, 0, &CG_use_setcc, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"short_form", "",
    0, 0, 0, &CG_use_short_form, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "p2align", "p2align",
    0, 0, 0,	&CG_p2align, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "loop32", "loop32",
    0, 0, 0,	&CG_loop32, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "compute_to", "compute_to",
    0, 0, 0,	&CG_compute_to, NULL },
  { OVK_UINT64,	OV_INTERNAL, TRUE, "p2align_freq", "",
    0, 0, UINT64_MAX>>1, &CG_p2align_freq, NULL, "freq threshold for .p2align" },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"p2align_max_skip_bytes", "",
    3, 0, 64, &CG_p2align_max_skip_bytes, NULL, "max skip bytes for .p2align" },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "use_xortozero", "",
    0, 0, 0,	&CG_use_xortozero, &CG_use_xortozero_Set },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "use_test", "",
    0, 0, 0,	&CG_use_test, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "fold_constimul", "",
    0, 0, 0,	&CG_fold_constimul, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "use_incdec", "",
    0, 0, 0,	&CG_use_incdec, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "fold_shiftadd", "", 
    0, 0, 0,	&CG_fold_shiftadd, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "use_prefetchnta", "", 
    0, 0, 0,	&CG_use_prefetchnta, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "idivbyconst_opt", "", 
    0, 0, 0,	&CG_idivbyconst_opt, NULL },
  { OVK_UINT32,	OV_INTERNAL, TRUE, "movnti", "",
    120, 0, UINT32_MAX>>1, &CG_movnti, NULL,
    "Use x86-64's movnti instead of mov when writing memory blocks of this size or larger (in KB)" },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"use_lddqu", "",
    0, 0, 0, &CG_use_lddqu, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "push_pop_int_saved_regs", "",
    0, 0, 0,	&CG_push_pop_int_saved_regs, &CG_push_pop_int_saved_regs_Set },
  { OVK_UINT32,	OV_INTERNAL, TRUE,"ptr_load_use", "",
    4, 0, 64, &CG_ptr_load_use_latency,  NULL,
    "extra latency between loading a pointer and its use"},
#endif

#ifdef TARG_X8664
  // x87:
  { OVK_BOOL,	OV_INTERNAL, TRUE, "x87_store", "",
    0, 0, 0, &CG_x87_store, NULL,
    "Store x87 floating point variables to memory after each computation, in order to reduce the variable's precision from 80 bits to 32/64 bits.  Default off."
  },
#endif
#ifdef TARG_ST
  { OVK_BOOL,	OV_INTERNAL, TRUE,"emit_space", "",
    0, 0, 0, &EMIT_space, &EMIT_space_overridden,
    "Emit with code size constraint (alignment constraint) "
  },
#endif
#ifdef TARG_STxP70
  { OVK_INT32,   OV_INTERNAL, TRUE,"itstackalign_val", "",
    0, 0, INT32_MAX, &CG_itstackalign_val, &CG_itstackalign_val_overridden,
    "Stack alignment in interruptions in bytes [Default is 8 bytes]"
  },
#endif

  // Misc:
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "gra_live_predicate_aware", "",
    0,0,0,      &GRA_LIVE_Predicate_Aware, NULL,
    "Allow GRA_LIVE to be predicate-aware [Default ON]"
  },
  { OVK_BOOL,	OV_INTERNAL, TRUE,  "pqs_disable", "",
    0,0,0,      &PQS_disabled, NULL,
    "Force PQS to be disabled [Default OFF]"
  },
  { OVK_INT32,	OV_INTERNAL, TRUE,"branch_taken_penalty", "",
    0, 0, INT32_MAX, &CGTARG_branch_taken_penalty,
    &CGTARG_branch_taken_penalty_overridden },
  { OVK_BOOL,   OV_INTERNAL, TRUE, "sched_est_calc_dep_graph", "",
    0, 0, 0,    &CG_SCHED_EST_calc_dep_graph, NULL },
  { OVK_BOOL,   OV_INTERNAL, TRUE, "sched_est_use_locs", "",
    0, 0, 0,    &CG_SCHED_EST_use_locs, NULL },
  { OVK_INT32,   OV_INTERNAL, TRUE, "sched_est_call_cost", "",
    0, 0, INT32_MAX, &CG_SCHED_EST_call_cost, NULL },
#ifndef KEY
  { OVK_BOOL,	OV_INTERNAL, TRUE, "enable_feedback", "",
    0, 0, 0,	&CG_enable_feedback, NULL },
#endif
  { OVK_INT32, OV_INTERNAL, TRUE, "mispredict_branch", "mispredict",
    0, 0, INT32_MAX, &CG_branch_mispredict_penalty, NULL },
  { OVK_INT32, OV_INTERNAL, TRUE, "mispredict_factor", "",
    0, 0, INT32_MAX, &CG_branch_mispredict_factor, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"enable_thr", "",
    0, 0, 0,	&CG_enable_thr, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"reverse_if_conversion", "",
    0, 0, 0,	&CG_enable_reverse_if_conversion,
	       	&CG_enable_reverse_if_conversion_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE,"body_ins_count_max", "",
    0, 0, INT32_MAX, &CG_maxinss, &CG_maxinss_overridden },
  { OVK_INT32,	OV_INTERNAL, TRUE,"body_blocks_count_max", "",
    0, 0, INT32_MAX, &CG_maxblocks, NULL },
#ifndef TARG_ST
  { OVK_BOOL,	OV_INTERNAL, TRUE,"spec_imul_idiv", "",
    0, 0, 0, &CG_enable_spec_imul, 
      &CG_enable_spec_imul_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"spec_idiv", "",
    0, 0, 0, &CG_enable_spec_idiv, 
	     &CG_enable_spec_idiv_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"spec_fdiv", "",
    0, 0, 0, &CG_enable_spec_fdiv, 
	     &CG_enable_spec_fdiv_overridden },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "spec_fsqrt", "",
    0, 0, 0, &CG_enable_spec_fsqrt, 
	     &CG_enable_spec_fsqrt_overridden },
#endif
  { OVK_BOOL,	OV_INTERNAL, TRUE,"cond_defs", "cond_defs",
    0, 0, 0, &CG_cond_defs_allowed, NULL },

  { OVK_BOOL,	OV_INTERNAL, TRUE,"rename", "",
    0, 0, 0, &CG_enable_rename, NULL },

#ifdef TARG_ST
  // Misc TARG_ST part
  { OVK_BOOL,	OV_INTERNAL, TRUE,"GRA_rename", "",
    0, 0, 0, &CG_enable_rename_after_GRA, NULL },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"SSA_min_max_abs", "",
    0, 0, 0, &CG_enable_min_max_abs, &CG_enable_min_max_abs_overriden },    
  { OVK_INT32,  OV_INTERNAL,	TRUE,	"bblength",		"bb",
    CG_bblength_default, CG_bblength_min, CG_bblength_max, &CG_split_BB_length, &CG_split_BB_length_overridden,
    "Restrict BB length by splitting longer BBs" },
  { OVK_BOOL,	OV_INTERNAL, TRUE,"callee_reg_mask", "callee_reg_mask",
    0, 0, 0, &CG_gen_callee_saved_regs_mask, &CG_gen_callee_saved_regs_mask_overriden },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "use_push_pop", "",
    0, 0, 0,	&CG_gen_callee_saved_regs_mask, &CG_gen_callee_saved_regs_mask_overriden, "Use push/pop instructions for callee saved registers. If disabled use spill code." },
  { OVK_INT32,	OV_INTERNAL, TRUE,"max_issue_width", "",
    0, 0, INT32_MAX, &CGTARG_max_issue_width, &CGTARG_max_issue_width_overriden },
  { OVK_BOOL,   OV_INTERNAL, TRUE,"safe_memmove", "safe_memmove",
    0, 0, 0, &CG_safe_memmove, NULL },
#endif /* TARG_ST (Misc.) */

  #ifdef TARG_ST
  // LAO TARG_ST part
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_optimization", "",
    0, 0, 65535,	&CG_LAO_activation, &CG_LAO_activation_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_activation", "",
    0, 0, 65535,	&CG_LAO_activation, &CG_LAO_activation_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_regiontype", "",
    1, 0, 2,	&CG_LAO_regiontype, &CG_LAO_regiontype_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_conversion", "",
    0, 0, 65535,	&CG_LAO_conversion, &CG_LAO_conversion_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_coalescing", "",
    0, 0, 65535,	&CG_LAO_coalescing, &CG_LAO_coalescing_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_predication", "",
    0, 0, 127,	&CG_LAO_predication, &CG_LAO_predication_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_scheduling", "",
    2, 0, 3,	&CG_LAO_scheduling, &CG_LAO_scheduling_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_allocation", "",
    2, 0, 3,	&CG_LAO_allocation, &CG_LAO_allocation_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_rcmssolving", "",
    0, 0, 255,	&CG_LAO_rcmssolving, &CG_LAO_rcmssolving_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_preloading", "",
    0, 0, 4,	&CG_LAO_preloading, &CG_LAO_preloading_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_l1missextra", "",
    0, 0, 255,	&CG_LAO_l1missextra, &CG_LAO_l1missextra_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_compensation", "",
    0, 0, 2,	&CG_LAO_compensation, &CG_LAO_compensation_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_speculation", "",
    3, 0, 4,	&CG_LAO_speculation, &CG_LAO_speculation_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_relaxation", "",
    2, 0, 3,	&CG_LAO_relaxation, &CG_LAO_relaxation_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_pipelining", "",
    2, 0, 7,	&CG_LAO_pipelining, &CG_LAO_pipelining_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_logtimeout", "",
    0, 0, 15,	&CG_LAO_logtimeout, &CG_LAO_logtimeout_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_renaming", "",
    2, 0, 3,	&CG_LAO_renaming, &CG_LAO_renaming_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_boosting", "",
    0, 0, 2,	&CG_LAO_boosting, &CG_LAO_boosting_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_aliasing", "",
    1, 0, 3,	&CG_LAO_aliasing, &CG_LAO_aliasing_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_prepadding", "",
    0, 0, 65536,	&CG_LAO_prepadding, &CG_LAO_prepadding_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_postpadding", "",
    0, 0, 65536,	&CG_LAO_postpadding, &CG_LAO_postpadding_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_overrun", "",
    0, 0, 65536,	&CG_LAO_overrun, &CG_LAO_overrun_overridden },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LAO_opslimit", "",
    0, 0, 65536,	&CG_LAO_opslimit, &CG_LAO_opslimit_overridden },
#endif /* TARG_ST (LAO) */

#ifdef BCO_ENABLED
  /* Cache Optimizer TARG_ST part. */
  { OVK_BOOL,	OV_INTERNAL, TRUE, "emit_bb_freqs", "",
    0, 0, 0,	&CG_emit_bb_freqs, NULL, "Emit .profile_info with basic block frequncies (estimated or feedback)" },
  /* Cache Optimizer TARG_ST part. */
  { OVK_BOOL,	OV_INTERNAL, TRUE, "emit_arcs", "",
    0, 0, 0,	&CG_emit_bb_freqs_arcs, NULL, "Emit .profile_info_arc with basic block frequncies and arc frequencies (estimated or feedback)" },
#endif /* BCO_ENABLED */

#ifdef CGG_ENABLED
  /* Code Generator Generator TARG_ST part. */
  { OVK_BOOL,	OV_INTERNAL, TRUE, "cgg_opt", "",
    0, 0, 0,	&CG_enable_cgg, NULL },

  { OVK_INT32,  OV_INTERNAL, TRUE, "cgg_level", "",
    0, 0, 2, &CG_cgg_level, NULL,
    "Specify optimization level for the code selector" },
#endif /* CGG_ENABLED */

#ifdef TARG_ST
  /* SSA related TARG_ST part. */
  { OVK_BOOL,	OV_INTERNAL, TRUE, "ssa_opt", "",
    0, 0, 0,	&CG_enable_ssa, &CG_enable_ssa_overridden },

  { OVK_INT32,  OV_INTERNAL,	TRUE,	"ssa_variables", "",
    1, 0, 0x7, &CG_ssa_variables, NULL,
    "Specify the categories of variables that can be renamed in SSA" },

  { OVK_INT32,  OV_INTERNAL,	TRUE,	"ssa_coalescing", "",
    0xF, 0, 0xF, &CG_ssa_coalescing, NULL,
    "Specify the level of colaescing to perform during the SSA construction or destruction" },

  { OVK_BOOL,  OV_INTERNAL,	TRUE,	"ssa_remat", "",
    0, 0, 0, &CG_ssa_rematerialization, NULL,
    "Enable rematerialization information computation in SSA form" },

  { OVK_BOOL,	OV_INTERNAL, TRUE, "if_convert", "",
    0, 0, 0,	&CG_enable_select, &CG_enable_select_overridden,
    "Enable if conversion using select op"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_subpart", "",
    0, 0, 0,	&CG_ifc_subpart, &CG_ifc_subpart_overridden,
    "Enable if conversion using select op only on then or else part"},

  { OVK_INT32,	OV_INTERNAL, TRUE, "ifc_forced", "",
    0, 0, 3,	&CG_force_select, NULL,
     "Force if conversion using select op"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "range_propagation", "",
    0, 0, 0,	&CG_enable_range_propagation, &CG_enable_range_propagation_overridden,
    "Enable range analysis"},

  { OVK_INT32,  OV_INTERNAL, TRUE, "range_recompute_limit", "",
    2, 1, INT32_MAX, &CG_range_recompute_limit, &CG_range_recompute_limit_overridden,
    "Limit on the number of times a value range is recomputed" },

  { OVK_INT32,  OV_INTERNAL, TRUE, "affirm_opt", "",
    1, 0, 3, &CG_affirm_opt, &CG_affirm_opt_overridden,
    "Generation and optimization of AFFIRM property" },

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_allow_dup", "",
    0, 0, 0,	&CG_ifc_allow_dup, &CG_ifc_allow_dup_overridden,
    "Allow basic blocks duplication for select if conversion"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_promote_mem", "",
    0, 0, 0,	&CG_select_promote_mem, NULL,
    "Promotion of base/offset conditional memory loads"},

  { OVK_INT32,	OV_INTERNAL, TRUE, "ifc_spec_loads", "",
    0, 0, 2,	&CG_select_spec_loads, NULL,
    "Allow load speculation for select if conversion"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_spec_stores", "",
    0, 0, 0,	&CG_select_spec_stores, &CG_select_spec_stores_overridden,
    "Allow store speculation using black holes stack slots"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_merge_stores", "",
    0, 0, 0,	&CG_select_merge_stores, NULL,
    "Allow store merging using select instruction"},

  { OVK_NAME,	OV_INTERNAL, TRUE, "ifc_factor", "",
    0, 0, 0,	&CG_ifc_factor_string, &CG_ifc_factor_overridden,
    "Extra gain in cycles for flattening a branch"},

  //TB: Export variable for cg_select.cxx
  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_freq", "",
    0, 0, 0,	&CG_ifc_freq, &CG_ifc_freq_overriden,
    "Use frequecies and probabilities to determine if if conversion is profitable"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_cycles", "",
    0, 0, 0,	&CG_ifc_cycles, &CG_ifc_cycles_overridden,
    "Use performance heuristic to determine if if conversion is profitable"},
  
  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_space", "",
    0, 0, 0,	&CG_ifc_space, &CG_ifc_space_overridden,
    "Take into account code size for if convertion"},
    
  { OVK_BOOL,	OV_INTERNAL, TRUE, "ifc_logif", "",
    0, 0, 0,	&CG_ifc_logif, NULL,
    "Do not if-convert multiple logif"},

  { OVK_BOOL,	OV_INTERNAL, TRUE, "iv_offset_space", "",
    0, 0, 0,	&CG_IV_offset_space, &CG_IV_offset_space_overridden,
    "Take into account code size for IV offsets optimization"},
    
#endif /* TARG_ST (SSA) */
 

#ifdef TARG_ST
  // [CL] force spill of return address (RA) so that
  // unwinding/backtracing is still possible
  { OVK_BOOL,	OV_INTERNAL, TRUE, "save_return_address", "",
    0, 0, 0,	&CG_save_return_address, NULL },

  { OVK_INT32,	OV_INTERNAL,	TRUE, "SCH_mask", "", 
    0, 0, 0xffffffff,	&CG_sched_mask, NULL,
    "Control Scheduler optimization (mask)" },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "LRA_mask", "", 
    0, 0, 0xffffffff,	&CG_LRA_mask, NULL,
    "Control LRA optimization (mask)" },
  { OVK_INT32,	OV_INTERNAL, TRUE, "sched_extract_earliest", "",
    1, 0, 2,	&CG_sched_extract_earliest, NULL,
    "Schedule EXTRACT ops as early as possible in BB to reduce reg pressure (0:off, 1:on, 2:on + aggressive mode)" },
  // [GS] for the exportation to DFGs and ISE identification.	
  { OVK_INT32,	OV_INTERNAL,	TRUE, "dfg_ise_mask", "", 
    0, 0, INT32_MAX,	&CG_dfg_ise_mask, NULL,
    "Control Exportation to DfgGraphs, dumping to graphml for ISE identification algorithms(mask)" },
  { OVK_INT32,	OV_INTERNAL,	TRUE, "dfg_debug_mask", "", 
    0, 0, INT32_MAX,	&CG_dfg_debug_mask, NULL,
    "Control Exportation to DfgGraphs, dumping to graphml for debug and code analysis(mask)" },

  // [TTh] Control the usage of preferred register list when choosing color for GRA/LRA.

  { OVK_BOOL,   OV_INTERNAL, TRUE,"use_preferred_regs", "",
    0, 0, 0,    &CG_COLOR_use_pref_regs, &CG_COLOR_use_pref_regs_overridden,
    "Turn on/off the usage of the set of preferred registers during register selection in LRA/GRA [Default TRUE]",
  },
  { OVK_INT32,  OV_INTERNAL, TRUE,"preferred_regs_priority", "",
    PREF_REGS_PRIORITY_MEDIUM, PREF_REGS_PRIORITY_LOW, PREF_REGS_PRIORITY_HIGH,
    &CG_COLOR_pref_regs_priority, &CG_COLOR_pref_regs_priority_overridden,
    "Set priority to be used during the register selection between caller/callee saved regs and preferred regs. "
    "A higher value means a higher priority given to registers defined in preferred sets. "
    "Registers are tried in the following order, depending on properties, which are "
    "SC=scratch(caller saved), PREF=preferred, USED=already used in current function: "
    "Low priority (0): SC+PREF > SC > !SC+USED+PREF > !SC+USED > !SC+PREF > ANY, "
    "Medium priority (1): SC+PREF > !SC+USED+PREF > SC > !SC+USED > !SC+PREF > ANY, "
    "High priority (2): SC+PREF > !SC+USED+PREF > !SC+PREF > SC > !SC+USED > ANY, "
    " [Default 1]",
  },

  // GTN Coalescing:
  { OVK_INT32,	OV_INTERNAL, TRUE, "coalesce_opt", "",
    0, 0, 3,	&CG_coalesce, &CG_coalesce_overridden,
    "Turn on/off coalescing passes:"
    " 0=no coalescing,"
    " 1=coalescing before scheduling,"
    " 2=coalescing after scheduling,"
    " 3=coalescing before and after scheduling [Default= 3]" },
  { OVK_BOOL,	OV_INTERNAL, TRUE, "coalesce_pair_only", "",
    0, 0, 0,	&CG_coalesce_pair_only, NULL,
    "Limit GTN coalescing to pair of GTNs, even if more GTNs could be merged together [Default FALSE]" },
  { OVK_INT32,	OV_INTERNAL, TRUE, "coalesce_max_transfo", "",
    0, 0, INT32_MAX, &CG_coalesce_max_transfo, NULL,
    "Define the maximum number of copies to coalesce in current UNIT" },

#endif

#ifdef ICE9A_HW_WORKAROUND
  // Bug 13027.
  { OVK_INT32, OV_INTERNAL, TRUE,"muld", "",
    0, 0, 5, &CG_muld, &CG_muld_overridden,
    "Add NOPs after mul.d to workaround hardware bug.  If 0, don't add any NOP.  If 1, scheduler adds NOPs.  If 2, add one SSNOP after each double multiply when emitting the assembly code, whether or not such SSNOP is needed; the scheduler doesn't add any NOP.  3 is like 2 but adds two SSNOPs.  If 4, insert minimum number of movn.d to guard against fp stall and rounding bugs (currently no scheduler support).  If 5, always insert movn.d to guard against each fp hazard, even if the movn.d is not necessary (fail-safe mode). [Default 4]"
  },
  { OVK_INT32, OV_INTERNAL, TRUE,"hw_round", "",
    0, 0, 2, &CG_hw_round, &CG_hw_round_overridden,
    "Add instructions to workaround fp rounding bug.  If 0, no workaround.  If 1, insert minimum number of movn.d (currently no scheduler support).  If 2, always insert movn.d to guard against possible hazard, even if the movn.d is not necessary (fail-safe mode).  [Default 1]"
  },
  { OVK_INT32, OV_INTERNAL, TRUE,"hw_stall", "",
    0, 0, 3, &CG_hw_stall, &CG_hw_stall_overridden,
    "Add instructions to workaround fp stall bug.  If 0, no workaround.  If 1, insert minimum number of movn.d (currently no scheduler support).  If 2, always insert movn.d to guard against possible hazard, even if the movn.d is not necessary (fail-safe version of 1).  If 3, behave as 2 but insert 5 movn.d instructions (most fail-safe).  [Default 1]"
  },
#endif

  { OVK_COUNT }
};


OPTION_GROUP Cg_Option_Groups[] = {
  { "SWP", ':', '=', Options_CG_SWP },
  { "CG", ':', '=', Options_CG },
  { "GRA", ':', '=', Options_GRA },
  { NULL }		/* List terminator -- must be last */
};



extern INT prefetch_ahead;
INT _prefetch_ahead = 2;
#if defined(BUILD_OS_DARWIN)
/* Apparently not referenced elsewhere; Mach-O can't do aliases */
#define prefetch_ahead (_prefetch_ahead)
#else /* defined(BUILD_OS_DARWIN) */
#pragma weak prefetch_ahead = _prefetch_ahead
#endif /* defined(BUILD_OS_DARWIN) */

/* =======================================================================
 *
 *  Configure_Prefetch
 *
 *  Configure the prefetch flags controlled by prefetch_ahead exported
 *  from LNO. It MUST be called after lno.so has been loaded.
 *
 * =======================================================================
 */
static void
Configure_prefetch_ahead(void)
{
  static INT32 save_L1_pf_latency = -1;
  static INT32 save_L2_pf_latency = -1;
  if ( save_L1_pf_latency < 0 ) {
    save_L1_pf_latency = CG_L1_pf_latency;
    save_L2_pf_latency = CG_L2_pf_latency;
  }
#ifdef TARG_ST
  if (PROC_has_enable_prefetch_ahead()) {
#else
  if (Enable_Prefetch_Ahead_For_Target()) {
#endif
    if ( ! CG_L2_pf_latency_overridden )
      if ( prefetch_ahead ) 
	CG_L2_pf_latency = 0;
      else
	CG_L2_pf_latency = save_L2_pf_latency;
    if ( ! CG_L1_pf_latency_overridden )
      if (prefetch_ahead)
	CG_L1_pf_latency = 0;
      else
	CG_L1_pf_latency = save_L1_pf_latency;
  }
}


/* =======================================================================
 *
 *  Configure_Prefetch
 *
 *  Configure the prefetch flags.
 *
 * =======================================================================
 */
static void
Configure_Prefetch(void)
{
  if ( ! OPT_shared_memory) {
	CG_exclusive_prefetch = TRUE;
  }
  /* Detect any of the various cases that cause us to disable 
   * prefetching entirely:
   *   isa < mips4
   *   -CG:prefetch=off
   *   -CG:z_conf_prefetch=off:nz_conf_prefetch=off
   */ 
#ifdef TARG_ST
  if (!PROC_has_prefetch()
#else 
  if (   ! Target_Has_Prefetch()
#endif
      || (CG_enable_prefetch_overridden && ! CG_enable_prefetch)
      || (   CG_enable_z_conf_prefetch_overridden 
	  && ! CG_enable_z_conf_prefetch
          && CG_enable_nz_conf_prefetch_overridden 
	  && ! CG_enable_nz_conf_prefetch)
  ) {
disable_prefetch:
    CG_enable_prefetch = FALSE;
    CG_enable_z_conf_prefetch  = FALSE;
    CG_enable_nz_conf_prefetch = FALSE;
    CG_enable_pf_L1_ld = FALSE;
    CG_enable_pf_L1_st = FALSE;
    CG_enable_pf_L2_ld = FALSE;
    CG_enable_pf_L2_st = FALSE;
    return;
  }

  /* At this point, -CG:prefetch was explicitly set to true, or
   * unspecified.
   */
  if ( ! CG_enable_prefetch_overridden ) {
    CG_enable_prefetch = FALSE;

    /* -CG:z_conf_prefetch or -CG:nz_conf_prefetch implicitly
     * set to TRUE, implies we should enable prefetching.
     */
    if (   (   CG_enable_z_conf_prefetch_overridden 
	    && CG_enable_z_conf_prefetch)
        || (   CG_enable_nz_conf_prefetch_overridden 
	    && CG_enable_nz_conf_prefetch)
    ) {
      CG_enable_prefetch = TRUE;
    }

    /* Some targets implicitly enable prefetching.
     */
#ifdef TARG_ST
    else if (PROC_has_enable_prefetch()) {
#else
    else if (Enable_Prefetch_For_Target()) {
#endif
      CG_enable_prefetch = TRUE;
    }

    /* No implicit enable of prefetching this time...
     */
    else goto disable_prefetch;
  }

  /* Prefetching is enabled, implicitly or explicitly. Handle any
   * defaults, both target independent and target specific.
   */
  if ( ! CG_enable_z_conf_prefetch_overridden )
    CG_enable_z_conf_prefetch = FALSE;
  if ( ! CG_enable_nz_conf_prefetch_overridden )
    CG_enable_nz_conf_prefetch = TRUE;
#ifndef TARG_ST
  if (Enable_Prefetch_For_Target()) {
    if ( ! CG_L1_ld_latency_overridden ) CG_L1_ld_latency = 8;
#ifdef KEY
    if ( ! CG_enable_pf_L1_ld_overridden ) CG_enable_pf_L1_ld = TRUE;
    if ( ! CG_enable_pf_L1_st_overridden ) CG_enable_pf_L1_st = TRUE;
#else
    if ( ! CG_enable_pf_L1_ld_overridden ) CG_enable_pf_L1_ld = FALSE;
    if ( ! CG_enable_pf_L1_st_overridden ) CG_enable_pf_L1_st = FALSE;
#endif
    if ( ! CG_enable_pf_L2_ld_overridden ) CG_enable_pf_L2_ld = TRUE;
    if ( ! CG_enable_pf_L2_st_overridden ) CG_enable_pf_L2_st = TRUE;
  } else {
    if ( ! CG_enable_pf_L1_ld_overridden ) CG_enable_pf_L1_ld = TRUE;
    if ( ! CG_enable_pf_L1_st_overridden ) CG_enable_pf_L1_st = TRUE;
    if ( ! CG_enable_pf_L2_ld_overridden ) CG_enable_pf_L2_ld = TRUE;
    if ( ! CG_enable_pf_L2_st_overridden ) CG_enable_pf_L2_st = TRUE;
  }
#endif
  /* Finally, check to see if we actually will do any prefetching, and
   * if not, disable prefetching all together.
   */
  if (   ! CG_enable_pf_L1_ld
      && ! CG_enable_pf_L1_st
      && ! CG_enable_pf_L2_ld
      && ! CG_enable_pf_L2_st ) goto disable_prefetch;
}


/* =======================================================================
 *
 *  Configure_CG_Options
 *
 *  After the comand line has been processed and CG_opt_level set, configure
 *  the various CG flags that depend on these two things.
 *  This is also called per PU if the PU opt level changes.
 *
 * =======================================================================
 */
static void
Configure_CG_Options(void)
{
  /* Set code generation options -- see	cg.h: */

  if ( ! CG_localize_tns_Set)
  	CG_localize_tns = (CG_opt_level <= 1);

#ifdef TARG_ST
  if (!Enable_SWP_overridden)
  {
    Enable_SWP = FALSE;
  }
  else {
    Enable_SWP = CG_opt_level >= 2;

    if (!Enable_SWP)
      DevWarn("Configure_CG_Options: SWP=ON but Opt_Level < 2; ignoring SWP");

  }
#else
  if ( ! Enable_SWP_overridden )
  {
    // Enable_SWP = (CG_opt_level > 2) && ! OPT_Space;
#ifdef TARG_IA64
    Enable_SWP = CG_opt_level >= 2;
#elif defined(TARG_MIPS)
    Enable_SWP = (CG_opt_level > 2) && ! OPT_Space;
#else
    Enable_SWP = FALSE;
#endif
  }
#endif

#ifdef TARG_ST
#ifdef LAO_ENABLED
  // Activate Multi BB unroll only with cyclic scheduling
  if ((Opt_Level >= 2) && !CG_LOOP_unroll_multi_bb_overridden) {
    CG_LOOP_unroll_multi_bb = TRUE;
  }
#endif

  if (Opt_Level >= 2) {

      if (!IPFEC_Enable_LICM_overridden) {
        IPFEC_Enable_LICM = 2;
      }
      if (!IPFEC_Enable_LICM_passes_overridden) {
        IPFEC_Enable_LICM_passes = 0x07;
      }
    }

#ifdef TARG_STxP70
  if ((Opt_Level >= 2) && !CG_AutoMod_overridden) {
    CG_AutoMod = TRUE;
  }

  if (!CG_AutoMod_RelaxPdom_overridden) {
    CG_AutoMod_RelaxPdom = FALSE;
  }
#else
  CG_AutoMod = FALSE;
#endif

#else
  if (CG_opt_level > 2 && !OPT_unroll_size_overridden )
#ifdef TARG_MIPS
     OPT_unroll_size = 256;
#else
     OPT_unroll_size = 128;
#endif
#endif
#ifdef TARG_STxP70
#ifndef TARG_ST
  //TB: no more OPT_Space
  if(CG_opt_level >= 2 && !CG_cbpo_optimize_load_imm_overridden) {
      CG_cbpo_optimize_load_imm = DEFAULT_LOAD_IMM(CG_opt_level, OPT_Space);
  }
  if(!CG_cbpo_block_method_overridden) {
      CG_cbpo_block_method = DEFAULT_BLOCK_METHOD(CG_opt_level, OPT_Space);
  }
  if(!CG_cbpo_ratio_overridden) {
      CG_cbpo_ratio = DEFAULT_RATIO(CG_opt_level, OPT_Space);
  }
  if(!CG_cbpo_facto_cst_overridden) {
      CG_cbpo_facto_cst = DEFAULT_FACTO_CST(CG_opt_level, OPT_Space);
  }
  if(CG_opt_level >= 2 && !CG_tailmerge_overridden) {
      CG_tailmerge = DEFAULT_TAILMERGE(CG_opt_level, OPT_Space);
  }
  if(CG_opt_level >= 2 && !CG_simp_flow_in_tailmerge_overridden) {
      CG_simp_flow_in_tailmerge = DEFAULT_SIMP_FLOW_TAILMERGE(CG_opt_level, OPT_Space);
  }
#endif
#endif

  if ( OPT_Unroll_Analysis_Set )
  {
    CG_LOOP_unroll_analysis = OPT_Unroll_Analysis;
  }
  CG_LOOP_unroll_times_max = OPT_unroll_times;
  CG_LOOP_unrolled_size_max = OPT_unroll_size;
  CG_LOOP_unroll_level = OPT_unroll_level;

  CG_LOOP_ooo_unroll_heuristics = PROC_is_out_of_order();


  if (OPT_Space)
  {
    CGEXP_expandconstant = 2;
  }

  if (!Integer_Divide_By_Constant_overridden
#ifdef KEY
      && CGEXP_cvrt_int_div_to_mult
#endif
	  ) {
    CGEXP_cvrt_int_div_to_mult = (!OPT_Space) && (CG_opt_level > 0);
  } 
#ifndef TARG_ST
  if (!Integer_Divide_Use_Float_overridden
#ifdef KEY
      && CGEXP_cvrt_int_div_to_fdiv
#endif
	  ) {
    CGEXP_cvrt_int_div_to_fdiv =    !Kernel_Code
				 && Enable_Idiv_In_FPU_For_Target()
				 && !OPT_Space
				 && CG_opt_level > 0;
  }
#endif
#ifdef KEY
  if (!Integer_Multiply_By_Constant_overridden &&
      CGEXP_cvrt_int_mult_to_add_shift) {
    CGEXP_cvrt_int_mult_to_add_shift = (!OPT_Space) && (CG_opt_level > 0);
  }
#endif

#ifdef TARG_ST
  if (!Kernel_Code && !CG_tail_call_overridden && CG_opt_level >= 2) CG_tail_call = TRUE;
#endif
  if (Kernel_Code && !CG_tail_call_overridden) CG_tail_call = FALSE;

#ifdef TARG_ST
  // [CL] disable tail call optim if safe unwinding is requested
  if (CG_save_return_address) CG_tail_call = FALSE;
#endif

  if (Kernel_Code && !GCM_Speculative_Ptr_Deref_Set)
    GCM_Eager_Ptr_Deref = FALSE;

  if (!CGTARG_Branch_Taken_Prob_overridden)
    CGTARG_Branch_Taken_Prob = "0.95";
  CGTARG_Branch_Taken_Probability = atof(CGTARG_Branch_Taken_Prob);
#ifndef TARG_ST 
  if ( !CG_enable_spec_idiv_overridden && Enable_Spec_Idiv_For_Target() )
    CG_enable_spec_idiv = FALSE;
#endif
  if ( ! CG_LOOP_fix_recurrences_specified
       && (      CG_LOOP_back_substitution
              && CG_LOOP_back_substitution_specified
           ||    CG_LOOP_interleave_reductions
              && CG_LOOP_interleave_reductions_specified
           ||    CG_LOOP_interleave_posti
	      && CG_LOOP_interleave_posti_specified
	   ||    CG_LOOP_nested_interleave
	      && CG_LOOP_nested_interleave_specified
           ||    CG_LOOP_reassociate 
              && CG_LOOP_reassociate_specified)) {
    CG_LOOP_fix_recurrences = TRUE;
  }

  if ( Enable_SWP && ! Enable_LOH_overridden )
#ifdef TARG_ST
    Enable_LOH = FALSE; // don't know what it is exactly and how to
                        // parametrize the proc_properties.cxx 
#else
    Enable_LOH = Enable_LOH_For_Target();
#endif

#ifdef TARG_ST
    if (!EBO_Opt_Level_overridden) {
      EBO_Opt_Level = (CG_opt_level > 1) ? EBO_Opt_Level_Default : 0;
    }
#else 
  if (!EBO_Opt_Level_overridden) {
    EBO_Opt_Level = (CG_opt_level > 0) ? EBO_Opt_Level_Default : 0;
  }
#endif
  #ifdef TARG_ST
  if (!Enable_CG_Peephole_overridden) {
    Enable_CG_Peephole = (CG_opt_level > 1 ) ? TRUE : FALSE;
  }

  // [CG]: turned on ssa
  // If user explicitely set SSA.
  // However, don't allow if opt_level < 1
  if (!CG_enable_ssa_overridden) {
    CG_enable_ssa = (CG_opt_level > 1) ? TRUE : FALSE;
  }
  
  if (CG_enable_ssa && (CG_opt_level < 2)) {
    DevWarn("CG: Ignoring ssa=ON, need optimization level -O2 or higher");
    CG_enable_ssa = FALSE;
  }

  if (CG_enable_ssa) {
    if (CG_ssa_variables & SSA_VAR_DEDICATED_GLOBAL) {
      if (!(CG_ssa_variables & SSA_VAR_NON_DEDICATED)) { 
	DevWarn("CG: SSA on dedicated global variables needs SSA on local dedicated variables. Added.");
	CG_ssa_variables |= SSA_VAR_DEDICATED_LOCAL;
      }
    }
    if (CG_ssa_variables & SSA_VAR_DEDICATED_LOCAL) {
      if (!(CG_ssa_variables & SSA_VAR_NON_DEDICATED)) {
	DevWarn("CG: SSA on dedicated local variables needs SSA on non dedicated variables. Added.");
	CG_ssa_variables |= SSA_VAR_NON_DEDICATED;
      }
    }
  }

  // still off by default
  // [CG] turned on select
  if (!CG_enable_select_overridden) {
    CG_enable_select =  (CG_opt_level > 1) ? CGTARG_Can_Predicate() || CGTARG_Can_Select() : FALSE;
  }

  if (CG_enable_select && !CG_enable_ssa) {
    DevWarn("CG: Ignoring select=ON, need ssa");
    CG_enable_select = FALSE;
  }

  // [SC] range analysis
  if (!CG_enable_range_propagation_overridden) {
    CG_enable_range_propagation =  (CG_opt_level > 1) ? TRUE : FALSE;
  }

  if (CG_enable_range_propagation && !CG_enable_ssa) {
    DevWarn("CG: Ignoring range_propagation=ON, need ssa");
    CG_enable_range_propagation = FALSE;
  }

  // [FdF] AFFIRM property generation and optimization
  if (!CG_affirm_opt_overridden) {
    CG_affirm_opt = (CG_opt_level > 1) ? 1 : 0;
  }

  // Enable CBPO at > -O1
  if (!CG_enable_cbpo_overriden) {
    CG_enable_cbpo =  (CG_opt_level > 1) ? TRUE : FALSE;
  }

  if (CG_enable_cbpo && !CG_enable_ssa) {
    DevWarn("CG: Ignoring enable_cbpo=ON, need ssa");
    CG_enable_cbpo = FALSE;
  }

  /* If not overidden, set LAO_activation according to Opt_Level. */

#ifdef LAO_ENABLED
  if (!CG_LAO_activation_overridden && (Opt_Level > 2))
    CG_LAO_activation = OptimizeActivation_PrePass
                      + OptimizeActivation_PostPass;

  // [FdF]: Ignore LAO options if opt_level < 2
  
  if ((CG_LAO_activation != 0) && (CG_opt_level < 2)) {
    DevWarn("CG: Ignoring LAO options, need optimization level -O2 or higher");
    CG_LAO_activation = 0;
  }
#endif

#endif  /* TARG_ST */
   Enable_CG_Peephole = (CG_opt_level > 0) ? TRUE : FALSE;

  /* Enable_Fill_Delay_Slots controls the filling of delay slots in locs
     and gcm */
#ifdef TARG_ST
  if (!PROC_has_branch_delay_slot() || !Enable_Fill_Delay_Slots) 
#else
  if (!Enable_Fill_Delay_Slots_For_Target() || !Enable_Fill_Delay_Slots) 
#endif
    GCM_Enable_Fill_Delay_Slots = FALSE;

  /* Clamp body_ins_count_max to max BB length
   */
  if (CG_maxinss_overridden) {
    if (CG_maxinss > Split_BB_Length) {
      Split_BB_Length = CG_maxinss;
    }
  } else {
    CG_maxinss = CG_maxinss_default * CG_opt_level;
    if (CG_maxinss == 0 || CG_maxinss > Split_BB_Length) {
      CG_maxinss = Split_BB_Length;
    }
  }
#ifdef TARG_ST
  // ST200 BB length default: 
  // - default up to O1 (300)
  // - 500 at O2
  // - 3000 at > O2
  // (Determined by RFI 1-1-0-B/p9)
  if (!CG_split_BB_length_overridden) {
    if (Opt_Level == 2) CG_split_BB_length = 500;
    else if (Opt_Level > 2) CG_split_BB_length = 3000;
  }
#endif
  
#ifdef TARG_ST200
  // Force the replacement of ;; NOP ;; NOP ;; bundles by ;; GOTO .+4
  // ;; on the st220 target.
  if (Is_Target_st220() && !CG_NOPs_to_GOTO_overridden && (CG_opt_level > 1)) {
    CG_NOPs_to_GOTO = TRUE;
  }
#endif

#ifdef TARG_ST200
  if (Enable_64_Bits_Ops && Opt_Level >= 2) {
    if (!CG_gen_callee_saved_regs_mask_overriden)
      CG_gen_callee_saved_regs_mask = TRUE;
  }
#endif

#ifdef TARG_ST200
  // CL: Favor branches condition to give more opportunities to LAO
  // boosting
  if (!CFLOW_Enable_Favor_Branches_Condition_overridden) {
    CFLOW_Enable_Favor_Branches_Condition = TRUE;
  }
#endif

#ifdef TARG_STxP70
  if ((CG_opt_level >= 1) && !CFLOW_enable_last_pass_overridden) {
    CFLOW_enable_last_pass = TRUE;
  }

  /* always push pop unless user override the value. */
  if (!CG_gen_callee_saved_regs_mask_overriden)
      CG_gen_callee_saved_regs_mask = TRUE;

  /* Pre-GRA coalescing is disabled by default on xp70 */
  if (!CG_coalesce_overridden) {
    CG_coalesce = 0;
  }
#endif


  /* Set BB clone limits
   */
  if ( Kernel_Code && ! CFLOW_Enable_Clone_overridden ) {
    // if kernel code then want really minimal space,
    // so turn off cloning altogether
    CFLOW_Enable_Clone = FALSE;
  } 
#ifndef TARG_ST
  else if (OPT_Space) {
 #ifdef TARG_STxP70
    if (!CFLOW_Enable_Clone_overridden) CFLOW_Enable_Clone = FALSE;
#endif
    if (!clone_incr_overridden) CFLOW_clone_incr = 1;
    if (!clone_min_incr_overridden) CFLOW_clone_min_incr = 1;
    if (!clone_max_incr_overridden) CFLOW_clone_max_incr = 3;
  } else {
      //TB:Set default value
      if (!clone_incr_overridden) CFLOW_clone_incr = 10;
      if (!clone_min_incr_overridden) CFLOW_clone_min_incr = 15;
      if (!clone_max_incr_overridden) CFLOW_clone_max_incr = 100;
  #ifdef TARG_STxP70
     if (!CFLOW_Enable_Clone_overridden) CFLOW_Enable_Clone = TRUE;
  #endif
    }
#endif

#ifdef TARG_ST
  /* The maximum number of resources matched the max number of slots
     on the processor */
  int i,nb_slots= -1;
  SI_RESOURCE * const *resources = Get_SI_resources();
  for (i=0; i< Get_SI_resource_count(); i++) {
    if ((int)(resources[i]->avail_per_cycle)  > nb_slots) {
      nb_slots = (int)resources[i]->avail_per_cycle;
    }
  }
  /* Isa_Max_Slots is defined in two simultaneous instances of the same
  targinfo library under windows and thus the necessity to double its
  initialization */
  set_Max_Slots(nb_slots);
  TI_BUNDLE_initialize(nb_slots);

  // [CG] Initialize default max_issue width
  if (!CGTARG_max_issue_width_overriden) {
    CGTARG_max_issue_width = ISA_MAX_SLOTS;
  }
  if (CGTARG_max_issue_width > ISA_MAX_SLOTS) {
    FmtAssert(0, ("CG: Max issue width %d invalid, must be <= %d", 
		  CGTARG_max_issue_width, ISA_MAX_SLOTS));
  }
  if (CGTARG_max_issue_width < 1) {
    FmtAssert(0, ("CG: Max issue width %d invalid, must be >= %d", 
		  CGTARG_max_issue_width, 1));
  }
#endif

#ifdef TARG_ST
#ifndef TARG_STxP70
  // Default to bacward schedule unless for STxP70.
  if (!LOCS_POST_Scheduling_overriden) {
    LOCS_POST_Scheduling = 1; // backward schedule
  }
#endif
#endif

  Configure_Prefetch();
#ifdef TARG_X8664
  if ((Target == TARGET_em64t ||
       Target == TARGET_core ||
       Target == TARGET_wolfdale) &&
      ! CG_use_xortozero_Set) {
    CG_use_xortozero = TRUE;
  }

  if (OPT_Space && !CG_use_xortozero_Set)	// Bug 9717
    CG_use_xortozero = TRUE;

  if (Target == TARGET_barcelona && ! CG_push_pop_int_saved_regs_Set)
    CG_push_pop_int_saved_regs = TRUE;
#endif

#ifdef TARG_MIPS
  if (CG_opt_level > 2 && !LOCS_Best_set)
    LOCS_Best = TRUE;
#endif

#ifdef TARG_ST
  //TDR - Initialize CG_enable_min_max_abs according to architecture
  if(!CG_enable_min_max_abs_overriden)  
	  CG_enable_min_max_abs = (!PROC_is_select() && CGTARG_Has_min_max_abs());
#endif
  

}

/* =======================================================================
 *
 *  CG_Configure_Opt_Level
 *
 *  See interface description.
 *
 * =======================================================================
 */
void
CG_Configure_Opt_Level( INT opt_level )
{
  static BOOL opt_level_configured = FALSE;

  if ( opt_level_configured && opt_level == CG_opt_level )
    return;

  if ( opt_level_configured && cg_opt_level_overridden ) {
    /* forget it */
    DevWarn("Attempt to override CG:opt_level=%d flag. Ignored.",CG_opt_level);
    return;
  }

  opt_level_configured = TRUE;

  if ( ! cg_opt_level_overridden )
    CG_opt_level = opt_level;

  Configure_CG_Options();
}


/* ====================================================================
 *
 * Build_Option_String
 *
 * Just	build a	string of all the options passed to the	Common Core
 * process, so the options can be printed at the beginning of the "*.s"
 * file(s).
 *
 * ====================================================================
 */

static void
Build_Option_String (INT argc, char **argv)
{
    INT16 i;
    INT16 arg_size = 0;

    Argv0 = argv[0];		    /* save argv[0] for R_Assemble_File() */

    for (i=1; i<argc; ++i)	    /* skip arg 0 (the program name) */
	if ( argv[i][0] == '-'  && argv[i][1] != 'f')
	    arg_size += ( strlen(argv[i]) + 1 );

    if ( arg_size > 0 ) {
	register char *p;
	
	p = option_string = (char *) malloc(arg_size+1);

	if ( option_string == NULL ) {
	    ErrMsg ( EC_No_Mem, "Build_Option_String" );
	    exit ( 1 );
	}

	p[0] = '\0';
	for (i=1; i<argc; ++i)
	    if ( argv[i][0] == '-'  && argv[i][1] != 'f') {
		register INT len = strlen (argv[i]) + 1;
		if (p != option_string)
		    *p++ = ' ';
		bcopy (argv[i], p, len);
		p += len - 1;
	    }
	
    } else {			    /* no options specified */
	option_string = const_cast<char*>("none");
    }
} /* end: Build_Option_String */

/* ====================================================================
 *
 * Process_Command_Line
 *
 * Process the command line arguments specific to CG.
 *
 * ====================================================================
 */

static void
Process_Command_Line (INT argc, char **argv)
{
    INT16 i;
    char *cp;

    /* Check the command line flags: */
    for ( i=0; i<argc; i++ ) {
	if ( argv[i] != NULL && *(argv[i]) == '-' ) {
	    cp = argv[i]+1;	    /* Pointer to next flag character */

	    /* First try to process as command-line option group */
	    if (Process_Command_Line_Group(cp, Cg_Option_Groups))
		continue;

	    switch ( *cp++ ) {

	    case 'f':		    /* file options */
		/* error case already handled by main driver */
		switch (*cp) {
		case 'a':	    /* Assembly file */
		case 's':
		    Assembly = TRUE;
		    Asm_File_Name = cp + 2;
		    break;
		case 'm':           /* MiniR file */
		    MiniR_Code = TRUE;
		    MiniR_File_Name = cp + 2;
		    break;

		case 'o':	    /* object file */
		    Object_Code = TRUE;
		    Obj_File_Name = cp + 2;
		    break;

		}
		break;

	    case 's':		    /* -s: Produce assembly file: */
	    case 'S':		    /* -S: Produce assembly file: */
                Assembly = TRUE;
                break;

	    case 't':
                /* handle the -tfprev10 option to fix tfp hardware bugs. */
                if ( strncmp ( cp-1, "tfprev10", 8 ) == 0 ) {
		    No_Quad_Aligned_Branch = TRUE;
                }

                break;

	    }
	}
    }
    FmtAssert(!(MiniR_Code && (Object_Code || Assembly)), ("Object_Code & Assembly incorrect if MiniR enabled"));
}

/* ====================================================================
 *
 * Prepare_Source
 *
 * Process the source argument and associated files.
 *
 * ====================================================================
 */

static void
Prepare_Source (void)
{
    char *fname;

    /* We've got a source file name -- open other files.
     * We want them to be created in the current directory, so we
     * strip off the filename only from Src_File_Name for use:
     */
    fname = Last_Pathname_Component ( Src_File_Name );

    /* If we're producing information for CITE, we need an assembly
     * file even if it wasn't explicitly requested:
     */
    if ( List_Cite ) {
      Assembly = TRUE;
    }

    if ( Assembly ) {
	if ( Asm_File_Name == NULL ) {
	    /* Replace source file extension to get assembly file name: */
	    Asm_File_Name = New_Extension (fname, ASM_FILE_EXTENSION );
	}
	/* Open	the ASM	file for compilation: */
	if ( ( Asm_File	= fopen	( Asm_File_Name, "w" ) ) == NULL ) {
	    ErrMsg ( EC_Asm_Open, Asm_File_Name, errno );
	    Terminate (1);
	}
    }
    if ( MiniR_Code ) {
	if ( MiniR_File_Name == NULL ) {
	    MiniR_File_Name = New_Extension (fname, MINIR_FILE_EXTENSION );
	}
	if ( ( MiniR_File = fopen ( MiniR_File_Name, "w" ) ) == NULL ) {
	    ErrMsg ( EC_Asm_Open, MiniR_File_Name, errno );
	    Terminate (1);
	}
    }

    /* Prepare relocatable object file name: */
    if ( Obj_File_Name == NULL ) {
#ifdef KEY
	/* bug 2025
	   Always create the object file in /tmp, since the current dir might
	   not be writable.
	 */
	char* tmp_fname = tempnam( NULL, NULL );
	Obj_File_Name = New_Extension( tmp_fname, OBJ_FILE_EXTENSION );
#else
	/* Replace source file extension to get	object file: */
	Obj_File_Name =	New_Extension (fname, OBJ_FILE_EXTENSION);
#endif
    }

#if 0
    /* already called by main */
    /* Configure internal options for this source file */
    Configure_Source ( NULL );
#endif
}

static void
Increment_Register_Name (char **name)
{
	INT i = atoi(*name);
	++i;
	sprintf(*name, "%d", i);
}

static void
Set_Register_Range_Not_Allocatable (char *regname1, char *regname2)
{
  char regname[8];
  char *p;	// points to first digit in regname 
  INT count = 0;
  strcpy(regname,regname1);
  // find where digits start
  for (p = regname; *p && !isdigit(*p); ++p) ;
  FmtAssert( strncmp(regname1, regname2, p - regname) == 0,
	("register range %s-%s doesn't have matching prefixes", 
	regname1, regname2));

  // create each regname in range
  while (strcmp(regname, regname2) != 0) {
	Set_Register_Never_Allocatable (regname);
	Increment_Register_Name (&p);
	++count; if (count > 200) break;	// avoid infinite loop
  }
  Set_Register_Never_Allocatable (regname);
}

struct Set_DREG_Not_Allocatable 
{
    inline void operator() (UINT32, ST_ATTR *st_attr) const {
	if (ST_ATTR_kind (*st_attr) != ST_ATTR_DEDICATED_REGISTER)
	    return;
	PREG_NUM p = ST_ATTR_reg_id(*st_attr);
	Set_Register_Never_Allocatable(p);
    }
};

// some variables can be pre-allocated to registers,
// in which case the symtab will be marked,
// or the command-line may list registers not to be used.
#ifdef TARG_ST
void
Mark_Specified_Registers_As_Not_Allocatable ()
{
  OPTION_LIST *ol = Registers_Not_Allocatable;
  char *start;
  char *p;
  char regname[16];
  char regname2[16];

  // go through global dreg list
  if (ST_ATTR_Table_Size (GLOBAL_SYMTAB)) {
    For_all (St_Attr_Table, GLOBAL_SYMTAB, Set_DREG_Not_Allocatable());
  }

  // now go through command-line list
  if (ol == NULL) return;
  for ( ; ol != NULL; ol = OLIST_next(ol)) {

    /* Check for commas and ranges: */
    p = OLIST_val(ol);
    start = p;
    while ( *p != ':' && *p != 0 ) {
      if ( *p == ',') {
	strncpy (regname, start, p-start+1);
	regname[p-start] = '\0';
	Set_Register_Never_Allocatable (regname);
	++p;
	start = p;
      }
      else if (*p == '-' ) {
	strncpy (regname, start, p-start+1);
	regname[p-start] = '\0';
	++p;
	start = p;
	while (*p != ',' && *p != '\0') {
	  ++p;
	}
	strncpy (regname2, start, p-start+1);
	regname2[p-start] = '\0';
	Set_Register_Range_Not_Allocatable (regname, regname2);
	if (*p == 0) {
#ifdef TARG_ST
	  // [TTh] We must continue on next olist element, if exist
	  start = 0;
	  break;
#else
	  return;
#endif
	}
	++p;
	start = p;
      }
      else {
	++p;
      }
    }
#ifdef TARG_ST
    if (start != 0) {
#endif
    strncpy (regname, start, p-start+1);
    Set_Register_Never_Allocatable (regname);
#ifdef TARG_ST
    }
#endif
  }
}
#else
void
Mark_Specified_Registers_As_Not_Allocatable (void)
{
  OPTION_LIST *ol = Registers_Not_Allocatable;
  char *start;
  char *p;
  char regname[8];
  char regname2[8];

  // go through global dreg list
  if ( ST_ATTR_Table_Size (GLOBAL_SYMTAB)) {
    For_all (St_Attr_Table, GLOBAL_SYMTAB, 
	Set_DREG_Not_Allocatable());
  }

  // now go through command-line list
  if ( ol == NULL ) return;
  for ( ; ol != NULL; ol = OLIST_next(ol) ) {

    /* Check for commas and ranges: */
    p = OLIST_val(ol);
    start = p;
    while ( *p != ':' && *p != 0 ) {
	if ( *p == ',') {
		strncpy (regname, start, p-start+1);
		regname[p-start] = '\0';
    		Set_Register_Never_Allocatable (regname);
		++p;
		start = p;
	}
 	else if (*p == '-' ) {
		strncpy (regname, start, p-start+1);
		regname[p-start] = '\0';
		++p;
		start = p;
		while (*p != ',' && *p != '\0') {
			++p;
		}
		strncpy (regname2, start, p-start+1);
		regname2[p-start] = '\0';
		Set_Register_Range_Not_Allocatable (regname, regname2);
		if (*p == 0) return;
		++p;
		start = p;
	}
	else {
		++p;
	}
    }
    strncpy (regname, start, p-start+1);
    Set_Register_Never_Allocatable (regname);
  }
}
#endif
#ifdef KEY
char ** be_command_line_args = NULL;
INT be_command_line_argc = 0;
#endif // KEY
#ifdef TARG_ST
//TB
/* ====================================================================
 *   CG_Save_Default_Options(void)
 *
 *   Save the current values for CG options.
 * ====================================================================
 */
void
CG_Save_Default_Options(void)
{
  Save_Option_Groups(Cg_Option_Groups);
}
/* ====================================================================
 *   CG_Reset_Default_Options(void)
 *
 *   Reset the default options.
 * ====================================================================
 */
void
CG_Reset_Default_Options(void)
{
  Reset_Option_Groups(Cg_Option_Groups);
}

/* ====================================================================
 *   CG_Apply_Opt_Level
 *
 *   Set options for a level of optimization.
 * ====================================================================
 */
void
CG_Apply_Opt_Level(UINT32 level)
{
#ifdef TARG_STxP70
  if(level >= 2 && !CG_cbpo_optimize_load_imm_overridden) {
    CG_cbpo_optimize_load_imm = DEFAULT_LOAD_IMM(level, FALSE);
  }
  if(!CG_cbpo_block_method_overridden) {
    CG_cbpo_block_method = DEFAULT_BLOCK_METHOD(level, FALSE);
  }
  if(!CG_cbpo_ratio_overridden) {
    CG_cbpo_ratio = DEFAULT_RATIO(level, FALSE);
  }
  if(!CG_cbpo_facto_cst_overridden) {
    CG_cbpo_facto_cst = DEFAULT_FACTO_CST(level, FALSE);
  }
  if(level >= 2 && !CG_tailmerge_overridden) {
    CG_tailmerge = DEFAULT_TAILMERGE(level, FALSE);
  }
  if(level >= 2 && !CG_simp_flow_in_tailmerge_overridden) {
    CG_simp_flow_in_tailmerge = DEFAULT_SIMP_FLOW_TAILMERGE(level, FALSE);
  }
  // TDR: Default to Optimized_Double_Post_Sched schedule for STxP70 in speed mode.
  if (!LOCS_POST_Scheduling_overriden) {
    LOCS_POST_Scheduling = Optimized_Double_Load_Sched;
  }
  // [TTh] Disable usage of preferred registers in -O3 on v4 archi
  // (On v4, pref regs are used to prioritize regs available in GP16,
  //  but it increases dependencies between instructions)
  if (level >= 3 && Generate_Code_For_v4 && !CG_COLOR_use_pref_regs_overridden) {
    CG_COLOR_use_pref_regs = FALSE;
  }
#endif
#ifdef TARG_ST
    // TDR: Modify global code motion default values
  if (!GCM_PRE_Enable_Scheduling_overridden)   	GCM_PRE_Enable_Scheduling = FALSE;
  if (!GCM_Forw_Circ_Motion_overridden)         GCM_Forw_Circ_Motion = FALSE;
#endif
  
  
}

/* ====================================================================
 *   CG_Apply_Opt_Size
 *
 *   Set options for size.
 * ====================================================================
 */
void
CG_Apply_Opt_Size(UINT32 level)
{
  //level = 0 means no size opt
  if (level == PU_OPTLEVEL_0 || level == PU_OPTLEVEL_UNDEF) return;

  FmtAssert(level == PU_OPTLEVEL_1,
	    ("CG_Apply_Opt_Size: only level 1 is implemented (asked was %d)",level));

  if (!CGEXP_expandconstant_set)
    CGEXP_expandconstant = 2;

  if (!clone_incr_overridden) 
    CFLOW_clone_incr = 1;

  if (!clone_min_incr_overridden) 
    CFLOW_clone_min_incr = 1;

  if (!clone_max_incr_overridden) 
    CFLOW_clone_max_incr = 3;

  if (!CFLOW_Space_overridden)
    CFLOW_Space = TRUE;

#ifdef TARG_STxP70
  if (!CFLOW_Enable_Clone_overridden) 
    CFLOW_Enable_Clone = FALSE;
#endif
  if (!EMIT_space_overridden)
    EMIT_space = TRUE;

  //cg_select
  if (!CG_ifc_freq_overriden)
    CG_ifc_freq = FALSE;
  if (!CG_ifc_cycles_overridden)
    CG_ifc_cycles = FALSE;
  if (!CG_ifc_allow_dup_overridden)
    CG_ifc_allow_dup = FALSE;
  if (!CG_select_spec_stores_overridden)
    CG_select_spec_stores = PROC_has_predicate_stores() && Enable_Conditional_Store;
  if (!CG_ifc_space_overridden)
    CG_ifc_space = TRUE;
  //Integrated Global and Local Scheduling Framework
  if (!CG_enable_thr_overridden)
    CG_enable_thr = FALSE;
  if (!CG_IV_offset_space_overridden)
    CG_IV_offset_space = TRUE;
  //GRA
  if (!GRA_spill_count_factor_for_size_set)
    GRA_spill_count_factor_for_size = TRUE;
  if (!GRA_split_for_size_set)
    GRA_split_for_size = TRUE;
  if (!GRA_spill_count_min_set)
    GRA_spill_count_min= TRUE;

#ifdef TARG_STxP70
  //common base pointer optimization
  if(!CG_cbpo_optimize_load_imm_overridden) {
    CG_cbpo_optimize_load_imm = DEFAULT_LOAD_IMM(CG_opt_level, TRUE);
  }
  if(!CG_cbpo_block_method_overridden) {
    CG_cbpo_block_method = DEFAULT_BLOCK_METHOD(CG_opt_level, TRUE);
  }
  if(!CG_cbpo_ratio_overridden) {
    CG_cbpo_ratio = DEFAULT_RATIO(CG_opt_level, TRUE);
  }
  if(!CG_cbpo_facto_cst_overridden) {
    CG_cbpo_facto_cst = DEFAULT_FACTO_CST(CG_opt_level, TRUE);
  }
  if(!CG_tailmerge_overridden) {
    CG_tailmerge = DEFAULT_TAILMERGE(CG_opt_level, TRUE);
  }
  if(!CG_simp_flow_in_tailmerge_overridden) {
    CG_simp_flow_in_tailmerge = DEFAULT_SIMP_FLOW_TAILMERGE(CG_opt_level, TRUE);
  }
  //[TDR][TTh] in V4, enable both lra_minregs and preferred_regs usage
  if (Generate_Code_For_v4) {
    if (!LRA_minregs_overridden) {
      LRA_minregs=TRUE;
    }
    if (!CG_COLOR_use_pref_regs_overridden) {
      CG_COLOR_use_pref_regs = TRUE;
    }
  }
  // TDR: Default to Optimized_Post_Sched schedule for STxP70 in size mode.
  if (!LOCS_POST_Scheduling_overriden) {
	  LOCS_POST_Scheduling = Optimized_Load_Sched; 
  }
#endif
}
#endif

/* ====================================================================
 *
 * main
 *
 * Main entry point and driver for the Code Generator.
 *
 * ====================================================================
 */

void
CG_Process_Command_Line (INT cg_argc, char **cg_argv, INT be_argc, char **be_argv)
{
    extern char *Whirl_Revision;

    if (strcmp (Whirl_Revision, WHIRL_REVISION) != 0)
	FmtAssert (!DEBUG_Ir_Version_Check,
		   ("WHIRL revision mismatch between be.so (%s) and cg.so (%s)",
		    Whirl_Revision, WHIRL_REVISION));
#ifdef TARG_ST
  if(be_argc == -1) {
	  Process_Command_Line (cg_argc, cg_argv);
	  return;
  }
#endif

    Set_Error_Descriptor (EP_BE, EDESC_BE);
    Set_Error_Descriptor (EP_CG, EDESC_CG);

#ifdef KEY
    be_command_line_args = be_argv;
    be_command_line_argc = be_argc;
#endif // KEY

    /* Perform preliminary command line processing: */
    Build_Option_String ( be_argc, be_argv );
    Process_Command_Line ( cg_argc, cg_argv );
#ifdef TARG_ST
  // [TTh] Append content of Disabled_Registers within Registers_Not_Allocatable,
  // to insure that Registers_Not_Allocatable is a superset of Disable_Registers
  OPTION_LIST *pred_olist    = Registers_Not_Allocatable;
  OPTION_LIST *disabled_regs = Disabled_Registers;
  OPTION_LIST *dup;

  for ( ; disabled_regs != NULL; disabled_regs = OLIST_next(disabled_regs)) {
    dup = (OPTION_LIST *) malloc ( sizeof(OPTION_LIST) );
    OLIST_val(dup)  = OLIST_val(disabled_regs) ? strdup ( OLIST_val(disabled_regs) ) : NULL;
    OLIST_opt(dup)  = OLIST_opt(disabled_regs) ? strdup ( OLIST_opt(disabled_regs) ) : NULL;
    OLIST_next(dup) = NULL;
    
    if (pred_olist == NULL) {
      // First element of the list
      Registers_Not_Allocatable = dup;
    } else {
      // Chain new element to existing list
      OLIST_next(pred_olist) = dup;
    }
    pred_olist = dup;
  }
#endif

    CG_Configure_Opt_Level(Opt_Level);

    Prepare_Source ();
} /* CG_Process_Command_Line */


/* Initialization that needs to be done after the global symtab is read */
void
CG_Init (void)
{
    Set_Error_Phase ( "Codegen Initialization" );
    MEM_POOL_Initialize (&MEM_local_region_pool, "local_region_pool", TRUE /* zero-memory */);
    MEM_POOL_Initialize (&MEM_local_region_nz_pool, "local_region_nz_pool", FALSE /* zero-memory */);

    REGISTER_Begin();	/* initialize the register package */
    Init_Dedicated_TNs ();

    Mark_Specified_Registers_As_Not_Allocatable ();

    EMT_Begin_File ( Argv0, option_string );
     /* this has to be done after LNO has been loaded to grep
   * prefetch_ahead fromn LNO */
  //    Configure_prefetch_ahead();

#ifdef LAO_ENABLED
  if (CG_LAO_activation != 0) {
    if (!CG_LAO_regiontype_overridden) CG_LAO_regiontype = 1;
    if (!CG_LAO_conversion_overridden) CG_LAO_conversion = 0;
    if (!CG_LAO_predication_overridden) CG_LAO_predication = 0;
    if (!CG_LAO_scheduling_overridden) CG_LAO_scheduling = 2;
    if (!CG_LAO_allocation_overridden) CG_LAO_allocation = 2;
    if (!CG_LAO_rcmssolving_overridden) CG_LAO_rcmssolving = 0;
    if (!CG_LAO_preloading_overridden) CG_LAO_preloading = 0;
    if (!CG_LAO_l1missextra_overridden) CG_LAO_l1missextra = 0;
    if (!CG_LAO_compensation_overridden) CG_LAO_compensation = 0;
    if (!CG_LAO_speculation_overridden) {
      if (Eager_Level == EAGER_NONE) CG_LAO_speculation = 0;
      else if (!Enable_Dismissible_Load) CG_LAO_speculation = 2;
      else CG_LAO_speculation = 3;
    }
    if (!CG_LAO_relaxation_overridden) CG_LAO_relaxation = 2;
    if (!CG_LAO_pipelining_overridden) CG_LAO_pipelining = 2;
    if (!CG_LAO_logtimeout_overridden) CG_LAO_logtimeout = 0;
    if (!CG_LAO_renaming_overridden) {
	if (CG_LOOP_unroll_times_max <= 1) CG_LAO_renaming = 1;
	else CG_LAO_renaming = 2;
    }
    if (!CG_LAO_boosting_overridden) CG_LAO_boosting = 1;
    if (!CG_LAO_aliasing_overridden) CG_LAO_aliasing = 1;
    if (!CG_LAO_prepadding_overridden) CG_LAO_prepadding = 0;
    if (!CG_LAO_postpadding_overridden) CG_LAO_postpadding = 0;
    if (!CG_LAO_overrun_overridden) CG_LAO_overrun = 0;
    if (!CG_LAO_opslimit_overridden) CG_LAO_opslimit = 1024;
    lao_init();
  }
#endif
#ifdef TARG_ST
# if 0 //probably it's enough to use path64 CG_Init_Gcov
  // TB: initialize coverage module
  if (Profile_Arcs_Enabled_Cgir)
    gcov_init (Remove_Extension(Src_File_Name));
#endif
  // [GS-DFGforISE] Init of exportation to DfgForIseclasses.
  if ( (CG_dfg_ise_mask & 0xff)
       || (CG_dfg_debug_mask & 0xff) ) {
    DfgForIse::ExportFromBackEnd_Initialize();
  }
#endif

    /* this has to be done after LNO has been loaded to grep
     * prefetch_ahead fromn LNO */
    Configure_prefetch_ahead();
#ifdef KEY
    if (flag_test_coverage || profile_arcs)
      CG_Init_Gcov();

    if (LOCS_Fwd_Scheduling_set) {
      fprintf(stderr, "warning: -CG:local_fwd_sched is deprecated,"
		      " use -CG:local_sched_alg\n");
      if (!LOCS_Scheduling_Algorithm_set) {
	LOCS_Scheduling_Algorithm = LOCS_Fwd_Scheduling ? 1 : 0;
	LOCS_Scheduling_Algorithm_set = TRUE;
      } else {
	fprintf(stderr, "warning: -CG:local_fwd_sched ignored,"
			" conflicts with -CG:local_sched_alg\n");
      }
    }
#endif // KEY
} /* CG_Init */

#ifdef KEY
extern void CG_End_Final();
#endif
/* Terimination routines for cg */
void
CG_Fini (void)
{

#ifdef TARG_ST
  // [GS-DFGforISE] End of exportation to DfgForIse classes.
  if ( (CG_dfg_ise_mask & 0xff)
       || (CG_dfg_debug_mask & 0xff) ) {
    DfgForIse::ExportFromBackEnd_Finalize();
  }
#if 0 //probably it's enough to use CG_End_Gcov
  // TB: finalize coverage module
  if (Profile_Arcs_Enabled_Cgir)
    gcov_finish (Asm_File);
#endif
#endif

#ifdef LAO_ENABLED
  if (CG_LAO_activation != 0) {
    lao_fini();
  }
#endif


#if defined( KEY) 
    extern BOOL profile_arcs;

    if (profile_arcs)
        CG_End_Final();
    if (flag_test_coverage || profile_arcs)
    	CG_End_Gcov();
#endif
    /* List global symbols if desired: */
    if ( List_Symbols ) {
	Print_global_symtab (Lst_File);
    }

    Set_Error_Phase ( "Codegen Emit" );
    /* Finish off the relocatable object file: */
    if (!MiniR_Code) EMT_End_File();
    MEM_POOL_Delete (&MEM_local_region_pool);
    MEM_POOL_Delete (&MEM_local_region_nz_pool);

#if defined( KEY) && !defined(TARG_ST)
    // Check to see if the asm file was written correctly.  Do this by writing
    // one extra char and checking its status.  Bug 11361.
    if (Assembly) {
      int n = fprintf(Asm_File, "\n");
      if (n != 1) {
	ErrMsg(EC_Asm_Write, Asm_File_Name);
	Terminate(1);
      }
    }
#endif
} /* CG_Fini */


/*
  Copyright (C) 2010, STMicroelectronics, All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.
*/

/* ====================================================================
 * ====================================================================
 *
 * Module: cg_minir.cxx
 *
 * Description:
 *
 * Outputs a minir file for the current module.
 *
 * Refer to MinimalIR project for the description of minir files:
 * https://www.assembla.com/wiki/show/minir-dev
 *
 *
 * Interface:
 *
 * void CG_Dump_Minir(INT32 pass_num, const char *pass_string, FILE *file)
 *
 *   Dumps the current region into a minir representation, appending to
 *   file.
 *   The first time this function is called the functions map entry is
 *   created in the output such that a whole module will be dumped
 *   in the same file as a list of functions in the MinIR.
 *
 * ====================================================================
 * ====================================================================
 */


#include "defs.h"
#include "config.h"
#include "glob.h"
#include "topcode.h"
#include "tn.h"
#include "op.h"
#include "bb.h"
#ifdef TARG_ST
#include "cg_ssa.h"
#endif

static const char *
TN_Register_Name(TN *tn)
{
  return REGISTER_name(TN_register_class(tn), TN_register(tn));
}

static const char *
TN_Register_Class_Name(TN *tn)
{
  return REGISTER_CLASS_name(TN_register_class(tn));
}

static const char *
TN_Virtual_Name(TN *tn)
{
  static char name_buffer[2+10+1]; /* 10 is max decimal digits for INT32. */
  sprintf(name_buffer, "TN%d", TN_number(tn));
  return name_buffer;
  
}

static const char *
BB_Name(BB *bb)
{
  static char name_buffer[2+10+1]; /* 10 is max decimal digits for INT32. */
  sprintf(name_buffer, "BB%d", BB_id(bb));
  return name_buffer;
}


static void 
CG_Dump_Minir_TN(TN *tn, TN *pinning, FILE *file)
{
  FmtAssert(TN_is_register(tn) || TN_is_constant(tn), ("Precondition on tn failed"));
  
  if (TN_is_register(tn)){
    if (TN_is_dedicated(tn)) {
      fprintf(file, "%s", TN_Register_Name(tn));
    } else
    if (TN_register(tn) != REGISTER_UNDEFINED) {
      fprintf(file, "%s", TN_Register_Name(tn));
    } else {
      if (pinning != NULL) {
	fprintf(file, "%s.%s", TN_Virtual_Name(tn), TN_Register_Name(pinning));
      } else {
	fprintf(file, "%s.%s", TN_Virtual_Name(tn), TN_Register_Class_Name(tn));
      }
    }
  } else if (TN_is_constant(tn)){
    if ( TN_has_value(tn)) {
      fprintf(file, "'" PRId64 "'", TN_value(tn));
    }
    else if (TN_is_enum(tn)) {
      fprintf(file, "%s", ISA_ECV_Name(TN_enum(tn)));
    }
    else if ( TN_is_label(tn) ) {
      const char *name = LABEL_name(TN_label(tn));
      INT64 offset = TN_offset(tn);
      if (offset == 0) {
	fprintf(file, "'(%s)'", name);
      } else {
	if (offset < 0) {
	  fprintf(file, "'(%s" PRId64 ")'", name, offset);
	} else {
	  fprintf(file, "'(%s+" PRId64 ")'", name, offset);
	}
      }
    } 
    else if ( TN_is_tag(tn) ) {
      fprintf(file, "'(%s)'", LABEL_name(TN_label(tn)));
    }
    else if ( TN_is_symbol(tn) ) {
      ST *var = TN_var(tn);
      
      fprintf(file, "'(");
#ifdef TARG_ST // relocation not available in targinfo path64 and available in MDS
      if (TN_relocs(tn) != TN_RELOC_NONE && 
	  *TN_RELOCS_Name(TN_relocs(tn)) != '\0') {
	fprintf(file, "%s", TN_RELOCS_Name(TN_relocs(tn)));
	if (TN_Reloc_has_parenthesis(TN_relocs(tn))) {
	  fprintf(file, "(");
	}
      }
#endif

      if (ST_class(var) == CLASS_CONST) {
      	fprintf(file, ".L_UNNAMED_CONST_%d", ST_tcon(var));
      } else {
	if (TN_offset(tn) == 0) {
	  fprintf(file,"%s", ST_name(var));
	} else {
	  if (TN_offset(tn) < 0) {
	    fprintf(file,"%s" PRId64, ST_name(var), TN_offset(tn));
	  } else {
	    fprintf(file,"%s+" PRId64, ST_name(var), TN_offset(tn));
	  }
	}
      }
#ifdef TARG_ST // relocation not available in targinfo path64 and available in MDS
      if (TN_relocs(tn) != TN_RELOC_NONE && 
	  *TN_RELOCS_Name(TN_relocs(tn)) != '\0' &&
	  TN_Reloc_has_parenthesis(TN_relocs(tn))) {
	fprintf(file, ")");
      }
#endif
      fprintf(file, ")'");
    }
    else {
      FmtAssert(FALSE, ("Precondition on tn failed"));
    }
  }
}

static void
CG_Dump_Minir_OP(BB *bb, OP *op, const char *pfx, FILE *file)
{
  const char *sep = "";
  fprintf(file, "%s { ", pfx);
  fprintf(file, "op: %s", TOP_Name(OP_code(op)));
  sep = ", ";
  if (OP_results(op) > 0) {
    fprintf(file, "%sdefs: [ ", sep);
    sep = "";
    for (int i = 0; i < OP_results(op); i++) {
      TN *tn = OP_result(op,i);
      TN *pinning = NULL;
#ifdef TARG_ST // no SSA in CGIR-path64
      if (OP_Has_ssa_pinning(op))
 	pinning = OP_Get_result_pinning(op, i);
#endif
      fprintf(file, "%s", sep);
      CG_Dump_Minir_TN(tn, pinning, file);
      sep =", ";
    }
    fprintf(file, "]");
    sep = ", ";
  }
  if (OP_opnds(op) > 0) {
    fprintf(file, "%suses: [ ", sep);
    sep = "";
    for (int i = 0; i < OP_opnds(op); i++) {
      TN *tn = OP_opnd(op,i);
      TN *pinning = NULL;
#ifdef TARG_ST // no SSA in CGIR-path64
      if (OP_Has_ssa_pinning(op))
 	pinning = OP_Get_opnd_pinning(op, i);
#endif
      fprintf(file, "%s", sep);
      CG_Dump_Minir_TN(tn, pinning, file);
#ifdef TARG_ST // no SSA in CGIR-path64
      if (OP_phi(op)) {
 	BB *phi_opnd_bb = Get_PHI_Predecessor (op, i);
	fprintf(file, "<%s>", BB_Name(phi_opnd_bb));
      }
#endif
      sep =", ";
    }
    fprintf(file, "]");
  }
  if (op == BB_branch_op(bb)) {
    INT nsuccs = BB_succs(bb) ? BBlist_Len(BB_succs(bb)): 0;
    if (nsuccs != 0) {
      BBLIST *bl;
      BB *bb_fallthru = BB_Fall_Thru_Successor(bb);
      BOOL single_target = nsuccs == 1 || (bb_fallthru != NULL && nsuccs == 2);
      if (single_target) {
	fprintf(file, "%starget: ", sep);
      } else {
	fprintf(file, "%stargets: [ ", sep);
      }
      sep = "";
      FOR_ALL_BB_SUCCS (bb, bl) {
	if (BBLIST_item(bl) != bb_fallthru) {
	  fprintf (file, "%s%s", sep, BB_Name(BBLIST_item(bl)));
	  sep = ", ";
	}
      }
      if (!single_target) {
	fprintf(file, "]");
      }
      if (bb_fallthru != NULL) {
	fprintf(file, ", fallthru: %s", BB_Name(bb_fallthru));
      }
      sep = ", ";
    }
  }
  if (OP_call(op)) {
    ISA_REGISTER_CLASS rc;
    INT num_clobbered = 0;
    FOR_ALL_ISA_REGISTER_CLASS(rc) {
      REGISTER_SET clobber_set = BB_call_clobbered(bb, rc);
      REGISTER reg;
      FOR_ALL_REGISTER_SET_members(clobber_set, reg) {
	TN *tn = Build_Dedicated_TN(rc, reg, 0);
	if (num_clobbered++ == 0) {
	  fprintf(file, "%simplicit_defs: [ ", sep);
	  sep = "";
	}
	fprintf(file, "%s", sep);
	CG_Dump_Minir_TN(tn, NULL, file);
	sep = ", ";
      }
    }
    if (num_clobbered > 0) {
      fprintf(file, "]");
      sep = ", ";
    }
  }

  fprintf(file, "}\n");
}

static void
CG_Dump_Minir_OPs(BB *bb, OP *op, const char *pfx, FILE *file)
{
  for ( ; op != NULL; op = OP_next(op))
    CG_Dump_Minir_OP(bb, op, pfx, file);
}

static void
CG_Dump_Minir_BB(BB *bb, const char *pfx, FILE *file)
{
  fprintf(file, "%slabel: %s\n", pfx, BB_Name(bb));
  if (BB_has_label(bb)) {
    const char *sep = "";
    ANNOTATION *ant;
    fprintf(file, "%sannotations: { labels: [ ", pfx);
    for (ant = ANNOT_First(BB_annotations(bb), ANNOT_LABEL);
	 ant != NULL;
	 ant = ANNOT_Next(ant, ANNOT_LABEL)) {
      fprintf (file,"%s'%s'", sep, LABEL_name(ANNOT_label(ant)));
      sep = ", ";
    }
    fprintf(file, "] }\n");
  }
  if (BB_first_op(bb)) {
    char ops_pfx[strlen(pfx) + 4 + 1];
    sprintf(ops_pfx, "%s  - ", pfx);
    fprintf(file, "%sops:\n", pfx);
    CG_Dump_Minir_OPs(bb, BB_first_op(bb), ops_pfx, file);
  } else {
    fprintf(file, "%sops: [ { op: NOP } ]\n", pfx);
  }
}

void
CG_Dump_Minir(INT32 pass_num, const char *pass_string, FILE *file)
{
  BB *bb;
  const char *sep;
  const char *pfx;
  const char *bb_pfx;
  static int function_num;

  FmtAssert(pass_string != NULL, ("Precondition on pass_string failed"));
  FmtAssert(file != NULL, ("Precondition on file failed"));
  FmtAssert(REGION_First_BB != NULL, ("Precondition on function failed"));
  FmtAssert(Cur_PU_Name != NULL, ("Precondition on function failed"));

  if (function_num++ == 0) {
    fprintf(file, "functions:\n");
  }
  fprintf(file, "  -\n");
  pfx = "    ";
  fprintf(file, "%slabel: %s\n", pfx, Cur_PU_Name);
  fprintf(file, "%sentries: [ ", pfx);
  sep = "";
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    if (BB_entry(bb)) {
      fprintf(file, "%s%s", sep, BB_Name(bb));
      sep = ", ";
    }
  }
  fprintf(file, "]\n");
  fprintf(file, "%sexit: [ ", pfx);
  sep = "";
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    if (BB_exit(bb)) {
      fprintf(file, "%s%s", sep, BB_Name(bb));
      sep = ", ";
    }
  }
  fprintf(file, "]\n");

  fprintf(file, "%sbbs:\n", pfx);

  bb_pfx = "        ";
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    fprintf(file, "%s  -\n", pfx);
    CG_Dump_Minir_BB(bb, bb_pfx, file);
  }
}


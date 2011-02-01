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
 * void CG_Dump_Minir(INT32 pass_num, const char *pass_string, std::ostringstream *oss)
 *
 *   Dumps the current region into a minir representation, appending to
 *   oss.
 *
 * ====================================================================
 * ====================================================================
 */

#include "errors.h"
#include "config_list.h"
#include "config_opt.h"
#include "config_asm.h"
#include "note.h"
#include "freq.h"
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

#include "cgemit.h"
#include <sstream>
#include <iostream>


static const char*
TN_Register_Name(TN *tn)
{
  static std::string res;
  res=REGISTER_name(TN_register_class(tn), TN_register(tn));
  FmtAssert(res[0]=='%', ("Register name not starting with %"));
  res[0]='$';
  return res.c_str();
}

static const char *
TN_Register_Class_Name(TN *tn)
{
  return REGISTER_CLASS_name(TN_register_class(tn));
}

static const char *
TN_Virtual_Name(TN *tn)
{
  static std::ostringstream name_buffer;
  static std::string str;
  name_buffer.str("");
  name_buffer << "V" << TN_number(tn);
  str = name_buffer.str();
  return str.c_str();
}

const char *
BB_Name(BB *bb)
{
  static std::ostringstream name_buffer;
  static std::string str;
  name_buffer.str("");
  name_buffer << "BB" << BB_id(bb);
  str = name_buffer.str();
  return str.c_str();
}


static void 
CG_Dump_Minir_TN(TN *tn, TN *pinning, std::ostringstream *oss)
{
  FmtAssert(TN_is_register(tn) || TN_is_constant(tn), ("Precondition on tn failed"));
  
  if (TN_is_register(tn)){
    if (TN_is_dedicated(tn)) {
      *oss << TN_Register_Name(tn);
    } else
    if (TN_register(tn) != REGISTER_UNDEFINED) {
      *oss << TN_Register_Name(tn);
    } else {
      if (pinning != NULL) {
        FmtAssert(false, ("%s:%d, Does not handle pinning for now!\n", __FILE__, __LINE__));
	*oss << TN_Virtual_Name(tn) << "." << TN_Register_Name(pinning);
      } else {
	*oss << TN_Virtual_Name(tn) << "." << TN_Register_Class_Name(tn);
      }
    }
  } else if (TN_is_constant(tn)){
    if ( TN_has_value(tn)) {
      *oss << "'" << TN_value(tn) << "'";
    }
    else if (TN_is_enum(tn)) {
      //fprintf(file, "%s", ISA_ECV_Name(TN_enum(tn)));
      FmtAssert(false, ("No enums should be printed by this function"));
    }
    else if ( TN_is_label(tn) ) {
      const char *name = LABEL_name(TN_label(tn));
      INT64 offset = TN_offset(tn);
      if (offset == 0) 
	*oss << "[" << name << "]";
      else 
	*oss << "[" << name << ", '" << std::showpos << offset << "']";
    } 
    else if ( TN_is_tag(tn) ) {
      *oss << "[" << LABEL_name(TN_label(tn)) << "]";
    }
    else if ( TN_is_symbol(tn) ) {
      ST *var = TN_var(tn);
      
      *oss << "[";
#ifdef TARG_ST // relocation not available in targinfo path64 and available in MDS
      if (TN_relocs(tn) != ISA_RELOC_UNDEFINED && 
	  *TN_RELOCS_Name(TN_relocs(tn)) != '\0') {
	*oss << TN_RELOCS_Name(TN_relocs(tn));
	if (TN_Reloc_has_parenthesis(TN_relocs(tn))) 
	  *oss << "(";
      }
#endif

      if (ST_class(var) == CLASS_CONST) 
	*oss << ".LC" << ST_IDX_index(ST_st_idx(var));
      else {
	if (TN_offset(tn) == 0) 
	  *oss << ST_name(var);
	else 
	  *oss << ST_name(var) << ", '" << std::showpos << TN_offset(tn) << "'";
      }
#ifdef TARG_ST // relocation not available in targinfo path64 and available in MDS
      if (TN_relocs(tn) != ISA_RELOC_UNDEFINED && 
	  *TN_RELOCS_Name(TN_relocs(tn)) != '\0' &&
	  TN_Reloc_has_parenthesis(TN_relocs(tn))) {
	*oss << ")";
      }
#endif
      *oss << "]";
    }
    else 
      FmtAssert(FALSE, ("Precondition  on tn failed"));
  }
}

static void
CG_Dump_Minir_OP(BB *bb, OP *op, const char *pfx, std::ostringstream *oss)
{
  const char *sep = "";
  *oss << pfx << " {";
  *oss << "op: " << TOP_Name(OP_code(op));
  if (OP_simulated(op)) 
    *oss << "(simulated)"; // Assemble_Simulated_OP(op, bb)

  FmtAssert(OP_code(op) != TOP_intrncall, ("see CGEMIT_Print_Intrinsic_OP if required"));

  // Append enums (modifiers) separated by dots `.'
  if (OP_opnds(op) > 0) {
    for (int i = 0; i < OP_opnds(op); i++) {
      TN *tn = OP_opnd(op,i);
      if (TN_is_enum(tn)) 
        *oss << "." << ISA_ECV_Name(TN_enum(tn));
    }
  }
  sep = ", ";
  if (OP_results(op) > 0) {
    *oss << sep << "defs: [";
    sep = "";
    for (int i = 0; i < OP_results(op); i++) {
      TN *tn = OP_result(op,i);
      TN *pinning = NULL;
#ifdef TARG_ST // no SSA in CGIR-path64
      if (OP_Has_ssa_pinning(op))
 	pinning = OP_Get_result_pinning(op, i);
#endif
      *oss << sep;
      CG_Dump_Minir_TN(tn, pinning, oss);
      sep =", ";
    }
    *oss << "]";
    sep = ", ";
  }
  if (OP_opnds(op) > 0) {
    *oss << sep << "uses: [";
    sep = "";
    for (int i = 0; i < OP_opnds(op); i++) {
      TN *tn = OP_opnd(op,i);
      TN *pinning = NULL;
      if (TN_is_enum(tn)) continue;
#ifdef TARG_ST // no SSA in CGIR-path64
      if (OP_Has_ssa_pinning(op))
 	pinning = OP_Get_opnd_pinning(op, i);
#endif
      *oss << sep;
      CG_Dump_Minir_TN(tn, pinning, oss);
#ifdef TARG_ST // no SSA in CGIR-path64
      if (OP_phi(op)) {
 	BB *phi_opnd_bb = Get_PHI_Predecessor (op, i);
	*oss << "<" << BB_Name(phi_opnd_bb) << ">";
      }
#endif
      sep =", ";
    }
    *oss << "]";
  }
  if (op == BB_branch_op(bb)) {
    INT nsuccs = BB_succs(bb) ? BBlist_Len(BB_succs(bb)): 0;
    if (nsuccs != 0) {
      BBLIST *bl;
      BB *bb_fallthru = nsuccs == 1 ? NULL : BB_Fall_Thru_Successor(bb);
      BOOL single_target = nsuccs == 1 || (bb_fallthru != NULL && nsuccs == 2);
      *oss << sep << (single_target ? "target: " : "targets: [");
      sep = "";
      FOR_ALL_BB_SUCCS (bb, bl) {
	if (BBLIST_item(bl) != bb_fallthru) 
	  {
	  *oss << sep << BB_Name(BBLIST_item(bl));
	  sep = ", ";
	}
      }
      if (!single_target) 
	*oss << "]";
      if (bb_fallthru != NULL) {
	*oss << ", fallthru: " << BB_Name(bb_fallthru);
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
	  *oss << sep << "implicit_defs: [";
	  sep = "";
	}
	*oss << sep;
	CG_Dump_Minir_TN(tn, NULL, oss);
	sep = ", ";
      }
    }
    if (num_clobbered > 0) {
      *oss << "]";
      sep = ", ";
    }
  }

  *oss << "}\n";
}

static void
CG_Dump_Minir_OPs(BB *bb, OP *op, const char *pfx, std::ostringstream *oss)
{
  for ( ; op != NULL; op = OP_next(op)) {
#if 0
    if (OP_simulated(op))
      Assemble_Simulated_OP(op, bb);
#endif
    CG_Dump_Minir_OP(bb, op, pfx, oss);
  }
}

void
CG_Dump_Minir_BB(BB *bb, const char *pfx, std::ostringstream *oss)
{
  // Dump labels
  *oss << "label: " << BB_Name(bb) << "\n";
  if (BB_has_label(bb))
    {
    const char *sep = ""; 
    ANNOTATION *ant;
    *oss << pfx << "labels: [";
    for (ant = ANNOT_First(BB_annotations(bb), ANNOT_LABEL);
	 ant != NULL;
	 ant = ANNOT_Next(ant, ANNOT_LABEL)) {
      *oss << sep << "'" << LABEL_name(ANNOT_label(ant)) << "'";
      sep = ", ";
    }
    *oss << "]\n";
  }

  // Dump loop info
  if (BB_loop_head_bb(bb)) {
    //       Emit_Loop_Note(bb, file);
  }

  // dump frequency & successor BBs probabilities
  *oss << pfx << "frequency: " << BB_freq(bb) << "\n";
  if (!BB_exit(bb)) { 
    const char *sep = "";
    BBLIST *item;
    *oss << pfx << "probabilities: { ";
    FOR_ALL_BB_SUCCS(bb, item) {
      BB *bb_succ = BBLIST_item(item);
      *oss << sep << BB_Name(bb_succ) << ": " << BBLIST_prob(item);
      sep = ", ";
    }
    *oss << " }\n";
  }

  // dump predecessor blocks
  if (!BB_entry(bb)) {
    const char *sep = "";
    BBLIST *item;
    *oss << pfx << "predecessors: { ";
    FOR_ALL_BB_PREDS(bb, item) {
      BB *bb_pred = BBLIST_item(item);
      *oss << sep << BB_Name(bb_pred);
      sep = ", ";
    }
    *oss << " }\n";
  }

  // Dump operations
  if (BB_first_op(bb)) { 
    char ops_pfx[strlen(pfx) + 4 + 1];
    sprintf(ops_pfx, "%s  - ", pfx);
    *oss << pfx << "ops:\n";
    CG_Dump_Minir_OPs(bb, BB_first_op(bb), ops_pfx, oss);
  } else {
    *oss << pfx << "ops: [ { op: NOP } ]\n";
  }
}



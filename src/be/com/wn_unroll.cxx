/*
 *  Copyright 2008 PathScale, LLC.  All Rights Reserved.
 */

/*
 *  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2005, 2006 PathScale, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.  
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

/* This file implements loop unrolling in WHIRL.  Currently, only DO_LOOPs
 * are unrolled.
 */

#ifdef USE_PCH
#include "be_com_pch.h"
#endif /* USE_PCH */
#pragma hdrstop

#include "defs.h"
#include "config.h"
#include "opt_config.h"
#include "be_errors.h"
#include "erglob.h"
#include "tracing.h"
#include "glob.h"
#include "timing.h"
#include "wn.h"
#include "wn_util.h"
#include "wn_lower.h"

class WN_UNROLL { // for implementing the actual unrolling mechanism
private:
  WN *_orig_wn;		// the original DO_LOOP WHIRL node
  WN *_indx_var;	// the IDNAME for the index var
  WN *_init_stmt;
  WN *_start;		// the expression for the start value
  WN *_end_cond;
  WN *_end;		// the expression for the end value; if NULL, designate
  			// a non-well-formed DO loop; this does NOT point to
			// the original WHIRL nodes (i.e. a copy of the tree is
			// made)
  WN *_trips;		// the expression for the absolute trip count; this also
  			// is made up of its own WHIRL nodes
  WN *_incr_stmt;
  TYPE_ID _rtype;	// the MTYPE for the induction variable
  WN *_loop_body;
  WN *_loop_info;	// loop_info from the original DO_LOOP
  INT _map_id;		// the map_id field in the original DO_LOOP

  INT _step_amt;	// if variable incr amount, set to 0
  INT _abs_step_amt;    // absolute value of _step_amt;
  INT _node_count;	// number of WHIRL nodes in loop body
  INT _if_count;	// number of IF statements in loop body
  INT _istore_count;	// number of indirect stores in loop body

public:
  WN *Loop_info(void) const { return _loop_info; }
  WN *End(void) const { return _end; }
  WN *Trips(void) const { return _trips; }
  INT Step_amt(void) const { return _step_amt; }
  INT Node_count(void) const { return _node_count; }
  INT If_count(void) const { return _if_count; }
  INT Istore_count(void) const { return _istore_count; }

  WN_UNROLL(WN *doloop): _orig_wn(doloop),
			 _indx_var(WN_kid0(doloop)),
			 _init_stmt(WN_kid1(doloop)),
			 _end_cond(WN_kid2(doloop)),
			 _incr_stmt(WN_kid(doloop, 3)),
			 _loop_body(WN_kid(doloop, 4)),
			 _map_id(WN_map_id(doloop)),
  			 _node_count(0), 
			 _if_count(0),
			 _istore_count(0)
    { _start = WN_kid0(_init_stmt);

      _rtype = WN_rtype(WN_kid0(_incr_stmt));

      // if index var is on rhs of condition, swap it
      if (WN_operator(WN_kid1(_end_cond)) == OPR_LDID &&
	  WN_st_idx(WN_kid1(_end_cond)) == WN_st_idx(_indx_var) &&
	  WN_offset(WN_kid1(_end_cond)) == WN_offset(_indx_var)) {
	WN *temp = WN_kid0(_end_cond);
	WN_kid0(_end_cond) = WN_kid1(_end_cond);
	WN_kid1(_end_cond) = temp;
	switch (WN_operator(_end_cond)) {
	case OPR_LT: WN_set_operator(_end_cond, OPR_GT); break;
	case OPR_LE: WN_set_operator(_end_cond, OPR_GE); break;
	case OPR_GT: WN_set_operator(_end_cond, OPR_LT); break;
	case OPR_GE: WN_set_operator(_end_cond, OPR_LE); break;
	default: _end = NULL; return;
	}
      }

      if (WN_operator(WN_kid0(_end_cond)) == OPR_LDID &&
	  WN_st_idx(WN_kid0(_end_cond)) == WN_st_idx(_indx_var) &&
	  WN_offset(WN_kid0(_end_cond)) == WN_offset(_indx_var))
	_end = WN_COPY_Tree_With_Map(WN_kid1(_end_cond));
      else _end = NULL;

      WN *incr_expr = WN_kid0(_incr_stmt);
      if (WN_operator(incr_expr) != OPR_ADD)
	_step_amt = 0;
      else if (WN_operator(WN_kid1(incr_expr)) != OPR_INTCONST)
	_step_amt = 0;
      else _step_amt =  WN_const_val(WN_kid1(incr_expr));

      if (! (_step_amt > 0 && (WN_operator(_end_cond) == OPR_LT ||
			       WN_operator(_end_cond) == OPR_LE) ||
	     _step_amt < 0 && (WN_operator(_end_cond) == OPR_GT ||
			       WN_operator(_end_cond) == OPR_GE)))
        _end = NULL;

      // adjust _end for convert LE and GE
      if (_end)
        if (WN_operator(_end_cond) == OPR_LE || 
	    WN_operator(_end_cond) == OPR_GE)
	  _end = WN_Add(_rtype, _end, WN_Intconst(_rtype, _step_amt));

      _abs_step_amt = _step_amt >= 0 ? _step_amt : -_step_amt;

      if (_end) {
	if (_step_amt > 0)
	  _trips = WN_Sub(_rtype, _end, WN_COPY_Tree_With_Map(_start));
	else _trips = WN_Sub(_rtype, WN_COPY_Tree_With_Map(_start), _end);
      }

      if (WN_kid_count(doloop) == 6)
	_loop_info = WN_kid(doloop, 5);
      else _loop_info = NULL;
    }
  ~WN_UNROLL(void) {}

  void Analyze_body_expr(WN *tree);
  void Analyze_body_stmt(WN *tree);
  WN * Replicate_expr(WN *expr, INT rep_cnt);
  WN * Replicate_stmt(WN *stmt, INT rep_cnt);
  void Unroll(INT unroll_times);
};

/* ====================================================================
 * traverse expressions in loop body to gather information about its content 
 * ==================================================================== */
void
WN_UNROLL::Analyze_body_expr(WN *tree)
{
  OPERATOR opr = WN_operator(tree);
  INT i;

  _node_count++;

  switch (opr) {
  // leaves
  case OPR_LDID: 
  case OPR_LDBITS:
  case OPR_INTCONST:
  case OPR_CONST:
  case OPR_LDA:
  case OPR_LDA_LABEL:
    return;

  // unary
  case OPR_ILOAD:
  case OPR_ILDBITS:
  case OPR_SQRT: case OPR_RSQRT: case OPR_RECIP:
  case OPR_PAREN:
  case OPR_REALPART: case OPR_IMAGPART:
  case OPR_HIGHPART: case OPR_LOWPART:
  case OPR_ALLOCA:
  case OPR_LNOT:
  case OPR_EXTRACT_BITS:
  case OPR_BNOT:
  case OPR_PARM:
  case OPR_TAS:
  case OPR_RND: case OPR_TRUNC: case OPR_CEIL: case OPR_FLOOR:
#ifdef KEY
  case OPR_REPLICATE:
  case OPR_REDUCE_ADD: case OPR_REDUCE_MPY: 
  case OPR_REDUCE_MAX: case OPR_REDUCE_MIN:
  case OPR_SHUFFLE:
  case OPR_ATOMIC_RSQRT:
#endif // KEY
  case OPR_NEG:
  case OPR_ABS:
  case OPR_MINPART: case OPR_MAXPART:
  case OPR_CVTL:
  case OPR_CVT:
    Analyze_body_expr(WN_kid0(tree));
    return;

  // binary
  case OPR_MLOAD:
  case OPR_ILOADX:
  case OPR_MPY:
  case OPR_DIV: 
  case OPR_MOD: case OPR_REM:
  case OPR_DIVREM:
  case OPR_ADD: case OPR_SUB:
  case OPR_MAX: case OPR_MIN: 
  case OPR_MINMAX:
  case OPR_BAND: case OPR_BIOR: case OPR_BNOR: case OPR_BXOR:
  case OPR_ASHR: case OPR_LSHR:
  case OPR_SHL: 
  case OPR_RROTATE:
  case OPR_EQ: case OPR_NE: 
  case OPR_GE: case OPR_GT: case OPR_LE: case OPR_LT:
  case OPR_LAND: case OPR_LIOR:
  case OPR_COMPLEX:
  case OPR_COMPOSE_BITS:
    Analyze_body_expr(WN_kid0(tree));
    Analyze_body_expr(WN_kid1(tree));
    return;

  // ternary
  case OPR_SELECT:
    Analyze_body_expr(WN_kid0(tree));
    Analyze_body_expr(WN_kid1(tree));
    Analyze_body_expr(WN_kid2(tree));
    return;

  // n-ary
  case OPR_INTRINSIC_OP: 
    for (i = 0; i < WN_kid_count(tree); i++)  // kids must be PARMs
      Analyze_body_expr(WN_kid(tree,i)); 
    return;

  default: Is_True(FALSE,("WN_UNROLL::Analyze_body_expr: unexpected operator"));
  }

  return;
}

/* ====================================================================
 * traverse statements in loop body to gather information about its content 
 * ==================================================================== */
void
WN_UNROLL::Analyze_body_stmt(WN *tree)
{
  OPERATOR opr = WN_operator(tree);

  switch (opr) {
  case OPR_COMMENT:
  case OPR_PRAGMA:
  case OPR_XPRAGMA:
    return;

  case OPR_PREFETCH:
  case OPR_ASSERT:
    _node_count++;
    // fall-thru

  case OPR_EVAL: 
    Analyze_body_expr(WN_kid0(tree));
    return;

  case OPR_MSTORE:
  case OPR_ISTOREX:
    Analyze_body_expr(WN_kid2(tree));
    // fall-thru

  case OPR_ISTORE:
  case OPR_ISTBITS:
    _istore_count++;
    Analyze_body_expr(WN_kid1(tree));
    // fall-thru

  case OPR_STBITS:
  case OPR_STID:
    _node_count++;
    Analyze_body_expr(WN_kid0(tree));
    return;

  case OPR_BLOCK: {
      WN *stmt;
      for (stmt = WN_first(tree); stmt; stmt = WN_next(stmt)) 
	Analyze_body_stmt(stmt);
      return;
    }

  case OPR_IF:
    _node_count++;
    _if_count++;
    Analyze_body_expr(WN_kid(tree, 0));
    Analyze_body_stmt(WN_kid(tree, 1));
    Analyze_body_stmt(WN_kid(tree, 2));
    return;

  default:	
    Is_True(FALSE,("WN_UNROLL::Analyze_body_stmt: unexpected operator"));
  }
  return;
}

/* ====================================================================
 * make a copy of the expression replacing each appearance of the index variable
 * i by i+rep_cnt and return the new expression
 * ==================================================================== */
WN *
WN_UNROLL::Replicate_expr(WN *expr, INT rep_cnt)
{
  OPERATOR opr = WN_operator(expr);
  INT i;

  WN *new_expr = WN_CopyNode(expr);
#if 0
  WN_COPY_All_Maps(new_expr, expr);
#else
  WN_set_map_id(new_expr, (WN_MAP_ID) (-1));
#endif


  switch (opr) {
  // leaves
  case OPR_LDID: 
    if (rep_cnt != 0 && WN_st_idx(expr) == WN_st_idx(_indx_var) &&
        WN_offset(expr) == WN_offset(_indx_var)) {
      new_expr = WN_Add(_rtype, new_expr, WN_Intconst(_rtype, rep_cnt));
    }
    break;

  case OPR_LDBITS:
  case OPR_INTCONST:
  case OPR_CONST:
  case OPR_LDA:
  case OPR_LDA_LABEL:
    break;

  // unary
  case OPR_ILOAD:
  case OPR_ILDBITS:
  case OPR_SQRT: case OPR_RSQRT: case OPR_RECIP:
  case OPR_PAREN:
  case OPR_REALPART: case OPR_IMAGPART:
  case OPR_HIGHPART: case OPR_LOWPART:
  case OPR_ALLOCA:
  case OPR_LNOT:
  case OPR_EXTRACT_BITS:
  case OPR_BNOT:
  case OPR_PARM:
  case OPR_TAS:
  case OPR_RND: case OPR_TRUNC: case OPR_CEIL: case OPR_FLOOR:
#ifdef KEY
  case OPR_REPLICATE:
  case OPR_REDUCE_ADD: case OPR_REDUCE_MPY: 
  case OPR_REDUCE_MAX: case OPR_REDUCE_MIN:
  case OPR_SHUFFLE:
  case OPR_ATOMIC_RSQRT:
#endif // KEY
  case OPR_NEG:
  case OPR_ABS:
  case OPR_MINPART: case OPR_MAXPART:
  case OPR_CVTL:
  case OPR_CVT:
    WN_kid0(new_expr) = Replicate_expr(WN_kid0(expr), rep_cnt);
    break;

  // binary
  case OPR_MLOAD:
  case OPR_ILOADX:
  case OPR_MPY:
  case OPR_DIV: 
  case OPR_MOD: case OPR_REM:
  case OPR_DIVREM:
  case OPR_ADD: case OPR_SUB:
  case OPR_MAX: case OPR_MIN: 
  case OPR_MINMAX:
  case OPR_BAND: case OPR_BIOR: case OPR_BNOR: case OPR_BXOR:
  case OPR_ASHR: case OPR_LSHR:
  case OPR_SHL: 
  case OPR_RROTATE:
  case OPR_EQ: case OPR_NE: 
  case OPR_GE: case OPR_GT: case OPR_LE: case OPR_LT:
  case OPR_LAND: case OPR_LIOR:
  case OPR_COMPLEX:
  case OPR_COMPOSE_BITS:
    WN_kid0(new_expr) = Replicate_expr(WN_kid0(expr), rep_cnt);
    WN_kid1(new_expr) = Replicate_expr(WN_kid1(expr), rep_cnt);
    break;

  // ternary
  case OPR_SELECT:
    WN_kid0(new_expr) = Replicate_expr(WN_kid0(expr), rep_cnt);
    WN_kid1(new_expr) = Replicate_expr(WN_kid1(expr), rep_cnt);
    WN_kid2(new_expr) = Replicate_expr(WN_kid2(expr), rep_cnt);
    break;

  // n-ary
  case OPR_INTRINSIC_OP: 
    for (i = 0; i < WN_kid_count(expr); i++)  // kids must be PARMs
      WN_kid(new_expr, i) = Replicate_expr(WN_kid(expr, i), rep_cnt);
    break;

  default: Is_True(FALSE,("WN_UNROLL::Replicate_expr: unexpected operator"));
  }

  return new_expr;
}

/* ====================================================================
 * make a copy of the statement replacing each appearance of the index variable
 * i by i+rep_cnt and return the new statement
 * ==================================================================== */
WN *
WN_UNROLL::Replicate_stmt(WN *stmt, INT rep_cnt)
{
  OPERATOR opr = WN_operator(stmt);

  if (opr == OPR_BLOCK) {
    WN *new_block = WN_CreateBlock();
    WN *s, *ns;
    for (s = WN_first(stmt); s; s= WN_next(s)) {
      ns = Replicate_stmt(s, rep_cnt);
      WN_INSERT_BlockLast(new_block, ns);
    }
    return new_block;
    }

  WN *new_stmt = WN_CopyNode(stmt);
#if 0
  WN_COPY_All_Maps(new_stmt, stmt);
#else
  WN_set_map_id(new_stmt, (WN_MAP_ID) (-1));
#endif

  switch (opr) {
  case OPR_COMMENT:
  case OPR_PRAGMA:
    break;

  case OPR_XPRAGMA:
  case OPR_PREFETCH:
  case OPR_ASSERT:
  case OPR_EVAL: 
    WN_kid0(new_stmt) = Replicate_expr(WN_kid0(stmt), rep_cnt);
    break;

  case OPR_MSTORE:
  case OPR_ISTOREX:
    WN_kid2(new_stmt) = Replicate_expr(WN_kid2(stmt), rep_cnt);
    // fall-thru

  case OPR_ISTORE:
  case OPR_ISTBITS:
    WN_kid1(new_stmt) = Replicate_expr(WN_kid1(stmt), rep_cnt);
    // fall-thru

  case OPR_STBITS:
  case OPR_STID:
    WN_kid0(new_stmt) = Replicate_expr(WN_kid0(stmt), rep_cnt);
    break;

  case OPR_IF:
    WN_kid0(new_stmt) = Replicate_expr(WN_kid0(stmt), rep_cnt);
    WN_kid1(new_stmt) = Replicate_stmt(WN_kid1(stmt), rep_cnt);
    WN_kid2(new_stmt) = Replicate_stmt(WN_kid2(stmt), rep_cnt);
    break;

  default:	
    Is_True(FALSE,("WN_UNROLL::Replicate_stmt: unexpected operator"));
  }
  return new_stmt;
}

/* ====================================================================
 * unroll the loop that many times.  The unroll template (assuming
 * unrolling 4 times) is:
 *
 * original:
 * 	for (i = start; i < end; i += step)
 *	  body(i)
 *
 * unrolled:
 *      trips = (end - start);
 * 	for (i = start; i < start + (trips / 4) * 4;  i += step*4) {
 *	  body(i)
 *	  body(i+step)
 *	  body(i+2*step)
 *	  body(i+3*step)
 *      }
 *	for (i = i; i < end; i += step)
 *	  body(i)
 * ==================================================================== */
void
WN_UNROLL::Unroll(INT unroll_times)
{
  INT i;
  WN *stmt, *new_stmt;

  // change _orig_wn to a BLOCK node
  WN_set_operator(_orig_wn, OPR_BLOCK);
  WN_set_rtype(_orig_wn, MTYPE_V);
  WN_set_desc(_orig_wn, MTYPE_V);
  WN_set_kid_count(_orig_wn, 0);
  WN_first(_orig_wn) = WN_last(_orig_wn) = NULL;
  WN_set_map_id(_orig_wn, (WN_MAP_ID) (-1));

  // form unrolled DO loop
  WN *unrolled_init_stmt = WN_COPY_Tree_With_Map(_init_stmt);

  WN *unrolled_trips;
  BOOL const_trips = 0; // if constant trip count, value is trip count
  if (WN_operator(_trips) == OPR_INTCONST)
    const_trips = WN_const_val(_trips);
  WN *unrolled_end_cond;
  if (const_trips) 
    unrolled_trips = WN_Intconst(_rtype, const_trips / 
    				 (unroll_times * _abs_step_amt) * 
				 (unroll_times * _abs_step_amt));
  else {
    unrolled_trips = WN_Div(_rtype, _trips, WN_Intconst(_rtype, unroll_times*_abs_step_amt));
    unrolled_trips = WN_Mpy(_rtype, unrolled_trips, WN_Intconst(_rtype, unroll_times*_abs_step_amt));
  }
  unrolled_end_cond = WN_Relational(_step_amt > 0 ? OPR_LT : OPR_GT, _rtype, 
			WN_CopyNode(WN_kid0(_end_cond)),
			WN_Binary(_step_amt > 0 ? OPR_ADD : OPR_SUB, _rtype, 
				WN_COPY_Tree_With_Map(_start), unrolled_trips));

  WN *unrolled_incr_stmt = WN_CopyNode(_incr_stmt);
  WN_COPY_All_Maps(unrolled_incr_stmt, _incr_stmt);
  WN_set_map_id(unrolled_incr_stmt, (WN_MAP_ID) (-1));
  WN_kid0(unrolled_incr_stmt) = Replicate_expr(WN_kid0(_incr_stmt), 
					       (unroll_times-1) * _step_amt);

  WN *unrolled_body = WN_CreateBlock();
  for (i = 0; i < unroll_times; i++) {
    for (stmt = WN_first(_loop_body); stmt; stmt = WN_next(stmt)) {
      if (i != 0 && WN_operator(stmt) == OPR_PREFETCH)
	continue;
      new_stmt = Replicate_stmt(stmt, i*_step_amt);
      WN_INSERT_BlockLast(unrolled_body, new_stmt);
    }
  }

  if (_loop_info) {
    WN_loop_trip_est(_loop_info) /= unroll_times;
    WN_Reset_Loop_Nz_Trip(_loop_info);
    if (WN_kid1(_loop_info))
      WN_kid1(_loop_info) = WN_Div(_rtype, WN_kid1(_loop_info),
					   WN_Intconst(_rtype, unroll_times));
  }
  WN *unrolled_do_loop = WN_CreateDO(WN_CopyNode(_indx_var), unrolled_init_stmt,
				     unrolled_end_cond, unrolled_incr_stmt, 
				     unrolled_body, _loop_info);
  WN_set_map_id(unrolled_do_loop, (WN_MAP_ID) (-1));

  WN_INSERT_BlockLast(_orig_wn, unrolled_do_loop);

  if (const_trips && (const_trips % (unroll_times*_abs_step_amt) == 0))
    return;

  // form remainder loop
  if (const_trips)
    WN_kid0(_init_stmt) = WN_Binary(_step_amt > 0 ? OPR_ADD : OPR_SUB, _rtype, 
    	WN_COPY_Tree(_start),
    	WN_Intconst(_rtype, const_trips / (unroll_times * _abs_step_amt) * 
				(unroll_times * _abs_step_amt)));
  else WN_kid0(_init_stmt) = WN_CopyNode(WN_kid0(_end_cond));
  WN *loop_info = WN_CreateLoopInfo(WN_CopyNode(WN_kid0(_end_cond)),
  				    NULL, unroll_times-1, 
				    _loop_info ? WN_loop_depth(_loop_info) : 0, 
				    9);
  WN_INSERT_BlockLast(_orig_wn, 
  		      WN_CreateDO(_indx_var, _init_stmt, _end_cond, _incr_stmt,
				  _loop_body, loop_info));
}

/* ====================================================================
 * The given DO loop is innermost and does not contain any constructs that
 * make it unsuitable for loop unrolling.  Change it into a BLOCK node that
 * contains the statements that represent the unrolled loop and return this
 * same node.  If nothing is done, return NULL.
 * ==================================================================== */
static void
WN_UNROLL_loop(WN *doloop)
{
  WN_UNROLL wn_unroll(doloop);
  if (wn_unroll.Step_amt() == 0) 
    return; // variable step amount
  if (wn_unroll.Loop_info()) {
    if (WN_Loop_Unimportant_Misc(wn_unroll.Loop_info()))
      return;
    if (WN_loop_trip_est(wn_unroll.Loop_info()) <= 16)
      return;
  }
  if (wn_unroll.End() == NULL)
    return; // non-well-formed loop
  if (WN_operator(wn_unroll.Trips()) == OPR_INTCONST)
    if (WN_const_val(wn_unroll.Trips()) <= 16)
      return; // constant trip count too low
  wn_unroll.Analyze_body_stmt(WN_kid(doloop, 4));
  if (WOPT_Enable_WN_Unroll < 2 && wn_unroll.If_count() == 0)
    return; // CG will unroll it
  USRCPOS srcpos;
  USRCPOS_srcpos(srcpos)  = WN_Get_Linenum(doloop);
  INT unroll_times;
  if (wn_unroll.Istore_count() == 0) {
    if (wn_unroll.Node_count() < 40)
      unroll_times = 8;
    else if (wn_unroll.Node_count() < 80)
      unroll_times = 4;
    else {
      if (WOPT_Enable_Verbose)
	fprintf(stderr, "WN_UNROLL: loop at %s:%d not unrolled because node count is %d\n",
		Cur_PU_Name, USRCPOS_linenum(srcpos), wn_unroll.Node_count());
      return;
    }
  }
  else {
    if (wn_unroll.Node_count() < 20)
      unroll_times = 8;
    else if (wn_unroll.Node_count() < 40)
      unroll_times = 4;
    else {
      if (WOPT_Enable_Verbose)
	fprintf(stderr, "WN_UNROLL: loop at %s:%d not unrolled because node count is %d\n",
		Cur_PU_Name, USRCPOS_linenum(srcpos), wn_unroll.Node_count());
      return;
    }
  }
  wn_unroll.Unroll(unroll_times);
  if (WOPT_Enable_Verbose)
    fprintf(stderr, "WN_UNROLL has unrolled loop at %s:%d %d times\n",
	    Cur_PU_Name, USRCPOS_linenum(srcpos), unroll_times);
  	  
}

/* ====================================================================
 * Look for loops to unroll inside the given statement recursively; unroll
 * any DO loops that satisify the unrolling criteria; return TRUE if no
 * construct is found inside that renders any enclosing DO loop as non-candidate
 * for unrolling
 * ==================================================================== */
static BOOL
WN_UNROLL_suitable(WN *tree)
{
  OPERATOR opr = WN_operator(tree);
  INT i;
  BOOL suitable;

  if (OPERATOR_is_store(opr) && MTYPE_is_vector(WN_desc(tree)))
    return FALSE; // do not unroll vectorized code

  switch (opr) {
  case OPR_REGION_EXIT:
  case OPR_GOTO:
  case OPR_GOTO_OUTER_BLOCK:
  case OPR_RETURN:
  case OPR_TRAP	:
  case OPR_FORWARD_BARRIER:
  case OPR_BACKWARD_BARRIER:
  case OPR_ALTENTRY:
  case OPR_LABEL:
  case OPR_DEALLOCA:	
  case OPR_AGOTO:
  case OPR_TRUEBR:
  case OPR_FALSEBR:
  case OPR_RETURN_VAL:
  case OPR_COMPGOTO:
  case OPR_XGOTO:
  case OPR_CALL:          
  case OPR_ICALL:
  case OPR_INTRINSIC_CALL:
  case OPR_PICCALL:
  case OPR_ASM_STMT:
    return FALSE;

  case OPR_COMMENT:
  case OPR_PRAGMA:
  case OPR_PREFETCH:
  case OPR_EVAL: 
  case OPR_ASSERT:
  case OPR_XPRAGMA:
  case OPR_ISTORE:
  case OPR_ISTOREX:
  case OPR_STID:
  case OPR_ISTBITS:
  case OPR_STBITS:
  case OPR_MSTORE:
    return TRUE;

  case OPR_BLOCK: {
      WN *stmt;
      suitable = TRUE;
      stmt = WN_first(tree);
      while (stmt != NULL) {
	if (! WN_UNROLL_suitable(stmt))
	  suitable = FALSE;
	if (WN_operator(stmt) == OPR_BLOCK) { 
	  // the DO_LOOP has been unrolled; integrate its contents to block
	  WN *unrolled_loop_block = stmt;
	  stmt = WN_next(stmt);
	  WN_EXTRACT_FromBlock(tree, unrolled_loop_block);
	  WN_INSERT_BlockBefore(tree, stmt, unrolled_loop_block);
	}
        else stmt = WN_next(stmt);
      }
      return suitable;
    }

  case OPR_DO_WHILE:
  case OPR_WHILE_DO:
    WN_UNROLL_suitable(WN_kid(tree, 1));
    return FALSE;

  case OPR_IF:
    suitable = WN_UNROLL_suitable(WN_kid(tree, 1));
    if (! WN_UNROLL_suitable(WN_kid(tree, 2)))
      suitable = FALSE;
    return suitable;

  case OPR_DO_LOOP:
    if (WN_UNROLL_suitable(WN_kid(tree, 4))) // the block
      WN_UNROLL_loop(tree);
    return FALSE;

  case OPR_REGION:
    WN_UNROLL_suitable(WN_region_body(tree));
    return FALSE;

  default: Is_True(FALSE,("WN_UNROLL_suitable: unexpected operator"));
  }

  return FALSE;
}

/* ====================================================================
 * Top level routine for performing unrolling of innermost loops whose
 * bodies have multiple BBs because they are not unrolled by CG
 * ==================================================================== */
void
WN_unroll(WN *tree)
{
  if (WOPT_Enable_WN_Unroll == 0)
    return;

  Start_Timer(T_Lower_CU);
  Set_Error_Phase("WN_unroll");

  if (WN_operator(tree) == OPR_FUNC_ENTRY) 
    WN_UNROLL_suitable(WN_func_body(tree));
  else if (WN_operator(tree) == OPR_REGION) 
    WN_UNROLL_suitable(WN_region_body(tree));
  else if (OPERATOR_is_stmt(WN_operator(tree)) || OPERATOR_is_scf(WN_operator(tree)))
    WN_UNROLL_suitable(tree);
  else Is_True(FALSE, ("WN_unroll: unexpected WHIRL operator"));

  Stop_Timer(T_Lower_CU);

  WN_Lower_Checkdump("After wn_unroll", tree, 0);   

  WN_verifier(tree);

  return;
}

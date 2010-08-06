/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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




#ifndef wn_lower_INCLUDED
#define wn_lower_INCLUDED

#include "opt_alias_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note: LOWER_ACTIONS isn't an enum since we want to combine actions
 *       with |, which isn't allowed for enums under C++.
 */
typedef INT64 LOWER_ACTIONS;

#define LOWER_NULL		  0x000000000000ll
#define LOWER_DO_LOOP		  0x000000000001ll
#define LOWER_DO_WHILE		  0x000000000002ll
#define LOWER_WHILE_DO		  0x000000000004ll
#define LOWER_IF		  0x000000000008ll
#define LOWER_COMPLEX		  0x000000000010ll
#define LOWER_ARRAY		  0x000000000020ll
#define LOWER_SPLIT_CONST_OFFSETS 0x000000000040ll
#define LOWER_ENTRY_EXIT	  0x000000000080ll
#define LOWER_CALL		  0x000000000100ll
#define LOWER_SPLIT_SYM_ADDRS	  0x000000000200ll
#define LOWER_IO_STATEMENT	  0x000000000400ll
#define LOWER_MSTORE		  0x000000000800ll
#define LOWER_CVT		  0x000000001000ll
#define LOWER_MP		  0x000000002000ll
#define LOWER_8X_ARRAY		  0x000000004000ll
#define LOWER_REGION		  0x000000008000ll
#define LOWER_QUAD		  0x000000010000ll
#define LOWER_COMPGOTO		  0x000000020000ll
#define LOWER_MADD		  0x000000040000ll
#define LOWER_INTRINSIC		  0x000000080000ll
#define LOWER_INLINE_INTRINSIC	  0x000000100000ll
#define LOWER_TOP_LEVEL_ONLY	  0x000000200000ll
#define LOWER_REGION_EXITS	  0x000000400000ll
#define LOWER_INL_STACK_INTRINSIC 0x000000800000ll
#define LOWER_PREFETCH_MAPS	  0x000001000000ll
#define LOWER_ALIAS_MAPS	  0x000002000000ll
#define LOWER_DEPGRAPH_MAPS	  0x000004000000ll
#define LOWER_PARITY_MAPS	  0x000008000000ll
#define LOWER_PICCALL	          0x000010000000ll
#define LOWER_BASE_INDEX	  0x000020000000ll
#define LOWER_ASSERT		  0x000040000000ll
#define LOWER_FREQUENCY_MAPS	  0x000080000000ll
#define LOWER_FORMAL_REF	  0x000100000000ll
#define LOWER_UPLEVEL		  0x000200000000ll
#define LOWER_ENTRY_FORMAL_REF	  0x000400000000ll
#define LOWER_SHORTCIRCUIT	  0x000800000000ll
#define LOWER_TREEHEIGHT	  0x001000000000ll
#define LOWER_RETURN_VAL	  0x002000000000ll
#define LOWER_MLDID_MSTID	  0x004000000000ll
#define LOWER_BIT_FIELD_ID	  0x008000000000ll
#define LOWER_BITS_OP		  0x010000000000ll
#ifdef TARG_ST
#define LOWER_DOUBLE              0x020000000000ll
#define LOWER_LONGLONG            0x040000000000ll
#define LOWER_ENTRY_PROMOTED      0x080000000000ll
#define LOWER_NESTED_FN_PTRS      0x100000000000ll
#define LOWER_INSTRUMENT          0x200000000000ll
#define LOWER_CNST_DIV		  0x400000000000ll
#define LOWER_TO_CG 		  0x800000000000ll
#define LOWER_FORMAL_HIDDEN_REF  0x1000000000000ll
#define LOWER_SELECT		 0x2000000000000ll
#define LOWER_FAST_DIV		 0x4000000000000ll
#define LOWER_FAST_MUL		 0x8000000000000ll
#define LOWER_CNST_MUL		0x10000000000000ll
#ifdef TARG_STxP70
#define LOWER_FARCALL		0x20000000000000ll
#endif

#define LOWER_RUNTIME		0x40000000000000ll
#define LOWER_HILO (LOWER_DOUBLE|LOWER_LONGLONG)
#else

#if defined(TARG_IA32) || defined(TARG_X8664)
#define LOWER_SLINK_SAVE	  0x020000000000ll
#endif
#define LOWER_TO_MEMLIB           0x040000000000ll
#define LOWER_FAST_EXP            0x080000000000ll

#define LOWER_TO_CG		  0x800000000000ll

#endif



/* 
 * LOWER_TOP_LEVEL_ONLY applies to scf nodes.  If it's set, we don't
 * call the lowerer recursively 
 */

/*
 * LOWER_REGION removes the hierarchy from the tree but
 * preserves information by keeping the region id (RID) for
 * the nearest enclosing REGION in the RID_map of each statement.
 * We may change the interface to CG so that this lowering is
 * not required.  We use any call to WN_Lower
 * to compute the REGION_tree which is kept in the parent, first_kid,
 * next, last_kid fields of the RID.  This tree could also be computed
 * on the fly when the optimizer is emitting REGION nodes.
 */

#define LOWER_SCF	(LOWER_DO_LOOP | LOWER_DO_WHILE | LOWER_WHILE_DO | LOWER_IF)

#define LOWER_ALL_MAPS	(LOWER_PREFETCH_MAPS | LOWER_ALIAS_MAPS | LOWER_DEPGRAPH_MAPS | LOWER_PARITY_MAPS | LOWER_FREQUENCY_MAPS)

/*
 * During map lowering (if enabled) complex  [real, imag] and quad [hi, lo]
 * are marked using these qualities
 *
 * This allows the CG dep graph builder (or LNO, wopt alias analysis) to
 * disambiguate using these qualities.
 *
 */
#ifdef TARG_ST
typedef enum
{
  PARITY_UNKNOWN=       ~0x0,
  PARITY_COMPLEX_REAL=  0x01,
  PARITY_COMPLEX_IMAG=  0x02,
  PARITY_QUAD_HI=       0x04,
  PARITY_QUAD_LO=       0x08,
  PARITY_DOUBLE_HI=     0x10,
  PARITY_DOUBLE_LO=     0x20,
  PARITY_LONGLONG_HI=   0x40,
  PARITY_LONGLONG_LO=   0x80
} PARITY;
#else
typedef enum
{
  PARITY_UNKNOWN=       ~0x0,
  PARITY_COMPLEX_REAL=  0x01,
  PARITY_COMPLEX_IMAG=  0x02,
  PARITY_QUAD_HI=       0x04,
  PARITY_QUAD_LO=       0x08
} PARITY;
#endif

/* return the parity associated with a WN */
extern PARITY WN_parity(WN *tree);

/* return true if parity shows independence */
extern BOOL WN_parity_independent(WN *wn1, WN *wn2);

/*
 * lowering specific initialization
 */
extern void Lower_Init(void);
extern void Lowering_Finalize(void);

extern FLD_HANDLE FLD_And_Offset_From_Field_Id (TY_IDX  struct_ty_idx, 
					        UINT    field_id, 
					        UINT&   cur_field_id,
					        UINT64& offset);

extern WN *WN_Lower(WN *tree, LOWER_ACTIONS actions, struct ALIAS_MANAGER *alias, const char *msg);

/*
 * lower an scf node but not things underneath it 
 */
extern WN *lower_scf_non_recursive(WN *block, WN *tree, LOWER_ACTIONS actions);

/* 
 * lower a block node and everything underneath it  
 */
extern WN *lower_block(WN *tree, LOWER_ACTIONS actions);

/*
 * character string for action
 */
extern const char *LOWER_ACTIONS_name(LOWER_ACTIONS action);

#ifdef __cplusplus
}
#endif

/*
 * return alignment consistant with offset and alignment
 */
extern UINT32 compute_offset_alignment(INT32 offset, UINT32 align);

/*
 * return alignment consistant with TY, ST and offset
 */
extern TY *compute_alignment_type(WN *tree, TY *type, INT64 offset);

/*
 * check trace flags and generate appropriate dumps
 */
extern void WN_Lower_Checkdump(const char *msg, WN *tree, LOWER_ACTIONS actions);

/*
 * lower M or lower WHIRL to conform to unsigned 64-bit instruction only ISA
 */
extern void U64_lower_wn(WN *, BOOL);

#ifdef TARG_ST
/*
 * Lower L WHIRL to handle operators not directly supported in the
 * target ISA
 */
BE_EXPORTED extern WN *EXT_lower_wn(WN *tree, BOOL last_pass);
BE_EXPORTED extern void RT_lower_wn(WN *tree);
BE_EXPORTED extern void HILO_lower_wn(WN *wn, WN **lopart, WN **hipart);

/* ====================================================================
 * Some support functions available locally to be/com for
 * lowering or instrument phases.
 * ====================================================================
 */
extern BOOL stmt_is_store_of_return_value(WN *stmt);

extern BOOL expr_loads_dedicated_preg(WN *expr);

extern BOOL stmt_is_store_of_callee_return_value(WN *stmt);

#endif

#ifdef KEY
extern void WN_retype_expr(WN *);
extern WN* Transform_To_Memcpy(WN *dst, WN *src, INT32 offset, TY_IDX dstTY, TY_IDX srcTY, WN *size);
#endif

/* ====================================================================
 * Assignment Support
 * ====================================================================
 */
extern void AssignPregName(const char *part1, const char *part2 = NULL);
extern char *CurrentPregName();
extern void ResetPregName(void);
extern PREG_NUM AssignPregExprPos(WN *block, WN *tree, TY_IDX ty,
				  SRCPOS srcpos, LOWER_ACTIONS actions);

extern PREG_NUM AssignExprTY(WN *block, WN *tree, TY_IDX type);
extern PREG_NUM AssignExpr(WN *block, WN *tree, TYPE_ID type);

extern void AssignToPregExprPos(PREG_NUM pregNo, 
				WN *block, WN *tree, TY_IDX ty,
				SRCPOS srcpos, LOWER_ACTIONS actions);
extern void
AssignToPregExprTY(PREG_NUM pregNo, WN *block, WN *tree, TY_IDX type);
extern void
AssignToPregExpr(PREG_NUM pregNo, WN *block, WN *tree, TYPE_ID type);
     

/* ==================================================================== */


/* ====================================================================
 * LEAF WHIRL Tree Support
 * ====================================================================
 */
typedef enum { LEAF_IS_CONST, LEAF_IS_INTCONST, LEAF_IS_PREG} LEAF_KIND;
typedef struct {
  LEAF_KIND kind;
  TYPE_ID type;
  union {
    PREG_NUM n;
    INT64 intval;
    TCON tc;
  } u;
} LEAF;

extern LEAF Make_Leaf(WN *block, WN *tree, TYPE_ID type);
extern WN *Load_Leaf(const LEAF &leaf);

/* ==================================================================== */

/* ====================================================================
 * WHIRL Tree Queries
 * ====================================================================
 */

extern BOOL Is_Intconst_Val(WN *tree);
extern INT64 Get_Intconst_Val(WN *tree);

extern BOOL WN_Is_Emulated(WN *tree);
extern BOOL WN_Is_Emulated_Type (TYPE_ID type);
extern BOOL WN_Is_Emulated_Operator (OPERATOR opr, TYPE_ID rtype, TYPE_ID desc);
extern BOOL WN_Madd_Allowed (TYPE_ID type);
extern BOOL WN_LDBITS_Allowed (TYPE_ID type);
extern BOOL WN_STBITS_Allowed (TYPE_ID type);


/* ==================================================================== */
#endif /* wn_lower_INCLUDED */

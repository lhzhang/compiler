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
 *   Dumps the current region into a minir representation.
 *
 * ====================================================================
 * ====================================================================
 */

#ifndef cg_minir_INCLUDED
#define cg_minir_INCLUDED

extern void CG_Dump_Minir(INT32 pass_num, const char *pass_string, std::ostringstream *oss);
extern void MINIR_Dump_PU( ST *pu, DST_IDX pu_dst, WN *rwn );
extern const char * BB_Name(BB *bb);
extern void CG_Dump_Minir_BB(BB *bb, const char *pfx, std::ostringstream *oss);
extern const char *BB_Name(BB *bb);
#endif /* cg_minir_INCLUDED */



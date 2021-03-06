/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


#define __STDC_LIMIT_MACROS
#include <stdint.h>
#ifdef USE_PCH
#include "lno_pch.h"
#endif // USE_PCH
#pragma hdrstop

#include "lnopt_main.h"
#include "dnf.h"


LINEAR_CLAUSE::LINEAR_CLAUSE(SYSTEM_OF_EQUATIONS *soe,
			     MEM_POOL *pool)
{
  _t = CLAUSE_ATOM;
  _m = pool;
  _u._atom = CXX_NEW (SYSTEM_OF_EQUATIONS (soe, pool), pool);
}

LINEAR_CLAUSE::LINEAR_CLAUSE(SYSTEM_OF_EQUATIONS **soe_list,
			     INT32 count,
			     MEM_POOL *pool)
{
  INT32 i;

  _t = CLAUSE_DISJ;
  _m = pool;
  _u._disj._conj = CXX_NEW_ARRAY (SYSTEM_OF_EQUATIONS *,
				  count, pool);
  _u._disj._nconj = count;
  for (i = 0; i < count; i++) {
    _u._disj._conj[i] = 
      CXX_NEW (SYSTEM_OF_EQUATIONS (soe_list[i], pool),
	       pool);
  }
}

LINEAR_CLAUSE*
_xcombine_atom_with_disj(LINEAR_CLAUSE *l1,
			 LINEAR_CLAUSE *l2)
{
  assert (CLAUSE_ATOM == l1->CLAUSE_type());
  assert (CLAUSE_DISJ == l2->CLAUSE_type());
  LINEAR_CLAUSE *result;
  SYSTEM_OF_EQUATIONS **soe_list;
  INT32 i;
  
  soe_list = CXX_NEW_ARRAY (SYSTEM_OF_EQUATIONS*, 
			    l2->CLAUSE_nconj(),
			    l2->CLAUSE_mem_pool());
  
  for (i = 0; i < l2->CLAUSE_nconj(); i++) {
    soe_list[i] = l2->CLAUSE_conj_ith (i);
  }
  result = CXX_NEW (LINEAR_CLAUSE (soe_list,
				   l2->CLAUSE_nconj (),
				   l2->CLAUSE_mem_pool ()),
		    l2->CLAUSE_mem_pool());
  assert (result->CLAUSE_nconj() == l2->CLAUSE_nconj());
  for (i = 0; i < result->CLAUSE_nconj (); i++) {
    result->CLAUSE_conj_ith (i)->Add_Soe (l1->CLAUSE_atom());
  }
  return result;
}

LINEAR_CLAUSE*
_xcombine_disj_with_disj(LINEAR_CLAUSE *l1,
			 LINEAR_CLAUSE *l2)
{
  assert (CLAUSE_DISJ == l1->CLAUSE_type());
  assert (CLAUSE_DISJ == l2->CLAUSE_type());

  LINEAR_CLAUSE         *result;
  SYSTEM_OF_EQUATIONS  **soe_list;
  SYSTEM_OF_EQUATIONS   *tmp;
  INT32 i, j;
  INT32 count;

  count = 0;
  /* We do this as follows: We first make an soe_list of conjs
     of l1 only. We repeat them as often as there are conjuncts 
     of l2. We create the require result, and then we'll add soe
     s from l2 in precisely the cyclical way we stepped before */

  soe_list = CXX_NEW_ARRAY (SYSTEM_OF_EQUATIONS*,
			    l1->CLAUSE_nconj() * l2->CLAUSE_nconj(),
			    l2->CLAUSE_mem_pool());
  for (i = 0; i < l1->CLAUSE_nconj (); i++) {
    for (j = 0; j < l2->CLAUSE_nconj (); j++) {
      soe_list[count] = 
	CXX_NEW (SYSTEM_OF_EQUATIONS (l1->CLAUSE_conj_ith (i),
				      l1->CLAUSE_mem_pool()),
		 l1->CLAUSE_mem_pool());
      count++;
    }
  }
  assert (count == l1->CLAUSE_nconj () * l2->CLAUSE_nconj ());
  result = CXX_NEW (LINEAR_CLAUSE (soe_list, count,
				   l2->CLAUSE_mem_pool()),
		    l2->CLAUSE_mem_pool());
  count = 0;
  for (i = 0; i < l1->CLAUSE_nconj (); i++) {
    for (j = 0; j < l2->CLAUSE_nconj (); j++) {
      result->CLAUSE_conj_ith(count)->Add_Soe(l2->CLAUSE_conj_ith(j));
      count++;
    }
  }
  assert (count == l1->CLAUSE_nconj () * l2->CLAUSE_nconj ());
  return result;
}

LINEAR_CLAUSE*
combine_clauses(LINEAR_CLAUSE *l1,
		LINEAR_CLAUSE *l2)
{
  LINEAR_CLAUSE *result;
  /* We have four cases */
  if (CLAUSE_ATOM == (l1->CLAUSE_type()) &&
      CLAUSE_ATOM == (l2->CLAUSE_type())) {
    result = CXX_NEW (LINEAR_CLAUSE (l1->CLAUSE_atom(),
				     l1->CLAUSE_mem_pool()),
		      l1->CLAUSE_mem_pool());
    result->CLAUSE_atom()->Add_Soe (l2->CLAUSE_atom());
    return result;
  }
  else if (CLAUSE_ATOM == (l1->CLAUSE_type()) &&
	   CLAUSE_DISJ == (l2->CLAUSE_type())) {
    return _xcombine_atom_with_disj (l1, l2);
  }
  else if (CLAUSE_DISJ == (l1->CLAUSE_type()) &&
	   CLAUSE_ATOM == (l2->CLAUSE_type())) {
    return _xcombine_atom_with_disj (l2, l1);
  }
  else 
    return _xcombine_disj_with_disj (l1, l2);
}

BOOL
LINEAR_CLAUSE::Is_Consistent()
{
  INT32 i;

  switch (_t) {
  case CLAUSE_ATOM:
    return _u._atom->Is_Consistent();
  case CLAUSE_DISJ:
    for (i = 0; i < _u._disj._nconj; i++) {
      if (_u._disj._conj[i]->Is_Consistent())
	return TRUE;
    }
    return FALSE;
  default:
    assert(0);
    return FALSE;
  }
}

void 
LINEAR_CLAUSE::Print(FILE *fp) const
{
  INT32 i;

  switch (_t) {
  case CLAUSE_ATOM:
    _u._atom->Print(fp);
    break;
  case CLAUSE_DISJ:
    fprintf(fp, "[[[");
    for (i = 0; i < _u._disj._nconj; i++) {
      fprintf (fp, "\t{{");
      _u._disj._conj[i]->Print (fp);
      fprintf (fp, "\t}}\n");
    }
    fprintf(fp, "]]]\n");
    break;
  }
}

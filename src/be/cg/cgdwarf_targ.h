/*
 *  Copyright (C) 2007. QLogic Corporation. All Rights Reserved.
 */
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


#ifndef cgdwarf_targ_INCLUDED
#define cgdwarf_targ_INCLUDED

extern BOOL Trace_Dwarf;

extern void Check_Dwarf_Rel(const Elf32_Rel &);
extern void Check_Dwarf_Rel(const Elf64_AltRel &);
extern void Check_Dwarf_Rela(const Elf64_AltRela &);
extern void Check_Dwarf_Rela(const Elf32_Rela &);
extern BOOL Is_Dwarf_Section_To_Emit(const char *name);
extern BOOL Dwarf_Require_Symbolic_Offsets();

extern void Init_Unwind_Info (BOOL trace);
extern void Finalize_Unwind_Info(void);
extern void Emit_Unwind_Directives_For_OP(OP *op, FILE *f);

#endif /* cgdwarf_targ_INCLUDED */

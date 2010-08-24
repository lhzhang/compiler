/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

   Copyright (C) STMicroelectronics All Rights Reserved.

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

*/

#ifndef __PROCESS_H__
#define __PROCESS_H__


extern string tmpdir;

extern void dump_argv (string *);

extern int do_compile (string *);

extern void ld_kill_compilation (int);

extern void add_to_tmp_file_list (string);

extern void cleanup_all_files (void);

extern string make_temp_file (string, char);

extern int create_tmpdir ( int tracing );

extern string create_unique_file (const string, char);

extern string create_tmp_file (const string);

extern string *get_command_line (an_object_file_ptr, string, string, int *);

extern int make_link (const string, const string);

extern string ld_compile (an_object_file_ptr);

extern char * always_demangle(char *, char );

extern char *__Release_ID;


#endif /* __PROCESS_H__ */

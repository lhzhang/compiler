/*
   Copyright (C) 2010 PathScale Inc. All Rights Reserved.
*/
/*
 * Copyright (C) 2006, 2007. PathScale Inc. All Rights Reserved.
 */

/*

   Path64 is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation version 3

   Path64 is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Path64; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

#include "ipa_command_line.h"
#include "ipa_symbols_sync.h"

extern char ** ipa_argv; //defined in ipa_command_line.cxx

int main (int argc, char **argv, char **envp){
  int ipa_argc;

  ipa_argc = ipa_search_command_line(argc, argv, envp);

  ipa_symbol_sync();
  (*p_ipa_driver) (ipa_argc, ipa_argv);
}

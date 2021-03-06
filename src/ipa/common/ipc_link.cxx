/*
 * Copyright (C) 2007, 2008. PathScale, LLC.  All Rights Reserved.
 */

/*
 * Copyright (C) 2006, 2007. QLogic Corporation. All Rights Reserved.
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


#include <sys/types.h>
#include <sys/stat.h>
#include "main.h"			// for arg_vector[0]
#include "ipc_weak.h"			// weak definition for arg_vector 

#include "defs.h"
#include "errors.h"                     // for ErrMsg
#include "erglob.h"                     // error code
#include "cxx_memory.h"			// for CXX_NEW
#ifdef KEY
#include "config.h"			// for IPA_lang
#endif
#include "ipc_link.h"

#include "lib_phase_dir.h"              // for BINDIR etc

// We break up the linker command line into two parts. with the list of
// WHIRL object created by IPA inserted in between.  Sine we no longer
// maintain a 1-1 relationship between the WHIRL .o's and .I's, we don't
// know the names of the .I's until after they are created
ARGV *ld_flags_part1;			// argv before the first WHIRL obj
ARGV *ld_flags_part2;			// rest of the link line
ARGV *current_ld_flags;

// For archives, we create a comma-separated list for the set of member
// objects selected.

ARGV *comma_list;
UINT32 comma_list_byte_count = 0;

#if defined(TARG_IA64) || defined(TARG_X8664) || defined(TARG_MIPS)

#if defined(TARG_X8664) || defined(ARCH_MIPS)
#define LINKER_NAME "gcc"
#define LINKER_NAME_WITH_SLASH "/gcc"
#define CC_LINKER_NAME "g++"
#define CC_LINKER_NAME_WITH_SLASH "/g++"
#elif defined(TARG_MIPS)
#define LINKER_NAME "mips64el-gentoo-linux-gnu-gcc"
#define LINKER_NAME_WITH_SLASH "/usr/bin/mips64el-gentoo-linux-gnu-gcc"
#define CC_LINKER_NAME "mips64el-gentoo-linux-gnu-g++"
#define CC_LINKER_NAME_WITH_SLASH "/usr/bin/mips64el-gentoo-linux-gnu-g++"
#else
#error /* unknown cross compiler target */
#endif

static char* concat_names(const char* a , const char* b)
{
    char * buf;
    buf = (char *)malloc(strlen(a)+strlen(b)+1);
    strcpy(buf, a);
    strcat(buf, b);
    return (buf);
}


// Returns true if path refers to an ordinary file.
static bool file_exists(const char* path)
{
  if (!path || strlen(path) == 0)
    return false;

  struct stat buf;
  return stat(path, &buf) == 0 && S_ISREG(buf.st_mode);
}

#ifdef KEY
static const char* get_ipa_lang (int argc, char** argv)
{
  for (INT i=1; i<argc; i++)
  {
    const char* p = argv[i];

    if (!strncmp(p, "-INTERNAL:lang=", 15 ))
    {
      p += 15;
      return p;
    }
  }
  return NULL;
}

static const char* get_ipa_linker (int argc, char** argv)
{
  for (INT i = 1; i < argc; ++i) {
    const char* p = argv[i];
    if (!strncmp(p, "-INTERNAL:linker=", 17)) {
      p += 17;
      return p;
    }
  }
  return NULL;
}
#endif

static const char* get_linker_name(int argc, char** argv)
{
    const char * toolroot = getenv("TOOLROOT");
    if (!toolroot) { toolroot = "" ; }

    char* linker_name;
    char *where_am_i = getenv("COMPILER_BIN");

#ifdef KEY
    // IPA_lang and IPA_linker have not been set yet in IPA command-line
    // processing, so scan the command-line.
    const char * ipa_lang = get_ipa_lang (argc, argv);
    const char * ipa_linker = get_ipa_linker (argc, argv);

    // 14839: Give -INTERNAL:linker= option first priority.
    if (ipa_linker && file_exists(ipa_linker)) {
      return ipa_linker;
    }

    // bug 14342: g++ must be used to link for C++.
    BOOL use_cc_linker = FALSE;
    if (ipa_lang && !strcmp(ipa_lang, "CC"))
      use_cc_linker = TRUE;
#endif

    if (where_am_i) {
	char *slash = strrchr (where_am_i, '/');
#ifdef KEY
	if (use_cc_linker)
	  asprintf (&linker_name, "%.*s/../" PSC_TARGET "/bin/" CC_LINKER_NAME,
		    (int)(slash - where_am_i), where_am_i);
	else
#endif
	  asprintf (&linker_name, "%.*s/../" PSC_TARGET "/bin/" LINKER_NAME,
		    (int)(slash - where_am_i), where_am_i);
	if (file_exists (linker_name)) {
	    return linker_name;
	}
	free (linker_name);
    }

#ifdef KEY
    if (use_cc_linker)
      linker_name = concat_names (toolroot, PHASEPATH CC_LINKER_NAME_WITH_SLASH);
    else
#endif
      linker_name = concat_names (toolroot, PHASEPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);

#ifdef KEY
    if (use_cc_linker)
      linker_name = concat_names (toolroot, GNUPHASEPATH CC_LINKER_NAME_WITH_SLASH);
    else
#endif
      linker_name = concat_names (toolroot, GNUPHASEPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);

#ifdef KEY
    if (use_cc_linker)
      linker_name = concat_names (toolroot, BINPATH CC_LINKER_NAME_WITH_SLASH);
    else
#endif
      linker_name = concat_names (toolroot, BINPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);
        
#ifdef KEY
    if (use_cc_linker)
      linker_name = concat_names (toolroot, ALTBINPATH CC_LINKER_NAME_WITH_SLASH);
    else
#endif
      linker_name = concat_names (toolroot, ALTBINPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);

#ifdef KEY
    if (use_cc_linker)
      return (CC_LINKER_NAME);
    else
#endif
      return (LINKER_NAME);
}

#endif

void
ipa_init_link_line (int argc, char** argv)
{
    ld_flags_part1 = CXX_NEW (ARGV, Malloc_Mem_Pool);
    ld_flags_part2 = CXX_NEW (ARGV, Malloc_Mem_Pool);
    comma_list = CXX_NEW (ARGV, Malloc_Mem_Pool);

    // Push the path and name of the final link tool
#if defined(TARG_IA64) || defined(TARG_X8664) || defined(TARG_MIPS)

#if 0
    char *t_path = arg_vector[0];
    char *t_name;
    char *buf;
    int len = strlen(t_path);
    int i,new_len;
    
    t_name = t_path + (len-1);
    for (i=0; i<len; i++,t_name--) {
    	if (*t_name == '/')
	    break;
    }
    new_len = len-i;
    buf = (char *)malloc(strlen(LINKER_NAME)+new_len+1);
    
    strncpy(buf,t_path,new_len);
    strcat(buf,LINKER_NAME);
    ld_flags_part1->push_back (buf);
    
    free(buf);
#endif
    ld_flags_part1->push_back (get_linker_name(arg_count, arg_vector));
#ifdef TARG_IA64
    ld_flags_part1->push_back (DYNAMIC_LINKER);
#endif

#else
    ld_flags_part1->push_back (arg_vector[0]);
#endif

    // Copy the arguments that have been saved
    for (INT i = 0; i < argc; ++i) {
	ld_flags_part1->push_back (argv[i]);
    }
    current_ld_flags = ld_flags_part1;
} // ipa_init_link_line


void
ipa_add_link_flag (const char* str)
{
    current_ld_flags->push_back (str);
} // ipa_add_link_flag

void
ipa_modify_link_flag (char* lname, char* fname)
{
    ARGV::iterator i;
    for(i = ld_flags_part1->begin(); i != ld_flags_part1->end(); i++) {
        if(!strcmp(lname, *i)) {
            ld_flags_part1->erase(i);
            ld_flags_part1->insert(i, fname);
        }
    }
    for(i = ld_flags_part2->begin(); i != ld_flags_part2->end();i++) {
        if(!strcmp(lname, *i)) {
            ld_flags_part2->erase(i);
            ld_flags_part2->insert(i, fname);
        }
    }
} // ipa_modify_link_flag



#ifdef KEY
extern "C" void
ipa_erase_link_flag (const char* str)
{
  ARGV::iterator p;
  bool changed; /* bug 9772 */

  do {
    changed = FALSE;
    for (p = ld_flags_part1->begin();
	 p != ld_flags_part1->end();
	 p++) {
      if (!strcmp(str, *p)) {
	ld_flags_part1->erase(p);
	changed = TRUE;
	break;
      }
    }
  } while (changed);

  do {
    changed = FALSE;
    for (p = ld_flags_part2->begin();
	 p != ld_flags_part2->end();
	 p++) {
      if (!strcmp(str, *p)) {
	ld_flags_part2->erase(p);
	changed = TRUE;
	break;
      }
    }
  } while (changed);
} // ipa_erase_link_flag
#endif


// mark where the list of .I's should be inserted
void
ipa_insert_whirl_obj_marker ()
{
    current_ld_flags = ld_flags_part2;
}

// this is just to let C programs access it too.
extern "C" void 
ipa_insert_whirl_marker(void)
{
    ipa_insert_whirl_obj_marker();
}

void
ipa_add_comma_list (const char* name)
{
    comma_list->push_back (name);
    comma_list_byte_count += strlen(name) + 1;
}

void
ipa_compose_comma_list (const char* name)
{
    if (comma_list->empty ())
	return;

    char* result = (char *) MEM_POOL_Alloc (Malloc_Mem_Pool,
					    comma_list_byte_count);
    ARGV::const_iterator first = comma_list->begin ();

    strcpy (result, *first);
    free ((void*) *first++);		// storage for this string was
					// allocated by ld using malloc, so 
					// don't use MEM_POOL_FREE
    while (first != comma_list->end ()) {
	strcat (result, ",");
	strcat (result, *first);
	free ((void*) *first);
	++first;
    }

    comma_list_byte_count = 0;
    comma_list->erase (comma_list->begin(), comma_list->end());

    ipa_add_link_flag ("-ar_members");
    ipa_add_link_flag (result);

    char* p = (char *) MEM_POOL_Alloc (Malloc_Mem_Pool, strlen(name) + 1);
    strcpy (p, name);
    ipa_add_link_flag (p);

} // ipa_compose_comma_list


ARGV *
ipa_link_line_argv (const ARGV* output_files,
                    const char* dir, 
		    const char* symtab_name)
{
    ARGV* argv = CXX_NEW (ARGV, Malloc_Mem_Pool);

    argv->reserve (ld_flags_part1->size () + 
    	    	   ld_flags_part2->size () +
		   output_files->size () + 
		   1);

    argv->insert (argv->end (),
		  ld_flags_part1->begin (), 
		  ld_flags_part1->end ());
    
    argv->insert (argv->end (),
		  output_files->begin (), 
		  output_files->end ());

    if (symtab_name && symtab_name[0] != 0) {
	argv->push_back(symtab_name);
    }
    
    argv->insert (argv->end (),
		  ld_flags_part2->begin (), 
		  ld_flags_part2->end ());

    return argv;
} // ipa_link_line_argv

#ifdef TODO
void
process_cord_obj_list (FILE *f)
{
    int i;

    for (i = 0; i < cur_argc; ++i) {
	if (arg_tag[i] == ARG_OBJ) {
	    fputs (arg_list[i].obj->other_name ?
		   arg_list[i].obj->other_name : arg_list[i].obj->name, f);
	    fputc ('\n', f);
	}
    }
} /* process_cord_obj_list */
#endif 

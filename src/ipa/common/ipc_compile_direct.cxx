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

#include <vector>       
#include <string>

#include <ext/hash_map> 

#include <time.h>       
#include <limits.h>
#include <sys/param.h>  

#include "cxx_memory.h" 
#include "erglob.h"     
#include "ipa_option.h" 
#include "ipc_file.h"   
#include "ipc_link.h"   
#include "ipc_weak.h"
#include "linker.h"     
#include "main_defs.h"
#include "opcode.h"     
#include "process.h"    

#include "wn_core.h"    
#include "pu_info.h"    
#include "ir_bread.h"   

#include "ipc_compile_run.h"
#include "ipc_compile_helper.h"

using std::vector;
using __gnu_cxx::hash_map;
extern char *psclp_arg;

// To create an executable, we have to 
// -- Compile the symtab file to a .o file (an elf symtab)
// -- Compile the symtab to a .G file
// -- Using the .G file, compile each of the regular whirl files
//    (each file that contains a pu) to an elf .o file
// -- Link all of the .o files.

// For each of the regular whirl files, we keep track of 
// -- its name
// -- the command line we'll use to compile it

// How to use these functions:
// (1) Initialize with ipa_compile_init.
// (2) Call ipacom_process_symtab, where the argument is the symtab file
//     name.  (e.g. "symtab.I")
// (3) For each other whirl file:
//     (a) call ipacom_process_file
//     (b) call ipacom_add_comment zero or more times, using the 
//         index that ipacom_process_file returned.
// (4) call ipacom_doit.  ipacom_doit never returns.


// --------------------------------------------------------------------------
// File-local variables

static vector<const char*>* infiles = 0;
static vector<const char*>* outfiles = 0;
static vector<const char*>* outfiles_fullpath = 0;
static vector<vector<std::string> >* commands = 0;
static vector<UINT32>* ProMP_Idx = 0;

// Name of the symtab file as written by the ipa.  (e.g. symtab.I)
static char input_symtab_name[PATH_MAX] = "";

// Name of the symtab file that will be used to compile other whirl
// files. (e.g. symtab.G)
static char whirl_symtab_name[PATH_MAX] = "";

// Name of the elf symtab file. (e.g. symtab.o)
static char elf_symtab_name[PATH_MAX] = "";

// Command line for producing the whirl and elf symtab files.  
static vector<std::string>* symtab_command_line = 0;
static vector<std::string>* symtab_extra_args = 0;


// Map from short forms of command names (e.g. "cc") to full 
// forms (e.g. "/usr/bin/cc").

namespace {
  struct eqstr {
    bool operator()(const char* s1, const char* s2) const
      { return strcmp(s1, s2) == 0; }
  };
  typedef __gnu_cxx::hash_map<const char*, const char*, __gnu_cxx::hash<const char*>, eqstr>
          COMMAND_MAP_TYPE;
}

static COMMAND_MAP_TYPE* command_map;


static vector<std::string> get_extra_args(const char* ipaa_filename);
static void get_extra_symtab_args(const ARGV&);

extern "C" void
ipa_compile_init ()
{ 
  Is_True(tmpdir, ("no IPA temp. directory"));

  Is_True(infiles == 0 && outfiles == 0 && commands == 0 
          && makefile_name == 0 && makefile == 0 && command_map == 0,
          ("ipa_compile_init already initialized"));

  infiles             = CXX_NEW (vector<const char*>,          Malloc_Mem_Pool);
  outfiles            = CXX_NEW (vector<const char*>,          Malloc_Mem_Pool);
  outfiles_fullpath   = CXX_NEW (vector<const char*>,          Malloc_Mem_Pool);
  commands            = CXX_NEW (vector<vector<std::string> >,          Malloc_Mem_Pool);
  symtab_command_line = CXX_NEW (vector<std::string>,          Malloc_Mem_Pool);
  symtab_extra_args   = CXX_NEW (vector<std::string>,          Malloc_Mem_Pool);

  if (infiles == 0 || outfiles == 0 || outfiles_fullpath == 0 ||
      commands == 0)
    ErrMsg (EC_No_Mem, "ipa_compile_init");

  if (ProMP_Listing)
      ProMP_Idx = CXX_NEW (vector<UINT32>, Malloc_Mem_Pool);

  command_map = CXX_NEW(COMMAND_MAP_TYPE, Malloc_Mem_Pool);
  if (command_map == 0)
    ErrMsg (EC_No_Mem, "ipa_compile_init");

  const char* toolroot = getenv("TOOLROOT");

  static const char* tmp_cc_name_base = PSC_INSTALL_PREFIX "/bin/" PSC_NAME_PREFIX "cc";
  static const char* cc_name_base = tmp_cc_name_base;
  static char my_cc[MAXPATHLEN];
  char *where_am_i = getenv("COMPILER_BIN");
  int retval;

  if (where_am_i) {
    tmp_cc_name_base = where_am_i;
    cc_name_base = where_am_i;
  }

  if (my_cc[0] == '\0' &&
      (retval = readlink ("/proc/self/exe", my_cc, sizeof(my_cc))) >= 0) {

      my_cc[retval] = '\0';	// readlink doesn't append NULL

      if (ipc_looks_like (my_cc, PSC_NAME_PREFIX "cc") ||
	  ipc_looks_like (my_cc, PSC_NAME_PREFIX "CC") ||
	  ipc_looks_like (my_cc, PSC_NAME_PREFIX "f90")) {
	  tmp_cc_name_base = my_cc;
	  cc_name_base = my_cc;
      } else if (ipc_looks_like (my_cc, "ipa_link")) {
	  char *s = strrchr(my_cc, '/');
	  if (s) {
	      *s = '\0';		// remove "/ipa_link"
	      s = strrchr(my_cc, '/');
	      *s = '\0';		// remove version number, e.g. "/2.3.99"
	      s = strrchr(my_cc, '/');

	      if (s) {
		  // Invoke the C/C++/Fortran compiler depending on the source
		  // language.  Bug 8620.
		  const char *compiler_name_suffix;
		  if (!strcmp(IPA_lang, "F77") ||
		      !strcmp(IPA_lang, "F90")) {
		    compiler_name_suffix = "f95";
		  } else if (!strcmp(IPA_lang, "C")) {
		    compiler_name_suffix = "cc";
		  } else if (!strcmp(IPA_lang, "CC")) {
		    compiler_name_suffix = "CC";
		  } else {
		    Fail_FmtAssertion ("ipa: unknown language");
		  }
		  strcpy(++s, "bin/" PSC_NAME_PREFIX);
		  s += strlen("bin/" PSC_NAME_PREFIX);
		  strcpy(s, compiler_name_suffix);

		  if (ipc_file_exists (my_cc)) {
		      tmp_cc_name_base = my_cc;
		      cc_name_base = my_cc;
		  }
	      }
	  }
      }
  }
  

  if (toolroot) {
      static char* new_cc_name_base = concat_names((const string)toolroot, (const string)tmp_cc_name_base);
      if (ipc_file_exists(new_cc_name_base))
	  (*command_map)["cc"] = new_cc_name_base;
      else 
	  (*command_map)["cc"] = cc_name_base;
  }
  else if (IPA_cc_name != NULL) {		// Bug 14371.
      (*command_map)["cc"]    = IPA_cc_name;
  }
  else 
      (*command_map)["cc"]    = cc_name_base;

} // ipa_compile_init


// Generate a command line to compile an ordinary whirl file into an object
// file.
static void
get_command_line (const IP_FILE_HDR& hdr, ARGV& argv, const char* inpath,
                  const char* outpath) 
{
  char* base_addr = (char*)
    WN_get_section_base (IP_FILE_HDR_input_map_addr (hdr), WT_COMP_FLAGS);

  if (base_addr == (char*) -1)
    ErrMsg (EC_IR_Scn_Read, "command line", IP_FILE_HDR_file_name (hdr));

  Elf64_Word argc = *((Elf64_Word *) base_addr);
  Elf64_Word* args = (Elf64_Word *) (base_addr + sizeof(Elf64_Word));

  // args[0] is the command, so we need to treat it specially.  If
  // TOOLROOT is set, and if args[0] isn't already an absolute pathname,
  // we need to construct an absolute pathname using TOOLROOT.

  Is_True(command_map != 0
            && command_map->find("cc") != command_map->end()
            && (*command_map)["cc"] != 0
            && strlen((*command_map)["cc"]) != 0,
          ("Full pathname for cc not set up"));

  if (argc > 0) {
    argv.push_back((*command_map)["cc"]);

    for (INT i = 1; i < argc; ++i) {
      argv.push_back (base_addr + args[i]);
    }
  }
  else {
    argv.push_back ((*command_map)["cc"]);
    argv.push_back ("-c");
  }

  argv.push_back(ipc_abi());

  argv.push_back (inpath);
  argv.push_back ("-o");
  argv.push_back (outpath);
  argv.push_back ("-c");
    
} // get_command_line



extern "C" void
ipacom_process_symtab (char* symtab_file)
{

  Is_True(infiles != 0 && outfiles != 0 && outfiles_fullpath != 0 &&
          commands != 0 ,
          ("ipacom_process_symtab: ipacom not yet initialized"));

  Is_True(strlen(input_symtab_name) == 0 &&
          strlen(whirl_symtab_name) == 0 &&
          strlen(elf_symtab_name) == 0 &&
          symtab_command_line->size() == 0,
          ("ipacom_process_symtab: symtab already initialized"));

  char* output_file = create_unique_file (symtab_file, 'o');
  add_to_tmp_file_list (output_file);
  unlink (output_file);

  const char* input_base = ipa_basename(symtab_file);
  const char* output_base = ipa_basename(output_file);

  // Save the three symtab file names in global variables.
  strcpy(input_symtab_name, symtab_file);
  strcpy(elf_symtab_name,   output_file);
  strcpy(whirl_symtab_name, output_file);
  whirl_symtab_name[strlen(whirl_symtab_name) - 1] = 'G';

  // Generate a command line to create the .G and .o files.
  char buf[3*PATH_MAX + 64];

  Is_True(command_map != 0
            && command_map->find("cc") != command_map->end()
            && (*command_map)["cc"] != 0
            && strlen((*command_map)["cc"]) != 0,
          ("Full pathname for cc not set up"));

  std::string toolroot = getenv("TOOLROOT")==0 ? "" : getenv("TOOLROOT");

  symtab_command_line->push_back(toolroot + (*command_map)["cc"]);
  symtab_command_line->push_back("-c");
  symtab_command_line->push_back(ipc_abi());
  symtab_command_line->push_back(input_symtab_name);
  symtab_command_line->push_back("-o");
  symtab_command_line->push_back(elf_symtab_name);
  symtab_command_line->push_back(std::string("-TENV:emit_global_data=")+whirl_symtab_name);
  if(IPA_Enable_AutoGnum){
    symtab_command_line->push_back("-Gspace");
    symtab_command_line->push_back("0");
  }

  Is_True(strlen(input_symtab_name) != 0 &&
          strlen(whirl_symtab_name) != 0 &&
          strlen(elf_symtab_name) != 0 &&
          symtab_command_line->size() != 0,
          ("ipacom_process_symtab: initialization failed"));
} // ipacom_process_symtab

// The return value is the index of this file in the vectors.
extern "C" 
size_t ipacom_process_file (char* input_file,
                            const PU_Info* pu, UINT32 ProMP_id)
{
  Is_True(infiles != 0 && outfiles_fullpath != 0 && commands != 0,
          ("ipacom_process_file: ipacom not initialized"));

  Is_True(strlen(input_symtab_name) != 0 &&
          strlen(whirl_symtab_name) != 0 &&
          strlen(elf_symtab_name) != 0 &&
          symtab_command_line->size() != 0,
          ("ipacom_process_file: symtab not initialized"));

  if (ProMP_Listing) {
      Is_True (ProMP_Idx != 0,
	       ("ipacom_process_file: ipacom not initialized"));
      ProMP_Idx->push_back (ProMP_id);
  }

  char* output_file = create_unique_file (input_file, 'o');

  add_to_tmp_file_list (output_file);

  const char* input_base = ipa_basename (input_file);
  const char* output_base = ipa_basename (output_file);

  infiles->push_back(input_file);
  outfiles->push_back(output_file);
  outfiles_fullpath->push_back(output_file);

  // Assemble the command line.

  ARGV argv;                          // vector<const char*>
  get_command_line (ipc_get_ip_file_hdr (pu), argv, input_file, output_file);

  char* str1 = (char*) malloc(PATH_MAX + 64);
  sprintf(str1, "-TENV:ipa_ident=%ld", time(0));
  argv.push_back(str1);

  char* str2 = (char*) malloc(PATH_MAX + 64);
  sprintf(str2, "-TENV:read_global_data=%s",
          whirl_symtab_name );
  argv.push_back(str2);

  if(IPA_Enable_AutoGnum){
      argv.push_back("-Gspace 0");
  }

  // Add "-psclp filename".
  if (psclp_arg != NULL) {
    char* str = static_cast<char*>(strdup(psclp_arg));
    argv.push_back(str);
  }

  if (ProMP_Listing) {
    char* str = static_cast<char*>(malloc(64));
    sprintf(str, "-PROMP:=ON -PROMP:next_id=%lu", (unsigned long) ProMP_id);
    argv.push_back(str);
  }
    
  if (IPA_Enable_Array_Sections)
    argv.push_back("-LNO:ipa");

  // Piece the command line together and push it onto the list.
  ARGV::const_iterator i;

  vector<std::string> cmdline;
  cmdline.reserve(argv.size());
    
  for (i = argv.begin(); i != argv.end(); ++i) {
    cmdline.push_back(*i);
  }

  commands->push_back(cmdline);
    
  Is_True (infiles->size() > 0 &&
           infiles->size() == outfiles->size() &&
           infiles->size() == outfiles_fullpath->size() &&
           infiles->size() == commands->size(),
           ("ipacom_process_file: inconsistent vector sizes"));

  // Set up extra args for compiling symtab, if necessary.
  if (symtab_extra_args->empty())
    get_extra_symtab_args(argv);

  return infiles->size() - 1;

} // ipacom_process_file


static 
const char*
Get_Annotation_Filename_With_Path (void) {
    static char buf[MAXPATHLEN];

    if (!Annotation_Filename) { buf[0] = '\0'; }
    else if (*Annotation_Filename == '/') {
        strcpy (buf, Annotation_Filename);
    }else {
        strcpy (buf, "$$dir/");		// bug 11686
        strcat (buf, Annotation_Filename);
    }
     
    return &buf[0];
}

extern "C"
void ipacom_doit (const char* ipaa_filename)
{
  int i,n;

  Is_True(infiles != 0 && outfiles != 0 && outfiles_fullpath != 0 &&
          commands != 0 && makefile != 0,
          ("ipacom_doit: ipacom not yet initialized"));
  Is_True(infiles->size() == outfiles->size() &&
          infiles->size() == outfiles_fullpath->size() &&
          infiles->size() == commands->size(),
          ("ipacom_doit: vectors are inconsistent"));


  if (infiles->size() > 0) {
    Is_True(strlen(input_symtab_name) != 0 &&
            strlen(whirl_symtab_name) != 0 &&
            strlen(elf_symtab_name) != 0 &&
            symtab_command_line->size() != 0,
            ("ipacom_doit: symtab not initialized"));
  }

  // These are used when compiling each .I file.
  vector<std::string> extra_args = get_extra_args(ipaa_filename);

  // Running the first command
  // This generates both the .o symtab and the .G symtab.
  if (strlen(elf_symtab_name) != 0) {
    char* toolroot = getenv("TOOLROOT");
    Is_True(strlen(input_symtab_name) != 0 &&
            strlen(whirl_symtab_name) != 0 &&
            symtab_command_line->size() != 0 ,
            ("ipacom_doit: symtab not initialized"));

    if (symtab_extra_args->empty())
      *symtab_extra_args = get_extra_args(0);

    if (Feedback_Filename) {
	n = symtab_command_line->size()+symtab_extra_args->size()+4;
	i = 0;
	vector<std::string> arguments(n);
	
	for(std::vector<std::string>::iterator iter=symtab_command_line->begin();iter!=symtab_command_line->end();iter++){
		arguments[i++] = *iter;
	}

        arguments[i++] = "-Wb,-OPT:procedure_reorder=on";
        arguments[i++] = "-fb_create";
        arguments[i++] = Feedback_Filename;

        for(std::vector<std::string>::iterator iter=symtab_extra_args->begin();iter!=symtab_extra_args->end();iter++){
                arguments[i++] = *iter;
        }

	arguments[i++] = "-Wb,-CG:enable_feedback=off";

        ipc_run_simple_program(arguments);
    } else if (Annotation_Filename) {
	n = symtab_command_line->size()+symtab_extra_args->size()+4;
	i = 0;
        vector<std::string> arguments(n);
	
	for(std::vector<std::string>::iterator iter=symtab_command_line->begin();iter!=symtab_command_line->end();iter++){
		arguments[i++] = *iter;
	}

        arguments[i++] = "-Wb,-OPT:procedure_reorder=on";
        arguments[i++] = "-fb_opt";
        arguments[i++] =  Get_Annotation_Filename_With_Path ();

        for(std::vector<std::string>::iterator iter=symtab_extra_args->begin();iter!=symtab_extra_args->end();iter++){
                arguments[i++] = *iter;
        }

	arguments[i++] = "-Wb,-CG:enable_feedback=on";

        ipc_run_simple_program(arguments);
    } else {
	n = symtab_command_line->size()+symtab_extra_args->size()+2;
	i = 0;
        vector<std::string> arguments(n);
	
	for(std::vector<std::string>::iterator iter=symtab_command_line->begin();iter!=symtab_command_line->end();iter++){
		arguments[i++] = *iter;
	}

        arguments[i++] = "-Wb,-OPT:procedure_reorder=on";

        for(std::vector<std::string>::iterator iter=symtab_extra_args->begin();iter!=symtab_extra_args->end();iter++){
                arguments[i++] = *iter;
        }

	arguments[i++] = "-Wb,-CG:enable_feedback=off";

        ipc_run_simple_program(arguments);
    }                                                                                                                    
  }


  // Second exec
  // For each whirl file, tell how to create the corresponding elf file.

  for (size_t command_num = 0; command_num < infiles->size(); ++command_num) {
    if (Feedback_Filename) {
	n = (*commands)[command_num].size() + extra_args.size() + 4;
	i = 0;
        vector<std::string> arguments(n);
	
	for(std::vector<std::string>::iterator iter=(*commands)[command_num].begin();iter!=(*commands)[command_num].end();iter++){
		arguments[i++] = *iter;
	}

        arguments[i++] = "-Wb,-OPT:procedure_reorder=on";
	arguments[i++] = "-fb_create";
	arguments[i++] = Feedback_Filename;

        for(std::vector<std::string>::iterator iter=extra_args.begin();iter!=extra_args.end();iter++){
                arguments[i++] = *iter;
        }

	arguments[i++] = "-Wb,-CG:enable_feedback=off";

        ipc_run_simple_program(arguments);
    } else if (Annotation_Filename) {
	n = (*commands)[command_num].size() + extra_args.size() + 4;
	i = 0;
        vector<std::string> arguments(n);
	
	for(std::vector<std::string>::iterator iter=(*commands)[command_num].begin();iter!=(*commands)[command_num].end();iter++){
		arguments[i++] = *iter;
	}

        arguments[i++] = "-Wb,-OPT:procedure_reorder=on";
	arguments[i++] = "-fb_opt";
	arguments[i++] = Get_Annotation_Filename_With_Path ();

        for(std::vector<std::string>::iterator iter=extra_args.begin();iter!=extra_args.end();iter++){
                arguments[i++] = *iter;
        }

	arguments[i++] = "-Wb,-CG:enable_feedback=on";

        ipc_run_simple_program(arguments);
    } else {
	n = (*commands)[command_num].size() + extra_args.size() + 2;
	i = 0;
        vector<std::string> arguments(n);
	
	for(std::vector<std::string>::iterator iter=(*commands)[command_num].begin();iter!=(*commands)[command_num].end();iter++){
		arguments[i++] = *iter;
	}

        arguments[i++] = "-Wb,-OPT:procedure_reorder=on";

        for(std::vector<std::string>::iterator iter=extra_args.begin();iter!=extra_args.end();iter++){
                arguments[i++] = *iter;
        }

	arguments[i++] = "-Wb,-CG:enable_feedback=off";

        ipc_run_simple_program(arguments);
    }                                                                                                                    
  }


  // The third exec: linking
  // The default target: either the executable, or all of the
  // elf object files.

  if (IPA_Enable_final_link) {
    // Get the link command line.
    const ARGV* link_line = ipa_link_line_argv (outfiles_fullpath, 
    	    	    	    	    	    	tmpdir, 
						elf_symtab_name);
    Is_True(link_line->size() > 1, ("Invalid link line ARGV vector"));

    n = link_line->size();
    vector<std::string> arguments;
    arguments.reserve(n);

    arguments.push_back((*command_map)["cc"]);

    ARGV::const_iterator i = link_line->begin();
    for (++i; i != link_line->end(); ++i) {
      // Since we are using pathcc to link, don't print out the run-time support
      // files.
      const char *p;
      if (((p = strstr(*i, "/crt1.o")) && p[7] == '\0') ||
	  ((p = strstr(*i, "/crti.o")) && p[7] == '\0') ||
	  ((p = strstr(*i, "/crtbegin.o")) && p[11] == '\0') ||
	  ((p = strstr(*i, "/crtend.o")) && p[9] == '\0') ||
	  ((p = strstr(*i, "/crtn.o")) && p[7] == '\0')) {
	continue;
      }
      // don't pass --dynamic-linker to pathcc - it knows it already
      if(!strncmp(*i,"--dynamic-linker",16))
        continue;
      // pathcc better knows the correct arch
      if(!strncmp(*i,"-melf",5))
        continue;
      
      arguments.push_back(*i);
    }

    // If we're compiling with -show, make sure we see the link line.
    if (ipa_opt[IPA_SHOW].flag) {
	arguments.push_back("-show");
    }

    ipc_run_simple_program (arguments);
  }

  // Temps cleanup
  if (!ipa_opt[IPA_KEEP_TEMPS].flag) {
    vector<const char*>::iterator i;
    for (i = infiles->begin(); i != infiles->end(); ++i)
      unlink(*i);
    for (i = outfiles->begin(); i != outfiles->end(); ++i)
      unlink(*i);

    if (strlen(input_symtab_name) != 0) {
      unlink(input_symtab_name);
      unlink(whirl_symtab_name);
      unlink(elf_symtab_name);
    }
  
    rmdir(tmpdir);
  }

  //ipacom_doit never returns
  exit(0);

} // ipacom_doit



// Collect any extra arguments that we will tack onto the command line.
// First collect them as a vector of strings, then concatenate them all
// together into a single string.
static vector<std::string> get_extra_args(const char* ipaa_filename)
{
  vector<std::string> args;
  args.reserve(16);
  
  switch (ipa_opt[IPA_SHARABLE].flag) {
  case F_MAKE_SHARABLE:
    args.push_back("-TENV:PIC");
    break;
  case F_CALL_SHARED:
  case F_CALL_SHARED_RELOC:
    args.push_back("-pic1");
    break;
  case F_NON_SHARED:
    args.push_back("-non_shared");
    break;
  case F_RELOCATABLE:
    if (IPA_Enable_Relocatable_Opt == TRUE)
      args.push_back("-pic1");
    break;
  }
  
  // -IPA:keeplight:=ON, which is the default, means that we keep only
  // the .I files, not the .s files.
  if (ipa_opt[IPA_KEEP_TEMPS].flag && !IPA_Enable_Keeplight)
    args.push_back("-keep");

  if (ipa_opt[IPA_SHOW].flag)
    args.push_back("-show");


  /* If there's an IPAA intermediate file, let WOPT know: */
  if (ipaa_filename) {
    char* buf = (char*) malloc(strlen(ipaa_filename) + 32);
    if (!buf)
      ErrMsg (EC_No_Mem, "extra_args");

    sprintf(buf, "-WOPT:ipaa:ipaa_file=%s", ipaa_filename );
    args.push_back(buf);
  }

  /* If there are -WB,... options, pull them out and add them to the
     * list.  Strip the '-WB,', and treat non-doubled internal commas
     * as delimiters for new arguments (undoubling the doubled ones):
     */
  if (WB_flags) {
    string p = ipc_copy_of (WB_flags);
    while (*p) {
      args.push_back(p);
      while (*p) {
        if (*p == ',') {
          if (p[1] != ',') {
            *p++ = 0;
            break;
          }
          else
            ipc_escape_char(p);
        }
        else if (p[0] == '\\' && p[1] != 0)
          ipc_escape_char (p);
        p++;
      }
    }
  }

  /* If there are -Yx,... options, pull them out and add them to the
     * list.  Several may be catenated with space delimiters:
     */
  vector<char*> space_ptr; // for restoring spaces overwritten by zero
  if (Y_flags) {
    char* p = Y_flags;
    while (*p) {
      args.push_back(p);
      while (*p) {
        if (*p == ' ') {
          space_ptr.push_back(p);
          *p++ = 0;
          break;
        }
        else if (p[0] == '\\')
          ipc_escape_char (p);
        p++;
      }
    }
  }

  // now restore spaces in Y_flags that were overwritten by zeros
  for (size_t idx = 0; idx < space_ptr.size(); idx++) {
    Is_True(*space_ptr[idx] == 0, ("space_ptr must point to 0"));
    *space_ptr[idx] = ' ';
  }

  return args;
} /* get_extra_args */

static void get_extra_symtab_args(const ARGV& argv)
{
  *symtab_extra_args = get_extra_args(0);

  for (ARGV::const_iterator i = argv.begin(); i != argv.end(); ++i) {
    const char* const debug_flag = "-DEBUG";
    const char* const G_flag = "-G";
    const char* const TARG_flag = "-TARG";
    const char* const OPT_flag = "-OPT";
    const int debug_len = 6;
    const int G_len = 2;
    const int TARG_len = 5;
    const int OPT_len = 4;
    bool flag_found = false;

    // The link line contains -r.  That means we don't have enough information
    // from the link line alone to determine whether the symtab should be
    // compiled shared or nonshared.  We have to look at how one of the other
    // files was compiled.
    if (ipa_opt[IPA_SHARABLE].flag == F_RELOCATABLE &&
                IPA_Enable_Relocatable_Opt != TRUE) {
      const char* const non_shared_flag = "-non_shared";
      if (strcmp(*i, non_shared_flag) == 0)
        flag_found = true;
    }

    if ((strncmp(*i, debug_flag, debug_len) == 0) ||
            (strncmp(*i, G_flag, G_len) == 0) ||
            (strncmp(*i, OPT_flag, OPT_len) == 0) ||
            (strncmp(*i, TARG_flag, TARG_len) == 0))

      flag_found = true;

    if (flag_found == true) {
	symtab_extra_args->push_back(*i);
     }
  }
}


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
#include <elf.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "ipa_command_line.h"
#include "errors.h"

#include "sys/elf_whirl.h"

void *(*p_ipa_open_input)(char *, off_t *) = NULL;
void (*p_ipa_init_link_line)(int, char **) = NULL;
void (*p_ipa_add_link_flag)(const char*) = NULL;
void (*p_ipa_driver)(int, char **) = NULL;
void (*p_process_whirl64)(void *, off_t, void *, int, const char *) = NULL;
void (*p_process_whirl32)(void *, off_t, void *, int, const char *) = NULL;
int  (*p_Count_elf_external_gots)(void) = NULL;
void (*p_ipa_insert_whirl_marker)(void) = NULL;
void (*p_Sync_symbol_attributes)(unsigned int, unsigned int, bool, unsigned int) = NULL;
void (*p_ipa_erase_link_flag)(const char*) = NULL;
void (*p_Ipalink_Set_Error_Phase)(char *) = NULL;
void (*p_Ipalink_ErrMsg_EC_infile)(char *) = NULL;
void (*p_Ipalink_ErrMsg_EC_outfile)(char *) = NULL;
char ** ipa_argv = NULL;
int ipa_argc = 0;

static int orig_iargc_size = 10;
char * WB_flags = NULL;
char * Y_flags = NULL;

static char char_opt[] = {'a','A','b','c','e','f','F','G','h','l','L','m','o','O','R','T','u','y','Y','z','0'};

int arg_count;			    /* argument count */
char **arg_vector;		    /* argument vector */
char **environ_vars;		    /* list of environment variables */

char *psclp_arg = NULL;	// PathScale subscription

char * outfilename = "./a.out";
char * tmpdir;

#define          F_RELOCATABLE       1
#define          F_NON_SHARED        2
#define          F_CALL_SHARED       4
#define          F_MAKE_SHARABLE     8
#define          F_STATIC    (F_NON_SHARED | F_RELOCATABLE)
#define          F_DYNAMIC   (~(F_STATIC))
#define          F_MAIN      (F_NON_SHARED | F_CALL_SHARED)
#define          F_EXEC      (~F_RELOCATABLE)
#define          F_ALL       (F_STATIC | F_DYNAMIC)
#define          F_CALL_SHARED_RELOC (F_RELOCATABLE | F_CALL_SHARED)


IPA_OPTION ipa_opt[] = {
    {IPA_SHARABLE, 	    F_CALL_SHARED,  F_CALL_SHARED}, 
    {IPA_DEMANGLE, 	    0,		    0}, 
    {IPA_SHOW, 	    0,		    0}, 
    {IPA_KEEP_TEMPS, 	    0,		    0},
    {IPA_HIDES, 	    0,		    0}, 
    {IPA_VERBOSE, 	    0,		    0} 
};



/*******************************************************
	Function: ipa_copy_of

	Allocate for and copy given string into a copy.

 *******************************************************/
char *
ipa_copy_of (char *str)
{
    register int len;
    register char *p;

    len = strlen(str) + 1;
    p = (char *) malloc (len);
    MALLOC_ASSERT (p);
    memcpy(p, str, len);
    return p;
} /* ipa_copy_of */




static int ipa_realloc_option (char **argv )
{
    if (ipa_argc == 0) {
	ipa_argv = (char **) malloc (orig_iargc_size * sizeof (char*));
	MALLOC_ASSERT (ipa_argv);
    }
    else if (ipa_argc >= orig_iargc_size) {
	orig_iargc_size *=2;
	ipa_argv = (char **) realloc (ipa_argv, (orig_iargc_size * sizeof(char *)));
	MALLOC_ASSERT (ipa_argv);
    }

    ipa_argv[ipa_argc++] = ipa_copy_of(argv[0]);

    return 1;
} /* ipa_realloc_option */

/* ====================================================================
 *
 * add_WB_opt
 *
 * We have an option -WB,... to be passed to the back end via ipacom.
 * If WB_flags is NULL, set it to this option with "-WB," stripped.
 * Otherwise append this option with "-WB" stripped, i.e. retaining the
 * comma separator.
 *
 * ====================================================================
 */

static void
add_WB_opt (char **argv)
{
    char *p = *argv;

    if ( WB_flags == NULL ) {
    	WB_flags = concat_names("-Wb,",&p[3]);
    } else {
    	char *flg = concat_names(WB_flags,&p[3] ); /* include the comma */
    	free(WB_flags);
    	WB_flags = flg;
  }

  return;
}


/* ====================================================================
 *
 * add_Y_opt
 *
 * We have an option -Y... to be passed to the back end via ipacom.
 * If Y_flags is NULL, set it to this option.  Otherwise append this
 * option with a space delimiter.
 *
 * ====================================================================
 */

static void add_Y_opt (char **argv)
{
    char *p = *argv;

    if ( Y_flags == NULL ) {
    	Y_flags = ipa_copy_of(p);
    } else {
    	char *flg;
	
	flg = concat_names(Y_flags," ");
	free (Y_flags);
	Y_flags = flg;
	
	flg = concat_names(Y_flags,p);
	free (Y_flags);
	Y_flags = flg;
    }
}



#ifdef X86_WHIRL_OBJECTS
static int look_for_elf32_section (const Elf32_Ehdr* ehdr, Elf32_Word type, Elf32_Word info)
{
    int i;

    Elf32_Shdr* shdr = (Elf32_Shdr*)((char*)ehdr + ehdr->e_shoff);
    for (i = 1; i < ehdr->e_shnum; ++i) {
        if (shdr[i].sh_type == type && shdr[i].sh_info == info)
            return 1;
    }

    return 0;
}

static int look_for_elf64_section (const Elf64_Ehdr* ehdr, Elf64_Word type, Elf64_Word info)
{
    int i;

    Elf64_Shdr* shdr = (Elf64_Shdr*)((char*)ehdr + ehdr->e_shoff);
    for (i = 1; i < ehdr->e_shnum; ++i) {
        if (shdr[i].sh_type == type && shdr[i].sh_info == info)
            return 1;
    }

    return 0;
}
#endif


#define ET_SGI_IR   (ET_LOPROC + 0)
static bool check_for_whirl(char *name, bool *is_elf)
{
    int fd = -1;
    char *raw_bits = NULL;
    int size,bufsize;
    Elf32_Ehdr *p_ehdr = NULL;
    struct stat statb;
    int test;
    
    *is_elf = false;

    fd = open(name, O_RDONLY, 0755);
    if (fd < 0)
	return false;

    if ((test = fstat(fd, &statb) != 0)) {
    	close(fd);
	return false;
    }

    if (statb.st_size < sizeof(Elf64_Ehdr)) {
    	close(fd);
    	return false;
    }
    
    bufsize = sizeof(Elf64_Ehdr);
    
    raw_bits = (char *)malloc(bufsize*4);
    MALLOC_ASSERT(raw_bits);

    size = read(fd, raw_bits, bufsize);
    /*
    * Check that the file is an elf executable.
    */
    p_ehdr = (Elf32_Ehdr *)raw_bits;
    if (p_ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	p_ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	p_ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	p_ehdr->e_ident[EI_MAG3] != ELFMAG3) {
	    close(fd);
	    free(raw_bits);
	    return(false);
    }

    *is_elf = true;

    if(p_ehdr->e_ident[EI_CLASS] == ELFCLASS32){
    	Elf32_Ehdr *p32_ehdr = (Elf32_Ehdr *)raw_bits;
#ifdef X86_WHIRL_OBJECTS
        char *second_buf = NULL;

        int new_size = p32_ehdr->e_shoff + sizeof(Elf32_Shdr)*p32_ehdr->e_shnum;
        lseek(fd, 0, SEEK_SET);
        second_buf = (char *)alloca(new_size);
        size = read(fd, second_buf, new_size);
        p32_ehdr = (Elf32_Ehdr *)second_buf;
	if (p32_ehdr->e_type == ET_REL && look_for_elf32_section(p32_ehdr, SHT_PROGBITS, WT_PU_SECTION)) {
#else
	if (p32_ehdr->e_type == ET_SGI_IR) {
#endif 
	    close(fd);
	    free(raw_bits);
	    return true;
	}
    }
    else {
	Elf64_Ehdr *p64_ehdr = (Elf64_Ehdr *)raw_bits;
#ifdef X86_WHIRL_OBJECTS
        char *second_buf = NULL;

        int new_size = p64_ehdr->e_shoff + sizeof(Elf64_Shdr)*p64_ehdr->e_shnum;
        lseek(fd, 0, SEEK_SET);
        second_buf = (char *)alloca(new_size);
        size = read(fd, second_buf, new_size);
        p64_ehdr = (Elf64_Ehdr *)second_buf;
	if (p64_ehdr->e_type == ET_REL && look_for_elf64_section(p64_ehdr, SHT_PROGBITS, WT_PU_SECTION)) {
#else
	if (p64_ehdr->e_type == ET_SGI_IR) {
#endif 
	    close(fd);
	    free(raw_bits);
	    return true;
	}
     }

    close(fd);
    free(raw_bits);
    return false;
    
 }

/*******************************************************
 Function: needs_argument.
 
 Determine if this option needs an argument.
 This routine will need to change as the commandline
 arguments change or are augmented.
 
 ******************************************************/
static bool
needs_argument(char *string, bool is_double_dash)
{

    int len = strlen(string);

    if (is_double_dash) {
    	if ((strcmp (string, "architecture") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "format") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "mri-script") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "entry") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "auxiliary") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "filter") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "gpsize") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "library") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "library-path") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "output") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "just-symbols") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "script") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "undefined") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "trace-symbol") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "assert") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "defsym") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "dynamic-linker") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "Map") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "oformat") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "retain-symbols-file") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "rpath-link") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "split-by-reloc") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "task-link") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "verbose") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "version-script") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "version-exports-section") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "wrap") == 0)) {
    	    return true;
    	}
    }
    else {
    	if (len == 1) {
	    int i;
	    int size = strlen(char_opt);
	    for (i=0;i<size;i++) {
	    	if (char_opt[i] == *string)
		    return true;
	    }
	    return false;
	}
    	if ((strcmp (string, "soname") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "rpath") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "Tbss") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "Tdata") == 0)) {
    	    return true;
    	}
    	if ((strcmp (string, "Ttext") == 0)) {
    	    return true;
    	}
    }
    
    return false;
}


/*******************************************************
 Function: blank_arg.
 
 Blank out the given argument so other parts
 of the program that read the argv don't get
 confused.

 *******************************************************/
static void
blank_arg(char **argv, int ndx)
{

    int j,len;

    len = strlen(argv[ndx]);

    if (len >=3)
    	strcpy(argv[ndx],"-g"); /* ignored by the linker */
    else {
    	for(j=0;j<len;j++)
    	    argv[ndx][j] = ' ';
    }
}



/*******************************************************
	Function: ipa_set_syms

	dlopen ipa.so and set entry points with
	dlsym calls.

 *******************************************************/
static void ipa_set_syms(void)
{

    void *p_handle = NULL;
    char *p_error = NULL;

    p_handle = dlopen("ipa.so",RTLD_LAZY);
    if (!p_handle) {
    	fputs (dlerror(), stderr);
    	exit(1);
    }
    
    p_ipa_open_input = (void* (*)(char*, off_t*)) dlsym(p_handle,"ipa_open_input");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_ipa_init_link_line = (void (*)(int, char**))dlsym(p_handle,"ipa_init_link_line");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_ipa_add_link_flag = (void (*)(const char*))dlsym(p_handle,"ipa_add_link_flag");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_ipa_driver = (void (*)(int, char**))dlsym(p_handle,"ipa_driver");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_process_whirl64 = (void (*)(void*, off_t, void*, int, const char*))dlsym(p_handle,"process_whirl64");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_process_whirl32 = (void (*)(void*, off_t, void*, int, const char*))dlsym(p_handle,"process_whirl32");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_ipa_insert_whirl_marker = (void (*)())dlsym(p_handle,"ipa_insert_whirl_marker");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_Sync_symbol_attributes = (void (*)(unsigned int, unsigned int, bool, unsigned int))dlsym(p_handle,"Sync_symbol_attributes");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_ipa_erase_link_flag = (void (*)(const char*))dlsym(p_handle,"ipa_erase_link_flag");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_Ipalink_Set_Error_Phase = (void (*)(char*))dlsym(p_handle,"Ipalink_Set_Error_Phase");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_Ipalink_ErrMsg_EC_infile = (void (*)(char*))dlsym(p_handle,"Ipalink_ErrMsg_EC_infile");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }

    p_Ipalink_ErrMsg_EC_outfile = (void (*)(char*))dlsym(p_handle,"Ipalink_ErrMsg_EC_outfile");
    if ((p_error = dlerror()) != NULL)  {
    	fputs(p_error, stderr);
    	exit(1);
    }
}

int ipa_search_command_line(int argc, char **argv, char **envp)
{
    int i;

    arg_count = argc;
    arg_vector = argv;
    environ_vars = envp;

    	/*
	 *  The ipa.so needs to be opened and entry
	 *  points need to be found with dlsym.
	 */
    ipa_set_syms();
    (*p_Ipalink_Set_Error_Phase)("IPA Startup");

    	/*
    	 * We need to build up the commandline options
    	 * that will be passed to the linker/compiler during
         * the second pass
    	 */
    (*p_ipa_init_link_line) (0, NULL);

    for (i=1;i<argc;i++) {
	bool is_elf = false;

    	char *string = argv[i];
    	if (*string == '-' && string[1] == '-') {
	    if ((strncmp (&string[2], "ipa", strlen ("ipa")) == 0)) {
	    	continue;
	    }
	    if ((strncmp (&string[2], "IPA:", strlen ("IPA:")) == 0)) {
    	    	char *p = &argv[i][1];
	    	ipa_realloc_option(&p);
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strcmp (&string[2], "keep") == 0)) {
    	    	ipa_opt[IPA_KEEP_TEMPS].flag = true;
		    /* Blank out argument */
		blank_arg(argv,i);
	    	continue;
	    }
	    else if ((needs_argument(&string[2],true) == true)) {
	    	(*p_ipa_add_link_flag) (argv[i++]);
		(*p_ipa_add_link_flag) (argv[i]);
		continue;
	    }
	}   /* if "--" */
	else if (*string == '-') {
	    if ((strncmp(string,"-WB,",4)) == 0) {
	    	add_WB_opt (&argv[i]);

		    /* Blank out argument */
		blank_arg(argv,i);
		continue;
	    }
	    else if ((strncmp(string,"-Y",2)) == 0) {
	    	add_Y_opt (&argv[i]);

		    /* Blank out argument */
		blank_arg(argv,i);
		continue;
	    }
	    if ((strncmp (string, "-ipacom", strlen ("-ipacom")) == 0)) {
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-ipa", strlen ("-ipa")) == 0)) {
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-DEFAULT:", strlen ("-DEFAULT:")) == 0)) {
	    	ipa_realloc_option(&argv[i]);
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-IPA:", strlen ("-IPA:")) == 0)) {
	    	ipa_realloc_option(&argv[i]);
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-INLINE:", strlen ("-INLINE:")) == 0)) {
	    	ipa_realloc_option(&argv[i]);
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-INTERNAL:", strlen ("-INTERNAL:")) == 0)) {
	    	ipa_realloc_option(&argv[i]);
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-OPT:", strlen ("-OPT:")) == 0)) {
	    	ipa_realloc_option(&argv[i]);
		blank_arg(argv,i);
	    	continue;
	    }
	    if ((strncmp (string, "-TENV:", strlen ("-TENV:")) == 0)) {
	    	ipa_realloc_option(&argv[i]);
		blank_arg(argv,i);
	    	continue;
	    }
	    // PathScale subscription
	    if ((strcmp (string, "-psclp") == 0)) {
	    	psclp_arg = (char *)malloc(10 + strlen(argv[i+1]));
		sprintf (psclp_arg, "-psclp %s", argv[i+1]);
		blank_arg(argv,i);
		blank_arg(argv,i+1);
	    	continue;
	    }
	    else if ((strcmp(string,"-keep")) == 0) {
    	    	ipa_opt[IPA_KEEP_TEMPS].flag = true;

		    /* Blank out argument */
		blank_arg(argv,i);
		continue;
	    }
	    else if ((strcmp(string,"-show")) == 0) {
    	    	ipa_opt[IPA_SHOW].flag = true;

		    /* Blank out argument */
		blank_arg(argv,i);
		continue;
	    }
	    else if ((strcmp(string,"-demangle")) == 0) {
    	    	ipa_opt[IPA_DEMANGLE].flag = true;

		    /* Blank out argument */
		blank_arg(argv,i);
		continue;
	    }
	    else if ((strcmp(string,"-o")) == 0) {
    	    	outfilename = (char *)malloc(strlen(argv[i+1])+3);
    	    	MALLOC_ASSERT(outfilename);
		strcpy(outfilename,"");
		strcat(outfilename,argv[i+1]);
	    	(*p_ipa_add_link_flag) (argv[i++]);
		(*p_ipa_add_link_flag) (argv[i]);
		continue;
	    }
	    else if ((strcmp(string,"-v")) == 0) {
    	    	ipa_opt[IPA_VERBOSE].flag = true;
	    }
	    	    /* Check for sgi debug tracing */
	    else if (string[1] == 't' && strlen(string) > 3) {
	    	ipa_realloc_option(&argv[i]);

		    /* Blank out argument */
		blank_arg(argv,i);
		continue;
	    }
	    else if (needs_argument(&string[1],false)) {
	    	(*p_ipa_add_link_flag) (argv[i++]);
		(*p_ipa_add_link_flag) (argv[i]);
		continue;
	    }
	}
	/* This splits the post ipa commandline arguments */
	else if (check_for_whirl(argv[i], &is_elf)) {
	    (*p_ipa_insert_whirl_marker)();
	    continue;
	}

	/* Split the ipa commandline arguments as long as any ELF object is
	   seen, whether or not it is WHIRL.  Needed to handle the case where
	   the only object given is a non-WHIRL object, followed by a WHIRL
	   archive:

	     pathcc -Ldir a.o -lfoo -lwhirl -lbar	; non-WHIRL a.o

	   The WHIRL objects from libwhirl.a will be compiled and linked at the
	   position of a.o.  
	 */

	if (is_elf == true)
	  (*p_ipa_insert_whirl_marker)();

    	(*p_ipa_add_link_flag) (argv[i]);

    }	    /* for */

    return ipa_argc;
}

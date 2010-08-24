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


#ifndef __IPA_OPTION_H__
#define __IPA_OPTION_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These are for passing option information from the
 * static linker to ipa without involving the linkers
 * internal option table. Any enumerated types defined
 * here can be added to,  but not altered in any other
 * way. This allows the linker to change without affecting
 * ipa.
 */


typedef enum{
    IPA_SHARABLE, 
    IPA_DEMANGLE, 
    IPA_SHOW, 
    IPA_KEEP_TEMPS,
    IPA_HIDES, 
    IPA_VERBOSE,
    MAX_IPA_OPT
}ipa_option_enum;

typedef struct ipa_option {
    ipa_option_enum opt_ndx;
    unsigned    flag		: 4;    /*  */
    unsigned     set		: 4;    /*  */
} IPA_OPTION;

extern IPA_OPTION ipa_opt[MAX_IPA_OPT];

#define HS_DEFAULT 0
#define HS_HIDES 1
#define HS_EXPORTS 2
#define HS_IGNORE 3

    /*
     * The following struct is used for storing lists
     * of names. Most often these lists are gathered
     * from the commandline, but not always.
     */
typedef struct {    /* all symbols specified in command line */
    char **sym;     /* are collected here, one type per entry */
    int    num;
    int	   max;
} OPTION_SYM;

                 /* these are set to bit fields for */
                 /* easy table initialization */
#define          F_RELOCATABLE       1
#define          F_NON_SHARED        2
#define          F_CALL_SHARED       4
#define          F_MAKE_SHARABLE     8
#define          F_STATIC    (F_NON_SHARED | F_RELOCATABLE)
#define          F_DYNAMIC   (~(F_STATIC))
#define          F_MAIN      (F_NON_SHARED | F_CALL_SHARED)
#define		 F_EXEC	     (~F_RELOCATABLE)
#define          F_ALL       (F_STATIC | F_DYNAMIC)
#define          F_CALL_SHARED_RELOC (F_RELOCATABLE | F_CALL_SHARED)

typedef enum {
	TOS_MIPS_O32,

#if 1
	TOS_MIPS_R32,
#endif
	TOS_MIPS_N32,
	TOS_MIPS_64,
	TOS_IA64_64,
	TOS_IA64_32, 
	TOS_MAX
}targos_enum;

extern string tos_string[TOS_MAX];

#ifdef _SUPPORT_IPA
#define verboseflag ipa_opt[IPA_VERBOSE].flag
#define keep	    ipa_opt[IPA_KEEP_TEMPS].flag
#endif

extern char *outfilename;
extern string ipacom_flags;
extern string WB_flags;
extern string Y_flags;
extern int ipa_argc;
extern string *ipa_argv;
extern int ipa_use_intrinsic;

extern void ld_to_ipa_option_update(void);
extern void ipa_to_ld_option_update(void);

#ifdef __cplusplus
}
#endif

#endif /* __IPA_OPTION_H__ */

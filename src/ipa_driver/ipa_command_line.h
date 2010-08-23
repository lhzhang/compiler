#ifndef IPA_COMMAND_LINE_H_INCLUDED
#define IPA_COMMAND_LINE_H_INCLUDED

#include <sys/types.h>

extern void *(*p_ipa_open_input)(char *, off_t *);
extern void (*p_ipa_init_link_line)(int, char **);
extern void (*p_ipa_add_link_flag)(const char*);
extern void (*p_ipa_driver)(int, char **);
extern void (*p_process_whirl64)(void *, off_t, void *, int, const char *);
extern void (*p_process_whirl32)(void *, off_t, void *, int, const char *);
extern void (*p_ipa_insert_whirl_marker)(void);
extern void (*p_ipa_erase_link_flag)(const char*);
extern void (*p_Ipalink_Set_Error_Phase)(char *);
extern void (*p_Ipalink_ErrMsg_EC_Outfile)(char *);

int ipa_search_command_line(int argc, char **argv, char **envp);

char *ipa_copy_of (char *str);

char * concat_names(const char * name1, const char * name2);

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


#endif //IPA_COMMAND_LINE_H_INCLUDED


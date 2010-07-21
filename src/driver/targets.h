/*
  Copyright (C) 2010 PathScale Inc. All Rights Reserved.
*/


#ifndef __DRIVER_TARGETS_H__
#define __DRIVER_TARGETS_H__


#include "basic.h"


void init_targets();


// Returns path to target library path
// Returned memory should be freed by caller
char *target_library_path();


// Returns current target phase path
// Returned memory should be freed by caller
char *target_phase_path();


// Returns path to target runtime path (location of crtX.o)
const char *target_runtime_path();


#ifdef TARG_X8664
// Returns TRUE if target architecture is X8664
boolean is_target_arch_X8664();
#endif // TARG_X8664


#ifdef TARG_MIPS
// Returns TRUE if target architecture is MIPS
boolean is_target_arch_MIPS();
#endif // TARG_MIPS

#endif // __DRIVER_TARGETS_H__


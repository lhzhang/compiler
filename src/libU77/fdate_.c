/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/* Copyright (C) 1995, 1996, 2001 Free Software Foundation, Inc.
This file is part of GNU Fortran libU77 library.

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with GNU Fortran; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "sys_ctime.h"

#include "f2c.h"

/* NB. this implementation is for a character*24 function.  There's
   also a subroutine version.  Of course, the calling convention is
   essentially the same for both. */

#ifdef KEY /* Bug 1683, 5019 */

#include "pathf90_libU_intrin.h"

#define CTIME_BUFLEN	26

void
pathf90_fdate(char *ret_val, int ret_val_len) 
{
  int s_copy ();
  char buf[CTIME_BUFLEN];
  time_t tloc;
  tloc = time (NULL);
  /* Allow a length other than 24 for compatibility with what other
     systems do, despite it being documented as 24. */
  s_copy (ret_val,
          call_sys_ctime_r ((time_t *) & tloc, buf, CTIME_BUFLEN),
          ret_val_len,
          24);
}

#else

/* Character *24 */ void
fdate_ (char *ret_val, ftnlen ret_val_len)
{
  int s_copy ();
  time_t tloc;
  tloc = time (NULL);
  /* Allow a length other than 24 for compatibility with what other
     systems do, despite it being documented as 24. */
  s_copy (ret_val, ctime ((time_t *) & tloc), ret_val_len, 24);
}
#endif /* KEY Bug 1683, 5019 */

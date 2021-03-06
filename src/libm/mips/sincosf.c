/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */


/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

   Path64 is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation version 2.1

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

/* ====================================================================
 * ====================================================================
 *
 * Module: sincosf.c
 * $Revision: 1.5 $
 * $Date: 04/12/21 14:58:22-08:00 $
 * $Author: bos@eng-25.internal.keyresearch.com $
 * $Source: /home/bos/bk/kpro64-pending/libm/mips/SCCS/s.sincosf.c $
 *
 * Revision history:
 *  10-Mar-00 - Original Version
 *
 * Description:	source code for sincosf subroutine
 *
 * ====================================================================
 * ====================================================================
 */

#ifdef _CALL_MATHERR
#include <stdio.h>
#include <math.h>
#include <errno.h>
#endif

#include "libm.h"
#include "complex.h"

#if defined(mips) && !defined(__GNUC__)
extern	void	sincosf(float, float *, float *);

#pragma weak sincosf = __sincosf
#endif

#if defined(BUILD_OS_DARWIN) /* Mach-O doesn't support aliases */
extern void __sincosf(float, float *, float *);
#pragma weak sincosf
void sincosf(float x, float *sinx, float *cosx) {
  return __sincosf(x, sinx, cosx);
}
#elif defined(__GNUC__)
extern  void  __sincosf(float, float *, float *);
void    sincosf(float, float *, float *) __attribute__ ((weak, alias ("__sincosf")));
#endif

/* coefficients for polynomial approximation of sin on +/- pi/4 */

static const du  S[] =
{
{D(0x3ff00000, 0x00000000)},
{D(0xbfc55554, 0x5268a030)},
{D(0x3f811073, 0xafd14db9)},
{D(0xbf29943e, 0x0fc79aa9)},
};

/* coefficients for polynomial approximation of cos on +/- pi/4 */

static const du  C[] =
{
{D(0x3ff00000, 0x00000000)},
{D(0xbfdffffb, 0x2a77e083)},
{D(0x3fa553e7, 0xf02ac8aa)},
{D(0xbf5644d6, 0x2993c4ad)},
};

static const du  rpiby2 =
{D(0x3fe45f30, 0x6dc9c883)};

static const du  piby2hi =
{D(0x3ff921fb, 0x50000000)};

static const du  piby2lo =
{D(0x3e5110b4, 0x611a6263)};

static const	fu	Qnan = {QNANF};

static const fu Inf = {0x7f800000};


/* ====================================================================
 *
 * FunctionName    __sincosf
 *
 * Description    computes sinf(arg) , cosf(arg)
 *
 * Note:  Routine __sincosf() computes sinf(arg) and cosf(arg) and should
 *	  be kept in sync with sinf() and cosf() so that it returns 
 *	  the same result as one gets by calling sinf() and cosf() 
 *	  separately.
 *
 * ====================================================================
 */

void
__sincosf(float x, float *sinx, float *cosx)
{
int  n;
int  ix, xpt;
double  dx, xsq;
double  dn;
double  sinpoly, cospoly;
complex  result;
#ifdef _CALL_MATHERR
struct exception	exstruct;
#endif

	FLT2INT(x, ix);	/* copy arg to an integer */

	xpt = (ix >> (MANTWIDTH-1));
	xpt &= 0x1ff;

	/* xpt is exponent(x) + 1 bit of mantissa */
	
	if ( xpt < 0xfd )
	{
		/* |x| < .75 */

		if ( xpt >= 0xe6 )
		{
			/* |x| >= 2^(-12) */

			dx = x;

			xsq = dx*dx;

			*cosx = ((C[3].d*xsq + C[2].d)*xsq + C[1].d)*xsq + C[0].d;
			*sinx = ((S[3].d*xsq + S[2].d)*xsq + S[1].d)*(xsq*dx) + dx;

			return;
		}
		else
		{
			*sinx = x;
			*cosx = 1.0f;

			return;
		}

	}

	if (xpt < 0x136)
	{
		/*  |x| < 2^28  */

		dx = x;
		dn = dx*rpiby2.d;

		n = ROUND(dn);
		dn = n;

		dx = dx - dn*piby2hi.d;
		dx = dx - dn*piby2lo.d;  /* dx = x - n*piby2 */

		xsq = dx*dx;

		sinpoly = ((S[3].d*xsq + S[2].d)*xsq + S[1].d)*(xsq*dx) + dx;
		cospoly = ((C[3].d*xsq + C[2].d)*xsq + C[1].d)*xsq + C[0].d;

		if ( n&1 )
		{
			if ( n&2 )
			{
				/*
				 *  n%4 = 3
				 *  result is (-cos(x), sin(x))
				 */

				*sinx = -cospoly;
				*cosx = sinpoly;
			}
			else
			{
				/*
				 *  n%4 = 1
				 *  result is (cos(x), -sin(x))
				 */

				*sinx = cospoly;
				*cosx = -sinpoly;
			}

			return;
		}

		if ( n&2 )
		{
			/*
			 *  n%4 = 2
			 *  result is (-sin(x), -cos(x))
			 */

			*sinx = -sinpoly;
			*cosx = -cospoly;
		}
		else
		{
			/*
			 *  n%4 = 0
			 *  result is (sin(x), cos(x))
			 */

			*sinx = sinpoly;
			*cosx = cospoly;
		}

		return;
	}

	if ( (x != x) || (fabsf(x) == Inf.f) )
	{
		/* x is a NaN or +/-inf; return a pair of quiet NaNs */

#ifdef _CALL_MATHERR

                exstruct.type = DOMAIN;
                exstruct.name = "cosf";
                exstruct.arg1 = x;
                exstruct.retval = Qnan.f;

                if ( matherr( &exstruct ) == 0 )
                {
                        fprintf(stderr, "domain error in cosf\n");
                        SETERRNO(EDOM);
                }

		*cosx = exstruct.retval;

                exstruct.type = DOMAIN;
                exstruct.name = "sinf";
                exstruct.arg1 = x;
                exstruct.retval = Qnan.f;

                if ( matherr( &exstruct ) == 0 )
                {
                        fprintf(stderr, "domain error in sinf\n");
                        SETERRNO(EDOM);
                }

		*sinx = exstruct.retval;

                return;
#else
		NAN_SETERRNO(EDOM);
		
		*cosx = *sinx = Qnan.f;

		return;
#endif
	}

	/* just give up and return 0.0, setting errno = ERANGE */

#ifdef _CALL_MATHERR

		exstruct.type = TLOSS;
		exstruct.name = "cosf";
		exstruct.arg1 = x;
		exstruct.retval = 0.0f;

		if ( matherr( &exstruct ) == 0 )
		{
			fprintf(stderr, "range error in cosf (total loss \
of significance)\n");
			SETERRNO(ERANGE);
		}

		*cosx = exstruct.retval;

		exstruct.type = TLOSS;
		exstruct.name = "sinf";
		exstruct.arg1 = x;
		exstruct.retval = 0.0f;

		if ( matherr( &exstruct ) == 0 )
		{
			fprintf(stderr, "range error in sinf (total loss \
of significance)\n");
			SETERRNO(ERANGE);
		}

		*sinx = exstruct.retval;

		return;

#else
		SETERRNO(ERANGE);

		*cosx = *sinx = 0.0f;

		return;
#endif

}


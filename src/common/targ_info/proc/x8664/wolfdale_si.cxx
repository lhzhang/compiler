/*
 * Copyright (C) 2008 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*
 *  Copyright (C) 2008 PathScale, LLC.  All Rights Reserved.
 */

/*
 *  Copyright (C) 2007 QLogic Corporation.  All Rights Reserved.
 */

/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


//  Core processor scheduling information
///////////////////////////////////////////////////////////////////
//  
//  Description:
//
//  Generate a scheduling description of an Intel Core processor
//  via the si_gen interface.
//
///////////////////////////////////////////////////////////////////


//  $Revision: 1.29 $
//  $Date: 05/11/10 18:45:11-08:00 $
//  $Author: tkong@hyalite.keyresearch $


#include "si_gen.h"
#include "targ_isa_subset.h"
#include "topcode.h"


int main (int argc, char *argv[])
{
  Machine( "wolfdale", ISA_SUBSET_x86_64, argc, argv );

  const RESOURCE res_issue  = RESOURCE_Create( "issue",  3 );
  const RESOURCE res_alu    = RESOURCE_Create( "alu",    3 );
  const RESOURCE res_agu    = RESOURCE_Create( "agu",    3 );
  const RESOURCE res_fmul   = RESOURCE_Create( "fmul",   1 );
  const RESOURCE res_fadd   = RESOURCE_Create( "fadd",   1 );
  const RESOURCE res_fstore = RESOURCE_Create( "fstore", 1 );

  Instruction_Group( "wolfdale simple alu",
		     TOP_ijmpx,
		     TOP_icallx,
		     TOP_ijmpxx,
		     TOP_icallxx,
		     TOP_ijmpxxx,
		     TOP_icallxxx,
		     TOP_add32,
		     TOP_adc32,
		     TOP_add64,
		     TOP_addi32,
		     TOP_adci32,
		     TOP_addi64,
		     TOP_inc8,
		     TOP_inc16,
		     TOP_inc32,
		     TOP_inc64,
		     TOP_dec8,
		     TOP_dec16,
		     TOP_dec32,
		     TOP_dec64,
		     TOP_and8,
		     TOP_and16,
		     TOP_and32,
		     TOP_and64,
		     TOP_andi32,
		     TOP_andi64,
		     TOP_cmovb,
		     TOP_cmovae,
		     TOP_cmovp,
		     TOP_cmovnp,
		     TOP_cmove,
		     TOP_cmovne,
		     TOP_cmovbe,
		     TOP_cmova,
		     TOP_cmovl,
		     TOP_cmovge,
		     TOP_cmovle,
		     TOP_cmovg,
		     TOP_cmovs,
		     TOP_cmovns,
		     TOP_cmp8,
		     TOP_cmp16,
		     TOP_cmp32,
		     TOP_cmp64,
		     TOP_cmpi8,
		     TOP_cmpi16,
		     TOP_cmpi32,
		     TOP_cmpi64,
		     TOP_neg32,
		     TOP_neg64,
		     TOP_not32,
		     TOP_not64,
		     TOP_or8,
		     TOP_or16,
		     TOP_or32,
		     TOP_or64,
		     TOP_ori32,
		     TOP_ori64,
		     TOP_ror8,
		     TOP_ror16,
		     TOP_ror32,
		     TOP_ror64,
		     TOP_rori8,
		     TOP_rori16,
		     TOP_rori32,
		     TOP_rori64,
		     TOP_rol8,
		     TOP_rol16,
		     TOP_rol32,
		     TOP_rol64,
		     TOP_roli8,
		     TOP_roli16,
		     TOP_roli32,
		     TOP_roli64,
		     TOP_sar32,
		     TOP_sar64,
		     TOP_sari32,
		     TOP_sari64,
		     TOP_shl32,
		     TOP_shld32,
		     TOP_shrd32,
		     TOP_shl64,
		     TOP_shli32,
		     TOP_shldi32,
		     TOP_shrdi32,
		     TOP_shli64,
		     TOP_shr32,
		     TOP_shr64,
		     TOP_shri32,
		     TOP_shri64,
		     TOP_sub32,
		     TOP_sbb32,
		     TOP_sub64,
		     TOP_subi32,
		     TOP_sbbi32,
		     TOP_subi64,
		     TOP_xor8,
		     TOP_xor16,
		     TOP_xor32,
		     TOP_xor64,
		     TOP_xori32,
		     TOP_xori64,
		     TOP_zero32,
		     TOP_zero64,
		     TOP_test32,
		     TOP_test64,
		     TOP_testi32,
		     TOP_testi64,
		     TOP_setb,
		     TOP_setae,
		     TOP_setp,
		     TOP_setnp,
		     TOP_sete,
		     TOP_setne,
		     TOP_setbe,
		     TOP_seta,
		     TOP_setl,
		     TOP_setge,
		     TOP_setle,
		     TOP_setg,
		     TOP_mov32,
		     TOP_mov64,
		     TOP_movsbl,
		     TOP_movzbl,
		     TOP_movswl,
		     TOP_movzwl,
		     TOP_movsbq,
		     TOP_movzbq,
		     TOP_movswq,
		     TOP_movzwq,
		     TOP_movslq,
		     TOP_movzlq,
		     TOP_ldc32,
		     TOP_ldc64,
		     TOP_movabsq,
		     TOP_cltd,
		     TOP_cqto,
		     TOP_bsf32, // guess!
		     TOP_bsf64, // guess!
		     TOP_bsr32, // guess!
		     TOP_bsr64, // guess!
		     TOP_mov64_m,
		     TOP_cmpeqss,
		     TOP_cmpltss,
		     TOP_cmpless,
		     TOP_cmpunordss,
		     TOP_cmpneqss,
		     TOP_cmpnltss,
		     TOP_cmpnless,
		     TOP_cmpordss,
		     TOP_cmpeqsd,
		     TOP_cmpltsd,
		     TOP_cmplesd,
		     TOP_cmpunordsd,
		     TOP_cmpneqsd,
		     TOP_cmpnltsd,
		     TOP_cmpnlesd,
		     TOP_cmpordsd,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "simple alu w/ mem operand",
		     TOP_addxxx32,
		     TOP_addxxx64,
		     TOP_subxxx32,
		     TOP_subxxx64,
		     TOP_xorxxx8,
		     TOP_xorxxx16,
		     TOP_xorxxx32,
		     TOP_xorxxx64,
		     TOP_orxxx8,
		     TOP_orxxx16,
		     TOP_orxxx32,
		     TOP_orxxx64,
		     TOP_andxxx8,
		     TOP_andxxx16,
		     TOP_andxxx32,
		     TOP_addxxxss,
		     TOP_addxxxsd,
		     TOP_subxxxss,
		     TOP_subxxxsd,
		     TOP_andxxx64,
		     TOP_cmpxxx8,
		     TOP_cmpxxx16,
		     TOP_cmpxxx32,
		     TOP_cmpxxx64,
		     TOP_cmpxxxr8,
		     TOP_cmpxxxr16,
		     TOP_cmpxxxr32,
		     TOP_cmpxxxr64,
		     TOP_cmpxxxi8,
		     TOP_cmpxxxi16,
		     TOP_cmpxxxi32,
		     TOP_cmpxxxi64,
		     TOP_testxxx32,
		     TOP_testxxx64,
		     TOP_andx32,
		     TOP_andxx32,
		     TOP_orx32,
		     TOP_orxx32,
		     TOP_orx64,
		     TOP_orxx64,
		     TOP_xorx32,
		     TOP_xorxx32,
		     TOP_xorx64,
		     TOP_xorxx64,
		     TOP_addx32,
		     TOP_addx64,
		     TOP_addxx32,
		     TOP_addxx64,
		     TOP_subx32,
		     TOP_subx64,
		     TOP_subxx32,
		     TOP_subxx64,
		     TOP_testx32,
		     TOP_testxx32,
		     TOP_testx64,
		     TOP_testxx64,
		     TOP_cmpx8,
		     TOP_cmpxx8,
		     TOP_cmpxr8,
		     TOP_cmpxxr8,
		     TOP_cmpxi8,
		     TOP_cmpxxi8,
		     TOP_cmpx16,
		     TOP_cmpxx16,
		     TOP_cmpxr16,
		     TOP_cmpxxr16,
		     TOP_cmpxi16,
		     TOP_cmpxxi16,
		     TOP_cmpx32,
		     TOP_cmpxx32,
		     TOP_cmpxr32,
		     TOP_cmpxxr32,
		     TOP_cmpxr32,
		     TOP_cmpxxr32,
		     TOP_cmpxi32,
		     TOP_cmpxxi32,
		     TOP_cmpx64,
		     TOP_cmpxx64,
		     TOP_cmpxr64,
		     TOP_cmpxxr64,
		     TOP_cmpxi64,
		     TOP_cmpxxi64,
		     TOP_xorx8,
		     TOP_xorx16,
		     TOP_xorxx8,
		     TOP_xorxx16,
		     TOP_orx8,
		     TOP_orx16,
		     TOP_orxx8,
		     TOP_orxx16,
		     TOP_andx8,
		     TOP_andx16,
		     TOP_andx64,
		     TOP_andxx8,
		     TOP_andxx16,
		     TOP_andxx64,
		     TOP_lock_add8,
		     TOP_lock_add16,
		     TOP_lock_add32,
		     TOP_lock_adc32,
		     TOP_lock_add64,
		     TOP_lock_and8,
		     TOP_lock_and16,
		     TOP_lock_and32,
		     TOP_lock_and64,
		     TOP_lock_or8,
		     TOP_lock_or16,
		     TOP_lock_or32,
		     TOP_lock_or64,
		     TOP_lock_xor8,
		     TOP_lock_xor16,
		     TOP_lock_xor32,
		     TOP_lock_xor64,
		     TOP_lock_sub8,
		     TOP_lock_sub16,
		     TOP_lock_sub32,
		     TOP_lock_sub64,
		     TOP_lock_xadd8,
		     TOP_lock_xadd16,
		     TOP_lock_xadd32,
		     TOP_lock_xadd64,
		     TOP_lock_cmpxchg8,
		     TOP_lock_cmpxchg16,
		     TOP_lock_cmpxchg32,
		     TOP_lock_cmpxchg64,
		     TOP_pextrw,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "packed insert",
                     TOP_pinsrw,
                     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "lea",
		     TOP_lea32,
		     TOP_lea64,
		     TOP_leax32,
		     TOP_leax64,
		     TOP_leaxx32,
		     TOP_leaxx64,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(2);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_agu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "push/pop",
		     TOP_pushl,
		     TOP_pushq,
		     TOP_popl,
		     TOP_popq,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "ret near",
		     TOP_ret,
		     TOP_reti,
		     TOP_fcmovb,
		     TOP_fcmovbe,
		     TOP_fcmovnb,
		     TOP_fcmovnbe,
		     TOP_fcmove,
		     TOP_fcmovne,
		     TOP_fcmovu,
		     TOP_fcmovnu,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "call",
		     TOP_call,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "indirect call/jump",
		     TOP_icall,
		     TOP_ijmp,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "imult32",
		     TOP_mul32,
		     TOP_imul32,
		     TOP_imulx32,
		     TOP_imuli32,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "imult64",
		     TOP_mulx64,
		     TOP_imul64,
		     TOP_imuli64,
		     TOP_imulx64,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("leave",
		    TOP_leave,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("enter",
		    TOP_enter,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(21);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("div32",
		    TOP_div32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(18);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("div64",
		    TOP_div64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(32);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("idiv32",
		    TOP_idiv32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(22);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("idiv64",
		    TOP_idiv64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(32);
  Resource_Requirement(res_alu, 0);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("load8_16/32/64",
		    TOP_ld8_abs,
		    TOP_ld16_abs,
		    TOP_ld8_32_n32,
		    TOP_ldu8_32_n32,
		    TOP_ld16_32_n32,
		    TOP_ldu16_32_n32,
		    TOP_ld8_32,
		    TOP_ldx8_32,
		    TOP_ldxx8_32,
		    TOP_ldu8_32,
		    TOP_ldxu8_32,
		    TOP_ldxxu8_32,
		    TOP_ld16_32,
		    TOP_ldx16_32,
		    TOP_ldxx16_32,
		    TOP_ldu16_32,
		    TOP_ldxu16_32,
		    TOP_ldxxu16_32,
		    TOP_ld8_64,
		    TOP_ldx8_64,
		    TOP_ldxx8_64,
		    TOP_ld8_64_off,
		    TOP_ldu8_64,
		    TOP_ldxu8_64,
		    TOP_ldxxu8_64,
		    TOP_ldu8_64_off,
		    TOP_ld16_64,
		    TOP_ldx16_64,
		    TOP_ldxx16_64,
		    TOP_ld16_64_off,
		    TOP_ldu16_64,
		    TOP_ldxu16_64,
		    TOP_ldxxu16_64,
		    TOP_ldu16_64_off,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_agu, 0);
  Load_Access_Time(3);

  Instruction_Group("load32/64",
		    TOP_ld64,
		    TOP_ld32,
		    TOP_ld32_n32,
		    TOP_ld32_64_off,
		    TOP_ld64_off,
		    TOP_ldx64,
		    TOP_ldxx64,
		    TOP_ldx32,
		    TOP_ldxx32,
		    TOP_ld32_64,
		    TOP_ldx32_64,
		    TOP_ldxx32_64,
		    TOP_ld32_abs,
		    TOP_ld64_abs,
		    TOP_ld64_2m,
		    TOP_ld64_2m_n32,
		    TOP_ld32_gs_seg_off,
		    TOP_ld64_fs_seg_off,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_agu, 0);
  Load_Access_Time(3);

  Instruction_Group("store",
		    TOP_store8_n32,
		    TOP_store16_n32,
		    TOP_store32_n32,
		    TOP_store64_off,
		    TOP_store8,
		    TOP_storex8,
		    TOP_storexx8,
		    TOP_store16,
		    TOP_storex16,
		    TOP_storexx16,
		    TOP_store32,
		    TOP_store64,
		    TOP_storex32,
		    TOP_storex64,
		    TOP_storexx32,
		    TOP_storexx64,
		    TOP_storenti32,
		    TOP_storentix32,
		    TOP_storentixx32,
		    TOP_storenti64,
		    TOP_storentix64,
		    TOP_storentixx64,
		    TOP_store8_abs,
		    TOP_store16_abs,
		    TOP_store32_abs,
		    TOP_store64_abs,
		    TOP_store64_fm,
		    TOP_store64_fm_n32,
		    TOP_storent64_fm,
		    TOP_maskmovq,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_agu, 0);
  Store_Available_Time(3);

  Instruction_Group("branch",
		    TOP_jb,
		    TOP_jae,
		    TOP_jp,
		    TOP_jnp,
		    TOP_je,
		    TOP_jne,
		    TOP_jbe,
		    TOP_ja,
		    TOP_jge,
		    TOP_jl,
		    TOP_jle,
		    TOP_jg,
		    TOP_jcxz,
		    TOP_jecxz,
		    TOP_jrcxz,
		    TOP_js,
		    TOP_jns,
		    TOP_jmp,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_agu, 0);

  Instruction_Group( "sqrtss",
		     TOP_sqrtss,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(29);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group( "sqrtsd",
		     TOP_sqrtsd,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(58);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group( "float-mov",
		     TOP_movsd,
		     TOP_movss,
		     TOP_movdq,
		     TOP_movapd,
		     TOP_movaps,
		     TOP_movhlps,
		     TOP_movlhps, 
		     TOP_fmov, 
		     TOP_fchs,
		     TOP_fabs,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd,  0);

  Instruction_Group( "float-mov-cmp",
                     TOP_cmpsd, 
                     TOP_cmpss, 
                     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd,  0);

  Instruction_Group( "float-load",
		     TOP_ldss_n32,
		     TOP_ldsd_n32,
		     TOP_ldss,
		     TOP_ldsd,
		     TOP_ldssx,
		     TOP_ldssxx,
		     TOP_ldsdx,
		     TOP_ldsdxx,
		     TOP_lddqa,
		     TOP_lddqa_n32,
		     TOP_lddqax,
		     TOP_lddqaxx,
		     TOP_fld,
		     TOP_flds,
		     TOP_flds_n32,
		     TOP_fldl,
		     TOP_fldl_n32,
		     TOP_fldt,
		     TOP_fldt_n32,
		     TOP_fldcw,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Load_Access_Time(3);

  Instruction_Group( "float-load vector",
		     TOP_lddqu,
		     TOP_lddqu_n32,
		     TOP_lddqux,
		     TOP_lddquxx,
		     TOP_ldupd,
		     TOP_ldupd_n32,
		     TOP_ldups,
		     TOP_ldups_n32,
		     TOP_ldmxcsr,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Load_Access_Time(3);

  Instruction_Group( "float-load vector II",
		     TOP_ldapd,
		     TOP_ldapd_n32,
		     TOP_ldapdx,
		     TOP_ldapdxx,
		     TOP_ldaps,
		     TOP_ldaps_n32,
		     TOP_ldapsx,
		     TOP_ldapsxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Load_Access_Time(3);

  Instruction_Group( "float-load vector low-high packed-single",
		     TOP_ldlps,
		     TOP_ldlps_n32,
		     TOP_ldlpsx,
		     TOP_ldlpsxx,
		     TOP_ldhps,
		     TOP_ldhps_n32,
		     TOP_ldhpsx,
		     TOP_ldhpsxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Load_Access_Time(3);

  Instruction_Group( "float-store vector low-high packed-single",
		     TOP_stlps,
		     TOP_stlps_n32,
		     TOP_stlpsx,
		     TOP_stlpsxx,
		     TOP_sthps,
		     TOP_sthps_n32,
		     TOP_sthpsx,
		     TOP_sthpsxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);
  Store_Available_Time(3);

  Instruction_Group( "float-load vector low-high packed-double",
		     TOP_ldlpd,
		     TOP_ldlpd_n32,
		     TOP_ldlpdx,
		     TOP_ldlpdxx,
		     TOP_ldhpd,
		     TOP_ldhpd_n32,
		     TOP_ldhpdx,
		     TOP_ldhpdxx,
		     TOP_ld64_2sse,
		     TOP_ld64_2sse_n32,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Load_Access_Time(3);

  Instruction_Group( "float-store vector low-high packed-double",
		     TOP_stlpd,
		     TOP_stlpd_n32,
		     TOP_stlpdx,
		     TOP_stlpdxx,
		     TOP_sthpd,
		     TOP_sthpd_n32,
		     TOP_sthpdx,
		     TOP_sthpdxx,
		     TOP_storelpd,
		     TOP_store64_fsse,
		     TOP_store64_fsse_n32,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);
  Store_Available_Time(3);

  Instruction_Group( "float-store",
		     TOP_stss_n32,
		     TOP_stsd_n32,
		     TOP_stss,
		     TOP_stsd,
		     TOP_stntss,
		     TOP_stntsd,
		     TOP_stssx,
		     TOP_stssxx,
		     TOP_stntssx,
		     TOP_stntssxx,
		     TOP_stsdx,
		     TOP_stsdxx,
		     TOP_stntsdx,
		     TOP_stntsdxx,
		     TOP_stdqa,
		     TOP_stdqa_n32,
		     TOP_stdqax,
		     TOP_stdqaxx,
		     TOP_stntpd,
		     TOP_stntpdx,
		     TOP_stntpdxx,
		     TOP_stntps,
		     TOP_stntpsx,
		     TOP_stntpsxx,
		     TOP_fstp,
		     TOP_fst,
		     TOP_fstps,
		     TOP_fstps_n32,
		     TOP_fstpl,
		     TOP_fstpl_n32,
		     TOP_fstpt,
		     TOP_fstpt_n32,
		     TOP_fsts,
		     TOP_fsts_n32,
		     TOP_fstl,
		     TOP_fstl_n32,
		     TOP_fnstcw,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);
  Store_Available_Time(3);

  Instruction_Group( "float-store vector",
		     TOP_stdqu,
		     TOP_stdqu_n32,
		     TOP_stdqux,
		     TOP_stdquxx,
		     TOP_stupd,
		     TOP_stups,
		     TOP_maskmovdqu,
		     TOP_stmxcsr,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);
  Store_Available_Time(3);

  Instruction_Group( "float-store vector II",
		     TOP_staps,
		     TOP_staps_n32,
		     TOP_stapsx,
		     TOP_stapsxx,
		     TOP_stapd,
		     TOP_stapd_n32,
		     TOP_stapdx,
		     TOP_stapdxx,
		     TOP_storenti128,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);
  Store_Available_Time(3);

  Instruction_Group( "vector cvt I",
		     TOP_cvtdq2pd,
		     TOP_cvtdq2ps,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt II",
		     TOP_cvtps2pd,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(2);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt III",
		     TOP_cvtpd2ps,
		     TOP_cvtpd2dq,
		     TOP_cvttpd2dq,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt IV",
		     TOP_cvtps2dq,
		     TOP_cvttps2dq,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt I w/ memory operand",
		     TOP_cvtdq2pd_x,
		     TOP_cvtdq2ps_x,
		     TOP_cvtdq2pd_xx,
		     TOP_cvtdq2ps_xx,
		     TOP_cvtdq2pd_xxx,
		     TOP_cvtdq2ps_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt II w/ memory operand",
		     TOP_cvtps2pd_x,
		     TOP_cvtps2pd_xx,
		     TOP_cvtps2pd_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt III w/ memory operand",
		     TOP_cvtpd2ps_x,
		     TOP_cvttpd2dq_x,
		     TOP_cvtpd2ps_xx,
		     TOP_cvttpd2dq_xx,
		     TOP_cvtpd2ps_xxx,
		     TOP_cvttpd2dq_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "vector cvt IV w/ memory operand",
		     TOP_cvtps2dq_x,
		     TOP_cvtps2dq_xx,
		     TOP_cvtps2dq_xxx,
		     TOP_cvttps2dq_x,
		     TOP_cvttps2dq_xx,
		     TOP_cvttps2dq_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "int-2-float w/ mem",
		     TOP_cvtsi2ss_x,
		     TOP_cvtsi2ss_xx,
		     TOP_cvtsi2ss_xxx,
		     TOP_cvtsi2ssq_x,
		     TOP_cvtsi2ssq_xx,
		     TOP_cvtsi2ssq_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "int-2-float",
		     TOP_cvtsi2ss,
		     TOP_cvtsi2ssq,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "double-2-float",
		     TOP_cvtsd2ss,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "double-2-float w/ mem",
		     TOP_cvtsd2ss_x,
		     TOP_cvtsd2ss_xx,
		     TOP_cvtsd2ss_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "int-2-double",
		     TOP_cvtsi2sd,
		     TOP_cvtsi2sdq,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "int-2-double w/ mem",
		     TOP_cvtsi2sd_x,
		     TOP_cvtsi2sd_xx,
		     TOP_cvtsi2sd_xxx,
		     TOP_cvtsi2sdq_x,
		     TOP_cvtsi2sdq_xx,
		     TOP_cvtsi2sdq_xxx,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "int-2-float",
		     TOP_movg2x,
		     TOP_movg2x64,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group( "float-2-int",
		     TOP_movx2g,
		     TOP_movx2g64,
		     TOP_cvtss2sd,
		     TOP_cvtss2si,
		     TOP_cvtsd2si,
		     TOP_cvtss2siq,
		     TOP_cvtsd2siq,
		     TOP_cvttss2si,
		     TOP_cvttsd2si,
		     TOP_cvttss2siq,
		     TOP_cvttsd2siq,
		     TOP_pmovmskb128,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group("float-alu",
		    TOP_addss,
		    TOP_addsd,
		    TOP_subsd,
		    TOP_subss,
		    TOP_fadd,
		    TOP_faddp,
		    TOP_fsub,
		    TOP_fsubp,
		    TOP_fsubr,
		    TOP_fsubrp,
		    TOP_fmul,
		    TOP_fmulp,
		    TOP_fucomi,
		    TOP_fucomip,
		    TOP_fistps,
		    TOP_fistpl,
		    TOP_fists,
		    TOP_fistl,
		    TOP_fistpll,
		    TOP_fisttps,
		    TOP_fisttpl,
		    TOP_fisttpll,
		    TOP_fldz,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  
  Instruction_Group("float-alu comi",
                    TOP_comisd,
                    TOP_comiss,
                    TOP_fxch,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class I",
		    TOP_faddsub128v32,
		    TOP_faddsub128v64,
		    TOP_fhadd128v32,
		    TOP_fhadd128v64,
		    TOP_fhsub128v32,
		    TOP_fhsub128v64,
		    TOP_fadd128v32,
		    TOP_fadd128v64,
		    TOP_fsub128v32,
		    TOP_fsub128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class I w/ memory operand",
		    TOP_faddsubx128v32,
		    TOP_faddsubx128v64,
		    TOP_fhaddx128v32,
		    TOP_fhaddx128v64,
		    TOP_fhsubx128v32,
		    TOP_fhsubx128v64,
		    TOP_faddsubxx128v32,
		    TOP_faddsubxx128v64,
		    TOP_fhaddxx128v32,
		    TOP_fhaddxx128v64,
		    TOP_fhsubxx128v32,
		    TOP_fhsubxx128v64,
		    TOP_faddsubxxx128v32,
		    TOP_faddsubxxx128v64,
		    TOP_fhaddxxx128v32,
		    TOP_fhaddxxx128v64,
		    TOP_fhsubxxx128v32,
		    TOP_fhsubxxx128v64,
		    TOP_faddx128v32,
		    TOP_faddx128v64,
		    TOP_fsubx128v32,
		    TOP_fsubx128v64,
		    TOP_faddxx128v32,
		    TOP_faddxx128v64,
		    TOP_fsubxx128v32,
		    TOP_fsubxx128v64,
		    TOP_faddxxx128v32,
		    TOP_faddxxx128v64,
		    TOP_fsubxxx128v32,
		    TOP_fsubxxx128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class II",
		    TOP_fand128v32,
		    TOP_fand128v64,
		    TOP_for128v32,
		    TOP_for128v64,
		    TOP_fxor128v32,
		    TOP_fxor128v64,
		    TOP_andps,
		    TOP_andpd,
		    TOP_xorps,
		    TOP_xorpd,
		    TOP_xzero32,
		    TOP_xzero64,
		    TOP_xzero128v32,
		    TOP_xzero128v64,
		    TOP_orps,
		    TOP_orpd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class II w/ memory operand",
		    TOP_fandx128v32,
		    TOP_fandx128v64,
		    TOP_forx128v32,
		    TOP_forx128v64,
		    TOP_fxorx128v32,
		    TOP_fxorx128v64,
		    TOP_fandxx128v32,
		    TOP_fandxx128v64,
		    TOP_forxx128v32,
		    TOP_forxx128v64,
		    TOP_fxorxx128v32,
		    TOP_fxorxx128v64,
		    TOP_fandxxx128v32,
		    TOP_fandxxx128v64,
		    TOP_forxxx128v32,
		    TOP_forxxx128v64,
		    TOP_fxorxxx128v32,
		    TOP_fxorxxx128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class III - mulps",
		    TOP_fmul128v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class III - mulpd",
                    TOP_fmul128v64,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class III - mulps w/memory operand",
		    TOP_fmulx128v32,
		    TOP_fmulxx128v32,
		    TOP_fmulxxx128v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class III - mulpd w/memory operand",
                    TOP_fmulx128v64,
                    TOP_fmulxx128v64,
                    TOP_fmulxxx128v64,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(7);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class IV -divps",
		    TOP_fdiv128v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(18);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class IV - fsqrt",
                    TOP_fsqrt,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(29);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("x87 cosine",
		    TOP_fcos,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(92);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("x87 sine",
		    TOP_fsin,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(93);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("float-alu for float vector class IV -divps w/ memory operand",
		    TOP_fdivx128v32,
		    TOP_fdivxx128v32,
		    TOP_fdivxxx128v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(20);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class V",
		    TOP_fdiv128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(32);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class V w/ memory operand",
		    TOP_fdivx128v64,
		    TOP_fdivxx128v64,
		    TOP_fdivxxx128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(34);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class VI",
		    TOP_fmax128v32,
		    TOP_fmax128v64,
		    TOP_fmin128v32,
		    TOP_fmin128v64,
		    TOP_psadbw,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class VI w/ memory operand",
		    TOP_fmaxx128v32,
		    TOP_fmaxx128v64,
		    TOP_fminx128v32,
		    TOP_fminx128v64,
		    TOP_fmaxxx128v32,
		    TOP_fmaxxx128v64,
		    TOP_fminxx128v32,
		    TOP_fminxx128v64,
		    TOP_fmaxxxx128v32,
		    TOP_fmaxxxx128v64,
		    TOP_fminxxx128v32,
		    TOP_fminxxx128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class VII",
		    TOP_fsqrt128v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(29);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class VIII",
		    TOP_fsqrt128v64,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(58);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class IX",
		    TOP_frsqrt128v32,
		    TOP_frcp128v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class X",
		    TOP_unpcklpd,
		    TOP_unpckhpd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XI",
		    TOP_unpcklps,
		    TOP_unpckhps,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XII 1",
		    TOP_fmovddup,
		    TOP_shufpd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1); 
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XII 1 w/ memory operand",
                    TOP_fmovddupx,
                    TOP_fmovddupxx,
                    TOP_fmovddupxxx,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XII 2",
                    TOP_fmovsldup,
                    TOP_fmovshdup,
                    TOP_pshufd,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(2);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XII 2 w/ memory operand",
                    TOP_fmovsldupx,
                    TOP_fmovshdupx,
                    TOP_fmovsldupxx,
                    TOP_fmovshdupxx,
                    TOP_fmovsldupxxx,
                    TOP_fmovshdupxxx,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XII 4",
                    TOP_shufps,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XIIa",
		    TOP_pshufw,
		    TOP_pshuflw,
		    TOP_pshufhw,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("mmx shuffle",
		    TOP_pshufw64v16,
		    TOP_psllw_mmx,
		    TOP_pslld_mmx,
		    TOP_psrlw_mmx,
		    TOP_psrld_mmx,
		    TOP_psraw_mmx,
		    TOP_psrad_mmx,
		    TOP_pand_mmx,
		    TOP_pandn_mmx,
            TOP_pand128,
		    TOP_por_mmx,
		    TOP_pxor_mmx,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XIII",
		    TOP_psrldq,
		    TOP_psrlq128v64,
		    TOP_pslldq,
		    TOP_psllw,
		    TOP_pslld,
		    TOP_psllq,
		    TOP_psrlw,
		    TOP_psrld,
		    TOP_psrlq,
		    TOP_psraw,
		    TOP_psrad,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(2);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XIV",
		    TOP_punpcklwd,
		    TOP_punpcklbw,
		    TOP_punpckldq,
		    TOP_punpckhbw,
		    TOP_punpckhwd,
		    TOP_punpckhdq,
		    TOP_punpckl64v8,
		    TOP_punpckl64v16,
		    TOP_punpckl64v32,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for float vector class XV",
		    TOP_cmpps,
		    TOP_cmppd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("sse parallel compares",
		    TOP_cmpeqps,
		    TOP_cmpltps,
		    TOP_cmpleps,
		    TOP_cmpunordps,
		    TOP_cmpneqps,
		    TOP_cmpnltps,
		    TOP_cmpnleps,
		    TOP_cmpordps,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class XVI",
		    TOP_cmpgt128v8,
		    TOP_cmpgt128v16,
		    TOP_cmpgt128v32,
		    TOP_cmpeq128v8,
		    TOP_cmpeq128v16,
		    TOP_cmpeq128v32,
		    TOP_pcmpeqb,
		    TOP_pcmpeqw,
		    TOP_pcmpeqd,
		    TOP_pcmpgtb,
		    TOP_pcmpgtw,
		    TOP_pcmpgtd,
		    TOP_max128v8,
		    TOP_max128v16,
		    TOP_max64v8,
		    TOP_max64v16,
		    TOP_min128v8,
		    TOP_min128v16,
		    TOP_min64v8,
		    TOP_min64v16,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for float vector class XVII",
		    TOP_cmpgtx128v8,
		    TOP_cmpgtx128v16,
		    TOP_cmpgtx128v32,
		    TOP_cmpeqx128v8,
		    TOP_cmpeqx128v16,
		    TOP_cmpeqx128v32,
		    TOP_maxx128v8,
		    TOP_maxx128v16,
		    TOP_minx128v8,
		    TOP_minx128v16,
		    TOP_cmpgtxx128v8,
		    TOP_cmpgtxx128v16,
		    TOP_cmpgtxx128v32,
		    TOP_cmpeqxx128v8,
		    TOP_cmpeqxx128v16,
		    TOP_cmpeqxx128v32,
		    TOP_maxxx128v8,
		    TOP_maxxx128v16,
		    TOP_minxx128v8,
		    TOP_minxx128v16,
		    TOP_cmpgtxxx128v8,
		    TOP_cmpgtxxx128v16,
		    TOP_cmpgtxxx128v32,
		    TOP_cmpeqxxx128v8,
		    TOP_cmpeqxxx128v16,
		    TOP_cmpeqxxx128v32,
		    TOP_maxxxx128v8,
		    TOP_maxxxx128v16,
		    TOP_minxxx128v8,
		    TOP_minxxx128v16,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for int vector",
		    TOP_add128v8,
		    TOP_add128v16,
		    TOP_add128v32,
		    TOP_add64v8,
		    TOP_add64v16,
		    TOP_add64v32,
		    TOP_paddsb,
		    TOP_paddsw,
		    TOP_psubsb,
		    TOP_psubsw,
		    TOP_paddusb,
		    TOP_paddusw,
		    TOP_psubusb,
		    TOP_psubusw,
		    TOP_sub128v8,
		    TOP_sub128v16,
		    TOP_sub128v32,		    
		    TOP_sub64v8,
		    TOP_sub64v16,
		    TOP_sub64v32,
		    TOP_and128v8,
		    TOP_and128v16,
		    TOP_and128v32,
		    TOP_and128v64,		    
		    TOP_andn128v8,
		    TOP_andn128v16,
		    TOP_andn128v32,
		    TOP_andn128v64,		    
		    TOP_or128v8,
		    TOP_or128v16,
		    TOP_or128v32,
		    TOP_or128v64,		    
		    TOP_xor128v8,
		    TOP_xor128v16,
		    TOP_xor128v32,
		    TOP_xor128v64,		    
		    TOP_subus128v16,
		    TOP_pavgb,
		    TOP_pavgw,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for int vector I",
                    TOP_add128v64,
                    TOP_paddq,
                    TOP_sub128v64,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(2);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for int vector II",
                    TOP_packsswb,
                    TOP_packssdw,
                    TOP_packuswb,                   
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for int mpy vector",
		    TOP_mul128v16,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-alu for mmx int mpy vector",
		    TOP_pmullw,
		    TOP_pmulhw,
		    TOP_pmulhuw,
		    TOP_pmaddwd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);
		    
  Instruction_Group("float-alu for int vector w/ memory operand",
		    TOP_addx128v8,
		    TOP_addx128v16,
		    TOP_addx128v32,
		    TOP_subx128v8,
		    TOP_subx128v16,
		    TOP_subx128v32,		    
		    TOP_andx128v8,
		    TOP_andx128v16,
		    TOP_andx128v32,
		    TOP_andx128v64,		    
		    TOP_andnx128v8,
		    TOP_andnx128v16,
		    TOP_andnx128v32,
		    TOP_andnx128v64,		    
		    TOP_orx128v8,
		    TOP_orx128v16,
		    TOP_orx128v32,
		    TOP_orx128v64,		    
		    TOP_xorx128v8,
		    TOP_xorx128v16,
		    TOP_xorx128v32,
		    TOP_xorx128v64,		    
		    TOP_addxx128v8,
		    TOP_addxx128v16,
		    TOP_addxx128v32,
		    TOP_subxx128v8,
		    TOP_subxx128v16,
		    TOP_subxx128v32,		    
		    TOP_andxx128v8,
		    TOP_andxx128v16,
		    TOP_andxx128v32,
		    TOP_andxx128v64,		    
		    TOP_andnxx128v8,
		    TOP_andnxx128v16,
		    TOP_andnxx128v32,
		    TOP_andnxx128v64,		    
		    TOP_orxx128v8,
		    TOP_orxx128v16,
		    TOP_orxx128v32,
		    TOP_orxx128v64,		    
		    TOP_xorxx128v8,
		    TOP_xorxx128v16,
		    TOP_xorxx128v32,
		    TOP_xorxx128v64,		    
		    TOP_addxxx128v8,
		    TOP_addxxx128v16,
		    TOP_addxxx128v32,
		    TOP_subxxx128v8,
		    TOP_subxxx128v16,
		    TOP_subxxx128v32,		    
		    TOP_andxxx128v8,
		    TOP_andxxx128v16,
		    TOP_andxxx128v32,
		    TOP_andxxx128v64,		    
		    TOP_andnxxx128v8,
		    TOP_andnxxx128v16,
		    TOP_andnxxx128v32,
		    TOP_andnxxx128v64,		    
		    TOP_orxxx128v8,
		    TOP_orxxx128v16,
		    TOP_orxxx128v32,
		    TOP_orxxx128v64,		    
		    TOP_xorxxx128v8,
		    TOP_xorxxx128v16,
		    TOP_xorxxx128v32,
		    TOP_xorxxx128v64,		    
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu for int vector I w/ memory operand",
                    TOP_addx128v64,                    
                    TOP_subx128v64,
                    TOP_addxx128v64,
                    TOP_subxx128v64,
                    TOP_addxxx128v64,
                    TOP_subxxx128v64,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu w/ memory operand",
		    TOP_addxss,
		    TOP_addxsd,
		    TOP_addxxss,
		    TOP_addxxsd,
		    TOP_subxsd,
		    TOP_subxss,
		    TOP_subxxsd,
		    TOP_subxxss,
		    TOP_filds,
		    TOP_fildl,
		    TOP_fildll,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-alu comi w/ memory operand",
                    TOP_comixsd,
                    TOP_comixxsd,
                    TOP_comixss,
                    TOP_comixxss,
                    TOP_comixxxsd,
                    TOP_comixxxss,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-max/min",
		    TOP_maxsd,
		    TOP_maxss,
		    TOP_minsd,
		    TOP_minss,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("float-xorp",
		    TOP_andnps,
		    TOP_andnpd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-xorp I",
                    TOP_rcpss,
                    TOP_rsqrtss,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-mul-single",
		    TOP_mulss,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-mul-double",
                    TOP_mulsd,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-mul-single w/ mem operand",
		    TOP_mulxss,
		    TOP_mulxxss,
		    TOP_mulxxxss,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-mul-double w/ mem operand",
                    TOP_mulxsd,
                    TOP_mulxxsd,
                    TOP_mulxxxsd,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(7);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-divide-single",
		    TOP_divss,
		    TOP_fdiv,
		    TOP_fdivp,
		    TOP_fdivr,
		    TOP_fdivrp,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(18);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-divide-double",
                    TOP_divsd,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(32);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-divide-single w/ memory operand",
		    TOP_divxss,
		    TOP_divxxss,
		    TOP_divxxxss,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(20);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-divide-double w/ memory operand",
                    TOP_divxsd,
                    TOP_divxxsd,
                    TOP_divxxxsd,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(34);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("float-others",
		    TOP_frndint,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(21);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);

  Instruction_Group("sync",
		    TOP_mfence,
		    TOP_lfence,
		    TOP_sfence,
		    TOP_UNDEFINED);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_agu, 0);

  Instruction_Group("prefetch",
		    TOP_prefetch,
		    TOP_prefetchw,
		    TOP_prefetcht0,
		    TOP_prefetcht1,
		    TOP_prefetchnta,
		    TOP_prefetchx,
		    TOP_prefetchwx,
		    TOP_prefetcht0x,
		    TOP_prefetcht1x,
		    TOP_prefetchntax,
		    TOP_prefetchxx,
		    TOP_prefetchwxx,
		    TOP_prefetcht0xx,
		    TOP_prefetcht1xx,
		    TOP_prefetchntaxx,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_agu, 0);

  Instruction_Group("nop",
		    TOP_nop,
		    TOP_UNDEFINED);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_alu, 0);

  Instruction_Group("emms",
                    TOP_emms,
                    TOP_UNDEFINED);
  Any_Result_Available_Time(6);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fadd, 0);
  Resource_Requirement(res_fmul, 0);
  Resource_Requirement(res_fstore, 0);

  Instruction_Group("mov-int-2-mmx",
                    TOP_movi32_2m,
                    TOP_movi64_2m,
                    TOP_UNDEFINED);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("mov-mmx-2-int",
                    TOP_movm_2i32,
                    TOP_movm_2i64,
		    TOP_pmovmskb,
                    TOP_UNDEFINED);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("mov-mmx-2-sse",
                    TOP_movq2dq,
                    TOP_UNDEFINED);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("mov-sse_2_mmx",
                    TOP_movdq2q,
                    TOP_UNDEFINED);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "vector cvt sse <-> mmx I",
		     TOP_cvtpi2ps,
		     TOP_cvtps2pi,
		     TOP_cvttps2pi,
		     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(3);
  Resource_Requirement(res_issue, 0);

  Instruction_Group( "vector cvt sse <-> mmx II",
                     TOP_cvtpi2pd,
                     TOP_cvtpd2pi,
                     TOP_cvttpd2pi,
                     TOP_UNDEFINED );
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(4);
  Resource_Requirement(res_issue, 0);


  Instruction_Group("SSE movmsk",
		    TOP_movmskps,
		    TOP_movmskpd,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("SSE4A extract",
                    TOP_extrq,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(2);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_fmul, 0);

  Instruction_Group("SSE4A insert",
                    TOP_insertq,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(5);
  Resource_Requirement(res_issue, 0);

  Instruction_Group("dummy",
		    TOP_asm,
		    TOP_intrncall,
		    TOP_spadjust,
		    TOP_savexmms,
		    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(1);
  Resource_Requirement(res_issue, 0);
  Resource_Requirement(res_alu, 0);

  Instruction_Group("dummy_no_res",
                    TOP_swp_start,
                    TOP_swp_stop,
                    TOP_UNDEFINED);
  Any_Operand_Access_Time(0);
  Any_Result_Available_Time(0);

  Machine_Done("wolfdale.c");
}

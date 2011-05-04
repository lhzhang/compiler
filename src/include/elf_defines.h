/*	$NetBSD: exec_elf.h,v 1.108 2010/12/24 12:41:43 skrll Exp $	*/

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ELF_DEFINES_H_
#define	ELF_DEFINES_H_

/*
 * The current ELF ABI specification is available at:
 *	http://www.sco.com/developers/gabi/
 *
 * Current header definitions are in:
 *	http://www.sco.com/developers/gabi/latest/ch4.eheader.html
 */

#include <inttypes.h>

typedef	uint8_t  	Elf_Byte;

typedef	uint32_t	Elf32_Addr;
#define	ELF32_FSZ_ADDR	4
typedef	uint32_t	Elf32_Off;
typedef	int32_t		Elf32_SOff;
#define	ELF32_FSZ_OFF	4
typedef	int32_t		Elf32_Sword;
#define	ELF32_FSZ_SWORD	4
typedef	uint32_t	Elf32_Word;
#define	ELF32_FSZ_WORD	4
typedef	uint16_t	Elf32_Half;
#define	ELF32_FSZ_HALF	2
typedef	uint64_t	Elf32_Lword;
#define	ELF32_FSZ_LWORD	8

typedef	uint64_t	Elf64_Addr;
#define	ELF64_FSZ_ADDR	8
typedef	uint64_t	Elf64_Off;
typedef	int64_t		Elf64_SOff;
#define	ELF64_FSZ_OFF	8
typedef	int32_t		Elf64_Shalf;
#define	ELF64_FSZ_SHALF	4

typedef	int32_t		Elf64_Sword;
#define	ELF64_FSZ_SWORD	4
typedef	uint32_t	Elf64_Word;
#define	ELF64_FSZ_WORD	4

typedef	int64_t		Elf64_Sxword;
#define	ELF64_FSZ_SXWORD 8
typedef	uint64_t	Elf64_Xword;
#define	ELF64_FSZ_XWORD	8
typedef	uint64_t	Elf64_Lword;
#define	ELF64_FSZ_LWORD	8
typedef	uint16_t	Elf64_Half;
#define	ELF64_FSZ_HALF 2

/*
 * ELF Header
 */
#define	ELF_NIDENT	16

typedef struct {
	unsigned char	e_ident[ELF_NIDENT];	/* Id bytes */
	Elf32_Half	e_type;			/* file type */
	Elf32_Half	e_machine;		/* machine type */
	Elf32_Word	e_version;		/* version number */
	Elf32_Addr	e_entry;		/* entry point */
	Elf32_Off	e_phoff;		/* Program hdr offset */
	Elf32_Off	e_shoff;		/* Section hdr offset */
	Elf32_Word	e_flags;		/* Processor flags */
	Elf32_Half      e_ehsize;		/* sizeof ehdr */
	Elf32_Half      e_phentsize;		/* Program header entry size */
	Elf32_Half      e_phnum;		/* Number of program headers */
	Elf32_Half      e_shentsize;		/* Section header entry size */
	Elf32_Half      e_shnum;		/* Number of section headers */
	Elf32_Half      e_shstrndx;		/* String table index */
} Elf32_Ehdr;

typedef struct {
	unsigned char	e_ident[ELF_NIDENT];	/* Id bytes */
	Elf64_Half	e_type;			/* file type */
	Elf64_Half	e_machine;		/* machine type */
	Elf64_Word	e_version;		/* version number */
	Elf64_Addr	e_entry;		/* entry point */
	Elf64_Off	e_phoff;		/* Program hdr offset */
	Elf64_Off	e_shoff;		/* Section hdr offset */
	Elf64_Word	e_flags;		/* Processor flags */
	Elf64_Half	e_ehsize;		/* sizeof ehdr */
	Elf64_Half	e_phentsize;		/* Program header entry size */
	Elf64_Half	e_phnum;		/* Number of program headers */
	Elf64_Half	e_shentsize;		/* Section header entry size */
	Elf64_Half	e_shnum;		/* Number of section headers */
	Elf64_Half	e_shstrndx;		/* String table index */
} Elf64_Ehdr;

/* e_ident offsets */
#define	EI_MAG0		0	/* '\177' */
#define	EI_MAG1		1	/* 'E'    */
#define	EI_MAG2		2	/* 'L'    */
#define	EI_MAG3		3	/* 'F'    */
#define	EI_CLASS	4	/* File class */
#define	EI_DATA		5	/* Data encoding */
#define	EI_VERSION	6	/* File version */
#define	EI_OSABI	7	/* Operating system/ABI identification */
#define	EI_ABIVERSION	8	/* ABI version */
#define	EI_PAD		9	/* Start of padding bytes up to EI_NIDENT*/
#define	EI_NIDENT	16	/* First non-ident header byte */

/* e_ident[EI_MAG0,EI_MAG3] */
#define	ELFMAG0		0x7f
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

/* e_ident[EI_CLASS] */
#define	ELFCLASSNONE	0	/* Invalid class */
#define	ELFCLASS32	1	/* 32-bit objects */
#define	ELFCLASS64	2	/* 64-bit objects */
#define	ELFCLASSNUM	3

/* e_ident[EI_DATA] */
#define	ELFDATANONE	0	/* Invalid data encoding */
#define	ELFDATA2LSB	1	/* 2's complement values, LSB first */
#define	ELFDATA2MSB	2	/* 2's complement values, MSB first */

/* e_ident[EI_VERSION] */
#define	EV_NONE		0	/* Invalid version */
#define	EV_CURRENT	1	/* Current version */
#define	EV_NUM		2

/* e_ident[EI_OSABI] */
#define	ELFOSABI_SYSV		0	/* UNIX System V ABI */
#define	ELFOSABI_HPUX		1	/* HP-UX operating system */
#define ELFOSABI_NETBSD		2	/* NetBSD */
#define ELFOSABI_LINUX		3	/* GNU/Linux */
#define ELFOSABI_HURD		4	/* GNU/Hurd */
#define ELFOSABI_86OPEN		5	/* 86Open */
#define ELFOSABI_SOLARIS	6	/* Solaris */
#define ELFOSABI_MONTEREY	7	/* Monterey */
#define ELFOSABI_IRIX		8	/* IRIX */
#define ELFOSABI_FREEBSD	9	/* FreeBSD */
#define ELFOSABI_TRU64		10	/* TRU64 UNIX */
#define ELFOSABI_MODESTO	11	/* Novell Modesto */
#define ELFOSABI_OPENBSD	12	/* OpenBSD */
#define	ELFOSABI_OPENVMS	13	/* OpenVMS */
#define	ELFOSABI_NSK		14	/* HP Non-Stop Kernel */
#define	ELFOSABI_AROS		15	/* Amiga Research OS */
/* Unofficial OSABIs follow */
#define ELFOSABI_ARM		97	/* ARM */
#define	ELFOSABI_STANDALONE	255	/* Standalone (embedded) application */

#define	ELFOSABI_NONE		ELFOSABI_SYSV
#define	ELFOSABI_AIX		ELFOSABI_MONTEREY

/* e_type */
#define	ET_NONE		0	/* No file type */
#define	ET_REL		1	/* Relocatable file */
#define	ET_EXEC		2	/* Executable file */
#define	ET_DYN		3	/* Shared object file */
#define	ET_CORE		4	/* Core file */
#define	ET_NUM		5

#define	ET_LOOS		0xfe00	/* Operating system specific range */
#define	ET_HIOS		0xfeff
#define	ET_LOPROC	0xff00	/* Processor-specific range */
#define	ET_HIPROC	0xffff

/* e_machine */
#define	EM_NONE		0	/* No machine */
#define	EM_M32		1	/* AT&T WE 32100 */
#define	EM_SPARC	2	/* SPARC */
#define	EM_386		3	/* Intel 80386 */
#define	EM_68K		4	/* Motorola 68000 */
#define	EM_88K		5	/* Motorola 88000 */
#define	EM_486		6	/* Intel 80486 */
#define	EM_860		7	/* Intel 80860 */
#define	EM_MIPS		8	/* MIPS I Architecture */
#define	EM_S370		9	/* Amdahl UTS on System/370 */
#define	EM_MIPS_RS3_LE	10	/* MIPS RS3000 Little-endian */
			/* 11-14 - Reserved */
#define	EM_RS6000	11	/* IBM RS/6000 XXX reserved */
#define	EM_PARISC	15	/* Hewlett-Packard PA-RISC */
#define	EM_NCUBE	16	/* NCube XXX reserved */
#define	EM_VPP500	17	/* Fujitsu VPP500 */
#define	EM_SPARC32PLUS	18	/* Enhanced instruction set SPARC */
#define	EM_960		19	/* Intel 80960 */
#define	EM_PPC		20	/* PowerPC */
#define	EM_PPC64	21	/* 64-bit PowerPC */
			/* 22-35 - Reserved */
#define	EM_S390		22	/* System/390 XXX reserved */
#define	EM_V800		36	/* NEC V800 */
#define	EM_FR20		37	/* Fujitsu FR20 */
#define	EM_RH32		38	/* TRW RH-32 */
#define	EM_RCE		39	/* Motorola RCE */
#define	EM_ARM		40	/* Advanced RISC Machines ARM */
#define	EM_ALPHA	41	/* DIGITAL Alpha */
#define	EM_SH		42	/* Hitachi Super-H */
#define	EM_SPARCV9	43	/* SPARC Version 9 */
#define	EM_TRICORE	44	/* Siemens Tricore */
#define	EM_ARC		45	/* Argonaut RISC Core */
#define	EM_H8_300	46	/* Hitachi H8/300 */
#define	EM_H8_300H	47	/* Hitachi H8/300H */
#define	EM_H8S		48	/* Hitachi H8S */
#define	EM_H8_500	49	/* Hitachi H8/500 */
#define	EM_IA_64	50	/* Intel Merced Processor */
#define	EM_MIPS_X	51	/* Stanford MIPS-X */
#define	EM_COLDFIRE	52	/* Motorola Coldfire */
#define	EM_68HC12	53	/* Motorola MC68HC12 */
#define	EM_MMA		54	/* Fujitsu MMA Multimedia Accelerator */
#define	EM_PCP		55	/* Siemens PCP */
#define	EM_NCPU		56	/* Sony nCPU embedded RISC processor */
#define	EM_NDR1		57	/* Denso NDR1 microprocessor */
#define	EM_STARCORE	58	/* Motorola Star*Core processor */
#define	EM_ME16		59	/* Toyota ME16 processor */
#define	EM_ST100	60	/* STMicroelectronics ST100 processor */
#define	EM_TINYJ	61	/* Advanced Logic Corp. TinyJ embedded family processor */
#define	EM_X86_64	62	/* AMD x86-64 architecture */
#define	EM_PDSP		63	/* Sony DSP Processor */
#define	EM_PDP10	64	/* Digital Equipment Corp. PDP-10 */
#define	EM_PDP11	65	/* Digital Equipment Corp. PDP-11 */
#define	EM_FX66		66	/* Siemens FX66 microcontroller */
#define	EM_ST9PLUS	67	/* STMicroelectronics ST9+ 8/16 bit microcontroller */
#define	EM_ST7		68	/* STMicroelectronics ST7 8-bit microcontroller */
#define	EM_68HC16	69	/* Motorola MC68HC16 Microcontroller */
#define	EM_68HC11	70	/* Motorola MC68HC11 Microcontroller */
#define	EM_68HC08	71	/* Motorola MC68HC08 Microcontroller */
#define	EM_68HC05	72	/* Motorola MC68HC05 Microcontroller */
#define	EM_SVX		73	/* Silicon Graphics SVx */
#define	EM_ST19		74	/* STMicroelectronics ST19 8-bit CPU */
#define	EM_VAX		75	/* Digital VAX */
#define	EM_CRIS		76	/* Axis Communications 32-bit embedded processor */
#define	EM_JAVELIN	77	/* Infineon Technologies 32-bit embedded CPU */
#define	EM_FIREPATH	78	/* Element 14 64-bit DSP processor */
#define	EM_ZSP		79	/* LSI Logic's 16-bit DSP processor */
#define	EM_MMIX		80	/* Donald Knuth's educational 64-bit processor */
#define	EM_HUANY	81	/* Harvard's machine-independent format */
#define	EM_PRISM	82	/* SiTera Prism */
#define	EM_AVR		83	/* Atmel AVR 8-bit microcontroller */
#define	EM_FR30		84	/* Fujitsu FR30 */
#define	EM_D10V		85	/* Mitsubishi D10V */
#define	EM_D30V		86	/* Mitsubishi D30V */
#define	EM_V850		87	/* NEC v850 */
#define	EM_M32R		88	/* Mitsubishi M32R */
#define	EM_MN10300	89	/* Matsushita MN10300 */
#define	EM_MN10200	90	/* Matsushita MN10200 */
#define	EM_PJ		91	/* picoJava */
#define	EM_OPENRISC	92	/* OpenRISC 32-bit embedded processor */
#define	EM_ARC_A5	93	/* ARC Cores Tangent-A5 */
#define	EM_XTENSA	94	/* Tensilica Xtensa Architecture */
#define	EM_VIDEOCORE	95	/* Alphamosaic VideoCore processor */
#define	EM_TMM_GPP	96	/* Thompson Multimedia General Purpose Processor */
#define	EM_NS32K	97	/* National Semiconductor 32000 series */
#define	EM_TPC		98	/* Tenor Network TPC processor */
#define	EM_SNP1K	99	/* Trebia SNP 1000 processor */
#define	EM_ST200	100	/* STMicroelectronics ST200 microcontroller */
#define	EM_IP2K		101	/* Ubicom IP2xxx microcontroller family */
#define	EM_MAX		102	/* MAX processor */
#define	EM_CR		103	/* National Semiconductor CompactRISC micorprocessor */
#define	EM_F2MC16	104	/* Fujitsu F2MC16 */
#define	EM_MSP430	105	/* Texas Instruments MSP430 */
#define	EM_BLACKFIN	106	/* Analog Devices Blackfin DSP */
#define	EM_SE_C33	107	/* Seiko Epson S1C33 family */
#define	EM_SEP		108	/* Sharp embedded microprocessor */
#define	EM_ARCA		109	/* Arca RISC microprocessor */
#define	EM_UNICORE	110	/* UNICORE from PKU-Unity Ltd. and MPRC Peking University */

/* Unofficial machine types follow */
#define	EM_AVR32	6317	/* used by NetBSD/avr32 */
#define	EM_ALPHA_EXP	36902	/* used by NetBSD/alpha; obsolete */
#define	EM_NUM		36903

/*
 * Program Header
 */
typedef struct {
	Elf32_Word	p_type;		/* entry type */
	Elf32_Off	p_offset;	/* offset */
	Elf32_Addr	p_vaddr;	/* virtual address */
	Elf32_Addr	p_paddr;	/* physical address */
	Elf32_Word	p_filesz;	/* file size */
	Elf32_Word	p_memsz;	/* memory size */
	Elf32_Word	p_flags;	/* flags */
	Elf32_Word	p_align;	/* memory & file alignment */
} Elf32_Phdr;

typedef struct {
	Elf64_Word	p_type;		/* entry type */
	Elf64_Word	p_flags;	/* flags */
	Elf64_Off	p_offset;	/* offset */
	Elf64_Addr	p_vaddr;	/* virtual address */
	Elf64_Addr	p_paddr;	/* physical address */
	Elf64_Xword	p_filesz;	/* file size */
	Elf64_Xword	p_memsz;	/* memory size */
	Elf64_Xword	p_align;	/* memory & file alignment */
} Elf64_Phdr;

/* p_type */
#define	PT_NULL		0		/* Program header table entry unused */
#define	PT_LOAD		1		/* Loadable program segment */
#define	PT_DYNAMIC	2		/* Dynamic linking information */
#define	PT_INTERP	3		/* Program interpreter */
#define	PT_NOTE		4		/* Auxiliary information */
#define	PT_SHLIB	5		/* Reserved, unspecified semantics */
#define	PT_PHDR		6		/* Entry for header table itself */
#define	PT_TLS		7		/* TLS initialisation image */
#define	PT_NUM		8

#define	PT_LOOS         0x60000000	/* OS-specific range */
#define	PT_GNU_EH_FRAME	0x6474e550	/* GNU-specific EH frame segment */
#define	PT_HIOS         0x6fffffff
#define	PT_LOPROC	0x70000000	/* Processor-specific range */
#define	PT_HIPROC	0x7fffffff

#define	PT_MIPS_REGINFO	0x70000000

/* p_flags */
#define	PF_R		0x4	/* Segment is readable */
#define	PF_W		0x2	/* Segment is writable */
#define	PF_X		0x1	/* Segment is executable */

#define	PF_MASKOS	0x0ff00000	/* Operating system specific values */
#define	PF_MASKPROC	0xf0000000	/* Processor-specific values */

/* Extended program header index. */
#define	PN_XNUM		0xffff

/*
 * Section Headers
 */
typedef struct {
	Elf32_Word	sh_name;	/* section name (.shstrtab index) */
	Elf32_Word	sh_type;	/* section type */
	Elf32_Word	sh_flags;	/* section flags */
	Elf32_Addr	sh_addr;	/* virtual address */
	Elf32_Off	sh_offset;	/* file offset */
	Elf32_Word	sh_size;	/* section size */
	Elf32_Word	sh_link;	/* link to another */
	Elf32_Word	sh_info;	/* misc info */
	Elf32_Word	sh_addralign;	/* memory alignment */
	Elf32_Word	sh_entsize;	/* table entry size */
} Elf32_Shdr;

typedef struct {
	Elf64_Word	sh_name;	/* section name (.shstrtab index) */
	Elf64_Word	sh_type;	/* section type */
	Elf64_Xword	sh_flags;	/* section flags */
	Elf64_Addr	sh_addr;	/* virtual address */
	Elf64_Off	sh_offset;	/* file offset */
	Elf64_Xword	sh_size;	/* section size */
	Elf64_Word	sh_link;	/* link to another */
	Elf64_Word	sh_info;	/* misc info */
	Elf64_Xword	sh_addralign;	/* memory alignment */
	Elf64_Xword	sh_entsize;	/* table entry size */
} Elf64_Shdr;

/* sh_type */
#define	SHT_NULL	0		/* Section header table entry unused */
#define	SHT_PROGBITS	1		/* Program information */
#define	SHT_SYMTAB	2		/* Symbol table */
#define	SHT_STRTAB	3		/* String table */
#define	SHT_RELA	4		/* Relocation information w/ addend */
#define	SHT_HASH	5		/* Symbol hash table */
#define	SHT_DYNAMIC	6		/* Dynamic linking information */
#define	SHT_NOTE	7		/* Auxiliary information */
#define	SHT_NOBITS	8		/* No space allocated in file image */
#define	SHT_REL		9		/* Relocation information w/o addend */
#define	SHT_SHLIB	10		/* Reserved, unspecified semantics */
#define	SHT_DYNSYM	11		/* Symbol table for dynamic linker */
#define	SHT_INIT_ARRAY	14		/* Initialization function pointers */
#define	SHT_FINI_ARRAY	15		/* Termination function pointers */
#define	SHT_PREINIT_ARRAY 16		/* Pre-initialization function ptrs */
#define	SHT_GROUP	17		/* Section group */
#define	SHT_SYMTAB_SHNDX 18		/* Section indexes (see SHN_XINDEX) */
#define	SHT_NUM		19

#define	SHT_LOOS	0x60000000	/* Operating system specific range */
#define	SHT_SUNW_move	0x6ffffffa
#define	SHT_SUNW_syminfo 0x6ffffffc
#define SHT_SUNW_verdef	0x6ffffffd	/* Versions defined by file */
#define	SHT_GNU_verdef	SHT_SUNW_verdef
#define SHT_SUNW_verneed 0x6ffffffe	/* Versions needed by file */
#define	SHT_GNU_verneed	SHT_SUNW_verneed
#define SHT_SUNW_versym	0x6fffffff	/* Symbol versions */
#define	SHT_GNU_versym	SHT_SUNW_versym
#define	SHT_HIOS	0x6fffffff
#define	SHT_LOPROC	0x70000000	/* Processor-specific range */
#define	SHT_AMD64_UNWIND 0x70000001	/* unwind information */
#define	SHT_HIPROC	0x7fffffff
#define	SHT_LOUSER	0x80000000	/* Application-specific range */
#define	SHT_HIUSER	0xffffffff

/* sh_flags */
#define	SHF_WRITE	0x1		/* Section contains writable data */
#define	SHF_ALLOC	0x2		/* Section occupies memory */
#define	SHF_EXECINSTR	0x4		/* Section contains executable insns */

#define	SHF_MASKOS	0x0f000000	/* Operating system specific values */
#define	SHF_MASKPROC	0xf0000000	/* Processor-specific values */

/*
 * Symbol Table
 */
typedef struct {
	Elf32_Word	st_name;	/* Symbol name (.strtab index) */
	Elf32_Word	st_value;	/* value of symbol */
	Elf32_Word	st_size;	/* size of symbol */
	Elf_Byte	st_info;	/* type / binding attrs */
	Elf_Byte	st_other;	/* unused */
	Elf32_Half	st_shndx;	/* section index of symbol */
} Elf32_Sym;

typedef struct {
	Elf64_Word	st_name;	/* Symbol name (.strtab index) */
	Elf_Byte	st_info;	/* type / binding attrs */
	Elf_Byte	st_other;	/* unused */
	Elf64_Half	st_shndx;	/* section index of symbol */
	Elf64_Addr	st_value;	/* value of symbol */
	Elf64_Xword	st_size;	/* size of symbol */
} Elf64_Sym;

/* Symbol Table index of the undefined symbol */
#define	ELF_SYM_UNDEFINED	0

#define STN_UNDEF		0	/* undefined index */

/* st_info: Symbol Bindings */
#define	STB_LOCAL		0	/* local symbol */
#define	STB_GLOBAL		1	/* global symbol */
#define	STB_WEAK		2	/* weakly defined global symbol */
#define	STB_NUM			3

#define	STB_LOOS		10	/* Operating system specific range */
#define	STB_HIOS		12
#define	STB_LOPROC		13	/* Processor-specific range */
#define	STB_HIPROC		15

/* st_info: Symbol Types */
#define	STT_NOTYPE		0	/* Type not specified */
#define	STT_OBJECT		1	/* Associated with a data object */
#define	STT_FUNC		2	/* Associated with a function */
#define	STT_SECTION		3	/* Associated with a section */
#define	STT_FILE		4	/* Associated with a file name */
#define	STT_COMMON		5	/* Uninitialised common block */
#define	STT_TLS			6	/* Thread local data object */
#define	STT_NUM			7

#define	STT_LOOS		10	/* Operating system specific range */
#define	STT_HIOS		12
#define	STT_LOPROC		13	/* Processor-specific range */
#define	STT_HIPROC		15

/* st_other: Visibility Types */
#define	STV_DEFAULT		0	/* use binding type */
#define	STV_INTERNAL		1	/* not referenced from outside */
#define	STV_HIDDEN		2	/* not visible, may be used via ptr */
#define	STV_PROTECTED		3	/* visible, not preemptible */
#define	STV_EXPORTED		4
#define	STV_SINGLETON		5
#define	STV_ELIMINATE		6

/* st_info/st_other utility macros */
#define	ELF_ST_BIND(info)		((uint32_t)(info) >> 4)
#define	ELF_ST_TYPE(info)		((uint32_t)(info) & 0xf)
#define	ELF_ST_INFO(bind,type)		((Elf_Byte)(((bind) << 4) | \
					 ((type) & 0xf)))
#define	ELF_ST_VISIBILITY(other)	((uint32_t)(other) & 3)

/*
 * Special section indexes
 */
#define	SHN_UNDEF	0		/* Undefined section */

#define	SHN_LORESERVE	0xff00		/* Reserved range */
#define	SHN_ABS		0xfff1		/*  Absolute symbols */
#define	SHN_COMMON	0xfff2		/*  Common symbols */
#define	SHN_XINDEX	0xffff		/* Escape -- index stored elsewhere */
#define	SHN_HIRESERVE	0xffff

#define	SHN_LOPROC	0xff00		/* Processor-specific range */
#define	SHN_HIPROC	0xff1f
#define	SHN_LOOS	0xff20		/* Operating system specific range */
#define	SHN_HIOS	0xff3f

#define	SHN_MIPS_ACOMMON 0xff00
#define	SHN_MIPS_TEXT	0xff01
#define	SHN_MIPS_DATA	0xff02
#define	SHN_MIPS_SCOMMON 0xff03

/*
 * Relocation Entries
 */
typedef struct {
	Elf32_Word	r_offset;	/* where to do it */
	Elf32_Word	r_info;		/* index & type of relocation */
} Elf32_Rel;

typedef struct {
	Elf32_Word	r_offset;	/* where to do it */
	Elf32_Word	r_info;		/* index & type of relocation */
	Elf32_Sword	r_addend;	/* adjustment value */
} Elf32_Rela;

/* r_info utility macros */
#define	ELF32_R_SYM(info)	((info) >> 8)
#define	ELF32_R_TYPE(info)	((info) & 0xff)
#define	ELF32_R_INFO(sym, type)	(((sym) << 8) + (unsigned char)(type))

typedef struct {
	Elf64_Addr	r_offset;	/* where to do it */
	Elf64_Xword	r_info;		/* index & type of relocation */
} Elf64_Rel;

typedef struct {
	Elf64_Addr	r_offset;	/* where to do it */
	Elf64_Xword	r_info;		/* index & type of relocation */
	Elf64_Sxword	r_addend;	/* adjustment value */
} Elf64_Rela;

/* r_info utility macros */
#define	ELF64_R_SYM(info)	((info) >> 32)
#define	ELF64_R_TYPE(info)	((info) & 0xffffffff)
#define	ELF64_R_INFO(sym,type)	(((sym) << 32) + (type))

/*
 * Move entries
 */
typedef struct {
	Elf32_Lword	m_value;	/* symbol value */
	Elf32_Word	m_info;		/* size + index */
	Elf32_Word	m_poffset;	/* symbol offset */
	Elf32_Half	m_repeat;	/* repeat count */
	Elf32_Half	m_stride;	/* stride info */
} Elf32_Move;

#define	ELF32_M_SYM(info)	((info) >> 8)
#define	ELF32_M_SIZE(info)	(info) & 0xff)
#define	ELF32_M_INFO(sym, size)	(((sym) << 8) + (unsigned char)(size))

typedef struct {
	Elf64_Lword	m_value;	/* symbol value */
	Elf64_Xword	m_info;		/* size + index */
	Elf64_Xword	m_poffset;	/* symbol offset */
	Elf64_Word	m_repeat;	/* repeat count */
	Elf64_Word	m_stride;	/* stride info */
} Elf64_Move;

#define	ELF64_M_SYM(info)	((info) >> 8)
#define	ELF64_M_SIZE(info)	(info) & 0xff)
#define	ELF64_M_INFO(sym, size)	(((sym) << 8) + (unsigned char)(size))

/*
 * Hardware/software capabilities entry
 */
typedef struct {
	Elf32_Word	c_tag;		/* entry tag value */
	union {
	    Elf32_Addr	c_ptr;
	    Elf32_Word	c_val;
	} c_un;
} Elf32_Cap;

typedef struct {
	Elf64_Xword	c_tag;		/* entry tag value */
	union {
	    Elf64_Addr	c_ptr;
	    Elf64_Xword	c_val;
	} c_un;
} Elf64_Cap;

/*
 * Dynamic Section structure array
 */
typedef struct {
	Elf32_Word	d_tag;		/* entry tag value */
	union {
	    Elf32_Addr	d_ptr;
	    Elf32_Word	d_val;
	} d_un;
} Elf32_Dyn;

typedef struct {
	Elf64_Xword	d_tag;		/* entry tag value */
	union {
	    Elf64_Addr	d_ptr;
	    Elf64_Xword	d_val;
	} d_un;
} Elf64_Dyn;

/* d_tag */
#define	DT_NULL		0	/* Marks end of dynamic array */
#define	DT_NEEDED	1	/* Name of needed library (DT_STRTAB offset) */
#define	DT_PLTRELSZ	2	/* Size, in bytes, of relocations in PLT */
#define	DT_PLTGOT	3	/* Address of PLT and/or GOT */
#define	DT_HASH		4	/* Address of symbol hash table */
#define	DT_STRTAB	5	/* Address of string table */
#define	DT_SYMTAB	6	/* Address of symbol table */
#define	DT_RELA		7	/* Address of Rela relocation table */
#define	DT_RELASZ	8	/* Size, in bytes, of DT_RELA table */
#define	DT_RELAENT	9	/* Size, in bytes, of one DT_RELA entry */
#define	DT_STRSZ	10	/* Size, in bytes, of DT_STRTAB table */
#define	DT_SYMENT	11	/* Size, in bytes, of one DT_SYMTAB entry */
#define	DT_INIT		12	/* Address of initialization function */
#define	DT_FINI		13	/* Address of termination function */
#define	DT_SONAME	14	/* Shared object name (DT_STRTAB offset) */
#define	DT_RPATH	15	/* Library search path (DT_STRTAB offset) */
#define	DT_SYMBOLIC	16	/* Start symbol search within local object */
#define	DT_REL		17	/* Address of Rel relocation table */
#define	DT_RELSZ	18	/* Size, in bytes, of DT_REL table */
#define	DT_RELENT	19	/* Size, in bytes, of one DT_REL entry */
#define	DT_PLTREL	20 	/* Type of PLT relocation entries */
#define	DT_DEBUG	21	/* Used for debugging; unspecified */
#define	DT_TEXTREL	22	/* Relocations might modify non-writable seg */
#define	DT_JMPREL	23	/* Address of relocations associated with PLT */
#define	DT_BIND_NOW	24	/* Process all relocations at load-time */
#define	DT_INIT_ARRAY	25	/* Address of initialization function array */
#define	DT_FINI_ARRAY	26	/* Size, in bytes, of DT_INIT_ARRAY array */
#define	DT_INIT_ARRAYSZ	27	/* Address of termination function array */
#define	DT_FINI_ARRAYSZ	28	/* Size, in bytes, of DT_FINI_ARRAY array*/
#define	DT_NUM		29

#define	DT_LOOS		0x60000000	/* Operating system specific range */
#define DT_VERSYM	0x6ffffff0	/* Symbol versions */
#define	DT_FLAGS_1	0x6ffffffb	/* ELF dynamic flags */
#define DT_VERDEF	0x6ffffffc	/* Versions defined by file */
#define DT_VERDEFNUM	0x6ffffffd	/* Number of versions defined by file */
#define DT_VERNEED	0x6ffffffe	/* Versions needed by file */
#define DT_VERNEEDNUM	0x6fffffff	/* Number of versions needed by file */
#define	DT_HIOS		0x6fffffff
#define	DT_LOPROC	0x70000000	/* Processor-specific range */
#define	DT_HIPROC	0x7fffffff

/* Flag values for DT_FLAGS_1 (incomplete) */
#define	DF_1_BIND_NOW	0x00000001	/* Same as DF_BIND_NOW */
#define	DF_1_NODELETE	0x00000008	/* Set the RTLD_NODELETE for object */
#define	DF_1_INITFIRST	0x00000020	/* Object's init/fini take priority */
#define	DF_1_NOOPEN	0x00000040	/* Do not allow loading on dlopen() */

/*
 * Auxiliary Vectors
 */
typedef struct {
	Elf32_Word	a_type;				/* 32-bit id */
	Elf32_Word	a_v;				/* 32-bit id */
} Aux32Info;

typedef struct {
	Elf64_Word	a_type;				/* 32-bit id */
	Elf64_Xword	a_v;				/* 64-bit id */
} Aux64Info;

/* a_type */
#define	AT_NULL		0	/* Marks end of array */
#define	AT_IGNORE	1	/* No meaning, a_un is undefined */
#define	AT_EXECFD	2	/* Open file descriptor of object file */
#define	AT_PHDR		3	/* &phdr[0] */
#define	AT_PHENT	4	/* sizeof(phdr[0]) */
#define	AT_PHNUM	5	/* # phdr entries */
#define	AT_PAGESZ	6	/* PAGESIZE */
#define	AT_BASE		7	/* Interpreter base addr */
#define	AT_FLAGS	8	/* Processor flags */
#define	AT_ENTRY	9	/* Entry address of executable */
#define	AT_DCACHEBSIZE	10	/* Data cache block size */
#define	AT_ICACHEBSIZE	11	/* Instruction cache block size */
#define	AT_UCACHEBSIZE	12	/* Unified cache block size */

	/* Vendor specific */
#define	AT_MIPS_NOTELF	10	/* XXX a_val != 0 -> MIPS XCOFF executable */

#define	AT_EUID		2000	/* euid (solaris compatible numbers) */
#define	AT_RUID		2001	/* ruid (solaris compatible numbers) */
#define	AT_EGID		2002	/* egid (solaris compatible numbers) */
#define	AT_RGID		2003	/* rgid (solaris compatible numbers) */

	/* Solaris kernel specific */
#define	AT_SUN_LDELF	2004	/* dynamic linker's ELF header */
#define	AT_SUN_LDSHDR	2005	/* dynamic linker's section header */
#define	AT_SUN_LDNAME	2006	/* dynamic linker's name */
#define	AT_SUN_LPGSIZE	2007	/* large pagesize */

	/* Other information */
#define	AT_SUN_PLATFORM	2008	/* sysinfo(SI_PLATFORM) */
#define	AT_SUN_HWCAP	2009	/* process hardware capabilities */
#define	AT_SUN_IFLUSH	2010	/* do we need to flush the instruction cache? */
#define	AT_SUN_CPU	2011	/* CPU name */
	/* ibcs2 emulation band aid */
#define	AT_SUN_EMUL_ENTRY 2012	/* coff entry point */
#define	AT_SUN_EMUL_EXECFD 2013	/* coff file descriptor */
	/* Executable's fully resolved name */
#define	AT_SUN_EXECNAME	2014

/*
 * Note Headers
 */
typedef struct {
	Elf32_Word n_namesz;
	Elf32_Word n_descsz;
	Elf32_Word n_type;
} Elf32_Nhdr;

typedef struct {
	Elf64_Word n_namesz;
	Elf64_Word n_descsz;
	Elf64_Word n_type;
} Elf64_Nhdr;

#define	ELF_NOTE_TYPE_ABI_TAG		1

/* GNU-specific note name and description sizes */
#define	ELF_NOTE_ABI_NAMESZ		4
#define	ELF_NOTE_ABI_DESCSZ		16
/* GNU-specific note name */
#define	ELF_NOTE_ABI_NAME		"GNU\0"

/* GNU-specific OS/version value stuff */
#define	ELF_NOTE_ABI_OS_LINUX		0
#define	ELF_NOTE_ABI_OS_HURD		1
#define	ELF_NOTE_ABI_OS_SOLARIS		2

/* NetBSD-specific note type: Emulation name.  desc is emul name string. */
#define	ELF_NOTE_TYPE_NETBSD_TAG	1
/* NetBSD-specific note name and description sizes */
#define	ELF_NOTE_NETBSD_NAMESZ		7
#define	ELF_NOTE_NETBSD_DESCSZ		4
/* NetBSD-specific note name */
#define	ELF_NOTE_NETBSD_NAME		"NetBSD\0\0"

/* NetBSD-specific note type: Checksum.  There should be 1 NOTE per PT_LOAD
   section.  desc is a tuple of <phnum>(16),<chk-type>(16),<chk-value>. */
#define	ELF_NOTE_TYPE_CHECKSUM_TAG	2
#define	ELF_NOTE_CHECKSUM_CRC32		1
#define	ELF_NOTE_CHECKSUM_MD5		2
#define	ELF_NOTE_CHECKSUM_SHA1		3
#define	ELF_NOTE_CHECKSUM_SHA256	4

/* NetBSD-specific note type: PaX.  There should be 1 NOTE per executable.
   section.  desc is a 32 bit bitmask */
#define ELF_NOTE_TYPE_PAX_TAG		3
#define	ELF_NOTE_PAX_MPROTECT		0x01	/* Force enable Mprotect */
#define	ELF_NOTE_PAX_NOMPROTECT		0x02	/* Force disable Mprotect */
#define	ELF_NOTE_PAX_GUARD		0x04	/* Force enable Segvguard */
#define	ELF_NOTE_PAX_NOGUARD		0x08	/* Force disable Servguard */
#define	ELF_NOTE_PAX_ASLR		0x10	/* Force enable ASLR */
#define	ELF_NOTE_PAX_NOASLR		0x20	/* Force disable ASLR */
#define ELF_NOTE_PAX_NAMESZ		4
#define ELF_NOTE_PAX_NAME		"PaX\0"
#define ELF_NOTE_PAX_DESCSZ		4

#if defined(ELFSIZE)
#define	CONCAT(x,y)	__CONCAT(x,y)
#define	ELFNAME(x)	CONCAT(elf,CONCAT(ELFSIZE,CONCAT(_,x)))
#define	ELFNAME2(x,y)	CONCAT(x,CONCAT(_elf,CONCAT(ELFSIZE,CONCAT(_,y))))
#define	ELFNAMEEND(x)	CONCAT(x,CONCAT(_elf,ELFSIZE))
#define	ELFDEFNNAME(x)	CONCAT(ELF,CONCAT(ELFSIZE,CONCAT(_,x)))
#endif

#if defined(ELFSIZE) && (ELFSIZE == 32)
#define	Elf_Ehdr	Elf32_Ehdr
#define	Elf_Phdr	Elf32_Phdr
#define	Elf_Shdr	Elf32_Shdr
#define	Elf_Sym		Elf32_Sym
#define	Elf_Rel		Elf32_Rel
#define	Elf_Rela	Elf32_Rela
#define	Elf_Dyn		Elf32_Dyn
#define	Elf_Word	Elf32_Word
#define	Elf_Sword	Elf32_Sword
#define	Elf_Half	Elf32_Half
#define	Elf_Addr	Elf32_Addr
#define	Elf_Off		Elf32_Off
#define	Elf_SOff	Elf32_SOff
#define	Elf_Nhdr	Elf32_Nhdr

#define	ELF_R_SYM	ELF32_R_SYM
#define	ELF_R_TYPE	ELF32_R_TYPE
#define	ELFCLASS	ELFCLASS32

#define	AuxInfo		Aux32Info
#elif defined(ELFSIZE) && (ELFSIZE == 64)
#define	Elf_Ehdr	Elf64_Ehdr
#define	Elf_Phdr	Elf64_Phdr
#define	Elf_Shdr	Elf64_Shdr
#define	Elf_Sym		Elf64_Sym
#define	Elf_Rel		Elf64_Rel
#define	Elf_Rela	Elf64_Rela
#define	Elf_Dyn		Elf64_Dyn
#define	Elf_Word	Elf64_Word
#define	Elf_Sword	Elf64_Sword
#define	Elf_Half	Elf64_Half
#define	Elf_Addr	Elf64_Addr
#define	Elf_Off		Elf64_Off
#define	Elf_SOff	Elf64_SOff
#define	Elf_Nhdr	Elf64_Nhdr

#define	ELF_R_SYM	ELF64_R_SYM
#define	ELF_R_TYPE	ELF64_R_TYPE
#define	ELFCLASS	ELFCLASS64

#define	AuxInfo		Aux64Info
#endif

#ifndef	Elf_Symindx
#define	Elf_Symindx	uint32_t
#endif

#define	ELF32_ST_BIND(info)		ELF_ST_BIND(info)
#define	ELF32_ST_TYPE(info)		ELF_ST_TYPE(info)
#define	ELF32_ST_INFO(bind,type)	ELF_ST_INFO(bind,type)
#define	ELF32_ST_VISIBILITY(other)	ELF_ST_VISIBILITY(other)

#define	ELF64_ST_BIND(info)		ELF_ST_BIND(info)
#define	ELF64_ST_TYPE(info)		ELF_ST_TYPE(info)
#define	ELF64_ST_INFO(bind,type)	ELF_ST_INFO(bind,type)
#define	ELF64_ST_VISIBILITY(other)	ELF_ST_VISIBILITY(other)

typedef struct {
	Elf32_Half	si_boundto;	/* direct bindings - symbol bound to */
	Elf32_Half	si_flags;	/* per symbol flags */
} Elf32_Syminfo;

typedef struct {
	Elf64_Word	si_boundto;	/* direct bindings - symbol bound to */
	Elf64_Word	si_flags;	/* per symbol flags */
} Elf64_Syminfo;

#define	SYMINFO_FLG_DIRECT	0x0001	/* symbol ref has direct association
					   to object containing definition */
#define	SYMINFO_FLG_PASSTHRU	0x0002	/* ignored - see SYMINFO_FLG_FILTER */
#define	SYMINFO_FLG_COPY	0x0004	/* symbol is a copy-reloc */
#define	SYMINFO_FLG_LAZYLOAD	0x0008	/* object containing defn should be
					   lazily-loaded */
#define	SYMINFO_FLG_DIRECTBIND	0x0010	/* ref should be bound directly to
					   object containing definition */
#define	SYMINFO_FLG_NOEXTDIRECT	0x0020	/* don't let an external reference
					   directly bind to this symbol */
#define	SYMINFO_FLG_FILTER	0x0002	/* symbol ref is associated to a */
#define	SYMINFO_FLG_AUXILIARY	0x0040	/*      standard or auxiliary filter */

#define	SYMINFO_BT_SELF		0xffff	/* symbol bound to self */
#define	SYMINFO_BT_PARENT	0xfffe	/* symbol bound to parent */
#define	SYMINFO_BT_NONE		0xfffd	/* no special symbol binding */
#define	SYMINFO_BT_EXTERN	0xfffc	/* symbol defined as external */
#define	SYMINFO_BT_LOWRESERVE	0xff00	/* beginning of reserved entries */

#define	SYMINFO_NONE		0	/* Syminfo version */
#define	SYMINFO_CURRENT		1
#define	SYMINFO_NUM		2

/*
 * These constants are used for Elf32_Verdef struct's version number.  
 */
#define VER_DEF_NONE		0
#define	VER_DEF_CURRENT		1

/*
 * These constants are used for Elf32_Verdef struct's vd_flags.  
 */
#define VER_FLG_BASE		0x1
#define	VER_FLG_WEAK		0x2

/*
 * These are used in an Elf32_Versym field.
 */
#define	VER_NDX_LOCAL		0
#define	VER_NDX_GLOBAL		1

/*
 * These constants are used for Elf32_Verneed struct's version number.  
 */
#define	VER_NEED_NONE		0
#define	VER_NEED_CURRENT	1

/*
 * GNU Extension hidding symb
 */
#define	VERSYM_HIDDEN		0x8000
#define	VERSYM_VERSION		0x7fff

#define	ELF_VER_CHR		'@'

/*
 * These are current size independent.
 */

typedef struct {
	Elf32_Half	vd_version;	/* version number of structure */
	Elf32_Half	vd_flags;	/* flags (VER_FLG_*) */
	Elf32_Half	vd_ndx;		/* version index */
	Elf32_Half	vd_cnt;		/* number of verdaux entries */
	Elf32_Word	vd_hash;	/* hash of name */
	Elf32_Word	vd_aux;		/* offset to verdaux entries */
	Elf32_Word	vd_next;	/* offset to next verdef */
} Elf32_Verdef;
typedef	Elf32_Verdef	Elf64_Verdef;

typedef struct {
	Elf32_Word	vda_name;	/* string table offset of name */
	Elf32_Word	vda_next;	/* offset to verdaux */
} Elf32_Verdaux;
typedef	Elf32_Verdaux	Elf64_Verdaux;

typedef struct {
	Elf32_Half	vn_version;	/* version number of structure */
	Elf32_Half	vn_cnt;		/* number of vernaux entries */
	Elf32_Word	vn_file;	/* string table offset of library name*/
	Elf32_Word	vn_aux;		/* offset to vernaux entries */
	Elf32_Word	vn_next;	/* offset to next verneed */
} Elf32_Verneed;
typedef	Elf32_Verneed	Elf64_Verneed;

typedef struct {
	Elf32_Word	vna_hash;	/* Hash of dependency name */
	Elf32_Half	vna_flags;	/* flags (VER_FLG_*) */
	Elf32_Half	vna_other;	/* unused */
	Elf32_Word	vna_name;	/* string table offset to version name*/
	Elf32_Word	vna_next;	/* offset to next vernaux */
} Elf32_Vernaux;
typedef	Elf32_Vernaux	Elf64_Vernaux;

typedef struct {
	Elf32_Half	vs_vers;
} Elf32_Versym;
typedef	Elf32_Versym	Elf64_Versym;

/*
 *
 *
 * Pathscale extensions start here
 *
 *
 */
typedef uint8_t Elf32_Byte;
typedef uint8_t Elf64_Byte;

#define ELF_BSS		".bss"
#define ELF_COMMENT	".comment"
#define ELF_DATA	".data"
#define ELF_GOT		".got"
#define ELF_RODATA	".rodata"
#define	ELF_SHSTRTAB	".shstrtab"
#define ELF_STRTAB	".strtab"
#define ELF_SYMTAB	".symtab"
#define ELF_TBSS	".tbss"
#define ELF_TDATA	".tdata"
#define ELF_TEXT	".text"

#define MIPS_LIT4               ".lit4"
#define MIPS_LIT8               ".lit8"
#define MIPS_LIT16              ".lit16"
#define MIPS_SBSS               ".sbss"
#define MIPS_SDATA              ".sdata"
#define MIPS_SRDATA             ".srdata"

#define	EF_IRIX_ABI64	0x00000010	

#define SHF_TLS		(1 << 10)	/* Section hold thread-local data.  */

#define IS_ELF(ehdr)	((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
			(ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
			(ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
			(ehdr).e_ident[EI_MAG3] == ELFMAG3)

/* Relocation table entry without addend (in section of type SHT_REL).
   These are the same size as Elf64_Rel and Elf64_Rela, but laid out
   a little differently. */

typedef struct
{
  Elf64_Addr	r_offset;
  Elf64_Byte	r_type;			/* 1st relocation op type */
  Elf64_Byte	r_type2;		/* 2nd relocation op type */
  Elf64_Byte	r_type3;		/* 3rd relocation op type */
  Elf64_Byte	r_ssym;			/* Special symbol */
  Elf64_Word	r_sym;			/* Symbol index */
} Elf64_AltRel;

typedef struct {
  Elf64_Addr	r_offset;
  Elf64_Byte	r_type;			/* 1st relocation op type */
  Elf64_Byte	r_type2;		/* 2nd relocation op type */
  Elf64_Byte	r_type3;		/* 3rd relocation op type */
  Elf64_Byte	r_ssym;			/* Special symbol */
  Elf64_Word	r_sym;			/* Symbol index */
  Elf64_Sxword	r_addend;
} Elf64_AltRela;

/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

/* Itanium extensions */
#define R_IA64_DIR32LSB		0x25	/* symbol + addend, data4 LSB */
#define R_IA64_DIR64LSB		0x27	/* symbol + addend, data8 LSB */
#define R_IA64_SEGREL64LSB	0x5f	/* @segrel(sym + add), data8 LSB */

#define	R_IA_64_NONE		0x00	/* None			None		*/
#define R_IA_64_PCREL21B	0x49	/* instr: imm21 (form1)	S+A-P		*/
#define	R_IA_64_DIR32MSB	0x24	/* word32 MSB		S+A		*/
#define	R_IA_64_DIR64MSB	0x26	/* word64 MSB		S+A		*/
#define	R_IA_64_DIR32LSB	0x25	/* word32 LSB		S+A		*/
#define	R_IA_64_DIR64LSB	0x27	/* word64 LSB		S+A		*/
#define	R_IA_64_SEGREL32LSB	0x5d	/* word32 LSB		@segrel(S+A)	*/
#define	R_IA_64_SEGREL64LSB	0x5f	/* word64 LSB		@segrel(S+A)	*/
#define	R_IA_64_SECREL64MSB	0x66	/* word64 MSB		@secrel(S+A)	*/
#define	R_IA_64_SECREL64LSB	0x67	/* word64 LSB		@secrel(S+A)	*/

#define IA64_INTERFACES         ".IA64.interfaces"
#define EI_TVERSION 15
#define EV_T_CURRENT 1

#define SHF_IA_64_SHORT		0x10000000	/* section near gp */

/* MIPS extensions */
#define R_MIPS_26	4		/* Direct 26 bit shifted */
#define R_MIPS_GPREL	7

#define SHT_MIPS_XLATE	       0x70000024
#define SHT_MIPS_XLATE_DEBUG   0x70000025
#define SHT_MIPS_XLATE_OLD     0x70000028

#define SHF_MIPS_MERGE	 0x20000000
#define SHF_MIPS_LOCAL	 0x04000000
#define SHF_MIPS_NAMES	 0x02000000
#define SHF_MIPS_NODUPE	 0x01000000

#define SHN_MIPS_SUNDEFINED 0xff04	/* Small undefined symbols */

#define SHN_LOPROC	0xff00		/* Start of processor-specific */
#define SHN_IRIX_LCOMMON	(SHN_LOPROC + 5)
#define SHN_MIPS_LCOMMON	SHN_IRIX_LCOMMON

#define SHT_IRIX_IFACE		(SHT_LOPROC + 11)

/*
 * Special Irix st_other
 */
#define STO_DEFAULT		0x0
#define STO_INTERNAL		0x1
#define STO_HIDDEN		0x2
#define STO_PROTECTED		0x3
#define STO_OPTIONAL		0x4
#define STO_SC_ALIGN_UNUSED	0xff	/* No longer used 		*/

/*
 * Special Irix st_info
 */
#define STB_SPLIT_COMMON	(STB_LOPROC+0)

#define EF_MIPS_PIC	  2		/* Contains PIC code */
#define EF_MIPS_CPIC	  4		/* Uses PIC calling sequence */
#define EF_MIPS_OPTIONS_FIRST	0x00000080 

#ifdef X86_WHIRL_OBJECTS
#define SHT_WHIRL_SECTION SHT_PROGBITS
#else 
#define SHT_WHIRL_SECTION SHT_MIPS_WHIRL
#endif

typedef enum {
    EK_NULL = 0x00,	    /* No valid information */
    EK_ADDR_RESET = 0x01,   /* Reset offset into associated text section */
    EK_INCR_LOC_EXT = 0x02, /* Increment offset into associated text section */
    EK_ENTRY = 0x03,	    /* Subprogram entrypoint */
    EK_IF_ENTRY = 0x04,	    /* Subprogram entrypoint with associated interface offset */
    EK_EXIT = 0x05,	    /* Subprogram exit */
    EK_PEND = 0x06,	    /* Subprogram end (last instruction) */

    EK_SWITCH_32 = 0x7,	    /* jr for switch stmt, table entries are 32bit */
    EK_SWITCH_64 = 0x8,	    /* jr for switch stmt, table entries are 64bit */
    EK_DUMMY = 0x09,	    /* empty slot */

    EK_BB_START = 0x0a,	    /* Basic block beginning */
    EK_INCR_LOC_UNALIGNED = 0x0b,    /* Increment unaligned byte offset */
    EK_GP_PROLOG_HI = 0x0c, /* Establish high 16bits of GP */
    EK_GP_PROLOG_LO = 0x0d, /* Establish low 16bits of GP */
    EK_GOT_PAGE = 0x0e,	    /* Compact relocation: GOT page pointer */
    EK_GOT_OFST = 0x0f,     /* Compact relocation: GOT page offset */
    EK_HI = 0x10,	    /* Compact relocation: high 16bits of abs. addr */
    EK_LO = 0x11,	    /* Compact relocation: low 16bits of abs. addr */
    EK_64_HIGHEST = 0x12,   /* Compact relocation: most significant 16 bits
			       of a 64bit absolute address */
    EK_64_HIGHER = 0x13,    /* Compact relocation: second most significant
			       16 bits of a 64bit absolute address */
    EK_64_HIGH = 0x14,	    /* Compact relocation: third most significant
			       16 bits of a 64bit absolute address */
    EK_64_LOW = 0x15,       /* Compact relocation: least significant 16 bits
			       of a 64bit absolute address */
    EK_GPREL = 0x16,        /* Compact relocation: GP relative reference */

    EK_DEF = 0x17,	    /* Define new event kind format */

    EK_FCALL_LOCAL = 0x18,	/* point-of-call (jalr) to a local procedure */
    EK_FCALL_EXTERN = 0x19,	/* jalr to extern procedure (small got case) */
    EK_FCALL_EXTERN_BIG = 0x1a,	/* jalr to extern procedure (large got case) */
    EK_FCALL_MULT = 0x1b,	/* jalr to more than one procedure */
    EK_FCALL_MULT_PARTIAL = 0x1c, /* jalr to multiple + unknown procedures */

    EK_LTR_FCALL = 0x1d,	/* jalr to rld lazy-text res.  index of
				   symbol associated. */
    EK_PCREL_GOT0 = 0x1e, 	/* immediate is hi 16 bits of 32-bit
				   constant.  Argument is offset to lo,
				   in instructions, not bytes*/

    /* The following events are reserved for supporting Purify-type tools: */
    EK_MEM_COPY_LOAD = 0x1f,    /* load only for copying data */
    EK_MEM_COPY_STORE = 0x20,   /* store only for copying data --
                                   LEB128 operand is word offset to
                                   paired load */
    EK_MEM_PARTIAL_LOAD = 0x21, /* load for reference to a subset of bytes --
                                   BYTE operand's 8 bits indicate which
                                   bytes are actually used */
    EK_MEM_EAGER_LOAD = 0x22,   /* load is speculative */
    EK_MEM_VALID_LOAD = 0x23,   /* load of data known to be valid */

		/*
		 * Yet to be defined kinds with no fields (like EK_EXIT)
		 */
    EK_CK_UNUSED_NONE_0 = 0x50, /*  */
    EK_CK_UNUSED_NONE_1 = 0x51, /*  */
    EK_CK_UNUSED_NONE_2 = 0x52, /*  */
    EK_CK_UNUSED_NONE_3 = 0x53, /*  */
    EK_CK_UNUSED_NONE_4 = 0x54, /*  */

		/*
		 * Yet to be defined kinds with 1 16 bit field
		 */
    EK_CK_UNUSED_16BIT_0 = 0x55,
    EK_CK_UNUSED_16BIT_1 = 0x56,
    EK_CK_UNUSED_16BIT_2 = 0x57, /*  */
    EK_CK_UNUSED_16BIT_3 = 0x58, /*  */
    EK_CK_UNUSED_16BIT_4 = 0x59, /*  */

		/*
		 * Yet to be defined kinds with 1 32 bit field
		 */
    EK_CK_UNUSED_32BIT_0 = 0x5a, /*  */
    EK_CK_UNUSED_32BIT_1 = 0x5b, /*  */
    EK_CK_UNUSED_32BIT_2 = 0x5c, /*  */

		/*
		 * Yet to be defined kinds with 1 64 bit field
		 */

    EK_CK_UNUSED_64BIT_0 = 0x5d,
    EK_CK_UNUSED_64BIT_1 = 0x5e,
    EK_CK_UNUSED_64BIT_2 = 0x5f, /*  */
    EK_CK_UNUSED_64BIT_3 = 0x60, /*  */
    EK_CK_UNUSED_64BIT_4 = 0x61, /*  */

		/*
		 * Yet to be defined kinds with 1 uleb128 field
		 */
    EK_CK_UNUSED_ULEB128_0 = 0x62, /* */
    EK_CK_UNUSED_ULEB128_1 = 0x63, /*  */
    EK_CK_UNUSED_ULEB128_2 = 0x64, /*  */
    EK_CK_UNUSED_ULEB128_3 = 0x65, /*  */
    EK_CK_UNUSED_ULEB128_4 = 0x66, /*  */
    EK_CK_UNUSED_ULEB128_5 = 0x67, /*  */
    EK_CK_UNUSED_ULEB128_6 = 0x68, /*  */
    EK_CK_UNUSED_ULEB128_7 = 0x69, /*  */
    EK_CK_UNUSED_ULEB128_8 = 0x6a, /*  */
    EK_CK_UNUSED_ULEB128_9 = 0x6b, /*  */


    EK_INCR_LOC = 0x80	    /* Increment offset into associated text section */

} Elf_MIPS_Event_Kind;
typedef	Elf_MIPS_Event_Kind Elf_Event_Kind;

/* Entries found in sections of type SHT_MIPS_OPTIONS.  */

typedef struct
{
  unsigned char kind;		/* Determines interpretation of the
				   variable part of descriptor.  */
  unsigned char size;		/* Size of descriptor, including header.  */
  Elf32_Section section;	/* Section header index of section affected,
				   0 for global options.  */
  Elf32_Word info;		/* Kind-specific information.  */
} Elf_Options;

/* Entry found in `.options' section.  */

typedef struct
{
  Elf32_Word hwp_flags1;	/* Extra flags.  */
  Elf32_Word hwp_flags2;	/* Extra flags.  */
} Elf_Options_Hw;

typedef struct {
    Elf64_Word symbol;		/* symbol table index of subprogram, or 0 */
    Elf64_Half attrs;		/* Attributes: See list below */
    Elf64_Byte pcnt;		/* Parameter count */
    Elf64_Byte fpmask;		/* bit on indicates an FP parameter register */
} Elf_Ifd;

/* Flags that can be set in the 'attrs' field of Elf_Interface_Descriptor */
#define SA_PROTOTYPED	0x8000	/* Does def or ref have prototype ? */
#define SA_VARARGS	0x4000	/* Is this a varargs subprogram ? */
#define SA_PIC		0x2000	/* Are memory references PIC? */
#define SA_DSO_ENTRY	0x1000	/* Is subprogram valid DSO entry? */
#define SA_ADDRESSED	0x0800	/* Is subprogram address taken? */
#define SA_FUNCTION	0x0400	/* Does subprogram return a result? */
#define SA_NESTED	0x0200	/* Is subprogram nested? */
#define SA_IGNORE_ERROR	0x0100	/* Ignore consistency errors? */
#define SA_DEFINITION	0x0080	/* Is this a definition (no just call)? */
#define SA_AT_FREE	0x0040	/* Is the at register free at all branches? */
#define SA_FREE_REGS	0x0020	/* Free register mask precedes parm profile */
#define SA_PARAMETERS	0x0010	/* Parameter profile follows descriptor? */
#define SA_ALTINTERFACE 0x0008	/* Alternate descriptor follows? */

/* Content kind -- valid for ELF-32 and ELF-64: */
typedef enum {
    CK_NULL	= 0,	    /* Invalid, same as EK_NULL */
    CK_DEFAULT	= 0x30,	    /* Default type of data for section */
    CK_ALIGN	= 0x31,	    /* Alignment for described range */
    CK_INSTR	= 0x32,	    /* Instructions */
    CK_DATA	= 0x33,	    /* Non-address data */
    CK_SADDR_32	= 0x34,	    /* Simple 32-bit addresses */
    CK_GADDR_32	= 0x35,	    /* GP-relative 32-bit addresses */
    CK_CADDR_32	= 0x36,	    /* Complex 32-bit addresses */
    CK_SADDR_64	= 0x37,	    /* Simple 64-bit addresses */
    CK_GADDR_64	= 0x38,	    /* GP-relative 64-bit addresses */
    CK_CADDR_64	= 0x39,	    /* Complex 64-bit addresses */
    CK_NO_XFORM	= 0x3a,	    /* No transformations allowed in this range */
    CK_NO_REORDER = 0x3b,   /* No reordering allowed in this range */
    CK_GP_GROUP = 0x3c,	    /* Text/data in range with length given by
			       second argument references GP group given
			       by first. */
    CK_STUBS	= 0x3d	    /* Text in range is stub code. ULEB */
} Elf_MIPS_Content_Kind;

/*
 * 	Elf_Options.kind		Options descriptor kinds
 */
#define ODK_NULL                 0      /* Undefined */
#define ODK_MIPS_REGINFO         1      /* MIPS Register usage information */
#define ODK_REGINFO         1      /* MIPS Register usage information */
#define ODK_MIPS_EXCEPTIONS      2      /* MIPS Exception processing options  */
#define ODK_EXCEPTIONS		 2      /* MIPS Exception processing options  */
#define ODK_PAD                  3      /* Section padding options */
#define ODK_MIPS_HWPATCH         4      /* MIPS Hardware workarounds performed*/
#define ODK_HWPATCH		 4      /* MIPS Hardware workarounds performed*/
#define ODK_FILL                 5      /* The fill value used by the linker */
#define ODK_TAGS                 6      /* Space for desktop tools to write */
#define ODK_MIPS_HWAND           7      /* HW workarounds.'AND' when merging */
#define ODK_HWAND		 7      /* HW workarounds.'AND' when merging */
#define ODK_MIPS_HWOR            8      /* HW workarounds.'OR'  when merging */
#define ODK_HWOR		 8      /* HW workarounds.'OR'  when merging */
#define ODK_GP_GROUP             9      /* GP group for text/data sections */
#define ODK_IDENT               10      /* ID information */
#define ODK_IA64_REGINFO        11      /* TO BE CHANGED! */
#define ODK_PAGESIZE            12      /* Alternate segment page size */


#define ODK_IA64_EXCEPTIONS     12      /* NOT USED! */
#define ODK_IA64_HWAND          13      /* NOT USED! */
#define ODK_IA64_HWOR           14      /* NOT USED! */

/*
 *	Elf_Options.kind == ODK_MIPS_EXCEPTIONS	
 *
 *      masks for Elf_Options.info
 */
#define OEX_PAGE0       0x10000 /* page zero must be mapped */
#define OEX_SMM         0x20000 /* Force sequential memory mode? */
#define OEX_FPDBUG      0x40000 /* Force floating point debug mode? */
#define OEX_PRECISEFP   OEX_FPDBUG
#define OEX_DISMISS     0x80000 /* Dismiss invalid address faults? */
#define OEX_FPU_MIN     0x1f    /* FPE's which MUST be enabled */
#define OEX_FPU_MAX     0x1f00  /* FPE's which MAY be enabled */
#define OEX_FPU_INVAL   0x10
#define OEX_FPU_DIV0    0x08
#define OEX_FPU_OFLO    0x04
#define OEX_FPU_UFLO    0x02
#define OEX_FPU_INEX    0x01

/*
 *	Elf_Options.kind == ODK_MIPS_HWPATCH
 *
 *      masks for Elf_Options.info
 */
#define OHW_R4KEOP      0x1     /* R4000 end-of-page patch */
#define OHW_R8KPFETCH   0x2     /* may need R8000 prefetch patch */
#define OHW_R5KEOP      0x4     /* R5000 end-of-page patch */
#define OHW_R5KCVTL     0x8     /* R5000 cvt.[ds].l bug.  clean=1 */
#define OHW_R10KLDL     0x10    /* R10000 requires LDL patch    */

/*
 *      Elf_Options.kind == ODK_MIPS_HWOR
 *
 *      masks for Elf_Options.info
 */
#define OHWO0_FIXADE	0x00000001	/* Object requires FIXADE call */

/*
 *      Elf_Options.kind == ODK_PAD
 *
 *      masks for Elf_Options.info
 */
#define OPAD_PREFIX	0x1	
#define OPAD_POSTFIX	0x2
#define OPAD_SYMBOL	0x4

/*
 *      Elf_Options.kind == ODK_GP_GROUP
 *
 *      masks for Elf_Options.info
 */
#define OGP_GROUP	0x0000ffff	/* GP group number */
#define OGP_SELF	0x00010000	/* Self-contained GP groups */

/* Fundamental Parameter Types */
#define FT_unknown         0x0000
#define FT_signed_char     0x0001
#define FT_unsigned_char   0x0002
#define FT_signed_short    0x0003
#define FT_unsigned_short  0x0004
#define FT_signed_int32    0x0005
#define FT_unsigned_int32  0x0006
#define FT_signed_int64    0x0007
#define FT_unsigned_int64  0x0008
#define FT_pointer32       0x0009
#define FT_pointer64       0x000a
#define FT_float32         0x000b
#define FT_float64         0x000c
#define FT_float128        0x000d
#define FT_complex64       0x000e
#define FT_complex128      0x000f
#define FT_complex256      0x0010
#define FT_void            0x0011
#define FT_bool32          0x0012
#define FT_bool64          0x0013
#define FT_label32         0x0014
#define FT_label64         0x0015
#define FT_struct          0x0020
#define FT_union           0x0021
#define FT_enum            0x0022
#define FT_typedef         0x0023
#define FT_set             0x0024
#define FT_range           0x0025
#define FT_member_ptr      0x0026
#define FT_virtual_ptr     0x0027
#define FT_class           0x0028

/* Parameter Qualifiers (aka Modifiers)  */
#define MOD_pointer_to     0x01
#define MOD_reference_to   0x02
#define MOD_const          0x03
#define MOD_volatile       0x04
#define MOD_function       0x80
#define MOD_array_of       0x81

/* Parameter descriptor mask flags */
#define PDMF_REFERENCE  0x40
#define PDMF_SIZE       0x20
#define PDMF_Qualifiers 0x0f

/* Needed for libdwarf */
#define R_386_32                1

#endif /* !ELF_DEFINES_H_ */

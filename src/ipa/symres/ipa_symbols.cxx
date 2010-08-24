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

#include <ipa_symbols.h>
#include <stdlib.h>
#include <fcntl.h>

#include "errors.h"


int ObjectReader::openFile(char *fname){

	char *base_ptr;

        if((fd = open(filename.c_str(), O_RDWR)) == ERR)                    
        {
		ErrMsg("couldnt open %s\n", file);
		return -1;
        }

        if((fstat(fd, &elf_stats)))
        {
		ErrMsg("could not fstat %s\n", file);
                close(fd);
		return -1;
        }

        if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
        {
		ErrMsg("could not malloc\n");
                close(fd);
		return -1;
        }

        if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
        {
		ErrMsg("could not read %s\n", file);
                free(base_ptr);
                close(fd);
		return -1;
        }

	elf_header = (Elf32_Ehdr *) base_ptr;	// point elf_header at our object in memory
	elf = elf_begin(fd, ELF_C_READ, NULL);	// Initialize 'elf' pointer to our file descriptor}
}

ObjectKind ObjectReader::getFileKind(){
	Elf_Kind ek;
	
	ek = elf_kind (e);
	switch (ek) {
	case ELF_K_AR :
		return KIND_ARCHIVE;
		break ;
	case ELF_K_ELF :
		retrurn KIND_OBJECT;
		break ;
	case ELF_K_NONE :
	default :
		return KIND_UNRECOGNIZED;
	}
	/* NOTREACHED */
}


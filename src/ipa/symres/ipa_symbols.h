#ifndef IPA_SYMBOLS_H_INCLUDED
#define IPA_SYMBOLS_H_INCLUDED

#include <elf.h>

enum ObjectKind{
	KIND_ARCHIVE,
        KIND_OBJECT,
        KIND_UNRECOGNIZED
};


class ObjectReader{
public:
	int openFile(char *fname);
	ObjectKind getFileKind();
        int startSymbolLookup();
        std::string getNextDefinedSymbol(); //we care only about defined symbols so far
private:
        Elf32_Ehdr *elf_header;		// ELF header 
	Elf *elf;                       // Our Elf pointer for libelf 
	Elf_Scn *scn;                   // Section Descriptor 
	Elf_Data *edata;                // Data Descriptor 
	GElf_Sym sym;			// Symbol 
	GElf_Shdr shdr;                 // Section Header 

        int fd; 			// File Descriptor
        std::string filename;	
        struct stat elf_stats;	
};        


#endif //IPA_SYMBOLS_H_INCLUDED


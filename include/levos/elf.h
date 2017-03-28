#ifndef __LEVOS_ELF_H
#define __LEVOS_ELF_H

#include <levos/types.h>
#include <levos/compiler.h>
#include <levos/fs.h>

int load_elf(struct file *);

inline const char *elf_type_as_string(uint8_t t)
{
    switch (t) {
        case 1: return "relocatable";
        case 2: return "executable";
        case 3: return "shared";
        case 4: return "core";
        default: return "unknown";
    }
}


typedef struct {
    unsigned char ei_magic[4]; /* 0x7F"ELF" */
    uint8_t ei_subarch;
    uint8_t ei_endian;
    uint8_t ei_elfver;
    uint8_t ei_abi;
    unsigned char ei_unused[8];
} elf_ident_t;

typedef struct {
    elf_ident_t e_ident;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf_header_t;

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} elf_section_header_t __packed;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf_program_header_t;


#endif /* __LEVOS_ELF_H */

/*
 * COMP 3430 - Operating Systems, Assignment 1, Question 1
 *
 * Reads a 64-bit ELF binary file and prints:
 *   - ELF header fields
 *   - All program headers with first 32 bytes of data
 *   - All section headers with names and first 32 bytes of data
 */

#include <stdio.h> //fopen, fread, fseek, printf
#include <stdlib.h> //malloc , free, exit
#include <stdint.h> // uint8_t, uint16_t, uint32_t, uint64_t
#include <string.h> // memset

//ELF DATA TYPE DEFINITION

/* ELF magic bytes that must appear at the start of every ELF file */
#define ELF_MAGIC_0   0x7f
#define ELF_MAGIC_1   'E'
#define ELF_MAGIC_2   'L'
#define ELF_MAGIC_3   'F'

#define ELF_CLASS_64  2   /* e_ident[4]: 64-bit file */
#define MAX_DUMP_BYTES 32 /* max bytes to dump per section/segment */

// ELF header is always the first 64 bytes of the file
// field names and sizes come from the ELF spec
typedef struct {
    uint8_t  e_ident[16];   // magic, class, endianness, OS ABI, padding
    uint16_t e_type;     // file type (executable, shared lib, etc)
    uint16_t e_machine;     // ISA (0x3e = x86-64)
    uint32_t e_version;
    uint64_t e_entry;       // entry point address
    uint64_t e_phoff;     // offset to program header table
    uint64_t e_shoff;       // offset to section header table
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;   // size of each program header entry
    uint16_t e_phnum;       // number of program headers
    uint16_t e_shentsize;   // size of each section header entry
    uint16_t e_shnum;       // number of section headers
    uint16_t e_shstrndx;    // index of the string table section header
} ElfHeader;
 
// each program header describes one segment (region loaded into memory)
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;  // where in the file this segment starts
    uint64_t p_vaddr;   // virtual address in memory
    uint64_t p_paddr;
    uint64_t p_filesz;  // size in file
    uint64_t p_memsz;
    uint64_t p_align;
} ProgramHeader;
 
// each section header describes one section (.text, .data, etc)
typedef struct {
    uint32_t sh_name;       // index into string table (not the name itself)
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;       // virtual address
    uint64_t sh_offset;     // where in the file this section starts
    uint64_t sh_size;       // size in bytes
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} SectionHeader;

// Helper: prints up to MAX_DUMP_BYTES bytes from a file at a given offset

static void print_hex_dump(FILE *fp, uint64_t offset, uint64_t size)
{
    uint64_t to_read = (size < MAX_DUMP_BYTES) ? size : MAX_DUMP_BYTES;
    uint8_t  buf[MAX_DUMP_BYTES];
    uint64_t i;

    /* If there's nothing to dump, print a blank line and return */
    if (to_read == 0) {
        printf("\n");
        return;
    }

    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: failed to seek to offset 0x%lx\n", offset);
        return;
    }

    to_read = fread(buf, 1, to_read, fp);

    for (i = 0; i < to_read; i++) {
        printf("%02x", buf[i]);

        /* After every 16 bytes start a new line, otherwise a space */
        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else if (i + 1 < to_read) {
            printf(" ");
        }
    }

    /* Trailing newline if the last row was not exactly 16 bytes */
    if (to_read % 16 != 0) {
        printf("\n");
    }
}

// reads the ELF header and prints its fields
// returns 0 on success, -1 if not a valid 64-bit ELF
static int print_elf_header(FILE *fp, ElfHeader *hdr)
{
    rewind(fp);
    if (fread(hdr, sizeof(ElfHeader), 1, fp) != 1) {
        fprintf(stderr, "Error: could not read ELF header.\n");
        return -1;
    }
 
    // first 4 bytes must be the ELF magic number
    if (hdr->e_ident[0] != ELF_MAGIC_0 ||
        hdr->e_ident[1] != ELF_MAGIC_1 ||
        hdr->e_ident[2] != ELF_MAGIC_2 ||
        hdr->e_ident[3] != ELF_MAGIC_3) {
        fprintf(stderr, "Error: not an ELF file.\n");
        return -1;
    }
 
    // byte 4 tells us if its 32 or 64 bit
    if (hdr->e_ident[4] != ELF_CLASS_64) {
        printf("ELF header:\n");
        printf(" * 32-bit\n");
        fprintf(stderr, "Error: only 64-bit ELF files are supported.\n");
        return -1;
    }
 
    printf("ELF header:\n");
    printf(" * 64-bit\n");
    printf(" * %s\n", (hdr->e_ident[5] == 1) ? "little" : "big");
    printf(" * compiled for 0x%02x (operating system)\n", hdr->e_ident[7]);
    printf(" * has type 0x%02x\n", hdr->e_type);
    printf(" * compiled for 0x%02x (isa)\n", hdr->e_machine);
    printf(" * entry point address              0x%016lx\n", hdr->e_entry);
    printf(" * program header table starts at   0x%016lx\n", hdr->e_phoff);
    printf(" * There are %d program headers, each is %d bytes\n",
           hdr->e_phnum, hdr->e_phentsize);
    printf(" * There are %d section headers, each is %d bytes\n",
           hdr->e_shnum, hdr->e_shentsize);
    printf(" * The section header string table is %d\n", hdr->e_shstrndx);
 
    return 0;
}

// loops through all program headers and prints each one
static void print_program_headers(FILE *fp, const ElfHeader *hdr)
{
    ProgramHeader phdr;
    int i;
 
    for (i = 0; i < hdr->e_phnum; i++) {
        // calculate where the i-th program header is in the file
        long offset = (long)(hdr->e_phoff + (uint64_t)i * hdr->e_phentsize);
        if (fseek(fp, offset, SEEK_SET) != 0) {
            fprintf(stderr, "Error: could not seek to program header %d.\n", i);
            return;
        }
        if (fread(&phdr, sizeof(ProgramHeader), 1, fp) != 1) {
            fprintf(stderr, "Error: could not read program header %d.\n", i);
            return;
        }
 
        printf("\nProgram header #%d:\n", i);
        printf(" * segment type 0x%08x\n", phdr.p_type);
        printf(" * virtual address of segment 0x%016lx\n", phdr.p_vaddr);
        printf(" * size in file %lu bytes\n", phdr.p_filesz);
        printf(" * first up to 32 bytes starting at file offset 0x%016lx:\n",
               phdr.p_offset);
        print_hex_dump(fp, phdr.p_offset, phdr.p_filesz);
    }
}

// loads the string table into memory so we can look up section names
// the string table is just one big block of null-terminated strings
// caller must free the returned pointer
static char *load_string_table(FILE *fp, const ElfHeader *hdr)
{
    SectionHeader shdr;
    char *strtab;
    long offset;
 
    // find the string table section header using e_shstrndx
    offset = (long)(hdr->e_shoff + (uint64_t)hdr->e_shstrndx * hdr->e_shentsize);
    if (fseek(fp, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: could not seek to string table section header.\n");
        return NULL;
    }
    if (fread(&shdr, sizeof(SectionHeader), 1, fp) != 1) {
        fprintf(stderr, "Error: could not read string table section header.\n");
        return NULL;
    }
 
    // allocate a buffer and read the whole string table into it
    strtab = malloc(shdr.sh_size);
    if (strtab == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return NULL;
    }
 
    if (fseek(fp, (long)shdr.sh_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: could not seek to string table data.\n");
        free(strtab);
        return NULL;
    }
    if (fread(strtab, 1, shdr.sh_size, fp) != shdr.sh_size) {
        fprintf(stderr, "Error: could not read string table data.\n");
        free(strtab);
        return NULL;
    }
 
    return strtab;
}

// loops through all section headers and prints each one
static void print_section_headers(FILE *fp, const ElfHeader *hdr)
{
    SectionHeader shdr;
    char *strtab;
    int i;
 
    // need the string table loaded first to get section names
    strtab = load_string_table(fp, hdr);
    if (strtab == NULL) {
        return;
    }
 
    for (i = 0; i < hdr->e_shnum; i++) {
        long offset = (long)(hdr->e_shoff + (uint64_t)i * hdr->e_shentsize);
        if (fseek(fp, offset, SEEK_SET) != 0) {
            fprintf(stderr, "Error: could not seek to section header %d.\n", i);
            break;
        }
        if (fread(&shdr, sizeof(SectionHeader), 1, fp) != 1) {
            fprintf(stderr, "Error: could not read section header %d.\n", i);
            break;
        }
 
        // sh_name is an offset into the string table, not the name itself
        printf("\nSection header #%d:\n", i);
        printf(" * section name >>%s<<\n", strtab + shdr.sh_name);
        printf(" * type 0x%02x\n", shdr.sh_type);
        printf(" * virtual address of section 0x%016lx\n", shdr.sh_addr);
        printf(" * size in file %lu bytes\n", shdr.sh_size);
        printf(" * first up to 32 bytes starting at file offset 0x%016lx:\n",
               shdr.sh_offset);
        print_hex_dump(fp, shdr.sh_offset, shdr.sh_size);
    }
 
    free(strtab);
}
 
int main(int argc, char *argv[])
{
    FILE *fp;
    ElfHeader hdr;
 
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <elf-file>\n", argv[0]);
        return EXIT_FAILURE;
    }
 
    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    }
 
    if (print_elf_header(fp, &hdr) != 0) {
        fclose(fp);
        return EXIT_FAILURE;
    }
 
    print_program_headers(fp, &hdr);
    print_section_headers(fp, &hdr);
 
    fclose(fp);
    return EXIT_SUCCESS;
}
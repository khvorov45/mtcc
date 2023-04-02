#include "cbuild.h"

#define function static
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)
#define assert(x) prb_assert(x)

typedef prb_Arena Arena;
typedef prb_Bytes Bytes;
typedef prb_Str   Str;
typedef uint8_t   u8;
typedef int32_t   i32;

function prb_Status
execCmd(Arena* arena, Str cmd) {
    prb_Process proc = prb_createProcess(cmd, (prb_ProcessSpec) {});
    prb_Status  result = prb_launchProcesses(arena, &proc, 1, prb_Background_No);
    return result;
}

function Str
ctoasm(Arena* arena, Str program) {
    prb_unused(arena);
    prb_unused(program);
    char digitChar = program.ptr[program.len - 3];
    Str  result = prb_fmt(
        arena,
        "    .intel_syntax noprefix\n"
         "    .globl main\n"
         "main:\n"
         "    mov eax, %c\n"
         "    ret\n",
        digitChar
    );
    return result;
}

typedef struct BinaryWriter {
    Arena* arena;
    Bytes  bytes;
} BinaryWriter;

function BinaryWriter
binstart(Arena* arena) {
    assert(!arena->lockedForStr);
    arena->lockedForStr = true;
    BinaryWriter bw = {.arena = arena, .bytes.data = prb_arenaFreePtr(arena)};
    return bw;
}

function void*
binwrite(BinaryWriter* bw, i32 len) {
    assert(bw->arena->lockedForStr);
    assert(len <= prb_arenaFreeSize(bw->arena));
    void* ptr = prb_arenaFreePtr(bw->arena);
    prb_memset(ptr, 0, len);
    prb_arenaChangeUsed(bw->arena, len);
    bw->bytes.len += len;
    return ptr;
}

function void*
binwrited(BinaryWriter* bw, u8* data, i32 len) {
    assert(bw->arena->lockedForStr);
    assert(len <= prb_arenaFreeSize(bw->arena));
    void* ptr = prb_arenaFreePtr(bw->arena);
    prb_memcpy(ptr, data, len);
    prb_arenaChangeUsed(bw->arena, len);
    bw->bytes.len += len;
    return ptr;
}

function void*
binwrites(BinaryWriter* bw, char* str) {
    assert(bw->arena->lockedForStr);
    void* ptr = prb_arenaFreePtr(bw->arena);
    i32   len = prb_strlen(str) + 1;
    assert(len <= prb_arenaFreeSize(bw->arena));
    prb_memcpy(ptr, str, len);
    prb_arenaChangeUsed(bw->arena, len);
    bw->bytes.len += len;
    return ptr;
}

function void
binalign(BinaryWriter* bw, i32 align) {
    intptr_t freeBefore = prb_arenaFreeSize(bw->arena);
    prb_arenaAlignFreePtr(bw->arena, align);
    bw->bytes.len += freeBefore - prb_arenaFreeSize(bw->arena);
}

function Bytes
binend(BinaryWriter* bw) {
    assert(bw->arena->lockedForStr);
    bw->arena->lockedForStr = false;
    Bytes result = bw->bytes;
    *bw = (BinaryWriter) {};
    return result;
}

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

enum {
    EI_MAG0 = 0,  // File identification
    EI_MAG1 = 1,  // File identification
    EI_MAG2 = 2,  // File identification
    EI_MAG3 = 3,  // File identification
    EI_CLASS = 4,  // File class
    EI_DATA = 5,  // Data encoding
    EI_VERSION = 6,  // File version
    EI_OSABI = 7,  // Operating system/ABI identification
    EI_ABIVERSION = 8,  // ABI version
    EI_PAD = 9,  // Start of padding bytes
    EI_NIDENT = 16,  // Size of e_ident[]
};

enum {
    ELFCLASSNONE = 0,  // Invalid class
    ELFCLASS32 = 1,  // 32-bit objects
    ELFCLASS64 = 2,  // 64-bit objects
};

enum {
    ELFDATANONE = 0,  // Invalid data encoding
    ELFDATA2LSB = 1,  // Little-endian
    ELFDATA2MSB = 2,  // Big-endian
};

enum {
    EV_NONE = 0,  // Invalid version
    EV_CURRENT = 1,  // Current version
};

enum {
    ET_NONE = 0,  // No file type
    ET_REL = 1,  // Relocatable file
    ET_EXEC = 2,  // Executable file
    ET_DYN = 3,  // Shared object file
    ET_CORE = 4,  // Core file
    ET_LOOS = 0xfe00,  // Operating system-specific
    ET_HIOS = 0xfeff,  // Operating system-specific
    ET_LOPROC = 0xff00,  // Processor-specific
    ET_HIPROC = 0xffff,  // Processor-specific
};

enum {
    EM_X86_64 = 62,
};

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

enum {
    SHT_NULL = 0,
    SHT_PROGBITS = 1,
    SHT_SYMTAB = 2,
    SHT_STRTAB = 3,
    SHT_RELA = 4,
    SHT_HASH = 5,
    SHT_DYNAMIC = 6,
    SHT_NOTE = 7,
    SHT_NOBITS = 8,
    SHT_REL = 9,
    SHT_SHLIB = 10,
    SHT_DYNSYM = 11,
    SHT_INIT_ARRAY = 14,
    SHT_FINI_ARRAY = 15,
    SHT_PREINIT_ARRAY = 16,
    SHT_GROUP = 17,
    SHT_SYMTAB_SHNDX = 18,
    SHT_LOOS = 0x60000000,
    SHT_HIOS = 0x6fffffff,
    SHT_LOPROC = 0x70000000,
    SHT_HIPROC = 0x7fffffff,
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xffffffff,
};

enum {
    SHF_WRITE = 0x1,
    SHF_ALLOC = 0x2,
    SHF_EXECINSTR = 0x4,
    SHF_MERGE = 0x10,
    SHF_STRINGS = 0x20,
    SHF_INFO_LINK = 0x40,
    SHF_LINK_ORDER = 0x80,
    SHF_OS_NONCONFORMING = 0x100,
    SHF_GROUP = 0x200,
    SHF_TLS = 0x400,
    SHF_MASKOS = 0x0ff00000,
    SHF_MASKPROC = 0xf0000000,
};

enum {
    STB_LOCAL = 0,
    STB_GLOBAL = 1,
    STB_WEAK = 2,
    STB_LOOS = 10,
    STB_HIOS = 12,
    STB_LOPROC = 13,
    STB_HIPROC = 15,
};

enum {
    STT_NOTYPE = 0,
    STT_OBJECT = 1,
    STT_FUNC = 2,
    STT_SECTION = 3,
    STT_FILE = 4,
    STT_COMMON = 5,
    STT_TLS = 6,
    STT_LOOS = 10,
    STT_HIOS = 12,
    STT_LOPROC = 13,
    STT_HIPROC = 15,
};

typedef struct {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

function Bytes
asmToBin(Arena* arena, Str assembly) {
    prb_unused(arena);
    prb_unused(assembly);

    BinaryWriter bw = binstart(arena);

    Elf64_Ehdr* header = (Elf64_Ehdr*)binwrite(&bw, sizeof(Elf64_Ehdr));

    *header = (Elf64_Ehdr) {
        .e_ident[EI_MAG0] = '\x7f',
        .e_ident[EI_MAG1] = 'E',
        .e_ident[EI_MAG2] = 'L',
        .e_ident[EI_MAG3] = 'F',
        .e_ident[EI_CLASS] = ELFCLASS64,
        .e_ident[EI_DATA] = ELFDATA2LSB,
        .e_ident[EI_VERSION] = EV_CURRENT,
        .e_type = ET_REL,
        .e_machine = EM_X86_64,
        .e_version = EV_CURRENT,
        .e_ehsize = sizeof(Elf64_Ehdr),
        .e_shentsize = sizeof(Elf64_Shdr),
        .e_shnum = 4,
        .e_shstrndx = 1,  // TODO(khvorov) Points to where string table header went
    };

    // NOTE(khvorov) Program instructions
    Elf64_Shdr instrHeader = {
        .sh_name = 0,  // TODO(khvorov) Write name
        .sh_type = SHT_PROGBITS,
        .sh_offset = bw.bytes.len,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addralign = 4,
    };

    {
        i32 instrSize = 6;
        u8* instr = binwrite(&bw, instrSize);
        instr[0] = 0xb8;
        instr[1] = 0x02;
        instr[5] = 0xc3;

        instrHeader.sh_size = instrSize;
    }

    // NOTE(khvorov) Symbol table
    binalign(&bw, 8);
    Elf64_Shdr symHeader = {
        .sh_name = 0,  // TODO(khvorov) Write name
        .sh_type = SHT_SYMTAB,
        .sh_offset = bw.bytes.len,
        .sh_size = 2 * sizeof(Elf64_Sym),
        .sh_link = 1,  // TODO(khvorov) String header table index in the section header array
        .sh_info = 1,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Sym),
    };

    {
        binwrite(&bw, sizeof(Elf64_Sym));  // NOTE(khvorov) Empty first symbol
        Elf64_Sym* mainsym = binwrite(&bw, sizeof(Elf64_Sym));
        *mainsym = (Elf64_Sym) {
            .st_name = 7,  // TODO(khvorov) Write name properly
            .st_info = (STB_GLOBAL << 4) | (STT_NOTYPE),
            .st_shndx = 2,  // TODO(khvorov) This is supposed to point to wherever the instructions header went?
        };
    }

    // TODO(khvorov) String table
    Elf64_Shdr strHeader = {
        .sh_name = 0,  // TODO(khvorov) Write name
        .sh_type = SHT_STRTAB,
        .sh_offset = bw.bytes.len,
        .sh_addralign = 1,
    };

    {
        binwrite(&bw, 1);  // NOTE(khvorov) Null first byte
        binwrites(&bw, ".text");
        binwrites(&bw, "main");
        binwrites(&bw, ".strtab");
        binwrites(&bw, ".symtab");

        strHeader.sh_size = bw.bytes.len - strHeader.sh_offset;
    }

    binalign(&bw, 8);
    header->e_shoff = bw.bytes.len;

    binwrite(&bw, sizeof(Elf64_Shdr));  // NOTE(khvorov) Empty first entry
    binwrited(&bw, (u8*)&strHeader, sizeof(Elf64_Shdr));
    binwrited(&bw, (u8*)&instrHeader, sizeof(Elf64_Shdr));
    binwrited(&bw, (u8*)&symHeader, sizeof(Elf64_Shdr));

    Bytes result = binend(&bw);
    return result;
}

function Bytes
binToExe(Arena* arena, Bytes obj) {
    prb_unused(arena);
    prb_unused(obj);

    // TODO(khvorov) Implement

    Bytes result = {};
    return result;
}

function void
printStrTable(Arena* arena, Elf64_Shdr header, Bytes data) {
    char* strtab = (char*)data.data + header.sh_offset;
    prb_writeToStdout(STR("String table:\n"));
    for (i32 ind = 1; ind < (i32)header.sh_size;) {
        char* str = strtab + ind;
        prb_writeToStdout(prb_fmt(arena, "%d: `%s`\n", ind, str));
        ind += prb_strlen(str) + 1;
    }
}

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    {
        Str   program = STR("int main() {return 2;}");
        Str   progAsm = ctoasm(arena, program);
        Bytes progBin = asmToBin(arena, progAsm);
        Bytes progExe = binToExe(arena, progBin);

        Elf64_Ehdr* myheader = (Elf64_Ehdr*)progBin.data;
        Elf64_Shdr* mysections = (Elf64_Shdr*)(progBin.data + myheader->e_shoff);
        Elf64_Sym*  mysyms = (Elf64_Sym*)(progBin.data + mysections[3].sh_offset);
        prb_unused(mysyms);

        // NOTE(khvorov) Reference assembly
        {
            Str progpath = prb_pathJoin(arena, rootDir, STR("temp-prog.c"));
            prb_assert(prb_writeEntireFile(arena, progpath, program.ptr, program.len));
            Str progRefAssembly = prb_pathJoin(arena, rootDir, STR("temp-prog-ref.s"));
            prb_assert(execCmd(arena, prb_fmt(arena, "clang -O3 -S -fno-asynchronous-unwind-tables -masm=intel %.*s -o %.*s", LIT(progpath), LIT(progRefAssembly))));
        }

        // NOTE(khvorov) Reference binary
        {
            Str asmPath = prb_pathJoin(arena, rootDir, STR("temp-prog.s"));
            prb_assert(prb_writeEntireFile(arena, asmPath, progAsm.ptr, progAsm.len));
            Str refPath = prb_pathJoin(arena, rootDir, STR("temp-prog-ref.obj"));
            prb_assert(execCmd(arena, prb_fmt(arena, "clang -c %.*s -o %.*s", LIT(asmPath), LIT(refPath))));
            prb_assert(execCmd(arena, prb_fmt(arena, "objdump -d -M intel %.*s", LIT(refPath))));
            prb_ReadEntireFileResult res = prb_readEntireFile(arena, refPath);
            prb_assert(res.success);
            Elf64_Ehdr* header = (Elf64_Ehdr*)res.content.data;
            Elf64_Shdr* sections = (Elf64_Shdr*)(res.content.data + header->e_shoff);
            Elf64_Sym*  syms = (Elf64_Sym*)(res.content.data + sections[3].sh_offset);
            prb_unused(syms);
            printStrTable(arena, sections[1], res.content);
        }

        // NOTE(khvorov) Reference exe
        {
            Str objPath = prb_pathJoin(arena, rootDir, STR("temp-prog.obj"));
            prb_assert(prb_writeEntireFile(arena, objPath, progBin.data, progBin.len));
            prb_assert(execCmd(arena, prb_fmt(arena, "objdump -d -M intel %.*s", LIT(objPath))));
            printStrTable(arena, mysections[1], progBin);
            Str refPath = prb_pathJoin(arena, rootDir, STR("temp-prog-ref.exe"));
            prb_assert(execCmd(arena, prb_fmt(arena, "clang %.*s -o %.*s", LIT(objPath), LIT(refPath))));
        }

        Str progExePath = prb_pathJoin(arena, rootDir, STR("temp-prog.exe"));
        prb_writeEntireFile(arena, progExePath, progExe.data, progExe.len);
        execCmd(arena, progExePath);
    }

    return 0;
}

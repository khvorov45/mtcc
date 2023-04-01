#include "cbuild.h"

#define function static
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)

typedef prb_Arena Arena;
typedef prb_Str   Str;

function void
execCmd(Arena* arena, Str cmd) {
    prb_Process proc = prb_createProcess(cmd, (prb_ProcessSpec) {});
    prb_assert(prb_launchProcesses(arena, &proc, 1, prb_Background_No));
}

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    {
        Str program = STR("int main() {return 2;}");
        Str progpath = prb_pathJoin(arena, rootDir, STR("temp-prog.c"));
        prb_assert(prb_writeEntireFile(arena, progpath, program.ptr, program.len));
        Str progGCCAssembly = prb_pathJoin(arena, rootDir, STR("temp-prog-gcc.s"));
        execCmd(arena, prb_fmt(arena, "gcc -O3 -S -fno-asynchronous-unwind-tables -masm=intel %.*s -o %.*s", LIT(progpath), LIT(progGCCAssembly)));

        Str progMyAssembly = prb_pathJoin(arena, rootDir, STR("temp-prog.s"));
        Str progMyExe = prb_pathJoin(arena, rootDir, STR("temp-prog.exe"));
        execCmd(arena, prb_fmt(arena, "gcc -g %.*s -o %.*s", LIT(progMyAssembly), LIT(progMyExe)));
    }

    return 0;
}

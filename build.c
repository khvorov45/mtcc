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
    prb_writelnToStdout(arena, cmd);
    prb_Process proc = prb_createProcess(cmd, (prb_ProcessSpec) {});
    prb_Status  result = prb_launchProcesses(arena, &proc, 1, prb_Background_No);
    return result;
}

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));
    Str tccDir = prb_pathJoin(arena, rootDir, STR("tinycc"));
    Str tccIncludeDir = prb_pathJoin(arena, tccDir, STR("include"));

    Str outDir = prb_pathJoin(arena, rootDir, STR("build"));
    assert(prb_createDirIfNotExists(arena, outDir));
    // assert(prb_clearDir(arena, outDir));

    Str tccExe = prb_pathJoin(arena, outDir, STR("tcc.exe"));
    Str tccLib = prb_pathJoin(arena, outDir, STR("tcc.lib"));

    // NOTE(khvorov) Compile TCC
    {
        Str libtcc1 = prb_pathJoin(arena, outDir, STR("libtcc1.a"));

        // NOTE(khvorov) Run the c2str tool
        {
            Str defs_ = prb_pathJoin(arena, tccDir, STR("tccdefs_.h"));
            if (!prb_isFile(arena, defs_)) {
                Str file = prb_pathJoin(arena, tccDir, STR("conftest.c"));
                Str out = prb_pathJoin(arena, tccDir, STR("c2str.exe"));
                assert(execCmd(arena, prb_fmt(arena, "clang -DC2STR %.*s -o %.*s", LIT(file), LIT(out))));

                Str defs = prb_pathJoin(arena, tccDir, STR("include/tccdefs.h"));
                assert(execCmd(arena, prb_fmt(arena, "%.*s %.*s %.*s", LIT(out), LIT(defs), LIT(defs_))));
            }
        }

        // NOTE(khvorov) Generate the config file
        {
            Str config = prb_pathJoin(arena, tccDir, STR("config.h"));
            if (!prb_isFile(arena, config)) {
                Str content = STR("#define CONFIG_TCC_PREDEFS 1\n#define TCC_VERSION \"0.9.27\"");
                assert(prb_writeEntireFile(arena, config, content.ptr, content.len));
            }
        }

        // NOTE(khvorov) Compile the main exe
        {
            if (!prb_isFile(arena, tccExe)) {
                Str in = prb_pathJoin(arena, tccDir, STR("tcc.c"));
                assert(execCmd(arena, prb_fmt(arena, "clang -g -DONE_SOURCE=1 -DCONFIG_TRIPLET=\"x86_64-linux-gnu\" -DTCC_TARGET_X86_64 %.*s -o %.*s", LIT(in), LIT(tccExe))));
            }
        }

        // NOTE(khvorov) Compile the main exe but as a library
        {
            if (!prb_isFile(arena, tccLib)) {
                Str in = prb_pathJoin(arena, tccDir, STR("tcc.c"));
                Str obj = prb_pathJoin(arena, tccDir, STR("tcc.o"));
                assert(execCmd(arena, prb_fmt(arena, "%.*s -g -c -I%.*s -Dmain=tcc_main -DONE_SOURCE=1 -DCONFIG_TRIPLET=\"x86_64-linux-gnu\" -DTCC_TARGET_X86_64 %.*s -o %.*s", LIT(tccExe), LIT(tccIncludeDir), LIT(in), LIT(obj))));
                assert(execCmd(arena, prb_fmt(arena, "%.*s -ar rcs %.*s %.*s", LIT(tccExe), LIT(tccLib), LIT(obj))));
            }
        }

        // NOTE(khvorov) Compile libtcc1 (not to be confused with libtcc which is different and included in tcc.exe)
        {
            if (!prb_isFile(arena, libtcc1)) {
                char* src[] = {
                    "libtcc1.c",
                    "alloca.S",
                    "alloca-bt.S",
                    "stdatomic.c",
                    "atomic.S",
                    "builtin.c",
                    "tcov.c",
                    "va_list.c",
                    "dsohandle.c",
                };

                Str tccLibDir = prb_pathJoin(arena, tccDir, STR("lib"));

                Str* allObjs = 0;
                for (i32 ind = 0; ind < prb_arrayCount(src); ind++) {
                    Str file = prb_pathJoin(arena, tccLibDir, STR(src[ind]));
                    Str out = prb_replaceExt(arena, file, STR("o"));
                    arrput(allObjs, out);
                    assert(execCmd(arena, prb_fmt(arena, "%.*s -g -c -I%.*s %.*s -o %.*s", LIT(tccExe), LIT(tccIncludeDir), LIT(file), LIT(out))));
                }

                Str allObjsStr = prb_stringsJoin(arena, allObjs, arrlen(allObjs), STR(" "));
                assert(execCmd(arena, prb_fmt(arena, "%.*s -ar rcs %.*s %.*s", LIT(tccExe), LIT(libtcc1), LIT(allObjsStr))));
            }
        }
    }

    // NOTE(khvorov) Test-run tcc
    if (false) {
        assert(execCmd(arena, prb_fmt(arena, "%.*s -g -I%.*s -L%.*s %s -o temp.exe", LIT(tccExe), LIT(tccIncludeDir), LIT(outDir), __FILE__)));
    }

    if (true) {
        Str content = STR("int main() {return 0;}");
        Str path = prb_pathJoin(arena, rootDir, STR("temp.c"));
        assert(prb_writeEntireFile(arena, path, content.ptr, content.len));
    }
}

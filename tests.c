#include "cbuild.h"

#define mtcc_assert(x) prb_assert(x)
#include "mtcc.h"

#define function static
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)
#define PTM(x) \
    (mtcc_Str) { x.ptr, x.len }
#define MTP(x) \
    (prb_Str) { x.ptr, x.len }
#define assert(x) prb_assert(x)

typedef prb_Arena Arena;
typedef prb_Bytes Bytes;
typedef prb_Str   Str;
typedef uint8_t   u8;
typedef int32_t   i32;

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    Str             content = STR("int main() {return 0;}");
    mtcc_Preprocess pp = mtcc_beginPreprocess(PTM(content));

    // while (mtcc_nextPreprocessAction(&tu)) {
    //     switch (tu.curPreprocessAction.kind) {
    //         case mtcc_PreprocessActionKind_None: assert(!"unreachable"); break;

    //         case mtcc_PreprocessActionKind_AddFileContent: {
    //         } break;

    //         case mtcc_PreprocessActionKind_GetAllEntriesInDirRecrusively: {
    //         } break;
    //     }
    // }

    return 0;
}

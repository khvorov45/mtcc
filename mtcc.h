#include <stdint.h>

#ifndef mtcc_assert
#define mtcc_assert(x)
#endif

#ifndef mtcc_PUBLICAPI
#define mtcc_PUBLICAPI static
#endif

typedef struct mtcc_Str {
    const char* ptr;
    intptr_t    len;
} mtcc_Str;

// typedef enum mtcc_More {
//     mtcc_More_No,
//     mtcc_More_Yes,
// } mtcc_More;

// typedef enum mtcc_PreprocessActionKind {
//     mtcc_PreprocessActionKind_None,
//     mtcc_PreprocessActionKind_AddFileContent,
//     mtcc_PreprocessActionKind_GetAllEntriesInDirRecrusively,
// } mtcc_PreprocessActionKind;

// typedef struct mtcc_PreprocessAction {
//     mtcc_PreprocessActionKind kind;
// } mtcc_PreprocessAction;

typedef struct mtcc_IncludeDirective {
    mtcc_Str literal;
} mtcc_IncludeDirective;

typedef struct mtcc_IncludeDirectiveArray {
    mtcc_IncludeDirective* ptr;
    intptr_t               len;
} mtcc_IncludeDirectiveArray;

typedef struct mtcc_Preprocess {
    int ignore_;
} mtcc_Preprocess;

mtcc_PUBLICAPI mtcc_Preprocess
mtcc_beginPreprocess(mtcc_Str fileContent) {
    mtcc_ensureTrailingUnescapedNewline();
    mtcc_removeEscapedNewlines();
    mtcc_decomposeIntoPreprocessingTokens();
    mtcc_replaceCommentsWithSpaces();
    // mtcc_executePreprocessingDirectives();
    // mtcc_deletePreprocessingDirectives();
    mtcc_Preprocess pp = {};
    return pp;
}

// mtcc_PUBLICAPI mtcc_More
// mtcc_nextPreprocessAction(mtcc_TranslationUnit* tu) {
//     mtcc_More result = mtcc_More_No;
//     return result;
// }

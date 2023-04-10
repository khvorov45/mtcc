#include <stdint.h>

#ifndef mtcc_assert
#define mtcc_assert(x)
#endif

#ifndef mtcc_PUBLICAPI
#define mtcc_PUBLICAPI static
#endif

#ifndef mtcc_PRIVATEAPI
#define mtcc_PRIVATEAPI static
#endif

#define mtcc_STR(x) (mtcc_Str) {x, mtcc_strlen(x)}

typedef struct mtcc_Str {
    const char* ptr;
    intptr_t    len;
} mtcc_Str;

typedef enum mtcc_PPTokenKind {
    mtcc_PPTokenKind_None,
    mtcc_PPTokenKind_Comment,
    mtcc_PPTokenKind_EscapedNewline,
    mtcc_PPTokenKind_Whitespace,
    mtcc_PPTokenKind_HeaderName,
    mtcc_PPTokenKind_Ident,
    mtcc_PPTokenKind_PPNumber,
    mtcc_PPTokenKind_CharConst,
    mtcc_PPTokenKind_StrLit,
    mtcc_PPTokenKind_Punctuator,
    mtcc_PPTokenKind_Other,
} mtcc_PPTokenKind;

typedef struct mtcc_PPToken {
    mtcc_PPTokenKind kind;
    mtcc_Str         str;
} mtcc_PPToken;

typedef struct mtcc_PPTokenArray {
    mtcc_PPToken* ptr;
    intptr_t      len;
} mtcc_PPTokenArray;

typedef struct mtcc_Preprocess {
    mtcc_PPTokenArray ppTokens;
} mtcc_Preprocess;

mtcc_PRIVATEAPI bool
mtcc_memeq(const void* ptr1, const void* ptr2, intptr_t bytes) {
    mtcc_assert(bytes >= 0);
    bool result = true;
    for (intptr_t ind = 0; ind < bytes; ind++) {
        if (((uint8_t*)ptr1)[ind] != ((uint8_t*)ptr2)[ind]) {
            result = false;
            break;
        }
    }
    return result;
}

mtcc_PRIVATEAPI intptr_t
mtcc_strlen(const char* ptr) {
    intptr_t result = 0;
    for (; ptr[result] != '\0'; result += 1) {}
    return result;
}

mtcc_PRIVATEAPI bool
mtcc_isspace(char ch) {
    bool result = ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\v' || ch == '\f';
    return result;
}

mtcc_PRIVATEAPI bool
mtcc_streq(mtcc_Str str1, mtcc_Str str2) {
    bool result = false;
    if (str1.len == str2.len) {
        result = mtcc_memeq(str1.ptr, str2.ptr, str1.len);
    }
    return result;
}

mtcc_PUBLICAPI mtcc_Preprocess
mtcc_beginPreprocess(mtcc_Str fileContent) {
    mtcc_Preprocess pp = {};

    mtcc_PPToken lastNotSpaceOrComment1 = {};
    mtcc_PPToken lastNotSpaceOrComment2 = {};

    for (intptr_t offset = 0; offset < fileContent.len;) {
        mtcc_PPToken tok = {};

        char ch = fileContent.ptr[offset];

        // NOTE(khvorov) Comment
        if (ch == '/' && offset < fileContent.len - 1) {
            char nextCh = fileContent.ptr[offset + 1];
            switch (nextCh) {
                case '*': {
                    tok.kind = mtcc_PPTokenKind_Comment;
                    tok.str.ptr = fileContent.ptr + offset;
                    intptr_t offsetBefore = offset;
                    offset += 2;

                    for (; offset < fileContent.len; offset += 1) {
                        char commentCh = fileContent.ptr[offset];
                        if (commentCh == '*' && offset < fileContent.len - 1) {
                            char nextCommentCh = fileContent.ptr[offset + 1];
                            if (nextCommentCh == '/') {
                                offset += 2;
                                break;
                            }
                        }
                    }

                    tok.str.len = offset - offsetBefore;
                } break;

                case '/': {
                    tok.kind = mtcc_PPTokenKind_Comment;
                    tok.str.ptr = fileContent.ptr + offset;
                    intptr_t offsetBefore = offset;
                    offset += 2;

                    for (; offset < fileContent.len; offset += 1) {
                        char commentCh = fileContent.ptr[offset];
                        if (commentCh == '\n' || commentCh == '\r') {
                            break;
                        }
                    }

                    tok.str.len = offset - offsetBefore;
                } break;

                default: break;
            }
        }

        // NOTE(khvorov) Escaped newlines
        if (tok.kind == mtcc_PPTokenKind_None && ch == '\\' && offset < fileContent.len - 1) {
            char nextCh = fileContent.ptr[offset + 1];
            if (mtcc_isspace(nextCh)) {
                for (intptr_t aheadInd = offset + 2; aheadInd < fileContent.len; aheadInd++) {
                    char aheadCh = fileContent.ptr[aheadInd];
                    if (aheadCh == '\r' || aheadCh == '\n') {
                        tok.kind = mtcc_PPTokenKind_EscapedNewline;
                        tok.str.ptr = fileContent.ptr + offset;
                        tok.str.len = aheadInd - offset;
                        offset = aheadInd;
                        if (aheadCh == '\r' && offset < fileContent.len - 1) {
                            char chAfterNewline = fileContent.ptr[offset + 1];
                            if (chAfterNewline == '\n') {
                                tok.str.len += 1;
                                offset += 1;
                            }
                        }
                    } else if (!mtcc_isspace(aheadCh)) {
                        break;
                    }
                }
            }
        }

        // NOTE(khvorov) Whitespace
        if (tok.kind == mtcc_PPTokenKind_None && mtcc_isspace(ch)) {
            tok.kind = mtcc_PPTokenKind_Whitespace;
            tok.str.ptr = fileContent.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            for (; offset < fileContent.len; offset += 1) {
                char nextCh = fileContent.ptr[offset];
                if (!mtcc_isspace(nextCh)) {
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        // NOTE(khvorov) Header name
        if (lastNotSpaceOrComment1.kind == mtcc_PPTokenKind_Punctuator && lastNotSpaceOrComment1.str.len == 1 && lastNotSpaceOrComment1.str.ptr[0] == '#') {
            if (lastNotSpaceOrComment2.kind == mtcc_PPTokenKind_Ident && mtcc_streq(lastNotSpaceOrComment2.str, mtcc_STR("include"))) {
                if (ch == '"' || ch == '<') {
                    tok.kind = mtcc_PPTokenKind_HeaderName;
                    tok.str.ptr = fileContent.ptr + offset;
                    intptr_t offsetBefore = offset;
                    offset += 1;
                    char searchCh = ch == '"' ? '"' : '>';
                    for (; offset < fileContent.len; offset += 1) {
                        char nextCh = fileContent.ptr[offset];
                        if (nextCh == searchCh) {
                            break;
                        }
                    }
                    tok.str.len = offset - offsetBefore;
                }
            }
        }

        mtcc_assert(tok.kind != mtcc_PPTokenKind_None);

        if (tok.kind != mtcc_PPTokenKind_EscapedNewline && tok.kind != mtcc_PPTokenKind_Whitespace && tok.kind != mtcc_PPTokenKind_Comment) {
            lastNotSpaceOrComment1 = lastNotSpaceOrComment2;
            lastNotSpaceOrComment2 = tok;
        }
    }

    return pp;
}

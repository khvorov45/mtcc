#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

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

#define mtcc_STR(x) \
    (mtcc_Str) { x, mtcc_strlen(x) }

typedef enum mtcc_More {
    mtcc_More_No,
    mtcc_More_Yes,
} mtcc_More;

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

typedef struct mtcc_Str {
    const char* ptr;
    intptr_t    len;
} mtcc_Str;

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

typedef enum mtcc_PPTokenIterState {
    mtcc_PPTokenIterState_None,
    mtcc_PPTokenIterState_Pound,
    mtcc_PPTokenIterState_PoundInclude,
} mtcc_PPTokenIterState;

typedef struct mtcc_PPTokenIter {
    mtcc_Str              input;
    intptr_t              offset;
    mtcc_PPToken          pptoken;
    mtcc_PPTokenIterState state;
} mtcc_PPTokenIter;

mtcc_PUBLICAPI mtcc_PPTokenIter
mtcc_createPPTokenIter(mtcc_Str input) {
    mtcc_PPTokenIter iter = {.input = input};
    return iter;
}

mtcc_PUBLICAPI mtcc_More
mtcc_ppTokenIterNext(mtcc_PPTokenIter* iter) {
    mtcc_More result = mtcc_More_No;

    if (iter->offset < iter->input.len) {
        result = mtcc_More_Yes;

        intptr_t     offset = iter->offset;
        mtcc_PPToken tok = {};

        char ch = iter->input.ptr[offset];

        // NOTE(khvorov) Comment
        if (tok.kind == mtcc_PPTokenKind_None && ch == '/' && offset < iter->input.len - 1) {
            char nextCh = iter->input.ptr[offset + 1];
            switch (nextCh) {
                case '*': {
                    tok.kind = mtcc_PPTokenKind_Comment;
                    tok.str.ptr = iter->input.ptr + offset;
                    intptr_t offsetBefore = offset;
                    offset += 2;

                    for (; offset < iter->input.len; offset += 1) {
                        char commentCh = iter->input.ptr[offset];
                        if (commentCh == '*' && offset < iter->input.len - 1) {
                            char nextCommentCh = iter->input.ptr[offset + 1];
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
                    tok.str.ptr = iter->input.ptr + offset;
                    intptr_t offsetBefore = offset;
                    offset += 2;

                    bool escaped = false;
                    for (; offset < iter->input.len; offset += 1) {
                        char commentCh = iter->input.ptr[offset];
                        if (!escaped && (commentCh == '\n' || commentCh == '\r')) {
                            break;
                        }
                        escaped = commentCh == '\\';
                    }

                    tok.str.len = offset - offsetBefore;
                } break;

                default: break;
            }
        }

        // NOTE(khvorov) Escaped newlines
        if (tok.kind == mtcc_PPTokenKind_None && ch == '\\' && offset < iter->input.len - 1) {
            for (intptr_t aheadInd = offset + 1; aheadInd < iter->input.len; aheadInd++) {
                char aheadCh = iter->input.ptr[aheadInd];
                if (aheadCh == '\r' || aheadCh == '\n') {
                    tok.kind = mtcc_PPTokenKind_EscapedNewline;
                    tok.str.ptr = iter->input.ptr + offset;
                    tok.str.len = aheadInd - offset + 1;
                    offset = aheadInd + 1;
                    if (aheadCh == '\r' && offset < iter->input.len - 1) {
                        char chAfterNewline = iter->input.ptr[offset + 1];
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

        // NOTE(khvorov) Whitespace
        if (tok.kind == mtcc_PPTokenKind_None && mtcc_isspace(ch)) {
            tok.kind = mtcc_PPTokenKind_Whitespace;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            for (; offset < iter->input.len; offset += 1) {
                char nextCh = iter->input.ptr[offset];
                if (!mtcc_isspace(nextCh)) {
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        // NOTE(khvorov) Identifier
        if (tok.kind == mtcc_PPTokenKind_None && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '_'))) {
            tok.kind = mtcc_PPTokenKind_Ident;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            for (; offset < iter->input.len; offset += 1) {
                char nextCh = iter->input.ptr[offset];
                if (!((nextCh >= 'a' && nextCh <= 'z') || (nextCh >= 'A' && nextCh <= 'Z') || (nextCh == '_') || (nextCh >= '0' && nextCh <= '9'))) {
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        // NOTE(khvorov) PPNumber
        if (tok.kind == mtcc_PPTokenKind_None && (ch >= '0' && ch <= '9')) {
            tok.kind = mtcc_PPTokenKind_PPNumber;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            for (; offset < iter->input.len; offset += 1) {
                char nextCh = iter->input.ptr[offset];
                if (!((nextCh >= 'a' && nextCh <= 'z') || (nextCh >= 'A' && nextCh <= 'Z') || (nextCh >= '0' && nextCh <= '9') || nextCh == '.')) {
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        // NOTE(khvorov) Char const and string lit
        if (tok.kind == mtcc_PPTokenKind_None && (ch == '\'' || (iter->state != mtcc_PPTokenIterState_PoundInclude && ch == '"'))) {
            tok.kind = ch == '"' ? mtcc_PPTokenKind_StrLit : mtcc_PPTokenKind_CharConst;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            for (; offset < iter->input.len; offset += 1) {
                char nextCh = iter->input.ptr[offset];
                if (nextCh == ch) {
                    offset += 1;
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        // NOTE(khvorov) Header name
        if (tok.kind == mtcc_PPTokenKind_None && iter->state == mtcc_PPTokenIterState_PoundInclude && (ch == '"' || ch == '<')) {
            tok.kind = mtcc_PPTokenKind_HeaderName;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            char searchCh = ch == '"' ? '"' : '>';
            for (; offset < iter->input.len; offset += 1) {
                char nextCh = iter->input.ptr[offset];
                if (nextCh == searchCh) {
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        mtcc_assert(tok.kind != mtcc_PPTokenKind_None);

        // NOTE(khvorov) State
        switch (iter->state) {
            case mtcc_PPTokenIterState_None: {
                if (tok.kind == mtcc_PPTokenKind_Punctuator && tok.str.len == 0 && tok.str.ptr[0] == '#') {
                    iter->state = mtcc_PPTokenIterState_Pound;
                }
            } break;

            case mtcc_PPTokenIterState_Pound: {
                if (tok.kind == mtcc_PPTokenKind_Ident && mtcc_streq(tok.str, mtcc_STR("include"))) {
                    iter->state = mtcc_PPTokenIterState_PoundInclude;
                } else if (tok.kind != mtcc_PPTokenKind_Comment && tok.kind != mtcc_PPTokenKind_Whitespace && tok.kind != mtcc_PPTokenKind_EscapedNewline) {
                    iter->state = mtcc_PPTokenIterState_None;
                }
            } break;

            case mtcc_PPTokenIterState_PoundInclude: {
                if (tok.kind != mtcc_PPTokenKind_Comment && tok.kind != mtcc_PPTokenKind_Whitespace && tok.kind != mtcc_PPTokenKind_EscapedNewline) {
                    iter->state = mtcc_PPTokenIterState_None;
                }
            } break;
        }

        iter->offset = offset;
        iter->pptoken = tok;
    }

    return result;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include <stdint.h>
#include <stdalign.h>

#ifndef mtcc_assert
#define mtcc_assert(x)
#endif

#ifndef mtcc_PUBLICAPI
#define mtcc_PUBLICAPI static
#endif

#ifndef mtcc_PRIVATEAPI
#define mtcc_PRIVATEAPI static
#endif

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

mtcc_PRIVATEAPI void
mtcc_memset(void* ptr1, uint8_t byte, intptr_t bytes) {
    mtcc_assert(bytes >= 0);
    for (intptr_t ind = 0; ind < bytes; ind++) {
        ((uint8_t*)ptr1)[ind] = byte;
    }
}

#define mtcc_STR(x) \
    (mtcc_Str) { x, mtcc_strlen(x) }

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

typedef struct mtcc_Bytes {
    uint8_t* ptr;
    intptr_t len;
} mtcc_Bytes;

typedef struct mtcc_Arena {
    void*    base;
    intptr_t size;
    intptr_t used;
} mtcc_Arena;

mtcc_PRIVATEAPI mtcc_Arena
mtcc_arenaFromBytes(mtcc_Bytes bytes) {
    mtcc_Arena arena = {.base = bytes.ptr, .size = bytes.len};
    return arena;
}

mtcc_PRIVATEAPI void*
mtcc_arenaFreePtr(mtcc_Arena* arena) {
    void* result = (uint8_t*)arena->base + arena->used;
    return result;
}

mtcc_PRIVATEAPI intptr_t
mtcc_arenaFreeSize(mtcc_Arena* arena) {
    intptr_t result = arena->size - arena->used;
    return result;
}

mtcc_PUBLICAPI intptr_t
mtcc_getOffsetForAlignment(void* ptr, intptr_t align) {
    mtcc_assert(((align > 0) && ((align & (align - 1)) == 0)));
    uintptr_t ptrAligned = (uintptr_t)((uint8_t*)ptr + (align - 1)) & (uintptr_t)(~(align - 1));
    mtcc_assert(ptrAligned >= (uintptr_t)ptr);
    intptr_t diff = (intptr_t)(ptrAligned - (uintptr_t)ptr);
    mtcc_assert(diff < align && diff >= 0);
    return (intptr_t)diff;
}

mtcc_PRIVATEAPI void
mtcc_arenaChangeUsed(mtcc_Arena* arena, intptr_t byteDelta) {
    mtcc_assert(mtcc_arenaFreeSize(arena) >= byteDelta);
    arena->used += byteDelta;
}

mtcc_PRIVATEAPI void
mtcc_arenaAlignFreePtr(mtcc_Arena* arena, intptr_t align) {
    intptr_t offset = mtcc_getOffsetForAlignment(mtcc_arenaFreePtr(arena), align);
    mtcc_arenaChangeUsed(arena, offset);
}

#define mtcc_arenaAllocStruct(arena, type) (type*)mtcc_arenaAllocAndZero(arena, sizeof(type), alignof(type))

mtcc_PRIVATEAPI void*
mtcc_arenaAllocAndZero(mtcc_Arena* arena, intptr_t size, intptr_t align) {
    mtcc_arenaAlignFreePtr(arena, align);
    void* result = mtcc_arenaFreePtr(arena);
    mtcc_arenaChangeUsed(arena, size);
    mtcc_memset(result, 0, (size_t)size);
    return result;
}

typedef enum mtcc_TokenKind {
    mtcc_TokenKind_None,
    mtcc_TokenKind_Comment,
    mtcc_TokenKind_EscapedNewline,
    mtcc_TokenKind_Whitespace,
    mtcc_TokenKind_HeaderName,
    mtcc_TokenKind_Ident,
    mtcc_TokenKind_PPNumber,
    mtcc_TokenKind_CharConst,
    mtcc_TokenKind_StrLit,
    mtcc_TokenKind_Punctuator,
    mtcc_TokenKind_Other,
} mtcc_TokenKind;

typedef struct mtcc_Token {
    mtcc_TokenKind kind;
    mtcc_Str       str;
} mtcc_Token;

typedef enum mtcc_TokenIterState {
    mtcc_TokenIterState_None,
    mtcc_TokenIterState_Pound,
    mtcc_TokenIterState_PoundInclude,
} mtcc_TokenIterState;

typedef struct mtcc_TokenIter {
    mtcc_Str            input;
    intptr_t            offset;
    mtcc_Token          token;
    mtcc_TokenIterState state;
} mtcc_TokenIter;

mtcc_PUBLICAPI mtcc_TokenIter
mtcc_createTokenIter(mtcc_Str input) {
    mtcc_TokenIter iter = {.input = input};
    return iter;
}

mtcc_PUBLICAPI mtcc_More
mtcc_tokenIterNext(mtcc_TokenIter* iter) {
    mtcc_More result = mtcc_More_No;

    if (iter->offset < iter->input.len) {
        result = mtcc_More_Yes;

        intptr_t   offset = iter->offset;
        mtcc_Token tok = {};

        char ch = iter->input.ptr[offset];

        // NOTE(khvorov) Comment
        if (tok.kind == mtcc_TokenKind_None && ch == '/' && offset < iter->input.len - 1) {
            char nextCh = iter->input.ptr[offset + 1];
            switch (nextCh) {
                case '*': {
                    tok.kind = mtcc_TokenKind_Comment;
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
                    tok.kind = mtcc_TokenKind_Comment;
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
        if (tok.kind == mtcc_TokenKind_None && ch == '\\' && offset < iter->input.len - 1) {
            for (intptr_t aheadInd = offset + 1; aheadInd < iter->input.len; aheadInd++) {
                char aheadCh = iter->input.ptr[aheadInd];
                if (aheadCh == '\r' || aheadCh == '\n') {
                    tok.kind = mtcc_TokenKind_EscapedNewline;
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
        if (tok.kind == mtcc_TokenKind_None && mtcc_isspace(ch)) {
            tok.kind = mtcc_TokenKind_Whitespace;
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
        if (tok.kind == mtcc_TokenKind_None && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '_'))) {
            tok.kind = mtcc_TokenKind_Ident;
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
        if (tok.kind == mtcc_TokenKind_None && (ch >= '0' && ch <= '9')) {
            tok.kind = mtcc_TokenKind_PPNumber;
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
        // TODO(khvorov) Prefixes
        if (tok.kind == mtcc_TokenKind_None && (ch == '\'' || (iter->state != mtcc_TokenIterState_PoundInclude && ch == '"'))) {
            tok.kind = ch == '"' ? mtcc_TokenKind_StrLit : mtcc_TokenKind_CharConst;
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
        if (tok.kind == mtcc_TokenKind_None && iter->state == mtcc_TokenIterState_PoundInclude && (ch == '"' || ch == '<')) {
            tok.kind = mtcc_TokenKind_HeaderName;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;
            char searchCh = ch == '"' ? '"' : '>';
            for (; offset < iter->input.len; offset += 1) {
                char nextCh = iter->input.ptr[offset];
                if (nextCh == searchCh) {
                    offset += 1;
                    break;
                }
            }
            tok.str.len = offset - offsetBefore;
        }

        // NOTE(khvorov) Punctuator
        if (tok.kind == mtcc_TokenKind_None && (ch == '[' || ch == ']' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '.' || ch == '-' || ch == '+' || ch == '&' || ch == '*' || ch == '~' || ch == '!' || ch == '/' || ch == '%' || ch == '<' || ch == '>' || ch == '^' || ch == '|' || ch == '?' || ch == ':' || ch == ';' || ch == '=' || ch == ',' || ch == '#')) {
            tok.kind = mtcc_TokenKind_Punctuator;
            tok.str.ptr = iter->input.ptr + offset;
            intptr_t offsetBefore = offset;
            offset += 1;

            if (offset < iter->input.len) {
                char nextCh = iter->input.ptr[offset];
                switch (ch) {
                    case '-': {
                        if (nextCh == '>' || nextCh == '-' || nextCh == '=') {
                            offset += 1;
                        }
                    } break;

                    case '+': {
                        if (nextCh == '+' || nextCh == '=') {
                            offset += 1;
                        }
                    } break;

                    case '<': {
                        if (nextCh == '<' || nextCh == '=' || nextCh == ':' || nextCh == '%') {
                            offset += 1;
                            if (nextCh == '<' && offset < iter->input.len) {
                                char nextNextCh = iter->input.ptr[offset];
                                if (nextNextCh == '=') {
                                    offset += 1;
                                }
                            }
                        }
                    } break;

                    case '>': {
                        if (nextCh == '>' || nextCh == '=') {
                            offset += 1;
                            if (nextCh == '>' && offset < iter->input.len) {
                                char nextNextCh = iter->input.ptr[offset];
                                if (nextNextCh == '=') {
                                    offset += 1;
                                }
                            }
                        }
                    } break;

                    case '%': {
                        if (nextCh == '=' || nextCh == '>' || nextCh == ':') {
                            offset += 1;
                            if (nextCh == ':' && offset < iter->input.len) {
                                char nextNextCh = iter->input.ptr[offset];
                                if (nextNextCh == '%' && offset < iter->input.len - 1) {
                                    char nextNextNextCh = iter->input.ptr[offset + 1];
                                    if (nextNextNextCh == ':') {
                                        offset += 2;
                                    }
                                }
                            }
                        }
                    } break;

                    case '=':
                    case '!':
                    case '*':
                    case '/':
                    case '&':
                    case '^': {
                        if (nextCh == '=') {
                            offset += 1;
                        }
                    } break;

                    case '|': {
                        if (nextCh == '|' || nextCh == '=') {
                            offset += 1;
                        }
                    } break;

                    case '.': {
                        if (nextCh == '.' && offset < iter->input.len - 1) {
                            char nextNextCh = iter->input.ptr[offset + 1];
                            if (nextNextCh == '.') {
                                offset += 2;
                            }
                        }
                    } break;

                    case '#': {
                        if (nextCh == '#') {
                            offset += 1;
                        }
                    } break;

                    case ':': {
                        if (nextCh == '>') {
                            offset += 1;
                        }
                    } break;

                    default: break;
                }
            }

            tok.str.len = offset - offsetBefore;
        }

        // TODO(khvorov) Trigraph sequences

        mtcc_assert(tok.kind != mtcc_TokenKind_None);

        // NOTE(khvorov) State
        switch (iter->state) {
            case mtcc_TokenIterState_None: {
                if (tok.kind == mtcc_TokenKind_Punctuator && tok.str.len == 1 && tok.str.ptr[0] == '#') {
                    iter->state = mtcc_TokenIterState_Pound;
                }
            } break;

            case mtcc_TokenIterState_Pound: {
                if (tok.kind == mtcc_TokenKind_Ident && mtcc_streq(tok.str, mtcc_STR("include"))) {
                    iter->state = mtcc_TokenIterState_PoundInclude;
                } else if (tok.kind != mtcc_TokenKind_Comment && tok.kind != mtcc_TokenKind_Whitespace && tok.kind != mtcc_TokenKind_EscapedNewline) {
                    iter->state = mtcc_TokenIterState_None;
                }
            } break;

            case mtcc_TokenIterState_PoundInclude: {
                if (tok.kind != mtcc_TokenKind_Comment && tok.kind != mtcc_TokenKind_Whitespace && tok.kind != mtcc_TokenKind_EscapedNewline) {
                    iter->state = mtcc_TokenIterState_None;
                }
            } break;
        }

        iter->offset = offset;
        iter->token = tok;
    }

    return result;
}

typedef enum mtcc_ASTSourceKind {
    mtcc_ASTSourceKind_None,
    mtcc_ASTSourceKind_Include,
} mtcc_ASTSourceKind;

typedef struct mtcc_ASTSource {
    mtcc_ASTSourceKind kind;
    mtcc_Str           include;
} mtcc_ASTSource;

typedef enum mtcc_ASTNodeKind {
    mtcc_ASTNodeKind_Root,
    mtcc_ASTNodeKind_PPDirective,
    mtcc_ASTNodeKind_Token,
} mtcc_ASTNodeKind;

typedef struct mtcc_ASTNode mtcc_ASTNode;
typedef struct mtcc_ASTNode {
    mtcc_ASTNode*    parent;
    mtcc_ASTNode*    child;
    mtcc_ASTNode*    nextSibling;
    mtcc_ASTNode*    prevSibling;
    mtcc_ASTNodeKind kind;
    mtcc_Token       token;
    mtcc_ASTNode*    expansion;
} mtcc_ASTNode;

typedef struct mtcc_AST {
    mtcc_ASTNode* root;
} mtcc_AST;

mtcc_PUBLICAPI mtcc_ASTNode*
mtcc_astNextSiblingNodeSkipWhitespaceAndComments(mtcc_ASTNode* start, mtcc_ASTNode* stop) {
    mtcc_ASTNode* result = 0;
    for (mtcc_ASTNode* child = start->nextSibling; child != stop; child = child->nextSibling) {
        mtcc_TokenKind kd = child->token.kind;
        if (kd != mtcc_TokenKind_EscapedNewline && kd != mtcc_TokenKind_Whitespace && kd != mtcc_TokenKind_Comment) {
            result = child;
            break;
        }
    }
    return result;
}

mtcc_PUBLICAPI mtcc_ASTNode*
mtcc_astNextSiblingTokenSkipWhitespaceAndComments(mtcc_ASTNode* start, mtcc_ASTNode* stop) {
    mtcc_ASTNode* result = mtcc_astNextSiblingNodeSkipWhitespaceAndComments(start, stop);
    mtcc_assert(result);
    mtcc_assert(result->kind == mtcc_ASTNodeKind_Token);
    return result;
}

mtcc_PUBLICAPI mtcc_ASTNode*
mtcc_astNextSiblingIdentSkipWhitespaceAndComments(mtcc_ASTNode* start, mtcc_ASTNode* stop) {
    mtcc_ASTNode* result = mtcc_astNextSiblingTokenSkipWhitespaceAndComments(start, stop);
    mtcc_assert(result->token.kind == mtcc_TokenKind_Ident);
    return result;
}

typedef struct mtcc_PPDefine {
    mtcc_Str      name;
    mtcc_ASTNode* replaceFirst;
    mtcc_ASTNode* replaceLast;
} mtcc_PPDefine;

typedef enum mtcc_PPDirectiveKind {
    mtcc_PPDirectiveKind_None,
    mtcc_PPDirectiveKind_Include,
    mtcc_PPDirectiveKind_Define,
} mtcc_PPDirectiveKind;

typedef struct mtcc_PPDirective {
    mtcc_PPDirectiveKind kind;
    union {
        mtcc_Str      include;
        mtcc_PPDefine define;
    };
} mtcc_PPDirective;

typedef struct mtcc_ASTNodeStackEntry mtcc_ASTNodeStackEntry;
typedef struct mtcc_ASTNodeStackEntry {
    mtcc_ASTNode*           node;
    mtcc_ASTNodeStackEntry* prev;
} mtcc_ASTNodeStackEntry;

typedef struct mtcc_PPDefineEntry mtcc_PPDefineEntry;
typedef struct mtcc_PPDefineEntry {
    mtcc_PPDefine       define;
    mtcc_PPDefineEntry* prev;
} mtcc_PPDefineEntry;

typedef struct mtcc_ASTBuilder {
    mtcc_AST                ast;
    mtcc_ASTNode*           node;
    mtcc_Arena              output;
    mtcc_ASTNodeStackEntry* nodesBeforeExpansion;
    mtcc_ASTNodeStackEntry* nodesToExpand;
    mtcc_ASTNodeStackEntry* nodesFree;
    mtcc_PPDefineEntry*     defines;
} mtcc_ASTBuilder;

mtcc_PUBLICAPI mtcc_ASTBuilder
mtcc_createASTBuilder(mtcc_Bytes output) {
    mtcc_ASTBuilder astb = {.output = mtcc_arenaFromBytes(output)};

    astb.node = mtcc_arenaAllocStruct(&astb.output, mtcc_ASTNode);
    astb.node->nextSibling = astb.node;
    astb.node->prevSibling = astb.node;

    astb.ast.root = astb.node;
    return astb;
}

mtcc_PRIVATEAPI mtcc_ASTNode*
mtcc_astBuilderNewRoot(mtcc_ASTBuilder* astb) {
    mtcc_ASTNode* node = mtcc_arenaAllocStruct(&astb->output, mtcc_ASTNode);
    node->nextSibling = node;
    node->prevSibling = node;
    node->kind = mtcc_ASTNodeKind_Root;
    return node;
}

mtcc_PRIVATEAPI mtcc_ASTNode*
mtcc_astBuilderPushChildToken(mtcc_ASTBuilder* astb, mtcc_Token token) {
    mtcc_ASTNode* node = mtcc_arenaAllocStruct(&astb->output, mtcc_ASTNode);
    node->kind = mtcc_ASTNodeKind_Token;
    node->token = token;
    node->parent = astb->node;
    if (astb->node->child) {
        node->nextSibling = astb->node->child;
        node->prevSibling = astb->node->child->prevSibling;
        node->nextSibling->prevSibling = node;
        node->prevSibling->nextSibling = node;
    } else {
        node->nextSibling = node;
        node->prevSibling = node;
        astb->node->child = node;
    }
    return node;
}

mtcc_PRIVATEAPI mtcc_ASTNode*
mtcc_astBuilderPushSiblingToken(mtcc_ASTBuilder* astb, mtcc_Token token) {
    mtcc_ASTNode* node = mtcc_arenaAllocStruct(&astb->output, mtcc_ASTNode);
    node->kind = mtcc_ASTNodeKind_Token;
    node->token = token;
    node->parent = astb->node->parent;
    mtcc_assert(node->parent);
    node->nextSibling = astb->node->nextSibling;
    node->prevSibling = astb->node;
    node->nextSibling->prevSibling = node;
    node->prevSibling->nextSibling = node;
    return node;
}

mtcc_PUBLICAPI void
mtcc_astBuilderNodeStackPush(mtcc_ASTBuilder* astb, mtcc_ASTNodeStackEntry** stack, mtcc_ASTNode* node) {
    mtcc_ASTNodeStackEntry* entry = 0;
    if (astb->nodesFree) {
        entry = astb->nodesFree;
        astb->nodesFree = astb->nodesFree->prev;
    } else {
        entry = mtcc_arenaAllocStruct(&astb->output, mtcc_ASTNodeStackEntry);
    }
    entry->node = node;
    entry->prev = (*stack);
    (*stack) = entry;
}

mtcc_PUBLICAPI mtcc_ASTNode*
mtcc_astBuilderNodeStackPop(mtcc_ASTBuilder* astb, mtcc_ASTNodeStackEntry** stack) {
    mtcc_ASTNodeStackEntry* entry = (*stack);
    mtcc_assert(entry);
    (*stack) = (*stack)->prev;
    mtcc_ASTNode* node = entry->node;
    entry->node = 0;
    entry->prev = astb->nodesFree;
    astb->nodesFree = entry;
    return node;
}

mtcc_PUBLICAPI void
mtcc_astBuilderBeginExpansion(mtcc_ASTBuilder* astb, mtcc_ASTNode* node) {
    mtcc_astBuilderNodeStackPush(astb, &astb->nodesBeforeExpansion, astb->node);
    astb->node = mtcc_astBuilderNewRoot(astb);
    node->expansion = astb->node;
}

mtcc_PUBLICAPI void
mtcc_astBuilderEndExpansion(mtcc_ASTBuilder* astb) {
    astb->node = mtcc_astBuilderNodeStackPop(astb, &astb->nodesBeforeExpansion);
}

mtcc_PUBLICAPI mtcc_PPDirective
mtcc_astParsePPDirective(mtcc_ASTNode* dirRoot) {
    mtcc_PPDirective result = {};

    mtcc_assert(dirRoot->kind == mtcc_ASTNodeKind_PPDirective);
    mtcc_assert(dirRoot->child);
    mtcc_assert(dirRoot->child->kind == mtcc_ASTNodeKind_Token);
    mtcc_assert(dirRoot->child->token.kind == mtcc_TokenKind_Punctuator);
    mtcc_assert(mtcc_streq(dirRoot->child->token.str, mtcc_STR("#")));

    mtcc_ASTNode* directiveName = mtcc_astNextSiblingIdentSkipWhitespaceAndComments(dirRoot->child, dirRoot->child);

    if (mtcc_streq(directiveName->token.str, mtcc_STR("include"))) {
        mtcc_ASTNode* include = mtcc_astNextSiblingTokenSkipWhitespaceAndComments(directiveName, dirRoot->child);
        mtcc_assert(include->token.kind == mtcc_TokenKind_HeaderName);
        result.kind = mtcc_PPDirectiveKind_Include;
        result.include = include->token.str;
    } else if (mtcc_streq(directiveName->token.str, mtcc_STR("define"))) {
        result.kind = mtcc_PPDirectiveKind_Define;
        mtcc_ASTNode* defineName = mtcc_astNextSiblingIdentSkipWhitespaceAndComments(directiveName, dirRoot->child);
        result.define.name = defineName->token.str;
        result.define.replaceFirst = mtcc_astNextSiblingNodeSkipWhitespaceAndComments(defineName, dirRoot->child);
        result.define.replaceLast = dirRoot->child->prevSibling;
    } else {
        mtcc_assert(!"unimplemented");
    }

    return result;
}

mtcc_PUBLICAPI void
mtcc_astBuilderAddDefine(mtcc_ASTBuilder* astb, mtcc_PPDefine define) {
    mtcc_PPDefineEntry entry = {.define = define, .prev = astb->defines};
    astb->defines = mtcc_arenaAllocStruct(&astb->output, mtcc_PPDefineEntry);
    *astb->defines = entry;
}

mtcc_PUBLICAPI mtcc_PPDefine*
mtcc_astBuilderLookupDefine(mtcc_PPDefineEntry* entries, mtcc_Str ident) {
    mtcc_PPDefine* result = 0;

    // TODO(khvorov) Better-than-linear search
    for (mtcc_PPDefineEntry* entry = entries; entry; entry = entry->prev) {
        if (mtcc_streq(entry->define.name, ident)) {
            result = &entry->define;
            break;
        }
    }

    return result;
}

typedef enum mtcc_ASTBuilderActionKind {
    mtcc_ASTBuilderActionKind_None,
    mtcc_ASTBuilderActionKind_Include,
} mtcc_ASTBuilderActionKind;

typedef struct mtcc_ASTBuilderAction {
    mtcc_ASTBuilderActionKind kind;
    mtcc_Str                  include;
    mtcc_ASTNode*             node;
} mtcc_ASTBuilderAction;

mtcc_PUBLICAPI mtcc_ASTBuilderAction
mtcc_astBuilderNext(mtcc_ASTBuilder* astb, mtcc_Token token) {
    mtcc_ASTBuilderAction result = {};

    switch (astb->node->kind) {
        case mtcc_ASTNodeKind_Root: {
            mtcc_ASTNode* child = mtcc_astBuilderPushChildToken(astb, token);
            switch (token.kind) {
                case mtcc_TokenKind_Punctuator: {
                    if (token.str.len == 1 && token.str.ptr[0] == '#') {
                        child->kind = mtcc_ASTNodeKind_PPDirective;
                        child->token = (mtcc_Token) {};
                        astb->node = child;
                        mtcc_astBuilderPushChildToken(astb, token);
                    }
                } break;

                case mtcc_TokenKind_Ident: {
                    mtcc_astBuilderNodeStackPush(astb, &astb->nodesToExpand, child);
                    while (astb->nodesToExpand) {
                        mtcc_ASTNode* nodeToExpand = mtcc_astBuilderNodeStackPop(astb, &astb->nodesToExpand);
                        mtcc_assert(nodeToExpand->kind == mtcc_ASTNodeKind_Token);
                        mtcc_assert(nodeToExpand->token.kind == mtcc_TokenKind_Ident);
                        mtcc_PPDefine* define = mtcc_astBuilderLookupDefine(astb->defines, nodeToExpand->token.str);
                        if (define) {
                            mtcc_astBuilderBeginExpansion(astb, nodeToExpand);
                            for (mtcc_ASTNode* replace = define->replaceFirst;; replace = replace->nextSibling) {
                                mtcc_assert(replace->kind == mtcc_ASTNodeKind_Token);
                                mtcc_assert(replace->child == 0);

                                mtcc_ASTNode* replaceCopy = mtcc_astBuilderPushChildToken(astb, replace->token);

                                if (replace->token.kind == mtcc_TokenKind_Ident) {
                                    mtcc_astBuilderNodeStackPush(astb, &astb->nodesToExpand, replaceCopy);
                                }

                                if (replace == define->replaceLast) {
                                    break;
                                }
                            }
                            mtcc_astBuilderEndExpansion(astb);
                        }
                    }
                } break;

                default: break;
            }
        } break;

        case mtcc_ASTNodeKind_PPDirective: {
            // TODO(khvorov) Should this be a separate token?
            bool unescapedNewline = false;
            if (token.kind == mtcc_TokenKind_Whitespace) {
                for (intptr_t ind = 0; ind < token.str.len; ind++) {
                    char ch = token.str.ptr[ind];
                    if (ch == '\n' || ch == '\r') {
                        unescapedNewline = true;
                        break;
                    }
                }
            }

            if (unescapedNewline) {
                mtcc_PPDirective directive = mtcc_astParsePPDirective(astb->node);
                switch (directive.kind) {
                    case mtcc_PPDirectiveKind_Include: {
                        result.kind = mtcc_ASTBuilderActionKind_Include;
                        result.include = directive.include;
                        result.node = astb->node;
                    } break;

                    case mtcc_PPDirectiveKind_Define: {
                        mtcc_astBuilderAddDefine(astb, directive.define);
                    } break;

                    default: mtcc_assert(!"not sure what to do here");
                }

                mtcc_astBuilderPushSiblingToken(astb, token);
                astb->node = astb->node->parent;
            } else {
                mtcc_astBuilderPushChildToken(astb, token);
            }
        } break;

        case mtcc_ASTNodeKind_Token: mtcc_assert(!"unreachable"); break;
    }

    return result;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "cbuild.h"

#define mtcc_assert(x) prb_assert(x)
#include "mtcc.h"

#define function static
#define STR(x) prb_STR(x)
#define MSTR(x) \
    (mtcc_Str) { x, prb_strlen(x) }
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
typedef intptr_t  isize;

typedef struct VisualTreeNode VisualTreeNode;
typedef struct VisualTreeNode {
    mtcc_ASTNode*   astnode;
    Str             nodeName;
    Str             nodeLabel;
    Str             srcName;
    VisualTreeNode* parent;
    VisualTreeNode* child;
    VisualTreeNode* nextSibling;
    VisualTreeNode* prevSibling;
} VisualTreeNode;

Str globalRootDir = {};

function prb_Status
execCmd(Arena* arena, Str cmd) {
    prb_writelnToStdout(arena, cmd);
    prb_Process proc = prb_createProcess(cmd, (prb_ProcessSpec) {});
    prb_Status  result = prb_launchProcesses(arena, &proc, 1, prb_Background_No);
    return result;
}

function void
addEscapedStr(prb_GrowingStr* gstr, mtcc_Str str) {
    for (isize ind = 0; ind < str.len; ind++) {
        char ch = str.ptr[ind];
        switch (ch) {
            case ' ': prb_addStrSegment(gstr, "\\\\s"); break;
            case '\n': prb_addStrSegment(gstr, "\\\\n"); break;
            case '"': prb_addStrSegment(gstr, "\\\""); break;
            default: prb_addStrSegment(gstr, "%c", ch); break;
        }
    }
}

function void
test_ppTokenIter_withSpaces(Arena* arena, Str* cases, i32 casesCount, mtcc_TokenKind expected) {
    prb_TempMemory temp = prb_beginTempMemory(arena);

    Str spacesAround[casesCount];
    for (i32 ind = 0; ind < casesCount; ind++) {
        spacesAround[ind] = prb_fmt(arena, " %.*s\n", LIT(cases[ind]));
    }

    for (i32 ind = 0; ind < casesCount; ind++) {
        Str            input = spacesAround[ind];
        mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));

        assert(mtcc_tokenIterNext(&iter));
        assert(iter.pptoken.kind == mtcc_TokenKind_Whitespace);
        assert(prb_streq(MTP(iter.pptoken.str), STR(" ")));

        assert(mtcc_tokenIterNext(&iter));
        assert(iter.pptoken.kind == expected);
        assert(prb_streq(MTP(iter.pptoken.str), cases[ind]));

        assert(mtcc_tokenIterNext(&iter));
        assert(iter.pptoken.kind == mtcc_TokenKind_Whitespace);
        assert(prb_streq(MTP(iter.pptoken.str), STR("\n")));

        assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
    }

    prb_endTempMemory(temp);
}

function void
test_tokenIter(Arena* arena) {
    // NOTE(khvorov) Comment
    {
        Str cases[] = {
            STR("// comment"),
            STR("// comment // comment"),
            STR("/* comment */"),
            STR("// comment \\\ncomment"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Comment);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_TokenKind_Comment);
    }

    // NOTE(khvorov) Escaped newline
    {
        Str cases[] = {
            STR("\\\n"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_EscapedNewline);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }
    }

    // NOTE(khvorov) Whitespace
    {
        Str cases[] = {
            STR(" \n \n \t"),
            STR("    "),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Whitespace);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }
    }

    // NOTE(khvorov) Identifier
    {
        Str cases[] = {
            STR("ident"),
            STR("iDent2"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Ident);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_TokenKind_Ident);
    }

    // NOTE(khvorov) PPNumber
    {
        Str cases[] = {
            STR("123"),
            STR("123.5"),
            STR("123.5f"),
            STR("123U"),
            STR("123l"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_PPNumber);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_TokenKind_PPNumber);
    }

    // NOTE(khvorov) Char const
    {
        Str cases[] = {
            STR("'a'"),
            STR("'aba'"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_CharConst);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_TokenKind_CharConst);
    }

    // NOTE(khvorov) String lit
    {
        Str cases[] = {
            STR("\"a\""),
            STR("\"aba\""),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_StrLit);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_TokenKind_StrLit);
    }

    // NOTE(khvorov) Punctuator
    {
        Str cases[] = {
            STR("["),
            STR("]"),
            STR("("),
            STR(")"),
            STR("{"),
            STR("}"),
            STR("."),
            STR("->"),
            STR("++"),
            STR("--"),
            STR("&"),
            STR("*"),
            STR("+"),
            STR("-"),
            STR("~"),
            STR("!"),
            STR("/"),
            STR("%"),
            STR("<<"),
            STR(">>"),
            STR("<"),
            STR(">"),
            STR("<="),
            STR(">="),
            STR("=="),
            STR("!="),
            STR("^"),
            STR("|"),
            STR("||"),
            STR("?"),
            STR(":"),
            STR(";"),
            STR("..."),
            STR("="),
            STR("*="),
            STR("/="),
            STR("%="),
            STR("+="),
            STR("-="),
            STR("<<="),
            STR(">>="),
            STR("&="),
            STR("^="),
            STR("|="),
            STR(","),
            STR("#"),
            STR("##"),
            STR("<:"),
            STR(":>"),
            STR("<%"),
            STR("%>"),
            STR("%:"),
            STR("%:%:"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Punctuator);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_TokenKind_Punctuator);
    }

    // NOTE(khvorov) Header name
    {
        Str cases[] = {
            STR("#include \"header.h\""),
            STR("#include <header.h>"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str            input = cases[ind];
            mtcc_TokenIter iter = mtcc_createTokenIter(PTM(input));
            assert(mtcc_tokenIterNext(&iter));
            assert(mtcc_tokenIterNext(&iter));
            assert(mtcc_tokenIterNext(&iter));
            assert(mtcc_tokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_HeaderName);
            Str headerPath = prb_strSlice(MTP(iter.pptoken.str), 1, iter.pptoken.str.len - 1);
            assert(prb_streq(headerPath, STR("header.h")));
            assert(mtcc_tokenIterNext(&iter) == mtcc_More_No);
        }
    }

    // NOTE(khvorov) Combined
    {
        Str program = STR(
            "#include \"header.h\"\n"
            "int main(void) {\n"
            "    int /*comment*/ x = 0;\n"
            "    // comment\n"
            "    int y = x + 1;\n"
            "    return 0;\n"
            "}\n"
        );

        mtcc_Token expectedTokens[] = {
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("#")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("include")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_HeaderName, .str = MSTR("\"header.h\"")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("int")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("main")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("(")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("void")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR(")")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("{")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n    ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("int")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Comment, .str = MSTR("/*comment*/")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("x")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("=")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_PPNumber, .str = MSTR("0")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR(";")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n    ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Comment, .str = MSTR("// comment")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n    ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("int")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("y")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("=")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("x")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("+")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_PPNumber, .str = MSTR("1")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR(";")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n    ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Ident, .str = MSTR("return")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR(" ")},
            (mtcc_Token) {.kind = mtcc_TokenKind_PPNumber, .str = MSTR("0")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR(";")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Punctuator, .str = MSTR("}")},
            (mtcc_Token) {.kind = mtcc_TokenKind_Whitespace, .str = MSTR("\n")},
        };

        mtcc_TokenIter iter = mtcc_createTokenIter(PTM(program));
        i32            tokensCount = 0;
        for (; mtcc_tokenIterNext(&iter); tokensCount++) {
            assert(tokensCount < prb_arrayCount(expectedTokens));
            mtcc_Token expected = expectedTokens[tokensCount];
            assert(iter.pptoken.kind == expected.kind);
            assert(mtcc_streq(iter.pptoken.str, expected.str));
        }
        assert(tokensCount == prb_arrayCount(expectedTokens));
    }
}

function void
test_ast(Arena* arena) {
    {
        Str program = STR(
            // "#include \"header1.h\"\n"
            "#include <header2.h>\n"
            "int func1() {}\n"
            // "int func2() {}\n"
            // "int func3() {}\n"
            // "int func4() {}\n"
        );

        Str header1 = STR("int h1 = 1;");
        Str header2 = STR("int h2 = 2;");

        prb_Arena       astbArena = prb_createArenaFromArena(arena, 1 * prb_MEGABYTE);
        mtcc_ASTBuilder astb = mtcc_createASTBuilder(
            (mtcc_Bytes) {prb_arenaFreePtr(&astbArena), prb_arenaFreeSize(&astbArena)},
            (mtcc_ASTSource) {}
        );

        mtcc_TokenIter  programIter = mtcc_createTokenIter(PTM(program));
        mtcc_TokenIter* iters = 0;
        arrput(iters, programIter);

        for (; arrlen(iters) > 0;) {
            mtcc_TokenIter* lastIter = iters + arrlen(iters) - 1;

            for (bool breakLoop = false; !breakLoop;) {
                breakLoop = mtcc_tokenIterNext(lastIter) == mtcc_More_No;
                if (!breakLoop) {
                    mtcc_ASTBuilderAction action = mtcc_astBuilderNext(&astb, lastIter->pptoken);

                    switch (action) {
                        case mtcc_ASTBuilderAction_None: break;
                        case mtcc_ASTBuilderAction_SwitchToInclude: {
                            Str path = prb_strSlice(MTP(astb.include), 1, astb.include.len - 1);
                            Str relevantStr = {};
                            if (prb_streq(path, STR("header1.h"))) {
                                relevantStr = header1;
                            } else if (prb_streq(path, STR("header2.h"))) {
                                relevantStr = header2;
                            } else {
                                assert(!"unreachable");
                            }
                            mtcc_TokenIter relevantIter = mtcc_createTokenIter(PTM(relevantStr));
                            arrput(iters, relevantIter);

                            mtcc_astBuilderSwitchToInclude(&astb);
                            breakLoop = true;
                        } break;
                    }
                } else {
                    arrpop(iters);
                    mtcc_astBuilderSrcDone(&astb);
                }
            }
        }

        prb_arenaChangeUsed(arena, astb.output.used);

        // NOTE(khvorov) Visualize the tree
        {
            VisualTreeNode* visRoot = prb_arenaAllocStruct(arena, VisualTreeNode);
            visRoot->astnode = astb.ast.root;
            visRoot->nextSibling = visRoot;
            visRoot->prevSibling = visRoot;

            VisualTreeNode** visnodes = 0;
            arrput(visnodes, visRoot);

            for (i32 visNodeIndex = 0; arrlen(visnodes) > 0; visNodeIndex++) {
                VisualTreeNode* visnode = arrpop(visnodes);

                prb_GrowingStr nodeNameBuilder = prb_beginStr(arena);
                switch (visnode->astnode->kind) {
                    case mtcc_ASTNodeKind_TU: {
                        prb_addStrSegment(&nodeNameBuilder, "TU");
                    } break;

                    case mtcc_ASTNodeKind_PPDirective: {
                        prb_addStrSegment(&nodeNameBuilder, "PPDirective");
                    } break;

                    case mtcc_ASTNodeKind_Token: {
                        prb_addStrSegment(&nodeNameBuilder, "Token");
                    } break;
                }
                prb_addStrSegment(&nodeNameBuilder, "%d", visNodeIndex);
                visnode->nodeName = prb_endStr(&nodeNameBuilder);

                prb_GrowingStr lblBuilder = prb_beginStr(arena);
                switch (visnode->astnode->kind) {
                    case mtcc_ASTNodeKind_TU:
                    case mtcc_ASTNodeKind_PPDirective: {
                        prb_addStrSegment(&lblBuilder, "%.*s", LIT(visnode->nodeName));
                    } break;

                    case mtcc_ASTNodeKind_Token: {
                        addEscapedStr(&lblBuilder, visnode->astnode->token.str);
                    } break;
                }
                visnode->nodeLabel = prb_endStr(&lblBuilder);

                if (visnode->astnode->child) {
                    VisualTreeNode* child = prb_arenaAllocStruct(arena, VisualTreeNode);
                    child->astnode = visnode->astnode->child;
                    child->parent = visnode;
                    visnode->child = child;
                    arrput(visnodes, child);
                }

                if (visnode->astnode->parent) {
                    if (visnode->astnode->nextSibling != visnode->astnode->parent->child) {
                        VisualTreeNode* sibling = prb_arenaAllocStruct(arena, VisualTreeNode);
                        sibling->astnode = visnode->astnode->nextSibling;
                        sibling->parent = visnode->parent;
                        visnode->nextSibling = sibling;
                        sibling->prevSibling = visnode;
                        arrput(visnodes, sibling);
                    } else {
                        visnode->nextSibling = visnode->parent->child;
                        visnode->parent->child->prevSibling = visnode;
                    }
                }

                assert(visnode->astnode->source);
                switch (visnode->astnode->source->src.kind) {
                    case mtcc_ASTSourceKind_None: {
                        visnode->srcName = STR("srcnone");
                    } break;
                    case mtcc_ASTSourceKind_Include: {
                        Str path = prb_strSlice(MTP(visnode->astnode->source->src.include), 1, visnode->astnode->source->src.include.len - 1);
                        if (prb_streq(path, STR("header1.h"))) {
                            visnode->srcName = STR("header1");
                        } else if (prb_streq(path, STR("header2.h"))) {
                            visnode->srcName = STR("header2");
                        } else {
                            assert(!"unreachable");
                        }
                    } break;
                }
            }

            prb_Arena      gstrArena = prb_createArenaFromArena(arena, 1 * prb_MEGABYTE);
            prb_GrowingStr gstr = prb_beginStr(&gstrArena);
            prb_addStrSegment(&gstr, "digraph {\n");

            assert(arrlen(visnodes) == 0);
            arrput(visnodes, visRoot);

            for (i32 visNodeIndex = 0; arrlen(visnodes) > 0; visNodeIndex++) {
                VisualTreeNode* visnode = arrpop(visnodes);

                prb_addStrSegment(&gstr, "    %.*s [label=\"%.*s\"]\n", LIT(visnode->nodeName), LIT(visnode->nodeLabel));

                if (visnode->child) {
                    prb_addStrSegment(&gstr, "    %.*s->%.*s\n", LIT(visnode->nodeName), LIT(visnode->child->nodeName));
                    arrput(visnodes, visnode->child);
                }

                if (visnode->astnode->parent) {
                    prb_addStrSegment(&gstr, "    %.*s->%.*s [arrowhead=box style=dashed]\n", LIT(visnode->nodeName), LIT(visnode->nextSibling->nodeName));
                    prb_addStrSegment(&gstr, "    %.*s->%.*s [arrowhead=box style=dashed color=red]\n", LIT(visnode->nodeName), LIT(visnode->prevSibling->nodeName));
                    if (visnode->nextSibling != visnode->parent->child) {
                        arrput(visnodes, visnode->nextSibling);
                    }
                }

                // TODO(khvorov) Figure out how to label the source when it changes
                // prb_addStrSegment(&gstr, "    %.*s->%.*s [arrowhead=inv style=dotted]\n", LIT(visnode->srcName), LIT(visnode->nodeName));
            }

            prb_addStrSegment(&gstr, "}\n");
            Str treestr = prb_endStr(&gstr);
            Str treepath = prb_pathJoin(arena, globalRootDir, STR("tree.dot"));
            prb_writeEntireFile(arena, treepath, treestr.ptr, treestr.len);
            assert(execCmd(arena, prb_fmt(arena, "dot -Tpdf -O %.*s", LIT(treepath))));
        }
    }
}

int
main(void) {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    globalRootDir = prb_getParentDir(arena, STR(__FILE__));

    test_tokenIter(arena);
    test_ast(arena);

    return 0;
}

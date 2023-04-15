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

function void
test_ppTokenIter_withSpaces(Arena* arena, Str* cases, i32 casesCount, mtcc_TokenKind expected) {
    prb_TempMemory temp = prb_beginTempMemory(arena);

    Str spacesAround[casesCount];
    for (i32 ind = 0; ind < casesCount; ind++) {
        spacesAround[ind] = prb_fmt(arena, " %.*s\n", LIT(cases[ind]));
    }

    for (i32 ind = 0; ind < casesCount; ind++) {
        Str            input = spacesAround[ind];
        mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));

        assert(mtcc_ppTokenIterNext(&iter));
        assert(iter.pptoken.kind == mtcc_TokenKind_Whitespace);
        assert(prb_streq(MTP(iter.pptoken.str), STR(" ")));

        assert(mtcc_ppTokenIterNext(&iter));
        assert(iter.pptoken.kind == expected);
        assert(prb_streq(MTP(iter.pptoken.str), cases[ind]));

        assert(mtcc_ppTokenIterNext(&iter));
        assert(iter.pptoken.kind == mtcc_TokenKind_Whitespace);
        assert(prb_streq(MTP(iter.pptoken.str), STR("\n")));

        assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Comment);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_EscapedNewline);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Whitespace);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Ident);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_PPNumber);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_CharConst);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_StrLit);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_Punctuator);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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
            mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_TokenKind_HeaderName);
            Str headerPath = prb_strSlice(MTP(iter.pptoken.str), 1, iter.pptoken.str.len - 1);
            assert(prb_streq(headerPath, STR("header.h")));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
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

        mtcc_TokenIter iter = mtcc_createPPTokenIter(PTM(program));
        i32            tokensCount = 0;
        for (; mtcc_ppTokenIterNext(&iter); tokensCount++) {
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
            "#include \"header.h\"\n"
            // "int func1() {}\n"
            // "int func2() {}\n"
            // "int func3() {}\n"
            // "int func4() {}\n"
        );

        mtcc_TokenIter  iter = mtcc_createPPTokenIter(PTM(program));
        mtcc_ASTBuilder astb = mtcc_createASTBuilder((mtcc_Bytes) {prb_arenaFreePtr(arena), prb_arenaFreeSize(arena)});

        for (; mtcc_ppTokenIterNext(&iter);) {
            mtcc_astBuilderNext(&astb, iter.pptoken);
        }

        prb_arenaChangeUsed(arena, astb.output.used);
    }
}

int
main(void) {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    // Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    test_tokenIter(arena);
    test_ast(arena);

    return 0;
}

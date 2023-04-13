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

function void
test_ppTokenIter_withSpaces(Arena* arena, Str* cases, i32 casesCount, mtcc_PPTokenKind expected) {
    prb_TempMemory temp = prb_beginTempMemory(arena);

    Str spacesAround[casesCount];
    for (i32 ind = 0; ind < casesCount; ind++) {
        spacesAround[ind] = prb_fmt(arena, " %.*s\n", LIT(cases[ind]));
    }

    for (i32 ind = 0; ind < casesCount; ind++) {
        Str              input = spacesAround[ind];
        mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));

        assert(mtcc_ppTokenIterNext(&iter));
        assert(iter.pptoken.kind == mtcc_PPTokenKind_Whitespace);
        assert(prb_streq(MTP(iter.pptoken.str), STR(" ")));

        assert(mtcc_ppTokenIterNext(&iter));
        assert(iter.pptoken.kind == expected);
        assert(prb_streq(MTP(iter.pptoken.str), cases[ind]));

        assert(mtcc_ppTokenIterNext(&iter));
        assert(iter.pptoken.kind == mtcc_PPTokenKind_Whitespace);
        assert(prb_streq(MTP(iter.pptoken.str), STR("\n")));

        assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
    }

    prb_endTempMemory(temp);
}

function void
test_ppTokenIter(Arena* arena) {
    // NOTE(khvorov) Comment
    {
        Str cases[] = {
            STR("// comment"),
            STR("// comment // comment"),
            STR("/* comment */"),
            STR("// comment \\\ncomment"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_Comment);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_PPTokenKind_Comment);
    }

    // NOTE(khvorov) Escaped newline
    {
        Str cases[] = {
            STR("\\\n"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_EscapedNewline);
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
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_Whitespace);
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
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_Ident);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_PPTokenKind_Ident);
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
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_PPNumber);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_PPTokenKind_PPNumber);
    }

    // NOTE(khvorov) Char const
    {
        Str cases[] = {
            STR("'a'"),
            STR("'aba'"),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_CharConst);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_PPTokenKind_CharConst);
    }

    // NOTE(khvorov) String lit
    {
        Str cases[] = {
            STR("\"a\""),
            STR("\"aba\""),
        };

        for (i32 ind = 0; ind < prb_arrayCount(cases); ind++) {
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_StrLit);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_PPTokenKind_StrLit);
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
            Str              input = cases[ind];
            mtcc_PPTokenIter iter = mtcc_createPPTokenIter(PTM(input));
            assert(mtcc_ppTokenIterNext(&iter));
            assert(iter.pptoken.kind == mtcc_PPTokenKind_Punctuator);
            assert(prb_streq(MTP(iter.pptoken.str), input));
            assert(mtcc_ppTokenIterNext(&iter) == mtcc_More_No);
        }

        test_ppTokenIter_withSpaces(arena, cases, prb_arrayCount(cases), mtcc_PPTokenKind_Punctuator);
    }
}

int
main(void) {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    // Str rootDir = prb_getParentDir(arena, STR(__FILE__));

    test_ppTokenIter(arena);

    return 0;
}

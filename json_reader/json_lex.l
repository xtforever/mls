%{
#define YYSTYPE char*
#include "mls.h"
#include "json_parse.tab.h"
#include "json_read.h"
#include <string.h>
extern int linecnt;
extern char* yylval;

#define YYCP() do { \
yylval=strdup(yytext); \
json_stack(yylval); } while(0)

%}

%option nounput
%option noinput

DIGIT1to9 [1-9]
DIGIT [0-9]
DIGITS {DIGIT}+
INT {DIGIT}|{DIGIT1to9}{DIGITS}|-{DIGIT}|-{DIGIT1to9}{DIGITS}
FRAC [.]{DIGITS}
EXP {E}{DIGITS}
E [eE][+-]?
HEX_DIGIT [0-9a-f]
NUMBER {INT}|{INT}{FRAC}|{INT}{EXP}|{INT}{FRAC}{EXP}
UNESCAPEDCHAR [ -!#-\[\]-~]
ESCAPEDCHAR \\["\\bfnrt/]
UNICODECHAR \\u{HEX_DIGIT}{HEX_DIGIT}{HEX_DIGIT}{HEX_DIGIT}
CHAR {UNESCAPEDCHAR}|{ESCAPEDCHAR}|{UNICODECHAR}
CHARS {CHAR}+
DBL_QUOTE ["]

%%
{DBL_QUOTE}{DBL_QUOTE} |
{DBL_QUOTE}{CHARS}{DBL_QUOTE} {
    YYCP();
    return STRING;
};
{NUMBER} {
    YYCP();
    return NUMBER;
}
true {
    YYCP();
    return true;
};
false {
    YYCP();
    return false;
};
null {
    YYCP();
    return null;
};
\{ {
    return O_BEGIN;
};

\} {
    return O_END;
};

\[ {
    return A_BEGIN;
};

\] {
    return A_END;
};

, {
    return COMMA;
};
: {
    return COLON;
};

[ \t\n]+ /* ignore whitespace */;

. {
    printf("Unexpected: %c\nExiting...\n",*yytext);
    exit(0);
}

%%

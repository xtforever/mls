%{

#define JSONSTYPE char* 
#include "mls.h"
#include "json_parse.tab.h"
#include "json_read.h"
#include <string.h>

#define yylval jsonlval

#define YYCP() do { \
yylval=yytext; \
} while(0)

%}

%option prefix="json"
%option nounput
%option noinput
%option noyywrap
%option yylineno

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
{DBL_QUOTE}{DBL_QUOTE} {
    yylval=""; 
    return  STRING;
}

{DBL_QUOTE}{CHARS}{DBL_QUOTE} {
    yylval=yytext+1;
    yylval[strlen(yylval)-1]=0; 
    return STRING;
}

{NUMBER} {
    YYCP();
    return NUMBER;
}

true {
    YYCP();
    return true;
}

false {
    YYCP();
    return false;
}

null {
    YYCP();
    return null;
}

\{ {
    return O_BEGIN;
}

\} {
    return O_END;
}

\[ {
    return A_BEGIN;
}

\] {
    return A_END;
}

, {
    return COMMA;
}

: {
    return COLON;
}

[ \t]+ /* ignore whitespace */;

[\n] {
    /* yylinecnt++; */
}

. {
    return UNEXPECTED;
}

%%
void set_input_string(const char* in) {
  yy_scan_string(in);
}

void end_lexical_scan(void) {
  yy_delete_buffer(YY_CURRENT_BUFFER);
  yylex_destroy();
}

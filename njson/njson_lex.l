%{
#define NJSONSTYPE char*
#include "mls.h"
#include "njson_parse.tab.h"
#include "njson_read.h"
#include <string.h>

#define yylval njsonlval

#define YYCP() do { \
yylval=yytext; \
} while(0)

%}

%option prefix="njson"
%option noinput
%option stack
%option nounput
%option noinput
%option noyywrap
%option yylineno
%option noyy_top_state


DIGIT1to9 [1-9]
DIGIT [0-9]
DIGITS {DIGIT}+
INT {DIGIT}|{DIGIT1to9}{DIGITS}|-{DIGIT}|-{DIGIT1to9}{DIGITS}
FRAC [.]{DIGITS}
EXP {E}{DIGITS}
E [eE][+-]?
HEX_DIGIT [0-9a-f]
NUMBER {INT}|{INT}{FRAC}|{INT}{EXP}|{INT}{FRAC}{EXP}
UNESCAPEDCHAR [ !#$%&(-\[\]-~]
ESCAPEDCHAR \\['"\\bfnrt/]
UNICODECHAR \\u{HEX_DIGIT}{HEX_DIGIT}{HEX_DIGIT}{HEX_DIGIT}
CHAR {UNESCAPEDCHAR}|{ESCAPEDCHAR}|{UNICODECHAR}
CHARS {CHAR}*
DBL_QUOTE ["]
SINGLE_QUOTE [']
CHAR_OR_DOUBLEQUOTE    {CHAR}|{DBL_QUOTE}
CHAR_OR_SINGLEQUOTE    {CHAR}|{SINGLE_QUOTE}

ALPHA [a-zA-Z]
ALNUM {ALPHA}|{DIGIT}
IDENTCHAR {ALNUM}|[_.'!]|-
IDENT0 {ALPHA}|[_]
IDENT {IDENT0}{IDENTCHAR}*




%x IN_COMMENT
%%
<INITIAL>{
"(*"              yy_push_state(IN_COMMENT); TRACE(1,"comment start");
}
<IN_COMMENT>{
"(*"              yy_push_state(IN_COMMENT); TRACE(1,"comment start");
"*)"      yy_pop_state(); TRACE(1,"comment end");
\n       /* yylinecnt++; */
.
}

{DBL_QUOTE}{DBL_QUOTE} {
    yylval=""; 
    return  STRING;
}
{SINGLE_QUOTE}{SINGLE_QUOTE} {
    yylval=""; 
    return  STRING;
}

{SINGLE_QUOTE}{CHAR_OR_DOUBLEQUOTE}*{SINGLE_QUOTE} {
    yylval=yytext+1;
    yylval[strlen(yylval)-1]=0;
    TRACE(1,"X-SQ-STRING %s", yylval );
    return STRING;
}

{DBL_QUOTE}{CHAR_OR_SINGLEQUOTE}*{DBL_QUOTE} {
    yylval=yytext+1;
    yylval[strlen(yylval)-1]=0;
    TRACE(1,"DQ-STRING %s", yylval );
    return STRING;
}

{IDENT}: {    
    yylval=yytext;
    yylval[ strlen( yylval)-1 ]=0;
    TRACE(1,"IDENT %s", yylval );
    return IDENT;
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

\( {
    TRACE(1,"OPEN");
    return O_BEGIN;
}

\) {
    TRACE(1,"CLOSE");
    return O_END;
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


{IDENT} {
    yylval=yytext;
    TRACE(1,"STRING %s", yylval );
    return STRING;
}


. {
    TRACE(1,"UNEXPECTED: %c", (int) *yytext );
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

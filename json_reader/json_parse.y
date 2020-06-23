%{
#include <stdio.h>
#include "mls.h"
#include "json_read.h"

 int yylex(void);    
 void yyerror(const char *str);
 
 #define YYSTYPE char*
 char *cur_text ="";
 int linecnt =0;
 int string_width =0;
 int num_strings=0;

%}

%token true false null
%left O_BEGIN O_END A_BEGIN A_END
%left COMMA
%left COLON

%token  NUMBER 
%token  STRING


%%
START: ARRAY {
    // printf("%s",$1);
  }
| OBJECT {
    // printf("%s",$1);
  }
;
OBJECT: O_BEGIN O_END {
    $$ = "{}";
  }
| O_BEGIN { json_object_push(); } MEMBERS O_END {
    // $$ = (char *)malloc(sizeof(char)*(1+strlen($3)+1+1));
    // sprintf($$,"{%s}",$3);
    json_object_pop();
  }
;
MEMBERS: PAIR {
    $$ = $1;
  }
| PAIR COMMA MEMBERS {
    // $$ = (char *)malloc(sizeof(char)*(strlen($1)+1+strlen($3)+1));
    // sprintf($$,"%s,%s",$1,$3);
  }
;
PAIR: STRING COLON VALUE {
    // $$ = (char *)malloc(sizeof(char)*(strlen($1)+1+strlen($3)+1));
    // sprintf($$,"%s:%s",$1,$3);
    json_pair($1,$3);
    TRACE(2, "PAIR %s :%s", $1, $3 );
  }
;
ARRAY: A_BEGIN A_END {
    // $$ = (char *)malloc(sizeof(char)*(2+1));
    // sprintf($$,"[]"); /* empty array */
  }
| A_BEGIN  ELEMENTS A_END {
    // $$ = (char *)malloc(sizeof(char)*(1+strlen($2)+1+1));
    // sprintf($$,"[%s]",$2); /* $2 is array */
 }
;
ELEMENTS: VALUE {
    $$ = $1; /* init array with $1 */
    json_array($1);
  }
| VALUE COMMA ELEMENTS {
    // $$ = (char *)malloc(sizeof(char)*(strlen($1)+1+strlen($3)+1));
    // sprintf($$,"%s,%s",$1,$3); /* pre-append $1 */
    json_array($1);
    TRACE(2,"ARR + %s", $1);
 }
;
VALUE: STRING {$$=yylval;  }
| NUMBER { $$=yylval; }
| OBJECT { $$=$1; }
| ARRAY { $$=$1; }
| true { $$=yylval; }
| false {$$=yylval; }
| null {$$=yylval; }
;
%%

void yyerror(const char *str)
{
  printf("error: %s %d\n",str,linecnt );
}

int yywrap()
{
        return 1;
}

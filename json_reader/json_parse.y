%{
#include <stdio.h>
#include "mls.h"
#include "json_read.h"

    void json_new(char *name, int t);
    void json_close(void);

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
  }
| OBJECT {
  }
;
OBJECT: O_BEGIN O_END {
    $$ = "{}";
    json_new("",5); json_close();
  }
| O_BEGIN {
    TRACE(3,"{");
    json_new("",5);
  } MEMBERS O_END {
      TRACE(3,"}");
      json_close();
  }
;
MEMBERS: PAIR {
    $$ = $1;
  }
| PAIR COMMA MEMBERS {
 }
;
PAIR: STRING COLON VALUE {
    TRACE(3,"NAME:%s", $1);
    json_name($1);
    // json_pair($1,$3);
    // json_pair2($1);
  }
;
ARRAY: A_BEGIN A_END {
  }
| A_BEGIN { TRACE(3,"["); json_new("",4); } ELEMENTS A_END  { TRACE(3,"]"); json_close(); } 
;
ELEMENTS: VALUE {
    $$ = $1; 
  }
| VALUE COMMA ELEMENTS {
    
 }
;
VALUE: STRING {$$=yylval;     json_new($$, 0);  }
| NUMBER { $$=yylval;         json_new($$, 1);  }
| OBJECT { $$=$1;                 }
| ARRAY { $$=$1;                  }
| true { $$=yylval;           json_new("true",  2);  }
| false {$$=yylval;           json_new("false", 2);  }
| null {$$=yylval;            json_new("null",  3);  }
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

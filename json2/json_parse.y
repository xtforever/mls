%{
#include <stdio.h>
#include "mls.h"
#include "json_read.h"
#include "json_lex.lex.h"
#include "json_parse.tab.h"  

  void jsonerror(char *str);
/*  int yylex(void);    
  void yyerror(const char *str);
  extern int yylineno;
*/

%}


%define api.value.type {char*}
%define api.prefix {json}
%token true false null
%left O_BEGIN O_END A_BEGIN A_END
%left COMMA
%left COLON

%token  NUMBER 
%token  STRING
%token  UNEXPECTED

%%
START: ARRAY | OBJECT ;

OBJECT: O_BEGIN O_END { json_new("",5); json_close(); }
| O_BEGIN       { TRACE(3,"{"); json_new("",5); }
  MEMBERS O_END { TRACE(3,"}"); json_close(); }
;

MEMBERS: PAIR | PAIR COMMA MEMBERS 
;

PAIR: STRING COLON VALUE { TRACE(3,"NAME:%s", $1); json_name($1); }
;

ARRAY: A_BEGIN A_END { json_new("",4); json_close(); }
| A_BEGIN            { TRACE(3,"["); json_new("",4); }
  ELEMENTS A_END     { TRACE(3,"]"); json_close();   } 
;

ELEMENTS: VALUE | VALUE COMMA ELEMENTS 
;

VALUE: STRING { json_new(yylval, 0);   }
| NUMBER      { json_new(yylval, 1);   }
| OBJECT      
| ARRAY       
| true        { json_new("true",  2);  }
| false       { json_new("false", 2);  }
| null        { json_new("null",  3);  }
;
%%

void jsonerror(char *str)
{
  printf("ERROR: '%s' in Line:%d\n",str,jsonlineno );
}



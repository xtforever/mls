/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_NJSON_NJSON_PARSE_H_INCLUDED
# define YY_NJSON_NJSON_PARSE_H_INCLUDED
/* Debug traces.  */
#ifndef NJSONDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define NJSONDEBUG 1
#  else
#   define NJSONDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define NJSONDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined NJSONDEBUG */
#if NJSONDEBUG
extern int njsondebug;
#endif

/* Token kinds.  */
#ifndef NJSONTOKENTYPE
# define NJSONTOKENTYPE
  enum njsontokentype
  {
    NJSONEMPTY = -2,
    NJSONEOF = 0,                  /* "end of file"  */
    NJSONerror = 256,              /* error  */
    NJSONUNDEF = 257,              /* "invalid token"  */
    true = 258,                    /* true  */
    false = 259,                   /* false  */
    null = 260,                    /* null  */
    O_BEGIN = 261,                 /* O_BEGIN  */
    O_END = 262,                   /* O_END  */
    A_BEGIN = 263,                 /* A_BEGIN  */
    A_END = 264,                   /* A_END  */
    COMMA = 265,                   /* COMMA  */
    COLON = 266,                   /* COLON  */
    IDENT = 267,                   /* IDENT  */
    NUMBER = 268,                  /* NUMBER  */
    STRING = 269,                  /* STRING  */
    UNEXPECTED = 270               /* UNEXPECTED  */
  };
  typedef enum njsontokentype njsontoken_kind_t;
#endif

/* Value type.  */
#if ! defined NJSONSTYPE && ! defined NJSONSTYPE_IS_DECLARED
typedef int NJSONSTYPE;
# define NJSONSTYPE_IS_TRIVIAL 1
# define NJSONSTYPE_IS_DECLARED 1
#endif


extern NJSONSTYPE njsonlval;


int njsonparse (void);


#endif /* !YY_NJSON_NJSON_PARSE_H_INCLUDED  */

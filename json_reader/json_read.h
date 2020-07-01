#ifndef JSON_READ_H
#define JSON_READ_H

#include <stdio.h>

/*interface*/
int json_from_file( FILE *fp );
int json_from_string( char *s );
void json_free(int opts );

/* internals*/
int json_parse(void);
void json_stack( char *arg );
void json_new(char *value, int t);
void json_close(void);
void json_name(char *name);

enum json_typ {
	       JSON_STRING,
	       JSON_NUM,
	       JSON_BOOL,
	       JSON_NULL,
	       JSON_ARR,
	       JSON_OBJ
};

struct json_st {
    int name;
    int typ;
    int d;
};

#endif


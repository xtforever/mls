#ifndef JSON_READ_H
#define JSON_READ_H

#include <stdio.h>

/* init stack */
int json_init( FILE *fp );


/* push current list, create new list */
void json_object_push();

/* put current list to A, pop current list from stack */
void json_object_pop();

/* pair arg with A (if A not empty) 
   put pair in list, reset A to empty
*/
void json_pair( char *arg, char *val );

/* put array to A, prepend with arg */
void json_array( char *arg );


#endif


#include <search.h>
#include <time.h>
#include "mls.h"

static int DAT;

static int
compare(const void *pa, const void *pb)
{
    intptr_t ia = (intptr_t) pa;
    intptr_t ib = (intptr_t) pb;

    int a = INT(DAT, ia );
    int b = INT(DAT, ib );
    return a-b;
}

static void
action(const void *nodep, VISIT which, int depth)
{
    intptr_t ia = *(intptr_t*) nodep;
    int data = INT(DAT, ia );
    
    switch (which) {
    case preorder:
	break;
    case postorder:
	printf("%6d\n", data);
	break;
    case endorder:
	break;
    case leaf:
	printf("%6d\n", data);
	break;
    }
}

void dummy_free(void *a)
{
}


int main(int argc, char **argv)
{
    m_init();
    trace_level=1;
    DAT = m_create(12,sizeof(int));
    
    intptr_t *val;
    void *root = NULL;
	
    srand(time(NULL));
    for (int i = 0; i < 12; i++) {
	int rnd = rand() % 10;
	intptr_t data = m_put(DAT, &rnd ); 	
	val = tsearch( (intptr_t*)data, &root, compare);
	if (val == NULL)
	    exit(EXIT_FAILURE);

    }
    twalk(root, action);
    tdestroy(root, dummy_free);
    m_free(DAT);
    m_destruct();
    exit(EXIT_SUCCESS);
}

#include "mls.h"
#include <search.h>
#define TRACE_MT 2

#define TF() TRACE(TRACE_MT,"")

typedef int (*compare_fn_t) (const void *a, const void *b);
struct mt_compare {
    char *name;
    compare_fn_t func;
    void *user_data;
};


typedef void (*free_fn_t) ( int func_num, int mt, int pos );
struct mt_free {
    char *name;
    free_fn_t func;
    void *user_data;
};

struct mt_api {
    char *name;
    
    void*  (*get)( int m, int i );
    int    (*create)(int m, int max, int w);
    int    (*free)(int h);
    int    (*put)( int m, const void* data );
    int    (*insert)(int m, int p, int n);
    void   (*remove)( int m, int p, int n );
    int    (*width)( int m);
    int    (*len)( int m );
    void   (*sort)( int list, int(*compar)(const void *, const void *));
    int    (*find)(const void *key, int list, int (*c)(const void *, const void *));
    int    (*lookup)( int buf, const void *data, int cmpnum );
    void *class_data;
};

struct mt_impl {
    int api;
    int default_cmp;
    int default_free;
    int type, class;
    int user_data;
    void *user_mem;
};

static struct mt_info {
    int MT_IMPL, MT_API, MT_COMPARE, MT_FREE;
    
} MT_INFO;

static int MT_IMPL, MT_API, MT_COMPARE, MT_FREE;

// compare function implementation
int cmp_int( const void *a,  const void *b )
{
    TF();
    return (*(const int *)a) - (*(const int *)b); 
}


// array implementation
void* array_get(  int m, int i )
{
    TF();
    struct mt_impl *mt = mls(MT_IMPL,m);
    return mls( mt->user_data, i );
}
int array_put(  int m, const void *d )
{    
    struct mt_impl *mt = mls(MT_IMPL,m);
    return m_put( mt->user_data, d );
}

int arry_put_sorted( int m, const void *d, int cmpnum )
{
     struct mt_impl *mt = mls(MT_IMPL,m);
     struct mt_compare *cmp = mls(MT_COMPARE, cmpnum);
     int ma = mt->user_data;
      
     m_binsert( ma, d, cmp );
}

int  arry_put_sorted_def( int m, const void *d )
{
    struct mt_impl *mt = mls(MT_IMPL,m);
    arry_put_sorted( m, d, mt->default_cmp );
}

int  array_create(  int m, int l, int w )
{
    struct mt_impl *mt = mls(MT_IMPL,m);
    return mt->user_data = m_create(l,w);
}

int array_lookup( int m, const void *data, int cmpnum )
{
    struct mt_impl *mt = mls(MT_IMPL,m);
    int ma = mt->user_data;
    struct mt_compare *cmp = mls(MT_COMPARE, cmpnum);
    

    int p; void *d;

    m_foreach(ma,p,d) {
	if( cmp->func( data, d ) == 0 ) return p;
    }

    p = m_new(ma,1);
    TRACE(TRACE_MT, "insert value at %d", p );
    memcpy( mls(ma,p), data, m_width(ma) );
    return p;    
}

int array_sort( int m, int cmpnum )
{
    struct mt_impl *mt = mls(MT_IMPL,m);
    struct mt_compare *cmp = mls(MT_COMPARE, cmpnum);
    int ma = mt->user_data;
    m_qsort(ma, cmp );
}



// END OF ARRAY IMPL ------------------------------------

/*

int node = [ elem1, ... elemN ]

first element in a node is representant for this node

btree_cmp(a,b)
node1 = get(node_list,a)
elem1 = get(node1, 0)
...

 */


/* data is stored as array, btree is used as index to array and stores only array index*/
int btree_lookup( int m, const void *data, int cmpnum )
{
    struct mt_impl *mt = mls(MT_IMPL,m);
    int ma = mt->user_data;
    struct mt_compare *cmp = mls(MT_COMPARE, cmpnum);
    intptr_t *val;
    intptr_t  data = m_put(ma, data);
    
    void *val = tsearch(data, &mt->user_mem, cmp);
    
}

int  btree_create(  int m, int l, int w )
{
    
}

// put a copy of d into array
int btree_put(  int m, const void *d )
{
    struct mt_impl *mt = mls(MT_IMPL,m);
    int num = m_put(mt->user_data, d);
    
}

void* btree_get(  int m, int i )
{
  
}



// wrapper implementation 
void mt_freedummy(int func_num, int mtnum, int p)
{
    struct mt_impl *mt = mls(MT_IMPL,mtnum);
    struct mt_api  *api = mls(MT_API, mt->api);
    struct mt_free *freefn = mls(MT_FREE, mt->func_num);
    // remove element p from memory
    // 
}    


void* mt_get( int m, int i )
{
    TF();
    struct mt_impl *mt = mls(MT_IMPL,m);
    struct mt_api  *api = mls(MT_API, mt->api);
    return api->get(m,i);
}

int  mt_put( int m, const void *d )
{
    struct mt_impl *mt  = mls(MT_IMPL,m);
    struct mt_api  *api = mls(MT_API, mt->api);
    return api->put(m,d);
}

int  mt_create( char *name, int len, int width, char *cmpname )
{
    int apinum = m_lookup_str(MT_API,name,1);
    ASERR( apinum >=0, "could not find api %s", name );
    int cmpnum = m_lookup_str(MT_COMPARE, cmpname,1);

    int mtnum = m_new(MT_IMPL,1);
    struct mt_impl *mt = mls(MT_IMPL,mtnum);
    mt->api = apinum;
    mt->default_cmp = cmpnum;
    mt->default_free = 0;
    
    // actual create data structure 
    struct mt_api *api = mls(MT_API,apinum);
    int e = api->create(mtnum, len,width);

    if( e < 0 ) {
	ERR("could not create %s", name );
    }

    return mtnum;    
}

int mt_lookup_def( int m, void *obj )
{
    TF();
    struct mt_impl *mt = mls(MT_IMPL,m);
    struct mt_api  *api = mls(MT_API, mt->api);
    return api->lookup(m,obj,mt->default_cmp);
}

void mt_init(void)
{
    MT_IMPL = m_create( 10, sizeof( struct mt_impl ));
    MT_API  = m_create( 10, sizeof( struct mt_api ));
    MT_COMPARE   = m_create( 10, sizeof(struct mt_compare ));
    MT_FREE   = m_create( 10, sizeof(struct mt_free ));
    
    struct mt_api *api = m_add(  MT_API  );
    api->get = array_get;
    api->create = array_create;
    api->lookup = array_lookup;
    api->put = array_put_sorted_def;
    api->name = "SORTEDARRAY";

    struct mt_api *api = m_add(  MT_API  );
    api->get = array_get;
    api->create = array_create;
    api->lookup = array_lookup;
    api->put = array_put;
    api->name = "ARRAY";

    struct mt_api *api = m_add(  MT_API  );
    api->get = btree_get;
    api->create = btree_create;
    api->lookup = btree_lookup;
    api->put = btree_put;
    api->name = "BTREE";

    struct mt_compare *cmp = m_add( MT_COMPARE );
    cmp->name = "CMP_INT";
    cmp->func = cmp_int;

    struct mt_compare *freefn = m_add( MT_FREE );
    freefn->name = "FREEDUMMY";
    free->func = mt_freedummy;
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
	// printf("%6d\n", data);
	break;
    case endorder:
	break;
    case leaf:
	printf("%6d\n", data);
	break;
    }
}



void insert_and_get(int ctx)
{

    for(int i=0; i<10; i++ )
	{
	    mt_put( ctx, & i );
	}
    
    
    for(int i=0; i<10; i++ )
	{
	    int data = *(int*) mt_get( ctx, i );
	    printf("At Index %d got Value %d\n", i, data );
	}
}

void test_named_dataset(char *name)
{
    int ctx1 = mt_create( name, 10, sizeof(int), "CMP_INT" );
    insert_and_get(ctx);
    mt_free(ctx1);
}

void dummy_free(void *a)
{
}

int  main(int argc, char ** argv)
{
    m_init();
    trace_level=2;

    mt_init();

    test_named_dataset("ARRAY");
    test_named_dataset("SORTEDARRAY");
    test_named_dataset("BTREE");
    

#if 0
    int data=100;
    int f1 = mt_lookup_def( ctx, &data );
    printf("At Index %d got Value %d\n", f1, data );

    int f2 = mt_lookup_def( ctx, &data );
    printf("At Index %d got Value %d\n", f2, data );
#endif
    

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
    
    m_destruct();
}

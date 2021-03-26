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
    void   (*destroy)(int h);
    int    (*put)( int m, const void* data );
    int    (*insert)(int m, int p, int n);
    void   (*remove)( int m, int p, int n );
    int    (*width)( int m);
    int    (*len)( int m );
    void   (*sort)( int list, int(*compar)(const void *, const void *));
    int    (*find)(const void *key, int list, int (*c)(const void *, const void *));
    int    (*lookup)( int buf, const void *data, int cmpnum );
    int    (*next)( int m, int *p, void *d );

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
    int m_ctx, m_api, m_cmp, m_free;
    
} MT_INFO;



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
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return mls( mt->user_data, i );
}

// array destroy implementation
void array_destroy(  int m )
{
    TF();
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    mt->api=-1;
    m_free( mt->user_data );
}


int array_put(  int m, const void *d )
{    
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return m_put( mt->user_data, d );
}

int arry_put_sorted( int m, const void *d, int cmpnum )
{
     struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
     struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
     int ma = mt->user_data;
      
     return m_binsert( ma, d, cmp->func );
}

int  array_put_sorted_def( int m, const void *d )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return arry_put_sorted( m, d, mt->default_cmp );
}

int  array_create(  int m, int l, int w )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return mt->user_data = m_create(l,w);
}

int array_lookup( int m, const void *data, int cmpnum )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    int ma = mt->user_data;
    struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
    

    int p; void *d;

    m_foreach(ma,p,d) {
	if( cmp->func( data, d ) == 0 ) return p;
    }

    p = m_new(ma,1);
    TRACE(TRACE_MT, "insert value at %d", p );
    memcpy( mls(ma,p), data, m_width(ma) );
    return p;    
}

void array_sort( int m, int cmpnum )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
    int ma = mt->user_data;
    m_qsort(ma, cmp->func );
}

int array_sorted_lookup( int m, const void *data, int cmpnum )
{
  struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
  int ma = mt->user_data;
  struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
  
  return m_bsearch( data, ma, cmp->func );
}




// END OF ARRAY IMPL ------------------------------------

// hashmap implementation
void* hashmap_get(  int m, int i )
{
    TF();
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return mls( mt->user_data, i );
}
int hashmap_put(  int m, const void *d )
{    
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return m_put( mt->user_data, d );
}

int  hashmap_put_sorted( int m, const void *d, int cmpnum )
{
     struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
     struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
     int ma = mt->user_data;
      
     return m_binsert( ma, d, cmp->func );
}

int  hashmap_put_sorted_def( int m, const void *d )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return arry_put_sorted( m, d, mt->default_cmp );
}

int  hashmap_create(  int m, int l, int w )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    return mt->user_data = m_create(l,w);
}

int hashmap_lookup( int m, const void *data, int cmpnum )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    int ma = mt->user_data;
    struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
    

    int p; void *d;

    m_foreach(ma,p,d) {
	if( cmp->func( data, d ) == 0 ) return p;
    }

    p = m_new(ma,1);
    TRACE(TRACE_MT, "insert value at %d", p );
    memcpy( mls(ma,p), data, m_width(ma) );
    return p;    
}


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
  #if 0
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    int ma = mt->user_data;
    struct mt_compare *cmp = mls(MT_INFO.m_cmp, cmpnum);
    intptr_t *val;
    intptr_t  data = m_put(ma, data);
    void *val = tsearch(data, &mt->user_mem, cmp);
#endif
    return 0;
}

int  btree_create(  int m, int l, int w )
{
  return 0;
}

// put a copy of d into array
int btree_put(  int m, const void *d )
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    int num = m_put(mt->user_data, d);
    return 0;
}

void* btree_get(  int m, int i )
{
  return NULL;
}



// wrapper implementation 
void mt_destroy_dummy(int func_num, int mtnum, int p)
{
    struct mt_impl *mt = mls(MT_INFO.m_ctx,mtnum);
    struct mt_api  *api = mls(MT_INFO.m_api, mt->api);
    struct mt_free *freefn = mls(MT_INFO.m_free, func_num);
    api->destroy(mtnum);
    // 
    // remove element p from memory
    //
    // the context is:
    //   data is stored in (mt)
    //   used api is       (api)
    //   add.info is       (freefn->user_data)
    //
}    

// -----------------------------------------------

void mt_destroy(int m)
{
    TF();
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    struct mt_api  *api = mls(MT_INFO.m_api, mt->api); 
    int free_num = mt->default_free;
    struct mt_free *freefn = mls(MT_INFO.m_free, free_num);
    freefn->func(free_num,m, 0);
}


void* mt_get( int m, int i )
{
    TF();
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    struct mt_api  *api = mls(MT_INFO.m_api, mt->api);
    return api->get(m,i);
}

int  mt_put( int m, const void *d )
{
    struct mt_impl *mt  = mls(MT_INFO.m_ctx,m);
    struct mt_api  *api = mls(MT_INFO.m_api, mt->api);
    return api->put(m,d);
}

int  mt_create( char *name, int len, int width, char *cmpname )
{
    int apinum = m_lookup_str(MT_INFO.m_api,name,1);
    ASERR( apinum >=0, "could not find api %s", name );
    int cmpnum = m_lookup_str(MT_INFO.m_cmp, cmpname,1);

    int mtnum = m_new(MT_INFO.m_ctx,1);
    struct mt_impl *mt = mls(MT_INFO.m_ctx,mtnum);
    mt->api = apinum;
    mt->default_cmp = cmpnum;
    mt->default_free = 0;
    
    // actual create data structure 
    struct mt_api *api = mls(MT_INFO.m_api,apinum);
    int e = api->create(mtnum, len,width);

    if( e < 0 ) {
	ERR("could not create %s", name );
    }

    return mtnum;    
}

int mt_lookup_def( int m, void *obj )
{
    TF();
    struct mt_impl *mt = mls(MT_INFO.m_ctx,m);
    struct mt_api  *api = mls(MT_INFO.m_api, mt->api);
    return api->lookup(m,obj,mt->default_cmp);
}

void mt_init(void)
{
    MT_INFO.m_ctx = m_create( 10, sizeof( struct mt_impl ));
    MT_INFO.m_api  = m_create( 10, sizeof( struct mt_api ));
    MT_INFO.m_cmp   = m_create( 10, sizeof(struct mt_compare ));
    MT_INFO.m_free   = m_create( 10, sizeof(struct mt_free ));
    
    struct mt_api *api;

    api = m_add(  MT_INFO.m_api  );
    api->get = array_get;
    api->create = array_create;
    api->lookup = array_sorted_lookup;
    api->put = array_put_sorted_def;
    api->destroy = array_destroy;
    api->name = "SORTEDARRAY";

    api = m_add(  MT_INFO.m_api  );
    api->get = array_get;
    api->create = array_create;
    api->lookup = array_lookup;
    api->put = array_put;
    api->destroy = array_destroy;
    api->name = "ARRAY";

    api = m_add(  MT_INFO.m_api  );
    api->get = btree_get;
    api->create = btree_create;
    
    api->lookup = btree_lookup;
    api->put = btree_put;
    api->name = "BTREE";

    struct mt_compare *cmp = m_add( MT_INFO.m_cmp );
    cmp->name = "CMP_INT";
    cmp->func = cmp_int;

    struct mt_free *freefn = m_add( MT_INFO.m_free );
    freefn->name = "DESTROYDUMMY";
    freefn->func = mt_destroy_dummy;
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


void test_lookup(int ctx)
{
  for(int i=0;i < 10; i++ ) {
    printf("search for %d\n", i);
    mt_lookup_def( ctx, &i );
  }
    
}


void test_named_dataset(char *name)
{
    int ctx1 = mt_create( name, 10, sizeof(int), "CMP_INT" );
    insert_and_get(ctx1);
    test_lookup(ctx1);
    mt_destroy(ctx1);
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
    // test_named_dataset("BTREE");


    

#if 0
    int data=100;
    int f1 = mt_lookup_def( ctx, &data );
    printf("At Index %d got Value %d\n", f1, data );

    int f2 = mt_lookup_def( ctx, &data );
    printf("At Index %d got Value %d\n", f2, data );
#endif
    
    m_destruct();
}

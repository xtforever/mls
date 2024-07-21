
#include <unistd.h>
#include "mls.h"
#include <stdlib.h>


#define CMD_MAX 16
#define POPULATION_START 200
/* one out of RADIATION_LEVEL gets mutated */
#define RADIATION_LEVEL 10
#define GEN_SIZE 80
#define MAX_ITEMS 3
#define POPULATION_MAX 1000

typedef struct {
    int width,height;
    int data;
} world_t;


typedef struct {

    int age,fitness,steps,items;
    int dir,posx,posy;
   
    int gen;
    int pc;
    world_t *w;    
} virus_t;



static void show_simulation( int p );
    
static int xrand( int max )
{
    int k = (rand() * 1.0 / RAND_MAX) * max;
    if( k >= max ) k = max-1;
    return k;
}

static void mutate( virus_t *v )
{
    int pos = xrand( m_len(v->gen) );

    // special case goto 8..15
    int cmd = xrand( 9 );
    if( cmd == 8 ) {
	cmd += xrand( 8 );
    }
    
    INT( v->gen, pos ) = cmd;
}


static void copy_gen(  virus_t *dest, int pos, virus_t *src, int from, int len  )
{
    if( from + len > m_len(src->gen) ) {
	WARN("something bad happend");
	return;
    }
    
    m_write( dest->gen, pos, mls(src->gen, from), len );    
}

static void crossover( virus_t *dest, virus_t *src )
{
    int r = xrand(4);
    int max =  m_len( src->gen);
    int pos = xrand( max / 2 );
    
    switch( r ) {
    case 0: // leave a
	TRACE(3, "keep a");
	return;
    case 1: // copy from b
	TRACE(3, "keep b");
	copy_gen( dest, 0, src, 0, max );
	return;
    case 2:
    case 3: // leave only 50% of a at a random point
	TRACE(3, "combine at %d", pos );
	copy_gen( dest, pos, src, pos, max / 2 );
	return;
    }
    
    return;
}


// number from 0 - 1000
// score:      steps 0..100  == fitness
//             items 1       == fitness + 300
static void fitness( virus_t *v )
{
    int fitness = v->items * 300;
    fitness += v->steps;
    fitness = Min( fitness, 1000 );
    v->fitness = fitness;    
}

static void print_world( world_t *w )
{

    int n=0;
    int ch;
    
    for( int y = 0; y < w->height; y++ )
	{
	    for( int x = 0; x < w->width; x++ ) {
		ch = INT(w->data,n); n++;
		if( ch == 0 ) ch='.';
		printf( "%c", ch );
	    }
	    putchar(10);
	}
}

static void init_world1( world_t *w )
{
    w->height = 15;
    w->width  = 45;
    int c=w->width * w->height;
    w->data = m_create( c, sizeof(int));   
    m_setlen( w->data, c ); 

    for(int i=0;i< MAX_ITEMS;i++) {
	int p = xrand(c);
	INT( w->data, p ) = '*';
    }
}


static void step_dir(int dir, int *x, int *y )
{
    switch( dir & 0x07 ) {
    case 0:
	(*y)--;
	break;
    case 1:
	(*y)--;
	(*x)++;
	break;
    case 2:
	(*x)++;
	break;
    case 3:
	(*y)++;
	(*x)++;
	break;
    case 4:
	(*y)++;
	break;
    case 5:
	(*y)++;
	(*x)--;
	break;
    case 6:
	(*x)--;
	break;
    case 7:
	(*y)--;
	(*x)--;
	break;
    }
}


int char_lookup( world_t *w, int x, int y)
{
    int n = y * w->width + x;
    if( n >= m_len(w->data) ) return 0;
    return INT(w->data,n);
}
int char_set( world_t *w, int x, int y, int ch )
{
    int n = y * w->width + x;
    if( n >= m_len(w->data) ) return 0;
    return INT(w->data,n) = ch;
}



int world_search( int dir, int len, int x, int y, world_t *w )
{

    for( int i = 0; i < len; i++ ) {

	step_dir( dir, &x, &y );
	if( x < 0 ) x=0;
	if( y < 0 ) y=0;
	if( x >= w->width )   x=w->width-1;
	if( y >= w->height )  y=w->height-1;
	if( char_lookup( w,x,y)  == '*' ) return 1;
    }
    return 0;
}


static void exec_cmd( virus_t *v )
{
    if( v->pc >= m_len(v->gen) || v->pc < 0 ) v->pc=0; // reset


    int cmd = INT( v->gen, v->pc );
    TRACE(3, "GEN %d: %d", v->pc, cmd );
    v->pc++;

    if( cmd >= 8 ) {
	int c = (cmd & 0x07) -5;
	if( c>=-1 ) c++;

	TRACE(3, "goto %d", c );

	
	v->pc += c;	
    }
    else {
	int c;
	
	switch( cmd & 0x0f ) {
	case 0: v->pc = 0;
	case 1: break;
	case 2: break;
	case 3:     step_dir( v->dir, &v->posx, &v->posy ); 	TRACE(3, "forw " ); v->steps++; break;
	case 4:     step_dir( v->dir +4, &v->posx, &v->posy );  TRACE(3, "backw" ); v->steps++; break;
	case 5: v->dir++;  TRACE(3, "cw" ); break;
	case 6: v->dir--;  TRACE(3, "ccw" );  break;
	case 7:
	    c = world_search(v->dir, 4, v->posx,v->posy, v->w  );
	    if( !c ) v->pc++;
    	    TRACE(3, "search %d", c );
	    break;
	}

	v->dir &= 0x07;
	if( v->posx < 0 ) v->posx=0;
	if( v->posy < 0 ) v->posy=0;
	if( v->posx >= v->w->width )   v->posx=v->w->width-1;
	if( v->posy >= v->w->height )  v->posy=v->w->height-1;

	if( char_lookup(v->w, v->posx, v->posy) == '*' ) {
	    v->items++;
	    char_set( v->w, v->posx, v->posy, 0 );
	}
	
    }
}



static void dump_cmd( int cmd, int pc )
{
    if( cmd >= 8 ) {
	int c = (cmd & 0x07) -4;
	if( c>=0 ) c++;
	printf( "goto %d", pc+c );
	return;
    }
    
    switch( cmd & 0x0f ) {
    case 0:
    case 1:
    case 2:
	printf("NOP");
	return;
	
    case 3:     printf( "forw" ); return;
    case 4:     printf( "backw" ); return;
    case 5:     printf( "cw" ); return;
    case 6:     printf( "ccw" ); return;
    case 7:
	printf( "search4" ); return;
    }
    
    return;
}


static void disassemble( virus_t *v )
{
    int old = -1, last_pc = 0, c=0;
    for( int pc = 0; pc < m_len(v->gen) ; pc++ )
	{
	    int cmd = INT(v->gen,pc);
	    if( old == cmd ) continue;
	    if( pc - last_pc > 1 ) c='*'; else c=32;	    
	    printf("%c%.3d [ %02x ]  : ", c, pc, cmd );
	    dump_cmd( cmd, pc );
	    printf("\n");
	    last_pc = pc;
	    old = cmd;
	}
}



static void init_virus( virus_t *v, world_t * w )
{
    virus_t v1 = { 0 };


    v1.gen = m_create(GEN_SIZE,sizeof(int) );
    m_setlen( v1.gen, GEN_SIZE );
    v1.w = w;
    
    m_puti( v1.gen, 5 );
    m_puti( v1.gen, 5 );
    m_puti( v1.gen, 5 );
    m_puti( v1.gen, 3 );
    m_puti( v1.gen, 7 );
    m_puti( v1.gen, 15  );
    m_puti( v1.gen, 8 );

    *v = v1;
    
}

static void dump_virus(virus_t *v)
{
    printf("%d (%d,%d) dir=%d steps=%d age=%d\n",v->gen, v->posx, v->posy, v->dir, v->steps, v->age  );
}





static void map_virus( world_t *w, virus_t *v )
{
    int x   = v->posx;
    int y   = v->posy;
    int ch  = '0' + v->dir;

    int n = y * w->width + x;
    if( n >= m_len(w->data) ) return;

    INT(w->data,n ) = ch;
    
}


static void copy_world( world_t *target, world_t *source )
{    
    int r = target->data;
    int m = source->data;
    m_write( r,0, mls(m,0), m_len(m) );
    *target = *source;
    target->data = r;
}

static void     destruct_virus(virus_t *v)
{
    m_free(v->gen); v->gen=0;
}
static void     destruct_world(world_t *w)
{
    m_free(w->data); w->data=0;
}


int VIRUS;
virus_t *virus_get( int pos )
{
    return mls(VIRUS,pos);
}

virus_t *virus_clone( int pos )
{
    virus_t *v1 = m_add(VIRUS);
    virus_t *v = virus_get(pos);
    v1->gen = m_dub(v->gen);
    v1->w   = v->w;
    return v1;
}

static void mutate_population(void )
{
    virus_t *d;
    int p;
    m_foreach( VIRUS,p,d ) {
	if( xrand(RADIATION_LEVEL) == 1 ) mutate(d);
    }
}


static int winner=-1;
static void run_virus( int p, int c )
{
    TRACE(4,"running %d", p);
    virus_t *v = virus_get(p);
    for(int i=0;i<c; i++) {   
	exec_cmd( v );

	if( v->items >= MAX_ITEMS ) {
	    TRACE(5,"perfect: %d", p );
	    if( winner != p ) {
		show_simulation(p);
		winner=p;
	    }
	    break;
	}
    }
}

static void run_each_virus(int c )
{
    // save current world
    virus_t *v = virus_get(0);
    world_t bak, *current = v->w;
    init_world1(&bak);
    copy_world(&bak, current);
	
    for(int i=0;i< m_len(VIRUS); i++) {
	run_virus(i,c);
	copy_world(current,&bak );
    }
}


static void fitness_each_virus(void)
{
    virus_t *v; int p;
    m_foreach(VIRUS,p,v) {
	fitness(v);
	if( v->items > 1 ) TRACE(5,"ITEMS FOUND: %d", v->items );
	v->posx=0; v->posy=0; v->age++;
	v->items=0;
	v->pc=0;
    }
}

static int cmp_fit( const void *a, const void *b )
{
    const virus_t *va=a,*vb=b;
    return vb->fitness - va->fitness;
}

static void sort_by_fitness(void)
{
    m_qsort( VIRUS, cmp_fit );
    TRACE(5,"%d", virus_get(0)->fitness );
}

static void overcrowded(void)
{
    virus_t *v; int p;
    p=m_len(VIRUS);
    while( p-- > POPULATION_MAX ) {
	v = virus_get(p);
	m_free(v->gen);
	m_del(VIRUS,p);
    }
}

static void random_killer(void)
{
    virus_t *v; int p;
    p=m_len(VIRUS);
    while( p-- ) {
	v = virus_get(p);
	int r = xrand( (v->fitness+1)*10 );
	TRACE(4,"%d %d", v->fitness, r );
	if( r < 7 ) {
	    TRACE(4,"kill %d", p );
	    m_free(v->gen);
	    m_del(VIRUS,p);
	}
    } 
}

static void make_child( int a, int b )
{
    int r = xrand(10)+1;

    if( r > 4 ) r=1;  
    
    while( r-- ) {	
	TRACE(4,"%d child from %d %d",r, a, b );
    

	virus_t *c  = virus_clone(a);
	virus_t *m  = virus_get(b);
	crossover( c, m );
    }
    
}


static void new_population( void )
{
    int max = m_len(VIRUS);
    int mother;
    int num=0;
    
    // find pair 
    for( int i = 0; i < max; i++ ) {
	virus_t *v = virus_get(i);
	int fortune =  v->fitness ;
	int r = xrand( 1201 );
	TRACE(4,"%d %d %d", v->fitness, fortune, r );


	
	if( r <= fortune || ( v->fitness > 100 && xrand(100) < 80)) {
	    TRACE(4,"found: %d", i);
	    num++; 
	    if( num == 2 ) { make_child(mother, i ); num=0; }
	    else mother=i;	    
	}	
    }
}

#define home()  printf("\033[%d;%df", 0,0 )
#define clear()     printf("\033[2J")

static void show_simulation( int p )
{
    int ch;
    virus_t *v = virus_get(p);
    world_t bak,*cur = v->w;
    init_world1(&bak);
    copy_world( &bak, cur );

    printf("\033[2J");

    int x,y;
    unsigned i=0, tm=0;
    v->items=0;
    while(1) {
	if( i++ > 1000000 ) break;
	if( v->items >= MAX_ITEMS ) break;
	x=v->posx;
	y=v->posy;
	exec_cmd(v);	
	if( v->posx==x && v->posy == y ) continue;

	printf("\033[%d;%df", 1,1 );
	ch = char_lookup(v->w, v->posx, v->posy);
	char_set( v->w, v->posx, v->posy, '$' );
	print_world(cur);
	printf("%d\n", i);
	usleep(1000 *  10 );
	char_set( v->w, v->posx, v->posy, ch );
	tm++;
	if( tm > 100 * 60 ) break;
    }
    copy_world( cur, &bak );
}




int main(int argc, char **argv )
{
    int p;
    world_t w1,*w;
    virus_t *v;
    
    m_init();
    trace_level=5;
    w=&w1;
    init_world1(w);

    clear();
    VIRUS = m_create(POPULATION_START,sizeof(virus_t) );
    for( int i =0; i< POPULATION_START; i++  )
	{
	    virus_t vv;
	    init_virus( &vv, w );
	    m_put( VIRUS, &vv );
	}

       
    print_world(w);


    for(int i=0; i< 10000; i++ )
	{
	    home(); printf("%5d", i );
	    mutate_population();
	}



    

    /* test crossover 
    
    virus_t *b  = virus_get(0);
    for(int i=0; i< m_len(b->gen);i++)
	INT(b->gen,i)=3;
    b  = virus_get(1);
    for(int i=0; i< m_len(b->gen);i++)
	INT(b->gen,i)=4;    
    printf("MOTHER----------------------------------------\n");
    disassemble( b );

    printf("CHILD----------------------------------------\n");
    virus_t *c  = virus_clone(0);
    crossover( c, b );
    disassemble( c );
    return;

    */
    
    world_t view;
    init_world1( &view );


    clear();
    for(int i =0; i < 1000; i++ ) {
	home(); printf("%d %d\n", i, m_len(VIRUS) );
	run_each_virus( GEN_SIZE * 100 );
	fitness_each_virus( );
	sort_by_fitness();
	random_killer();
	overcrowded();	
	mutate_population();
	new_population();


    }

    m_foreach(VIRUS,p,v) {
	printf("%d: fit=%d\n", p, v->fitness);
    }

    
    m_foreach(VIRUS,p,v) {
	printf("%d %p:   ", p, virus_get(p) );
	dump_virus(v);
    }
    run_each_virus(1);
    
    exit(1);
    
    m_foreach(VIRUS,p,v) {
	printf("%d: fit=%d\n", p, v->fitness);
    }

    random_killer();

    m_foreach(VIRUS,p,v) {
	printf("%d: fit=%d\n", p, v->fitness);
    }

    new_population();

    m_foreach(VIRUS,p,v) {
	printf("%d: fit=%d\n", p, v->fitness);
    }


    
    m_foreach(VIRUS,p,v) {
	destruct_virus(v);	
    }
    m_free(VIRUS);
    
    destruct_world(w);
    destruct_world(&view);
    
    m_destruct();
}

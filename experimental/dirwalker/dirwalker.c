#include "mls.h"
#include "m_tool.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#define DW 8

struct dirnode {
	int 
	name,    //  char[]
	subs,    //  int [] , referencing indices into dirnodelist
	parent;  // index of parent node (faster path lookup)
};

int dirnodelist = 0;     // list of struct dirnode
int bm_dirnodeleaf = 0;  // bitmap for all leafs in dirnodelist (faster search)

void bit_set(int bm, int index)
{
	int word = index / 64;
	int bit  = index % 64;
	if (word >= m_len(bm))
		m_setlen(bm, word + 1);
	uint64_t *w = mls(bm, word);
	*w |= (1ULL << bit);
}

int bit_get(int bm, int index)
{
	int word = index / 64;
	int bit  = index % 64;
	if (word >= m_len(bm))
		return 0;
	uint64_t *w = mls(bm, word);
	return (*w >> bit) & 1ULL;
}

struct dirnode *dirnode_get(int n)
{
	return mls(dirnodelist,n);
}

int dirnode_new(int parent, const char *dir) 
{
  struct dirnode *d = m_add(dirnodelist);
  d->parent = parent;
  d->name = s_printf(0,0, "%s", dir );
  return m_len(dirnodelist)-1;
}

void dirnodelist_free(int m)
{
	struct dirnode *dn; int p;
	m_foreach(m,p,dn) {
		m_free(dn->name);
		m_free(dn->subs);
		TRACE(DW , "free dirnode %d", p );
	}
}

void  dirwalker_init(void)
{	
	dirnodelist = m_alloc(100, sizeof(struct dirnode), m_reg_freefn(dirnodelist_free) ); 
        // m_alloc works like calloc, but returns handle to array
	bm_dirnodeleaf = m_alloc(100, sizeof(uint64_t), MFREE );
}

void  dirwalker_free(void)
{
	m_free(dirnodelist);
	m_free(bm_dirnodeleaf);
}


/*
	dirtree:
		dirnodelist[0] == root
		dirnodelist[1] == first entry in root dir 
		dirnodelist[0].subs[*] == indices of dirnode structs for subdir


0:
	name: mls	
	subs: [1 2 3]
	parent: -1

1:
	name: lib	
	subs: [4 5 6 7 8 9 10] 
	parent: 0

4:
	name: mls.c	
	subs:  0
	parent: 1

	

	
	
*/

void read_dir(int parent, int cur, int mpath)
{
  const char *path = m_str(mpath);
  DIR *dir;
  struct dirent *entry;
  struct stat info;
  struct dirnode *curp = dirnode_get(cur);
  // Open the directory
  if (!(dir = opendir(path))) {
    perror("opendir");
    return;
  }
  int fullPath = 0;
  while ((entry = readdir(dir)) != NULL) {


    // Skip "." and ".." entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    // Build the full path
    fullPath = s_printf(fullPath,0, "%s/%s", path, entry->d_name);
    
    // Get file info
    if (stat( m_str(fullPath), &info) != 0) {
	    ERR("stat( %s ): %s\n", m_str(fullPath), strerror(errno));
      continue;
    }

    if (S_ISDIR(info.st_mode)) {
      printf("[DIR]  %s\n", entry->d_name);
      int node = dirnode_new(parent, entry->d_name);
      if( curp->subs ==0 ) curp->subs = m_create(10,sizeof(int));
      m_put(curp->subs, &node);      
      read_dir(node, node, fullPath);  // Recursive call for subdirectory
    } else {
      printf("       %s\n", entry->d_name);
      int node = dirnode_new(parent, entry->d_name);
      if( curp->subs ==0 ) curp->subs = m_create(10,sizeof(int));
      m_put(curp->subs, &node);
      bit_set(bm_dirnodeleaf, node);
    }
  }
  m_free(fullPath);
  closedir(dir);
}



void print_dir(int root)
{
	struct dirnode *dn = dirnode_get(root);
	if( dn->subs > 0 ) {
		printf(" [DIR] %s\n", m_str(dn->name));
		int p,*d;
		m_foreach( dn->subs, p, d ) print_dir(*d);
	} else  printf("[FILE] %s\n", m_str(dn->name));
}

int get_dirname(int index)
{
	return dirnode_get(index)->name;
}

void print_path(int node)
{
	struct dirnode *dn = dirnode_get(node);
	int path = s_printf(0, 0, "%s", m_str(dn->name));
        int p = dn->parent;
                while (p >= 0) {
                        struct dirnode *pd = dirnode_get(p);
                        path = s_printf(path, 0, "%s/%s", m_str(pd->name), m_str(path));
                        p = pd->parent;
                }
                printf("%s\n", m_str(path));
                m_free(path);
}


void print_files_fullpath(void)
{
	struct dirnode *dn; int i;
	m_foreach(dirnodelist, i, dn) {
		if (!bit_get(bm_dirnodeleaf, i)) continue;
		print_path(i);
	}
}

int sorted_index = 0;

int dirname_compare (const void *a0, const void *b0)
{
	int a = *(const int *)a0;
	int b = *(const int *)b0;
	return s_cmp( get_dirname(a), get_dirname(b) );
}


void create_sorted_index(void)
{
	if(!sorted_index) {
		sorted_index = m_create( m_len(dirnodelist), sizeof(int));
		for(int i=1;i<m_len(dirnodelist);i++) m_puti(sorted_index,i);
	}
	m_qsort(sorted_index, dirname_compare );
}

void print_sorted_index(void)
{
	int p,*d;
	m_foreach(sorted_index, p, d) {
		printf("%s\n", m_str(get_dirname(*d)));
	}
}

int bsearch_cmp_dirname (const void *a0, const void *b0)
{
        int key = *(const int *)a0;
        int index = *(const int *)b0;
//	printf("key:%s, Dir:%s\n", m_str(key), get_dirname( INT(sorted_index,index)));

	printf("key:%d Index:%d Dir:%d\n", key & 0xffff, index, INT(sorted_index,index) );
	int k = INT(sorted_index,index) ;
        int d = get_dirname(k);
        printf("dn:%d %s\n", d & 0xffff, m_str(d) );
	
	return s_cmp( key, get_dirname( index ));
}

void find_dir(void)
{
	int dir = s_cstr("idea");
	printf("dir:%d %s\n",dir &0xffff, m_str(dir));
	int x = m_bsearch (&dir, sorted_index, bsearch_cmp_dirname );
	if(x<0) {
		puts("dir not found");
	}
	printf("index:%d\n", x ); 
	int k = INT(sorted_index,x);
	printf("Index %d %d\n", x, INT(sorted_index,x)); 
	print_path(INT(sorted_index,x));
	
        struct dirnode *dn = dirnode_get(k);
	

}

int main()
{
  m_init();
  trace_level = DW;
  TRACE(DW, "TRACE ON" );
  dirwalker_init();
  int m = dirnode_new(-1, ".." );
  read_dir(m, m, dirnode_get(m)->name );
  TRACE(DW, "Succesfully read");
  print_dir(0);
  printf("\n--- files with full path ---\n");
  print_files_fullpath();
  printf("\n--- sorted dir ---\n");
  create_sorted_index();
  print_sorted_index();
  find_dir();
  dirwalker_free();
  m_destruct();
}



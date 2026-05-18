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
      read_dir(cur, node, fullPath);  // Recursive call for subdirectory
    } else {
      printf("       %s\n", entry->d_name);
      int node = dirnode_new(parent, entry->d_name);
      if( curp->subs ==0 ) curp->subs = m_create(10,sizeof(int));
      m_put(curp->subs, &node);
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



int main()
{
  m_init();
  trace_level = DW;
  TRACE(DW, "TRACE ON" );
  dirwalker_init();
  int m = dirnode_new(-1, ".." );
  read_dir(-1, m, dirnode_get(0)->name );
  TRACE(DW, "Succesfully read");
  print_dir(0);
  dirwalker_free();
  m_destruct();
}
  



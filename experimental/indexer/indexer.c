#include "mls.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

struct node {
    int name;    // Handle to the string name
    int type;    // 0 for file, 1 for directory
    int entries; // Handle to the list of child nodes (if directory)
};

void  node_free(int node)
{
	int i;
	struct node *n;
	m_foreach(node,i,n) {
		m_free(n->name);
		if( n->type == 1 ) {
			node_free(n->entries);
		}
	}
	m_free(node);

}


int scan_directory(const char *path) {
    struct dirent *entry;
    DIR *dp = opendir(path);

    if (dp == NULL) {
        return -1; 
    }

    // Create a list to hold the nodes found in this directory
    int list_handle = m_create(10, sizeof(struct node));

    while ((entry = readdir(dp))) {
        // Skip navigation links "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct node new_node;
        
        // Convert string name to your internal integer handle
        new_node.name = s_printf(0,0,"%s", entry->d_name);
        new_node.entries = -1; // Default to no children

        // Determine if entry is a directory or file
        if (entry->d_type == DT_DIR) {
            new_node.type = 1;
            
            // Construct full path for recursion
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            
            // Recurse and store the resulting list handle in 'entries'
            new_node.entries = scan_directory(full_path);
        } else {
            new_node.type = 0;
        }

        // Add the populated node to the current list
        m_put(list_handle, &new_node);
    }

    closedir(dp);
    return list_handle;
}



int search_rec(int res, int path, int nodes, const char *s)
{
	if( path == 0 ) path=m_create(10,sizeof(int));
	if( res == 0 )  res=m_alloc(10,sizeof(int), MFREE_EACH );
	int i;
	struct node *n;
	m_foreach( nodes, i, n ) {
		if( n->type == 0 ) {
			
			if( s_strcmp_c(n->name, s) == 0 ) {
				int x,*d,comma=32;
				m_foreach(path,x,d) {
					printf("%c%d", comma, *d); comma=',';
				};
				printf(" %d ", i );
				puts( m_str (n->name) );
				x=m_dub(path);
				m_puti(x,i);
				m_puti(res,x);
			}
		} else {
			m_puti(path,i); 
			search_rec(res,path, n->entries, s );
			m_pop(path);
		}
 	}
	if( m_len(path) == 0 ) m_free(path);
	return res;
}

void print_res(int nodes, int res)
{
	struct node *n;
	int x,*d,comma=32;
	m_foreach(res,x,d) {
		printf("%c%d", comma, *d); comma=',';
		n=mls(nodes,*d);
		printf( "%s/", m_str(n->name));
		nodes=n->entries;
	}       
}


int main()
{
	m_init();
	int nodes = scan_directory( "/home/jens/git/mls" );
	int res = search_rec(0,0,nodes, "mls.o");
	int x,*d;
	m_foreach(res,x,d) {
		print_res(nodes,*d);
		printf( "================\n" );
	}

		

	node_free(nodes);
	m_free(res);
	m_destruct();
	return 0;
	
}

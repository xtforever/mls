#include "../lib/m_hdf.h"
#include "../lib/m_tool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_hdf_basic() {
    const char *data = 
        "(rem \"This is a comment\")\n"
        "(server\n"
        "  (port 8080)\n"
        "  (root \"/var/www\")\n"
        "  (rem skip this)\n"
        "  (debug true)\n"
        "  (raw [[multi\nline]])\n"
        ")";
    
    int root = hdf_parse_string(data);
    assert(root > 0);
    assert(hdf_get_type(root) == HDF_TYPE_LIST);
    
    int children = hdf_get_children(root);
    assert(m_len(children) == 1); // Only 'server', 'rem' should be gone
    
    int server = INT(children, 0);
    assert(hdf_get_type(server) == HDF_TYPE_LIST);
    
    const char *port = hdf_get_property(server, "port");
    assert(port != NULL);
    assert(strcmp(port, "8080") == 0);
    
    const char *root_path = hdf_get_property(server, "root");
    assert(root_path != NULL);
    assert(strcmp(root_path, "/var/www") == 0);
    
    const char *debug = hdf_get_property(server, "debug");
    assert(debug != NULL);
    assert(strcmp(debug, "true") == 0);

    int raw_node = hdf_find_node(server, "raw");
    assert(raw_node > 0);
    const char *raw_val = hdf_get_value(INT(hdf_get_children(raw_node), 1));
    assert(strcmp(raw_val, "multi\nline") == 0);
    
    hdf_free(root);
    printf("HDF basic test passed.\n");
}

int main() {
    m_init();
    conststr_init();
    trace_level = 1;
    
    test_hdf_basic();
    
    conststr_free();
    return 0;
}

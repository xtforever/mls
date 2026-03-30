#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_hdf.h"
#include "../lib/m_http_server.h"
#include <stdio.h>

int main() {
    m_init();
    conststr_init();
    trace_level = 1;

    const char *config_data = 
        "(server\n"
        "  (port 20000)\n"
        "  (host \"127.0.0.1\")\n"
        "  (rem \"Routes\")\n"
        "  (route (path \"/\") (text \"Welcome to HDF Server\"))\n"
        "  (route (path \"/api/status\") (json \"{\\\"status\\\":\\\"ok\\\"}\"))\n"
        "  (route (path \"/echo\") (echo true))\n"
        ")";

    int hdf_root = hdf_parse_string(config_data);
    if (hdf_root <= 0) {
        fprintf(stderr, "Failed to parse HDF config\n");
        return 1;
    }

    http_server_config_t conf = http_server_config_load(hdf_root);
    
    // We can free HDF tree after loading config if we copied everything
    hdf_free(hdf_root);

    http_server_run(&conf);

    http_server_config_free(&conf);
    conststr_free();
    m_destruct();
    return 0;
}

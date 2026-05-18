#include "../lib/mls.h"
#include "../lib/m_flask.h"
#include "../lib/m_http_server.h" // Potentially useful for lower-level config if needed
#include "video_indexer.h"
#include "api_handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE "indexer.hdf"

// Global variable for the video index (defined in video_indexer.c)
// extern VideoIndex global_video_index;

int main() {
    // 1. Initialize MLS and its core components
    m_init();
    conststr_init();
    flask_init();
    
    // Initialize the video indexer
    if (video_index_init() != 0) {
        fprintf(stderr, "Failed to initialize video indexer.\n");
        return 1;
    }

    // 2. Register Flask handlers
    // These handlers are defined in api_handlers.c
    flask_register("index_controller", index_controller);
    flask_register("search_controller", search_controller);
    flask_register("status_controller", status_controller);

    // 3. Load configuration from HDF file and start the server
    // We use flask_run_mt for multithreaded request handling.
    // The HDF file will define routes and server port.
    printf("Starting video indexer server with config: %s\n", CONFIG_FILE);

    // Note: For flask_run_mt to be available, MLS_THREAD_SAFE must be defined during compilation.
    // We assume it is defined or will be in the Makefile.
#ifdef MLS_THREAD_SAFE
    flask_run_mt(CONFIG_FILE);
#else
    // Fallback to single-threaded if MLS_THREAD_SAFE is not defined
    // This is not ideal for the demo, but prevents compilation errors.
    fprintf(stderr, "Warning: MLS_THREAD_SAFE not defined. Running in single-threaded mode.\n");
    flask_run(CONFIG_FILE); 
#endif

    // Clean up resources upon server shutdown (e.g., if flask_run_mt returns)
    // This part might not be reached if the server runs indefinitely.
    video_index_free();
    m_destruct();

    return 0;
}

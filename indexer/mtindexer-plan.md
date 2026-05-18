Project Plan: Multithreaded Flask Video Indexer Demo

**Goal:** To build a functional demo application that indexes video files across multiple directories using `mls` and exposes this functionality via a multithreaded Flask-like API.

**Key Technologies:**
*   `mls` library (for memory management, string manipulation, data structures, HTTP server components, HDF configuration)
*   POSIX Threads (`pthread`) for concurrent indexing
*   HDF format for configuration and routing

**Phase 1: Project Setup and Core Data Structures**

1.  **Create Project Structure:**
    *   Create essential source and header files:
        *   `main.c`: Entry point, initializes `mls`, sets up Flask, loads config, starts server.
        *   `video_indexer.h`: Declarations for `VideoEntry`, `VideoIndex`, and indexing functions.
        *   `video_indexer.c`: Implementation of `VideoEntry`, `VideoIndex`, and indexing logic.
        *   `api_handlers.h`: Declarations for API controller functions.
        *   `api_handlers.c`: Implementation of `index_controller`, `search_controller`, `status_controller`.
        *   `indexer.hdf`: HDF configuration file for server routes, port, and initial scan directories.
    *   Create a `Makefile` for building the C project, linking against `mls` and `pthread`.

2.  **Define Data Structures:**
    *   Implement `VideoEntry` struct in `video_indexer.h` and `video_indexer.c` using `m_str_t` for fields.
    *   Implement `VideoIndex` struct in `video_indexer.h` and `video_indexer.c` using `m_list_t` for `entries` and `scan_dirs`.
    *   **Thread Safety:** Integrate `pthread_mutex_t` for `VideoIndex.entries` and `VideoIndex.scan_dirs` to ensure safe concurrent access from multiple threads.

**Phase 2: Implementing Indexing Logic**

1.  **`video_indexer.c` Enhancements:**
    *   **`index_videos` function:**
        *   This function will be the core of the indexing process.
        *   It will iterate through the `VideoIndex.scan_dirs` list.
        *   For each directory, it will recursively scan for video files (using standard C file I/O or `mls` equivalents if available/suitable).
        *   Implement filename parsing logic (using `mls` string functions) to detect show titles, season numbers, and episode numbers based on common patterns (e.g., "Show Name SxxExx", "Show Name - Season X - Episode Y").
        *   Create `VideoEntry` objects for each found video.
        *   **Thread-Safe Updates:** Use the `index_mutex` to protect additions to `VideoIndex.entries` and the `scan_dir_mutex` when modifying the `scan_dirs` list.
    *   **Indexing Thread Management:**
        *   Create functions to manage worker threads for scanning directories and processing files. This could involve a thread pool or spawning threads dynamically.
        *   Ensure threads properly signal completion and handle potential errors.

**Phase 3: API Development**

1.  **`api_handlers.c` Implementation:**
    *   **`index_controller` function:**
        *   Parse directory path from request (e.g., query parameter).
        *   Add directory to `VideoIndex.scan_dirs` (using mutex).
        *   If indexing is not active, start the `index_videos` process in a new thread (or manage a worker pool).
        *   Return a JSON response indicating success or current indexing status.
    *   **`search_controller` function:**
        *   Parse search query parameters (e.g., `?q=title`, `?season=1`).
        *   Acquire read lock/mutex on `VideoIndex.entries`.
        *   Iterate through `VideoEntry` list, filtering based on search criteria.
        *   Construct JSON response with matching videos.
        *   Release lock/mutex.
        *   Return JSON response.
    *   **`status_controller` function:**
        *   Access global `VideoIndex` state (indexing status, file count, current scan path).
        *   Return a JSON response with status information.

2.  **`main.c` and Flask Integration:**
    *   Initialize `mls`, `conststr`, and `m_flask`.
    *   Load `indexer.hdf` configuration.
    *   Register handler functions (`index_controller`, `search_controller`, `status_controller`) with `m_flask` using routes defined in HDF.
    *   Start the multithreaded server using `flask_run_mt`.

**Phase 4: Configuration and Build**

1.  **`indexer.hdf` File:**
    *   Define server port (e.g., `(server (port 8080))`).
    *   Define routes mapping paths to handlers:
        ```hdf
        (server
          (port 8080)
          (bind (path "/index") (call "index_controller"))
          (bind (path "/search") (call "search_controller"))
          (bind (path "/status") (call "status_controller"))
        )
        ```
    *   Potentially include initial directories to scan or settings for parsing heuristics.

2.  **`Makefile`:**
    *   Define compilation rules to build the executable.
    *   Ensure linking with `mls` library and `pthread` library.

**Phase 5: Testing and Refinement**

1.  **API Testing:**
    *   Use `curl` or a scripting language (like Python) to send requests to `/index`, `/search`, and `/status` endpoints.
    *   Verify responses, especially JSON formats and search results.
    *   Test concurrent requests to `/search` and `/index` to ensure `flask_run_mt` and indexing threads work correctly.
2.  **Code Review & Optimization:**
    *   Review code for thread-safety issues, memory leaks (though `mls` should help), and efficiency.
    *   Refine filename parsing heuristics for better TV show detection.
    *   Add error handling for file system operations and parsing.

This plan provides a structured approach to building the demo, leveraging the capabilities of `mls` and `pthread` for a robust and efficient video indexer.
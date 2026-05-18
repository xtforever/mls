#include "video_indexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

// --- Global Index Instance ---
VideoIndex global_video_index;

// --- Helper Functions ---

// Allocates and initializes a VideoEntry
static VideoEntry* create_video_entry(const char* filepath) {
    VideoEntry* entry = m_alloc(sizeof(VideoEntry), 1, MFREE);
    if (!entry) return NULL;

    // Initialize fields to default/null values
    entry->path = m_strdup(filepath);
    entry->filename = m_strrchr(filepath, '/') ? m_strdup(m_strrchr(filepath, '/') + 1) : m_strdup(filepath);
    entry->show_title = NULL;
    entry->season_num = 0;
    entry->episode_num = 0;

    // More sophisticated parsing will go here later
    // For now, just setting path and filename

    return entry;
}

// Frees a VideoEntry
static void free_video_entry(VideoEntry* entry) {
    if (!entry) return;
    m_free(entry->path);
    m_free(entry->filename);
    m_free(entry->show_title);
    m_free(entry);
}

// Callback for m_list_free to free each VideoEntry pointer
static void free_video_entry_list_cb(void* ptr) {
    free_video_entry((VideoEntry*)ptr);
}

// Parses a filename to extract show title, season, and episode
// This is a placeholder and needs to be fleshed out with heuristics.
static void parse_filename_heuristics(VideoEntry* entry) {
    if (!entry || !entry->filename) return;

    const char* filename = m_str(entry->filename);
    size_t len = m_len(entry->filename);

    // Basic example: Look for "SxxExx" pattern
    char* s_ptr = strstr(filename, "S");
    if (s_ptr) {
        char* e_ptr = strstr(s_ptr + 1, "E");
        if (e_ptr) {
            // Found potential SxxExx pattern
            // Extract season number
            char season_str[3] = {0};
            if (e_ptr - s_ptr > 1 && e_ptr - s_ptr < 5) { // Ensure reasonable length
                strncpy(season_str, s_ptr + 1, e_ptr - (s_ptr + 1));
                entry->season_num = atoi(season_str);
            }

            // Extract episode number
            char episode_str[3] = {0};
            char* next_space = NULL;
            for(char* temp = e_ptr + 1; *temp && (isdigit(*temp) || *temp == ' ' || *temp == '.'); ++temp) {
                if (*temp == ' ' || *temp == '.') {
                    next_space = temp;
                    break;
                }
            }
            if (next_space && next_space - (e_ptr + 1) > 0 && next_space - (e_ptr + 1) < 3) {
                strncpy(episode_str, e_ptr + 1, next_space - (e_ptr + 1));
                entry->episode_num = atoi(episode_str);
            }

            // Try to extract show title from beginning of filename up to S
            if (s_ptr > filename) {
                entry->show_title = m_strndup(filename, s_ptr - filename);
                // Trim trailing spaces/dots from show title
                size_t title_len = m_len(entry->show_title);
                while(title_len > 0 && (entry->show_title.buf[title_len-1] == ' ' || entry->show_title.buf[title_len-1] == '.' || entry->show_title.buf[title_len-1] == '-')) {
                    entry->show_title.buf[title_len-1] = '\0';
                    title_len--;
                }
                entry->show_title.len = title_len;
            }
        }
    }
}

// Placeholder for recursive directory scanning
// This function will be called by indexing threads.
static void scan_directory_recursive(const char *dir_path, VideoIndex *index) {
    DIR *dir;
    struct dirent *entry;
    char full_path[1024]; // Assuming paths won't exceed 1024 chars

    // Update global status for current scan path (needs mutex)
    pthread_mutex_lock(&index->scan_dir_mutex);
    m_free(index->current_scan_path); // Free previous path if any
    index->current_scan_path = m_strdup(dir_path);
    pthread_mutex_unlock(&index->scan_dir_mutex);

    dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", dir_path, strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full path
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        // Check if it's a directory
        if (entry->d_type == DT_DIR) {
            // Recursively scan subdirectory
            scan_directory_recursive(full_path, index);
        } else if (entry->d_type == DT_REG) {
            // It's a regular file, check if it's a video file (basic extension check)
            // In a real app, you'd have a more robust check.
            if (strstr(entry->d_name, ".mp4") || 
                strstr(entry->d_name, ".mkv") || 
                strstr(entry->d_name, ".avi") ||
                strstr(entry->d_name, ".mov")) { // Add more extensions as needed
                
                VideoEntry* video_entry = create_video_entry(full_path);
                if (video_entry) {
                    parse_filename_heuristics(video_entry);
                    
                    // Add to global index list (thread-safe)
                    pthread_mutex_lock(&index->index_mutex);
                    if (!m_list_push(index->entries, video_entry)) {
                        fprintf(stderr, "Error adding entry to index list for %s\n", full_path);
                        free_video_entry(video_entry); // Clean up if push fails
                    } else {
                        index->indexed_file_count++;
                    }
                    pthread_mutex_unlock(&index->index_mutex);
                }
            }
        }
    }

    closedir(dir);
}

// --- Indexing Worker Thread Function ---

// This is the function that each indexing thread will execute.
// It takes a directory path and processes it.
static void* indexing_worker_thread(void *arg) {
    const char *dir_to_scan = (const char *)arg;
    if (!dir_to_scan) return NULL;

    printf("Worker thread scanning: %s\n", dir_to_scan);
    scan_directory_recursive(dir_to_scan, &global_video_index);

    // Remove the directory from the scan queue once done (needs mutex)
    pthread_mutex_lock(&global_video_index.scan_dir_mutex);
    // This removal logic needs careful handling to avoid race conditions
    // For simplicity in this example, we might just let it stay in the list
    // or implement a more robust queue management.
    // For now, we assume scan_dirs is just a queue to be processed.
    pthread_mutex_unlock(&global_video_index.scan_dir_mutex);

    return NULL;
}

// --- Public API Functions ---

int video_index_init() {
    // Initialize the index structure
    global_video_index.entries = m_list_create();
    global_video_index.scan_dirs = m_list_create(); // Using a list for scan_dirs queue
    global_video_index.num_scan_dirs = 0; // Not using num_scan_dirs with m_list
    global_video_index.is_indexing = false;
    global_video_index.current_scan_path = NULL;
    global_video_index.indexed_file_count = 0;

    if (pthread_mutex_init(&global_video_index.index_mutex, NULL) != 0) {
        fprintf(stderr, "Mutex initialization failed for index_mutex\n");
        return -1;
    }
    if (pthread_mutex_init(&global_video_index.scan_dir_mutex, NULL) != 0) {
        fprintf(stderr, "Mutex initialization failed for scan_dir_mutex\n");
        // Clean up previously initialized mutex
        pthread_mutex_destroy(&global_video_index.index_mutex);
        return -1;
    }

    if (!global_video_index.entries || !global_video_index.scan_dirs) {
        fprintf(stderr, "Failed to create MLS lists for index\n");
        pthread_mutex_destroy(&global_video_index.index_mutex);
        pthread_mutex_destroy(&global_video_index.scan_dir_mutex);
        return -1;
    }

    printf("Video index initialized.\n");
    return 0;
}

void video_index_free() {
    // Free all entries in the index
    if (global_video_index.entries) {
        m_list_foreach(global_video_index.entries, free_video_entry_list_cb);
        m_list_free(global_video_index.entries);
        global_video_index.entries = NULL;
    }
    // Free scan directories list
    if (global_video_index.scan_dirs) {
        m_list_foreach(global_video_index.scan_dirs, free_video_entry_list_cb); // Reuse cb for m_str_t
        m_list_free(global_video_index.scan_dirs);
        global_video_index.scan_dirs = NULL;
    }
    // Free current scan path string
    m_free(global_video_index.current_scan_path);
    global_video_index.current_scan_path = NULL;

    // Destroy mutexes
    pthread_mutex_destroy(&global_video_index.index_mutex);
    pthread_mutex_destroy(&global_video_index.scan_dir_mutex);

    printf("Video index freed.\n");
}

bool video_index_add_scan_dir(const char *dir_path) {
    if (!dir_path) return false;

    pthread_mutex_lock(&global_video_index.scan_dir_mutex);
    m_str_t* dir_str = m_list_push(global_video_index.scan_dirs, m_strdup(dir_path));
    pthread_mutex_unlock(&global_video_index.scan_dir_mutex);

    if (!dir_str) {
        fprintf(stderr, "Failed to add scan directory: %s\n", dir_path);
        return false;
    }
    printf("Added scan directory: %s\n", dir_path);
    return true;
}

// This function will start the indexing process.
// It should manage threads and the global index state.
void video_index_start_indexing() {
    // Simple approach: Process directories one by one in separate threads.
    // A more robust system might use a thread pool or process multiple at once.

    pthread_mutex_lock(&global_video_index.scan_dir_mutex);
    if (m_list_size(global_video_index.scan_dirs) == 0) {
        printf("No directories to scan. Indexing is idle.\n");
        global_video_index.is_indexing = false;
        pthread_mutex_unlock(&global_video_index.scan_dir_mutex);
        return;
    }

    // Check if already indexing to avoid starting multiple times if called rapidly
    if (global_video_index.is_indexing) {
        printf("Indexing is already in progress.\n");
        pthread_mutex_unlock(&global_video_index.scan_dir_mutex);
        return;
    }

    global_video_index.is_indexing = true;
    global_video_index.indexed_file_count = 0;
    m_free(global_video_index.current_scan_path);
    global_video_index.current_scan_path = NULL;
    printf("Starting indexing process...\n");

    // We need to process directories from the scan_dirs list.
    // Let's create a copy of the list to iterate over, so we can clear the original.
    m_list_t *dirs_to_process = m_list_copy(global_video_index.scan_dirs);
    m_list_clear(global_video_index.scan_dirs); // Clear the original queue
    global_video_index.num_scan_dirs = 0; // Reset if using array approach
    pthread_mutex_unlock(&global_video_index.scan_dir_mutex);

    // Create threads for each directory to scan
    size_t num_dirs = m_list_size(dirs_to_process);
    if (num_dirs > 0) {
        pthread_t threads[num_dirs];
        for (size_t i = 0; i < num_dirs; ++i) {
            m_str_t* dir_str = m_list_get(dirs_to_process, i);
            if (dir_str && dir_str->buf) {
                // Important: Need to pass a copy of the string to the thread
                // as the original might be freed or modified.
                char* dir_copy = m_strdup(dir_str->buf);
                if (pthread_create(&threads[i], NULL, indexing_worker_thread, dir_copy) != 0) {
                    fprintf(stderr, "Failed to create indexing thread for %s\n", dir_copy);
                    // Handle error: maybe free dir_copy, and potentially clean up other threads
                    // For now, just continue.
                    m_free(dir_copy);
                }
            }
        }

        // Wait for all indexing threads to complete
        // This is a simplification; in a real app, you might want to manage threads
        // more dynamically or allow them to run in the background.
        for (size_t i = 0; i < num_dirs; ++i) {
            pthread_join(threads[i], NULL);
        }
    }

    m_list_free(dirs_to_process);

    pthread_mutex_lock(&global_video_index.index_mutex); // Ensure final status update is safe
    global_video_index.is_indexing = false;
    m_free(global_video_index.current_scan_path);
    global_video_index.current_scan_path = NULL;
    printf("Indexing complete. %d files indexed.\n", global_video_index.indexed_file_count);
    pthread_mutex_unlock(&global_video_index.index_mutex);
}

// Placeholder for search functionality
// This would be implemented in api_handlers.c, using global_video_index.entries

// Placeholder for status functionality
// This would be implemented in api_handlers.c, using global_video_index state


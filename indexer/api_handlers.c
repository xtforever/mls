#include "api_handlers.h"
#include "../lib/m_hdf.h" // For HDF parsing in handlers
#include <stdio.h>
#include <string.h>

// --- Helper for JSON Response ---
// Builds a simple JSON string for responses.
static void build_json_response(int res, int status_code, const char* json_content) {
    flask_json(res, status_code, json_content);
}

// --- Controller Implementations ---

void index_controller(int req, int res) {
    const char *dir_param = flask_arg(req, "dir", NULL);
    
    if (!dir_param) {
        build_json_response(res, 400, "{\"error\": \"Missing 'dir' query parameter.\"}");
        return;
    }

    if (video_index_add_scan_dir(dir_param)) {
        // Check if indexing is already running. If not, start it.
        pthread_mutex_lock(&global_video_index.index_mutex); // Lock to check and potentially set is_indexing
        if (!global_video_index.is_indexing) {
            // In a real app, you might want to fork or detach a thread here
            // to not block the HTTP request. For this demo, we'll call it directly
            // but ideally, it should be asynchronous.
            // For now, we'll assume flask_run_mt handles request concurrency,
            // and we'll start indexing in a background thread.
            
            // Create a thread for starting the indexing process.
            // This prevents the /index request from blocking while indexing happens.
            pthread_t indexing_thread;
            if (pthread_create(&indexing_thread, NULL, (void*(*)(void*))video_index_start_indexing, NULL) != 0) {
                fprintf(stderr, "Failed to create indexing thread.\n");
                build_json_response(res, 500, "{\"error\": \"Failed to start indexing thread.\"}");
                global_video_index.is_indexing = false;
            } else {
                // Detach the thread so it runs independently. 
                // We don't need to join it from here.
                pthread_detach(indexing_thread);
                printf("Indexing process started in background thread.\n");
            }
        }
        pthread_mutex_unlock(&global_video_index.index_mutex);

        build_json_response(res, 200, "{\"message\": \"Directory added to scan queue. Indexing started if not already in progress.\"}");
    } else {
        build_json_response(res, 500, "{\"error\": \"Failed to add directory to scan queue.\"}");
    }
}

void search_controller(int req, int res) {
    const char *query = flask_arg(req, "q", NULL);
    const char *season_str = flask_arg(req, "season", NULL);
    const char *episode_str = flask_arg(req, "episode", NULL);

    // Acquire read lock for searching the index
    pthread_mutex_lock(&global_video_index.index_mutex);

    m_list_t *results = m_list_create();
    if (!results) {
        fprintf(stderr, "Failed to create list for search results\n");
        pthread_mutex_unlock(&global_video_index.index_mutex);
        build_json_response(res, 500, "{\"error\": \"Internal error creating search results list.\"}");
        return;
    }

    // Convert query params to integers if provided
    int search_season = (season_str) ? atoi(season_str) : -1;
    int search_episode = (episode_str) ? atoi(episode_str) : -1;

    // Iterate through the index entries
    if (global_video_index.entries) {
        m_list_foreach(global_video_index.entries, ^(void* data) {
            VideoEntry* entry = (VideoEntry*)data;
            bool match = true;

            // Query string match (simple substring search on filename or title)
            if (query && entry->filename && entry->show_title) {
                if (strstr(m_str(entry->filename), query) == NULL &&
                    strstr(m_str(entry->show_title), query) == NULL) {
                    match = false;
                }
            } else if (query && entry->filename) { // Fallback if show_title is not parsed
                 if (strstr(m_str(entry->filename), query) == NULL) {
                    match = false;
                }
            }

            // Season match
            if (match && search_season != -1 && entry->season_num != search_season) {
                match = false;
            }

            // Episode match
            if (match && search_episode != -1 && entry->episode_num != search_episode) {
                match = false;
            }

            if (match) {
                m_list_push(results, entry); // Add pointer to the results list
            }
        });
    }

    pthread_mutex_unlock(&global_video_index.index_mutex);

    // Build JSON response from search results
    // This part requires string manipulation to format JSON correctly.
    // For simplicity, let's just return a count and a placeholder for now.
    // A real implementation would serialize the VideoEntry data.
    
    // Building JSON string manually for demonstration
    // This can become complex for many results. Consider a JSON library or careful string building.
    char json_buffer[1024 * 10]; // Start with a reasonable buffer size
    int current_len = snprintf(json_buffer, sizeof(json_buffer), "{\"count\": %zu, \"results\": [", m_list_size(results));

    for (size_t i = 0; i < m_list_size(results) && current_len < sizeof(json_buffer) - 100; ++i) {
        VideoEntry* entry = m_list_get(results, i);
        if (!entry) continue;

        // Basic JSON formatting for each entry
        current_len += snprintf(json_buffer + current_len, sizeof(json_buffer) - current_len,
                                "{\"path\": \"%s\", \"filename\": \"%s\", \"show_title\": \"%s\", \"season\": %d, \"episode\": %d}",
                                m_str(entry->path), m_str(entry->filename), 
                                entry->show_title ? m_str(entry->show_title) : "null", 
                                entry->season_num, entry->episode_num);
        if (i < m_list_size(results) - 1) {
            strncat(json_buffer, ",", sizeof(json_buffer) - current_len -1);
        }
    }
    strncat(json_buffer, "]}", sizeof(json_buffer) - current_len -1);

    m_list_free(results);
    build_json_response(res, 200, json_buffer);
}

void status_controller(int req, int res) {
    char status_json[256];
    
    pthread_mutex_lock(&global_video_index.index_mutex);
    bool is_indexing = global_video_index.is_indexing;
    int indexed_count = global_video_index.indexed_file_count;
    m_str_t current_path = global_video_index.current_scan_path;
    size_t scan_queue_size = m_list_size(global_video_index.scan_dirs);
    pthread_mutex_unlock(&global_video_index.index_mutex);

    snprintf(status_json, sizeof(status_json),
             "{\"indexing\": %s, \"indexed_files\": %d, \"scan_queue_size\": %zu, \"current_scan\": \"%s\"}",
             is_indexing ? "true" : "false",
             indexed_count,
             scan_queue_size,
             current_path ? m_str(current_path) : "none");

    build_json_response(res, 200, status_json);
}

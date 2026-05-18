# Video Indexer Structures

## VideoEntry

Represents a single video file and its parsed metadata.

```c
typedef struct {
    m_str_t path;           // Full path to the video file
    m_str_t filename;       // Base filename
    m_str_t show_title;     // Parsed TV show title (e.g., "The Mandalorian")
    int     season_num;     // Parsed season number (0 if not applicable/found)
    int     episode_num;    // Parsed episode number (0 if not applicable/found)
    // Potentially add other metadata here as needed, e.g., duration, resolution
} VideoEntry;
```

## VideoIndex

Manages the collection of video entries and the indexing process.

```c
#define VIDEO_INDEX_MAX_SCAN_DIRS 100 // Example limit

typedef struct {
    m_list_t *entries;         // List of VideoEntry * (the actual index)
    m_str_t scan_dirs[VIDEO_INDEX_MAX_SCAN_DIRS]; // Array of directories to scan
    int num_scan_dirs;         // Number of directories currently in scan_dirs
    pthread_mutex_t index_mutex; // Mutex to protect access to 'entries' list
    pthread_mutex_t scan_dir_mutex; // Mutex to protect access to 'scan_dirs' array and num_scan_dirs
    bool is_indexing;          // Flag to indicate if indexing is currently active
    m_str_t current_scan_path; // Directory currently being scanned by a worker thread
    int indexed_file_count;    // Count of files successfully indexed
    // Potentially add other status/state variables here
} VideoIndex;

// Global instance of the index
extern VideoIndex global_video_index;

// Function to initialize the index
int video_index_init();

// Function to free the index and its contents
void video_index_free();

// Function to add a directory to the scan queue
bool video_index_add_scan_dir(const char *dir_path);

// Function to start the indexing process (likely spawns threads)
void video_index_start_indexing();

// Function to get current indexing status
// (Potentially returns a struct with status info)

// Function to search the index
// (Takes search criteria, returns a list of matching VideoEntry pointers)

// Helper to parse filename and extract metadata
// VideoEntry* parse_video_filename(const char *filepath);

// Helper to recursively scan a directory (called by worker threads)
// void scan_directory_recursive(const char *dir_path, VideoIndex *index);

#endif // VIDEO_INDEXER_H

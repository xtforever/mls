#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include "../lib/m_flask.h"
#include "../lib/mls.h"
#include "video_indexer.h" // For VideoIndex and VideoEntry access

// --- API Controller Functions ---

// Handler for POST /index
// Adds a directory to the scan queue and potentially starts indexing.
void index_controller(int req, int res);

// Handler for GET /search
// Searches the video index based on query parameters.
void search_controller(int req, int res);

// Handler for GET /status
// Returns the current status of the indexing process.
void status_controller(int req, int res);

#endif // API_HANDLERS_H

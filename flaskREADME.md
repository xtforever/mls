# Flask Microframework for MLS

This framework brings a Flask-like developer experience to C, leveraging the power of **MLS** (Memory List System) and **HDF** (Human Data Forms) for routing and configuration.

## 1. Running the Tests

To compile and run the existing Flask tests, use the following commands from the project root:

```bash
# Compile the advanced test
make -C tests test_flask_advanced.exed

# Run the test (it includes an automated test client using curl)
./tests/test_flask_advanced.exed
```

## 2. Building a Simple API Tool

The following example demonstrates how to implement a date conversion service using idiomatic MLS style. It uses `s_printf` to build the response and `flask_json_h` to send it.

### Example Code: `date_tool.c`

```c
#include "m_flask.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void date_handler(int req, int res) {
    const char *iso = flask_arg(req, "iso", NULL);
    const char *unix_str = flask_arg(req, "unix", NULL);
    int json_h = 0;

    if (iso) {
        struct tm tm = {0};
        // Note: strptime requires _XOPEN_SOURCE or _GNU_SOURCE
        if (strptime(iso, "%Y-%m-%d", &tm)) {
            time_t t = mktime(&tm);
            json_h = s_printf(0, 0, "{\"unix\": %ld}", (long)t);
            flask_json_h(res, 200, json_h);
        } else {
            flask_json(res, 400, "{\"error\": \"Invalid ISO format. Use YYYY-MM-DD\"}");
        }
    } else if (unix_str) {
        time_t t = (time_t)atol(unix_str);
        struct tm *tm = gmtime(&t);
        if (tm) {
            json_h = s_printf(0, 0, "{\"iso\": \"%04d-%02d-%02d\"}", 
                              tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
            flask_json_h(res, 200, json_h);
        } else {
            flask_json(res, 400, "{\"error\": \"Invalid unix timestamp\"}");
        }
    } else {
        flask_json(res, 400, "{\"error\": \"Missing 'iso' or 'unix' parameter\"}");
    }
    
    // flask_json_h copies the data, so we free our local handle
    m_free(json_h);
}

int main() {
    m_init();
    conststr_init();
    flask_init();

    // Register the C function to a name used in HDF
    flask_register("date_handler", date_handler);

    // Create a simple config file on the fly
    FILE *fp = fopen("config.hdf", "w");
    fprintf(fp, "(server (port 8080) (bind (path \"/api/date\") (call \"date_handler\")))\n");
    fclose(fp);

    printf("Date tool running on port 8080...\n");
    printf("Try: curl \"http://localhost:8080/api/date?iso=2024-11-15\"\n");
    
    flask_run("config.hdf");
    return 0;
}
```

## 3. How it works

1.  **Initialization**: `flask_init()` sets up the internal handler registry.
2.  **Registration**: `flask_register` maps a C function to a string identifier.
3.  **Routing**: The `.hdf` file defines which URL paths map to which registered identifiers. This allows you to change your API structure without recompiling.
4.  **Handling**:
    *   `flask_arg(req, "key", "default")`: Extracts query parameters (e.g., `?iso=...`).
    *   `flask_header(req, "User-Agent")`: Retrieves request headers (case-insensitive).
    *   `flask_json_h(res, status, handle)`: Sets the Content-Type to JSON and takes an MLS handle for the body.
    *   `flask_printf(res, "fmt", ...)`: Appends formatted text to the response body.

## 4. Testing the Tool

Once running, you can interact with it via `curl`:

```bash
# Convert ISO date to Unix timestamp
curl "http://localhost:8080/api/date?iso=2024-11-15"
# Response: {"unix": 1731628800}

# Convert Unix timestamp to ISO date
curl "http://localhost:8080/api/date?unix=1731628800"
# Response: {"iso": "2024-11-15"}
```

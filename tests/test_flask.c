#include "../lib/m_flask.h"
#include <stdio.h>

void index_handler(int req, int res) {
    flask_printf(res, "<h1>Welcome to MLS Flask</h1>");
    flask_printf(res, "<p>Method: %s</p>", flask_arg(req, "method", "unknown"));
}

void api_hello(int req, int res) {
    const char *name = flask_arg(req, "name", "World");
    char json[256];
    snprintf(json, sizeof(json), "{\"message\": \"Hello, %s!\"}", name);
    flask_json(res, 200, json);
}

int main() {
    m_init();
    conststr_init();
    flask_init();

    flask_register("index_handler", index_handler);
    flask_register("api_hello", api_hello);

    // Create a temporary HDF file for testing
    FILE *fp = fopen("flask_test.hdf", "w");
    fprintf(fp, 
        "(server\n"
        "  (port 20001)\n"
        "  (bind (path \"/\") (call \"index_handler\"))\n"
        "  (bind (path \"/api/hello\") (call \"api_hello\"))\n"
        ")\n");
    fclose(fp);

    printf("Starting test flask server on port 20001...\n");
    printf("Try: curl \"http://127.0.0.1:20001/api/hello?name=Gemini\"\n");
    
    flask_run("flask_test.hdf");

    return 0;
}

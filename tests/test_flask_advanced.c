#include "../lib/m_flask.h"
#include "../lib/m_hdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

/* 
 * This advanced example demonstrates:
 * 1. Using HDF for both routing and application configuration.
 * 2. Retrieving data from HDF within handlers (flask_get_config).
 * 3. Handling query parameters (flask_arg).
 * 4. Generating JSON responses (flask_json).
 * 5. Programmatic verification using fork().
 */

void status_handler(int req, int res) {
    int config = flask_get_config();
    const char *app_name = hdf_get_property(config, "app_name");
    const char *version = hdf_get_property(config, "version");
    
    char json[512];
    snprintf(json, sizeof(json), 
        "{\"status\": \"running\", \"app\": \"%s\", \"version\": \"%s\", \"method\": \"%s\"}", 
        app_name ? app_name : "unknown", 
        version ? version : "0.0.0",
        flask_method(req));
    
    flask_json(res, 200, json);
}

void user_handler(int req, int res) {
    const char *username = flask_arg(req, "user", NULL);
    if (!username) {
        flask_json(res, 400, "{\"error\": \"Missing user parameter\"}");
        return;
    }

    int config = flask_get_config();
    int users_node = hdf_find_node(config, "users");
    if (users_node > 0) {
        int user_node = hdf_find_node(users_node, username);
        if (user_node > 0) {
            const char *role = hdf_get_property(user_node, "role");
            const char *email = hdf_get_property(user_node, "email");
            char json[512];
            snprintf(json, sizeof(json), 
                "{\"user\": \"%s\", \"role\": \"%s\", \"email\": \"%s\"}", 
                username, role ? role : "user", email ? email : "none");
            flask_json(res, 200, json);
            return;
        }
    }

    flask_json(res, 404, "{\"error\": \"User not found\"}");
}

void echo_handler(int req, int res) {
    flask_printf(res, "URI: %s\n", flask_uri(req));
    flask_printf(res, "Method: %s\n", flask_method(req));
    flask_printf(res, "User-Agent: %s\n", flask_header(req, "User-Agent"));
}

void setup_test_config(const char *path) {
    FILE *fp = fopen(path, "w");
    fprintf(fp, 
        "(app_name \"MLS Flask Demo\")\n"
        "(version \"1.2.3\")\n"
        "\n"
        "(users\n"
        "  (alice (role \"admin\") (email \"alice@example.com\"))\n"
        "  (bob   (role \"user\")  (email \"bob@example.com\"))\n"
        ")\n"
        "\n"
        "(server\n"
        "  (port 20002)\n"
        "  (bind (path \"/status\") (call \"status_handler\"))\n"
        "  (bind (path \"/user\")   (call \"user_handler\"))\n"
        "  (bind (path \"/echo\")   (call \"echo_handler\"))\n"
        ")\n");
    fclose(fp);
}

int main() {
    m_init();
    conststr_init();
    flask_init();

    flask_register("status_handler", status_handler);
    flask_register("user_handler", user_handler);
    flask_register("echo_handler", echo_handler);

    const char *config_path = "flask_adv.hdf";
    setup_test_config(config_path);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process: run the server
        printf("[Server] Starting on port 20002...\n");
        flask_run(config_path);
        exit(0);
    } else if (pid > 0) {
        // Parent process: run tests
        sleep(1); // Give server time to start

        printf("[Test] Verifying /status...\n");
        system("curl -v \"http://127.0.0.1:20002/status\"");
        printf("\n");

        printf("[Test] Verifying /user?user=alice...\n");
        system("curl -v \"http://127.0.0.1:20002/user?user=alice\"");
        printf("\n");

        printf("[Test] Verifying /user?user=charlie (404)...\n");
        system("curl -s \"http://127.0.0.1:20002/user?user=charlie\"");
        printf("\n");

        printf("[Test] Verifying /echo...\n");
        system("curl -s \"http://127.0.0.1:20002/echo\"");
        printf("\n");

        printf("[Test] Killing server process %d...\n", pid);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        printf("[Test] Done.\n");
    } else {
        perror("fork");
        return 1;
    }

    return 0;
}

# Unleashing the Power of `m_table`: Real-World Greatness

`m_table` brings the familiar and flexible dictionary (or hashmap) data structure from interpreted languages directly into `mls` in C. It allows you to store and retrieve data by both string names and integer indices, supporting nested structures and providing type introspection – all while benefiting from `mls`'s robust memory management.

Forget defining endless `struct`s or battling with string parsing for configuration files. `m_table` lets you build dynamic, self-describing data with ease.

## Example 1: Dynamic Configuration Management

Imagine managing a complex application configuration. Some settings are simple key-value pairs, others are nested sections, and some might be lists of options. `m_table` handles it all gracefully.

```c
#include "mls.h"
#include "m_tool.h"
#include "m_table.h"
#include <stdio.h>

void show_config(int config) {
    printf("--- Application Configuration ---\n");

    // Retrieve simple values
    int window_width = m_table_get_cstr(config, "window_width");
    int window_height = m_table_get_cstr(config, "window_height");
    printf("Window: %dx%d\n", window_width, window_height);

    int app_name_h = m_table_get_cstr(config, "app_name");
    printf("App Name: %s\n", m_str(app_name_h));

    // Accessing a nested section
    int db_settings = m_table_get_cstr(config, "database");
    if (db_settings > 0 && m_table_get_type_cstr(config, "database") == MLS_TABLE_TYPE_TABLE) {
        printf("  Database Settings:\n");
        printf("    Host: %s\n", m_str(m_table_get_cstr(db_settings, "host")));
        printf("    Port: %d\n", m_table_get_cstr(db_settings, "port"));
        printf("    User: %s\n", m_str(m_table_get_cstr(db_settings, "user")));
    }

    // Accessing a list of values
    int features_h = m_table_get_cstr(config, "enabled_features");
    if (features_h > 0 && m_table_get_type_cstr(config, "enabled_features") == MLS_TABLE_TYPE_LIST) {
        printf("  Enabled Features:\n");
        int p, *feature_str_h;
        m_foreach(features_h, p, feature_str_h) {
            printf("    - %s\n", m_str(*feature_str_h));
        }
    }
    printf("---------------------------------\n");
}

int main() {
    m_init();
    conststr_init();

    int config = m_table_create();

    // Set simple key-value pairs (using the new, simpler API!)
    m_table_set_const_string_by_cstr(config, "app_name", "My Awesome App");
    m_table_set_string_by_cstr(config, "version", "1.0.2");
    m_table_set_int_val_by_cstr(config, "window_width", 1024);
    m_table_set_int_val_by_cstr(config, "window_height", 768);

    // Create a nested table for database settings
    int db_settings = m_table_create();
    m_table_set_const_string_by_cstr(db_settings, "host", "localhost");
    m_table_set_int_val_by_cstr(db_settings, "port", 5432);
    m_table_set_string_by_cstr(db_settings, "user", "app_user");
    m_table_set_table_by_cstr(config, "database", db_settings);

    // Create a list of enabled features
    int features_list = m_alloc(0, sizeof(int), MFREE); // List of string handles
    m_put(features_list, &(int){s_cstr("auth")});
    m_put(features_list, &(int){s_cstr("logging")});
    m_put(features_list, &(int){s_printf(0,0,"analytics")});
    m_table_set_list_by_cstr(config, "enabled_features", features_list);
    
    show_config(config);

    // --- Modify Configuration ---
    printf("\n--- Modifying Configuration ---\n");
    m_table_set_int_val_by_cstr(config, "window_width", 1280); // Update width
    m_table_set_string_by_cstr(config, "version", "1.1.0-beta"); // Update version
    show_config(config);


    m_table_free(config);
    conststr_free();
    m_destruct();
    return 0;
}
```

## Example 2: Inspecting Data (Introspection Greatness)

`m_table` doesn't just store data; it remembers its type! This is incredibly powerful for generic data processing, UI generation, or serialization/deserialization.

```c
#include "mls.h"
#include "m_table.h"
#include <stdio.h>

int main() {
    m_init();
    conststr_init();

    int user_profile = m_table_create();
    m_table_set_const_string_by_cstr(user_profile, "username", "johndoe");
    m_table_set_int_val_by_cstr(user_profile, "level", 15);
    
    int friend_list = m_alloc(0, sizeof(int), MFREE);
    m_put(friend_list, &(int){s_cstr("Jane")});
    m_table_set_list_by_cstr(user_profile, "friends", friend_list);

    printf("--- User Profile Inspection ---\n");
    
    // Iterate over entries (linear scan in current implementation)
    m_table_entry_t *entry; // Internal struct, not part of public API
    int p;
    m_foreach(user_profile, p, entry) { 
        if (entry->is_str_key) {
            printf("Key: \"%s\" (Type: ", m_str(entry->key));
        } else {
            // Integer keys not used in this example.
            printf("Key: %d (Type: ", entry->key);
        }
        
        switch (entry->type) {
            case MLS_TABLE_TYPE_INT: printf("INT) Value: %d\n", entry->value); break;
            case MLS_TABLE_TYPE_CONST_STRING: printf("CONST_STRING) Value: \"%s\"\n", m_str(entry->value)); break;
            case MLS_TABLE_TYPE_STRING: printf("STRING) Value: \"%s\"\n", m_str(entry->value)); break;
            case MLS_TABLE_TYPE_LIST:
                printf("LIST) Value: Handle %d (Length %d)\n", entry->value, m_len(entry->value));
                // You could further iterate/inspect the list here
                break;
            case MLS_TABLE_TYPE_TABLE:
                printf("TABLE) Value: Handle %d\n", entry->value);
                // You could recursively call inspection function here
                break;
            default: printf("UNKNOWN) Value: Handle %d\n", entry->value); break;
        }
    }
    printf("-------------------------------\n");

    m_table_free(user_profile);
    conststr_free();
    m_destruct();
    return 0;
}
```

## Why `m_table` is Great:

-   **Dynamic Schemas:** No need for rigid `struct` definitions. Add new fields on the fly.
-   **Mixed Data Types:** Store raw integers, strings, lists, or even other tables all within the same `m_table`.
-   **Built-in Introspection:** Know the type of any stored value without guessing or complex type-tagging logic. Perfect for generic data processing, UI generation, or serialization/deserialization.
-   **Automatic Memory Management:** `m_table` cleans up all its contents (including nested tables and lists) with a single `m_table_free()` call.
-   **Readability:** Code becomes declarative, focusing on *what* data is stored, not *how* memory is managed. The new simplified API significantly enhances this.

---

[Back to Home](README.md)

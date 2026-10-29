/**
 * @file server.c
 * @brief Standalone Online Food Ordering System HTTP Server in C
 * 
 * Features:
 *  - Native TCP Sockets (Winsock on Windows, POSIX on Linux/macOS)
 *  - RESTful HTTP routing:
 *      GET  /          -> Serves modern web frontend (index.html)
 *      GET  /api/menu  -> Returns menu catalog as JSON
 *      POST /api/order -> Parses order, computes totals & tax, writes to orders.txt, returns invoice
 *      GET  /api/orders-> Returns past orders log
 *  - File persistence for menu (menu.txt) and orders (orders.txt)
 *  - Complete cross-platform compatibility and zero third-party dependencies
 */

#include "server.h"

/* Global menu state loaded from menu.txt */
MenuItem g_menu[MAX_MENU_ITEMS];
int g_menu_count = 0;

/* Global user accounts loaded from users.txt */
User g_users[MAX_USERS];
int g_user_count = 0;

/* ========================================================================= */
/* Embedded Default Frontend (Fallback if index.html is missing)             */
/* ========================================================================= */
static const char *EMBEDDED_FALLBACK_HTML = 
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<title>Online Food Ordering System - Server Active</title>"
    "<style>"
    "body { font-family: system-ui, sans-serif; background: #060907; color: #f8fafc; display: flex; "
    "align-items: center; justify-content: center; height: 100vh; margin: 0; }"
    ".card { background: #0f1813; padding: 2rem; border-radius: 1rem; border: 1px solid rgba(0,255,136,0.2); text-align: center; max-width: 500px; box-shadow: 0 0 25px rgba(0,255,136,0.15); }"
    "h1 { color: #00ff88; margin-top: 0; text-shadow: 0 0 20px rgba(0,255,136,0.3); }"
    "p { color: #94a3b8; line-height: 1.6; }"
    ".badge { display: inline-block; background: rgba(0,255,136,0.15); color: #00ff88; border: 1px solid rgba(0,255,136,0.4); font-weight: bold; padding: 0.3rem 0.8rem; border-radius: 9999px; margin-bottom: 1rem; box-shadow: 0 0 10px rgba(0,255,136,0.2); }"
    "</style></head><body>"
    "<div class=\"card\">"
    "<span class=\"badge\">C HTTP Server Online</span>"
    "<h1>BiteCraft Gourmet</h1>"
    "<p>The C backend server is successfully running on port 8080.</p>"
    "<p>Please ensure <code>index.html</code> is present in the server working directory to view the complete interactive food ordering interface.</p>"
    "<p><a href=\"/api/menu\" style=\"color: #00ff88; text-decoration: none; font-weight: bold;\">&rarr; View API Menu (JSON)</a></p>"
    "</div></body></html>";

/* ========================================================================= */
/* Network Initialization & Socket Helpers                                   */
/* ========================================================================= */

/**
 * Initializes the operating system network stack.
 * On Windows, this performs WSAStartup (Winsock initialization).
 * On Unix/Linux/macOS, this is a no-op.
 */
int init_networking(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed with error code: %d\n", result);
        return 0;
    }
    printf("[NETWORK] Winsock 2.2 initialized successfully.\n");
#endif
    return 1;
}

/**
 * Cleans up the operating system network stack on shutdown.
 */
void cleanup_networking(void) {
#ifdef _WIN32
    WSACleanup();
    printf("[NETWORK] Winsock cleaned up.\n");
#endif
}

/**
 * Creates, binds, and configures a TCP listening socket on the specified port.
 */
socket_t create_server_socket(int port) {
    socket_t server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    /* 1. Create stream socket (TCP) */
    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!IS_VALID_SOCKET(server_fd)) {
        fprintf(stderr, "[ERROR] socket() creation failed: %d\n", GET_SOCKET_ERROR());
        return INVALID_SOCKET;
    }

    /* 2. Set socket options to reuse address/port (prevents 'Address already in use') */
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    /* 3. Configure server address structure */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  /* Bind to all local interfaces (0.0.0.0) */
    server_addr.sin_port = htons((unsigned short)port);

    /* 4. Bind socket to IP address and port */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] bind() failed on port %d: %d\n", port, GET_SOCKET_ERROR());
        CLOSE_SOCKET(server_fd);
        return INVALID_SOCKET;
    }

    /* 5. Set socket to passive listening mode with a backlog queue of 16 */
    if (listen(server_fd, 16) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] listen() failed: %d\n", GET_SOCKET_ERROR());
        CLOSE_SOCKET(server_fd);
        return INVALID_SOCKET;
    }

    return server_fd;
}

/* ========================================================================= */
/* Utility Functions (Time, ID Generation, JSON Escaping)                    */
/* ========================================================================= */

/**
 * Generates formatted ISO-like timestamp string: YYYY-MM-DD HH:MM:SS
 */
void get_current_timestamp(char *out_time, size_t max_len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(out_time, max_len, "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(out_time, max_len, "2026-10-22 12:00:00");
    }
}

/**
 * Generates a unique, professional Order ID: ORD-YYYYMMDD-XXXX
 */
void generate_order_id(char *out_id, size_t max_len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char date_part[16] = {0};
    int random_num = 1000 + (rand() % 9000);

    if (tm_info) {
        strftime(date_part, sizeof(date_part), "%Y%m%d", tm_info);
    } else {
        snprintf(date_part, sizeof(date_part), "20261022");
    }

    snprintf(out_id, max_len, "ORD-%s-%04d", date_part, random_num);
}

/**
 * Finds a menu item by ID. Returns pointer to MenuItem or NULL if not found.
 */
const MenuItem* find_menu_item(int id) {
    for (int i = 0; i < g_menu_count; i++) {
        if (g_menu[i].id == id) {
            return &g_menu[i];
        }
    }
    return NULL;
}

/* ========================================================================= */
/* File Persistence: Menu & Orders Handling                                  */
/* ========================================================================= */

/**
 * Trims leading and trailing whitespace / newlines from a string in-place.
 */
static void trim_string(char *str) {
    char *end;
    char *start = str;

    while (isspace((unsigned char)*start)) start++;
    if (*start == 0) {
        *str = 0;
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * Loads menu items from menu.txt using standard file I/O.
 * Format: id|name|category|price|description|badge|image_url
 */
int load_menu_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    char line[1024];

    g_menu_count = 0;

    if (!fp) {
        printf("[WARN] Could not open '%s' for reading. Initializing default menu...\n", filename);
        /* Fallback sample item */
        g_menu[0].id = 1;
        snprintf(g_menu[0].name, sizeof(g_menu[0].name), "Truffle Smash Burger");
        snprintf(g_menu[0].category, sizeof(g_menu[0].category), "Fast Food");
        g_menu[0].price = 12.99;
        snprintf(g_menu[0].description, sizeof(g_menu[0].description), "Double Angus beef patty with truffle aioli");
        snprintf(g_menu[0].badge, sizeof(g_menu[0].badge), "Chef's Special");
        snprintf(g_menu[0].image_url, sizeof(g_menu[0].image_url), "https://images.unsplash.com/photo-1568901346375-23c9450c58cd?w=600&auto=format&fit=crop&q=80");
        g_menu_count = 1;
        return g_menu_count;
    }

    while (fgets(line, sizeof(line), fp) && g_menu_count < MAX_MENU_ITEMS) {
        trim_string(line);
        if (strlen(line) == 0 || line[0] == '#') continue;

        MenuItem item;
        memset(&item, 0, sizeof(item));

        /* Parse pipe-delimited fields */
        char *token = strtok(line, "|");
        if (!token) continue;
        item.id = atoi(token);

        token = strtok(NULL, "|");
        if (token) snprintf(item.name, sizeof(item.name), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(item.category, sizeof(item.category), "%s", token);

        token = strtok(NULL, "|");
        if (token) item.price = atof(token);

        token = strtok(NULL, "|");
        if (token) snprintf(item.description, sizeof(item.description), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(item.badge, sizeof(item.badge), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(item.image_url, sizeof(item.image_url), "%s", token);

        g_menu[g_menu_count++] = item;
    }

    fclose(fp);
    printf("[MENU] Successfully loaded %d menu items from '%s'.\n", g_menu_count, filename);
    return g_menu_count;
}

/**
 * Loads registered customer accounts from users.txt.
 * Format: username|password|name|phone|address
 */
int load_users_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    char line[1024];

    g_user_count = 0;

    if (!fp) {
        printf("[WARN] Could not open '%s' for reading. Initializing default accounts...\n", filename);
        snprintf(g_users[0].username, sizeof(g_users[0].username), "alex");
        snprintf(g_users[0].password, sizeof(g_users[0].password), "password123");
        snprintf(g_users[0].name, sizeof(g_users[0].name), "Alexander Wright");
        snprintf(g_users[0].phone, sizeof(g_users[0].phone), "+1 (555) 234-5678");
        snprintf(g_users[0].address, sizeof(g_users[0].address), "88 Tech Boulevard, 4th Floor");
        g_user_count = 1;
        return g_user_count;
    }

    while (fgets(line, sizeof(line), fp) && g_user_count < MAX_USERS) {
        trim_string(line);
        if (strlen(line) == 0 || line[0] == '#') continue;

        User u;
        memset(&u, 0, sizeof(u));

        char *token = strtok(line, "|");
        if (!token) continue;
        snprintf(u.username, sizeof(u.username), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(u.password, sizeof(u.password), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(u.name, sizeof(u.name), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(u.phone, sizeof(u.phone), "%s", token);

        token = strtok(NULL, "|");
        if (token) snprintf(u.address, sizeof(u.address), "%s", token);

        g_users[g_user_count++] = u;
    }

    fclose(fp);
    printf("[USERS] Successfully loaded %d customer accounts from '%s'.\n", g_user_count, filename);
    return g_user_count;
}

/**
 * Registers a new user account, persisting to users.txt.
 */
int register_user(const User *user) {
    for (int i = 0; i < g_user_count; i++) {
        if (strcmp(g_users[i].username, user->username) == 0) {
            return 0; /* Username taken */
        }
    }
    if (g_user_count >= MAX_USERS) return 0;

    FILE *fp = fopen(USERS_FILE_PATH, "a");
    if (fp) {
        fprintf(fp, "%s|%s|%s|%s|%s\n", user->username, user->password, user->name, user->phone, user->address);
        fclose(fp);
    }

    g_users[g_user_count++] = *user;
    printf("[USERS] Registered new account: '%s' (%s)\n", user->username, user->name);
    return 1;
}

/**
 * Authenticates user credentials against in-memory registry.
 */
const User* authenticate_user(const char *username, const char *password) {
    for (int i = 0; i < g_user_count; i++) {
        if (strcmp(g_users[i].username, username) == 0 && strcmp(g_users[i].password, password) == 0) {
            return &g_users[i];
        }
    }
    return NULL;
}

/**
 * Appends a confirmed order invoice to orders.txt.
 */
int save_order_to_file(const Order *order) {
    FILE *fp = fopen(ORDERS_FILE_PATH, "a");
    if (!fp) {
        fprintf(stderr, "[ERROR] Failed to open '%s' for writing order.\n", ORDERS_FILE_PATH);
        return 0;
    }

    fprintf(fp, "================================================================================\n");
    fprintf(fp, "ORDER INVOICE: %s\n", order->order_id);
    fprintf(fp, "USER ID      : %s\n", (strlen(order->username) > 0 ? order->username : "guest"));
    fprintf(fp, "DATE / TIME  : %s\n", order->timestamp);
    fprintf(fp, "STATUS       : %s\n", order->status);
    fprintf(fp, "CUSTOMER     : %s\n", order->customer_name);
    fprintf(fp, "PHONE        : %s\n", order->customer_phone);
    fprintf(fp, "ADDRESS      : %s\n", order->delivery_address);
    if (strlen(order->notes) > 0) {
        fprintf(fp, "NOTES        : %s\n", order->notes);
    }
    fprintf(fp, "--------------------------------------------------------------------------------\n");
    fprintf(fp, "%-36s %8s %12s %14s\n", "ITEM DESCRIPTION", "QTY", "UNIT PRICE", "SUBTOTAL");
    fprintf(fp, "--------------------------------------------------------------------------------\n");

    for (int i = 0; i < order->item_count; i++) {
        fprintf(fp, "%-36s %8d   ₹%9.2f   ₹%11.2f\n",
                order->items[i].name,
                order->items[i].quantity,
                order->items[i].unit_price,
                order->items[i].subtotal);
    }

    fprintf(fp, "--------------------------------------------------------------------------------\n");
    fprintf(fp, "%-48s ₹%11.2f\n", "ITEMS SUBTOTAL:", order->subtotal);
    fprintf(fp, "%-48s ₹%11.2f\n", "ESTIMATED GST (5%):", order->tax);
    fprintf(fp, "%-48s ₹%11.2f\n", "DELIVERY CHARGE:", order->delivery_fee);
    fprintf(fp, "================================================================================\n");
    fprintf(fp, "%-48s ₹%11.2f\n", "GRAND TOTAL:", order->total);
    fprintf(fp, "================================================================================\n\n");

    fclose(fp);
    printf("[PERSISTENCE] Order %s saved to '%s' successfully.\n", order->order_id, ORDERS_FILE_PATH);
    return 1;
}

/* ========================================================================= */
/* HTTP Response Transmission Helpers                                        */
/* ========================================================================= */

/**
 * Helper to ensure complete transmission across TCP streams in case of partial sends.
 */
static int send_all(socket_t fd, const char *data, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        int sent = send(fd, data + total_sent, (int)(len - total_sent), 0);
        if (sent <= 0) return 0;
        total_sent += sent;
    }
    return 1;
}

/**
 * Sends a standard HTTP/1.1 response with custom status code, MIME type, and CORS headers.
 */
void send_http_response(socket_t client_fd, int status_code, const char *status_text,
                        const char *content_type, const char *body, size_t body_len) {
    char header_buffer[1024];
    int header_len = snprintf(header_buffer, sizeof(header_buffer),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    /* Send HTTP response headers */
    send_all(client_fd, header_buffer, (size_t)header_len);

    /* Send response payload body */
    if (body && body_len > 0) {
        send_all(client_fd, body, body_len);
    }
}

/**
 * Helper to send a JSON formatted response with 200/400/500 code.
 */
void send_json_response(socket_t client_fd, int status_code, const char *json_body) {
    const char *status_text = (status_code == 200) ? "OK" :
                              (status_code == 201) ? "Created" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 404) ? "Not Found" : "Internal Server Error";
    send_http_response(client_fd, status_code, status_text, "application/json", json_body, strlen(json_body));
}

/**
 * Sends standard HTTP 404 Not Found error response.
 */
void send_404_response(socket_t client_fd) {
    const char *msg = "{\"error\": \"Resource Not Found\", \"code\": 404}";
    send_http_response(client_fd, 404, "Not Found", "application/json", msg, strlen(msg));
}

/**
 * Sends standard HTTP 500 Internal Server Error response.
 */
void send_500_response(socket_t client_fd, const char *error_msg) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "{\"error\": \"%s\", \"code\": 500}", error_msg);
    send_http_response(client_fd, 500, "Internal Server Error", "application/json", buffer, strlen(buffer));
}

/* ========================================================================= */
/* Route Handlers                                                            */
/* ========================================================================= */

/**
 * Route: GET /
 * Serves the modern interactive frontend UI from index.html (or embedded fallback).
 */
void route_serve_index(socket_t client_fd) {
    FILE *fp = fopen(INDEX_HTML_PATH, "rb");
    if (!fp) {
        printf("[HTTP] 'index.html' not found on disk. Serving embedded fallback HTML.\n");
        send_http_response(client_fd, 200, "OK", "text/html", EMBEDDED_FALLBACK_HTML, strlen(EMBEDDED_FALLBACK_HTML));
        return;
    }

    /* Determine file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        send_500_response(client_fd, "Failed to read index.html");
        return;
    }

    char *html_buffer = (char *)malloc((size_t)file_size + 1);
    if (!html_buffer) {
        fclose(fp);
        send_500_response(client_fd, "Memory allocation error");
        return;
    }

    size_t bytes_read = fread(html_buffer, 1, (size_t)file_size, fp);
    html_buffer[bytes_read] = '\0';
    fclose(fp);

    send_http_response(client_fd, 200, "OK", "text/html", html_buffer, bytes_read);
    free(html_buffer);
}

/**
 * Route: GET /api/menu
 * Serializes the in-memory menu items array into a structured JSON payload.
 */
void route_get_menu(socket_t client_fd) {
    /* Reload latest menu catalog from file persistence */
    load_menu_from_file(MENU_FILE_PATH);

    /* Allocate buffer for JSON construction */
    size_t alloc_size = 32768;
    char *json = (char *)malloc(alloc_size);
    if (!json) {
        send_500_response(client_fd, "Memory allocation failed");
        return;
    }

    strcpy(json, "{\"success\":true,\"count\":");
    char num_buf[16];
    snprintf(num_buf, sizeof(num_buf), "%d", g_menu_count);
    strcat(json, num_buf);
    strcat(json, ",\"items\":[");

    for (int i = 0; i < g_menu_count; i++) {
        char item_buf[1024];
        snprintf(item_buf, sizeof(item_buf),
            "%s{\"id\":%d,\"name\":\"%s\",\"category\":\"%s\",\"price\":%.2f,"
            "\"description\":\"%s\",\"badge\":\"%s\",\"imageUrl\":\"%s\"}",
            (i > 0 ? "," : ""),
            g_menu[i].id,
            g_menu[i].name,
            g_menu[i].category,
            g_menu[i].price,
            g_menu[i].description,
            g_menu[i].badge,
            g_menu[i].image_url
        );

        if (strlen(json) + strlen(item_buf) + 10 > alloc_size) {
            alloc_size *= 2;
            char *new_json = (char *)realloc(json, alloc_size);
            if (!new_json) {
                free(json);
                send_500_response(client_fd, "Out of memory during menu serialization");
                return;
            }
            json = new_json;
        }
        strcat(json, item_buf);
    }

    strcat(json, "]}");
    send_json_response(client_fd, 200, json);
    free(json);
}

/* ========================================================================= */
/* JSON Parsing Helpers for Order Submissions                                */
/* ========================================================================= */

/**
 * Minimalist, safe JSON string field extractor.
 * Searches for "key": "value" in a JSON string, handling arbitrary whitespace and newlines.
 */
static int extract_json_string(const char *json, const char *key, char *out_val, size_t max_len) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (!pos) return 0;

    pos += strlen(pattern);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\r' || *pos == '\n')) pos++;

    if (*pos != '\"') return 0;
    pos++; /* Skip opening quote */

    size_t i = 0;
    while (*pos && *pos != '\"' && i < max_len - 1) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++; /* Handle basic escape */
        }
        out_val[i++] = *pos++;
    }
    out_val[i] = '\0';
    return 1;
}

/**
 * Route: POST /api/order
 * Parses incoming JSON order payload, matches items from menu, calculates totals,
 * saves to orders.txt, and sends back an itemized invoice confirmation.
 */
void route_post_order(socket_t client_fd, const char *body) {
    if (!body || strlen(body) == 0) {
        send_json_response(client_fd, 400, "{\"error\": \"Empty order body received\"}");
        return;
    }

    Order new_order;
    memset(&new_order, 0, sizeof(new_order));

    /* 1. Extract Customer Info */
    if (!extract_json_string(body, "username", new_order.username, sizeof(new_order.username))) {
        strcpy(new_order.username, "guest");
    }
    if (!extract_json_string(body, "customerName", new_order.customer_name, sizeof(new_order.customer_name))) {
        strcpy(new_order.customer_name, "Valued Customer");
    }
    if (!extract_json_string(body, "customerPhone", new_order.customer_phone, sizeof(new_order.customer_phone))) {
        strcpy(new_order.customer_phone, "N/A");
    }
    if (!extract_json_string(body, "deliveryAddress", new_order.delivery_address, sizeof(new_order.delivery_address))) {
        strcpy(new_order.delivery_address, "Dine-in / Pickup");
    }
    extract_json_string(body, "notes", new_order.notes, sizeof(new_order.notes));

    /* 2. Parse items array: "items": [ {"id": 1, "qty": 2}, ... ] */
    const char *items_start = strstr(body, "\"items\"");
    if (!items_start) {
        send_json_response(client_fd, 400, "{\"error\": \"Order payload missing 'items' array\"}");
        return;
    }

    items_start = strchr(items_start, '[');
    if (!items_start) {
        send_json_response(client_fd, 400, "{\"error\": \"Invalid items array format\"}");
        return;
    }

    const char *ptr = items_start + 1;
    new_order.item_count = 0;
    new_order.subtotal = 0.0;

    while (*ptr && *ptr != ']' && new_order.item_count < MAX_ORDER_ITEMS) {
        /* Search for next object { ... } */
        const char *obj_start = strchr(ptr, '{');
        if (!obj_start) break;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        /* Extract id and qty within this object */
        char obj_str[256];
        size_t len = (size_t)(obj_end - obj_start + 1);
        if (len >= sizeof(obj_str)) len = sizeof(obj_str) - 1;
        strncpy(obj_str, obj_start, len);
        obj_str[len] = '\0';

        int item_id = 0;
        int qty = 1;

        const char *id_pos = strstr(obj_str, "\"id\"");
        if (id_pos) {
            const char *colon = strchr(id_pos, ':');
            if (colon) item_id = atoi(colon + 1);
        }

        const char *qty_pos = strstr(obj_str, "\"qty\"");
        if (!qty_pos) qty_pos = strstr(obj_str, "\"quantity\"");
        if (qty_pos) {
            const char *colon = strchr(qty_pos, ':');
            if (colon) qty = atoi(colon + 1);
        }

        if (item_id > 0 && qty > 0) {
            const MenuItem *menu_item = find_menu_item(item_id);
            if (menu_item) {
                OrderItem *oi = &new_order.items[new_order.item_count];
                oi->item_id = menu_item->id;
                snprintf(oi->name, sizeof(oi->name), "%s", menu_item->name);
                oi->unit_price = menu_item->price;
                oi->quantity = qty;
                oi->subtotal = menu_item->price * qty;

                new_order.subtotal += oi->subtotal;
                new_order.item_count++;
            }
        }

        ptr = obj_end + 1;
    }

    if (new_order.item_count == 0) {
        send_json_response(client_fd, 400, "{\"error\": \"No valid menu items in order\"}");
        return;
    }

    /* 3. Compute Financial Totals */
    new_order.tax = new_order.subtotal * TAX_RATE;
    new_order.delivery_fee = (new_order.subtotal >= FREE_DELIVERY_MIN) ? 0.0 : STANDARD_DELIVERY;
    new_order.total = new_order.subtotal + new_order.tax + new_order.delivery_fee;

    generate_order_id(new_order.order_id, sizeof(new_order.order_id));
    get_current_timestamp(new_order.timestamp, sizeof(new_order.timestamp));
    strcpy(new_order.status, "Confirmed");

    /* 4. Persist to orders.txt */
    save_order_to_file(&new_order);

    /* 5. Format Confirmation Invoice JSON */
    size_t invoice_size = 16384;
    char *invoice_json = (char *)malloc(invoice_size);
    if (!invoice_json) {
        send_500_response(client_fd, "Memory allocation failure generating invoice");
        return;
    }

    snprintf(invoice_json, invoice_size,
        "{"
        "\"success\": true,"
        "\"orderId\": \"%s\","
        "\"username\": \"%s\","
        "\"timestamp\": \"%s\","
        "\"status\": \"%s\","
        "\"customer\": {"
            "\"name\": \"%s\","
            "\"phone\": \"%s\","
            "\"address\": \"%s\","
            "\"notes\": \"%s\""
        "},"
        "\"pricing\": {"
            "\"subtotal\": %.2f,"
            "\"tax\": %.2f,"
            "\"taxRate\": \"5%%\","
            "\"deliveryFee\": %.2f,"
            "\"freeDeliveryApplied\": %s,"
            "\"total\": %.2f"
        "},"
        "\"items\": [",
        new_order.order_id,
        new_order.username,
        new_order.timestamp,
        new_order.status,
        new_order.customer_name,
        new_order.customer_phone,
        new_order.delivery_address,
        new_order.notes,
        new_order.subtotal,
        new_order.tax,
        new_order.delivery_fee,
        (new_order.delivery_fee == 0.0) ? "true" : "false",
        new_order.total
    );

    for (int i = 0; i < new_order.item_count; i++) {
        char item_entry[256];
        snprintf(item_entry, sizeof(item_entry),
            "%s{\"id\": %d, \"name\": \"%s\", \"unitPrice\": %.2f, \"quantity\": %d, \"subtotal\": %.2f}",
            (i > 0 ? "," : ""),
            new_order.items[i].item_id,
            new_order.items[i].name,
            new_order.items[i].unit_price,
            new_order.items[i].quantity,
            new_order.items[i].subtotal
        );
        strcat(invoice_json, item_entry);
    }

    strcat(invoice_json, "]}");

    send_json_response(client_fd, 201, invoice_json);
    free(invoice_json);
}

/**
 * Route: GET /api/orders
 * Reads the orders.txt file and returns the plain text / log so the client can inspect history.
 */
void route_get_orders(socket_t client_fd) {
    FILE *fp = fopen(ORDERS_FILE_PATH, "r");
    if (!fp) {
        send_http_response(client_fd, 200, "OK", "text/plain", "No orders have been recorded yet.", 35);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        send_http_response(client_fd, 200, "OK", "text/plain", "No orders recorded yet.", 23);
        return;
    }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        send_500_response(client_fd, "Memory allocation failure");
        return;
    }

    size_t read_bytes = fread(buf, 1, (size_t)size, fp);
    buf[read_bytes] = '\0';
    fclose(fp);

    send_http_response(client_fd, 200, "OK", "text/plain", buf, read_bytes);
    free(buf);
}

/**
 * Route: POST /api/signup
 * Handles customer registration.
 */
void route_post_signup(socket_t client_fd, const char *body) {
    User u;
    memset(&u, 0, sizeof(u));
    extract_json_string(body, "username", u.username, sizeof(u.username));
    extract_json_string(body, "password", u.password, sizeof(u.password));
    extract_json_string(body, "name", u.name, sizeof(u.name));
    extract_json_string(body, "phone", u.phone, sizeof(u.phone));
    extract_json_string(body, "address", u.address, sizeof(u.address));

    if (strlen(u.username) < 3 || strlen(u.password) < 4 || strlen(u.name) == 0) {
        send_json_response(client_fd, 400, "{\"success\":false,\"error\":\"Username must be at least 3 characters and password at least 4 characters.\"}");
        return;
    }

    if (!register_user(&u)) {
        send_json_response(client_fd, 409, "{\"success\":false,\"error\":\"Username is already registered. Please choose another username.\"}");
        return;
    }

    char json[1024];
    snprintf(json, sizeof(json),
        "{\"success\":true,\"user\":{\"username\":\"%s\",\"name\":\"%s\",\"phone\":\"%s\",\"address\":\"%s\"}}",
        u.username, u.name, u.phone, u.address);
    send_json_response(client_fd, 201, json);
}

/**
 * Route: POST /api/login
 * Validates credentials and returns authenticated user profile.
 */
void route_post_login(socket_t client_fd, const char *body) {
    char username[64] = {0};
    char password[64] = {0};
    extract_json_string(body, "username", username, sizeof(username));
    extract_json_string(body, "password", password, sizeof(password));

    const User *u = authenticate_user(username, password);
    if (!u) {
        send_json_response(client_fd, 401, "{\"success\":false,\"error\":\"Invalid username or password. Please try again.\"}");
        return;
    }

    char json[1024];
    snprintf(json, sizeof(json),
        "{\"success\":true,\"user\":{\"username\":\"%s\",\"name\":\"%s\",\"phone\":\"%s\",\"address\":\"%s\"}}",
        u->username, u->name, u->phone, u->address);
    send_json_response(client_fd, 200, json);
}

/**
 * Route: GET /api/user/orders?username=... OR POST /api/user/orders
 * Returns customer-specific order history parsed from orders.txt.
 */
void route_get_user_orders(socket_t client_fd, const char *username_filter) {
    FILE *fp = fopen(ORDERS_FILE_PATH, "r");
    if (!fp) {
        send_json_response(client_fd, 200, "{\"success\":true,\"orders\":[]}");
        return;
    }

    size_t alloc_size = 32768;
    char *json = (char *)malloc(alloc_size);
    if (!json) {
        fclose(fp);
        send_500_response(client_fd, "Memory allocation error");
        return;
    }

    snprintf(json, alloc_size, "{\"success\":true,\"username\":\"%s\",\"orders\":[", username_filter);

    char line[512];
    char cur_order_id[64] = {0};
    char cur_user[64] = {0};
    char cur_date[64] = {0};
    char cur_status[32] = {0};
    char cur_cust[96] = {0};
    char cur_total[32] = {0};
    char items_summary[512] = {0};
    int in_items = 0;
    int order_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        trim_string(line);

        if (strncmp(line, "ORDER INVOICE:", 14) == 0) {
            /* Flush previous order if matching filter */
            /* Flush previous order if matching filter */
            int matches = 0;
            if (strcmp(username_filter, "all") == 0) {
                matches = 1;
            } else if (strlen(cur_user) > 0 && strcmp(cur_user, username_filter) == 0) {
                matches = 1;
            }

            if (strlen(cur_order_id) > 0 && matches) {
                char entry[1024];
                snprintf(entry, sizeof(entry),
                    "%s{\"orderId\":\"%s\",\"username\":\"%s\",\"timestamp\":\"%s\",\"status\":\"%s\",\"customerName\":\"%s\",\"total\":\"%s\",\"items\":\"%s\"}",
                    (order_count > 0 ? "," : ""),
                    cur_order_id, cur_user, cur_date, cur_status, cur_cust, cur_total, items_summary);
                if (strlen(json) + strlen(entry) + 10 > alloc_size) {
                    alloc_size *= 2;
                    char *new_j = (char *)realloc(json, alloc_size);
                    if (new_j) json = new_j;
                }
                strcat(json, entry);
                order_count++;
            }

            /* Reset for next block */
            memset(cur_order_id, 0, sizeof(cur_order_id));
            memset(cur_user, 0, sizeof(cur_user));
            memset(cur_date, 0, sizeof(cur_date));
            memset(cur_status, 0, sizeof(cur_status));
            memset(cur_cust, 0, sizeof(cur_cust));
            memset(cur_total, 0, sizeof(cur_total));
            memset(items_summary, 0, sizeof(items_summary));
            in_items = 0;

            const char *p = line + 14;
            while (*p == ' ') p++;
            snprintf(cur_order_id, sizeof(cur_order_id), "%s", p);
        } else if (strncmp(line, "USER ID", 7) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ') colon++;
                snprintf(cur_user, sizeof(cur_user), "%s", colon);
            }
        } else if (strncmp(line, "DATE / TIME", 11) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ') colon++;
                snprintf(cur_date, sizeof(cur_date), "%s", colon);
            }
        } else if (strncmp(line, "STATUS", 6) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ') colon++;
                snprintf(cur_status, sizeof(cur_status), "%s", colon);
            }
        } else if (strncmp(line, "CUSTOMER", 8) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ') colon++;
                snprintf(cur_cust, sizeof(cur_cust), "%s", colon);
            }
        } else if (strncmp(line, "ITEM DESCRIPTION", 16) == 0) {
            in_items = 1;
        } else if (strncmp(line, "ITEMS SUBTOTAL:", 15) == 0) {
            in_items = 0;
        } else if (strncmp(line, "GRAND TOTAL:", 12) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '$' || (unsigned char)*colon >= 128) colon++;
                snprintf(cur_total, sizeof(cur_total), "%s", colon);
            }
        } else if (in_items && strlen(line) > 0 && line[0] != '-') {
            if (strlen(items_summary) > 0) strcat(items_summary, " | ");
            strncat(items_summary, line, 64);
        }
    }

    /* Flush final block */
    int final_match = 0;
    if (strcmp(username_filter, "all") == 0) {
        final_match = 1;
    } else if (strlen(cur_user) > 0 && strcmp(cur_user, username_filter) == 0) {
        final_match = 1;
    }

    if (strlen(cur_order_id) > 0 && final_match) {
        char entry[1024];
        snprintf(entry, sizeof(entry),
            "%s{\"orderId\":\"%s\",\"username\":\"%s\",\"timestamp\":\"%s\",\"status\":\"%s\",\"customerName\":\"%s\",\"total\":\"%s\",\"items\":\"%s\"}",
            (order_count > 0 ? "," : ""),
            cur_order_id, cur_user, cur_date, cur_status, cur_cust, cur_total, items_summary);
        strcat(json, entry);
        order_count++;
    }

    strcat(json, "]}");
    fclose(fp);
    send_json_response(client_fd, 200, json);
    free(json);
}

/* ========================================================================= */
/* Client Connection & HTTP Request Parser                                   */
/* ========================================================================= */

/**
 * Handles a single client connection:
 *  1. Receives raw HTTP request over TCP socket
 *  2. Parses HTTP method, URI path, and body
 *  3. Dispatches to the appropriate route handler
 *  4. Gracefully closes the connection
 */
void handle_client(socket_t client_fd) {
    char buffer[BUFFER_SIZE];
    int total_bytes = 0;
    int content_length = 0;
    char *header_end = NULL;

    /* 1. Read from TCP socket until we encounter the header terminator (\r\n\r\n or \n\n) */
    while (total_bytes < BUFFER_SIZE - 1) {
        int bytes = recv(client_fd, buffer + total_bytes, sizeof(buffer) - 1 - total_bytes, 0);
        if (bytes <= 0) break;
        total_bytes += bytes;
        buffer[total_bytes] = '\0';

        header_end = strstr(buffer, "\r\n\r\n");
        if (!header_end) header_end = strstr(buffer, "\n\n");
        if (header_end) break;
    }

    if (!header_end || total_bytes <= 0) {
        CLOSE_SOCKET(client_fd);
        return;
    }

    /* 2. Check for Expect: 100-continue (standard in PowerShell, curl, HttpClient) */
    char *expect_header = strstr(buffer, "Expect: 100-continue");
    if (!expect_header) expect_header = strstr(buffer, "expect: 100-continue");
    if (expect_header) {
        const char *cont_resp = "HTTP/1.1 100 Continue\r\n\r\n";
        send(client_fd, cont_resp, (int)strlen(cont_resp), 0);
    }

    /* 3. Extract Content-Length for POST payloads */
    char *cl_header = strstr(buffer, "Content-Length:");
    if (!cl_header) cl_header = strstr(buffer, "content-length:");
    if (cl_header) {
        content_length = atoi(cl_header + 15);
    }

    /* 4. Determine body start pointer */
    char *body = NULL;
    if (strstr(header_end, "\r\n\r\n") == header_end) {
        body = header_end + 4;
    } else {
        body = header_end + 2;
    }

    /* 5. Ensure all body bytes are received based on Content-Length */
    int body_received = total_bytes - (int)(body - buffer);
    while (content_length > 0 && body_received < content_length && total_bytes < BUFFER_SIZE - 1) {
        int bytes = recv(client_fd, buffer + total_bytes, sizeof(buffer) - 1 - total_bytes, 0);
        if (bytes <= 0) break;
        total_bytes += bytes;
        buffer[total_bytes] = '\0';
        body_received += bytes;
    }

    /* Extract Request Line: e.g. "GET /api/menu HTTP/1.1" */
    char method[16] = {0};
    char path[256] = {0};
    char protocol[32] = {0};

    if (sscanf(buffer, "%15s %255s %31s", method, path, protocol) < 2) {
        send_http_response(client_fd, 400, "Bad Request", "text/plain", "Malformed HTTP request", 22);
        CLOSE_SOCKET(client_fd);
        return;
    }

    printf("[HTTP REQUEST] %-6s %s (body: %d bytes)\n", method, path, body_received);

    /* Handle CORS Preflight OPTIONS */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client_fd, 204, "No Content", "text/plain", "", 0);
        CLOSE_SOCKET(client_fd);
        return;
    }

    /* Route Dispatching */
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            route_serve_index(client_fd);
        } else if (strcmp(path, "/api/menu") == 0) {
            route_get_menu(client_fd);
        } else if (strcmp(path, "/api/orders") == 0) {
            route_get_orders(client_fd);
        } else if (strncmp(path, "/api/user/orders", 16) == 0) {
            const char *q = strstr(path, "username=");
            if (q) {
                route_get_user_orders(client_fd, q + 9);
            } else {
                route_get_user_orders(client_fd, "all");
            }
        } else {
            send_404_response(client_fd);
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/api/order") == 0) {
            route_post_order(client_fd, body);
        } else if (strcmp(path, "/api/signup") == 0 || strcmp(path, "/api/register") == 0) {
            route_post_signup(client_fd, body);
        } else if (strcmp(path, "/api/login") == 0) {
            route_post_login(client_fd, body);
        } else if (strcmp(path, "/api/user/orders") == 0) {
            char ufilter[64] = {0};
            extract_json_string(body, "username", ufilter, sizeof(ufilter));
            route_get_user_orders(client_fd, ufilter);
        } else {
            send_404_response(client_fd);
        }
    } else {
        send_http_response(client_fd, 405, "Method Not Allowed", "text/plain", "Method not supported", 20);
    }

#ifdef _WIN32
    shutdown(client_fd, SD_SEND);
#else
    shutdown(client_fd, SHUT_WR);
#endif
    CLOSE_SOCKET(client_fd);
}

/* ========================================================================= */
/* Server Main Event Loop                                                    */
/* ========================================================================= */

/**
 * Runs the main listening and accepting loop for incoming HTTP requests.
 */
void run_server(int port) {
    socket_t server_fd = create_server_socket(port);
    if (!IS_VALID_SOCKET(server_fd)) {
        fprintf(stderr, "[FATAL] Failed to initialize server socket on port %d.\n", port);
        return;
    }

    printf("\n==========================================================\n");
    printf("   ONLINE FOOD ORDERING SYSTEM - HTTP SERVER IN C         \n");
    printf("==========================================================\n");
    printf(" [x] Server status : RUNNING (TCP Socket)\n");
    printf(" [x] Listening on  : http://localhost:%d\n", port);
    printf(" [x] Web Interface : http://localhost:%d/\n", port);
    printf(" [x] API Menu      : http://localhost:%d/api/menu\n", port);
    printf(" [x] API Orders    : http://localhost:%d/api/orders\n", port);
    printf(" [x] API Auth      : http://localhost:%d/api/login & /api/signup\n", port);
    printf(" [x] Press Ctrl+C in this terminal to stop the server.\n");
    printf("==========================================================\n\n");

    while (1) {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif

        socket_t client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (!IS_VALID_SOCKET(client_fd)) {
            fprintf(stderr, "[WARN] accept() failed: %d\n", GET_SOCKET_ERROR());
            continue;
        }

        /* Handle client request (synchronous for simplicity and clarity) */
        handle_client(client_fd);
    }

    CLOSE_SOCKET(server_fd);
}

/* ========================================================================= */
/* Program Entry Point                                                       */
/* ========================================================================= */

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;

    /* Allow custom port from command line argument (e.g. ./server 8081) */
    if (argc > 1) {
        int custom_port = atoi(argv[1]);
        if (custom_port > 1024 && custom_port <= 65535) {
            port = custom_port;
        }
    }

    srand((unsigned int)time(NULL));

    /* Initialize networking library */
    if (!init_networking()) {
        return 1;
    }

    /* Load menu items and registered users from file persistence */
    load_menu_from_file(MENU_FILE_PATH);
    load_users_from_file(USERS_FILE_PATH);

    /* Start HTTP server event loop */
    run_server(port);

    /* Cleanup sockets */
    cleanup_networking();
    return 0;
}

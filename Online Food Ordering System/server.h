/**
 * @file server.h
 * @brief Header file for Online Food Ordering System HTTP Web Server in C
 * 
 * Provides cross-platform networking abstraction (Windows Winsock2 & POSIX Sockets),
 * data structures for Menu Items, Orders, and HTTP handling functions.
 */

#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ========================================================================= */
/* Cross-Platform Socket Abstraction (Windows Winsock vs POSIX Sockets)      */
/* ========================================================================= */
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif

    typedef SOCKET socket_t;
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERROR() WSAGetLastError()
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <errno.h>

    typedef int socket_t;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERROR() (errno)
#endif

/* ========================================================================= */
/* Application Constants                                                     */
/* ========================================================================= */
#define SERVER_PORT         8080
#define BUFFER_SIZE         65536     /* 64 KB request/response buffer */
#define MAX_MENU_ITEMS      64        /* Maximum menu catalog capacity */
#define MAX_ORDER_ITEMS     32        /* Maximum items per single order */
#define TAX_RATE            0.05      /* 5% GST */
#define FREE_DELIVERY_MIN   199.00    /* Free delivery threshold (INR ₹199) */
#define STANDARD_DELIVERY   29.00     /* Flat delivery fee (INR ₹29) */

#define MENU_FILE_PATH      "menu.txt"
#define ORDERS_FILE_PATH    "orders.txt"
#define USERS_FILE_PATH     "users.txt"
#define INDEX_HTML_PATH     "index.html"
#define MAX_USERS           128

/* ========================================================================= */
/* Data Structures                                                           */
/* ========================================================================= */

/**
 * @brief Represents a registered customer user account.
 */
typedef struct {
    char username[64];
    char password[64];
    char name[96];
    char phone[32];
    char address[256];
} User;

/**
 * @brief Represents an individual food item available on the menu.
 */
typedef struct {
    int id;
    char name[96];
    char category[32];
    double price;
    char description[256];
    char badge[32];
    char image_url[256];
} MenuItem;

/**
 * @brief Represents a specific line item within a customer's order.
 */
typedef struct {
    int item_id;
    char name[96];
    double unit_price;
    int quantity;
    double subtotal;
} OrderItem;

/**
 * @brief Represents a complete customer order invoice with financial totals.
 */
typedef struct {
    char order_id[32];
    char username[64];             /* Linked customer username ("guest" if unauthenticated) */
    char customer_name[96];
    char customer_phone[32];
    char delivery_address[256];
    char notes[128];
    OrderItem items[MAX_ORDER_ITEMS];
    int item_count;
    double subtotal;
    double tax;
    double delivery_fee;
    double total;
    char timestamp[32];
    char status[32];
} Order;

/* ========================================================================= */
/* Global / Shared State Prototypes                                          */
/* ========================================================================= */
extern MenuItem g_menu[MAX_MENU_ITEMS];
extern int g_menu_count;
extern User g_users[MAX_USERS];
extern int g_user_count;

/* ========================================================================= */
/* Function Prototypes                                                       */
/* ========================================================================= */

/* Networking & Lifecycle */
int init_networking(void);
void cleanup_networking(void);
socket_t create_server_socket(int port);
void run_server(int port);
void handle_client(socket_t client_fd);

/* Data Persistence & Business Logic */
int load_menu_from_file(const char *filename);
int load_users_from_file(const char *filename);
int register_user(const User *user);
const User* authenticate_user(const char *username, const char *password);
int save_order_to_file(const Order *order);
const MenuItem* find_menu_item(int id);
void generate_order_id(char *out_id, size_t max_len);
void get_current_timestamp(char *out_time, size_t max_len);

/* HTTP Request Parsing & Routing */
void send_http_response(socket_t client_fd, int status_code, const char *status_text,
                        const char *content_type, const char *body, size_t body_len);
void send_json_response(socket_t client_fd, int status_code, const char *json_body);
void send_404_response(socket_t client_fd);
void send_500_response(socket_t client_fd, const char *error_msg);

/* Route Handlers */
void route_serve_index(socket_t client_fd);
void route_get_menu(socket_t client_fd);
void route_post_order(socket_t client_fd, const char *body);
void route_get_orders(socket_t client_fd);
void route_post_signup(socket_t client_fd, const char *body);
void route_post_login(socket_t client_fd, const char *body);
void route_get_user_orders(socket_t client_fd, const char *username_filter);

#endif /* SERVER_H */

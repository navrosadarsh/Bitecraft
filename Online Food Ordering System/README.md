# BiteCraft Gourmet: Online Food Ordering System in Pure C

A complete, production-grade, and modern Online Food Ordering System where the backend server, network sockets, routing, business logic, and order persistence are written **entirely in standard C** (using native POSIX and Windows Winsock2 sockets).

---

## 🌟 Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture Diagram](#architecture-diagram)
3. [Project File Structure](#project-file-structure)
4. [Core Data Structures in C](#core-data-structures-in-c)
5. [Quickstart & Compilation Guide](#quickstart--compilation-guide)
   - [Windows (GCC / MinGW / Clang / MSVC)](#windows)
   - [Linux (GCC / Clang)](#linux)
   - [macOS (Clang / GCC)](#macos)
6. [REST API Specifications](#rest-api-specifications)
7. [File Persistence: `menu.txt` & `orders.txt`](#file-persistence)
8. [College Project & Viva Presentation Guide: "How C Talks to the Browser"](#college-project--viva-presentation-guide)

---

## 🍴 Project Overview

Traditional web development often abstracts away the inner workings of networks using high-level frameworks like Express, Django, or Spring Boot. This project strips away external dependencies to demonstrate **bare-metal systems programming**:

- **Standalone C HTTP Web Server**: Implements HTTP/1.1 directly over TCP/IP sockets (`socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()`).
- **Zero Third-Party Dependencies**: Compiles cleanly with only the C standard library (`stdio.h`, `stdlib.h`, `string.h`, `time.h`) and standard operating system socket libraries (`ws2_32` on Windows, standard libc on POSIX).
- **Cross-Platform Compatibility**: Automatically detects Windows (Winsock2) vs. Linux/macOS (POSIX Sockets) via conditional preprocessor macros.
- **Persistent File Storage**: Reads the food catalog from `menu.txt` and appends itemized financial invoices to `orders.txt`.
- **Modern Gourmet Web Interface**: Warm, responsive dark-mode UI with category pills, live search, dynamic cart sidebar with free delivery progress tracking, checkout modal, print-ready invoice receipt, and live `orders.txt` log stream.

---

## 📐 Architecture Diagram

```
+-----------------------------------------------------------------------------------+
|                            CLIENT BROWSER (UI/UX)                                 |
|  - Modern HTML5 / CSS3 / Vanilla JavaScript                                       |
|  - Category Filtering, Live Search, Dynamic Cart (+/-), Free Delivery Progress    |
|  - Checkout Form & Modal, Official Invoice Modal, Live orders.txt Log             |
+-----------------------------------------------------------------------------------+
                                   │           ▲
                HTTP Request       │           │  HTTP Response
         (GET /, POST /api/order)  ▼           │  (HTML, JSON, Plain Text)
+-----------------------------------------------------------------------------------+
|                        OPERATING SYSTEM SOCKET LAYER                              |
|           Windows Winsock2 (ws2_32.dll)   |   POSIX Sockets (sys/socket.h)        |
+-----------------------------------------------------------------------------------+
                                   │           ▲
                       recv()      │           │  send_all()
                                   ▼           │
+-----------------------------------------------------------------------------------+
|                        C HTTP WEB SERVER (server.c)                               |
|  1. Request Line Parser:    Extracts Method, Path, Protocol                       |
|  2. Header & Body Reader:   Reads Content-Length & Handles Expect: 100-continue   |
|  3. Route Dispatcher:                                                             |
|     - GET  /              -> Reads & streams 'index.html'                         |
|     - GET  /api/menu      -> Serializes g_menu[] into JSON array                  |
|     - POST /api/order     -> Parses JSON, calculates Tax & Totals, Saves Order    |
|     - GET  /api/orders    -> Reads & streams 'orders.txt'                         |
+-----------------------------------------------------------------------------------+
                                   │           ▲
                  fopen("a")       │           │  fopen("r")
                                   ▼           │
+-----------------------------------------------------------------------------------+
|                         FILE PERSISTENCE SUBSYSTEM                                |
|          menu.txt (Food Catalog)      │       orders.txt (Transaction Invoices)   |
+-----------------------------------------------------------------------------------+
```

---

## 📂 Project File Structure

```
Online Food Ordering System/
├── server.c         # Main C HTTP server, socket loop, request parser, JSON serializer & persistence
├── server.h         # Header file containing structs, cross-platform macros, and function prototypes
├── index.html       # Modern, responsive food ordering UI (HTML/CSS/JS embedded bundle)
├── menu.txt         # Pipe-delimited food menu catalog (id|name|category|price|desc|badge|image)
├── orders.txt       # Append-only persistent transaction audit log for customer invoices
├── build.bat        # Automated 1-click Windows build script (auto-detects GCC, Clang, or MSVC)
├── Makefile         # Standard cross-platform Makefile for Linux, macOS, and MSYS2
└── README.md        # Complete technical documentation & presentation guide
```

---

## 🧩 Core Data Structures in C (`server.h`)

### 1. `MenuItem`
Represents an individual food item available on the restaurant's menu:
```c
typedef struct {
    int id;
    char name[96];
    char category[32];      /* Fast Food, Main Course, Beverages, Desserts */
    double price;           /* Unit price in USD */
    char description[256];  /* Culinary description */
    char badge[32];         /* Chef's Special, Popular, Trending, etc. */
    char image_url[256];    /* High-res food photography link */
} MenuItem;
```

### 2. `OrderItem`
Represents a specific line item within a customer's order:
```c
typedef struct {
    int item_id;
    char name[96];
    double unit_price;
    int quantity;
    double subtotal;        /* unit_price * quantity */
} OrderItem;
```

### 3. `Order`
Represents a complete confirmed customer transaction invoice:
```c
typedef struct {
    char order_id[32];              /* e.g., ORD-20261022-7495 */
    char customer_name[96];
    char customer_phone[32];
    char delivery_address[256];
    char notes[128];
    OrderItem items[MAX_ORDER_ITEMS];
    int item_count;
    double subtotal;                /* Sum of item subtotals */
    double tax;                     /* 5% Sales Tax */
    double delivery_fee;            /* $0.00 if subtotal >= $30.00, else $2.99 */
    double total;                   /* subtotal + tax + delivery_fee */
    char timestamp[32];             /* ISO timestamp YYYY-MM-DD HH:MM:SS */
    char status[32];                /* "Confirmed", "In Kitchen", etc. */
} Order;
```

---

## 🚀 Quickstart & Compilation Guide

### Windows

#### Option A: 1-Click Automated Build Script
Simply double-click [`build.bat`](file:///e:/Online%20Food%20Ordering%20System/build.bat) or run from Command Prompt / PowerShell:
```cmd
build.bat
```

#### Option B: Manual GCC / MinGW
```cmd
gcc -O2 -Wall server.c -o server.exe -lws2_32
server.exe
```

#### Option C: Manual Clang
```cmd
clang -O2 -Wall server.c -o server.exe -lws2_32
server.exe
```

#### Option D: Microsoft Visual C++ (Developer Command Prompt)
```cmd
cl /O2 /W3 server.c ws2_32.lib /Fe:server.exe
server.exe
```

---

### Linux
```bash
# Using GCC
gcc -O2 -Wall -Wextra server.c -o server
./server

# Or using Make
make
make run
```

---

### macOS
```bash
# Using Clang
clang -O2 -Wall -Wextra server.c -o server
./server

# Or using Make
make
make run
```

Once running, navigate to **`http://localhost:8080`** in any web browser.

---

## 📡 REST API Specifications

### 1. `GET /`
- **Description**: Serves the modern interactive food ordering web app (`index.html`).
- **Response**: `200 OK`, `Content-Type: text/html; charset=UTF-8`.

---

### 2. `GET /api/menu`
- **Description**: Returns all food items loaded into server memory from `menu.txt`.
- **Response**: `200 OK`, `Content-Type: application/json`.
- **JSON Payload Example**:
```json
{
  "success": true,
  "count": 12,
  "items": [
    {
      "id": 1,
      "name": "Truffle Smash Burger",
      "category": "Fast Food",
      "price": 12.99,
      "description": "Double Angus beef patty, melted aged cheddar, truffle aioli...",
      "badge": "Chef's Special",
      "imageUrl": "https://images.unsplash.com/photo-1568901346375-23c9450c58cd?w=600"
    }
  ]
}
```

---

### 3. `POST /api/order`
- **Description**: Receives order selections, matches menu items, calculates totals, tax, and delivery fee, appends invoice to `orders.txt`, and returns an order receipt.
- **Request Headers**: `Content-Type: application/json`.
- **Request Body Example**:
```json
{
  "customerName": "Alexander Wright",
  "customerPhone": "+1 (555) 234-5678",
  "deliveryAddress": "88 Tech Boulevard, 4th Floor",
  "notes": "Leave at front desk",
  "items": [
    { "id": 1, "qty": 2 },
    { "id": 7, "qty": 1 }
  ]
}
```
- **Response**: `201 Created`, `Content-Type: application/json`.
- **Response Body Example**:
```json
{
  "success": true,
  "orderId": "ORD-20261022-7495",
  "timestamp": "2026-10-22 09:12:15",
  "status": "Confirmed",
  "customer": {
    "name": "Alexander Wright",
    "phone": "+1 (555) 234-5678",
    "address": "88 Tech Boulevard, 4th Floor",
    "notes": "Leave at front desk"
  },
  "pricing": {
    "subtotal": 30.93,
    "tax": 1.55,
    "taxRate": "5%",
    "deliveryFee": 0.00,
    "freeDeliveryApplied": true,
    "total": 32.48
  },
  "items": [
    { "id": 1, "name": "Truffle Smash Burger", "unitPrice": 12.99, "quantity": 2, "subtotal": 25.98 },
    { "id": 7, "name": "Cold Brew Vanilla Foam", "unitPrice": 4.95, "quantity": 1, "subtotal": 4.95 }
  ]
}
```

---

### 4. `POST /api/signup`
- **Description**: Registers a new customer account and persists it to `users.txt`.
- **Request Headers**: `Content-Type: application/json`.
- **Request Body Example**:
```json
{
  "name": "Marcus Vance",
  "username": "marcus",
  "password": "secretpassword",
  "phone": "+1 (555) 321-4321",
  "address": "120 Silicon Way, Apt 3A"
}
```
- **Response**: `201 Created` with authenticated user profile.

---

### 5. `POST /api/login`
- **Description**: Authenticates existing customer against `users.txt`.
- **Request Headers**: `Content-Type: application/json`.
- **Request Body Example**:
```json
{
  "username": "alex",
  "password": "password123"
}
```
- **Response**: `200 OK` with user profile or `401 Unauthorized` on mismatch.

---

### 6. `GET /api/user/orders?username=...`
- **Description**: Streams the list of verified, past orders placed exclusively by the specified customer.
- **Response**: `200 OK`, `Content-Type: application/json`.
- **Response Payload Example**:
```json
{
  "success": true,
  "username": "elena",
  "orders": [
    {
      "orderId": "ORD-20261022-1108",
      "username": "elena",
      "timestamp": "2026-10-22 09:25:59",
      "status": "Confirmed",
      "customerName": "Elena Rostova",
      "total": "28.44",
      "items": "Creamy Tuscan Fettuccine | New York Berry Cheesecake"
    }
  ]
}
```

---

### 7. `GET /api/orders`
- **Description**: Streams the full raw transaction audit log directly from `orders.txt`.
- **Response**: `200 OK`, `Content-Type: text/plain`.

---

## 💾 File Persistence

### 1. `menu.txt`
Structured text file maintaining menu items. Format:
```
id|name|category|price|description|badge|image_url
```
Example:
```text
1|Truffle Smash Burger|Fast Food|12.99|Double Angus beef patty, melted cheddar|Chef's Special|https://...
```

### 2. `orders.txt`
Human-readable and structured transaction ledger. New orders are automatically appended via `fopen(ORDERS_FILE_PATH, "a")`:
```text
================================================================================
ORDER INVOICE: ORD-20261022-7495
DATE / TIME  : 2026-10-22 09:12:15
STATUS       : Confirmed
CUSTOMER     : Prof. Robert Thorne
PHONE        : +1 (555) 789-0123
ADDRESS      : CS Department, Room 304, Turing Hall
NOTES        : Call upon arrival
--------------------------------------------------------------------------------
ITEM DESCRIPTION                          QTY   UNIT PRICE       SUBTOTAL
--------------------------------------------------------------------------------
Truffle Smash Burger                        2   $    12.99   $      25.98
Cold Brew Vanilla Foam                      1   $     4.95   $       4.95
--------------------------------------------------------------------------------
ITEMS SUBTOTAL:                                  $      30.93
ESTIMATED TAX (5%):                              $       1.55
DELIVERY CHARGE:                                 $       0.00
================================================================================
GRAND TOTAL:                                     $      32.48
================================================================================
```

---

## 🎓 College Project & Viva Presentation Guide

### "How C Talks to the Web Browser via HTTP & TCP Sockets"

Use this structured breakdown to explain the networking concepts during your project presentation or oral examination:

#### 1. The Core Concept: What is a Socket?
- A **socket** is an operating system abstraction representing an endpoint of a bidirectional communication channel over a network.
- In C, sockets are represented as file descriptors (`int` in Linux/POSIX) or handles (`SOCKET` unsigned integer in Windows).
- Reading from a socket (`recv()`) and writing to a socket (`send()`) mirrors standard Unix file I/O semantics.

#### 2. Network Initialization: Windows vs. POSIX
- **POSIX (Linux/macOS)**: Sockets are built into the kernel's standard C library. No special startup call is needed.
- **Windows**: Networking runs via the Windows Sockets DLL (`ws2_32.dll`). The program must call `WSAStartup(MAKEWORD(2, 2), &wsaData)` to initialize the networking subsystem before creating sockets, and `WSACleanup()` on termination.

#### 3. The 5-Step Server Lifecycle in C
1. **`socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)`**:
   Requests a socket from the OS using IPv4 (`AF_INET`) and reliable connection-oriented TCP stream semantics (`SOCK_STREAM`).
2. **`setsockopt(..., SO_REUSEADDR, ...)`**:
   Tells the OS kernel to permit immediate reuse of the local port address even if a previous connection is in the `TIME_WAIT` state.
3. **`bind(server_fd, &server_addr, sizeof(server_addr))`**:
   Associates the socket with a specific network interface (in our case, `INADDR_ANY` or `0.0.0.0`) and port `8080`.
4. **`listen(server_fd, backlog)`**:
   Transitions the socket from active (connecting) mode to passive (listening) mode with a connection queue backlog.
5. **`accept(server_fd, &client_addr, &addr_len)`**:
   Blocks execution until an incoming client initiates a **TCP 3-Way Handshake** (`SYN`, `SYN-ACK`, `ACK`). Upon completion, `accept()` returns a brand new `client_fd` dedicated solely to communicating with that specific browser tab.

#### 4. The Anatomy of an HTTP Request in C
When the user opens `http://localhost:8080`, the browser transmits plain text over the TCP stream:
```http
GET /api/menu HTTP/1.1
Host: localhost:8080
User-Agent: Mozilla/5.0...
Accept: application/json
\r\n
```
- In C, we read this stream into a buffer via `recv()`.
- The HTTP specification defines `\r\n\r\n` (CRLF CRLF) as the exact delimiter separating headers from the body.
- For `POST` requests, our C code extracts the `Content-Length:` header and continues reading `recv()` until the full JSON payload has been received into memory.

#### 5. Safe Transmission with `send_all()`
In stream sockets, `send()` is not guaranteed to send all bytes in a single call due to OS TCP window buffer limits. Our server implements `send_all()`:
```c
static int send_all(socket_t fd, const char *data, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        int sent = send(fd, data + total_sent, (int)(len - total_sent), 0);
        if (sent <= 0) return 0;
        total_sent += sent;
    }
    return 1;
}
```
This guarantees that large files like `index.html` (over 60 KB) are delivered completely without truncation or browser stalls.

#### 6. Graceful TCP Connection Teardown
Once the response is sent:
- We issue `shutdown(client_fd, SD_SEND)` to transmit the TCP `FIN` packet, notifying the browser that the response body is complete.
- We then call `closesocket(client_fd)` to release socket descriptors back to the operating system.

---

## 🏆 Key Strengths & Technical Highlights for Examiners
- **No external frameworks or runtime dependencies** (pure ANSI C + native OS sockets).
- **Graceful handling of real-world HTTP edge-cases** (`Expect: 100-continue`, chunked TCP stream reads, CORS headers).
- **Atomic file persistence** guaranteeing data survives server restarts.
- **Premium, modern consumer-grade UX** rivaling commercial food delivery applications.

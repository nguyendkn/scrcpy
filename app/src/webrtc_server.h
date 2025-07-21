#ifndef SC_WEBRTC_SERVER_H
#define SC_WEBRTC_SERVER_H

#include "common.h"

#ifdef HAVE_WEBRTC

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "trait/frame_sink.h"
#include "util/thread.h"
#include "util/net.h"

#define SC_WEBRTC_DEFAULT_PORT 8080
#define SC_WEBRTC_MAX_CLIENTS 10
#define SC_WEBRTC_BUFFER_SIZE 65536

struct sc_webrtc_client {
    int id;
    void *peer_connection;
    void *data_channel;
    bool connected;
    sc_socket socket;
    struct sc_webrtc_server *server;
};

struct sc_webrtc_server {
    sc_socket server_socket;
    uint16_t port;
    bool stopped;
    
    // Threading
    sc_thread thread;
    sc_mutex mutex;
    
    // Clients management
    struct sc_webrtc_client clients[SC_WEBRTC_MAX_CLIENTS];
    int client_count;
    
    // Frame sink for receiving video frames
    struct sc_frame_sink frame_sink;
    
    // Callbacks
    void (*on_client_connected)(struct sc_webrtc_server *server, int client_id);
    void (*on_client_disconnected)(struct sc_webrtc_server *server, int client_id);
    void (*on_error)(struct sc_webrtc_server *server, const char *error);
};

struct sc_webrtc_server_params {
    uint16_t port;
    const char *stun_server;
    const char *turn_server;
    const char *turn_username;
    const char *turn_password;
};

// Initialize WebRTC server
bool
sc_webrtc_server_init(struct sc_webrtc_server *server,
                      const struct sc_webrtc_server_params *params);

// Start WebRTC server
bool
sc_webrtc_server_start(struct sc_webrtc_server *server);

// Stop WebRTC server
void
sc_webrtc_server_stop(struct sc_webrtc_server *server);

// Join server thread
void
sc_webrtc_server_join(struct sc_webrtc_server *server);

// Destroy WebRTC server
void
sc_webrtc_server_destroy(struct sc_webrtc_server *server);

// Send video frame to all connected clients
bool
sc_webrtc_server_send_frame(struct sc_webrtc_server *server,
                            const struct sc_frame *frame);

// Handle HTTP requests (for signaling)
bool
sc_webrtc_server_handle_http(struct sc_webrtc_server *server,
                             sc_socket client_socket);

// WebSocket upgrade handling
bool
sc_webrtc_server_handle_websocket(struct sc_webrtc_server *server,
                                  sc_socket client_socket);

// Client management
int
sc_webrtc_server_add_client(struct sc_webrtc_server *server,
                            sc_socket client_socket);

void
sc_webrtc_server_remove_client(struct sc_webrtc_server *server, int client_id);

struct sc_webrtc_client *
sc_webrtc_server_get_client(struct sc_webrtc_server *server, int client_id);

// Frame sink implementation
static inline struct sc_frame_sink *
sc_webrtc_server_get_frame_sink(struct sc_webrtc_server *server) {
    return &server->frame_sink;
}

#endif // HAVE_WEBRTC

#endif // SC_WEBRTC_SERVER_H

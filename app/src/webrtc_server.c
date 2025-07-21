#include "webrtc_server.h"

#ifdef HAVE_WEBRTC

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rtc/rtc.h>

#include "util/log.h"
#include "util/str.h"

#define TAG "webrtc-server"

// HTTP response templates
static const char *HTTP_RESPONSE_TEMPLATE = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Content-Length: %zu\r\n"
    "\r\n"
    "%s";

static const char *HTML_CLIENT = 
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>scrcpy WebRTC Stream</title>\n"
    "    <style>\n"
    "        body { font-family: Arial, sans-serif; margin: 20px; }\n"
    "        video { width: 100%%; max-width: 800px; border: 1px solid #ccc; }\n"
    "        .controls { margin: 10px 0; }\n"
    "        button { padding: 10px 20px; margin: 5px; }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>scrcpy WebRTC Stream</h1>\n"
    "    <div class=\"controls\">\n"
    "        <button onclick=\"startStream()\">Start Stream</button>\n"
    "        <button onclick=\"stopStream()\">Stop Stream</button>\n"
    "        <span id=\"status\">Disconnected</span>\n"
    "    </div>\n"
    "    <video id=\"video\" autoplay playsinline muted></video>\n"
    "    <script>\n"
    "        let pc = null;\n"
    "        let ws = null;\n"
    "        const video = document.getElementById('video');\n"
    "        const status = document.getElementById('status');\n"
    "\n"
    "        function updateStatus(msg) {\n"
    "            status.textContent = msg;\n"
    "        }\n"
    "\n"
    "        function startStream() {\n"
    "            const wsUrl = `ws://${window.location.host}/ws`;\n"
    "            ws = new WebSocket(wsUrl);\n"
    "            \n"
    "            ws.onopen = () => {\n"
    "                updateStatus('WebSocket connected');\n"
    "                createPeerConnection();\n"
    "            };\n"
    "            \n"
    "            ws.onmessage = async (event) => {\n"
    "                const message = JSON.parse(event.data);\n"
    "                await handleSignalingMessage(message);\n"
    "            };\n"
    "            \n"
    "            ws.onclose = () => {\n"
    "                updateStatus('WebSocket disconnected');\n"
    "            };\n"
    "        }\n"
    "\n"
    "        function createPeerConnection() {\n"
    "            pc = new RTCPeerConnection({\n"
    "                iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]\n"
    "            });\n"
    "            \n"
    "            pc.ontrack = (event) => {\n"
    "                video.srcObject = event.streams[0];\n"
    "                updateStatus('Stream connected');\n"
    "            };\n"
    "            \n"
    "            pc.onicecandidate = (event) => {\n"
    "                if (event.candidate) {\n"
    "                    sendSignalingMessage({\n"
    "                        type: 'ice-candidate',\n"
    "                        candidate: event.candidate\n"
    "                    });\n"
    "                }\n"
    "            };\n"
    "            \n"
    "            // Request offer from server\n"
    "            sendSignalingMessage({ type: 'request-offer' });\n"
    "        }\n"
    "\n"
    "        async function handleSignalingMessage(message) {\n"
    "            switch (message.type) {\n"
    "                case 'offer':\n"
    "                    await pc.setRemoteDescription(message.offer);\n"
    "                    const answer = await pc.createAnswer();\n"
    "                    await pc.setLocalDescription(answer);\n"
    "                    sendSignalingMessage({\n"
    "                        type: 'answer',\n"
    "                        answer: answer\n"
    "                    });\n"
    "                    break;\n"
    "                case 'ice-candidate':\n"
    "                    await pc.addIceCandidate(message.candidate);\n"
    "                    break;\n"
    "            }\n"
    "        }\n"
    "\n"
    "        function sendSignalingMessage(message) {\n"
    "            if (ws && ws.readyState === WebSocket.OPEN) {\n"
    "                ws.send(JSON.stringify(message));\n"
    "            }\n"
    "        }\n"
    "\n"
    "        function stopStream() {\n"
    "            if (pc) {\n"
    "                pc.close();\n"
    "                pc = null;\n"
    "            }\n"
    "            if (ws) {\n"
    "                ws.close();\n"
    "                ws = null;\n"
    "            }\n"
    "            video.srcObject = null;\n"
    "            updateStatus('Disconnected');\n"
    "        }\n"
    "    </script>\n"
    "</body>\n"
    "</html>";

// WebSocket frame parsing
static bool
parse_websocket_frame(const uint8_t *data, size_t len, char **payload, size_t *payload_len) {
    if (len < 2) return false;
    
    uint8_t opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_length = data[1] & 0x7F;
    
    size_t header_len = 2;
    
    if (payload_length == 126) {
        if (len < 4) return false;
        payload_length = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_length == 127) {
        if (len < 10) return false;
        payload_length = 0;
        for (int i = 0; i < 8; i++) {
            payload_length = (payload_length << 8) | data[2 + i];
        }
        header_len = 10;
    }
    
    if (masked) header_len += 4;
    
    if (len < header_len + payload_length) return false;
    
    *payload = malloc(payload_length + 1);
    if (!*payload) return false;
    
    memcpy(*payload, data + header_len, payload_length);
    (*payload)[payload_length] = '\0';
    *payload_len = payload_length;
    
    if (masked) {
        uint8_t mask[4];
        memcpy(mask, data + header_len - 4, 4);
        for (size_t i = 0; i < payload_length; i++) {
            (*payload)[i] ^= mask[i % 4];
        }
    }
    
    return true;
}

// WebSocket frame creation
static bool
create_websocket_frame(const char *payload, size_t payload_len, uint8_t **frame, size_t *frame_len) {
    size_t header_len = 2;
    
    if (payload_len > 65535) {
        header_len = 10;
    } else if (payload_len > 125) {
        header_len = 4;
    }
    
    *frame_len = header_len + payload_len;
    *frame = malloc(*frame_len);
    if (!*frame) return false;
    
    (*frame)[0] = 0x81; // FIN + text frame
    
    if (payload_len > 65535) {
        (*frame)[1] = 127;
        for (int i = 0; i < 8; i++) {
            (*frame)[2 + i] = (payload_len >> (8 * (7 - i))) & 0xFF;
        }
    } else if (payload_len > 125) {
        (*frame)[1] = 126;
        (*frame)[2] = (payload_len >> 8) & 0xFF;
        (*frame)[3] = payload_len & 0xFF;
    } else {
        (*frame)[1] = payload_len & 0x7F;
    }
    
    memcpy(*frame + header_len, payload, payload_len);
    return true;
}

// Frame sink implementation
static bool
webrtc_frame_sink_open(struct sc_frame_sink *sink, const AVCodecContext *ctx) {
    struct sc_webrtc_server *server = DOWNCAST(sink, struct sc_webrtc_server, frame_sink);
    (void) server;
    (void) ctx;
    
    LOGD("WebRTC frame sink opened");
    return true;
}

static void
webrtc_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_webrtc_server *server = DOWNCAST(sink, struct sc_webrtc_server, frame_sink);
    (void) server;
    
    LOGD("WebRTC frame sink closed");
}

static bool
webrtc_frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct sc_webrtc_server *server = DOWNCAST(sink, struct sc_webrtc_server, frame_sink);
    
    // Convert AVFrame to format suitable for WebRTC
    // This is a simplified implementation - in practice you'd need proper encoding
    sc_mutex_lock(&server->mutex);
    
    for (int i = 0; i < server->client_count; i++) {
        struct sc_webrtc_client *client = &server->clients[i];
        if (client->connected && client->peer_connection) {
            // Send frame data through WebRTC data channel
            // Implementation depends on libdatachannel API
        }
    }
    
    sc_mutex_unlock(&server->mutex);
    return true;
}

static const struct sc_frame_sink_ops webrtc_frame_sink_ops = {
    .open = webrtc_frame_sink_open,
    .close = webrtc_frame_sink_close,
    .push = webrtc_frame_sink_push,
};

// HTTP request handling
bool
sc_webrtc_server_handle_http(struct sc_webrtc_server *server, sc_socket client_socket) {
    char buffer[4096];
    ssize_t received = net_recv(client_socket, buffer, sizeof(buffer) - 1);

    if (received <= 0) {
        return false;
    }

    buffer[received] = '\0';

    // Check for WebSocket upgrade
    if (strstr(buffer, "Upgrade: websocket")) {
        return sc_webrtc_server_handle_websocket(server, client_socket);
    }

    // Serve HTML client
    char response[strlen(HTML_CLIENT) + 512];
    snprintf(response, sizeof(response), HTTP_RESPONSE_TEMPLATE, strlen(HTML_CLIENT), HTML_CLIENT);

    ssize_t sent = net_send_all(client_socket, response, strlen(response));
    return sent > 0;
}

// WebSocket upgrade handling
bool
sc_webrtc_server_handle_websocket(struct sc_webrtc_server *server, sc_socket client_socket) {
    // WebSocket handshake response
    const char *websocket_response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        "\r\n";

    ssize_t sent = net_send_all(client_socket, websocket_response, strlen(websocket_response));
    if (sent <= 0) {
        return false;
    }

    // Add client
    int client_id = sc_webrtc_server_add_client(server, client_socket);
    if (client_id < 0) {
        return false;
    }

    LOGD("WebSocket client %d connected", client_id);

    if (server->on_client_connected) {
        server->on_client_connected(server, client_id);
    }

    return true;
}

// Client management
int
sc_webrtc_server_add_client(struct sc_webrtc_server *server, sc_socket client_socket) {
    sc_mutex_lock(&server->mutex);

    if (server->client_count >= SC_WEBRTC_MAX_CLIENTS) {
        sc_mutex_unlock(&server->mutex);
        return -1;
    }

    int client_id = server->client_count;
    struct sc_webrtc_client *client = &server->clients[client_id];

    client->id = client_id;
    client->socket = client_socket;
    client->connected = true;
    client->server = server;
    client->peer_connection = NULL;
    client->data_channel = NULL;

    server->client_count++;

    sc_mutex_unlock(&server->mutex);
    return client_id;
}

void
sc_webrtc_server_remove_client(struct sc_webrtc_server *server, int client_id) {
    sc_mutex_lock(&server->mutex);

    if (client_id >= 0 && client_id < server->client_count) {
        struct sc_webrtc_client *client = &server->clients[client_id];

        if (client->connected) {
            client->connected = false;
            net_close(client->socket);

            if (client->peer_connection) {
                // Clean up WebRTC peer connection
                rtcDeletePeerConnection(client->peer_connection);
                client->peer_connection = NULL;
            }

            LOGD("Client %d disconnected", client_id);

            if (server->on_client_disconnected) {
                server->on_client_disconnected(server, client_id);
            }
        }
    }

    sc_mutex_unlock(&server->mutex);
}

struct sc_webrtc_client *
sc_webrtc_server_get_client(struct sc_webrtc_server *server, int client_id) {
    if (client_id >= 0 && client_id < server->client_count) {
        return &server->clients[client_id];
    }
    return NULL;
}

// Server thread function
static int
run_webrtc_server(void *data) {
    struct sc_webrtc_server *server = data;

    LOGD("WebRTC server thread started on port %d", server->port);

    while (!server->stopped) {
        sc_socket client_socket = net_accept(server->server_socket);
        if (client_socket == SC_SOCKET_NONE) {
            if (!server->stopped) {
                LOGW("Failed to accept client connection");
            }
            continue;
        }

        // Handle client in current thread (simplified)
        // In production, you might want to use a thread pool
        bool handled = sc_webrtc_server_handle_http(server, client_socket);
        if (!handled) {
            net_close(client_socket);
        }
    }

    LOGD("WebRTC server thread stopped");
    return 0;
}

// Initialize WebRTC server
bool
sc_webrtc_server_init(struct sc_webrtc_server *server,
                      const struct sc_webrtc_server_params *params) {
    server->port = params->port ? params->port : SC_WEBRTC_DEFAULT_PORT;
    server->stopped = false;
    server->client_count = 0;

    // Initialize mutex
    bool ok = sc_mutex_init(&server->mutex);
    if (!ok) {
        return false;
    }

    // Initialize frame sink
    server->frame_sink.ops = &webrtc_frame_sink_ops;

    // Create server socket
    server->server_socket = net_socket();
    if (server->server_socket == SC_SOCKET_NONE) {
        LOGE("Could not create WebRTC server socket");
        sc_mutex_destroy(&server->mutex);
        return false;
    }

    // Bind and listen
    ok = net_listen(server->server_socket, IPV4_LOCALHOST, server->port, 5);
    if (!ok) {
        LOGE("Could not listen on WebRTC server socket");
        net_close(server->server_socket);
        sc_mutex_destroy(&server->mutex);
        return false;
    }

    LOGI("WebRTC server initialized on port %d", server->port);
    return true;
}

// Start WebRTC server
bool
sc_webrtc_server_start(struct sc_webrtc_server *server) {
    bool ok = sc_thread_create(&server->thread, run_webrtc_server, "webrtc-server", server);
    if (!ok) {
        LOGE("Could not create WebRTC server thread");
        return false;
    }

    LOGI("WebRTC server started on http://localhost:%d", server->port);
    return true;
}

// Stop WebRTC server
void
sc_webrtc_server_stop(struct sc_webrtc_server *server) {
    server->stopped = true;

    // Close all client connections
    sc_mutex_lock(&server->mutex);
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i].connected) {
            sc_webrtc_server_remove_client(server, i);
        }
    }
    sc_mutex_unlock(&server->mutex);

    // Close server socket to unblock accept()
    if (server->server_socket != SC_SOCKET_NONE) {
        net_close(server->server_socket);
        server->server_socket = SC_SOCKET_NONE;
    }
}

// Join server thread
void
sc_webrtc_server_join(struct sc_webrtc_server *server) {
    sc_thread_join(&server->thread, NULL);
}

// Destroy WebRTC server
void
sc_webrtc_server_destroy(struct sc_webrtc_server *server) {
    sc_mutex_destroy(&server->mutex);
}

// Send video frame to all connected clients
bool
sc_webrtc_server_send_frame(struct sc_webrtc_server *server,
                            const struct sc_frame *frame) {
    // This would be implemented with proper WebRTC video track
    // For now, just return true
    (void) server;
    (void) frame;
    return true;
}

#endif // HAVE_WEBRTC

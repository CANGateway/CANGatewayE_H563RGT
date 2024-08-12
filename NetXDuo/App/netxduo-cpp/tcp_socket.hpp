#pragma once

#include "main.h"
#include "nx_api.h"
#include "tx_api.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

extern NX_PACKET_POOL NxAppPool;

namespace netxduo {

#define ERROR_TS(format, ...)  printf(format, ##__VA_ARGS__)

template <size_t TX_BUFFER_SIZE, size_t RX_BUFFER_SIZE> class tcp_socket {
public:
    tcp_socket(NX_IP *interface, size_t window_size = 512) : interface_(interface) {
        UINT ret;
        ret = nx_tcp_socket_create(interface, &socket_, "TCP Server Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                   NX_IP_TIME_TO_LIVE, window_size, NX_NULL, NX_NULL);
        if (ret != NX_SUCCESS) {
            Error_Handler();
        }
    }

    ~tcp_socket() {
        UINT ret;
        ret = nx_tcp_socket_delete(&socket_);
        if (ret != NX_SUCCESS) {
            Error_Handler();
        }

        if (listen_callbacks.find(&socket_) != listen_callbacks.end()) {
            listen_callbacks.erase(&socket_);
        }
    }

    // for server
    void listen(uint16_t port, UINT max_clients) {
        UINT ret;
        ret = nx_tcp_server_socket_listen(interface_, port, &socket_, max_clients, tcp_listen_callback);
        if (ret != NX_SUCCESS) {
            Error_Handler();
        }
        is_listened_ = true;
    }

    // for server
    void relisten(uint16_t port) {
        assert(is_listened_);
        UINT ret;
        ret = nx_tcp_server_socket_relisten(interface_, port, &socket_);
        if (ret != NX_SUCCESS) {
            Error_Handler();
        }
    }

    // for server
    void accept() {
        UINT ret;
        ret = nx_tcp_server_socket_accept(&socket_, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
            Error_Handler();
        }
    }

    // for client
    void bind(uint16_t port) {
        UINT ret;
        ret = nx_tcp_client_socket_bind(&socket_, port, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
        	ERROR_TS("bind: %x\n", ret);
            Error_Handler();
        }
    }

    // for clients
    void connect(uint32_t ip_address, uint16_t port) {
        UINT ret;
        ret = nx_tcp_client_socket_connect(&socket_, ip_address, port, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
        	ERROR_TS("connect: %x\n", ret);
            Error_Handler();
        }
    }

    void disconnect() {
        UINT ret;
        ret = nx_tcp_socket_disconnect(&socket_, NX_NO_WAIT);
        ret = nx_tcp_server_socket_unaccept(&socket_);
        if (ret != NX_SUCCESS) {
            Error_Handler();
        }
    }

    bool is_connected() { return socket_.nx_tcp_socket_state == NX_TCP_ESTABLISHED; }

    void send_str(std::string &str) {
        UINT ret;
        NX_PACKET *data_packet;

        size_t len = std::strlen(str.c_str());

        str.copy((char *)tx_buffer_, len);

        // ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
        // if (ret != NX_SUCCESS) {
        //     return;
        // }

        // nx_packet_data_append(data_packet, tx_buffer_, len, &NxAppPool, TX_WAIT_FOREVER);
        // ret = nx_tcp_socket_send(&socket_, data_packet, TX_WAIT_FOREVER);
        // if (ret != NX_SUCCESS) {
        //     nx_packet_release(data_packet);
        //     return;
        // }
        send((uint8_t *)tx_buffer_, len);
    }

    void send(uint8_t *data, const size_t len) {
        UINT ret;
        NX_PACKET *data_packet;

        ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
            return;
        }

        nx_packet_data_append(data_packet, data, len, &NxAppPool, TX_WAIT_FOREVER);
        ret = nx_tcp_socket_send(&socket_, data_packet, TX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
            nx_packet_release(data_packet);
            return;
        }
    }

    std::string receive_str() {
        UINT ret;
        ULONG bytes_read;
        NX_PACKET *data_packet;

        ret = nx_tcp_socket_receive(&socket_, &data_packet, NX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
            return std::string();
        }

        nx_packet_data_retrieve(data_packet, rx_buffer_, &bytes_read);
        nx_packet_release(data_packet);
        return std::string((char *)rx_buffer_, bytes_read);
    }

    bool receive(uint8_t *data, size_t &len) {
        UINT ret;
        ULONG bytes_read;
        NX_PACKET *data_packet;

        ret = nx_tcp_socket_receive(&socket_, &data_packet, NX_WAIT_FOREVER);
        if (ret != NX_SUCCESS) {
            return false;
        }

        nx_packet_data_retrieve(data_packet, data, &bytes_read);
        nx_packet_release(data_packet);
        len = bytes_read;

        return true;
    }

    void on_listen(std::function<void(UINT)> listen_callback) {
        listen_callback_ = listen_callback;
        listen_callbacks[&socket_] = this;
    }

private:
    static VOID tcp_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port) {
        if (listen_callbacks.find(socket_ptr) != listen_callbacks.end()) {
            if (auto &listen_callback = listen_callbacks[socket_ptr]->listen_callback_; listen_callback.has_value()) {
                listen_callback.value()(port);
            }
        }
    }

private:
    NX_IP *interface_;
    NX_TCP_SOCKET socket_;
    bool is_listened_ = false;
    std::optional<std::function<void(UINT)>> listen_callback_;

    uint8_t tx_buffer_[TX_BUFFER_SIZE];
    uint8_t rx_buffer_[RX_BUFFER_SIZE];

    static inline std::unordered_map<NX_TCP_SOCKET *, tcp_socket<TX_BUFFER_SIZE, RX_BUFFER_SIZE> *> listen_callbacks;
};

} // namespace netxduo

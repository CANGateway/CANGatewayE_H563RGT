#pragma once

#include "app_netxduo.h"
#include "cannelloni.hpp"
#include "main.h"
#include "netxduo-cpp/tcp_socket.hpp"
#include "stmbed/can.hpp"
#include "stmbed/digital_out.hpp"
#include "threadx-cpp/mutex.hpp"
#include "threadx-cpp/thread.hpp"
#include <functional>
#include <memory>
#include <sstream>
#include <string>

class CannelloniChannel {
public:
    using TCPSocketType = netxduo::tcp_socket<256, 256>;

    CannelloniChannel(NX_IP *interface, uint16_t port, stmbed::CAN &hcan, int channel_id)
        : interface_(interface), port_(port), can_(hcan), channel_id_(channel_id) {}

    void start() {
        main_thread_ = std::make_unique<threadx::static_thread<THREAD_STACK_SIZE>>(
            "Main Thread", std::bind(&CannelloniChannel::channel_main, this, std::placeholders::_1));
    }

    int channel_id() const { return channel_id_; }

private:
    void channel_main(ULONG thread_input) {
        printf("channel_main\n");
        using namespace threadx;

        printf("can attach\n");
        can_.attach([&](const stmbed::CANMessage &msg) {
            if (is_initiated) {
                // printf("can recv: id: %d, size: %d\n", msg.id, msg.size);
                tx_msg_queue_.push(msg); // ISRから呼び出すためlockせず直接push
            } else {
                printf("can recv but not initiated\n");
            }
        });

        // create socket
        tcp_socket_ = std::make_unique<TCPSocketType>(interface_);
        tcp_socket_->listen(port_, MAX_TCP_CLIENTS);
        printf("TCP Server listening on PORT %d ..\n", port_);

        while (1) {
            printf("wait accept\n");
            tcp_socket_->accept();

            // スレッドの作成
            receive_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
                "Receive Thread", std::bind(&CannelloniChannel::receive_thread_entry, this, std::placeholders::_1));
            send_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
                "Send Thread", std::bind(&CannelloniChannel::send_thread_entry, this, std::placeholders::_1));

            printf("wait for disconnect\n");
            // クライアントの切断を待つ
            while (1) {
                if (!tcp_socket_->is_connected()) {
                    printf("Connection lost, resetting socket\n");
                    break;
                }

                this_thread::sleep_for(100); // 少し待機
            }

            // cleanup_and_relisten
            cleanup_and_relisten();
        }
    }

    void receive_thread_entry(ULONG thread_input) {
        using namespace threadx;

        // hand shake
        while (1) {
            std::string str = tcp_socket_->receive_str();
            if (str == cannelloni::tcp_protocol::HANDSHAKE) {
                printf("handshake: %s\n", str.c_str());
                is_initiated = true;
                break;
            }
        }

        // main loop
        printf("start receive thread\n");
        std::vector<uint8_t> recv_buf;
        recv_buf.reserve(256);
        std::vector<stmbed::CANMessage> msgs;
        msgs.reserve(8);
        size_t recv_size;
        while (1) {
            if (tcp_socket_->receive(recv_buf.data(), recv_size)) {

                // printf("recv: (%d) ", recv_size);
                // for (int i = 0; i < recv_size; i++) {
                //     printf("%x ", recv_buf[i]);
                // }
                // printf("\n");

                if (cannelloni::tcp_protocol::parse_packet(msgs, recv_buf.data(), recv_size)) {
                    for (auto &msg : msgs) {
                        // printf("id: %x, type: %s, data: ", msg.id,
                        //        msg.format == stmbed::CANFormat::CANStandard ? "Standard" : "Extended");
                        // for (auto &data : msg.data) {
                        //     printf("%x ", data);
                        // }
                        // printf("\n");
                        rx_msg_queue_.push(msg);
                    }
                    // printf("rx_msg_queue_.size(): %ld\n", rx_msg_queue_.size());
                } else {
                    printf("parse fail\n");
                }
            }

            if (rx_msg_queue_.size() > 10) {
                printf("warn: rx_msg_queue_.size() > 10 (%ld)\n", rx_msg_queue_.size());
            }

            while (!rx_msg_queue_.empty() && can_.writeable()) {
                // printf("can write port: %d\n", port_);
                can_.write(rx_msg_queue_.front());
                rx_msg_queue_.pop();
            }

            this_thread::sleep_for(10);
        }
    }

    void send_thread_entry(ULONG thread_input) {
        using namespace threadx;

        printf("start handshake\n");
        std::string hs_str = cannelloni::tcp_protocol::HANDSHAKE;
        tcp_socket_->send_str(hs_str);

        while (not is_initiated) {
            this_thread::sleep_for(100);
        }

        printf("start send thread\n");
        std::vector<stmbed::CANMessage> msgs;
        std::vector<uint8_t> send_buf;
        msgs.reserve(8);
        send_buf.reserve(256);
        while (1) {
            if (!tx_msg_queue_.empty()) {
                tx_msg_queue_mutex_.lock();

                // copy queue to vector
                while (!tx_msg_queue_.empty()) {
                    msgs.push_back(tx_msg_queue_.front());
                    tx_msg_queue_.pop();
                }

                tx_msg_queue_mutex_.unlock();

                size_t packet_size = cannelloni::tcp_protocol::build_packet(send_buf.data(), msgs);
                msgs.clear();
                // printf("send packet_size: %d\n", packet_size);
                tcp_socket_->send(send_buf.data(), packet_size);
            } else {
                this_thread::sleep_for(10);
            }
        }
    }

    void cleanup_and_relisten(void) {
        printf("cleanup_and_relisten\n");
        // スレッドの削除
        receive_thread_.reset();
        send_thread_.reset();

        // ソケットの切断とリセット
        tcp_socket_->disconnect();
        tcp_socket_->relisten(port_);
        printf("cleanup_and_relisten - end\n");

        is_initiated = false;
    }

    void add_tx_queue(const stmbed::CANMessage &&str) {
        if (is_initiated) {
            tx_msg_queue_mutex_.lock();
            tx_msg_queue_.push(str);
            tx_msg_queue_mutex_.unlock();
        }
    }

    NX_IP *interface_;
    uint16_t port_;
    stmbed::CAN can_;
    const int channel_id_;

    constexpr static UINT THREAD_STACK_SIZE = 2048;
    std::unique_ptr<threadx::thread> main_thread_;
    std::unique_ptr<threadx::thread> receive_thread_;
    std::unique_ptr<threadx::thread> send_thread_;
    std::unique_ptr<TCPSocketType> tcp_socket_;

    std::queue<stmbed::CANMessage> tx_msg_queue_; // CANから受信したデータのキュー
    threadx::mutex tx_msg_queue_mutex_;

    std::queue<stmbed::CANMessage> rx_msg_queue_; // ホストから受信したデータのキュー

    bool is_initiated = false;
};

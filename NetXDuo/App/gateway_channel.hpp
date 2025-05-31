#pragma once

#include "app_netxduo.h"
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

template <class... Args> static std::string format(const char *fmt, Args... args) {
    size_t len = snprintf(nullptr, 0, fmt, args...) + 1;
    std::string buf;
    buf.resize(len);
    snprintf(buf.data(), len, fmt, args...);
    return buf;
}

class GatewayChannel {
public:
    using TCPSocketType = netxduo::tcp_socket<256, 256>;

    enum class GatewayState {
        IDLE,
        CONNECTING,
        CONNECTED,
        EXPECT_OK,
        DISCONNECTED,
    };

    GatewayChannel() = default;

    GatewayChannel(NX_IP *interface, uint32_t server_ip_address, const uint16_t server_port, uint16_t port,
                   std::string can_interface_name, stmbed::CAN &hcan)
        : interface_(interface), server_ip_address_(server_ip_address), server_port_(server_port), /*port_(port),*/
          can_interface_name_(can_interface_name), can_(hcan) {}

    ~GatewayChannel() = default;

    void start() {
        main_thread_ = std::make_unique<threadx::static_thread<THREAD_STACK_SIZE>>(
            "Main Thread", std::bind(&GatewayChannel::channel_main, this, std::placeholders::_1));
    }

private:
    void channel_main(ULONG thread_input) {
        printf("channel_main\n");
        using namespace threadx;

        printf("can attach\n");
        can_.attach([&](const stmbed::CANMessage &msg) {
            if (state_ != GatewayState::CONNECTED) {
                return;
            }
            // printf("can recv: id: %x, size: %d, is_extended: %d\n", msg.id, msg.size,
            //        msg.format == stmbed::CANFormat::CANExtended);
            std::string str = to_socketcan_frame_str(msg);
            // printf("to_socketcan_frame_str: \"%s\"\n", str.c_str());
            // add_tx_queue(str);
            send_str_cmd_queue_.push(std::move(str)); // ISRから呼び出すため直接push
        });

        // create socket
        tcp_socket_ = std::make_unique<TCPSocketType>(interface_);

        printf("bind\n");
        nx_tcp_client_socket_unbind(&tcp_socket_->get_native_handle());
        tcp_socket_->bind(0);

        while (1) {
            // printf("wait connect\n");
            if (tcp_socket_->connect(server_ip_address_, server_port_)) {
                // 接続に成功したらスレッドの作成
                printf("tcp connection!\n");
                receive_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
                    "Receive Thread", std::bind(&GatewayChannel::receive_thread_entry, this, std::placeholders::_1));
                send_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
                    "Send Thread", std::bind(&GatewayChannel::send_thread_entry, this, std::placeholders::_1));
            } else {
                this_thread::sleep_for(500);
                continue;
            }

            // printf("wait for disconnect\n");
            // クライアントの切断を待つ
            while (1) {
                // 接続が切断された場合
                if (!tcp_socket_->is_connected()) {
                    if (state_ != GatewayState::DISCONNECTED) {
                        printf("Connection lost, resetting socket\n");
                    }
                    state_ = GatewayState::DISCONNECTED;
                    break;
                }

                // メイン処理
                while (!recv_str_cmd_queue_.empty()) {
                    recv_str_cmd_mute_.lock();
                    auto cmd = recv_str_cmd_queue_.front();
                    recv_str_cmd_queue_.pop();
                    recv_str_cmd_mute_.unlock();

                    // printf("stm32 listen: %s\n", cmd.c_str());

                    if (cmd.starts_with("< hi >") && state_ != GatewayState::CONNECTING) {
                        state_ = GatewayState::CONNECTING;
                        add_tx_queue("< open " + can_interface_name_ + " >");
                    } else if (cmd == "< error could not open bus >") {
                        state_ = GatewayState::DISCONNECTED;
                    } else if (cmd == "< ok >") {
                        if (state_ == GatewayState::CONNECTING) {
                            add_tx_queue("< rawmode >");
                            state_ = GatewayState::EXPECT_OK;
                        }
                        if (state_ == GatewayState::EXPECT_OK) {
                            state_ = GatewayState::CONNECTED;
                            // add_tx_queue("< ok >");
                        }
                    } else if (cmd == "< echo >") {
                        add_tx_queue("< echo >");
                    } else if (state_ == GatewayState::CONNECTED) {
                        // if (cmd.starts_with("< send ")) {
                        //     // e.g. "< send 1FFFFFFF 5 a 0 0 1 cf >"
                        //     //       < send [id] [dlc] [data] >

                        //     // printf("to_can_message\n");

                        //     stmbed::CANMessage msg = to_can_message(cmd);

                        //     // printf("msg.format: %d\n", msg.format);
                        //     // printf("msg.id: %d\n", msg.id);
                        //     // printf("msg.size: %d\n", msg.size);
                        //     // for(size_t i = 0; i < msg.size; i++) {
                        //     //     printf("msg.data[%d]: %d\n", i, msg.data[i]);
                        //     // }

                        //     // printf("can->tx_fifo_size(): %d\n", can_->tx_fifo_size());

                        //     // printf("can write\n");
                        //     can_.write(msg);

                        //     // printf("can->write\n");
                        // } else
                        if (cmd.starts_with("< frame ")) {
                            // e.g. "< frame 1FFFFFFF 5 a 0 0 1 cf >"
                            //       < frame [id] [dlc] [data] >
                            stmbed::CANMessage msg = to_can_frame(cmd);
                            // printf("msg.format: %d\n", msg.format);
                            // printf("msg.id: %d\n", msg.id);
                            // printf("msg.size: %d\n", msg.size);
                            // for (size_t i = 0; i < msg.size; i++) {
                            // printf("msg.data[%d]: %d\n", i, msg.data[i]);
                            // }
                            can_.write(msg);
                        } else if (cmd == "< close >") {
                            state_ = GatewayState::DISCONNECTED;
                            add_tx_queue("< close >");
                        } else if (cmd != "") {
                            printf("unknown command2: %s\n", cmd.c_str());
                            // add_tx_queue("< ok >");
                        }
                    } else {
                        printf("unknown command1: %s\n", cmd.c_str());
                    }
                }

                this_thread::sleep_for(10); // 少し待機
            }

            // cleanup_and_relisten
            cleanup_and_relisten();
        }
    }

    void receive_thread_entry(ULONG thread_input) {
        using namespace threadx;
        std::string rx_str;
        std::vector<std::string> str_cmd_list;
        printf("start receive thread\n");
        while (1) {
            rx_str = tcp_socket_->receive_str();

            str_cmd_list = parse_cmd(rx_str);

            // add str_cmd_list to
            recv_str_cmd_mute_.lock();
            for (auto &cmd : str_cmd_list) {
                recv_str_cmd_queue_.push(cmd);
                // printf("recv_str_cmd_queue_.push: %s\n", cmd.c_str());
            }
            recv_str_cmd_mute_.unlock();

            this_thread::sleep_for(10);
        }
    }

    void send_thread_entry(ULONG thread_input) {
        using namespace threadx;
        std::string str;

        printf("start send thread\n");
        while (1) {
            if (!send_str_cmd_queue_.empty()) {
                send_str_cmd_mute_.lock();
                str = send_str_cmd_queue_.front();
                send_str_cmd_queue_.pop();
                send_str_cmd_mute_.unlock();

                // printf("send: %s, %s\n", can_interface_name_.c_str(), str.c_str());
                tcp_socket_->send_str(str);
            } else {
                this_thread::sleep_for(10);
            }
        }
    }

    void cleanup_and_relisten(void) {
        // printf("cleanup_and_relisten\n");
        //  スレッドの削除
        receive_thread_.reset();
        send_thread_.reset();

        // ソケットの切断とリセット
        tcp_socket_->disconnect();
        //        tcp_socket_.reset();
        //        tcp_socket_ = std::make_unique<TCPSocketType>(interface_);
        tcp_socket_->bind(0);
        // printf("cleanup_and_relisten - end\n");
    }

    void add_tx_queue(const std::string &str) {
        send_str_cmd_mute_.lock();
        send_str_cmd_queue_.push(str);
        send_str_cmd_mute_.unlock();
    }

    std::vector<std::string> parse_cmd(const std::string &str) {
        std::vector<std::string> cmd_list;

        size_t start = 0;
        size_t end = 0;
        while ((start = str.find("<", end)) != std::string::npos) {
            end = str.find(">", start);
            if (end == std::string::npos) {
                break;
            }
            cmd_list.push_back(str.substr(start, end - start + 1));
        }

        return cmd_list;
    }

    // stmbed::CANMessage to_can_message(const std::string &packet) {
    //     stmbed::CANMessage msg;

    //     // e.g. "< send 1FFFFFFF 5 a 0 0 1 cf >"
    //     //       < send [id] [dlc] [data] >

    //     bool is_extended;
    //     uint32_t id;
    //     std::vector<uint8_t> data;

    //     size_t pos = 0;
    //     size_t len = packet.size();

    //     // Find the start of the packet
    //     while (pos < len && packet[pos] != '<')
    //         ++pos;
    //     if (pos == len)
    //         return stmbed::CANMessage();
    //     ++pos;

    //     // Skip whitespace and "send"
    //     while (pos < len && std::isspace(packet[pos]))
    //         ++pos;
    //     if (pos == len || packet.substr(pos, 4) != "send")
    //         return stmbed::CANMessage();
    //     pos += 4;

    //     // Skip whitespace
    //     while (pos < len && std::isspace(packet[pos]))
    //         ++pos;

    //     // Read ID
    //     size_t id_start = pos;
    //     while (pos < len && std::isalnum(packet[pos]))
    //         ++pos;
    //     if (pos == id_start)
    //         return stmbed::CANMessage();

    //     std::string id_str = packet.substr(id_start, pos - id_start);
    //     if (id_str.size() == 3) {
    //         is_extended = false;
    //         id = std::strtoul(id_str.c_str(), nullptr, 16);
    //     } else if (id_str.size() == 8) {
    //         is_extended = true;
    //         id = std::strtoul(id_str.c_str(), nullptr, 16);
    //     } else {
    //         return stmbed::CANMessage();
    //     }

    //     // Skip whitespace
    //     while (pos < len && std::isspace(packet[pos]))
    //         ++pos;

    //     // Read DLC
    //     size_t dlc_start = pos;
    //     while (pos < len && std::isdigit(packet[pos]))
    //         ++pos;
    //     if (pos == dlc_start)
    //         return stmbed::CANMessage();

    //     int dlc = std::strtoul(packet.substr(dlc_start, pos - dlc_start).c_str(), nullptr, 10);

    //     // Skip whitespace
    //     while (pos < len && std::isspace(packet[pos]))
    //         ++pos;

    //     // Read Data
    //     data.clear();
    //     for (int i = 0; i < dlc; ++i) {
    //         size_t data_start = pos;
    //         while (pos < len && std::isalnum(packet[pos]))
    //             ++pos;
    //         if (pos == data_start)
    //             return stmbed::CANMessage();

    //         std::string byte_str = packet.substr(data_start, pos - data_start);
    //         uint8_t byte = std::strtoul(byte_str.c_str(), nullptr, 16);
    //         data.push_back(byte);

    //         // Skip whitespace
    //         while (pos < len && std::isspace(packet[pos]))
    //             ++pos;
    //     }

    //     msg.format = is_extended ? stmbed::CANExtended : stmbed::CANStandard;
    //     msg.id = id;
    //     msg.size = dlc;
    //     for (size_t i = 0; i < dlc; i++) {
    //         msg.data[i] = data[i];
    //     }

    //     return msg;
    // }

    stmbed::CANMessage to_can_frame(const std::string &packet) {
        stmbed::CANMessage msg{};
        bool is_extended = false;
        uint32_t id = 0;
        // double timestamp = 0.0;
        std::vector<uint8_t> data;

        size_t pos = 0, len = packet.size();
        // “<” までスキップ
        while (pos < len && packet[pos] != '<')
            ++pos;
        if (pos == len)
            return msg;
        ++pos;

        // skip whitespace + "frame"
        while (pos < len && std::isspace(packet[pos]))
            ++pos;
        if (pos + 5 > len || packet.substr(pos, 5) != "frame")
            return msg;
        pos += 5;

        // skip whitespace
        while (pos < len && std::isspace(packet[pos]))
            ++pos;

        // ID 読み取り
        size_t id_start = pos;
        while (pos < len && std::isalnum(packet[pos]))
            ++pos;
        std::string id_str = packet.substr(id_start, pos - id_start);
        if (id_str.size() == 3) {
            is_extended = false;
            id = std::strtoul(id_str.c_str(), nullptr, 16);
        } else if (id_str.size() == 8) {
            is_extended = true;
            id = std::strtoul(id_str.c_str(), nullptr, 16);
        } else {
            return msg;
        }

        // skip whitespace
        while (pos < len && std::isspace(packet[pos]))
            ++pos;

        // タイムスタンプ読み取り
        size_t ts_start = pos;
        while (pos < len && (std::isdigit(packet[pos]) || packet[pos] == '.'))
            ++pos;
        std::string ts_str = packet.substr(ts_start, pos - ts_start);

        // Todo: timestamp の変換
        // try {
        //     timestamp = std::stod(ts_str);
        // } catch (...) {
        //     return msg;
        // }

        // skip whitespace
        while (pos < len && std::isspace(packet[pos]))
            ++pos;

        // データ部（hex）読み取り
        size_t data_start = pos;
        while (pos < len && std::isxdigit(packet[pos]))
            ++pos;
        std::string hex_str = packet.substr(data_start, pos - data_start);
        size_t dlc = hex_str.size() / 2;
        data.reserve(dlc);
        for (size_t i = 0; i < dlc; ++i) {
            std::string byte_str = hex_str.substr(i * 2, 2);
            data.push_back(static_cast<uint8_t>(std::strtoul(byte_str.c_str(), nullptr, 16)));
        }

        // msg に詰める
        msg.format = is_extended ? stmbed::CANExtended : stmbed::CANStandard;
        msg.id = id;
        msg.size = static_cast<uint8_t>(dlc);
        // msg.timestamp = timestamp;
        for (size_t i = 0; i < dlc && i < sizeof(msg.data); ++i) {
            msg.data[i] = data[i];
        }
        return msg;
    }

    std::string to_socketcan_frame_str(const stmbed::CANMessage &msg) {
        std::string str;

        std::string id_str;
        if (msg.format == stmbed::CANStandard) {
            id_str = format("%03X", msg.id);
        } else {
            id_str = format("%08X", msg.id);
        }
        // printf("%s\n", id_str.c_str());

        if (msg.size > 0) {
            std::string data_str;
            // printf("msg.size: %d\n", msg.size);
            for (size_t i = 0; i < msg.size; i++) {
                data_str = format("%s%02X ", data_str.c_str(), msg.data[i]);
            }
            // printf("%s\n", data_str.c_str());
            str = format("< send %s %d %s>", id_str.c_str(), msg.size, data_str.c_str());
        } else {
            str = format("< send %s 0 >", id_str.c_str());
        }

        // std::string time_str;
        // time_str = format("%.3f", 0.0);
        // // printf("%s\n", time_str.c_str());

        // std::string data_str;
        // // printf("msg.size: %d\n", msg.size);
        // for (size_t i = 0; i < msg.size; i++) {
        //     data_str = format("%s%02X", data_str.c_str(), msg.data[i]);
        // }
        // // printf("%s\n", data_str.c_str());

        // str = format("< frame %s %s %s >", id_str.c_str(), time_str.c_str(), data_str.c_str());
        return str;
    }

    NX_IP *interface_;
    // uint16_t port_;
    std::string can_interface_name_;
    stmbed::CAN can_;

    const uint32_t server_ip_address_;
    const uint16_t server_port_;

    GatewayState state_ = GatewayState::IDLE;

    constexpr static UINT THREAD_STACK_SIZE = 1024 * 2;
    std::unique_ptr<threadx::thread> main_thread_;
    std::unique_ptr<threadx::thread> receive_thread_;
    std::unique_ptr<threadx::thread> send_thread_;
    std::unique_ptr<TCPSocketType> tcp_socket_;

    std::queue<std::string> recv_str_cmd_queue_;
    threadx::mutex recv_str_cmd_mute_;
    std::queue<std::string> send_str_cmd_queue_;
    threadx::mutex send_str_cmd_mute_;
};

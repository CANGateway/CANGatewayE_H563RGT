#pragma once

#include "main.h"
#include "app_netxduo.h"
#include "stmbed/can.hpp"
#include "threadx-cpp/thread.hpp"
#include "threadx-cpp/mutex.hpp"
#include "netxduo-cpp/tcp_socket.hpp"
#include <string>
#include <memory>
#include <sstream>
#include <functional>
#include "stmbed/can.hpp"

template <class... Args> 
static std::string format(const char *fmt, Args... args)
{
    size_t len = snprintf(nullptr, 0, fmt, args...) + 1;
    std::string buf;
    buf.resize(len);
    snprintf(buf.data(), len, fmt, args...);
    return buf;
}

class GatewayChannel
{
public:
	using TCPSocketType = netxduo::tcp_socket<256, 256>;

    GatewayChannel(NX_IP* interface, uint16_t port, std::shared_ptr<stmbed::CAN> can) :
        interface_(interface),
        port_(port),
        can_(can)
    {

    }

    void start() {
         main_thread_ = std::make_unique<threadx::static_thread<THREAD_STACK_SIZE>>(
             "Main Thread",
             std::bind(&GatewayChannel::channel_main, this, std::placeholders::_1)
         );
    }

private:
    void channel_main(ULONG thread_input)
    {
        printf("channel_main\n");
        using namespace threadx;

        can_->attach(
            [&](const stmbed::CANMessage &msg) {
                printf("can recv: id: %d\n", msg.id);

                std::string str = to_socketcan_frame_str(msg);
                printf("to_socketcan_frame_str: %s\n", str.c_str());
                add_tx_queue(str);
            }
        );
        
        // create socket
        tcp_socket_ = std::make_unique<TCPSocketType>(interface_);
        tcp_socket_->listen(port_, MAX_TCP_CLIENTS);
        {
            printf("TCP Server listening on PORT %d ..\n", port_);
        }

        while (1)
        {
            printf("tp3 wait accept\n");
            tcp_socket_->accept();

            // スレッドの作成
            receive_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
                "Receive Thread", std::bind(&GatewayChannel::receive_thread_entry, this, std::placeholders::_1)
            );
            send_thread_ = std::make_unique<static_thread<THREAD_STACK_SIZE>>(
                "Send Thread", std::bind(&GatewayChannel::send_thread_entry, this, std::placeholders::_1)
            );

            printf("wait for disconnect\n");
            // クライアントの切断を待つ
            while (1)
            {
                if(!tcp_socket_->is_connected())
                {
                    printf("Connection lost, resetting socket\n");
                    break;
                }

                this_thread::sleep_for(10); // 少し待機
            }

            // cleanup_and_relisten
            cleanup_and_relisten();
        }
    }

    void receive_thread_entry(ULONG thread_input) 
    {
        using namespace threadx;
        std::string rx_str;
        std::vector<std::string> str_cmd_list;

        printf("start receive thread\n");
        while (1)
        {
            rx_str = tcp_socket_->receive_str();

            str_cmd_list = parse_cmd(rx_str);

            for(auto &cmd : str_cmd_list) {
                printf("stm32 listen: %s\n", cmd.c_str());

                // 複数パケットを分割
                if(cmd.starts_with("< open ")) {
                    add_tx_queue("< ok >");
                }
                if(cmd == "< rawmode >") {
                    add_tx_queue("< ok >");
                }
                if(cmd.starts_with("< send ")) {
                    // e.g. "< send 1FFFFFFF 5 a 0 0 1 cf >"
                    //       < send [id] [dlc] [data] >

                    // printf("to_can_message\n");

                    stmbed::CANMessage msg = to_can_message(cmd);

                    // printf("msg.format: %d\n", msg.format);
                    // printf("msg.id: %d\n", msg.id);
                    // printf("msg.size: %d\n", msg.size);
                    // for(size_t i = 0; i < msg.size; i++) {
                    //     printf("msg.data[%d]: %d\n", i, msg.data[i]);
                    // }

                    printf("can->tx_fifo_size(): %d\n", can_->tx_fifo_size());
                    
                    // printf("can write\n");
                    can_->write(msg);

                    // printf("can->write\n");
                }
                else if(cmd != ""){
                    add_tx_queue("< ok >");
                }
            }
            this_thread::sleep_for(10);
        }
    }

    void send_thread_entry(ULONG thread_input) 
    {
        using namespace threadx;
        std::string str;

        printf("start send thread\n");
        add_tx_queue("< hi >");
        while (1)
        {
            if(!socketcan_tx_msg_queue.empty()) {
                tx_socketcan_msg_queue_mutex_.lock();
                str = socketcan_tx_msg_queue.front();
                socketcan_tx_msg_queue.pop();
                tx_socketcan_msg_queue_mutex_.unlock();

                tcp_socket_->send_str(str);
            }
            else        
            {
                this_thread::sleep_for(10);
            }
        }
    }

    void cleanup_and_relisten(void)
    {
        printf("cleanup_and_relisten\n");
        // スレッドの削除
        receive_thread_.reset();
        send_thread_.reset();

        // ソケットの切断とリセット
        tcp_socket_->disconnect();
        tcp_socket_->relisten(port_);
    }

    void add_tx_queue(const std::string &str) {
        tx_socketcan_msg_queue_mutex_.lock();
        socketcan_tx_msg_queue.push(str);
        tx_socketcan_msg_queue_mutex_.unlock();
    }

    std::vector<std::string> parse_cmd(const std::string &str) {
        std::vector<std::string> cmd_list;

        size_t start = 0;
        size_t end = 0;
        while((start = str.find("<", end)) != std::string::npos) {
            end = str.find(">", start);
            if(end == std::string::npos) {
                break;
            }
            cmd_list.push_back(str.substr(start, end - start + 1));
        }

        return cmd_list;
    }

    stmbed::CANMessage to_can_message(const std::string &packet) {
        stmbed::CANMessage msg;

        // e.g. "< send 1FFFFFFF 5 a 0 0 1 cf >"
        //       < send [id] [dlc] [data] >

        bool is_extended;
        uint32_t id;
        std::vector<uint8_t> data;

        size_t pos = 0;
        size_t len = packet.size();

        // Find the start of the packet
        while (pos < len && packet[pos] != '<') ++pos;
        if (pos == len) return stmbed::CANMessage();
        ++pos;

        // Skip whitespace and "send"
        while (pos < len && std::isspace(packet[pos])) ++pos;
        if (pos == len || packet.substr(pos, 4) != "send") return stmbed::CANMessage();
        pos += 4;

        // Skip whitespace
        while (pos < len && std::isspace(packet[pos])) ++pos;

        // Read ID
        size_t id_start = pos;
        while (pos < len && std::isalnum(packet[pos])) ++pos;
        if (pos == id_start) return stmbed::CANMessage();

        std::string id_str = packet.substr(id_start, pos - id_start);
        if (id_str.size() == 3) {
            is_extended = false;
            id = std::strtoul(id_str.c_str(), nullptr, 16);
        } else if (id_str.size() == 8) {
            is_extended = true;
            id = std::strtoul(id_str.c_str(), nullptr, 16);
        } else {
            return stmbed::CANMessage();
        }

        // Skip whitespace
        while (pos < len && std::isspace(packet[pos])) ++pos;

        // Read DLC
        size_t dlc_start = pos;
        while (pos < len && std::isdigit(packet[pos])) ++pos;
        if (pos == dlc_start) return stmbed::CANMessage();

        int dlc = std::strtoul(packet.substr(dlc_start, pos - dlc_start).c_str(), nullptr, 10);

        // Skip whitespace
        while (pos < len && std::isspace(packet[pos])) ++pos;

        // Read Data
        data.clear();
        for (int i = 0; i < dlc; ++i) {
            size_t data_start = pos;
            while (pos < len && std::isalnum(packet[pos])) ++pos;
            if (pos == data_start) return stmbed::CANMessage();

            std::string byte_str = packet.substr(data_start, pos - data_start);
            uint8_t byte = std::strtoul(byte_str.c_str(), nullptr, 16);
            data.push_back(byte);

            // Skip whitespace
            while (pos < len && std::isspace(packet[pos])) ++pos;
        }

        msg.format = is_extended ? stmbed::CANExtended : stmbed::CANStandard;
        msg.id = id;
        msg.size = dlc;
        for(size_t i = 0; i < dlc; i++) {
            msg.data[i] = data[i];
        }

        return msg;
    }

    std::string to_socketcan_frame_str(const stmbed::CANMessage &msg) {
        std::string str;

        std::string id_str;
        if(msg.format == stmbed::CANStandard) {
            id_str = format("%03X", msg.id);
        }
        else {
            id_str = format("%08X", msg.id);
        }

        std::string data_str;
        for(size_t i = 0; i < msg.size; i++) {
            data_str += format("%02X", msg.data[i]);
        }

        str = format("< frame %s %f %s >", id_str.c_str(), 0.0, data_str.c_str());

        return str;
    }

    NX_IP* interface_;
    uint16_t port_;
    std::shared_ptr<stmbed::CAN> can_;

	constexpr static UINT THREAD_STACK_SIZE = 2048;
    std::unique_ptr<threadx::thread> main_thread_;
    std::unique_ptr<threadx::thread> receive_thread_;
    std::unique_ptr<threadx::thread> send_thread_;
    std::unique_ptr<TCPSocketType> tcp_socket_;

    std::queue<std::string> socketcan_tx_msg_queue;
    threadx::mutex tx_socketcan_msg_queue_mutex_;
};

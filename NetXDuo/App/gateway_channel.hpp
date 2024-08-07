#pragma once

#include "main.h"
#include "app_netxduo.h"
#include "stmbed/can.hpp"
#include "thread.hpp"
#include "tcp_socket.hpp"
#include <string>
#include <memory>
#include <functional>
#include "stmbed/can.hpp"

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

    void add_tx_queue(const std::string &str) {
        tx_mutex_get(&can_queue_mutex, TX_WAIT_FOREVER);
        socketcan_tx_msg_queue.push(str);
        tx_mutex_put(&can_queue_mutex);
    }

    void receive_thread_entry(ULONG thread_input) 
    {
        std::string rx_str;
        std::vector<std::string> str_cmd_list;

        printf("start receive thread\n");
        while (1)
        {
            rx_str = tcp_socket_->receive_str();

            if (rx_str != "")
            {
                printf("stm32 listen: %s\n", rx_str.c_str());

                // 複数パケットを分割
                if(rx_str == "< open can0 >") {
                    add_tx_queue("< ok >");
                }
                if(rx_str == "< rawmode >") {
                    add_tx_queue("< ok >");
                }
                if(rx_str.starts_with("< send")) {
                    // add_tx_queue("< ok >");

                    // dummy recv
                    add_tx_queue("< frame 123 00.000000 1A220344 >");
                }
                else if(rx_str != ""){
                    add_tx_queue("< ok >");
                }
            }
            else
            {
                tx_thread_sleep(1);
            }
        }
    }

    void send_thread_entry(ULONG thread_input) 
    {
        UINT ret;
        NX_PACKET *data_packet;
        CHAR data_buffer[128];
        uint32_t message_length;

        std::string str;

        printf("start send thread\n");

        add_tx_queue("< hi >");

        while (1)
        {
            if(!socketcan_tx_msg_queue.empty()) {
                tx_mutex_get(&can_queue_mutex, TX_WAIT_FOREVER);
                str = socketcan_tx_msg_queue.front();
                socketcan_tx_msg_queue.pop();
                tx_mutex_put(&can_queue_mutex);

                tcp_socket_->send_str(str);
            }
            else        
            {
                tx_thread_sleep(1);
            }
        }
    }

    void cleanup_and_relisten(void)
    {
        printf("cleanup_and_relisten start\n");
        // スレッドの削除
        receive_thread_.reset();
        send_thread_.reset();


        printf("cleanup_and_relisten disconnect\n");

        // ソケットの切断とリセット
        tcp_socket_->disconnect();
        printf("cleanup_and_relisten relisten\n");
        tcp_socket_->relisten(port_);

        printf("cleanup_and_relisten end\n");
    }

    NX_IP* interface_;
    uint16_t port_;
    std::shared_ptr<stmbed::CAN> can_;

	constexpr static UINT THREAD_STACK_SIZE = 1024;
    std::unique_ptr<threadx::thread> main_thread_;
    std::unique_ptr<threadx::thread> receive_thread_;
    std::unique_ptr<threadx::thread> send_thread_;
    std::unique_ptr<TCPSocketType> tcp_socket_;

    std::queue<std::string> socketcan_tx_msg_queue;
    TX_MUTEX can_queue_mutex;

};

// #include "main.h"
// #include "app_netxduo.h"
// #include "can_message.hpp"
// #include <stdbool.h>
// #include <queue>
// #include <mutex>
// #include <cstring>
// #include <string.h>

// #include "gateway_channel.hpp"

// #include "unit_test.hpp"

// extern UART_HandleTypeDef huart3;
// extern NX_IP NetXDuoEthIpInstance;
// extern NX_PACKET_POOL NxAppPool;

// ULONG IpAddress;
// ULONG NetMask;

// // extern FDCAN_HandleTypeDef hfdcan1;
// // extern FDCAN_HandleTypeDef hfdcan2;

// NX_TCP_SOCKET TCPSocket;
// std::array<NX_TCP_SOCKET, 2> TCPSockets;

// TX_THREAD receive_thread;
// TX_THREAD send_thread;

// // uint32_t can_recv_cnt1 = 0;

// // std::queue<CANMessageWithCh> can_rx_msg_queue;
// std::queue<std::string> socketcan_tx_msg_queue;
// TX_MUTEX can_queue_mutex;

// #define TCP_THREAD_STACK_SIZE 2 * 1024
// CHAR send_thread_stack[TCP_THREAD_STACK_SIZE];
// CHAR recv_thread_stack[TCP_THREAD_STACK_SIZE];

// void receive_thread_entry(ULONG thread_input);
// void send_thread_entry(ULONG thread_input);
// void cleanup_and_relisten(void);

// // void can_init(FDCAN_HandleTypeDef *handle);
// // HAL_StatusTypeDef can_write(CANMessageWithCh &msg, bool wait_mailbox_free);
// // HAL_StatusTypeDef can_write_raw(FDCAN_HandleTypeDef *handle, uint32_t id, uint8_t *data, uint32_t size, bool is_extended_frame, bool wait_mailbox_free);

// static VOID tcp_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port)
// {
//     printf("tcp_listen_callback port: %d\n", port);
// }

// void add_tx_queue(const std::string &str) {
//     tx_mutex_get(&can_queue_mutex, TX_WAIT_FOREVER);
//     socketcan_tx_msg_queue.push(str);
//     tx_mutex_put(&can_queue_mutex);
// }

// extern "C" VOID cangateway_main(ULONG thread_input)
// {
//     printf("\nmyMain start\n");

//     unit_test::thread_cpp_test();

//     UINT ret;

//     // can_init(&hfdcan1);
//     // can_init(&hfdcan2);


//     while(1){
//         printf("main\n");
//         tx_thread_sleep(1000);
//     }

//     /*
//      * Print IPv4
//      */
//     ret = nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
//     if (ret != TX_SUCCESS)
//     {
//         Error_Handler();
//     }
//     else
//     {
//         PRINT_IP_ADDRESS(IpAddress);
//     }

//     // ここからソケットごとの処理
//     printf("tp1\n");
//     /*
//      * Create a TCP Socket
//      */
//     ret = nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket, "TCP Server Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE, 512, NX_NULL, NX_NULL);
//     if (ret != NX_SUCCESS)
//     {
//         Error_Handler();
//     }

//     printf("tp2\n");

//     ret = nx_tcp_server_socket_listen(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket, MAX_TCP_CLIENTS, tcp_listen_callback);
//     if (ret != NX_SUCCESS)
//     {
//         Error_Handler();
//     }
//     else
//     {
//         printf("TCP Server listening on PORT %d ..\n", DEFAULT_PORT);
//     }

//     while (1)
//     {
//         printf("tp3 wait accept\n");
//         ret = nx_tcp_server_socket_accept(&TCPSocket, TX_WAIT_FOREVER);
//         if (ret != NX_SUCCESS)
//         {
//             Error_Handler();
//         }

//         printf("tp4\n");

//         // スレッドの作成
//         ret = tx_thread_create(&receive_thread,
//                                "Receive Thread",
//                                receive_thread_entry,
//                                0,
//                                recv_thread_stack,
//                                TCP_THREAD_STACK_SIZE,
//                                NX_APP_THREAD_PRIORITY,
//                                NX_APP_THREAD_PRIORITY,
//                                TX_NO_TIME_SLICE,
//                                TX_AUTO_START);
//         if (ret != NX_SUCCESS)
//         {
//             Error_Handler();
//         }

//         printf("tp5\n");

//         ret = tx_thread_create(&send_thread,
//                                "Send Thread",
//                                send_thread_entry,
//                                0,
//                                send_thread_stack,
//                                TCP_THREAD_STACK_SIZE,
//                                NX_APP_THREAD_PRIORITY,
//                                NX_APP_THREAD_PRIORITY,
//                                TX_NO_TIME_SLICE,
//                                TX_AUTO_START);
//         if (ret != NX_SUCCESS)
//         {
//             Error_Handler();
//         }

//         printf("wait for disconnect\n");
//         // クライアントの切断を待つ
//         while (1)
//         {
//             ULONG socket_state;

//             // printf("check state\n");
//             ret = nx_tcp_socket_info_get(&TCPSocket, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &socket_state, NULL, NULL, NULL);
//             if (ret != NX_SUCCESS)
//             {
//                 Error_Handler();
//             }

//             if (socket_state != NX_TCP_ESTABLISHED)
//             {
//                 printf("Connection lost, resetting socket\n");
//                 break;
//             }

//             tx_thread_sleep(10); // 少し待機
//         }

//         // クライアントが切断された場合のクリーンアップ
//         cleanup_and_relisten();
//     }
// }

// void receive_thread_entry(ULONG thread_input)
// {
//     UINT ret;
//     NX_PACKET *data_packet;
//     UCHAR data_buffer[512];
//     ULONG bytes_read;
//     uint32_t decoded_length;

//     std::string rx_str;
//     std::vector<std::string> str_cmd_list;

//     printf("start receive thread\n");
//     while (1)
//     {
//         TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));
//         ret = nx_tcp_socket_receive(&TCPSocket, &data_packet, NX_WAIT_FOREVER);

//         if (ret == NX_SUCCESS)
//         {
//             nx_packet_data_retrieve(data_packet, data_buffer, &bytes_read);

//             // printf("listen:\n");
//             // printf("listen: %d\n", data_buffer[0]);
//             printf("stm32 listen: %s\n", data_buffer);
//             rx_str = std::string((char *)data_buffer);

//             // 複数パケットを分割

//             if(rx_str == "< open can0 >") {
//                 add_tx_queue("< ok >");
//             }
//             if(rx_str == "< rawmode >") {
//                 add_tx_queue("< ok >");
//             }
//             if(rx_str.starts_with("< send")) {
//                 // add_tx_queue("< ok >");

//                 // dummy recv
//                 add_tx_queue("< frame 123 00.000000 1A220344 >");
//             }
//             else if(rx_str != ""){
//                 add_tx_queue("< ok >");
//             }

//             // CANMessageWithCh msg;
//             // UCHAR *data_buffer_ptr = data_buffer;
//             // while (1)
//             // {
//             //     // DecodeResult dec_ret = decode_can_message(data_buffer_ptr, bytes_read, msg, &decoded_length);
//             //     // // printf("%ld\n", bytes_read);
//             //     // if (dec_ret == DECODE_SUCCESS)
//             //     // {
//             //     //     HAL_StatusTypeDef hal_ret = can_write(msg, true);
//             //     //     // printf("hal_ret: %d\n", hal_ret);
//             //     //     // printf("can_recv_cnt1: %d\n", can_recv_cnt1);
//             //     //     // printf("Decoded CAN message: ID: %08lX, DLC: %d, Data: \n", msg.msg.id, msg.msg.dlc);
//             //     //     // for (int i = 0; i < message.dlc; i++)
//             //     //     // {
//             //     //     //     printf("%02X, ", message.data[i]);
//             //     //     // }
//             //     //     // printf("\n");
//             //     //     if (bytes_read > decoded_length)
//             //     //     {
//             //     //         data_buffer_ptr += decoded_length;
//             //     //         bytes_read -= decoded_length;
//             //     //     }
//             //     //     else
//             //     //     {
//             //     //         break;
//             //     //     }
//             //     // }
//             //     // else
//             //     // {
//             //     //     // printf("decode failed: %d\n", dec_ret);
//             //     //     break;
//             //     // }
//             // }

//             nx_packet_release(data_packet);
//         }
//         else
//         {
//             tx_thread_sleep(1);
//         }
//     }
// }

// void send_thread_entry(ULONG thread_input)
// {
//     UINT ret;
//     NX_PACKET *data_packet;
//     CHAR data_buffer[128];
//     uint32_t message_length;

//     std::string str;

//     printf("start send thread\n");

//     // tx_thread_sleep(1000);

//     printf("Add < hi > msg\n");

//     auto str_send = [&](const std::string &str) {
//         str.copy(data_buffer, str.length());

//         ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
//         if (ret != NX_SUCCESS)
//         {
//             printf("error 1\n");
//             Error_Handler();
//         }

//         ret = nx_packet_data_append(data_packet, data_buffer, str.length(), &NxAppPool, TX_WAIT_FOREVER);
//         if (ret != NX_SUCCESS)
//         {
//             printf("error 2\n");
//             nx_packet_release(data_packet);
//             Error_Handler();
//         }

//         ret = nx_tcp_socket_send(&TCPSocket, data_packet, TX_WAIT_FOREVER);
//         if (ret != NX_SUCCESS)
//         {
//             printf("error 3\n");
//             nx_packet_release(data_packet);
//             // printf("nx_tcp_socket_send error %d\n", ret);
//             // Error_Handler();
//         }

//         // nx_packet_release(data_packet);
//     };

//     // tx_thread_sleep(1000);
//     add_tx_queue("< hi >");

//     while (1)
//     {
//         if(!socketcan_tx_msg_queue.empty()) {
//             tx_mutex_get(&can_queue_mutex, TX_WAIT_FOREVER);
//             str = socketcan_tx_msg_queue.front();
//             socketcan_tx_msg_queue.pop();
//             tx_mutex_put(&can_queue_mutex);

//             str_send(str);
//         }
//         else        
//         {
//             tx_thread_sleep(1);
//         }
//     }
// }

// void cleanup_and_relisten(void)
// {
//     // スレッドの削除
//     tx_thread_terminate(&receive_thread);
//     tx_thread_delete(&receive_thread);

//     tx_thread_terminate(&send_thread);
//     tx_thread_delete(&send_thread);

//     // ソケットの切断とリセット
//     nx_tcp_socket_disconnect(&TCPSocket, TX_WAIT_FOREVER);
//     nx_tcp_server_socket_unaccept(&TCPSocket);
//     nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket);

//     // socketcan_tx_msg_queue.clear();
//     // std::queue<std::string>().swap(socketcan_tx_msg_queue);
// }

// // void can_init(FDCAN_HandleTypeDef *handle)
// // {
// //     if (handle->State == HAL_FDCAN_STATE_READY)
// //     {
// //         FDCAN_FilterTypeDef filter;
// //         filter.IdType = FDCAN_STANDARD_ID;
// //         filter.FilterIndex = 0;
// //         filter.FilterType = FDCAN_FILTER_MASK;
// //         filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
// //         filter.FilterID1 = 0;
// //         filter.FilterID2 = 0x1FF;

// //         if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK)
// //         {
// //             Error_Handler();
// //         }

// //         filter.IdType = FDCAN_EXTENDED_ID;
// //         filter.FilterIndex = 1;
// //         filter.FilterID1 = 0;
// //         filter.FilterID2 = 0x1FFFFFFF;

// //         if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK)
// //         {
// //             Error_Handler();
// //         }

// //         if (HAL_FDCAN_ConfigGlobalFilter(handle, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE,
// //                                          FDCAN_REJECT_REMOTE) != HAL_OK)
// //         {
// //             Error_Handler();
// //         }

// //         if (HAL_FDCAN_ActivateNotification(handle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
// //         {
// //             Error_Handler();
// //         }

// //         if (HAL_FDCAN_Start(handle) != HAL_OK)
// //         {
// //             Error_Handler();
// //         }
// //     }
// //     else
// //     {
// //         Error_Handler();
// //     }
// // }

// // HAL_StatusTypeDef can_write(CANMessageWithCh &msg, bool wait_mailbox_free)
// // {
// //     FDCAN_HandleTypeDef *handle = (msg.bus_ch == CAN_BUS1) ? &hfdcan1 : &hfdcan2;
// //     bool is_extended_frame = (msg.msg.format == CAN_EXTENDED) ? true : false;
// //     return can_write_raw(handle, msg.msg.id, msg.msg.data, msg.msg.dlc, is_extended_frame, wait_mailbox_free);
// // }

// // HAL_StatusTypeDef can_write_raw(FDCAN_HandleTypeDef *handle, uint32_t id, uint8_t *data, uint32_t size, bool is_extended_frame, bool wait_mailbox_free)
// // {
// //     FDCAN_TxHeaderTypeDef tx_header;
// //     tx_header.Identifier = id;
// //     tx_header.IdType = is_extended_frame ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
// //     tx_header.TxFrameType = FDCAN_DATA_FRAME;
// //     tx_header.DataLength = (size > 8 ? 8 : size);
// //     tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
// //     tx_header.BitRateSwitch = FDCAN_BRS_OFF;
// //     tx_header.FDFormat = FDCAN_CLASSIC_CAN;
// //     tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
// //     tx_header.MessageMarker = 0;

// //     // printf("can writ raw\n");

// //     while (wait_mailbox_free && HAL_FDCAN_GetTxFifoFreeLevel(handle) == 0)
// //         ;

// //     return HAL_FDCAN_AddMessageToTxFifoQ(handle, &tx_header, data);
// // }

// // void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
// // {
// //     // can_recv_cnt1++;
// //     FDCAN_RxHeaderTypeDef rx_header;
// //     CANMessageWithCh msg;

// //     if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, msg.msg.data) == HAL_OK)
// //     {
// //         msg.bus_ch = (hfdcan == &hfdcan1) ? CAN_BUS1 : CAN_BUS2;
// //         msg.msg.id = rx_header.Identifier;
// //         msg.msg.dlc = rx_header.DataLength;
// //         msg.msg.format = (rx_header.IdType == FDCAN_STANDARD_ID) ? CAN_STANDARD : CAN_EXTENDED;

// //         tx_mutex_get(&can_queue_mutex, TX_WAIT_FOREVER);
// //         can_rx_msg_queue.push(msg);
// //         tx_mutex_put(&can_queue_mutex);
// //     }
// // }

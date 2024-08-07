// #include "main.h"
// #include "app_netxduo.h"
// #include "can_message.hpp"
// #include <stdbool.h>
// #include <queue>
// #include <mutex>

// extern UART_HandleTypeDef huart3;
// extern NX_IP NetXDuoEthIpInstance;
// extern NX_PACKET_POOL NxAppPool;

// extern FDCAN_HandleTypeDef hfdcan1;
// extern FDCAN_HandleTypeDef hfdcan2;

// NX_TCP_SOCKET TCPSocket;
// ULONG IpAddress;
// ULONG NetMask;

// TX_THREAD receive_thread;
// TX_THREAD send_thread;

// // std::queue<CANMessageWithCh> can_rx_msg_queue;
// // TX_MUTEX can_queue_mutex;

// // #define TCP_THREAD_STACK_SIZE 2 * 1024
// // CHAR send_thread_stack[TCP_THREAD_STACK_SIZE];
// // CHAR recv_thread_stack[TCP_THREAD_STACK_SIZE];

// // void receive_thread_entry(ULONG thread_input);
// // void send_thread_entry(ULONG thread_input);
// // void cleanup_and_relisten(void);

// // static VOID tcp_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port)
// // {
// //     printf("tcp_listen_callback\n");
// // }

// extern "C" VOID cangateway_main(ULONG thread_input)
// {
//     printf("\nmyMain start\n");
//     UINT ret;

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

//     /* bind the client socket for the DEFAULT_PORT */
//     ret = nx_tcp_client_socket_bind(&TCPSocket, DEFAULT_PORT, NX_WAIT_FOREVER);

//     if (ret != NX_SUCCESS)
//     {
//         Error_Handler();
//     }

//     /* connect to the remote server on the specified port */
//     ret = nx_tcp_client_socket_connect(&TCPSocket, TCP_SERVER_ADDRESS, TCP_SERVER_PORT, NX_WAIT_FOREVER);

//     if (ret != NX_SUCCESS)
//     {
//         Error_Handler();
//     }

//     while (1)
//     {
//         /* wait for the server response */
//         ret = nx_tcp_socket_receive(&TCPSocket, &server_packet, DEFAULT_TIMEOUT);

//         if (ret == NX_SUCCESS)
//         {
//             /* get the server IP address and  port */
//             nx_udp_source_extract(server_packet, &source_ip_address, &source_port);

//             /* retrieve the data sent by the server */
//             nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read);

//             /* print the received data */
//             PRINT_DATA(source_ip_address, source_port, data_buffer);

//             /* release the server packet */
//             nx_packet_release(server_packet);

//             /* toggle the green led on success */
//             HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
//         }
//         else
//         {
//             /* no message received exit the loop */
//             break;
//         }
//     }
// }

// // void receive_thread_entry(ULONG thread_input)
// // {
// //     UINT ret;
// //     NX_PACKET *data_packet;
// //     UCHAR data_buffer[512];
// //     ULONG bytes_read;
// //     uint32_t decoded_length;

// //     printf("start receive thread\n");
// //     while (1)
// //     {
// //         TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));
// //         ret = nx_tcp_socket_receive(&TCPSocket, &data_packet, NX_WAIT_FOREVER);

// //         if (ret == NX_SUCCESS)
// //         {
// //             nx_packet_data_retrieve(data_packet, data_buffer, &bytes_read);

// //             CANMessageWithCh msg;
// //             UCHAR *data_buffer_ptr = data_buffer;
// //             while (1)
// //             {
// //                 DecodeResult dec_ret = decode_can_message(data_buffer_ptr, bytes_read, msg, &decoded_length);
// //                 // printf("%ld\n", bytes_read);
// //                 if (dec_ret == DECODE_SUCCESS)
// //                 {
// //                     HAL_StatusTypeDef hal_ret = can_write(msg, true);
// //                     // printf("hal_ret: %d\n", hal_ret);
// //                     // printf("can_recv_cnt1: %d\n", can_recv_cnt1);
// //                     // printf("Decoded CAN message: ID: %08lX, DLC: %d, Data: \n", msg.msg.id, msg.msg.dlc);
// //                     // for (int i = 0; i < message.dlc; i++)
// //                     // {
// //                     //     printf("%02X, ", message.data[i]);
// //                     // }
// //                     // printf("\n");
// //                     if (bytes_read > decoded_length)
// //                     {
// //                         data_buffer_ptr += decoded_length;
// //                         bytes_read -= decoded_length;
// //                     }
// //                     else
// //                     {
// //                         break;
// //                     }
// //                 }
// //                 else
// //                 {
// //                     // printf("decode failed: %d\n", dec_ret);
// //                     break;
// //                 }
// //             }

// //             nx_packet_release(data_packet);
// //         }
// //         else
// //         {
// //             tx_thread_sleep(1);
// //         }
// //     }
// // }

// // void send_thread_entry(ULONG thread_input)
// // {
// //     UINT ret;
// //     NX_PACKET *data_packet;
// //     UCHAR data_buffer[128];
// //     uint32_t message_length;

// //     printf("start send thread\n");

// //     while (1)
// //     {
// //         if (!can_rx_msg_queue.empty())
// //         {
// //             CANMessageWithCh msg;
// //             tx_mutex_get(&can_queue_mutex, TX_WAIT_FOREVER);
// //             msg = can_rx_msg_queue.front();
// //             can_rx_msg_queue.pop();
// //             tx_mutex_put(&can_queue_mutex);

// //             // printf("can send\n");
// //             encode_can_message(msg, data_buffer, sizeof(data_buffer), &message_length);

// //             ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
// //             if (ret != NX_SUCCESS)
// //             {
// //                 Error_Handler();
// //             }

// //             ret = nx_packet_data_append(data_packet, data_buffer, message_length, &NxAppPool, TX_WAIT_FOREVER);
// //             if (ret != NX_SUCCESS)
// //             {
// //                 nx_packet_release(data_packet);
// //                 Error_Handler();
// //             }

// //             ret = nx_tcp_socket_send(&TCPSocket, data_packet, TX_WAIT_FOREVER);
// //             if (ret != NX_SUCCESS)
// //             {
// //                 nx_packet_release(data_packet);
// //                 // printf("nx_tcp_socket_send error %d\n", ret);
// //                 // Error_Handler();
// //             }
// //         }
// //         else
// //         {
// //             tx_thread_sleep(1);
// //         }
// //     }
// // }

// // void cleanup_and_relisten(void)
// // {
// //     // スレッドの削除
// //     tx_thread_terminate(&receive_thread);
// //     tx_thread_delete(&receive_thread);

// //     tx_thread_terminate(&send_thread);
// //     tx_thread_delete(&send_thread);

// //     // ソケットの切断とリセット
// //     nx_tcp_socket_disconnect(&TCPSocket, TX_WAIT_FOREVER);
// //     nx_tcp_server_socket_unaccept(&TCPSocket);
// //     nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket);
// // }

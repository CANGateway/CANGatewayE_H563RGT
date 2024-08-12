// #include "app_netxduo.h"
// #include "gateway_channel.hpp"
// #include "main.h"
// #include "stmbed/digital_out.hpp"
// #include <cstring>
// #include <queue>
// #include <stdbool.h>
// #include <string.h>

// #include "unit_test.hpp"

// extern UART_HandleTypeDef huart3;
// extern NX_IP NetXDuoEthIpInstance;
// extern NX_PACKET_POOL NxAppPool;

// extern FDCAN_HandleTypeDef hfdcan1;
// extern FDCAN_HandleTypeDef hfdcan2;

// VOID App_Link_Thread_Entry(ULONG thread_input);

// extern "C" VOID cangateway_main(ULONG thread_input) {
//     using namespace threadx;
//     printf("\nmyMain start\n");

//     // unit_test::can_test();
//     // unit_test::thread_cpp_test();
//     // unit_test::thread_can_test();

//     std::array<stmbed::CAN, 2> can = {
//         stmbed::CAN(&hfdcan1),
//         stmbed::CAN(&hfdcan2),
//     };

//     ULONG IpAddress;
//     ULONG NetMask;

//     for (size_t i = 0; i < 5; i++)
//         tx_thread_sleep(20000);

//     UINT ret;
//     ret = nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
//     if (ret != TX_SUCCESS) {
//         Error_Handler();
//     } else {
//         PRINT_IP_ADDRESS(IpAddress);
//     }

//     using TCPSocketType = netxduo::tcp_socket<256, 256>;
//     std::unique_ptr<TCPSocketType> tcp_socket_;

//     const uint32_t server_ip_address = IP_ADDRESS(192, 168, 0, 1);
//     const uint16_t server_port = 6000;
//     const uint16_t port = 6000;
//     std::unique_ptr<thread> test_thread = std::make_unique<static_thread<1024>>("Test Thread", [&](ULONG) {
//         printf("test cannalloni thread\n");

//         tcp_socket_ = std::make_unique<TCPSocketType>(&NetXDuoEthIpInstance);
//         // tcp_socket_->listen(port_, MAX_TCP_CLIENTS);
//         // printf("TCP Server listening on PORT %d ..\n", port_);

//         tcp_socket_->bind(port);

//         while (1) {

//             // printf("wait accept\n");
//             // tcp_socket_->accept();
//             printf("wait connect\n");
//             tcp_socket_->connect(server_ip_address, server_port);

//             //            std::vector<uint8_t> send_buf;
//             //            std::vector<stmbed::CANMessage> msgs = {
//             //                stmbed::CANMessage(0x123, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}),
//             //            };
//             //            send_buf.resize(32);

//             std::string start_str = "CANNELLONIv1";
//             tcp_socket_->send_str(start_str);

//             printf("tp1\n");
//             this_thread::sleep_for(20000);
//             while (tcp_socket_->is_connected()) {
//                 std::vector<uint8_t> send_buf;
//                 std::vector<stmbed::CANMessage> msgs = {
//                     stmbed::CANMessage(0x123, {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44}),
//                 };
//                 send_buf.resize(32);
//                 // printf("tp2\n");
//                 size_t packet_size = cannelloni_build_packet_header(send_buf.data(), msgs);
//                 printf("send packet_size: %d\n", packet_size);
//                 tcp_socket_->send(send_buf.data(), packet_size);
//                 // printf("tp3\n");

//                 this_thread::sleep_for(50000);
//             }

//             printf("disconnect\n");
//             tcp_socket_->disconnect();
//             printf("disconnect tp1\n");
//             tcp_socket_->relisten(port_);
//             printf("disconnect tp2\n");
//         }
//     });

//     std::unique_ptr<thread> link_thread = std::make_unique<static_thread<1024>>("Link Thread",
//     App_Link_Thread_Entry);

//     while (1) {
//         this_thread::sleep_for(1000);
//     }
// }

// VOID App_Link_Thread_Entry(ULONG thread_input) {
//     ULONG actual_status;
//     UINT linkdown = 0, status;
//     stmbed::DigitalOut led1(LED1_GPIO_Port, LED1_Pin);

//     printf("App_Link_Thread_Entry\n");

//     while (1) {
//         /* Get Physical Link status. */
//         status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_LINK_ENABLED, &actual_status, 10);

//         if (status == NX_SUCCESS) {
//             if (linkdown == 1) {
//                 linkdown = 0;
//                 status =
//                     nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_ADDRESS_RESOLVED, &actual_status,
//                     10);
//                 if (status == NX_SUCCESS) {
//                     /* The network cable is connected again. */
//                     printf("The network cable is connected again.\n");
//                     /* Print TCP Server is available again. */
//                     printf("TCP Server is available again.\n");
//                 } else {
//                     /* The network cable is connected. */
//                     printf("The network cable is connected.\n");
//                     /* Send command to Enable Nx driver. */
//                     nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_ENABLE, &actual_status);
//                     /* Restart DHCP Client. */
//                     // nx_dhcp_stop(&DHCPClient);
//                     // nx_dhcp_start(&DHCPClient);
//                 }
//             }
//         } else {
//             if (0 == linkdown) {
//                 linkdown = 1;
//                 /* The network cable is not connected. */
//                 printf("The network cable is not connected.\n");
//             }
//         }

//         led1 = !linkdown;

//         tx_thread_sleep(NX_APP_CABLE_CONNECTION_CHECK_PERIOD);
//     }
// }
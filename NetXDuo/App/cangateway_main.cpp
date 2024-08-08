#include "app_netxduo.h"
#include "gateway_channel.hpp"
#include "main.h"
#include "stmbed/digital_out.hpp"
#include <cstring>
#include <queue>
#include <stdbool.h>
#include <string.h>

#include "unit_test.hpp"

extern UART_HandleTypeDef huart3;
extern NX_IP NetXDuoEthIpInstance;
extern NX_PACKET_POOL NxAppPool;

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

VOID App_Link_Thread_Entry(ULONG thread_input);

extern "C" VOID cangateway_main(ULONG thread_input) {
    using namespace threadx;
    printf("\nmyMain start\n");

    // unit_test::can_test();
    // unit_test::thread_cpp_test();
    // unit_test::thread_can_test();

    std::array<stmbed::CAN, 2> can = {
        stmbed::CAN(&hfdcan1),
        stmbed::CAN(&hfdcan2),
    };

    ULONG IpAddress;
    ULONG NetMask;

    UINT ret;
    ret = nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
    if (ret != TX_SUCCESS) {
        Error_Handler();
    } else {
        PRINT_IP_ADDRESS(IpAddress);
    }

    std::array<GatewayChannel, 2> channels = {
        GatewayChannel(&NetXDuoEthIpInstance, 6000, can[0]),
        GatewayChannel(&NetXDuoEthIpInstance, 6001, can[1]),
    };

    channels[0].start();
    channels[1].start();

    std::unique_ptr<thread> link_thread = std::make_unique<static_thread<1024>>("Link Thread", App_Link_Thread_Entry);

    while (1) {
        this_thread::sleep_for(1000);
    }
}

VOID App_Link_Thread_Entry(ULONG thread_input) {
    ULONG actual_status;
    UINT linkdown = 0, status;
    stmbed::DigitalOut led1(LED1_GPIO_Port, LED1_Pin);

    printf("App_Link_Thread_Entry\n");

    while (1) {
        /* Get Physical Link status. */
        status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_LINK_ENABLED, &actual_status, 10);

        if (status == NX_SUCCESS) {
            if (linkdown == 1) {
                linkdown = 0;
                status =
                    nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_ADDRESS_RESOLVED, &actual_status, 10);
                if (status == NX_SUCCESS) {
                    /* The network cable is connected again. */
                    printf("The network cable is connected again.\n");
                    /* Print TCP Server is available again. */
                    printf("TCP Server is available again.\n");
                } else {
                    /* The network cable is connected. */
                    printf("The network cable is connected.\n");
                    /* Send command to Enable Nx driver. */
                    nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_ENABLE, &actual_status);
                    /* Restart DHCP Client. */
                    // nx_dhcp_stop(&DHCPClient);
                    // nx_dhcp_start(&DHCPClient);
                }
            }
        } else {
            if (0 == linkdown) {
                linkdown = 1;
                /* The network cable is not connected. */
                printf("The network cable is not connected.\n");
            }
        }

        led1 = !linkdown;

        tx_thread_sleep(NX_APP_CABLE_CONNECTION_CHECK_PERIOD);
    }
}
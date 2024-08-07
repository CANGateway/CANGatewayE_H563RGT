#include "main.h"
#include "app_netxduo.h"
#include <stdbool.h>
#include <queue>
#include <cstring>
#include <string.h>

#include "gateway_channel.hpp"

#include "unit_test.hpp"

extern UART_HandleTypeDef huart3;
extern NX_IP NetXDuoEthIpInstance;
extern NX_PACKET_POOL NxAppPool;

ULONG IpAddress;
ULONG NetMask;

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

std::array<std::shared_ptr<stmbed::CAN>, 2> can;
std::array<std::unique_ptr<GatewayChannel>, 2> channel;

extern "C" VOID cangateway_main(ULONG thread_input)
{
    printf("\nmyMain start\n");

    // unit_test::can_test();
    // unit_test::thread_cpp_test();

    can[0] = std::make_shared<stmbed::CAN>(&hfdcan1);
    can[1] = std::make_shared<stmbed::CAN>(&hfdcan2);

    UINT ret;
    ret = nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
    if (ret != TX_SUCCESS)
    {
        Error_Handler();
    }
    else
    {
        PRINT_IP_ADDRESS(IpAddress);
    }

    channel[0] = std::make_unique<GatewayChannel>(&NetXDuoEthIpInstance, 6000, can[0]);
    channel[1] = std::make_unique<GatewayChannel>(&NetXDuoEthIpInstance, 6001, can[1]);

    channel[0]->start();
    channel[1]->start();

    while(1) {
        tx_thread_sleep(1000);
    }
}

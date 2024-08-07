#pragma once

#include "main.h"
#include "app_netxduo.h"
#include "threadx-cpp/thread.hpp"

#include <memory>

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

namespace unit_test
{

void can_test()
{
    printf("CAN Test\n");

    stmbed::CAN can1(&hfdcan1);
    stmbed::CAN can2(&hfdcan2);

    can1.attach(
        [&](const stmbed::CANMessage &msg) {
            printf("can1 recv: id: %x\n", msg.id);
        }
    );

    can2.attach(
        [&](const stmbed::CANMessage &msg) {
            printf("can2 recv: id: %x\n", msg.id);
        }
    );

    stmbed::CANMessage msg1(0x123, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07});
    stmbed::CANMessage msg2(0x456, {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17});

    while(1) {
        printf("can2 write\n");
        can2.write(msg2);
        threadx::this_thread::sleep_for(1000);

        printf("can1 write\n");
        can1.write(msg1);
        threadx::this_thread::sleep_for(1000);
    }
}

void thread_cpp_test()
{
    constexpr size_t STACK_SIZE = 1024;
    std::unique_ptr<threadx::static_thread<STACK_SIZE>> test_thread_;

    uint8_t cnt = 0;

    test_thread_ = std::make_unique<threadx::static_thread<STACK_SIZE>>(
        "Test Thread",
		[&](ULONG input) {
            while(1)
            {
                printf("Test Thread Entry %d\n", cnt++);
                threadx::this_thread::sleep_for(1000);
            }
        },
		0,
		10
    );

    while(1)
    {
        threadx::this_thread::sleep_for(1000);
    }
}

}

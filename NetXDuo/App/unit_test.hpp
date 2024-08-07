#pragma once

#include "main.h"
#include "app_netxduo.h"
#include "thread.hpp"

#include <memory>

namespace unit_test
{

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

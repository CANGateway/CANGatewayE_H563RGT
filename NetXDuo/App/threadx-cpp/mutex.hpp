#pragma once

#include "tx_api.h"
#include <cassert>

namespace threadx {

class mutex : public TX_MUTEX {
public:
    mutex(bool is_inherit = false) {
        auto result = tx_mutex_create(this, "Mutex", is_inherit ? TX_INHERIT : TX_NO_INHERIT);
        assert(result == TX_SUCCESS);
    }

    mutex(const mutex &) = delete;
    mutex &operator=(const mutex &) = delete;

    void lock() {
        auto result = tx_mutex_get(this, TX_WAIT_FOREVER);
        assert(result == TX_SUCCESS);
    }

    bool try_lock() {
        auto result = tx_mutex_get(this, TX_NO_WAIT);
        return result == TX_SUCCESS;
    }

    void unlock() {
        auto result = tx_mutex_put(this);
        assert(result == TX_SUCCESS);
    }

    TX_MUTEX *native_handle() { return this; }
};

} // namespace threadx
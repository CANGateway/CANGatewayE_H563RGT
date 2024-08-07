#pragma once

#include "tx_api.h"
#include <string>
#include <functional>
#include <cassert>

namespace threadx {

struct priority {
    constexpr priority()
            : value_(1)
    { }
    constexpr priority(UINT value)
            : value_(value)
    { }
    operator UINT&()
    {
        return value_;
    }
    constexpr operator UINT() const
    {
        return value_;
    }

    static constexpr priority max()
    {
        return TX_MAX_PRIORITIES;
    }
    static constexpr priority min()
    {
        return 0;
    }

private:
    UINT value_;
};

// struct thread_config {
//     thread_config(char *name) : name_(name) { };

//     thread_config& set_name(char *name) { name_ = name; return *this; }
//     char *name() const { return name_; }

// private:
//     char *name_;
// };

class thread : TX_THREAD{

public:
    thread(std::function<void(ULONG)> entry, const std::string name, ULONG input, void *pstack, ULONG stack_size, priority prio) :
        name_(name),
        entry_(entry),
        input_(input),
        pstack_(pstack),
        stack_size_(stack_size),
        prio_(prio)
    {
        auto result = tx_thread_create(
                this,
                const_cast<char*>(name_.c_str()),
                raw_callback,
                reinterpret_cast<ULONG>(this),
                pstack,
                stack_size,
                prio,                       // UINT priority
                prio,                       // UINT preempt_threshold
                TX_NO_TIME_SLICE,           // ULONG time_slice
                TX_AUTO_START);             // UINT auto_start
        assert(result == TX_SUCCESS);
    }

    ~thread() {
        if (tx_thread_state != TX_COMPLETED)
        {
            auto result = tx_thread_terminate(this);
            assert(result == TX_SUCCESS);
        }
        auto result = tx_thread_delete(this);
        assert(result == TX_SUCCESS);
    }

    // void join() {
    //     tx_thread_wait_abort(this);
    // }

    // void detach() {
    //     tx_thread_terminate(this);
    // }

    // bool joinable() {
    //     return (tx_thread_state == TX_READY);
    // }

    void suspend() {
        tx_thread_suspend(this);
    }

    void resume() {
        tx_thread_resume(this);
    }

    // ULONG get_id() {
    // }

private:
    static void raw_callback(ULONG thread_ptr){
        thread* t = reinterpret_cast<thread*>(thread_ptr);
        t->entry_(t->input_);
    }

private:
    const std::string name_;
    std::function<void(ULONG)> entry_;
    ULONG input_;
    void *pstack_;
    ULONG stack_size_;
    priority prio_;
};

template<size_t STACK_SIZE>
class static_thread : public thread {
public:
    static_thread(const std::string &name, std::function<void(ULONG)> entry, ULONG input = 0, priority prio = priority(10)) :
        thread(entry, name, input, stack_, STACK_SIZE, prio)
    { }

    ~static_thread() = default;

private:
    char stack_[STACK_SIZE];
};

namespace this_thread {
    thread* get_id() {
        return reinterpret_cast<thread*>(tx_thread_identify());
    }

    void sleep_for(ULONG ms) {
        tx_thread_sleep(ms);
    }
}

} // namespace threadx

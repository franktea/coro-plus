/*
 * coropp.h
 *
 *  Created on: Jun 11, 2019
 *      Author: frank
 */

#ifndef COROPP_H_
#define COROPP_H_

#include <chrono>
#include <stdint.h>
#include <map>
#include <assert.h>
#include <boost/context/detail/fcontext.hpp>

namespace CoroPP
{

using boost::context::detail::fcontext_t;
using boost::context::detail::transfer_t;
using boost::context::detail::jump_fcontext;
using boost::context::detail::make_fcontext;
using boost::context::detail::ontop_fcontext;

class Coro;
class Scheduler;

template<std::size_t Default>
class simple_stack_allocator
{
public:
    static std::size_t default_stacksize()
    { return Default; }

    void * allocate( std::size_t size) const
    {
        void * limit = malloc( size);
        if ( ! limit) throw std::bad_alloc();

        return static_cast< char * >( limit) + size;
    }

    void deallocate( void * vp, std::size_t size) const
    {
        void * limit = static_cast< char * >( vp) - size;
        free( limit);
    }
};

typedef simple_stack_allocator<64 * 1024> stack_allocator;

stack_allocator alloc;

fcontext_t main_ = nullptr; // main coroutine

class Coro;
Coro* current_coro_ = nullptr; // current coroutine, not main

enum CoroStatus
{
    CS_CREATED,
    CS_RUNNING,
    CS_FINISHED,
};

class Coro
{
    friend class Scheduler;
public:
    Coro() : t_(nullptr), status_(CS_CREATED)
    {
        sp_ = alloc.allocate(stack_allocator::default_stacksize());
    }

    void Run()
    {
        status_ = CS_RUNNING;
        func_();
    }
public:
    void Resume()
    {
        assert(current_coro_ == nullptr);
        current_coro_ = this; // 设置好需要跳转的协程，再跳转
        transfer_t t = jump_fcontext(t_, 0); // 跳到协程的函数中去

        // 从协程里面返回到main了
        t_ = t.fctx;
    }

    virtual ~Coro() {}
public:
    void* sp_;
    fcontext_t t_;
    CoroStatus status_;
    std::function<void()> func_;
};

void Yield()
{
    assert(current_coro_ != nullptr);
    current_coro_ = nullptr; // 先设置成null再跳出
    transfer_t t = jump_fcontext(main_, 0); //  跳到main里面去

    // 从main里面返回到当前协程里面了
    main_ = t.fctx;
}

class CoroContext
{
private:
    fcontext_t f_;
};


void Entry(transfer_t t)
{
    Coro* coro = (Coro*)t.data;
    main_ = t.fctx;
    current_coro_ = coro;
    coro->Run();
    coro->status_ = CS_FINISHED;
    current_coro_ = nullptr;
    std::cout<<"heiheiheihei\n";
    jump_fcontext(main_, 0);
}

template<class Func>
Coro* Create(Func&& func) {
    Coro* coro = new Coro;
    coro->func_ = std::move(func);
    fcontext_t ctx = make_fcontext(coro->sp_, stack_allocator::default_stacksize(), Entry);
    transfer_t t = jump_fcontext( ctx, coro); // 跳到协程的入口函数：Entry

    // 从协程里面返回到main了
    coro->t_ = t.fctx;
    std::cout<<"lalalalalal\n";
    return coro;
}

/*
class Scheduler
{
public:
    template<class F>
    bool Dispatch(F&& f);
    int32_t Run();
    int32_t RunFor(std::chrono::duration duration);
};
*/
} //namespace CoroPP
#endif /* COROPP_H_ */

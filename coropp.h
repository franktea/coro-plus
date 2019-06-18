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
#include <vector>
#include <ctime>
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

using CoroID = struct {
    time_t time_stamp;
    uint32_t index;
};

inline bool operator<(const CoroID& lhs, const CoroID& rhs)
{
    return lhs.time_stamp < rhs.time_stamp ||
            (lhs.time_stamp == rhs.time_stamp && lhs.index < rhs.index);
}

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

class Coro;

class Scheduler
{
    friend class Coro;
    friend void Yield();
    friend void Entry(transfer_t t);
public:
    Scheduler(): main_(nullptr), current_coro_(nullptr), id_(0) {}

    // find coro by id, return null if not found.
    Coro* FindCoro(const CoroID id)
    {
        auto it = running_coros_.find(id);
        if(it == running_coros_.end())
        {
            return nullptr;
        }

        return it->second;
    }

    template<class Func>
    Coro* Spawn(Func&& f);

    int32_t Run();

    template< class Rep, class Period >
    int32_t RunFor(std::chrono::duration<Rep, Period> duration);

    static Scheduler& Instance()
    {
        static Scheduler* p = new Scheduler;
        return *p;
    }
    Scheduler(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;
private:
    CoroID NextID()
    {
       if(++id_ == 0)
       {
           ++id_;
       }
       return CoroID { std::time(nullptr), id_ };
    }
private:
    fcontext_t main_;
    Coro* current_coro_;
private:
    uint32_t id_;
    const size_t max_coros_ = 100; // 最多允许同时开启的协程个数
    std::map<CoroID, Coro*> running_coros_;
    std::vector<Coro*> free_list_;
};

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
    Coro(const CoroID id) : t_(nullptr), status_(CS_CREATED), id_(id)
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
        assert(Scheduler::Instance().current_coro_ == nullptr);
        Scheduler::Instance().current_coro_ = this; // 设置好需要跳转的协程，再跳转
        transfer_t t = jump_fcontext(t_, 0); // 跳到协程的函数中去

        // 从协程里面返回到main了
        t_ = t.fctx;
    }

    virtual ~Coro() {}
public:
    void* sp_;
    fcontext_t t_;
    CoroStatus status_;
    const CoroID id_;
    std::function<void()> func_;
};

void Yield()
{
    assert(Scheduler::Instance().current_coro_ != nullptr);
    Scheduler::Instance().current_coro_ = nullptr; // 先设置成null再跳出
    transfer_t t = jump_fcontext(Scheduler::Instance().main_, 0); //  跳到main里面去

    // 从main里面返回到当前协程里面了
    Scheduler::Instance().main_ = t.fctx;
}

inline void Entry(transfer_t t)
{
    Coro* coro = (Coro*)t.data;
    Scheduler::Instance().main_ = t.fctx;
    Scheduler::Instance().current_coro_ = coro;
    coro->Run();
    coro->status_ = CS_FINISHED;
    Scheduler::Instance().current_coro_ = nullptr;
    std::cout<<"heiheiheihei\n";
    jump_fcontext(Scheduler::Instance().main_, 0);
}

template<class Func>
inline Coro* CoroPP::Scheduler::Spawn(Func&& f)
{
    {
        if(running_coros_.size() + free_list_.size() > max_coros_)
        {
            //TODO: 协程个数达到上限
            return nullptr;
        }

        CoroID id = Scheduler::Instance().NextID();
        Coro* coro = new Coro(id);
        coro->func_ = std::move(f);
        fcontext_t ctx = make_fcontext(coro->sp_, stack_allocator::default_stacksize(), Entry);
        transfer_t t = jump_fcontext( ctx, coro); // 跳到协程的入口函数：Entry

        // 从协程里面返回到main了
        coro->t_ = t.fctx;
        std::cout<<"lalalalalal\n";

        return coro;
    }
}

} //namespace CoroPP

#endif /* COROPP_H_ */

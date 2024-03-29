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
#include <any>
#include <vector>
#include <ctime>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <assert.h>
#include <boost/context/detail/fcontext.hpp>
#if defined(__APPLE__) || defined(__MACH__) || defined(__linux__)
#include <sys/mman.h>
#endif

namespace CoroPP
{

using boost::context::detail::fcontext_t;
using boost::context::detail::transfer_t;
using boost::context::detail::jump_fcontext;
using boost::context::detail::make_fcontext;

class Coro;
class CoroPool;

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

class CoroPool
{
    friend class Coro;
    friend void Yield();

    template<class Rep, class Period>
    friend bool Yield(std::chrono::duration<Rep, Period> timeout);

    friend void Entry(transfer_t t);

    struct TimeOut {}; // 一个空的类型，用来标识超时
public:
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

    template< class Rep, class Period >
    int32_t ProcessTimers(std::chrono::duration<Rep, Period> duration);

    static CoroPool& Instance()
    {
        static CoroPool* p = new CoroPool;
        return *p;
    }
    CoroPool(const CoroPool&) = delete;
    CoroPool(CoroPool&&) = delete;
    CoroPool& operator=(const CoroPool&) = delete;
    CoroPool& operator=(CoroPool&&) = delete;

    size_t TimerCount()
    {
        return timers_.size();
    }

    Coro* CurrentCoro()
    {
        return current_coro_;
    }
private:
    CoroPool(): main_(nullptr), current_coro_(nullptr), id_(0) {}

    CoroID NextID()
    {
       if(++id_ == 0)
       {
           ++id_;
       }
       return CoroID { std::time(nullptr), id_ };
    }

    inline void Recyle(Coro* coro); // 回收协程资源
private:
    fcontext_t main_;
    Coro* current_coro_;
private:
    uint32_t id_;
    const size_t max_coros_ = 1000; // 最多允许同时开启的协程个数
    std::map<CoroID, Coro*> running_coros_;
    std::vector<Coro*> free_list_;
    using TimerManager = boost::bimap<boost::bimaps::multiset_of<long long>, boost::bimaps::set_of<CoroID>>;
    TimerManager timers_;
};

enum CoroStatus
{
    CS_CREATED,
    CS_RUNNING,
    CS_FINISHED,
    CS_RECYCLED
};

class Coro
{
    friend class CoroPool;
public:
    Coro() : t_(nullptr), status_(CS_CREATED), id_({0, 0})
    {
        sp_ = alloc.allocate(stack_allocator::default_stacksize());
#if defined(__APPLE__) || defined(__MACH__) || defined(__linux__)
//TODO: mprotect
#endif
    }

    void Run()
    {
        status_ = CS_RUNNING;
        func_(id_);
    }

    bool TimeOut()
    {
        try {
            std::any_cast<CoroPool::TimeOut>(param_);
            param_.reset();
            return true;
        } catch (const std::bad_any_cast& e) {}
        return false;
    }
public:
    void Resume() // 正常的Resume，如果有定时器需要在此函数中删除定时器
    {
        assert(CoroPool::Instance().current_coro_ == nullptr);
        CoroPool::Instance().current_coro_ = this; // 设置好需要跳转的协程，再跳转

        if(param_.has_value()) // 判断是否有定时器，有就删除
        {
            try {
                CoroID id = std::any_cast<CoroID>(param_);
                param_.reset();
                //删除定时器
                auto deleted = CoroPool::Instance().timers_.right.erase(id);
                assert(deleted == 1); // 应该是删除有且仅有一个定时器
            } catch(const std::bad_any_cast&) {}
        }

        transfer_t t = jump_fcontext(t_, &param_); // 跳到协程的函数中去

        // 从协程里面返回到main了
        t_ = t.fctx;
    }

    virtual ~Coro() {}
private:
    void Resume(CoroPool::TimeOut) // 超时的时候调用Resume，由scheduler在处理定时器的时候调用
    {
        assert(CoroPool::Instance().current_coro_ == nullptr);
        CoroPool::Instance().current_coro_ = this; // 设置好需要跳转的协程，再跳转
        transfer_t t = jump_fcontext(t_, &param_); // 跳到协程的函数中去

        // 从协程里面返回到main了
        t_ = t.fctx;
    }

    void Clear() // 内存回收的时候把除了sp以外的变量都清空
    {
        t_ = nullptr;
        status_ = CS_RECYCLED;
        id_ = {0, 0};
        param_.reset();
        func_ = nullptr;
    }
public:
    void* sp_;
    fcontext_t t_;
    CoroStatus status_;
    CoroID id_;
    std::any param_; // 用于在协程间传参，记录超时或者传参信息。用any来保存实际类型，是为了避免用void*
    std::function<void(CoroID id)> func_;
};

void Yield()
{
    assert(CoroPool::Instance().current_coro_ != nullptr);
    CoroPool::Instance().current_coro_ = nullptr; // 先设置成null再跳出
    transfer_t t = jump_fcontext(CoroPool::Instance().main_, 0); //  跳到main里面去

    // 从main里面返回到当前协程里面了
    CoroPool::Instance().main_ = t.fctx;
}

// yield的同时设置超时时间，在timeout内如果没有Resume则定时器会触发，当前异步调用超时
// 返回值：如果超时，则返回true
template<class Rep, class Period>
bool Yield(std::chrono::duration<Rep, Period> timeout)
{
    assert(CoroPool::Instance().current_coro_ != nullptr);

    // 先添加定时器
    Coro* coro = CoroPool::Instance().current_coro_;
    using namespace std::chrono;
    time_point<system_clock> now = system_clock::now();
    long long millis = duration_cast<milliseconds>(now.time_since_epoch()).count() +
            duration_cast<milliseconds>(timeout).count();
    auto[it, inserted] = CoroPool::Instance().timers_.insert(CoroPool::TimerManager::value_type(millis, coro->id_));
    assert(inserted);
    coro->param_ = coro->id_; // 当前协程带了定时器，用coro id标记，正常Resume的时候根据此值删除定时器

    // 再调用Yield
    CoroPool::Instance().current_coro_ = nullptr; // 先设置成null再跳出
    transfer_t t = jump_fcontext(CoroPool::Instance().main_, 0); //  跳到main里面去

    // 从main里面返回到当前协程里面了
    CoroPool::Instance().main_ = t.fctx;
    assert(coro == CoroPool::Instance().current_coro_);
    return coro->TimeOut();
}

inline void Entry(transfer_t t)
{
    Coro* coro = (Coro*)t.data;
    CoroPool::Instance().main_ = t.fctx;
    CoroPool::Instance().current_coro_ = coro;
    coro->Run();
    coro->status_ = CS_FINISHED;
    //协程结束了，回收该资源
    CoroPool::Instance().Recyle(CoroPool::Instance().current_coro_);
    CoroPool::Instance().current_coro_ = nullptr;
    jump_fcontext(CoroPool::Instance().main_, 0);
}

template<class Func>
inline Coro* CoroPP::CoroPool::Spawn(Func&& f)
{
    // 这个函数只能在main里面调用，协程里面不允许嵌套协程
    if(current_coro_)
    {
        std::cerr<<"spawn must be called in main\n";
        return nullptr;
    }

    if(free_list_.empty() && (running_coros_.size() >= max_coros_))
    {
        // 协程个数达到上限
        std::cerr<<"too many coros created\n";
        return nullptr;
    }

    Coro* coro = nullptr;

    if(!free_list_.empty())
    {
        coro = free_list_.back();
        free_list_.pop_back();
        std::cout<<"free list not empty, reuse\n";
    }
    else
    {
        coro = new Coro();
    }

    CoroID id = CoroPool::Instance().NextID();
    coro->id_ = id;
    coro->func_ = std::move(f);
    fcontext_t ctx = make_fcontext(coro->sp_, stack_allocator::default_stacksize(), Entry);
    transfer_t t = jump_fcontext( ctx, coro); // 跳到协程的入口函数：Entry

    // 从协程里面返回到main了
    coro->t_ = t.fctx;

    running_coros_.insert(std::make_pair(id, coro));
    return coro;
}

void CoroPP::CoroPool::Recyle(Coro* coro) // 回收协程资源
{
    assert(coro->status_ == CS_FINISHED);
    running_coros_.erase(coro->id_);
    coro->Clear();
    free_list_.push_back(coro);
}

template<class Rep, class Period>
inline int32_t CoroPP::CoroPool::ProcessTimers(
        std::chrono::duration<Rep, Period> duration)
{
    //this function should run in main
    assert(current_coro_ == 0);

    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    auto passed = now.time_since_epoch();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(passed).count();
    while(!timers_.empty() && timers_.left.begin()->first <= ms)
    {
        auto it = timers_.left.begin();
        timers_.left.erase(it);
        auto coro_it = running_coros_.find(it->second);
        if(coro_it == running_coros_.end())
        {
            std::cerr<<"cannot find coro on timeout\n";
        }
        else
        {
            Coro* coro = coro_it->second;
            coro->param_ = TimeOut{};
            coro->Resume(TimeOut{});
        }
    }

    return 0;
}

} //namespace CoroPP

#endif /* COROPP_H_ */

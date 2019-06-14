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

template< std::size_t Max, std::size_t Default, std::size_t Min >
class simple_stack_allocator
{
public:
    static std::size_t maximum_stacksize()
    { return Max; }

    static std::size_t default_stacksize()
    { return Default; }

    static std::size_t minimum_stacksize()
    { return Min; }

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

typedef simple_stack_allocator<
            8 * 1024 * 1024, 64 * 1024, 8 * 1024
        > stack_allocator;

stack_allocator alloc;

fcontext_t main_; // jump to main

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
        DoRun();
    }

    virtual void DoRun()
    {
        std::cout<<"please override this\n";
        Yield();
        std::cout<<"111111111\n";
    }
public:
    void Resume()
    {
        transfer_t t = jump_fcontext(t_, 0);
        t_ = t.fctx;
    }

    void Yield()
    {
        transfer_t t = jump_fcontext(main_, 0);
        main_ = t.fctx;
    }

    virtual ~Coro() {}
public:
    void* sp_;
    fcontext_t t_;
    CoroStatus status_;
};

class CoroContext
{
private:
    fcontext_t f_;
};


void Entry(transfer_t t)
{
    Coro* coro = (Coro*)t.data;
    main_ = t.fctx;
    coro->Run();
    coro->status_ = CS_FINISHED;
    jump_fcontext(main_, 0);
}


void Create(Coro* coro) {
    fcontext_t ctx = make_fcontext( coro->sp_, stack_allocator::default_stacksize(), Entry);
    transfer_t t = jump_fcontext( ctx, coro);
    coro->t_ = t.fctx;
    std::cout<<"lalalalalal\n";
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

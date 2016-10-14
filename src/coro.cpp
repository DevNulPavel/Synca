/*
 * Copyright 2014 Grigory Demchenko (aka gridem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "coro.h"
#include "helpers.h"

#define CLOG(D_msg)                 LOG(coro::isInsideCoro() << ": " << D_msg)

namespace coro {

TLS Coro* t_coro = nullptr;
const size_t STACK_SIZE = 1024*32;


struct ContextSwitchInfo{
    boost::context::detail::fcontext_t* saveContextPtr;
    void* data;
};


// switch context from coroutine
void yield()
{
    VERIFY(isInsideCoro(), "yield() outside coro");
    t_coro->yield0();
}

// checking that we are inside coroutine
bool isInsideCoro()
{
    return t_coro != nullptr;
}

Coro::Coro()
{
    init0();
}

Coro::Coro(Handler handler)
{
    init0();
    start(std::move(handler));
}

Coro::~Coro()
{
    if (isStarted())
        RLOG("Destroying started coro");
}

void Coro::start(Handler handler)
{
    VERIFY(!isStarted(), "Trying to start already started coro");
    context = boost::context::detail::make_fcontext(&stack.back(), stack.size(), &starterWrapper0);
    jump0(reinterpret_cast<intptr_t>(&handler));
}

// continue coroutine execution after yield
void Coro::resume()
{
    VERIFY(started, "Cannot resume: not started");
    VERIFY(!running, "Cannot resume: in running state");
    jump0();
}

// is coroutine was started and not completed
bool Coro::isStarted() const
{
    return started || running;
}

void Coro::init0()
{
    started = false;
    running = false;
    context = nullptr;
    savedContext = nullptr;
    stack.resize(STACK_SIZE);
}

// returns to saved context
void Coro::yield0()
{
    ContextSwitchInfo info;
    info.saveContextPtr = &context;
    info.data = nullptr;
    boost::context::detail::jump_fcontext(savedContext, 0);
}

void Coro::jump0(intptr_t p)
{
    Coro* old = this;
    std::swap(old, t_coro);
    running = true;
    
    ContextSwitchInfo info;
    info.saveContextPtr = &savedContext;
    info.data = (void*)p;
    
    boost::context::detail::jump_fcontext(context, (void*)(&info));
    running = false;
    std::swap(old, t_coro);
    if (exc != std::exception_ptr())
        std::rethrow_exception(exc);
}

void Coro::starterWrapper0(boost::context::detail::transfer_t p)
{
    ContextSwitchInfo* info = reinterpret_cast<ContextSwitchInfo*>(p.data);
    if (info) {
        // обновляем старый контекст
        *(info->saveContextPtr) = p.fctx;
        // данные
        intptr_t data = (intptr_t)info->data;
        t_coro->starter0(data);
    }else{
        t_coro->starter0(0);
    }
}

void Coro::starter0(intptr_t p)
{
    started = true;
    try
    {
        exc = nullptr;
        if(p != 0){
            Handler handler = std::move(*reinterpret_cast<Handler*>(p));
            handler();
        }
    }
    catch (...)
    {
        exc = std::current_exception();
    }
    started = false;
    yield0();
}

}

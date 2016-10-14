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

// switch context from coroutine
void yield() {
    VERIFY(isInsideCoro(), "yield() outside coro");
    t_coro->yield0();
}

// checking that we are inside coroutine
bool isInsideCoro() {
    return t_coro != nullptr;
}

Coro::Coro() {
    init0();
}

Coro::Coro(Handler handler) {
    init0();
    start(std::move(handler));
}

Coro::~Coro() {
    if (isStarted())
        RLOG("Destroying started coro");
}

void Coro::start(Handler handler) {
    VERIFY(!isStarted(), "Trying to start already started coro");
    coroutine = new PushCoroutine([this](PullCoroutine& source) {
        savedCoroutine = &source;

        const Handler& handler = source.get();
        starter0(handler);
        return;
    });
    jump0(handler);
}

// continue coroutine execution after yield
void Coro::resume() {
    VERIFY(started, "Cannot resume: not started");
    VERIFY(!running, "Cannot resume: in running state");
    jump0(nullptr);
}

// is coroutine was started and not completed
bool Coro::isStarted() const {
    return started || running;
}

void Coro::init0() {
    started = false;
    running = false;
}

// returns to saved context
void Coro::yield0() {
    if (savedCoroutine) {
        (*savedCoroutine)();
    }
}

void Coro::jump0(const Handler& p) {
    Coro* old = this;
    std::swap(old, t_coro);
    running = true;

    (*coroutine)(p);

    running = false;
    std::swap(old, t_coro);
    if (exc != std::exception_ptr())
        std::rethrow_exception(exc);
}

void Coro::starter0(const Handler& handler) {
    started = true;
    try {
        exc = nullptr;
        if(handler != 0) {
            handler();
        }
    } catch (...) {
        exc = std::current_exception();
    }
    started = false;
    yield0();
}

}

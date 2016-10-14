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

#pragma once

#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "common.h"

// thread log: outside coro
#define  TLOG(D_msg)             LOG(mt::name() << "#" << mt::number() << ": " << D_msg)
#define RTLOG(D_msg)            RLOG(mt::name() << "#" << mt::number() << ": " << D_msg)

// multithreading
namespace mt {

typedef boost::asio::io_service IoService;
typedef boost::asio::io_service::work Work;

const char* name();
int number();

std::thread createThread(const Handler& handler, int number, const char* name = "");

struct IScheduler : IObject {
    virtual void schedule(Handler handler) = 0;
    virtual const char* name() const {
        return "<unknown>";
    }
};

struct IService : IObject {
    virtual IoService& ioService() = 0;
};

// пулл потоков
struct ThreadPool : IScheduler, IService {
    ThreadPool(size_t threadCount, const char* name = "");
    ~ThreadPool();

    void schedule(Handler handler);
    void wait();
    const char* name() const;

private:
    IoService& ioService();

    const char* _tpName;
    std::unique_ptr<Work> _work;
    IoService _service;
    std::vector<std::thread> _threads;  // массив потоков
    std::mutex _mutex;                  // блокировка
    std::condition_variable _cond;
    bool _toStop;
};

}

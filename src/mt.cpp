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

#include "mt.h"
#include "helpers.h"

// ThreadPool log: inside ThreadPool functionality
#define PLOG(D_msg)             TLOG("@" << this->name() << ": " << D_msg)

namespace mt {

// Переменные, индивидуальные для каждого потока
TLS int t_number = 0;
TLS const char* t_name = "main";

// геттеры для этих переменных
const char* name() {
    return t_name;
}
int number() {
    return t_number;
}

// Функция создания потока и выполнение задачи внутри этого потока
std::thread createThread(const Handler& handler, int number, const char* name) {
    return std::thread([handler, number, name] {
        t_number = number + 1;
        t_name = name;
        try
        {
            TLOG("thread created");
            handler();
            TLOG("thread ended");
        } catch (std::exception& e) {
            (void) e;
            TLOG("thread ended with error: " << e.what());
        }
    });
}

ThreadPool::ThreadPool(size_t threadCount, const char* name) :
    _tpName(name),
    _toStop(false) {
    
    _work.reset(new boost::asio::io_service::work(_service));
    _threads.reserve(threadCount);
    
    for (size_t i = 0; i < threadCount; ++ i){
        // код потока
        Handler threadCode = [this]() {
            while (true) {
                // запуск задачи сервиса
                _service.run();
                
                // блокировка
                std::unique_lock<std::mutex> lock(_mutex);
                
                // необохдимо ли завершить поток?
                if (_toStop){
                    break;
                }
                // нету задачи?
                if (!_work) {
                    _work.reset(new boost::asio::io_service::work(_service));
                    _service.reset();
                    
                    lock.unlock();
                    _cond.notify_all();
                }
            }
        };
        
        // создание потока + сохраняем поток
        _threads.emplace_back(createThread(threadCode, i, _tpName));
    }
    PLOG("thread pool created with threads: " << threadCount);
}

ThreadPool::~ThreadPool() {
    _mutex.lock();
    _toStop = true;
    _work.reset();
    _mutex.unlock();
    PLOG("join threads in pool");
    for (size_t i = 0; i < _threads.size(); ++ i){
        _threads[i].join();
    }
    PLOG("thread pool stopped");
}

// кидаем задачу на выполнение
void ThreadPool::schedule(Handler handler) {
    _service.post(std::move(handler));
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(_mutex);
    _work.reset();
    while (true) {
        _cond.wait(lock);
        TLOG("WAIT: waitCompleted: " << (_work != nullptr));
        if (_work)
            break;
    }
}

const char* ThreadPool::name() const {
    return _tpName;
}

IoService& ThreadPool::ioService() {
    return _service;
}

}

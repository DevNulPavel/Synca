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

#include "network.h"
#include "core.h"

namespace synca {
namespace net {

//////////////////////////////////////////////////////////////////
typedef boost::system::error_code Error;
typedef std::function<void(const Error&)> IoHandler;
typedef std::function<void(const Error&, size_t)> BufferIoHandler;
typedef std::function<void(IoHandler)> CallbackIoHandler;
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
// Вспомогательные функции
//////////////////////////////////////////////////////////////////

// вызов отложеной операции и коллбека после
void deferIo(const CallbackIoHandler& cb) {
    Error error;
    deferProceed([cb, &error](Handler proceed) {
        cb([proceed, &error](const Error& e) {
            error = e;
            proceed();
        });
    });
    if (!!error) {
        throw boost::system::system_error(error, "synca");
    }
}

// создает коллбек буффера
BufferIoHandler bufferIoHandler(Buffer& buffer, const IoHandler& proceed) {
     BufferIoHandler function = [&buffer, proceed](const Error& error, size_t size) {
        if (!error){
            buffer.resize(size);
        }
        proceed(error);
    };
    return function;
}

BufferIoHandler bufferIoHandler(const IoHandler& proceed) {
    BufferIoHandler function = [proceed](const Error& error, size_t size) {
        proceed(error);
    };
    return function;
}

//////////////////////////////////////////////////////////////////
// Socket class
//////////////////////////////////////////////////////////////////
Socket::Socket() :
    _socket(service<NetworkTag>()) {
}

Socket::Socket(Socket&& other):
    _socket(std::move(other._socket)){
}

boost::asio::ip::tcp::socket& Socket::getSocket(){
    return _socket;
}

// чтение данных в буффер размером с сам буффер
void Socket::read(Buffer& buffer) {
    CallbackIoHandler callback = [&buffer, this](IoHandler proceed) {
        // коллбек завершения чтения
        BufferIoHandler handler = bufferIoHandler(buffer, std::move(proceed));
        // TODO:
        // запуск чтения из сокета
        boost::asio::async_read(_socket,
                                boost::asio::buffer(&buffer[0], buffer.size()),
                                handler);
    };
    deferIo(callback);
}

void Socket::partialRead(Buffer& buffer) {
    CallbackIoHandler callback = [&buffer, this](IoHandler proceed) {
        // коллбек завершения чтения
        BufferIoHandler handler = bufferIoHandler(buffer, std::move(proceed));
        // TODO:
        // запуск чтения из сокета
        _socket.async_read_some(boost::asio::buffer(&buffer[0], buffer.size()),
                                handler);
    };
    deferIo(callback);
}

void Socket::readUntil(Buffer& buffer, const Buffer& stopValue) {
    CallbackIoHandler callback = [&buffer, stopValue, this](IoHandler proceed) {
        // коллбек завершения чтения
        BufferIoHandler handler = bufferIoHandler(buffer, std::move(proceed));
        auto asioBuffer = boost::asio::buffer(&buffer[0], buffer.size());
        
        // условие окончания
        auto completeCond = [&buffer, stopValue](const boost::system::error_code& error, std::size_t bytes_transferred){
            bool found = false;
            if (bytes_transferred >= stopValue.length()) {
                found = (0 == buffer.compare(bytes_transferred - stopValue.length(), stopValue.length(), stopValue));
            }
            return found;
        };
        // запуск чтения из сокета
        boost::asio::async_read(_socket,
                                asioBuffer,
                                completeCond,
                                handler);
    };
    deferIo(callback);
}

void Socket::write(const Buffer& buffer) {
    CallbackIoHandler callback = [&buffer, this](IoHandler proceed) {
        // коллбек завершения записи
        BufferIoHandler handler = bufferIoHandler(std::move(proceed));
        // запуск записи в сокет
        boost::asio::async_write(_socket,
                                 boost::asio::buffer(&buffer[0], buffer.size()),
                                 handler);
    };
    deferIo(callback);
}

void Socket::connect(const std::string& ip, int port) {
    CallbackIoHandler callback = [&ip, port, this](IoHandler proceed) {
        EndPoint endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port);
        _socket.async_connect(endpoint, proceed);
    };
    deferIo(callback);
}

void Socket::connect(const EndPoint& e) {
    CallbackIoHandler callback = [&e, this](IoHandler proceed) {
        _socket.async_connect(e, proceed);
    };
    deferIo(callback);
}

void Socket::close() {
    _socket.close();
}


//////////////////////////////////////////////////////////////////
// Acceptor class
//////////////////////////////////////////////////////////////////
Acceptor::Acceptor(int port) :
    _acceptor(service<NetworkTag>(),
              boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
              port)) {
}

Socket Acceptor::accept() {
    Socket socket;
    deferIo([this, &socket](IoHandler proceed) {
        _acceptor.async_accept(socket.getSocket(), proceed);
    });
    return socket;
}

void Acceptor::goAccept(SocketHandler handler) {
    std::unique_ptr<Socket> holder(new Socket(accept()));
    Socket* socket = holder.get();
    go([socket, handler] {
        std::unique_ptr<Socket> goHolder(socket);
        handler(*goHolder);
    });
    holder.release();
}

Resolver::Resolver() : _resolver(service<NetworkTag>()) {}

EndPoints Resolver::resolve(const std::string& hostname, int port) {
    boost::asio::ip::tcp::resolver::query query(hostname, std::to_string(port));
    EndPoints ends;
    deferIo([this, &query, &ends](IoHandler proceed) {
        _resolver.async_resolve(query, [proceed, &ends](const Error& e, EndPoints es) {
            if (!e)
                ends = es;
            proceed(e);
        });
    });
    return ends;
}

}
}

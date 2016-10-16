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

#include <unordered_map>
#include "network.h"
#include "mt.h"
#include "core.h"
#include "helpers.h"
#include "journey.h"

namespace server {

using namespace std;
using namespace mt;
using namespace synca;
using namespace synca::net;

enum class State{
    NO_NAME = 0,
    WITH_NAME = 1
};

struct ChatUser{
    std::weak_ptr<Socket> socket;
    State state;
    std::string name;
    
    ChatUser(){
        state = State::NO_NAME;
    }
};


std::mutex mutex;
std::unordered_map<Socket*, ChatUser> chatMap;


struct TimeoutSocketTag;
struct TimeoutSocket {
    TimeoutSocket(int ms, const Handler& inCallback):
        timer(service<TimeoutSocketTag>(), boost::posix_time::milliseconds(ms)),
        callback(inCallback)
    {
        Goer goer = journey().goer();
        timer.async_wait([this, goer](const boost::system::error_code& error) mutable {
            if (!error){
                if(callback){
                    callback();
                }
                goer.timedout();
            }
        });
    }
    
    ~TimeoutSocket(){
        timer.cancel_one();
        handleEvents();
    }
    
private:
    boost::asio::deadline_timer timer;
    Handler callback;
};

void serve(int port)
{

    ThreadPool net(4, "net");
    
    service<NetworkTag>().attach(net);
    service<TimeoutSocketTag>().attach(net);
    scheduler<DefaultTag>().attach(net);
    
    Handler handler = [port] {
        enableEvents();
    
        // создаем приемник соединений
        Acceptor acceptor(port);
        
        // ожидаем подключения
        while (true){
            SocketHandler socketHandler = [](const std::weak_ptr<Socket>& socket) {
                Socket* socketPtr = nullptr;
                if (socket.expired() == false) {
                    socketPtr = socket.lock().get();
                }
                
                JLOG("accepted");
                // работаем с сокетом в цикле
                while (true) {
                    if (socket.expired()) {
                        break;
                    }
                    
                    // нет юзера - создастся автоматически
                    mutex.lock();
                    ChatUser& curUser = chatMap[socketPtr];
                    mutex.unlock();
                    
                    // имя юзера
                    if(curUser.state == State::NO_NAME){
                        socket.lock()->write("Enter name:\n");
                        
                        Buffer userName(64, 0);
                        socketPtr->readUntil(userName, Buffer("\n"));
                        
                        size_t index = userName.find("\r\n", 0);
                        if (index != std::string::npos){
                            userName.replace(index, 2, "\0\0");
                        }
                        
                        curUser.name = userName;
                        curUser.state = State::WITH_NAME;
                        curUser.socket = socket;
                    }
                    
                    socket.lock()->write("Enter message: ");
                    
                    // читаем пока не завершится или не закончится буффер
                    Buffer message(64, 0);
                    {
                        string userName = curUser.name;
                    
                        TimeoutSocket t(150000, [userName, socketPtr](){
                            // закрываем сокет по таймауту
                            socketPtr->close();
                        
                            // Удаляем из мапы
                            mutex.lock();
                            chatMap.erase(socketPtr);
                            mutex.unlock();
                            
                            // передаем всем юзерам сообщение
                            mutex.lock();
                            string resultText = "\t<" + userName + "> left chat by timeout\n";
                            for (const std::pair<Socket*, ChatUser>& user: chatMap){
                                if (user.second.socket.expired() == false) {
                                    user.second.socket.lock()->write("");
                                }
                            }
                            mutex.unlock();
                        });
                        socketPtr->readUntil(message, Buffer("\n"));
                    }
                    
                    size_t index = message.find("\r\n", 0);
                    if (index != std::string::npos){
                        message.replace(index, 2, "\0\0");
                    }
                    
                    // выход из чата
                    std::string resultText;
                    if (message.find("exit") != std::string::npos) {
                        resultText = "\t<" + curUser.name + "> left chat\n";
                        socketPtr->close();
                        break;
                    }else{
                        resultText = "\t<" + curUser.name + ">:" + message + "\n";
                    }
                    
                    // передаем всем юзерам сообщение
                    mutex.lock();
                    for (const std::pair<Socket*, ChatUser>& user: chatMap){
                        if (user.second.socket.expired() == false) {
                            user.second.socket.lock()->write(resultText);
                        }
                    }
                    mutex.unlock();
                    
                    // обрываем
                    if (socket.expired()) {
                        break;
                    }
                }
                
                // TODO:
                // Соединение закрылось??
                mutex.lock();
                chatMap.erase(socketPtr);
                mutex.unlock();
            };
            
            // Работа непосредственно с открытым соединением
            acceptor.goAccept(socketHandler);
        }
    };
    go(handler, net);
}

void echo(int port)
{
    ThreadPool net(4, "net");
    
    service<NetworkTag>().attach(net);
    service<TimeoutSocketTag>().attach(net);
    scheduler<DefaultTag>().attach(net);
    
    Handler handler = [port] {
        enableEvents();
        
        // создаем приемник соединений
        Acceptor acceptor(port);
        
        // ожидаем подключения
        while (true){
            SocketHandler socketHandler = [](const std::weak_ptr<Socket>& socket) {
                Socket* socketPtr = nullptr;
                if (socket.expired() == false) {
                    socketPtr = socket.lock().get();
                }
                
                // читаем пока не завершится или не закончится буффер
                Buffer message(64, 0);
                {
                    TimeoutSocket t(5000, [socketPtr](){
                        // закрываем сокет по таймауту
                        socketPtr->close();
                    });
                    socketPtr->readUntil(message, Buffer("\n"));
                }
                
                // Пишем
                {
                    TimeoutSocket t(5000, [socketPtr](){
                        // закрываем сокет по таймауту
                        socketPtr->close();
                    });
                    socketPtr->write(message);
                }
                
                socketPtr->close();
            };
            
            // Работа непосредственно с открытым соединением
            acceptor.goAccept(socketHandler);
        }
    };
    go(handler, net);
}

}

int main(int argc, char* argv[])
{
    try
    {
        server::echo(8800);
    }
    catch (std::exception& e)
    {
        RLOG("Error: " << e.what());
        return 1;
    }
    catch (...)
    {
        RLOG("Unknown error");
        return 2;
    }
    RLOG("main ended");
    return 0;
}

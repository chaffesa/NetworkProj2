//server.cpp

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#pragma comment (lib, "ws2_32.lib")

struct Message{
    int id;
    std::string sender;
    std::string subject;
    std::string body;
    std::string timestamp;
};

struct ClientInfo {
    SOCKET sockfd;
    std::string username;
};

std::mutex g_mutex;
//sockfd to ClientInfo
std::map<SOCKET, ClientInfo> g_clients;
//All public messages
std::vector<Message> g_messages;
int g_nextMessageId = 1;

//Send a line
bool sendLine(SOCKET sockfd, const std::string &line) {
    std::string data = line + "\n";
    int totalSent = 0;
    int dataSize = static_cast<int>(data.size());

    while (totalSent < dataSize) {
        int n = send(sockfd, data.c_str() + totalSent, dataSize - totalSent, 0);
        if (n == SOCKET_ERROR){
            return false;
        }
        totalSent += n;
    }
    return true;
}

void broadcast(const std::string &line) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = g_clients.begin(); it != g_clients.end();) {
        SOCKET sock = it->first;
        if (!sendLine(sock, line)) {
            //Disconnect inactive client
            closesocket(sock);
            it = g_clients.erase(it);
        } else {
            ++it;
        }
    }
}

std::string currentTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    char buf[64];
    std::tm tm_buf;
    localtime_s(&tm_buf, &t);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buf);
}

void sendUsersList(SOCKET sockfd) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream oss;
    oss << "Users ";
    bool first = true;
    for (const auto &pair : g_clients) {
        const ClientInfo &ci = pair.second;
        if (!ci.username.empty()){
            if (!first) oss << ", ";
            oss << ci.username;
            first = false;
        }
    }
    sendLine(sockfd, oss.str());
}

bool getMessageById(int id, Message &out){
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto &m : g_messages){
        if (m.id == id){
            out = m;
            return true;
        }
    }
    return false;
}

void sendLastTwoMessages(SOCKET sockfd){
    std::lock_guard<std::mutex> lock(g_mutex);
    int n = static_cast<int>(g_messages.size());
    int start = (n >= 2) ? n - 2 : 0;
    for (int i = start; i < n; i++){
        const Message &m = g_messages[i];
        std::ostringstream oss;
        oss << "MSGHDR " << m.id << "|" << m.sender << "|" << m.timestamp << "|" << m.subject;
        sendLine(sockfd, oss.str());
    }
}

void handleClient(SOCKET clientSock){
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_clients[clientSock] = ClientInfo{clientSock, ""};
    }

    std::string buffer;
    char buf[1024];

    bool running = true;
    while (running) {
        int n = recv(clientSock, buf, sizeof(buf), 0);
        if (n == 0){
            break;
        }
        if (n == SOCKET_ERROR){
            std::cout << "Receive failed: " << WSAGetLastError() << "\n";
            break;
        }
        buffer.append(buf, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos){
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if(line.empty()) continue;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "LOGIN"){
                std::string username;
                iss >> username;
                if (username.empty()){
                    sendLine(clientSock, "ERROR Missing username");
                    continue;
                }

                bool exists = false;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    for (const auto &pair : g_clients){
                        if (pair.second.username == username){
                            exists = true;
                            break;
                        }
                    }
                }
                if (exists){
                    sendLine(clientSock, "ERROR Username already being used");
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_clients[clientSock].username = username;
                }

                sendLine(clientSock, "SUCCESS Logged in as " + username);
                sendUsersList(clientSock);
                sendLastTwoMessages(clientSock);

                //notify other users
                broadcast("USERJOIN " + username);
            }
            else if (cmd == "POST") {
                std::string rest;
                std::getline(iss, rest);
                if (!rest.empty() && rest[0] == ' '){
                    rest.erase(0, 1);
                }
                size_t sep = rest.find('|');
                if (sep == std::string::npos) {
                    sendLine(clientSock, "ERROR POST format requires subject|body");
                    continue;
                }
                std::string subject = rest.substr(0, sep);
                std::string body = rest.substr(sep + 1);

                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    username = g_clients[clientSock].username;
                }
                if (username.empty()){
                    sendLine(clientSock, "ERROR You must LOGIN first");
                    continue;
                }

                Message m;
                m.id = g_nextMessageId++;
                m.sender = username;
                m.subject = subject;
                m.body = body;
                m.timestamp = currentTimestamp();

                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_messages.push_back(m);
                }

                std::ostringstream oss;
                oss << "MSGHDR " << m.id << "|" << m.sender << "|" << m.timestamp << "|" << m.subject;
                broadcast(oss.str());
                sendLine(clientSock, "SUCCESS Message posted");
            }
            else if (cmd == "USERS") {
                sendUsersList(clientSock);
            }
            else if (cmd == "MSG") {
                int id;
                if (!(iss >> id)) {
                    sendLine(clientSock, "ERROR MSG requires id");
                    continue;
                }
                Message m;
                if (!getMessageById(id, m)){
                    sendLine(clientSock, "ERROR No message with that id");
                    continue;
                }
                std::ostringstream oss;
                oss << "MSGFULL " << m.id << "|" << m.sender << "|" << m.timestamp << "|" << m.subject << "|" << m.body;
                sendLine(clientSock, oss.str());
            }
            else if (cmd == "LEAVE") {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    username = g_clients[clientSock].username;
                    g_clients[clientSock].username.clear();
                }
                if (!username.empty()){
                    broadcast("USERLEAVE " + username);
                }
                sendLine(clientSock, "SUCCESS Left group");
            }
            else if (cmd == "QUIT") {
                sendLine(clientSock, "SUCCESS Goodbye");
                running = false;
                break;
            }
            else{
                sendLine(clientSock, "ERROR Unknown command");
            }
        }
    }

    std::string username;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_clients.find(clientSock);
        if (it != g_clients.end()){
            username = it->second.username;
            g_clients.erase(it);
        }
    }
    if (!username.empty()){
        broadcast("USERLEAVE " + username);
    }

    closesocket(clientSock);
}

int main(int argc, char *argv[]){
    if (argc != 2){
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);

    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0){
        std::cerr << "WSAStartup failed: " << wsaResult << "\n";
        return 1;
    }

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock == INVALID_SOCKET){
        std::cerr << "Socket failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR){
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR){
        std::cerr << "Listen failed: " << WSAGetLastError() << "\n";
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << port << "...\n";

    while (true){
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientSock == INVALID_SOCKET){
            std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
            break;
        }
        
        std::thread t(handleClient, clientSock);
        t.detach();
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}
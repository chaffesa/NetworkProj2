//client.cpp

#define _WINDSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

bool sendLine(SOCKET sockfd, const std::string &line){
    std::string data = line + "\n";
    int totalSent = 0;
    int dataSize = static_cast<int>(data.size());

    while (totalSent < dataSize){
        int n = send(sockfd, data.c_str() + totalSent, dataSize - totalSent, 0);
        if (n == SOCKET_ERROR){
            std::cout << "Send failed: " << WSAGetLastError() << "\n";
            return false;
        }
        totalSent += n;
    }
    return true;
}

void receiverThread(SOCKET sockfd) {
    char buf[1024];
    std::string buffer;

    while (true){
        int n = recv(sockfd, buf, sizeof(buf), 0);
        if (n == 0){
            std::cout << "\n[INFO] Server closed the connection. \n";
            break;
        }
        if (n == SOCKET_ERROR){
            std::cout << "\n[ERROR] Receiver failed: " << WSAGetLastError() << "\n";
            break;
        }

        buffer.append(buf, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos){
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (!line.empty() && line.back() == '\r'){
                line.pop_back();
            }
            if (line.empty()){
                continue;
            }

            std::cout << "\n[SERVER] " << line << "\n";
            std::cout.flush();
        }
    }
}

int main(int argc, char *argv[]){
    if (argc != 3){
        std::cerr << "Usage: " << argv[0] << " <server-ip> <port>\n";
        return 1;
    }

    std::string serverIp = argv[1];
    int port = std::stoi(argv[2]);

    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0){
        std::cerr << "WSAStartup failed: " << wsaResult << "\n";
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET){
        std::cerr << "Socket failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serv.sin_addr) <= 0){
        std::cerr << "Invalid address\n";
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv) , sizeof(serv)) == SOCKET_ERROR){
        std::cerr << "Connect failed: " << WSAGetLastError() << "\n";
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server: " << serverIp << ", port: " << port << "\n";
    std::cout << "Commands:\n";
    std::cout << " LOGIN <username>\n";
    std::cout << " POST <subject>|<body>\n";
    std::cout << " USERS\n";
    std::cout << " MSD <id>\n";
    std::cout << " LEAVE\n";
    std::cout << " QUIT\n";

    //Start receiver thread
    std::thread recvThread(receiverThread, sockfd);

    //Read user input and send to server
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)){
            break;
        }
        if (line.empty()){
            continue;
        }
        if (!sendLine(sockfd, line)){
            break;
        }

        std::string cmd = line.substr(0, line.find(' '));
        if (cmd == "QUIT"){
            break;
        }
    }

    closesocket(sockfd);
    recvThread.join();
    WSACleanup();
    return 0;
}
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#define PORT 8080
#define BUFFER_SIZE 1024

// Function to continuously receive messages
void receiveMessages(SOCKET clientSocket){
    //buffer to receive message from the other clients present
    char buffer[BUFFER_SIZE];
    while(true){
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if(bytesReceived > 0){
            std::cout << buffer << std::endl;
        } 
        else if(bytesReceived == 0){
            std::cout << "Server closed the connection." << std::endl;
            break;
        } 
        else{
            std::cerr << "Error receiving data!" << std::endl;
            break;
        }
    }
}

int main(){
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;

    // Initialize Winsock
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Connecting to the server via proving the port at which it will listen
    //along with the IP
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    //connecting to the server
    connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    std::string username;
    std::cout << "Enter your username: ";
    std::cin >> username;
    //sending the username of the client to the server over the socket
    send(clientSocket, username.c_str(), username.size(), 0);

    // Start a thread to receive messages
    std::thread recvThread(receiveMessages, clientSocket);
    //detaching the thread so it runs independtly, if the current client connecting breaks,
    //making other conversation running concurrently
    recvThread.detach();

    // Send messages in a loop
    std::string message;
    std::cin.ignore();  // Ignore leftover newline character from username input
    while(true){
        std::getline(std::cin, message);
        if(message == "/quit"){
            std::cout << "Exiting chat..." << std::endl;
            break;
        }
        send(clientSocket, message.c_str(), message.size(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

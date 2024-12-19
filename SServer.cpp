#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <map>
#include <fstream>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#define PORT 8080
#define BUFFER_SIZE 1024

std::map<SOCKET, std::string> clients;  // Map to store connected clients and their usernames
std::mutex clientMutex;                 // Mutex for thread-safe access to `clients` (so that at a time ony one thread can access the client map)
std::ofstream chatLog("chat_history.txt", std::ios::app);  // File created to store chat history

void handleClient(SOCKET clientSocket){
    //buffer to store the incoming msg from other clients
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Receive the username
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

    //if data is received then the bytes received is >0, 
    //if 0 is received then the client disconnected,
    //id < 0 then error occured
    if (bytesReceived > 0){
        std::string username(buffer, bytesReceived);
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            //updating the map, to map the key as SOCKET with the username of the client
            clients[clientSocket] = username;
        }
        //displaying on the Server, for every user connecting on it.
        std::cout << username << " connected." << std::endl;
    }

    while(true){
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if(bytesReceived > 0){
            std::string message(buffer, bytesReceived);

            // Parse message for DM or broadcast, here @ is used to indicate 
            //a personal message to the Reciepient.
            if (message[0] == '@'){
                size_t pos = message.find(": ");
                if(pos != std::string::npos){
                    std::string recipient = message.substr(1, pos - 1);
                    std::string dmMessage = message.substr(pos + 2);

                    // Find the recipient
                    SOCKET recipientSocket = INVALID_SOCKET;
                    {
                        //mutex ensures that only one thread at a time
                        //can access and modify the clients map (shared resource between many clients)
                        //so that at a time, only one client will try to talk to someone, being a client
                        //or a server or all
                        std::lock_guard<std::mutex> lock(clientMutex);
                        for(auto& [sock, name] : clients){
                            if(name == recipient){
                                recipientSocket = sock;
                                break;
                            }
                        }
                    }

                    if(recipientSocket != INVALID_SOCKET){
                        std::string formattedMessage = clients[clientSocket] + " -> " + recipient + ": " + dmMessage;
                        std::string frmt_msg = clients[clientSocket] + ": " + dmMessage;
                        send(recipientSocket, frmt_msg.c_str(), frmt_msg.size(), 0);

                        // Log to file, updating the msgg
                        chatLog << formattedMessage << std::endl;
                    }
                    else{
                        std::string errorMsg = "User " + recipient + " not found!";
                        send(clientSocket, errorMsg.c_str(), errorMsg.size(), 0);
                    }
                }
            }
            else{
                // Broadcast to all clients, the message is for all
                std::lock_guard<std::mutex> lock(clientMutex);
                for(auto& [sock, name] : clients){
                    if(sock != clientSocket){
                        //sending the message to all the sockets in the Clients map
                        send(sock, message.c_str(), message.size(), 0);
                    }
                }

                // Log to file
                chatLog << clients[clientSocket] + ": " + message << std::endl;
            }
        }
        else if(bytesReceived == 0){
            // Client disconnected
            std::cout << clients[clientSocket] << " disconnected." << std::endl;
            {
                //mutex needed for accessing the shared resource (clients map)
                //removing the entry of a particular client from the map, 
                //mutex helpful in safe removal
                std::lock_guard<std::mutex> lock(clientMutex);
                clients.erase(clientSocket);
            }
            closesocket(clientSocket);
            break;
        } 
        else{
            // cout<<userna...
            std::cerr << "Error receiving data!" << std::endl;
            std::cout << clients[clientSocket] << " disconnected." << std::endl;
            closesocket(clientSocket);
            break;
        }
    }
}

//fucntion for broadcating the message from Server to all...
void broadcastServerMessage(){
    std::string serverMessage;
    while(true){
        //Input the message from Server..
        std::getline(std::cin, serverMessage);
        std::string formattedMessage = "Server -> all: " + serverMessage;
        if(!serverMessage.empty()){
            //forwading this message to all the active clients conncted via serever
            std::lock_guard<std::mutex> lock(clientMutex);
            for(auto& [sock, name] : clients){
                //sending the message to all the socket entry in the clients map
                send(sock, formattedMessage.c_str(), formattedMessage.size(), 0);
            }
            chatLog << "Server: " + serverMessage << std::endl;
        }
    }
}

int main(){
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int addrLen = sizeof(clientAddr);

    // Initializing the Winsock (used for windows socket programming)
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Creating socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Binding socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    // Listen for connections
    listen(serverSocket, 5);
    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // Start a thread to handle server-side broadcasting
    //This thread is responsible for the Server to talk to all the clients 
    //it is listening to at the moment.
    std::thread broadcastThread(broadcastServerMessage);
    //detaching making the process of others convering to each other via server
    //smooth even if one client exitss, as all the threads are working independtly..
    broadcastThread.detach();

    while(true){
        // Accept new client connection
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrLen);

        // Handle client in a new thread
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}

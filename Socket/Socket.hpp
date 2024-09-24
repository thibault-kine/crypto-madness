#pragma once
#include "../Packet/Packet.hpp"
#include "../Utils/Utils.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class Socket {
private:
  int socketFd;
  struct sockaddr_in address;
  bool isServer = false;
  bool running = true;
  std::string message;

  Packet acceptClient(char *dataBuffer, uint64_t dataSize,
                      std::string userName);
  Packet registerUser(char *dataBuffer, uint64_t dataSize, std::string userName);
  Packet password(char *dataBuffer, uint64_t dataSize, std::string userName);
  std::string getPassword(int mode);

public:
  // Constructeur par défaut
  Socket() : socketFd(0), running(true) { memset(&address, 0, sizeof(address)); }
  
  ~Socket() { closeSocket(); }

  bool createSocket();
  bool bindSocket(const char *ip, int port);
  bool listenSocket();
  int acceptSocket();
  bool closeSocket();
  bool connectSocket(const char *ip, int port);

  bool sendPacket(int clientFd, Packet message);
  Packet receivePacket(int fd);
  Packet managePacket(char *dataBuffer, uint64_t dataSize, std::string userName, PacketType type);
  Packet connectSocket(char *dataBuffer, uint64_t dataSize, std::string userName);
  Packet maskData(char *dataBuffer, uint64_t dataSize, std::string userName);
  void createFileFromPacket(char *data, ssize_t dataSize, std::string userName);

  int getSocketFd() { return socketFd; }
  std::string getMessage() { return message; }
  struct sockaddr_in getAddress() { return address; }
  bool IsServer() { return isServer; }

  void handleUserInput(Packet &p, const std::string &userName);

  void setSocketFd(int socketFd) { this->socketFd = socketFd; }
  void setAddress(struct sockaddr_in address) { this->address = address; }
  void setIsServer(bool value) { this->isServer = value; }
};

#include "../Socket/Socket.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <vector>
#include <algorithm>
#include <random>
#include <fstream>
#include <cstdio> // Pour std::remove et std::rename

std::string getServerAddress() {
  struct ifaddrs *ifAddrStruct = NULL;
  struct ifaddrs *ifa = NULL;
  void *tmpAddrPtr = NULL;
  std::string address;

  getifaddrs(&ifAddrStruct);

  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) {
      continue;
    }
    if (ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, "lo") != 0) {
      tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
      address = addressBuffer;

      if (address.find("172.") != 0) {
        break;
      }
    }
  }

  if (ifAddrStruct != NULL) {
    freeifaddrs(ifAddrStruct);
  }

  return address;
}

void broadcastMessage(Packet &packet, int senderFd, Socket &server, std::vector<int> &clients, std::vector<std::string> &usernames, std::mutex &clientsMutex) {
  std::lock_guard<std::mutex> lock(clientsMutex);
  for (size_t i = 0; i < clients.size(); ++i) {
    if (clients[i] != senderFd) {
      std::string encryptedData = XorCrypt(packet.getDataStr(), usernames[i], true);
      Packet encryptedPacket(PacketType::MESSAGE , encryptedData.c_str() , packet.getUserNameStr().c_str());
      server.sendPacket(clients[i], encryptedPacket);
    }
  }
}

char *generateMaskFile(std::string username) {
  std::string filename = "Server/masks/mask_" + username + ".bin";
  std::remove(filename.c_str());
  char *mask = new char[1000000];

  std::ofstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Error: could not create mask file" << std::endl;
    exit(1);
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0x20, 0x7E);

  for (int i = 0; i < 1000000; i++) {
    uint8_t byte = dis(gen);
    file.write(reinterpret_cast<char*>(&byte), sizeof(byte));
    mask[i] = byte;
  }

  file.close();
  return mask;
}

int handleClient(int clientFd, Socket &server, std::vector<int> &clients, std::vector<std::string> &usernames, std::mutex &clientsMutex) {
  if (clientFd == -1) {
    return 1;
  }
  std::cout << "Client connected" << std::endl;

  Packet sent = server.receivePacket(clientFd);
  server.sendPacket(clientFd, sent);
  sent = server.receivePacket(clientFd);
  server.sendPacket(clientFd, sent);

  if (sent.getDataStr() == std::string("Connexion refusé")) {
    return 1;
  }

  std::string username = sent.getUserNameStr();
  {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.push_back(clientFd);
    usernames.push_back(username);
  }

  Packet mask = Packet(MASK, generateMaskFile(username), username.c_str());
  sent = server.receivePacket(clientFd);
  server.sendPacket(clientFd, mask);

  while (true) {
    sent = server.receivePacket(clientFd);
    if (sent.getPacketType() == PacketType::NONE) {
      break;
    } else if (sent.getPacketType() == PacketType::ERROR) {
      std::cout << "Error: " << perror << std::endl;
      break;
    }
    broadcastMessage(sent, clientFd, server, clients, usernames, clientsMutex);
  }

  {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = std::find(clients.begin(), clients.end(), clientFd);
    if (it != clients.end()) {
      size_t index = std::distance(clients.begin(), it);
      clients.erase(it);
      usernames.erase(usernames.begin() + index);
    }
  }

  return 0;
}

int startServer(std::string ip, int port) {
  Socket server("server");
  server.setIsServer(true);
  if (server.createSocket() == false) {
    return 1;
  }
  if (server.bindSocket(ip.c_str(), port) == false) {
    return 1;
  }
  if (server.listenSocket() == false) {
    return 1;
  }
  std::cout << "Server started on " << ip << ":" << port << std::endl;

  std::vector<int> clients;
  std::vector<std::string> usernames;
  std::mutex clientsMutex;
  std::vector<std::thread> clientThreads;

  while (true) {
    int clientFd = server.acceptSocket();
    if (clientFd == -1) {
      return 1;
    }
    clientThreads.emplace_back(std::thread(handleClient, clientFd, std::ref(server), std::ref(clients), std::ref(usernames), std::ref(clientsMutex)));
  }

  for (std::thread &t : clientThreads) {
    if (t.joinable()) {
      t.join();
    }
  }

  return 0;
}

int main(int ac, char **av) {
  if (ac != 2) {
    std::cerr << "Usage: " << av[0] << "<port>" << std::endl;
    return 1;
  }
  int port = std::stoi(av[1]);
  std::string ip = getServerAddress();
  if (ip.empty()) {
    std::cerr << "Error: getIp" << std::endl;
    return 1;
  } else {
    std::cout << "Server IP: " << ip << std::endl;
  }

  return startServer(ip, port);
}
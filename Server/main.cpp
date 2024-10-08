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



std::string getServerAddress() {
  struct ifaddrs *ifAddrStruct = NULL;
  struct ifaddrs *ifa = NULL;
  void *tmpAddrPtr = NULL;
  std::string address;

  // Get the list of network interfaces
  getifaddrs(&ifAddrStruct);

  // Iterate through the network interfaces
  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) {
      continue;
    }
    // Check if it is an IPv4 address and not a loopback address
    if (ifa->ifa_addr->sa_family == AF_INET &&
        strcmp(ifa->ifa_name, "lo") != 0) {
      tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
      address = addressBuffer;

      // Avoid WSL internal network addresses (e.g., those starting with "172.")
      if (address.find("172.") != 0) {
        break;
      }
    }
  }

  // Free the memory allocated for network interfaces
  if (ifAddrStruct != NULL) {
    freeifaddrs(ifAddrStruct);
  }

  // Return the found address or an empty string if none found
  return address;
}

void broadcastMessage(const Packet &packet, int senderFd, Socket &server, std::vector<int> &clients, std::mutex &clientsMutex) {
  std::lock_guard<std::mutex> lock(clientsMutex);
  for (int clientFd : clients) {
    if (clientFd != senderFd) {
      server.sendPacket(clientFd, packet);
    }
  }
}

char *generateMaskFile(std::string username) {
  std::string filename = "Server/masks/mask_" + username + ".bin";
  std::remove(filename.c_str());
  char *mask = new char[1000000];

  // Ouvrir le fichier en mode binaire
  std::ofstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Error: could not create mask file" << std::endl;
    exit(1);
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0x20, 0x7E);  // Distribution entre 0x00 et 0xFF (valeurs hexadécimales)

  for (int i = 0; i < 1000000; i++) {
    uint8_t byte = dis(gen);  // Générer un octet aléatoire (8 bits)
    file.write(reinterpret_cast<char*>(&byte), sizeof(byte));  // Écrire l'octet dans le fichier
    mask[i] = byte;
  }

  file.close();
  return mask;
}

int handleClient(int clientFd, Socket &server, std::vector<int> &clients, std::mutex &clientsMutex) {
  if (clientFd == -1) {
    return 1;
  }
  std::cout << "Client connected" << std::endl;

  {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.push_back(clientFd);
  }
  
  // Connexion
  Packet sent = server.receivePacket(clientFd);
  server.sendPacket(clientFd, sent);
  // Requete mdp
  sent = server.receivePacket(clientFd);
  server.sendPacket(clientFd, sent);

  if (sent.getDataStr() == std::string("Connexion refusé")) {
    return 1;
  }
  Packet mask= Packet(MASK, generateMaskFile(sent.getUserNameStr()), sent.getUserNameStr().c_str());
  // envoie mask
  sent = server.receivePacket(clientFd);
  server.sendPacket(clientFd, mask);
  
  while (true) {
    sent = server.receivePacket(clientFd);    
    if (sent.getPacketType() == PacketType::NONE
    ) {
      break;
    }else if(sent.getPacketType() == PacketType::ERROR){
      std::cout << "Error: " << perror << std::endl;
      break;
    }
    broadcastMessage(sent, clientFd, server, clients, clientsMutex);
  }

  {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.erase(std::remove(clients.begin(), clients.end(), clientFd), clients.end());
  }

  return 0;
}

int startServer(std::string ip, int port) {
  Socket server;
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
  std::mutex clientsMutex;
  std::vector<std::thread> clientThreads;

  while (true) {
    int clientFd = server.acceptSocket();
    if (clientFd == -1) {
      return 1;
    }
    clientThreads.emplace_back(std::thread(handleClient, clientFd, std::ref(server), std::ref(clients), std::ref(clientsMutex)));
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
  if (empty(ip)) {
    std::cerr << "Error: getIp" << std::endl;
    return 1;
  } else {
    std::cout << "Server IP: " << ip << std::endl;
  }

  return startServer(ip, port);
}
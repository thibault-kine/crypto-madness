#include "../Packet/Packet.hpp"
#include "../Socket/Socket.hpp"
#include "../Utils/Utils.hpp"
#include <cstring>
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <thread>

// Utilisation de std::vector<std::string> pour éviter les fuites de mémoire
std::vector<std::string> splitIdentification(char *ipPortString) {
  std::string arg = ipPortString;
  size_t posUser = arg.find("@");
  size_t pos = arg.find(":");
  if (pos == std::string::npos || posUser == std::string::npos) {
    std::cerr << "Bad formatting for user@<ip:port>" << std::endl;
    return {};
  }
  std::string user = arg.substr(0, posUser);
  std::string ip = arg.substr(posUser + 1, pos - posUser - 1);
  std::string port = arg.substr(pos + 1);

  // Validation de l'IP et du port avec regex
  std::string ip_regex = "^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$";
  std::string port_regex = "^[0-9]{1,5}$";
  if (!std::regex_match(ip, std::regex(ip_regex))) {
    std::cerr << "Invalid IP address" << std::endl;
    return {};
  }
  if (!std::regex_match(port, std::regex(port_regex))) {
    std::cerr << "Invalid port" << std::endl;
    return {};
  }
  
  return {ip, port, user}; // On retourne un std::vector avec les trois éléments
}

int startClient(const std::string &ip, int port, Packet &p) {
  Socket client;
  if (!client.createSocket()) {
    std::cerr << "Error creating socket" << std::endl;
    return 1;
  }
  if (!client.connectSocket(ip.c_str(), port)) {
    std::cerr << "Error connecting to server" << std::endl;
    return 1;
  }

  // Convertir username en string
  std::vector<uint8_t> usernameVector = p.getUserName();
  std::string username(usernameVector.begin(), usernameVector.end());

  std::cout << username << " started on " << ip << ":" << port << std::endl;

  // Envoyer la demande de connexion
  if (!client.sendPacket(client.getSocketFd(), Packet(PacketType::CONNECT, "", username.c_str()))) {
    std::cerr << "Error sending connection request" << std::endl;
    return 1;
  }

  // Envoyer le mot de passe
  Packet password = client.receivePacket(client.getSocketFd());
  if (!client.sendPacket(client.getSocketFd(), password)) {
    std::cerr << "Error sending password" << std::endl;
    return 1;
  }

  // Vérifier la réponse de connexion
  Packet response = client.receivePacket(client.getSocketFd());
  if (response.getDataStr() == "Connexion refusé") {
    std::cerr << "Connection refused" << std::endl;
    return 1;
  }

  // Répondre avec le paquet reçu
  if (!client.sendPacket(client.getSocketFd(), response)) {
    std::cerr << "Error sending response" << std::endl;
    return 1;
  }

  // Créer un thread pour gérer l'entrée utilisateur
  std::thread inputThread([&client, &p, username]() {
    client.handleUserInput(p, username);
  });
  inputThread.detach();

  // Boucle pour recevoir et envoyer les paquets
  while (true) {
    Packet sent = client.receivePacket(client.getSocketFd());
    if (sent.getPacketType() == PacketType::NONE) {
      client.closeSocket();
      break;
    }else if(sent.getPacketType() == PacketType::ERROR){
      std::cerr << "Error: " << std::endl;
      break;  
    }
  }
  client.sendPacket(client.getSocketFd(), Packet(PacketType::NONE, "", username.c_str()));
  return 0;
}

int main(int ac, char **av) {
  if (ac < 2) {
    std::cerr << "Usage: " << av[0] << " <user@ip:port>" << std::endl;
    return 1;
  }

  std::vector<std::string> identification = splitIdentification(av[1]);
  if (identification.empty()) {
    return 1;
  }

  Packet p(PacketType::MESSAGE, "", identification[2].c_str());
  return startClient(identification[0], std::stoi(identification[1]), p);
}

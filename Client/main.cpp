#include "../Packet/Packet.hpp"
#include "../Socket/Socket.hpp"
#include "../Utils/Utils.hpp"
#include <cstring>
#include <iostream>
#include <regex>
#include <string>

const std::string *splitIdentification(char *ipPortString) {
  std::string arg = ipPortString;
  size_t posUser = arg.find("@");
  size_t pos = arg.find(":");
  if (pos == std::string::npos || posUser == std::string::npos) {
    std::cerr << "Bad formating for user@<ip:port>" << std::endl;
    return NULL;
  }
  std::string user = arg.substr(0, posUser);
  std::string ip = arg.substr(posUser + 1, pos - posUser - 1);
  std::string port = arg.substr(pos + 1);

  std::string ip_regex = "^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$";
  std::string port_regex = "^[0-9]{1,5}$";
  if (std::regex_match(ip, std::regex(ip_regex)) == false) {
    std::cerr << "Invalid ip address" << std::endl;
    return NULL;
  }
  if (std::regex_match(port, std::regex(port_regex)) == false) {
    std::cerr << "Invalid port" << std::endl;
    return NULL;
  }
  std::string *args = new std::string[3];
  args[0] = ip;
  args[1] = port;
  args[2] = user;
  return args;
}

int startClient(std::string ip, int port, Packet &p) {
  Socket client;
  if (client.createSocket() == false) {
    return 1;
  }
  if (client.connectSocket(ip.c_str(), port) == false) {
    return 1;
  }
  // convertit packet username en string
  std::vector<uint8_t> usernameVector = p.getUserName();
  std::string username(usernameVector.begin(), usernameVector.end());

  std::cout << username << " started on " << ip << ":" << port << std::endl;
  // envoie demande de connexion
  client.sendPacket(client.getSocketFd(),
                    Packet(PacketType::CONNECT, "", username.c_str()));
  // envoie mot de passe
  Packet password = client.receivePacket(client.getSocketFd());
  client.sendPacket(client.getSocketFd(), password);

  Packet sent = client.receivePacket(client.getSocketFd());
  if (sent.getDataStr() == std::string("Connexion refusé")) {
    return 1;
  }
  client.sendPacket(client.getSocketFd(), sent);
  while (true) {
    sent = client.receivePacket(client.getSocketFd());
    client.sendPacket(client.getSocketFd(), sent);
    if (sent.getPacketType() == PacketType::NONE) {
      break;
    }
  }
  return 0;
}

int main(int ac, char **av) {
  const std::string *identification = splitIdentification(av[1]);
  if (identification == NULL) {
    return 1;
  }
  Packet p = Packet(PacketType::MESSAGE, "", identification[2].c_str());
  return startClient(identification[0], std::stoi(identification[1]), p);
}
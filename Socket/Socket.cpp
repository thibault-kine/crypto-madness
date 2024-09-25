#include "Socket.hpp"
#include "../Utils/Logger.hpp"
#include "../Utils/Utils.hpp"
#include "../Utils/SHA.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <termios.h>
#include <argon2.h>
#include <regex>

namespace fs = std::filesystem;

Logger *logger = Logger::getInstance();

void enableRawMode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term); // Obtenir les attributs actuels du terminal
    term.c_lflag &= ~(ICANON | ECHO); // Désactiver l'entrée canonique et l'écho
    tcsetattr(STDIN_FILENO, TCSANOW, &term); // Appliquer les changements
}

void disableRawMode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO); // Réactiver l'entrée canonique et l'écho
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void Socket::handleUserInput(Packet &p, const std::string &userName) {
  char c;
  while (this->running) { // running contrôle l'exécution du thread
      message = "";
      std::cout << "\033[2K\r";
      std::cout << "You: "<< std::flush;
      enableRawMode();
      while (true) {
      c = getchar();  // Lire un caractère
        if (c == '\n') {  // Si 'Enter' est appuyé, on arrête
            break;
        } else if (c == 127) {  // Gérer la touche "backspace" (ASCII 127)
            if (!message.empty()) {
                message.pop_back();  // Retirer le dernier caractère
                std::cout << "\b \b";  // Effacer le caractère du terminal
            }
        } else {
            message += c;  // Ajouter le caractère à la chaîne
            std::cout << c;
        }
      }
      disableRawMode();  // Rétablir le mode normal
      if (message.empty()) {
          continue; // Ignorer les messages vides
      }
      std::cout << "\033[2K\r";
      std::cout<< getCurrentTimeHM() << " - You: "<< message << std::endl << std::flush;
      message = XorCrypt(message, userName, false);
      p.setDataFromStr(message.c_str(), userName.c_str());
      this->sendPacket(this->getSocketFd(), p); // Envoyer le message
  }
}

bool Socket::createSocket() {
  socketFd = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFd == -1) {
    logger->errLog("Failed to create socket");
    return false;
  }
  return true;
}

bool Socket::bindSocket(const char *ip, int port) {
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr(ip);
  address.sin_port = htons(port);

  if (bind(socketFd, (struct sockaddr *)&address, sizeof(address)) == -1) {
    logger->errLog("Failed to bind socket");
    return false;
  }
  return true;
}

bool Socket::listenSocket() {
  if (listen(socketFd, 10) == -1) {
    logger->errLog("Failed to listen to socket");
    return false;
  }
  return true;
}

int Socket::acceptSocket() {
  struct sockaddr_in clientAddress;
  socklen_t clientAddressSize = sizeof(clientAddress);
  int clientFd =
      accept(socketFd, (struct sockaddr *)&clientAddress, &clientAddressSize);
  if (clientFd == -1) {
    logger->errLog("Failed to accept connection");
    return -1;
  }
  return clientFd;
}

bool Socket::closeSocket() {
  if (close(socketFd) == -1) {
    logger->errLog("Failed to close socket");
    return false;
  }
  return true;
}

bool Socket::sendPacket(int clientFd, Packet message) {
  std::vector<uint8_t> packet = message.toBytes();
  if(message.getPacketType() == PacketType::MESSAGE)
  {
    std::cout << "\033[2K\r" << std::flush;
    std::cout << getCurrentTimeHM() << " - You: "<< this->message << std::flush;
  }
  int packetSize = packet.size(); // Size includes header size
  ssize_t totalSent = 0;
  while (totalSent < packetSize) {
    ssize_t bytesSent =
        send(clientFd, packet.data() + totalSent, packetSize - totalSent, 0);
    if (bytesSent == -1) {
      logger->errLog("Failed to send packet");
      return false;
    }
    totalSent += bytesSent;
  }

  logger->log(this->isServer, message);

  return true;
}

// Fonction utilitaire pour lire les tailles en Big Endian
uint64_t fromBigEndian(const std::vector<uint8_t> bytes, size_t offset,
                       size_t length) {
  uint64_t value = 0;
  for (size_t i = 0; i < length; ++i) {
    value = (value << 8) | bytes[offset + i];
  }
  return value;
}

Packet Socket::registerUser(char *dataBuffer, uint64_t dataSize,
                            std::string userName) {
  return Packet(PacketType::PASSWORD, this->getPassword(1).c_str(), userName.c_str());
}

Packet Socket::managePacket(char *dataBuffer, uint64_t dataSize,
                            std::string postUserName, PacketType type) {

  switch (type) {

  case PacketType::MESSAGE: {
    std::string message(dataBuffer, dataSize);
    Packet p = Packet(PacketType::MESSAGE, message.c_str(), postUserName.c_str());
    if (message != "Connexion accepté" && message != "Connexion refusé") {
      message = XorCrypt(message, isServer ? postUserName : this->username, this->isServer);
      p.setDataFromStr(message.c_str(), postUserName.c_str());
    }
    std::cout << "\033[2K\r" << std::flush;
    std::cout << getCurrentTimeHM() << " - " << postUserName << ": ";
    p.printData();
    if (!this->IsServer()) {    
      std::cout << "You: "<< this->message << std::flush;
    }
    return p;
    break;
  }

  case PacketType::PASSWORD: {
    return this->password(dataBuffer, dataSize, postUserName);
    break;
  }

  case PacketType::CONNECT: {
    return this->acceptClient(dataBuffer, dataSize, postUserName);
    break;
  }

  case PacketType::MASK: {
    return this->maskData(dataBuffer, dataSize, postUserName);
    break;
  }

  case PacketType::REGISTER: {
    return this->registerUser(dataBuffer, dataSize, postUserName);
    break;
  }

  default:
    return Packet(PacketType::MESSAGE, "", "");
    break;
  }
  
  return Packet(PacketType::MESSAGE, "FAILED", postUserName.c_str());
}

Packet Socket::acceptClient(char *dataBuffer, uint64_t dataSize,
                            std::string userName) {
  if (this->isServer) {
    std::cout << "Received connect packet" << std::endl;
    if (isUserExisting(userName)) {
      return Packet(PacketType::PASSWORD, "Utilisateur déjà existant", userName.c_str());
    } else {
      return Packet(PacketType::REGISTER, "", userName.c_str());
    }
  }else{
    return Packet(PacketType::MESSAGE, "connexion réussie", userName.c_str());
  }
}

/**
 * mode:
 * 0 = LOGIN
 * 1 = REGISTER
 */
std::string Socket::getPassword(int mode) {
  std::string password;
  std::cout << "Enter Password: ";
  std::getline(std::cin, password);
  std::regex password_regex("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[#@$!%*?&])[A-Za-z\\d#@$!%*?&]{8,}$");
  if (!std::regex_match(password, password_regex)) {
    std::cout << "Le mot de passe ne respecte pas les critères de sécurité :\n";
    std::cout << "- 8 caractères minimum\n";
    std::cout << "- Au moins une lettre majuscule\n";
    std::cout << "- Au moins un chiffre\n";
    std::cout << "- Au moins un caractère spécial (ex: #@$!%*?&)\n\n";
    return (getPassword(mode));
  }
  if (mode == 1) {
    std::cout << "Confirm password: ";
    std::string passwordVerif;
    std::getline(std::cin, passwordVerif);
    if (password.compare(passwordVerif) != 0) {
      std::cout << "Passwords don't match" << std::endl;
      return (getPassword(mode));
    }
  }

  // On initialize le salt à la création du compte
  this->salt = generateRandomString(12);

  // Et ensuite on génère la clé
  uint8_t key[16];
  generateKey(username, password, key, sizeof(key));

  std::string saltedPassword = salt.append(password);

  return sha256(saltedPassword);
}

Packet Socket::password(char *dataBuffer, uint64_t dataSize, std::string userName) {
  if (this->isServer) {
    std::cout << "Received password packet" << std::endl;
    if (isPasswordValid(userName, std::string(dataBuffer, dataSize))) {
      return Packet(PacketType::MESSAGE, "Connexion accepté", userName.c_str());
    } else {
      return Packet(PacketType::MESSAGE, "Connexion refusé", userName.c_str());
    }
  } else {
    std::string password = this->getPassword(0);
    return Packet(PacketType::PASSWORD, password.c_str(), userName.c_str());
  }
}

// Fonction pour recevoir et traiter un paquet
Packet Socket::receivePacket(int clientFd) {
  PacketHeader packetHeader;

  // Lire l'en-tête
  ssize_t totalReceived = 0;
  while (totalReceived < sizeof(PacketHeader)) {
    ssize_t bytesReceived =
        recv(clientFd, reinterpret_cast<char *>(&packetHeader) + totalReceived,
             sizeof(PacketHeader) - totalReceived, 0);
    if (bytesReceived <= 0) {
      perror("recv failed while receiving packet header");
      logger->errLog("Connection closed while receiving packet header");
      return Packet(PacketType::ERROR, "", "");
    }
    totalReceived += bytesReceived;
  }
  // Convertir les tailles depuis Big Endian
  std::vector<uint8_t> headerBytes(reinterpret_cast<uint8_t *>(&packetHeader),
                                   reinterpret_cast<uint8_t *>(&packetHeader) +
                                       sizeof(PacketHeader));
  uint8_t type = fromBigEndian(headerBytes, 0, 1);
  uint32_t userNameSize = fromBigEndian(
      headerBytes, 1, 4); // offset 1 pour ignorer le type du paquet
  uint64_t dataSize = fromBigEndian(
      headerBytes, 5, 8); // offset 17 pour ignorer le type du paquet

  // Allocation des buffers

  char *userNameBuffer = (char *)malloc(userNameSize);
  if (userNameBuffer == nullptr) {
    logger->errLog("Failed to allocate memory for username buffer");
    free(userNameBuffer);
    return Packet(PacketType::MESSAGE, "", "");
  }

  char *dataBuffer = (char *)malloc(dataSize);
  if (dataBuffer == nullptr) {
    logger->errLog("Failed to allocate memory for data buffer");
    return Packet(PacketType::MESSAGE, "", "");
  }

  // Lire le nom de l'utilisateur
  totalReceived = 0;
  while (totalReceived < userNameSize) {
    ssize_t bytesReceived = recv(clientFd, userNameBuffer + totalReceived,
                                 userNameSize - totalReceived, 0);
    if (bytesReceived < 0) {
      perror("recv failed while receiving username");
      free(userNameBuffer);
      free(dataBuffer);
      return Packet(PacketType::MESSAGE, "", "");
    }
    if (bytesReceived == 0) {
      logger->errLog("Connection closed while receiving username");
      free(userNameBuffer);
      free(dataBuffer);
      return Packet(PacketType::MESSAGE, "", "");
    }
    totalReceived += bytesReceived;
  }

  // Lire les données
  totalReceived = 0;
  while (totalReceived < dataSize) {
    ssize_t bytesReceived =
        recv(clientFd, dataBuffer + totalReceived, dataSize - totalReceived, 0);
    if (bytesReceived < 0) {
      std::cout << "recv failed while receiving data" << std::endl;
      free(userNameBuffer);
      free(dataBuffer);
      return Packet(PacketType::MESSAGE, "", "");
    }
    if (bytesReceived == 0) {
      logger->errLog("Connection closed while receiving data");
      free(userNameBuffer);
      free(dataBuffer);
      return Packet(PacketType::MESSAGE, "", "");
    }
    totalReceived += bytesReceived;
  }

  std::string userString(userNameBuffer, userNameSize);
  Packet p = managePacket(dataBuffer, dataSize, userString, static_cast<PacketType>(packetHeader.type));
  free(userNameBuffer);
  free(dataBuffer);

  return p; // Retourner un objet Packet avec les données
}

bool Socket::connectSocket(const char *ip, int port) {
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr(ip);
  address.sin_port = htons(port);

  if (connect(socketFd, (struct sockaddr *)&address, sizeof(address)) == -1) {
    std::cerr << "Failed to connect to server" << std::endl;
    return false;
  }
  return true;
}

Packet Socket::maskData(char *dataBuffer, uint64_t dataSize, std::string userName) {
  createFileFromPacket(dataBuffer, dataSize, userName);
  return Packet(PacketType::MESSAGE, "File uploaded successfully", userName.c_str());
}

void Socket::createFileFromPacket(char *data, ssize_t dataSize, std::string userName) {
  std::string storagePath = "";
  if (this->isServer) {
    storagePath.append("Storage/")
        .append(userName)
        .append("/")
        .append("mask_data.bin");
  } else {
    storagePath.append(userName).append("_mask_data.bin");
  }
  std::ofstream newFile;
  newFile.open(storagePath.c_str(), std::ios_base::binary);

  newFile.write(data, dataSize);

  std::cout << "File successfully copied in " << storagePath << std::endl;
  newFile.close();
}

void Socket::generateKey(std::string userName, std::string password, uint8_t* key, size_t keyLength) {

    std::ofstream serverFile;
    if(!fs::exists("Server/user_keys_do_not_steal_plz.txt")) {
        std::ofstream file("Server/user_keys_do_not_steal_plz.txt");
        file.close();
    }
    serverFile.open("Server/user_keys_do_not_steal_plz.txt", std::ios_base::app);
    if (!serverFile.is_open()) {
        std::cout << "Could not open file" << std::endl;
        exit(1);
    }

    std::ofstream clientFile;
    std::string clientFileName = userName + "_key.txt";
    if(!fs::exists(clientFileName.c_str())) {
        std::ofstream file(clientFileName.c_str());
        file.close();
    }
    clientFile.open(clientFileName.c_str(), std::ios_base::app);
    if (!serverFile.is_open()) {
        std::cout << "Could not open file" << std::endl;
        exit(1);
    }

    uint32_t cost = 2; // nb d'itérations
    uint32_t mem = (1 << 16); // cout en mémoire
    uint32_t version = ARGON2_VERSION_13; // numéro de version

    int result = argon2i_hash_raw(
        cost, mem, 1,
        message.data(), message.size(),
        salt.data(), salt.size(),
        key, keyLength
    );
    
    if(result != ARGON2_OK) {
        std::cerr << "Error while generating key : " << argon2_error_message(result) << std::endl;
    }

    std::ostringstream oss;
    for(size_t i = 0; i < keyLength; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(key[i]);
    }

    serverFile << userName << ":" << oss.str() << std::endl;
    serverFile.close();

    clientFile << oss.str() << std::endl;
    clientFile.close();
}
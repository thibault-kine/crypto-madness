#include "Utils.hpp"
#include "../Packet/Packet.hpp"
#include <algorithm>
#include <filesystem>
#include <string.h>
#include <stdio.h>

namespace fs = std::filesystem;


std::vector<std::string> split(std::string &s, const std::string &delimiter) {
  std::vector<std::string> tokens;
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    tokens.push_back(token);
    s.erase(0, pos + delimiter.length());
  }
  tokens.push_back(s);

  return tokens;
}

void addUser(std::string username, std::string password) {
  std::ofstream file;
  if(!fs::exists("Server/actually_safe_this_time.txt")) {
    std::ofstream file("Server/actually_safe_this_time.txt");
    file.close();
  }
  file.open("Server/actually_safe_this_time.txt", std::ios_base::app);
  if (!file.is_open()) {
    std::cout << "Could not open file" << std::endl;
    exit(1);
  }

  file << username << ":" << password << std::endl;
  std::cout << "User " << username << " successfully added!" << std::endl;

  file.close();
}


bool isUserExisting(std::string username) {
  std::ifstream file;
  if(!fs::exists("Server/actually_safe_this_time.txt")) {
    std::ofstream file("Server/actually_safe_this_time.txt");
    file.close();
  }
  file.open("Server/actually_safe_this_time.txt");
  if (!file.is_open()) {
    std::cout << "Could not open file" << std::endl;
    exit(1);
  }
  std::string line;
  std::vector<std::string> loginInfo;
  while (std::getline(file, line)) {
    loginInfo = split(line, ":");

    trim(loginInfo[0]);
    trim(loginInfo[1]);

    // Si on trouve l'username ET le password sur la même ligne
    if (loginInfo[0] == trim(username)) {
      file.close();
      return true;
      loginInfo.clear();
    }

    loginInfo.clear();
  }

  file.close();

  return false;
}

bool isPasswordValid(std::string username, std::string password) {
  std::ifstream file;
  if(!fs::exists("Server/actually_safe_this_time.txt")) {
    std::ofstream file("Server/actually_safe_this_time.txt");
    file.close();
  }
  file.open("Server/actually_safe_this_time.txt");
  if (!file.is_open()) {
    std::cout << "Could not open file" << std::endl;
    exit(1);
  }

  std::string line;
  std::vector<std::string> loginInfo;
  while (std::getline(file, line)) {
    loginInfo = split(line, ":");

    trim(loginInfo[0]);
    trim(loginInfo[1]);

    // Si on trouve l'username ET le password sur la même ligne
    if (loginInfo[0] == trim(username)) {
      if (loginInfo[1] == trim(password)) {
        return true;
      }
      return false;
    }

    loginInfo.clear();
  }

  file.close();
  addUser(username, password);

  return true;
}

// trim from start (in place)
inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

inline std::string trim(std::string &s) {
  ltrim(s);
  rtrim(s);
  return s;
}

std::string getCurrentTimeHMS() {
  auto now = std::chrono::system_clock::now();
  std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTime = *std::localtime(&currentTime);
  std::ostringstream oss;
  oss << std::put_time(&localTime, "%H:%M:%S");

  return oss.str();
}

std::string getCurrentTimeHM() {
  auto now = std::chrono::system_clock::now();
  std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTime = *std::localtime(&currentTime);
  std::ostringstream oss;
  oss << std::put_time(&localTime, "%H:%M");

  return oss.str();
}

std::string XorCrypt(std::string data, std::string user, bool isServer) {
  std::string maskPath = "";
  if (isServer) {
    maskPath = "Server/masks/mask_" + user + ".bin";
  } else {
    maskPath = user + "_mask_data.bin";
  }
  std::string cryptedData = "";
  std::ifstream maskFile(maskPath, std::ios::binary);
  std::ofstream tempFile("temp.txt", std::ios::binary);
  char maskByte;

  if (!maskFile.is_open()) {
    std::cerr << "Could not open mask file: " << maskPath << std::endl;
    exit(1);
  }

  if (!tempFile.is_open()) {
    std::cerr << "Could not open temp file: " << "temp.txt" << std::endl;
    exit(1);
  }

  ssize_t i = 0;
  while (i < data.size() && maskFile.read(&maskByte, sizeof(maskByte))) {
    cryptedData += data[i] ^ maskByte;
    i++;
  }

    // Copier les octets restants dans le fichier temporaire
  while (maskFile.read(&maskByte, sizeof(maskByte))) {
    tempFile.write(&maskByte, sizeof(maskByte));
  }

  maskFile.close();
  tempFile.close();

  // Remplacer le fichier original par le fichier temporaire
  std::remove(maskPath.c_str());
  std::rename("temp.txt", maskPath.c_str());

  return cryptedData;
}

std::string generateRandomString(int length) {
    char alphanum[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string temp_s;
    temp_s.reserve(length);

    for(int i = 0; i < length; i++) {
        temp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return temp_s;
}
#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <ext/stdio_filebuf.h>
#include <fstream>
#include <iomanip>

Logger *Logger::instance = nullptr;
std::mutex Logger::mutex;

Logger *Logger::getInstance() {
  std::lock_guard<std::mutex> lock(mutex);
  if (instance == nullptr) {
    instance = new Logger();
    instance->file.open(".log", std::ios_base::app);
  }
  return instance;
}

Logger::~Logger() {
  std::lock_guard<std::mutex> lock(mutex);
  if (file.is_open()) {
    file.close();
  }
}

/**
 * Ecrit un message dans le format suivant :
 *
 * <temps au format h:m:s> [SERVER/CLIENT] <description>
 */
void Logger::log(bool isServer, Packet packet) {
  std::lock_guard<std::mutex> lock(mutex);
  std::string message;

  message += getCurrentTime();
  if (isServer) {
    message += " [SERVER] ";
  } else {
    message += " [" + packet.getUserNameStr() + "] ";
  }

  message += "à envoyé un packet de " +
             std::to_string(packet.toBytes().size()) + " octets de type " +
             Packet::packetTypeToString(packet.getPacketType());

  if (file.is_open()) {
    file << message << "\n";
    file.flush();
  }
}

void Logger::errLog(std::string message) {
  std::lock_guard<std::mutex> lock(mutex);
  std::string msg;

  msg += getCurrentTime() + " [ERROR!] " + message;

  if (file.is_open()) {
    file << msg << "\n";
    file.flush();
  }
}
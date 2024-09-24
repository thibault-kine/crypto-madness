#pragma once

#include <stdint.h>
#include <sys/socket.h>
#include <vector>
#include <string>

enum PacketType : uint8_t {
  NONE,
  MESSAGE,
  PASSWORD,
  CONNECT,
  LOGIN,
  REGISTER,
  MASK,
  ERROR
};

class Packet {
private:
  PacketType packetType;
  uint64_t dataSize;
  uint32_t userNameSize;
  std::vector<uint8_t> data;
  std::vector<uint8_t> userName;

public:
  Packet(std::vector<uint8_t> bytes) { fromBytes(bytes); }
  Packet(PacketType packetType, const char *data, const char *user)
      : packetType(packetType) {
    setDataFromStr(data, user);
  }
  ~Packet(){};

  std::vector<uint8_t> toBytes();
  void fromBytes(std::vector<uint8_t> bytes);

  PacketType getPacketType() { return this->packetType; }
  uint32_t getDataSize() { return this->dataSize; }
  std::vector<uint8_t> getData() { return this->data; }
  std::vector<uint8_t> getUserName() { return this->userName; }
  std::string getDataStr();
  std::string getUserNameStr();

  void setPacketType(const PacketType packetType) {
    this->packetType = packetType;
  }
  void setDataSize(const uint32_t dataSize) { this->dataSize = dataSize; }
  void setData(const std::vector<uint8_t> data) { this->data = data; }
  void setDataFromStr(const char *str, const char *user);

  void printData();

  static std::string packetTypeToString(PacketType pt);
};

struct PacketHeader {
  u_int8_t type;
  uint32_t userNameSize;
  uint64_t dataSize;
} __attribute((packed));
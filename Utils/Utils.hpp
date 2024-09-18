#pragma once

#include <fstream>
#include <iostream>
#include <limits>
#include <stdint.h>
#include <string>
#include <vector>

#include "../Packet/Packet.hpp"

// void test(char* filename);
std::vector<uint8_t> readFileToUint8Vector(const char *filename,
                                           PacketType type,
                                           const std::string user);

std::vector<std::string> split(std::string &s, const std::string &delimiter);

void addUser(std::string username);

bool isPasswordValid(std::string username, std::string password);

// trim from start (in place)
inline void ltrim(std::string &s);

// trim from end (in place)
inline void rtrim(std::string &s);

inline std::string trim(std::string &s);

std::string getCurrentTimeHMS();

std::string getCurrentTimeHM();
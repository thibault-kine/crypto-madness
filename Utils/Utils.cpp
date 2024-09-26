#include "Utils.hpp"
#include "../Packet/Packet.hpp"
#include <algorithm>
#include <filesystem>
#include <string.h>
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

// Fonction pour encoder en Base64
std::string base64Encode(const std::string &data)
{
	BIO *bio, *b64;
	BUF_MEM *bufferPtr;

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Pas de nouvelle ligne à la fin
	BIO_write(bio, data.c_str(), data.length());
	BIO_flush(bio);
	BIO_get_mem_ptr(bio, &bufferPtr);

	std::string encodedData(bufferPtr->data, bufferPtr->length);
	BIO_free_all(bio);

	return encodedData;
}

// Fonction pour décoder du Base64
std::string base64Decode(const std::string &encodedData)
{
	BIO *bio, *b64;
	char buffer[encodedData.length()];
	memset(buffer, 0, sizeof(buffer));

	bio = BIO_new_mem_buf(encodedData.c_str(), -1);
	b64 = BIO_new(BIO_f_base64());
	bio = BIO_push(b64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	int decodedLength = BIO_read(bio, buffer, encodedData.length());
	BIO_free_all(bio);

	return std::string(buffer, decodedLength);
}

std::vector<std::string> split(std::string &s, const std::string &delimiter)
{
	std::vector<std::string> tokens;
	size_t pos = 0;
	std::string token;
	while ((pos = s.find(delimiter)) != std::string::npos)
	{
		token = s.substr(0, pos);
		tokens.push_back(token);
		s.erase(0, pos + delimiter.length());
	}
	tokens.push_back(s);

	return tokens;
}

void addUser(std::string username, std::string password)
{
	std::ofstream file;
	if (!fs::exists("Server/actually_safe_this_time.txt"))
	{
		std::ofstream file("Server/actually_safe_this_time.txt");
		file.close();
	}
	file.open("Server/actually_safe_this_time.txt", std::ios_base::app);
	if (!file.is_open())
	{
		std::cout << "Could not open file" << std::endl;
		exit(1);
	}

	file << username << ":" << password << std::endl;
	std::cout << "User " << username << " successfully added!" << std::endl;

	file.close();
}

bool isUserExisting(std::string username)
{
	std::ifstream file;
	if (!fs::exists("Server/actually_safe_this_time.txt"))
	{
		std::ofstream file("Server/actually_safe_this_time.txt");
		file.close();
	}
	file.open("Server/actually_safe_this_time.txt");
	if (!file.is_open())
	{
		std::cout << "Could not open file" << std::endl;
		exit(1);
	}
	std::string line;
	std::vector<std::string> loginInfo;
	while (std::getline(file, line))
	{
		loginInfo = split(line, ":");

		trim(loginInfo[0]);
		trim(loginInfo[1]);

		// Si on trouve l'username ET le password sur la même ligne
		if (loginInfo[0] == trim(username))
		{
			file.close();
			return true;
			loginInfo.clear();
		}

		loginInfo.clear();
	}

	file.close();

	return false;
}

bool isPasswordValid(std::string username, std::string password)
{
	std::ifstream file;
	if (!fs::exists("Server/actually_safe_this_time.txt"))
	{
		std::ofstream file("Server/actually_safe_this_time.txt");
		file.close();
	}
	file.open("Server/actually_safe_this_time.txt");
	if (!file.is_open())
	{
		std::cout << "Could not open file" << std::endl;
		exit(1);
	}

	std::string line;
	std::vector<std::string> loginInfo;
	while (std::getline(file, line))
	{
		loginInfo = split(line, ":");

		trim(loginInfo[0]);
		trim(loginInfo[1]);

		// Si on trouve l'username ET le password sur la même ligne
		if (loginInfo[0] == trim(username))
		{
			if (loginInfo[1] == trim(password))
			{
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
inline void ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
																																	{ return !std::isspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
																						[](unsigned char ch)
																						{ return !std::isspace(ch); })
													.base(),
									s.end());
}

inline std::string trim(std::string &s)
{
	ltrim(s);
	rtrim(s);
	return s;
}

std::string getCurrentTimeHMS()
{
	auto now = std::chrono::system_clock::now();
	std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm localTime = *std::localtime(&currentTime);
	std::ostringstream oss;
	oss << std::put_time(&localTime, "%H:%M:%S");

	return oss.str();
}

std::string getCurrentTimeHM()
{
	auto now = std::chrono::system_clock::now();
	std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm localTime = *std::localtime(&currentTime);
	std::ostringstream oss;
	oss << std::put_time(&localTime, "%H:%M");

	return oss.str();
}

std::string XorCrypt(std::string data, std::string user, bool isServer)
{
	std::string maskPath = "";
	if (isServer)
	{
		maskPath = "Server/masks/mask_" + user + ".bin";
	}
	else
	{
		maskPath = user + "_mask_data.bin";
	}
	std::string cryptedData = "";
	std::ifstream maskFile(maskPath, std::ios::binary);
	std::ofstream tempFile("temp.txt", std::ios::binary);
	char maskByte;

	if (!maskFile.is_open())
	{
		std::cerr << "Could not open mask file: " << maskPath << std::endl;
		exit(1);
	}

	if (!tempFile.is_open())
	{
		std::cerr << "Could not open temp file: " << "temp.txt" << std::endl;
		exit(1);
	}

	ssize_t i = 0;
	std::string encryptedBytes;
	while (i < data.size() && maskFile.read(&maskByte, sizeof(maskByte)))
	{
		char cryptedChar = data[i] ^ maskByte;
		encryptedBytes.push_back(cryptedChar);
		i++;
	}

	// Copier les octets restants dans le fichier temporaire
	while (maskFile.read(&maskByte, sizeof(maskByte)))
	{
		tempFile.write(&maskByte, sizeof(maskByte));
	}

	maskFile.close();
	tempFile.close();

	// Remplacer le fichier original par le fichier temporaire
	std::remove(maskPath.c_str());
	std::rename("temp.txt", maskPath.c_str());

	// Encoder le résultat en Base64 pour obtenir une chaîne imprimable
	cryptedData = base64Encode(encryptedBytes);

	struct stat fileStat;
	if (stat(maskPath.c_str(), &fileStat) == 0)
	{
		if (fileStat.st_size < 100)
		{
			std::cerr << "Mask file has less than 100 bytes remaining. Please disconnect and reconnect." << std::endl;
			exit(1);
		}
	}
	else
	{
		std::cerr << "Could not get file status: " << maskPath << std::endl;
		exit(1);
	}
	return cryptedData;
}

std::string XorDecrypt(std::string base64Data, std::string user, bool isServer)
{
	std::string maskPath = isServer ? "Server/masks/mask_" + user + ".bin" : user + "_mask_data.bin";
	std::string decryptedData = "";
	std::ifstream maskFile(maskPath, std::ios::binary);
	std::ofstream tempFile("temp.txt", std::ios::binary);
	char maskByte;

	if (!maskFile.is_open())
	{
		std::cerr << "Could not open mask file: " << maskPath << std::endl;
		exit(1);
	}

	if (!tempFile.is_open())
	{
		std::cerr << "Could not open temp file: " << "temp.txt" << std::endl;
		exit(1);
	}

	// Décoder la chaîne Base64
	std::string encryptedBytes = base64Decode(base64Data);

	ssize_t i = 0;
	while (i < encryptedBytes.size() && maskFile.read(&maskByte, sizeof(maskByte)))
	{
		char decryptedChar = encryptedBytes[i] ^ maskByte;
		decryptedData.push_back(decryptedChar);
		i++;
	}
	// Copier les octets restants dans le fichier temporaire
	while (maskFile.read(&maskByte, sizeof(maskByte)))
	{
		tempFile.write(&maskByte, sizeof(maskByte));
	}

	maskFile.close();
	tempFile.close();

	// Remplacer le fichier original par le fichier temporaire
	std::remove(maskPath.c_str());
	std::rename("temp.txt", maskPath.c_str());

	struct stat fileStat;
	if (stat(maskPath.c_str(), &fileStat) == 0)
	{
		if (fileStat.st_size < 100)
		{
			std::cerr << "Mask file has less than 100 bytes remaining. Please disconnect and reconnect." << std::endl;
			exit(1);
		}
	}
	else
	{
		std::cerr << "Could not get file status: " << maskPath << std::endl;
		exit(1);
	}

	return decryptedData;
}

std::string generateRandomString(int length)
{
	char alphanum[] =
					"0123456789"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz";
	std::string temp_s;
	temp_s.reserve(length);

	for (int i = 0; i < length; i++)
	{
		temp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return temp_s;
}
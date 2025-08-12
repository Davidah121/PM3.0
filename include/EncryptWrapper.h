#pragma once
#include <iostream>
#include <vector>

class EncryptWrapper
{
public:
	static std::string hash(unsigned char* data, size_t size);
	static std::vector<unsigned char> encrypt(unsigned char* data, size_t size, unsigned char* key, size_t keySize);
	static std::vector<unsigned char> decrypt(unsigned char* data, size_t size, unsigned char* key, size_t keySize);
};
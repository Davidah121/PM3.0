// #include "EncryptWrapper.h"
// #include <StringTools.h>

// //NOTE: SHOULD NOT BE USING ANY OF THESE. PROVIDE YOUR OWN METHODS. THESE IMPLEMENTATIONS DO NOTHING BUT PASS THE DATA THROUGH.
// //TESTING PURPOSES OR INTERNAL USE ONLY

// std::string EncryptWrapper::hash(unsigned char* data, size_t size)
// {
// 	return smpl::StringTools::base64Encode(data, size, false);
// }
// std::vector<unsigned char> EncryptWrapper::encrypt(unsigned char* data, size_t size, unsigned char* key, size_t keySize)
// {
// 	std::vector<unsigned char> output = std::vector<unsigned char>(size);
// 	for(size_t i=0; i<size; i++)
// 		output[i] = data[i];
// 	return output;
// }
// std::vector<unsigned char> EncryptWrapper::decrypt(unsigned char* data, size_t size, unsigned char* key, size_t keySize)
// {
// 	std::vector<unsigned char> output = std::vector<unsigned char>(size);
// 	for(size_t i=0; i<size; i++)
// 		output[i] = data[i];
// 	return output;
// }
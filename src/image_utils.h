#pragma once

#include <string>
#include <vector>

bool encryptPassword(const std::string &masterPassphrase,
                     const std::string &password, std::string &outputFilename,
                     std::string &error,
                     const std::string &requestedFilename = {});
bool decryptPassword(const std::string &masterPassphrase,
                     const std::string &filename, std::string &password,
                     std::string &error);
std::vector<std::string> listEncFiles();

#pragma once

#include <vector>
#include <string>
#include "../Include/lodepng.h"

void encryptPassword(const std::string& password);
std::string decryptPassword(const std::string& filename);
std::vector<std::string> listEncFiles();
#pragma once
#include <string>
#include <vector>

void loadCallList(const std::string& path);
std::string pickCall();

extern std::vector<std::string> gCalls;

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace sm::logger {

void start(const std::string& command);
void stop();
bool active();
const std::filesystem::path& path();
void info(const std::string& message);
void command(const std::vector<std::string>& argv, int exitCode, const std::string& output);

}  // namespace sm::logger

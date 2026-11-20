#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace sm::ssh {

bool ensureAgent(std::string& error);
bool agentRunning();
std::size_t loadedKeys();

bool hasKeyPair(const std::string& login);
bool downloadKeys(const std::string& login, std::string& error, std::string& hint);
bool ensureKeys(const std::string& login, const std::string& email, bool& reused,
                std::string& error, std::string& hint);
bool uploadKeys(const std::string& login, std::string& error, std::string& hint);
bool addKey(const std::string& login, std::string& error, std::string& hint);
bool removeKeys(const std::string& login);
bool testGithub(std::string& detail);

std::filesystem::path privateKeyPath(const std::string& login);
std::filesystem::path publicKeyPath(const std::string& login);

}  // namespace sm::ssh

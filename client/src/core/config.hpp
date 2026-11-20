#pragma once

#include <filesystem>
#include <string>

#include "core/types.hpp"

namespace sm::config {

struct Data {
    std::string login;
    std::string email;

    bool hasGithub() const { return !login.empty(); }
};

Data load();
bool save(const Data& data, std::string& error);
void clear();

fs::path dir();
fs::path loginFile();
fs::path emailFile();

bool validateLogin(const std::string& login, std::string& error);
bool validateEmail(const std::string& email, std::string& error);

}  // namespace sm::config

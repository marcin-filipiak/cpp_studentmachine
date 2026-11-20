#pragma once

#include <string>
#include <vector>

#include "core/types.hpp"

namespace sm::commands {

int install(const Context& context, const std::vector<std::string>& arguments);
int update(const Context& context, const std::vector<std::string>& arguments);
int systemup(const Context& context, const std::vector<std::string>& arguments);
int templates(const Context& context, const std::vector<std::string>& arguments);
int exercise(const Context& context, const std::vector<std::string>& arguments);
int savework(const Context& context, const std::vector<std::string>& arguments);
int systemdown(const Context& context, const std::vector<std::string>& arguments);
int status(const Context& context, const std::vector<std::string>& arguments);
int doctor(const Context& context, const std::vector<std::string>& arguments);

int selfUpdate(bool announce);

struct SaveReport {
    bool ok = false;
    bool nothingToSave = false;
    bool noGithub = false;
    std::string login;
    std::string repository;
    std::string commit;
    std::string error;
    std::string hint;
};

SaveReport saveSession(const std::string& message);

}  // namespace sm::commands

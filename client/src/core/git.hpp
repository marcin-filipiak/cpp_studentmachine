#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "core/types.hpp"

namespace sm::git {

enum class SaveState {
    Saved,
    NothingToSave,
    NotRepository,
    Error,
};

struct SaveResult {
    SaveState state = SaveState::Error;
    std::string detail;
    std::string hint;
    std::string commit;
};

struct Info {
    bool repository = false;
    std::string branch;
    std::string remote;
    std::string lastCommit;
    int changedFiles = 0;
};

bool configure(const std::string& name, const std::string& email, std::string& error);
bool clone(const std::string& login, const fs::path& destination, std::string& error,
           std::string& hint);
Info inspect(const fs::path& repository);
SaveResult save(const fs::path& repository, const std::string& name, const std::string& email,
                const std::string& message);
std::string repositoryUrl(const std::string& login);

}  // namespace sm::git

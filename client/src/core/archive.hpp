#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace sm::archive {

struct Listing {
    bool ok = false;
    std::vector<std::string> entries;
    std::string error;
};

Listing list(const std::filesystem::path& archive);
bool extract(const std::filesystem::path& archive, const std::filesystem::path& destination,
             std::string& error);
std::size_t countCollisions(const std::vector<std::string>& entries,
                            const std::filesystem::path& destination);
std::vector<std::string> topLevelFolders(const std::vector<std::string>& entries);
bool isSafeEntry(const std::string& entry);

}  // namespace sm::archive

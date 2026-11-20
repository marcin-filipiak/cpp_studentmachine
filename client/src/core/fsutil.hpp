#pragma once

#include <sys/types.h>

#include <cstddef>
#include <filesystem>
#include <string>

#include "core/types.hpp"

namespace sm::fsutil {

fs::path home();
fs::path cacheDir();
fs::path ensureDir(const fs::path& directory, std::string& error);
void ensureDir(const fs::path& directory);
bool readFile(const fs::path& file, std::string& content);
bool writeFile(const fs::path& file, const std::string& content, mode_t mode, std::string& error);
bool writeFile(const fs::path& file, const std::string& content, mode_t mode = 0644);
bool copyTree(const fs::path& source, const fs::path& destination, std::string& error,
              std::size_t* copied = nullptr);
bool removeAll(const fs::path& path, std::string& error);
std::size_t countEntries(const fs::path& directory);
std::string displayPath(const fs::path& path);
bool isExecutable(const fs::path& file);

class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const fs::path& path() const { return path_; }
    bool valid() const { return !path_.empty(); }

private:
    fs::path path_;
};

}  // namespace sm::fsutil

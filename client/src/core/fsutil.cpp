#include "core/fsutil.hpp"

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#include "core/strings.hpp"

namespace sm::fsutil {
namespace {

std::string errorText(const std::error_code& code) {
    return code ? code.message() : std::string("unknown filesystem error");
}

bool copyEntry(const fs::path& source, const fs::path& destination, std::string& error,
               std::size_t& copied) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(source, ec);
    if (ec) {
        error = "Cannot inspect " + displayPath(source) + ": " + errorText(ec);
        return false;
    }

    if (fs::is_symlink(status)) {
        std::error_code readError;
        const fs::path target = fs::read_symlink(source, readError);
        if (readError) {
            error = "Cannot read symlink " + displayPath(source) + ": " + errorText(readError);
            return false;
        }
        fs::remove(destination, ec);
        fs::create_symlink(target, destination, ec);
        if (ec) {
            error = "Cannot recreate symlink " + displayPath(destination) + ": " + errorText(ec);
            return false;
        }
        ++copied;
        return true;
    }

    if (fs::is_directory(status)) {
        fs::create_directories(destination, ec);
        if (ec) {
            error = "Cannot create directory " + displayPath(destination) + ": " + errorText(ec);
            return false;
        }
        return true;
    }

    if (fs::is_regular_file(status)) {
        fs::create_directories(destination.parent_path(), ec);
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error = "Cannot copy " + displayPath(source) + " to " + displayPath(destination) + ": " +
                    errorText(ec);
            return false;
        }
        const fs::perms permissions = fs::status(source, ec).permissions();
        if (!ec) {
            fs::permissions(destination, permissions, fs::perm_options::replace, ec);
            ec.clear();
        }
        ++copied;
        return true;
    }

    return true;
}

}  // namespace

fs::path home() {
    const char* value = std::getenv("HOME");
    if (value != nullptr && *value != '\0') {
        return fs::path(value);
    }
    const struct passwd* user = getpwuid(getuid());
    if (user != nullptr && user->pw_dir != nullptr) {
        return fs::path(user->pw_dir);
    }
    return {};
}

fs::path cacheDir() {
    const char* value = std::getenv("XDG_CACHE_HOME");
    if (value != nullptr && *value != '\0') {
        return fs::path(value) / "studentmachine";
    }
    return home() / ".cache" / "studentmachine";
}

fs::path ensureDir(const fs::path& directory, std::string& error) {
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        error = "Cannot create directory " + displayPath(directory) + ": " + errorText(ec);
        return {};
    }
    return directory;
}

void ensureDir(const fs::path& directory) {
    std::string ignored;
    ensureDir(directory, ignored);
}

bool readFile(const fs::path& file, std::string& content) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return false;
    }
    content.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

bool writeFile(const fs::path& file, const std::string& content, mode_t mode, std::string& error) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (ec) {
        error = "Cannot create directory " + displayPath(file.parent_path()) + ": " + errorText(ec);
        return false;
    }

    const fs::path temporary = file.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "Cannot write " + displayPath(temporary);
            return false;
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        stream.flush();
        if (!stream) {
            error = "Cannot write " + displayPath(temporary);
            return false;
        }
    }

    if (::chmod(temporary.c_str(), mode) != 0) {
        error = "Cannot set permissions on " + displayPath(temporary) + ": " + std::strerror(errno);
        fs::remove(temporary, ec);
        return false;
    }

    fs::rename(temporary, file, ec);
    if (ec) {
        error = "Cannot finalize " + displayPath(file) + ": " + errorText(ec);
        fs::remove(temporary, ec);
        return false;
    }
    return true;
}

bool writeFile(const fs::path& file, const std::string& content, mode_t mode) {
    std::string ignored;
    return writeFile(file, content, mode, ignored);
}

bool copyTree(const fs::path& source, const fs::path& destination, std::string& error,
              std::size_t* copied) {
    std::size_t count = 0;
    std::error_code ec;
    const fs::file_status sourceStatus = fs::status(source, ec);
    if (ec || !fs::exists(sourceStatus)) {
        return true;
    }
    if (!fs::is_directory(sourceStatus)) {
        if (!copyEntry(source, destination, error, count)) {
            return false;
        }
        if (copied != nullptr) {
            *copied = count;
        }
        return true;
    }

    fs::create_directories(destination, ec);
    if (ec) {
        error = "Cannot create directory " + displayPath(destination) + ": " + errorText(ec);
        return false;
    }

    fs::recursive_directory_iterator iterator(source, fs::directory_options::skip_permission_denied,
                                              ec);
    if (ec) {
        error = "Cannot read directory " + displayPath(source) + ": " + errorText(ec);
        return false;
    }

    const fs::recursive_directory_iterator end;
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            error = "Cannot read " + displayPath(iterator->path()) + ": " + errorText(ec);
            return false;
        }
        const fs::path relative = fs::relative(iterator->path(), source, ec);
        if (ec) {
            error = "Cannot resolve " + displayPath(iterator->path()) + ": " + errorText(ec);
            return false;
        }
        if (!copyEntry(iterator->path(), destination / relative, error, count)) {
            return false;
        }
    }

    if (copied != nullptr) {
        *copied = count;
    }
    return true;
}

bool removeAll(const fs::path& path, std::string& error) {
    std::error_code ec;
    fs::remove_all(path, ec);
    if (ec) {
        error = "Cannot remove " + displayPath(path) + ": " + errorText(ec);
        return false;
    }
    return true;
}

std::size_t countEntries(const fs::path& directory) {
    std::error_code ec;
    if (!fs::is_directory(directory, ec)) {
        return 0;
    }
    std::size_t count = 0;
    for (fs::directory_iterator iterator(directory, ec), end; iterator != end;
         iterator.increment(ec)) {
        if (ec) {
            break;
        }
        ++count;
    }
    return count;
}

std::string displayPath(const fs::path& path) {
    const fs::path userHome = home();
    const std::string full = path.string();
    const std::string homeText = userHome.string();
    if (!homeText.empty() && full == homeText) {
        return "~";
    }
    if (!homeText.empty() && str::startsWith(full, homeText + "/")) {
        return "~" + full.substr(homeText.size());
    }
    return full;
}

bool isExecutable(const fs::path& file) {
    return ::access(file.c_str(), X_OK) == 0;
}

TempDir::TempDir() {
    std::string pattern = (fs::temp_directory_path() / "studentmachine-XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (::mkdtemp(buffer.data()) != nullptr) {
        path_ = fs::path(buffer.data());
    }
}

TempDir::~TempDir() {
    if (!path_.empty()) {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
}

}  // namespace sm::fsutil

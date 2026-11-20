#include "core/config.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <string>

#include "core/fsutil.hpp"
#include "core/strings.hpp"
#include "core/types.hpp"

namespace sm::config {

fs::path dir() {
    return fsutil::home() / ".studentmachine";
}

fs::path loginFile() {
    return dir() / "githublogin";
}

fs::path emailFile() {
    return dir() / "githubmail";
}

Data load() {
    Data data;
    std::string content;
    if (fsutil::readFile(loginFile(), content)) {
        data.login = str::trim(content);
    }
    if (fsutil::readFile(emailFile(), content)) {
        data.email = str::trim(content);
    }
    if (data.email.empty()) {
        data.email = kDefaultEmail;
    }
    return data;
}

bool save(const Data& data, std::string& error) {
    std::string directoryError;
    if (fsutil::ensureDir(dir(), directoryError).empty()) {
        error = directoryError;
        return false;
    }
    ::chmod(dir().c_str(), 0700);

    if (!fsutil::writeFile(loginFile(), data.login + "\n", 0600, error)) {
        return false;
    }
    if (!fsutil::writeFile(emailFile(), data.email + "\n", 0600, error)) {
        return false;
    }
    return true;
}

void clear() {
    std::error_code ec;
    fs::remove_all(dir(), ec);
}

bool validateLogin(const std::string& login, std::string& error) {
    if (login.empty()) {
        error = "GitHub login cannot be empty.";
        return false;
    }
    if (login.size() > 39) {
        error = "GitHub login can contain at most 39 characters.";
        return false;
    }
    if (login.front() == '-' || login.back() == '-') {
        error = "GitHub login cannot start or end with a hyphen.";
        return false;
    }
    for (char character : login) {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '-') {
            error = "GitHub login may only contain letters, digits and hyphens.";
            return false;
        }
    }
    return true;
}

bool validateEmail(const std::string& email, std::string& error) {
    const std::size_t at = email.find('@');
    if (email.empty() || at == std::string::npos || at == 0 || at + 1 >= email.size() ||
        email.find('@', at + 1) != std::string::npos) {
        error = "Enter a valid e-mail address.";
        return false;
    }
    if (email.find(' ') != std::string::npos) {
        error = "E-mail address cannot contain spaces.";
        return false;
    }
    return true;
}

}  // namespace sm::config

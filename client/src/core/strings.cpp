#include "core/strings.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sm::str {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string& value, const std::string& needle) {
    return value.find(needle) != std::string::npos;
}

std::vector<std::string> splitLines(const std::string& value) {
    std::vector<std::string> result;
    std::istringstream stream(value);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        result.push_back(line);
    }
    return result;
}

std::vector<std::string> splitWords(const std::string& value) {
    std::vector<std::string> result;
    std::istringstream stream(value);
    std::string word;
    while (stream >> word) {
        result.push_back(word);
    }
    return result;
}

std::string join(const std::vector<std::string>& parts, const std::string& separator) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            result += separator;
        }
        result += parts[i];
    }
    return result;
}

std::string ellipsize(const std::string& value, std::size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }
    if (maxLength <= 3) {
        return value.substr(0, maxLength);
    }
    return value.substr(0, maxLength - 3) + "...";
}

std::string shellQuote(const std::string& value) {
    if (value.empty()) {
        return "''";
    }
    bool simple = true;
    for (char c : value) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '.' || c == '_' ||
              c == '-' || c == ':' || c == '=' || c == '@')) {
            simple = false;
            break;
        }
    }
    if (simple) {
        return value;
    }
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

std::string replaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
    return value;
}

}  // namespace sm::str

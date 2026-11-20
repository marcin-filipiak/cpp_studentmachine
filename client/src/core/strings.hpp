#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace sm::str {

std::string trim(std::string value);
std::string lower(std::string value);
bool startsWith(const std::string& value, const std::string& prefix);
bool endsWith(const std::string& value, const std::string& suffix);
bool contains(const std::string& value, const std::string& needle);
std::vector<std::string> splitLines(const std::string& value);
std::vector<std::string> splitWords(const std::string& value);
std::string join(const std::vector<std::string>& parts, const std::string& separator);
std::string ellipsize(const std::string& value, std::size_t maxLength);
std::string shellQuote(const std::string& value);
std::string replaceAll(std::string value, const std::string& from, const std::string& to);

}  // namespace sm::str

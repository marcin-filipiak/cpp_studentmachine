#pragma once

#include <string>

namespace sm::term {

void init(bool noColor);
bool color();
bool unicode();
bool isInputTty();
bool isOutputTty();
int width();

std::string bold(const std::string& text);
std::string dim(const std::string& text);
std::string red(const std::string& text);
std::string green(const std::string& text);
std::string yellow(const std::string& text);
std::string blue(const std::string& text);
std::string magenta(const std::string& text);
std::string cyan(const std::string& text);
std::string gray(const std::string& text);

// Picks a UTF-8 glyph with an ASCII fallback.
std::string glyph(const char* utf8, const char* ascii);

}  // namespace sm::term

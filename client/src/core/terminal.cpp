#include "core/terminal.hpp"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace sm::term {
namespace {

bool gColor = false;
bool gUnicode = false;

std::string paint(const std::string& text, const char* code) {
    if (!gColor || text.empty()) {
        return text;
    }
    return std::string("\033[") + code + "m" + text + "\033[0m";
}

const char* environment(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? value : nullptr;
}

}  // namespace

void init(bool noColor) {
    const char* term = environment("TERM");
    const bool tty = isatty(STDOUT_FILENO) != 0;
    const bool noColorEnv = environment("NO_COLOR") != nullptr;
    const bool dumb = term != nullptr && std::strcmp(term, "dumb") == 0;
    gColor = !noColor && !noColorEnv && tty && !dumb;

    const char* locale = environment("LC_ALL");
    if (locale == nullptr) {
        locale = environment("LC_CTYPE");
    }
    if (locale == nullptr) {
        locale = environment("LANG");
    }
    gUnicode = locale != nullptr &&
               (std::strstr(locale, "UTF-8") != nullptr || std::strstr(locale, "utf8") != nullptr);
}

bool color() {
    return gColor;
}

bool unicode() {
    return gUnicode;
}

bool isInputTty() {
    return isatty(STDIN_FILENO) != 0;
}

bool isOutputTty() {
    return isatty(STDOUT_FILENO) != 0;
}

int width() {
    struct winsize size {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return std::clamp(static_cast<int>(size.ws_col), 40, 120);
    }
    const char* columns = environment("COLUMNS");
    if (columns != nullptr) {
        int value = std::atoi(columns);
        if (value > 0) {
            return std::clamp(value, 40, 120);
        }
    }
    return 80;
}

std::string bold(const std::string& text) {
    return paint(text, "1");
}

std::string dim(const std::string& text) {
    return paint(text, "2");
}

std::string red(const std::string& text) {
    return paint(text, "31");
}

std::string green(const std::string& text) {
    return paint(text, "32");
}

std::string yellow(const std::string& text) {
    return paint(text, "33");
}

std::string blue(const std::string& text) {
    return paint(text, "34");
}

std::string magenta(const std::string& text) {
    return paint(text, "35");
}

std::string cyan(const std::string& text) {
    return paint(text, "36");
}

std::string gray(const std::string& text) {
    return paint(text, "90");
}

std::string glyph(const char* utf8, const char* ascii) {
    return gUnicode ? std::string(utf8) : std::string(ascii);
}

}  // namespace sm::term

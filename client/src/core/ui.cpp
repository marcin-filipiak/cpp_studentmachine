#include "core/ui.hpp"

#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <iostream>

#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "version.hpp"

namespace sm::ui {
namespace {

bool gAssumeYes = false;

struct termios gOriginalTerminal {};
volatile sig_atomic_t gRawTerminal = 0;

void restoreTerminalOnSignal(int signalNumber) {
    if (gRawTerminal != 0) {
        ::tcsetattr(STDIN_FILENO, TCSANOW, &gOriginalTerminal);
        gRawTerminal = 0;
    }
    ::signal(signalNumber, SIG_DFL);
    ::raise(signalNumber);
}

std::string repeat(const std::string& text, int count) {
    std::string result;
    for (int i = 0; i < count; ++i) {
        result += text;
    }
    return result;
}

std::string mark(const char* utf8, const char* ascii) {
    return term::glyph(utf8, ascii);
}

std::string padded(const std::string& text, std::size_t width) {
    if (text.size() >= width) {
        return text;
    }
    return text + std::string(width - text.size(), ' ');
}

}  // namespace

void setAssumeYes(bool value) {
    gAssumeYes = value;
}

bool assumeYes() {
    return gAssumeYes;
}

void clearScreen() {
    if (!term::isOutputTty()) {
        return;
    }
    std::cout << "\033[2J\033[H" << std::flush;
}

void banner() {
    const int width = std::min(term::width() - 4, 68);
    std::cout << "\n";

    if (!term::unicode() || width < 54) {
        std::cout << "  " << term::bold(kAppName) << "  "
                  << term::dim(std::string(kCodename) + " " + kVersion) << "\n";
        std::cout << "  " << term::dim(repeat("-", width)) << "\n";
        std::cout << "  " << term::dim("Workspace and GitHub companion for students") << "\n\n";
        return;
    }

    const int inner = width - 4;
    const std::string top = "╭" + repeat("─", width - 2) + "╮";
    const std::string bottom = "╰" + repeat("─", width - 2) + "╯";

    std::cout << "  " << term::cyan(top) << "\n";
    std::cout << "  " << term::cyan("│") << " " << term::bold(kAppName) << term::dim("  ·  ")
              << term::dim(std::string(kCodename) + " " + kVersion)
              << repeat(" ", std::max(0, inner - static_cast<int>(std::string(kAppName).size()) -
                                                 static_cast<int>(std::string(kCodename).size()) -
                                                 static_cast<int>(std::string(kVersion).size()) -
                                                 6))
              << " " << term::cyan("│") << "\n";
    std::cout << "  " << term::cyan("│") << " "
              << term::dim("Workspace and GitHub companion for students")
              << repeat(" ", std::max(0, inner - 43)) << " " << term::cyan("│") << "\n";
    std::cout << "  " << term::cyan(bottom) << "\n\n";
}

void section(const std::string& title) {
    const std::string dash = term::unicode() ? "─" : "-";
    const int used = static_cast<int>(title.size()) + 4;
    const int fill = std::max(0, term::width() - used - 2);
    std::cout << "\n  " << term::cyan(dash + dash + " " + title + " " + repeat(dash, fill))
              << "\n";
}

void line() {
    const std::string dash = term::unicode() ? "─" : "-";
    std::cout << "  " << term::dim(repeat(dash, std::max(10, term::width() - 4))) << "\n";
}

void blank() {
    std::cout << "\n";
}

void info(const std::string& message) {
    std::cout << "  " << term::cyan(mark("›", ">")) << " " << message << "\n";
}

void success(const std::string& message) {
    std::cout << "  " << term::green(mark("✔", "[OK]")) << " " << term::green(message) << "\n";
}

void warn(const std::string& message) {
    std::cout << "  " << term::yellow(mark("!", "!")) << " " << term::yellow(message) << "\n";
}

void error(const std::string& message, const std::string& hintText) {
    std::cout << "  " << term::red(mark("✖", "[ERR]")) << " " << term::bold(term::red(message))
              << "\n";
    if (!hintText.empty()) {
        std::cout << "    " << term::dim("Hint: " + hintText) << "\n";
    }
}

void hint(const std::string& message) {
    std::cout << "    " << term::dim(message) << "\n";
}

void bullet(const std::string& message) {
    std::cout << "  " << term::cyan(mark("•", "-")) << " " << message << "\n";
}

void kv(const std::string& key, const std::string& value, int keyWidth) {
    const std::string label =
        key.size() < static_cast<std::size_t>(keyWidth) ? padded(key, static_cast<std::size_t>(keyWidth))
                                                        : key;
    std::cout << "  " << term::dim(label) << "  " << value << "\n";
}

void summary(bool ok, const std::string& title, const std::vector<std::string>& lines) {
    const std::string dash = term::unicode() ? "─" : "-";
    const int used = static_cast<int>(title.size()) + 5;
    const int fill = std::max(0, term::width() - used - 2);

    std::cout << "\n  "
              << (ok ? term::green(mark("✔", "[OK]")) : term::red(mark("✖", "[ERR]"))) << " "
              << term::bold(title) << " " << term::dim(repeat(dash, fill)) << "\n";
    for (const std::string& entry : lines) {
        std::cout << "    " << entry << "\n";
    }
    std::cout << "  " << term::dim(repeat(dash, std::max(10, term::width() - 4))) << "\n\n";
}

std::optional<std::string> prompt(const std::string& label, const std::string& defaultValue,
                                  const Validator& validator) {
    for (;;) {
        std::cout << "  " << term::cyan(mark("?", "?")) << " " << label;
        if (!defaultValue.empty()) {
            std::cout << " " << term::dim("[" + defaultValue + "]");
        }
        std::cout << ": " << std::flush;

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << "\n";
            return std::nullopt;
        }
        input = str::trim(input);
        if (input.empty()) {
            input = defaultValue;
        }
        if (input.empty()) {
            error("A value is required.");
            continue;
        }

        std::string reason;
        if (validator && !validator(input, reason)) {
            error(reason.empty() ? "That value is not valid." : reason);
            continue;
        }
        return input;
    }
}

bool confirm(const std::string& question, bool defaultYes) {
    if (gAssumeYes) {
        std::cout << "  " << term::yellow(mark("?", "?")) << " " << question << " "
                  << term::dim("[auto-confirmed]") << "\n";
        return true;
    }

    for (;;) {
        std::cout << "  " << term::yellow(mark("?", "?")) << " " << question << " "
                  << term::dim(defaultYes ? "[Y/n]" : "[y/N]") << " " << std::flush;
        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << "\n";
            return defaultYes;
        }
        input = str::lower(str::trim(input));
        if (input.empty()) {
            return defaultYes;
        }
        if (input == "y" || input == "yes" || input == "t" || input == "tak") {
            return true;
        }
        if (input == "n" || input == "no" || input == "nie") {
            return false;
        }
        warn("Please answer y or n.");
    }
}

std::optional<char> readKey() {
    if (!term::isInputTty()) {
        std::string input;
        if (!std::getline(std::cin, input)) {
            return std::nullopt;
        }
        input = str::trim(input);
        return input.empty() ? std::optional<char>('\n') : std::optional<char>(input.front());
    }

    struct termios original {};
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        std::string input;
        if (!std::getline(std::cin, input)) {
            return std::nullopt;
        }
        input = str::trim(input);
        return input.empty() ? std::optional<char>('\n') : std::optional<char>(input.front());
    }

    struct termios raw = original;
    raw.c_lflag &= static_cast<tcflag_t>(~ICANON);
    raw.c_lflag &= static_cast<tcflag_t>(~ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    gOriginalTerminal = original;

    struct sigaction action {};
    action.sa_handler = restoreTerminalOnSignal;
    sigemptyset(&action.sa_mask);
    struct sigaction oldInterrupt {};
    struct sigaction oldTerminate {};
    struct sigaction oldHangup {};
    struct sigaction oldStop {};
    sigaction(SIGINT, &action, &oldInterrupt);
    sigaction(SIGTERM, &action, &oldTerminate);
    sigaction(SIGHUP, &action, &oldHangup);
    sigaction(SIGTSTP, &action, &oldStop);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        sigaction(SIGINT, &oldInterrupt, nullptr);
        sigaction(SIGTERM, &oldTerminate, nullptr);
        sigaction(SIGHUP, &oldHangup, nullptr);
        sigaction(SIGTSTP, &oldStop, nullptr);
        return std::nullopt;
    }

    gRawTerminal = 1;
    char value = 0;
    const ssize_t count = ::read(STDIN_FILENO, &value, 1);
    gRawTerminal = 0;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &original);

    sigaction(SIGINT, &oldInterrupt, nullptr);
    sigaction(SIGTERM, &oldTerminate, nullptr);
    sigaction(SIGHUP, &oldHangup, nullptr);
    sigaction(SIGTSTP, &oldStop, nullptr);

    if (count != 1) {
        return std::nullopt;
    }
    return value;
}

std::optional<char> menu(const std::string& title, const std::vector<MenuItem>& items) {
    if (!title.empty()) {
        std::cout << "\n  " << term::bold(title) << "\n\n";
    }

    std::size_t titleWidth = 0;
    for (const MenuItem& item : items) {
        titleWidth = std::max(titleWidth, item.title.size());
    }
    titleWidth = std::min<std::size_t>(titleWidth, 38);

    for (const MenuItem& item : items) {
        const std::string key = term::cyan(std::string(1, item.key));
        const std::string label = padded(str::ellipsize(item.title, 38), titleWidth);
        const std::string colored =
            item.danger ? term::red(label) : term::bold(label);
        std::cout << "  " << key << "  " << colored;
        if (!item.description.empty()) {
            std::cout << "   " << term::dim(item.description);
        }
        std::cout << "\n";
    }

    std::cout << "\n  " << term::dim("Select an option: ") << std::flush;
    const std::optional<char> selected = readKey();
    if (!selected) {
        std::cout << "\n";
        return std::nullopt;
    }
    std::cout << *selected << "\n";

    if (*selected == 'q' || *selected == 'Q') {
        return std::nullopt;
    }
    for (const MenuItem& item : items) {
        if (*selected == item.key || *selected == static_cast<char>(std::toupper(item.key))) {
            return item.key;
        }
    }
    warn("Unknown option.");
    return menu({}, items);
}

void pause(const std::string& text) {
    if (!term::isInputTty()) {
        return;
    }
    std::cout << "\n  " << term::dim(text) << " " << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

}  // namespace sm::ui

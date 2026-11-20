#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sm::ui {

struct MenuItem {
    char key = '?';
    std::string title;
    std::string description;
    bool danger = false;
};

using Validator = std::function<bool(const std::string&, std::string&)>;

void setAssumeYes(bool value);
bool assumeYes();

void clearScreen();
void banner();
void section(const std::string& title);
void line();
void blank();

void info(const std::string& message);
void success(const std::string& message);
void warn(const std::string& message);
void error(const std::string& message, const std::string& hint = {});
void hint(const std::string& message);
void bullet(const std::string& message);
void kv(const std::string& key, const std::string& value, int keyWidth = 20);
void summary(bool ok, const std::string& title, const std::vector<std::string>& lines = {});

std::optional<std::string> prompt(const std::string& label, const std::string& defaultValue = {},
                                  const Validator& validator = {});
bool confirm(const std::string& question, bool defaultYes = false);
std::optional<char> menu(const std::string& title, const std::vector<MenuItem>& items);
std::optional<char> readKey();
void pause(const std::string& text = "Press Enter to continue...");

}  // namespace sm::ui

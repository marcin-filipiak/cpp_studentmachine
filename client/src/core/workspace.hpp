#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/types.hpp"

namespace sm::workspace {

fs::path dir();
fs::path osConfigDir();
bool exists();
bool reset(std::string& error);

std::size_t syncSettingsToRepo(std::string& error);
std::size_t restoreSettingsFromRepo(std::string& error);

struct TemplatesResult {
    bool ok = false;
    std::string error;
    std::string hint;
};

TemplatesResult downloadTemplates();

struct ExerciseResult {
    enum class Status {
        Ok,
        NotFound,
        Collisions,
        Cancelled,
        Error,
    };

    Status status = Status::Error;
    std::size_t collisions = 0;
    std::string error;
    std::string hint;
    std::vector<std::string> folders;
};

ExerciseResult downloadExercise(
    const std::string& name,
    const std::function<bool(std::size_t)>& confirmOverwrite = {});

bool validExerciseName(const std::string& name, std::string& error);

}  // namespace sm::workspace

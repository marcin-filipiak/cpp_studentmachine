#include "commands/commands.hpp"

#include <string>

#include "core/fsutil.hpp"
#include "core/strings.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"

namespace sm::commands {

int exercise(const Context& context, const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        ui::error("An exercise name is required.",
                  "Example: studentmachine exercise 01-introduction");
        return ExitUsage;
    }
    if (arguments.size() > 1) {
        ui::error("Only one exercise name can be downloaded at a time.");
        return ExitUsage;
    }

    const std::string name = arguments.front();
    std::string nameError;
    if (!workspace::validExerciseName(name, nameError)) {
        ui::error(nameError);
        return ExitUsage;
    }

    if (!workspace::exists()) {
        ui::error("The workspace " + fsutil::displayPath(workspace::dir()) + " does not exist.",
                  "Run `studentmachine templates` first.");
        return ExitDependency;
    }

    ui::section("Downloading exercise");
    ui::kv("Exercise", name, 10);

    const workspace::ExerciseResult result = workspace::downloadExercise(
        name, [](std::size_t collisions) {
            ui::warn(std::to_string(collisions) +
                     " file(s) from the exercise already exist in the workspace.");
            return ui::confirm("Overwrite these files?", false);
        });

    switch (result.status) {
        case workspace::ExerciseResult::Status::Ok: {
            std::vector<std::string> lines;
            if (result.folders.empty()) {
                lines.push_back("Files: " + fsutil::displayPath(workspace::dir()));
            } else {
                for (const std::string& folder : result.folders) {
                    lines.push_back("Folder: " + fsutil::displayPath(workspace::dir() / folder));
                }
            }
            lines.push_back("Workspace: " + fsutil::displayPath(workspace::dir()));
            ui::summary(true, "Exercise is ready", lines);
            return ExitOk;
        }
        case workspace::ExerciseResult::Status::NotFound:
            ui::error(result.error, result.hint);
            return ExitFailure;
        case workspace::ExerciseResult::Status::Collisions:
            ui::error("Some exercise files already exist in the workspace.",
                      "Run the command again and confirm the overwrite.");
            return ExitAborted;
        case workspace::ExerciseResult::Status::Cancelled:
            ui::info("Nothing was changed.");
            return ExitAborted;
        case workspace::ExerciseResult::Status::Error:
            break;
    }

    ui::error("Could not download the exercise.", result.hint);
    if (!result.error.empty()) {
        ui::hint(result.error);
    }
    (void)context;
    return ExitExternal;
}

}  // namespace sm::commands

#include "commands/commands.hpp"

#include "core/types.hpp"
#include "core/ui.hpp"

namespace sm::commands {

int savework(const Context& context, const std::vector<std::string>& arguments) {
    if (!arguments.empty()) {
        ui::error("The savework command does not take any arguments.");
        return ExitUsage;
    }

    ui::section("Saving your work");

    const SaveReport report = saveSession("StudentMachine: save work");
    if (report.noGithub) {
        ui::error(report.error, report.hint);
        return ExitFailure;
    }
    if (!report.ok) {
        ui::error("Your work could not be saved.", report.hint);
        if (!report.error.empty()) {
            ui::hint(report.error);
        }
        return ExitExternal;
    }

    if (report.nothingToSave) {
        ui::success("No changes to save.");
        ui::summary(true, "Everything is up to date",
                    {"Repository: " + report.repository,
                     "Your workspace has not changed since the last save."});
        (void)context;
        return ExitOk;
    }

    std::vector<std::string> lines = {"Repository: " + report.repository};
    if (!report.commit.empty()) {
        lines.push_back("Commit: " + report.commit);
    }
    lines.push_back("Your work is safely stored on GitHub.");
    ui::summary(true, "Work saved", lines);
    (void)context;
    return ExitOk;
}

}  // namespace sm::commands

#include "commands/commands.hpp"

#include "core/fsutil.hpp"
#include "core/git.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"

namespace sm::commands {

int templates(const Context& context, const std::vector<std::string>& arguments) {
    if (!arguments.empty()) {
        ui::error("The templates command does not take any arguments.");
        return ExitUsage;
    }

    ui::section("Downloading project templates");

    if (workspace::exists()) {
        const git::Info info = git::inspect(workspace::dir());
        if (info.changedFiles > 0) {
            ui::warn("The workspace contains " + std::to_string(info.changedFiles) +
                     " uncommitted change(s).");
        }
        if (!ui::confirm("Templates may overwrite files in " +
                             fsutil::displayPath(workspace::dir()) + ". Continue?",
                         false)) {
            ui::info("Nothing was changed.");
            return ExitAborted;
        }
    }

    const workspace::TemplatesResult result = workspace::downloadTemplates();
    if (!result.ok) {
        ui::error("Could not prepare the workspace.", result.hint);
        if (!result.error.empty()) {
            ui::hint(result.error);
        }
        return ExitExternal;
    }

    ui::summary(true, "Templates are ready",
                {"Workspace: " + fsutil::displayPath(workspace::dir()),
                 "Add an exercise with `studentmachine exercise <name>`."});
    (void)context;
    return ExitOk;
}

}  // namespace sm::commands

#include "commands/commands.hpp"

#include <system_error>

#include "core/config.hpp"
#include "core/fsutil.hpp"
#include "core/git.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/ssh.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"

namespace sm::commands {
namespace {

bool cleanup(std::string& error) {
    progress::Spinner spinner("Cleaning up");

    if (ssh::agentRunning()) {
        proc::run({"ssh-add", "-D"}, {});
    }

    const config::Data data = config::load();
    if (data.hasGithub()) {
        ssh::removeKeys(data.login);
    }

    if (!workspace::reset(error)) {
        spinner.fail();
        return false;
    }
    if (!fsutil::removeAll(fsutil::home() / ".config", error)) {
        spinner.fail();
        return false;
    }

    std::error_code ec;
    fs::remove(fsutil::home() / ".bash_history", ec);
    fs::remove(fsutil::home() / ".zsh_history", ec);

    spinner.succeed();
    return true;
}

}  // namespace

int systemdown(const Context& context, const std::vector<std::string>& arguments) {
    if (!arguments.empty()) {
        ui::error("The systemdown command does not take any arguments.");
        return ExitUsage;
    }

    ui::section("Finishing the session");

    const config::Data data = config::load();
    SaveReport report;
    if (data.hasGithub()) {
        report = saveSession("StudentMachine: finish session");
        if (!report.ok) {
            ui::error("Your work could not be saved. The machine was not powered off.",
                      report.hint);
            if (!report.error.empty()) {
                ui::hint(report.error);
            }
            return ExitExternal;
        }
        if (report.nothingToSave) {
            ui::success("No changes to save. Everything is already on GitHub.");
        } else {
            ui::success("Your work was saved to GitHub.");
        }
    } else {
        ui::warn("GitHub is not configured. Your work will NOT be saved.");
        if (!ui::confirm("Power off without saving?", false)) {
            ui::info("The machine stays on.");
            return ExitAborted;
        }
    }

    if (!ui::confirm("Power off now?", true)) {
        ui::info("The machine stays on.");
        return ExitAborted;
    }

    std::string cleanupError;
    if (!cleanup(cleanupError)) {
        ui::warn("Cleanup did not finish completely: " + cleanupError);
        if (!ui::confirm("Power off anyway?", false)) {
            return ExitFailure;
        }
    }

    std::vector<std::string> lines;
    if (data.hasGithub()) {
        lines.push_back("Repository: " + git::repositoryUrl(data.login));
        if (!report.commit.empty()) {
            lines.push_back("Commit: " + report.commit);
        }
    } else {
        lines.push_back("GitHub: not configured (work was not saved)");
    }
    lines.push_back("Goodbye!");
    ui::summary(true, "Session finished", lines);

    ui::info("Powering off...");
    proc::RunOptions poweroffOptions;
    poweroffOptions.timeoutSeconds = 30;
    const proc::RunResult poweroff = proc::run(proc::elevate({"poweroff"}), poweroffOptions);
    if (!poweroff.ok()) {
        ui::error("Could not power off the machine.",
                  "Run `sudo poweroff` manually to finish.");
        return ExitExternal;
    }
    (void)context;
    return ExitOk;
}

}  // namespace sm::commands

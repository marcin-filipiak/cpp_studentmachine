#include "commands/commands.hpp"

#include <string>

#include "core/config.hpp"
#include "core/fsutil.hpp"
#include "core/git.hpp"
#include "core/ssh.hpp"
#include "core/terminal.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"
#include "version.hpp"

namespace sm::commands {

int status(const Context& context, const std::vector<std::string>& arguments) {
    if (!arguments.empty()) {
        ui::error("The status command does not take any arguments.");
        return ExitUsage;
    }

    ui::section("Configuration");
    const config::Data data = config::load();
    if (!data.hasGithub()) {
        ui::warn("GitHub is not configured.");
        ui::hint("Run `studentmachine systemup` to connect an account.");
    } else {
        ui::kv("GitHub account", data.login);
        ui::kv("Git e-mail", data.email);
        ui::kv("Config file", fsutil::displayPath(config::loginFile()));
    }

    ui::section("Workspace");
    if (!workspace::exists()) {
        ui::warn("The workspace " + fsutil::displayPath(workspace::dir()) + " does not exist.");
        ui::hint("Run `studentmachine templates` or `studentmachine systemup`.");
    } else {
        ui::kv("Location", fsutil::displayPath(workspace::dir()));
        const git::Info info = git::inspect(workspace::dir());
        if (info.repository) {
            ui::kv("Repository", info.remote.empty() ? "not configured" : info.remote);
            ui::kv("Branch", info.branch.empty() ? "unknown" : info.branch);
            ui::kv("Changes", info.changedFiles == 0
                                  ? term::green("clean")
                                  : term::yellow(std::to_string(info.changedFiles) + " file(s)"));
            ui::kv("Last commit", info.lastCommit.empty() ? "none" : info.lastCommit);
        } else {
            ui::warn("The workspace is not a Git repository.");
        }
    }

    ui::section("SSH");
    if (ssh::agentRunning()) {
        ui::kv("Agent", term::green("running"));
        ui::kv("Loaded keys", std::to_string(ssh::loadedKeys()));
    } else {
        ui::kv("Agent", term::yellow("not running"));
    }
    if (data.hasGithub()) {
        ui::kv("Account key", ssh::hasKeyPair(data.login) ? term::green("present")
                                                         : term::red("missing"));
    }

    ui::section("StudentMachine");
    ui::kv("Version", std::string(kVersion) + " (" + kCodename + ")");
    ui::kv("Logs", fsutil::displayPath(fsutil::cacheDir() / "logs"));

    (void)context;
    return ExitOk;
}

}  // namespace sm::commands

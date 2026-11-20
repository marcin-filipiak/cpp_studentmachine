#include "commands/commands.hpp"

#include <sys/statvfs.h>

#include <iostream>
#include <string>

#include "core/config.hpp"
#include "core/fsutil.hpp"
#include "core/git.hpp"
#include "core/net.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/ssh.hpp"
#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"

namespace sm::commands {
namespace {

std::string padded(const std::string& value, std::size_t width) {
    if (value.size() >= width) {
        return value;
    }
    return value + std::string(width - value.size(), ' ');
}

struct Counters {
    int errors = 0;
    int warnings = 0;
};

void line(const std::string& status, const std::string& name, const std::string& detail) {
    std::cout << "  " << status << " " << term::bold(padded(name, 15)) << "  " << detail << "\n";
}

void tool(Counters& counters, const std::string& name, bool required, const std::string& role) {
    const std::string found = proc::pathOf(name);
    if (!found.empty()) {
        line(term::green(term::glyph("✔", "[OK]")), name, found);
        return;
    }
    if (required) {
        line(term::red(term::glyph("✖", "[NO]")), name, term::red("missing (" + role + ")"));
        ++counters.errors;
    } else {
        line(term::yellow("!"), name, term::yellow("missing (" + role + ")"));
        ++counters.warnings;
    }
}

void network(Counters& counters, const std::string& name, const std::string& url) {
    std::string error;
    if (net::endpointReachable(url, error)) {
        line(term::green(term::glyph("✔", "[OK]")), name, url);
        return;
    }
    line(term::red(term::glyph("✖", "[NO]")), name, term::red(error));
    ++counters.errors;
}

}  // namespace

int doctor(const Context& context, const std::vector<std::string>& arguments) {
    bool offline = false;
    for (const std::string& argument : arguments) {
        if (argument == "--offline") {
            offline = true;
        } else {
            ui::error("Unknown option for doctor: " + argument);
            return ExitUsage;
        }
    }

    const config::Data data = config::load();
    const bool github = data.hasGithub();
    Counters counters;

    ui::section("Tools");
    tool(counters, "curl", true, "downloads");
    tool(counters, "tar", true, "archives");
    tool(counters, "git", github, "GitHub");
    tool(counters, "ssh", github, "GitHub");
    tool(counters, "ssh-keygen", github, "GitHub");
    tool(counters, "ssh-add", github, "GitHub");
    tool(counters, "ssh-agent", github, "GitHub");
    tool(counters, "sudo", false, "administration");
    tool(counters, "apt-get", false, "install command");
    tool(counters, "poweroff", false, "shutdown");

    ui::section("Network");
    if (offline) {
        ui::info("Network checks skipped (--offline).");
    } else {
        network(counters, "StudentMachine API",
                std::string(kApiBase) + "/files/student_projects.tar");
        network(counters, "GitHub", "https://github.com");
    }

    ui::section("Configuration");
    if (!github) {
        line(term::yellow("!"), "GitHub account", term::yellow("not configured"));
        ++counters.warnings;
    } else {
        std::string loginError;
        const bool validLogin = config::validateLogin(data.login, loginError);
        line(validLogin ? term::green(term::glyph("✔", "[OK]")) : term::red(term::glyph("✖", "[NO]")),
             "GitHub account", data.login);
        if (!validLogin) {
            ++counters.errors;
        }

        std::string emailError;
        const bool validEmail = config::validateEmail(data.email, emailError);
        line(validEmail ? term::green(term::glyph("✔", "[OK]")) : term::red(term::glyph("✖", "[NO]")),
             "Git e-mail", data.email);
        if (!validEmail) {
            ++counters.errors;
        }

        const bool key = ssh::hasKeyPair(data.login);
        line(key ? term::green(term::glyph("✔", "[OK]")) : term::red(term::glyph("✖", "[NO]")),
             "SSH key", key ? "present" : "missing");
        if (!key) {
            ++counters.errors;
        }

        const bool agent = ssh::agentRunning();
        line(agent ? term::green(term::glyph("✔", "[OK]")) : term::yellow("!"), "SSH agent",
             agent ? "running" : "not running (starts automatically)");
        if (!agent) {
            ++counters.warnings;
        }
    }

    ui::section("Workspace");
    if (!workspace::exists()) {
        line(term::yellow("!"), "student_projects", term::yellow("not created"));
        ++counters.warnings;
    } else {
        line(term::green(term::glyph("✔", "[OK]")), "student_projects",
             fsutil::displayPath(workspace::dir()));
        const git::Info info = git::inspect(workspace::dir());
        if (github && !info.repository) {
            line(term::red(term::glyph("✖", "[NO]")), "Git repository",
                 term::red("not initialized"));
            ++counters.errors;
        } else if (info.repository && info.changedFiles > 0) {
            line(term::yellow("!"), "Uncommitted changes",
                 term::yellow(std::to_string(info.changedFiles) + " file(s)"));
            ++counters.warnings;
        }
    }

    ui::section("Disk space");
    struct statvfs stats {};
    if (statvfs(fsutil::home().c_str(), &stats) != 0) {
        line(term::yellow("!"), "Home directory", term::yellow("cannot read free space"));
        ++counters.warnings;
    } else {
        const std::uint64_t freeBytes =
            static_cast<std::uint64_t>(stats.f_bavail) * stats.f_frsize;
        if (freeBytes < 200ULL * 1024 * 1024) {
            line(term::red(term::glyph("✖", "[NO]")), "Free space",
                 term::red(progress::humanBytes(freeBytes) + " available"));
            ++counters.errors;
        } else if (freeBytes < 1024ULL * 1024 * 1024) {
            line(term::yellow("!"), "Free space",
                 term::yellow(progress::humanBytes(freeBytes) + " available"));
            ++counters.warnings;
        } else {
            line(term::green(term::glyph("✔", "[OK]")), "Free space",
                 progress::humanBytes(freeBytes) + " available");
        }
    }

    if (counters.errors > 0) {
        ui::summary(false, "Diagnostics found problems",
                    {std::to_string(counters.errors) + " error(s), " +
                         std::to_string(counters.warnings) + " warning(s).",
                     "Fix the items marked with a red cross and run doctor again."});
        (void)context;
        return ExitDependency;
    }

    if (counters.warnings > 0) {
        ui::summary(true, "System is ready",
                    {std::to_string(counters.warnings) + " warning(s), no errors."});
    } else {
        ui::summary(true, "All checks passed", {"StudentMachine is ready to use."});
    }
    (void)context;
    return ExitOk;
}

}  // namespace sm::commands

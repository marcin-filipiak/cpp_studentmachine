#include "core/git.hpp"

#include <cstdlib>
#include <system_error>

#include "core/fsutil.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/strings.hpp"

namespace sm::git {
namespace {

std::vector<std::string> gitEnvironment() {
    return {"GIT_TERMINAL_PROMPT=0",
            "GIT_SSH_COMMAND=ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o "
            "ConnectTimeout=15"};
}

proc::RunResult runGit(const fs::path& repository, const std::vector<std::string>& arguments,
                       const proc::RunOptions& options = {}) {
    std::vector<std::string> argv = {"git"};
    argv.insert(argv.end(), arguments.begin(), arguments.end());

    proc::RunOptions actual = options;
    actual.environment = gitEnvironment();
    if (!repository.empty()) {
        actual.workingDirectory = repository.string();
    }
    return proc::run(argv, actual);
}

bool isRepository(const fs::path& repository) {
    std::error_code ec;
    if (!fs::exists(repository / ".git", ec)) {
        return false;
    }
    const proc::RunResult result =
        runGit(repository, {"rev-parse", "--is-inside-work-tree"});
    return result.ok() && str::trim(result.output) == "true";
}

std::string tail(const std::string& text, std::size_t lines) {
    const std::vector<std::string> all = str::splitLines(str::trim(text));
    if (all.empty()) {
        return {};
    }
    const std::size_t start = all.size() > lines ? all.size() - lines : 0;
    std::string result;
    for (std::size_t i = start; i < all.size(); ++i) {
        if (!result.empty()) {
            result += "\n";
        }
        result += all[i];
    }
    return result;
}

void explainGitFailure(const std::string& output, std::string& error, std::string& hint) {
    const std::string lowered = str::lower(output);
    if (str::contains(lowered, "permission denied (publickey)")) {
        error = "GitHub rejected the SSH key.";
        hint = "Add the public key to your GitHub account, then try again.";
        return;
    }
    if (str::contains(lowered, "could not resolve host") ||
        str::contains(lowered, "network is unreachable") ||
        str::contains(lowered, "connection timed out")) {
        error = "Cannot reach GitHub.";
        hint = "Check your internet connection and try again.";
        return;
    }
    if (str::contains(lowered, "repository not found") ||
        str::contains(lowered, "does not appear to be a git repository")) {
        error = "The GitHub repository was not found.";
        hint = "Create a private repository named \"student_projects\" on your GitHub account.";
        return;
    }
    if (str::contains(lowered, "host key verification failed")) {
        error = "The GitHub host key is not trusted.";
        hint = "Run `ssh -T git@github.com` once to accept the host key.";
        return;
    }
    if (str::contains(lowered, "no space left on device")) {
        error = "There is not enough free disk space.";
        hint = "Free some space and try again.";
        return;
    }
    error = tail(output, 6);
    if (error.empty()) {
        error = "The Git operation failed.";
    }
    hint = "Review the full log for details.";
}

bool looksDiverged(const std::string& output) {
    const std::string lowered = str::lower(output);
    return str::contains(lowered, "non-fast-forward") || str::contains(lowered, "fetch first") ||
           str::contains(lowered, "rejected") || str::contains(lowered, "diverged") ||
           str::contains(lowered, "failed to push some refs");
}

std::optional<int> aheadCount(const fs::path& repository) {
    const proc::RunResult upstream =
        runGit(repository, {"rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"});
    if (!upstream.ok()) {
        return std::nullopt;
    }
    const proc::RunResult count = runGit(repository, {"rev-list", "--count", "@{u}..HEAD"});
    if (!count.ok()) {
        return std::nullopt;
    }
    return std::atoi(str::trim(count.output).c_str());
}

proc::RunResult push(const fs::path& repository, bool reportFailure) {
    progress::Spinner spinner("Pushing to GitHub");
    proc::RunOptions options;
    options.timeoutSeconds = 300;
    options.onTick = [&]() { spinner.tick(); };
    proc::RunResult result = runGit(repository, {"push", "-u", "origin", "HEAD"}, options);
    if (result.ok()) {
        spinner.succeed();
    } else if (reportFailure) {
        spinner.fail();
    } else {
        spinner.cancel();
    }
    return result;
}

}  // namespace

std::string repositoryUrl(const std::string& login) {
    return "https://github.com/" + login + "/student_projects";
}

bool configure(const std::string& name, const std::string& email, std::string& error) {
    if (!proc::exists("git")) {
        error = "git is not installed.";
        return false;
    }

    proc::RunOptions options;
    options.environment = gitEnvironment();
    const proc::RunResult nameResult =
        proc::run({"git", "config", "--global", "user.name", name}, options);
    if (!nameResult.ok()) {
        error = "Cannot set the Git user name.";
        const std::string detail = str::trim(nameResult.output);
        if (!detail.empty()) {
            error += " " + detail;
        }
        return false;
    }

    const proc::RunResult emailResult =
        proc::run({"git", "config", "--global", "user.email", email}, options);
    if (!emailResult.ok()) {
        error = "Cannot set the Git user e-mail.";
        const std::string detail = str::trim(emailResult.output);
        if (!detail.empty()) {
            error += " " + detail;
        }
        return false;
    }
    return true;
}

bool clone(const std::string& login, const fs::path& destination, std::string& error,
           std::string& hint) {
    if (!proc::exists("git")) {
        error = "git is not installed.";
        hint = "Run `studentmachine install` to install the required tools.";
        return false;
    }

    std::error_code ec;
    if (fs::exists(destination, ec)) {
        if (fs::is_directory(destination, ec) && fs::is_empty(destination, ec)) {
            fs::remove(destination, ec);
        } else {
            error = "The workspace " + fsutil::displayPath(destination) + " already exists.";
            hint = "Remove it or use the templates command to refresh it.";
            return false;
        }
    }

    std::string directoryError;
    if (fsutil::ensureDir(destination.parent_path(), directoryError).empty()) {
        error = directoryError;
        return false;
    }

    progress::Spinner spinner("Cloning student_projects from GitHub");
    proc::RunOptions options;
    options.timeoutSeconds = 300;
    options.onTick = [&]() { spinner.tick(); };

    const std::string url = "git@github.com:" + login + "/student_projects.git";
    std::vector<std::string> argv = {"git", "clone", "--quiet", url, destination.string()};
    const proc::RunResult result = proc::run(argv, options);

    if (!result.ok()) {
        spinner.fail();
        explainGitFailure(result.output, error, hint);
        return false;
    }

    if (!isRepository(destination)) {
        spinner.fail();
        error = "The clone finished, but the workspace is not a Git repository.";
        hint = "Check the repository on GitHub and try again.";
        return false;
    }

    spinner.succeed();
    return true;
}

Info inspect(const fs::path& repository) {
    Info info;
    if (!isRepository(repository)) {
        return info;
    }

    info.repository = true;
    const proc::RunResult branch = runGit(repository, {"rev-parse", "--abbrev-ref", "HEAD"});
    if (branch.ok()) {
        info.branch = str::trim(branch.output);
    }
    const proc::RunResult remote = runGit(repository, {"config", "--get", "remote.origin.url"});
    if (remote.ok()) {
        info.remote = str::trim(remote.output);
    }
    const proc::RunResult last =
        runGit(repository, {"log", "-1", "--format=%h %s (%cr)"});
    if (last.ok()) {
        info.lastCommit = str::trim(last.output);
    }
    const proc::RunResult status = runGit(repository, {"status", "--porcelain"});
    if (status.ok()) {
        for (const std::string& line : str::splitLines(status.output)) {
            if (!str::trim(line).empty()) {
                ++info.changedFiles;
            }
        }
    }
    return info;
}

SaveResult save(const fs::path& repository, const std::string& name, const std::string& email,
                const std::string& message) {
    SaveResult result;
    if (!isRepository(repository)) {
        result.state = SaveState::NotRepository;
        result.detail = "The workspace " + fsutil::displayPath(repository) +
                        " is not a Git repository.";
        result.hint = "Run `studentmachine systemup` to configure the workspace.";
        return result;
    }

    const proc::RunResult add = runGit(repository, {"add", "-A"});
    if (!add.ok()) {
        explainGitFailure(add.output, result.detail, result.hint);
        return result;
    }

    const proc::RunResult staged = runGit(repository, {"diff", "--cached", "--quiet"});
    const bool hasChanges = staged.exitCode == 1;
    if (staged.exitCode != 0 && staged.exitCode != 1) {
        explainGitFailure(staged.output, result.detail, result.hint);
        return result;
    }

    if (!hasChanges) {
        const std::optional<int> ahead = aheadCount(repository);
        if (!ahead.has_value() || *ahead == 0) {
            result.state = SaveState::NothingToSave;
            return result;
        }
    } else {
        progress::Spinner spinner("Committing changes");
        const proc::RunResult commit =
            runGit(repository, {"-c", "user.name=" + name, "-c", "user.email=" + email, "commit",
                                "-m", message});
        if (!commit.ok()) {
            spinner.fail();
            explainGitFailure(commit.output, result.detail, result.hint);
            return result;
        }
        spinner.succeed();

        const proc::RunResult hash = runGit(repository, {"rev-parse", "--short", "HEAD"});
        if (hash.ok()) {
            result.commit = str::trim(hash.output);
        }
    }

    proc::RunResult pushed = push(repository, false);
    if (!pushed.ok() && looksDiverged(pushed.output)) {
        progress::Spinner spinner("Integrating remote changes");
        proc::RunOptions options;
        options.timeoutSeconds = 300;
        options.onTick = [&]() { spinner.tick(); };

        const proc::RunResult branchResult =
            runGit(repository, {"rev-parse", "--abbrev-ref", "HEAD"});
        const std::string branch = str::trim(branchResult.output);
        if (!branchResult.ok() || branch.empty() || branch == "HEAD") {
            spinner.fail();
            result.detail = "Cannot determine the current Git branch.";
            result.hint = "Check out a branch and try again.";
            return result;
        }

        const proc::RunResult fetched = runGit(repository, {"fetch", "origin"}, options);
        if (!fetched.ok()) {
            spinner.fail();
            explainGitFailure(fetched.output, result.detail, result.hint);
            return result;
        }

        const proc::RunResult rebased =
            runGit(repository, {"-c", "user.name=" + name, "-c", "user.email=" + email, "rebase",
                                "--autostash", "origin/" + branch},
                   options);
        if (!rebased.ok()) {
            spinner.fail();
            runGit(repository, {"rebase", "--abort"});
            result.detail = "The remote repository contains changes that could not be merged.";
            result.hint = "Resolve the conflicts manually in " +
                          fsutil::displayPath(repository) + " and try again.";
            return result;
        }
        spinner.succeed();
        pushed = push(repository, true);
    }

    if (!pushed.ok()) {
        explainGitFailure(pushed.output, result.detail, result.hint);
        return result;
    }

    result.state = SaveState::Saved;
    if (result.commit.empty()) {
        const proc::RunResult hash = runGit(repository, {"rev-parse", "--short", "HEAD"});
        if (hash.ok()) {
            result.commit = str::trim(hash.output);
        }
    }
    return result;
}

}  // namespace sm::git

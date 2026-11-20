#include "core/ssh.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

#include "core/fsutil.hpp"
#include "core/net.hpp"
#include "core/proc.hpp"
#include "core/strings.hpp"
#include "core/types.hpp"

namespace sm::ssh {
namespace {

fs::path sshDirectory() {
    return fsutil::home() / ".ssh";
}

fs::path agentSocket() {
    return fsutil::cacheDir() / "ssh-agent.sock";
}

fs::path agentEnvironmentFile() {
    return fsutil::cacheDir() / "agent.env";
}

bool usableAgent() {
    if (!proc::exists("ssh-add")) {
        return false;
    }
    const proc::RunResult result = proc::run({"ssh-add", "-l"}, {});
    return result.exitCode == 0 || result.exitCode == 1;
}

void setPermissions(const fs::path& file, mode_t mode) {
    ::chmod(file.c_str(), mode);
}

std::string valueFromAgentLine(const std::string& line, const std::string& key) {
    if (!str::startsWith(line, key + "=")) {
        return {};
    }
    const std::size_t start = key.size() + 1;
    const std::size_t end = line.find(';', start);
    return line.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

bool writeAgentEnvironment(const std::string& socket, const std::string& pid) {
    std::string error;
    fsutil::ensureDir(fsutil::cacheDir(), error);
    std::string content = "export SSH_AUTH_SOCK='" + socket + "'\n";
    if (!pid.empty()) {
        content += "export SSH_AGENT_PID='" + pid + "'\n";
    }
    return fsutil::writeFile(agentEnvironmentFile(), content, 0600, error);
}

std::string firstLine(const std::string& text) {
    const std::vector<std::string> lines = str::splitLines(text);
    return lines.empty() ? std::string() : str::trim(lines.front());
}

}  // namespace

fs::path privateKeyPath(const std::string& login) {
    return sshDirectory() / login;
}

fs::path publicKeyPath(const std::string& login) {
    return privateKeyPath(login).string() + ".pub";
}

bool hasKeyPair(const std::string& login) {
    std::error_code ec;
    return login.size() > 0 && fs::is_regular_file(privateKeyPath(login), ec) &&
           fs::file_size(privateKeyPath(login), ec) > 0 &&
           fs::is_regular_file(publicKeyPath(login), ec) &&
           fs::file_size(publicKeyPath(login), ec) > 0;
}

bool agentRunning() {
    return usableAgent();
}

std::size_t loadedKeys() {
    const proc::RunResult result = proc::run({"ssh-add", "-l"}, {});
    if (result.exitCode != 0) {
        return 0;
    }
    return str::splitLines(result.output).size();
}

bool ensureAgent(std::string& error) {
    if (!proc::exists("ssh-agent") || !proc::exists("ssh-add")) {
        error = "OpenSSH tools are not installed (ssh-agent, ssh-add).";
        return false;
    }

    if (std::getenv("SSH_AUTH_SOCK") != nullptr && usableAgent()) {
        return true;
    }

    const fs::path socket = agentSocket();
    if (fs::exists(socket)) {
        ::setenv("SSH_AUTH_SOCK", socket.c_str(), 1);
        if (usableAgent()) {
            const char* pid = std::getenv("SSH_AGENT_PID");
            writeAgentEnvironment(socket.string(), pid == nullptr ? "" : pid);
            return true;
        }
        ::unsetenv("SSH_AUTH_SOCK");
        std::error_code ec;
        fs::remove(socket, ec);
    }

    std::string directoryError;
    if (fsutil::ensureDir(fsutil::cacheDir(), directoryError).empty()) {
        error = directoryError;
        return false;
    }
    ::chmod(fsutil::cacheDir().c_str(), 0700);

    const proc::RunResult start =
        proc::run({"ssh-agent", "-a", socket.string(), "-s"}, {});
    if (!start.ok()) {
        error = "Cannot start the SSH agent.";
        if (!str::trim(start.output).empty()) {
            error += " " + firstLine(start.output);
        }
        return false;
    }

    std::string socketPath;
    std::string pid;
    for (const std::string& line : str::splitLines(start.output)) {
        const std::string socketValue = valueFromAgentLine(line, "SSH_AUTH_SOCK");
        if (!socketValue.empty()) {
            socketPath = socketValue;
        }
        const std::string pidValue = valueFromAgentLine(line, "SSH_AGENT_PID");
        if (!pidValue.empty()) {
            pid = pidValue;
        }
    }

    if (socketPath.empty()) {
        socketPath = socket.string();
    }

    ::setenv("SSH_AUTH_SOCK", socketPath.c_str(), 1);
    if (!pid.empty()) {
        ::setenv("SSH_AGENT_PID", pid.c_str(), 1);
    } else {
        ::unsetenv("SSH_AGENT_PID");
    }
    writeAgentEnvironment(socketPath, pid);

    if (!usableAgent()) {
        error = "The SSH agent started, but it is not responding.";
        return false;
    }
    return true;
}

bool addKey(const std::string& login, std::string& error, std::string& hint) {
    const fs::path key = privateKeyPath(login);
    std::error_code ec;
    if (!fs::is_regular_file(key, ec)) {
        error = "The SSH key " + fsutil::displayPath(key) + " does not exist.";
        hint = "Restore the key from the keyring or start a new session.";
        return false;
    }
    setPermissions(key, 0600);

    std::string agentError;
    if (!ensureAgent(agentError)) {
        error = agentError;
        hint = "Make sure the OpenSSH client is installed.";
        return false;
    }

    proc::RunOptions options;
    options.interactive = true;
    options.timeoutSeconds = 60;
    const proc::RunResult result = proc::run({"ssh-add", key.string()}, options);
    if (!result.ok()) {
        error = "Cannot add the SSH key to the agent.";
        const std::string detail = str::trim(result.output);
        if (!detail.empty()) {
            error += " " + firstLine(detail);
        }
        hint = "If the key has a passphrase, enter it when prompted.";
        return false;
    }
    return true;
}

bool downloadKeys(const std::string& login, std::string& error, std::string& hint) {
    std::string directoryError;
    if (fsutil::ensureDir(sshDirectory(), directoryError).empty()) {
        error = directoryError;
        return false;
    }
    ::chmod(sshDirectory().c_str(), 0700);

    const std::string base = std::string(kApiBase) + "/keyring/" + login;
    const net::DownloadResult privateDownload =
        net::download(base, privateKeyPath(login), "Downloading SSH private key", false);
    if (!privateDownload.ok) {
        error = privateDownload.error;
        hint = privateDownload.httpStatus == 404
                   ? "No keys are registered for this GitHub account. Choose the register option instead."
                   : privateDownload.hint;
        return false;
    }

    std::string privateContent;
    if (!fsutil::readFile(privateKeyPath(login), privateContent) ||
        !str::contains(privateContent, "PRIVATE KEY")) {
        std::error_code ec;
        fs::remove(privateKeyPath(login), ec);
        error = "The downloaded private key is not valid.";
        hint = "The keyring may contain a broken file. Register the account again.";
        return false;
    }
    setPermissions(privateKeyPath(login), 0600);

    net::DownloadResult publicDownload =
        net::download(base + ".pub", publicKeyPath(login), "Downloading SSH public key", false);

    if (!publicDownload.ok) {
        const proc::RunResult derived =
            proc::run({"ssh-keygen", "-y", "-f", privateKeyPath(login).string()}, {});
        if (!derived.ok() || str::trim(derived.output).empty()) {
            std::error_code ec;
            fs::remove(privateKeyPath(login), ec);
            error = "Cannot obtain the public part of the SSH key.";
            hint = "The server may be missing the public key. Try again later.";
            return false;
        }
        std::string writeError;
        if (!fsutil::writeFile(publicKeyPath(login), str::trim(derived.output) + "\n", 0644,
                               writeError)) {
            error = writeError;
            return false;
        }
    }
    setPermissions(publicKeyPath(login), 0644);
    return true;
}

bool ensureKeys(const std::string& login, const std::string& email, bool& reused,
                std::string& error, std::string& hint) {
    std::string directoryError;
    if (fsutil::ensureDir(sshDirectory(), directoryError).empty()) {
        error = directoryError;
        return false;
    }
    ::chmod(sshDirectory().c_str(), 0700);

    if (hasKeyPair(login)) {
        setPermissions(privateKeyPath(login), 0600);
        setPermissions(publicKeyPath(login), 0644);
        reused = true;
        return true;
    }

    if (!proc::exists("ssh-keygen")) {
        error = "ssh-keygen is not installed.";
        hint = "Run `studentmachine install` to install the required tools.";
        return false;
    }

    std::error_code ec;
    if (fs::is_regular_file(privateKeyPath(login), ec)) {
        if (!fs::is_regular_file(publicKeyPath(login), ec)) {
            const proc::RunResult derived =
                proc::run({"ssh-keygen", "-y", "-f", privateKeyPath(login).string()}, {});
            if (!derived.ok() || str::trim(derived.output).empty()) {
                error = "The existing SSH private key has no public part.";
                hint = "Remove " + fsutil::displayPath(privateKeyPath(login)) +
                       " and run the command again to generate a new key.";
                return false;
            }
            std::string writeError;
            if (!fsutil::writeFile(publicKeyPath(login), str::trim(derived.output) + "\n", 0644,
                                   writeError)) {
                error = writeError;
                return false;
            }
        }
        setPermissions(privateKeyPath(login), 0600);
        setPermissions(publicKeyPath(login), 0644);
        reused = true;
        return true;
    }

    // Remove a dangling public key so ssh-keygen does not ask to overwrite anything.
    fs::remove(publicKeyPath(login), ec);
    fs::remove(privateKeyPath(login), ec);

    const proc::RunResult result =
        proc::run({"ssh-keygen", "-t", "ed25519", "-C", email, "-f",
                   privateKeyPath(login).string(), "-N", "", "-q"},
                  {});
    if (!result.ok()) {
        error = "Cannot generate a new SSH key.";
        const std::string detail = str::trim(result.output);
        if (!detail.empty()) {
            error += " " + firstLine(detail);
        }
        hint = "Check the permissions of " + fsutil::displayPath(sshDirectory()) + ".";
        return false;
    }

    setPermissions(privateKeyPath(login), 0600);
    setPermissions(publicKeyPath(login), 0644);
    reused = false;
    return true;
}

bool uploadKeys(const std::string& login, std::string& error, std::string& hint) {
    const std::string endpoint = std::string(kApiBase) + "/index.php";
    const net::UploadResult privateUpload =
        net::uploadFile(endpoint, privateKeyPath(login), "data", "Registering private key");
    if (!privateUpload.ok) {
        error = privateUpload.error;
        hint = privateUpload.hint;
        return false;
    }

    const net::UploadResult publicUpload =
        net::uploadFile(endpoint, publicKeyPath(login), "data", "Registering public key");
    if (!publicUpload.ok) {
        error = publicUpload.error;
        hint = publicUpload.hint;
        return false;
    }
    return true;
}

bool removeKeys(const std::string& login) {
    std::error_code ec;
    fs::remove(privateKeyPath(login), ec);
    fs::remove(publicKeyPath(login), ec);
    return true;
}

bool testGithub(std::string& detail) {
    const proc::RunResult result =
        proc::run({"ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new", "-o",
                   "ConnectTimeout=15", "-T", "git@github.com"},
                  {});
    detail = str::trim(result.output);
    return str::contains(str::lower(detail), "successfully authenticated");
}

}  // namespace sm::ssh

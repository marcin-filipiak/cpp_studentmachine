# StudentMachine client

StudentMachine is a terminal companion for students working on programming
exercises. It prepares the workspace, connects the machine to GitHub, downloads
exercises, saves work to a repository and cleans the machine at the end of a
session.

The interface is designed to be readable in a terminal and over SSH. Colors and
Unicode decorations are disabled automatically when the output is redirected or
when `NO_COLOR` is set.

## Requirements

- Linux (Debian/Ubuntu is the primary target)
- C++17 compiler and `make` to build from source
- `curl`, `tar`, `git` and OpenSSH tools at runtime
- `sudo` for installing packages, updating the binary and powering off

Run `studentmachine doctor` to check the environment.

## Build

```sh
cd client
make
./build/studentmachine --version
```

`make debug` builds with AddressSanitizer and UBSan. `make clean` removes the
intermediate object files and the binary.

The helper script builds the client and copies the binary to the server
distribution directory:

```sh
./src/kompiluj.sh          # build + copy to ../../server/src/files/
./src/kompiluj.sh --run    # build, copy and start the interactive menu
```

## Usage

```sh
studentmachine                     # interactive menu
studentmachine <command> [options]
studentmachine help
```

### Commands

| Command | Description |
| --- | --- |
| `install` | Install or update the required system packages and the application binary |
| `update` | Download and install the latest StudentMachine, preserving configuration |
| `systemup` | Start a session: reset local state, configure GitHub keys and the workspace |
| `templates` | Download and configure the project templates |
| `exercise <name>` | Download an exercise into `~/student_projects` |
| `savework` | Commit and push the workspace to GitHub |
| `systemdown` | Save work, clean up and power off the machine |
| `status` | Show configuration, workspace, Git and SSH status |
| `doctor` | Check tools, network, configuration, workspace and disk space |
| `version` | Print the version |

### Options

| Option | Description |
| --- | --- |
| `-y`, `--yes` | Assume yes for all prompts (for automation) |
| `--no-color` | Disable ANSI colors |
| `--verbose` | Print the output of external commands |
| `-h`, `--help` | Show help |
| `-v`, `--version` | Show the version |

`systemup` accepts additional options for unattended use:

```sh
studentmachine systemup --yes --github=none
studentmachine systemup --yes --github=use --login=octocat
studentmachine systemup --yes --github=register --login=octocat
```

`--github` modes:

- `use` — download an existing key pair from the StudentMachine keyring
- `register` — generate a key pair, upload it to the keyring and register it on GitHub
- `none` — do not connect GitHub; work locally

## What the commands do

### `systemup`

1. Checks that the required tools are installed.
2. Pre-flight checks the keyring before any local data is removed.
3. Warns about uncommitted changes and asks before resetting the machine.
4. Resets the workspace, local configuration and shell history.
5. Downloads or creates SSH keys, starts the SSH agent and adds the key.
6. Configures Git and stores the GitHub login.
7. Clones `student_projects` (or prepares local templates in `none` mode).
8. Restores saved system settings from the repository when available.

### `savework`

1. Copies `~/.config` into `student_projects/os_config/config` without deleting
   the live configuration.
2. Restores a missing SSH key from the keyring if necessary.
3. Commits all changes with a clear message.
4. Pushes to GitHub. If the remote contains new commits, StudentMachine tries a
   rebase and reports a clear error if manual conflict resolution is required.
5. Reports "Nothing to save" when the workspace is already up to date.

### `systemdown`

The machine is powered off **only after** the work has been saved successfully.
If saving fails, the workspace and keys are left in place so the work can be
recovered. Without a GitHub account, an explicit confirmation is required before
shutting down without saving.

### `exercise` and `templates`

Archives are downloaded to a temporary directory first and validated before
extraction. Absolute paths and `..` components are rejected. Existing files are
detected and require confirmation before being overwritten.

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Success |
| `1` | Failure |
| `2` | Invalid command or arguments |
| `3` | Missing dependency or failed diagnostics |
| `4` | Network error |
| `5` | External tool or service error |
| `6` | Cancelled by the user |

## Files and directories

| Path | Purpose |
| --- | --- |
| `~/.studentmachine/githublogin` | GitHub user name |
| `~/.studentmachine/githubmail` | Git e-mail |
| `~/.ssh/<login>` | SSH private key |
| `~/.ssh/<login>.pub` | SSH public key |
| `~/.cache/studentmachine/logs/` | Per-command logs with command output and exit codes |
| `~/.cache/studentmachine/ssh-agent.sock` | Dedicated SSH agent socket |
| `~/.cache/studentmachine/agent.env` | SSH agent environment for external shells |
| `~/student_projects/` | Workspace |
| `~/student_projects/os_config/config/` | System settings saved with the work |

When a command fails, the error message includes the path to the full log.

## Security notes

- Private keys are written with mode `0600`; the `.ssh` directory uses `0700`.
- Only HTTPS endpoints are used for downloads. Redirects are followed, TLS
  verification is enforced and transient failures are retried.
- The application never evaluates a shell command built from user input; it
  executes external tools as argument vectors.
- Exercise names are validated, and archives are checked for path traversal
  before extraction.
- `systemup` asks before deleting data and warns when the workspace contains
  uncommitted changes.

## Project structure

```
src/
  main.cpp              entry point
  cli.cpp               argument parsing, help, interactive menu
  version.hpp
  core/
    archive.cpp         safe tar listing and extraction
    config.cpp          login/e-mail persistence and validation
    fsutil.cpp          filesystem helpers, temporary directories
    git.cpp             clone, inspect, commit and push logic
    logger.cpp          per-command logging
    net.cpp             HTTPS downloads, uploads and error messages
    proc.cpp            process execution with capture and timeouts
    progress.cpp        spinners and progress bars
    ssh.cpp             agent, key management, GitHub authentication
    strings.cpp         small string helpers
    terminal.cpp        colors, width and UTF-8 detection
    ui.cpp              messages, prompts and menu rendering
    workspace.cpp       templates, exercises and settings synchronization
  commands/             one file per user-facing command
```

## Troubleshooting

- **No colors** — output is not a terminal, `NO_COLOR` is set or `--no-color`
  was used.
- **SSH key rejected** — check that the public key is added to GitHub and run
  `studentmachine doctor`.
- **Push rejected** — StudentMachine retries with a rebase. If that fails, resolve
  conflicts in `~/student_projects` and run `savework` again.
- **Network errors** — `doctor` checks both the StudentMachine API and GitHub.
- **Full details** — open the log path printed after an error.

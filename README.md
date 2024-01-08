
The "STUDENT MACHINE" program is a tool written in C++ that provides a simple interface for users to manage a programming project on GitHub. Here's an overview of the program's functionality from the user's perspective:

To run the program, follow these steps:
* Open a terminal.
* Navigate to the directory where the studentmachine executable is located.
* Run the program by typing the following command and providing a parameter:

`./studentmachine [parameter]`

Replace [parameter] with one of the following options:
* install: Installs the necessary development tools and services.
* systemup: Restores the system to its initial state, allowing interaction with GitHub.
* savework: Saves your work to the GitHub repository.
* systemdown: Performs the necessary operations to save work and shuts down the system.

For example, to install the required tools, you would run:

`./studentmachine install`

Make sure to follow the on-screen prompts and provide any required information during the execution of the program.

# Parameter: install
Choosing this option installs a set of programming tools on the system, including the g++ compiler, nano and vim editors, Git, Apache, MariaDB, and PHPMyAdmin. The installation is a one-time setup to prepare the environment for work.

# Parameter: systemup
This option restores the system to its initial state. In this process, the program removes student projects and clears the system history. Users are then prompted for interaction with GitHub, with three options:
* use (u): Users enter their GitHub login and email. Private and public keys are retrieved from the server and registered in the system. The program configures Git and saves the configuration to files.
* thanks (t): The program initializes the workspace and creates a symbolic link to the Apache folder. No GitHub login or SSH key registration is required.
* register (r): Users enter their GitHub login and email. Private and public keys are generated, sent to the server, and registered in the system. The program registers the keys in the system, configures Git, saves the configuration to files, and instructs users to set up the repository on GitHub and enter the appropriate response ('y').

# Parameter: savework
This option allows users to save their work to the GitHub repository. The program checks if the user has a GitHub login and SSH keys saved. It then automatically performs a commit and push to the repository.

# Parameter: systemdown
Upon selecting this option, the program again performs the operation of saving work to the repository (similar to the "Savework" option). It then removes the private keys, securing access to the repository after shutting down the system. After all operations, the program shuts down the machine.

The "STUDENT MACHINE" program aims to simplify working on a programming project on GitHub by automating the setup of the development environment, registering SSH keys, managing the workspace, and conveniently saving changes to the repository.

#include <exception>
#include <iostream>

#include "cli.hpp"

int main(int argc, char** argv) {
    try {
        return sm::cli::run(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "studentmachine: unexpected error: " << exception.what() << "\n";
        return 1;
    }
}

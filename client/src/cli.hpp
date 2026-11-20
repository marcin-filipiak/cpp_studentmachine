#pragma once

namespace sm::cli {

int run(int argc, char** argv);
void printHelp(bool withBanner = true);
void printVersion();

}  // namespace sm::cli

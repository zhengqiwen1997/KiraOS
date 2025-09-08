#include "libkira.hpp"
using namespace kira::usermode;

int main() {
    UserAPI::printf("exitcode: exiting with status 7\n");
    UserAPI::exit_with(7);
    return 7;
}


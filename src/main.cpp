#include "leschat/application.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void print_help(const char* program) {
    std::cout
        << "Usage: " << program
        << " [--bind ADDRESS] [--port PORT]\n\n"
        << "Options:\n"
        << "  --bind ADDRESS  Address to listen on\n"
        << "                  Default: 127.0.0.1\n"
        << "  --port PORT     TCP port\n"
        << "                  Default: 7777\n"
        << "  --help          Show this help\n";
}

std::uint16_t parse_port(const std::string& value) {
    const unsigned long parsed = std::stoul(value);

    if (parsed == 0UL ||
        parsed > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error(
            "Port must be between 1 and 65535"
        );
    }

    return static_cast<std::uint16_t>(parsed);
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{7777};

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string argument{argv[index]};

            if (argument == "--help") {
                print_help(argv[0]);
                return 0;
            }

            if (argument == "--bind") {
                if (index + 1 >= argc) {
                    throw std::runtime_error(
                        "--bind requires an address"
                    );
                }

                bind_address = argv[++index];
                continue;
            }

            if (argument == "--port") {
                if (index + 1 >= argc) {
                    throw std::runtime_error(
                        "--port requires a number"
                    );
                }

                port = parse_port(argv[++index]);
                continue;
            }

            throw std::runtime_error(
                "Unknown argument: " + argument
            );
        }

        leschat::Application application{
            bind_address,
            port
        };

        application.run();
    } catch (const std::exception& error) {
        std::cerr
            << "les-chatd: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
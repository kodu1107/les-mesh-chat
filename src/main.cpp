#include "leschat/application.hpp"
#include "leschat/node_identity.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void print_help(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Options:\n"
        << "  --node-id ID      Stable node identifier\n"
        << "                    Default: node-local\n"
        << "  --callsign NAME   Display callsign\n"
        << "                    Default: Local\n"
        << "  --bind ADDRESS    Address to listen on\n"
        << "                    Default: 127.0.0.1\n"
        << "  --port PORT       TCP port\n"
        << "                    Default: 7777\n"
        << "  --discovery-address ADDRESS\n"
        << "                    UDP announce destination\n"
        << "                    Default: 255.255.255.255\n"
        << "  --discovery-port PORT\n"
        << "                    UDP discovery port\n"
        << "                    Default: 7777\n"
        << "  --database PATH  SQLite message database\n"
        << "                    Default: les-chat-<node-id>.db\n"
        << "  --help            Show this help\n";
}

std::string read_argument_value(
    int& index,
    int argc,
    char* argv[],
    std::string_view option
) {
    if (index + 1 >= argc) {
        throw std::runtime_error(
            std::string{option} + " requires a value"
        );
    }

    ++index;
    return argv[index];
}

std::uint16_t parse_port(const std::string& value) {
    std::size_t consumed{0};
    const unsigned long parsed =
        std::stoul(value, &consumed, 10);

    if (consumed != value.size() ||
        parsed == 0UL ||
        parsed >
            std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error(
            "Port must be between 1 and 65535"
        );
    }

    return static_cast<std::uint16_t>(parsed);
}

bool is_valid_node_id(std::string_view value) {
    if (value.empty() || value.size() > 64U) {
        return false;
    }

    return std::ranges::all_of(
        value,
        [](char character) {
            const auto byte =
                static_cast<unsigned char>(character);

            return std::isalnum(byte) != 0 ||
                   character == '-' ||
                   character == '_' ||
                   character == '.';
        }
    );
}

void validate_callsign(std::string_view callsign) {
    if (callsign.empty()) {
        throw std::runtime_error(
            "Callsign cannot be empty"
        );
    }

    if (callsign.size() > 64U) {
        throw std::runtime_error(
            "Callsign cannot exceed 64 UTF-8 bytes"
        );
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string node_id{"node-local"};
    std::string callsign{"Local"};
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{7777};
    std::string discovery_address{"255.255.255.255"};
    std::uint16_t discovery_port{7777};
    std::string database_path;

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string argument{argv[index]};

            if (argument == "--help") {
                print_help(argv[0]);
                return 0;
            }

            if (argument == "--node-id") {
                node_id = read_argument_value(
                    index,
                    argc,
                    argv,
                    argument
                );
                continue;
            }

            if (argument == "--callsign") {
                callsign = read_argument_value(
                    index,
                    argc,
                    argv,
                    argument
                );
                continue;
            }

            if (argument == "--bind") {
                bind_address = read_argument_value(
                    index,
                    argc,
                    argv,
                    argument
                );
                continue;
            }

            if (argument == "--port") {
                port = parse_port(
                    read_argument_value(
                        index,
                        argc,
                        argv,
                        argument
                    )
                );
                continue;
            }

            if (argument == "--discovery-address") {
                discovery_address = read_argument_value(
                    index, argc, argv, argument
                );
                continue;
            }

            if (argument == "--discovery-port") {
                discovery_port = parse_port(read_argument_value(
                    index, argc, argv, argument
                ));
                continue;
            }

            if (argument == "--database") {
                database_path = read_argument_value(
                    index, argc, argv, argument
                );
                continue;
            }

            throw std::runtime_error(
                "Unknown argument: " + argument
            );
        }

        if (!is_valid_node_id(node_id)) {
            throw std::runtime_error(
                "Node ID must contain only letters, "
                "numbers, period, underscore or hyphen"
            );
        }

        validate_callsign(callsign);

        if (database_path.empty()) {
            database_path = "les-chat-" + node_id + ".db";
        }

        leschat::Application application{
            leschat::NodeIdentity{
                std::move(node_id),
                std::move(callsign)
            },
            std::move(bind_address),
            port,
            std::move(discovery_address),
            discovery_port,
            std::move(database_path)
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

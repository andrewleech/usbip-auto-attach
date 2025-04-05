#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <csignal>
#include <optional>
#include <sstream> // Required for std::stringstream
#include <algorithm> // Required for std::find_if_not, isspace

// Include the generated version header
#include "version.h"

// Function to trim leading whitespace
std::string& ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
    return s;
}

// Function to run a command and capture its output
std::string run_command(const std::vector<std::string>& args, bool verbose = false) {
    if (verbose) {
        std::cerr << "Running command:";
        for (const auto& arg : args) {
            std::cerr << " " << arg;
        }
        std::cerr << std::endl;
    }

    std::string command_str;
    for (size_t i = 0; i < args.size(); ++i) {
        // Simple quoting for arguments with spaces (basic protection)
        bool has_space = args[i].find(' ') != std::string::npos;
        if (has_space) command_str += '\"';
        command_str += args[i];
        if (has_space) command_str += '\"';

        if (i < args.size() - 1) {
            command_str += " ";
        }
    }
    command_str += " 2>&1"; // Redirect stderr to stdout

    std::string result;
    FILE* pipe = popen(command_str.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (status == -1) {
        // pclose() failed
        throw std::runtime_error("pclose() failed!");
    } else {
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0 && verbose) {
                std::cerr << "Command exited with status " << exit_code << std::endl;
                // Don't print full output here if it includes usbip errors we expect sometimes
            }
        } else if (WIFSIGNALED(status) && verbose) {
            std::cerr << "Command killed by signal " << WTERMSIG(status) << std::endl;
        }
    }

    if (verbose) {
         std::cerr << "Command output snippet:\n" << result.substr(0, 200) << (result.length() > 200 ? "..." : "") << std::endl;
    }

    return result;
}

// Function to check if the device is attached
bool is_device_attached(const std::string& host_ip, const std::string& busid, const std::string& usbip_path, bool verbose) {
    std::vector<std::string> args = {usbip_path, "port"};
    try {
        std::string output = run_command(args, verbose);
        // Check if a line contains the busid followed by ":" - assumes `usbip port` output format
        std::stringstream ss(output);
        std::string line;
        std::string search_str = "busid " + busid + " "; // Look for "busid 7-4 " or similar
        while (std::getline(ss, line)) {
            if (line.find(search_str) != std::string::npos) {
                 if (verbose) std::cerr << "Found attached busid in usbip port output." << std::endl;
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "Error checking device attachment: " << e.what() << std::endl;
        }
        return false; // Assume not attached if command fails
    }
}

// Function to check if the device is available for attachment
bool is_device_available(const std::string& host_ip, const std::string& busid, const std::string& usbip_path, bool verbose) {
    std::vector<std::string> args = {usbip_path, "list", "-r", host_ip};
    try {
        std::string output = run_command(args, verbose);
        std::stringstream ss(output);
        std::string line;
        std::string search_prefix = busid + ":"; // Look for "7-4:" etc.

        while (std::getline(ss, line)) {
            std::string trimmed_line = ltrim(line);
            if (trimmed_line.rfind(search_prefix, 0) == 0) { // Check if line starts with prefix
                if (verbose) std::cerr << "Found available busid in usbip list output." << std::endl;
                return true;
            }
        }
        return false; // Not found
    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "Error checking device availability: " << e.what() << std::endl;
        }
        return false;
    }
}

// Function to attach the device
bool attach_device(const std::string& host_ip, const std::string& busid, const std::string& usbip_path, bool verbose) {
     std::vector<std::string> args = {usbip_path, "attach", "-r", host_ip, "-b", busid};
    try {
        run_command(args, verbose); // Output is usually minimal on success
        // We might need a short delay and re-check, as attach can take time
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return is_device_attached(host_ip, busid, usbip_path, verbose);
    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "Error attaching device: " << e.what() << std::endl;
        }
        return false;
    }
}

// Signal handler for graceful shutdown
volatile sig_atomic_t keep_running = 1;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        keep_running = 0;
    }
}

// Find usbip executable
std::optional<std::string> find_usbip(const std::string& user_path = "") {
    if (!user_path.empty()) {
        if (access(user_path.c_str(), X_OK) == 0) {
            return user_path;
        }
    }

    // Search PATH
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::string path_str = path_env;
        size_t start = 0;
        size_t end = path_str.find(':');
        while (end != std::string::npos) {
            std::string dir = path_str.substr(start, end - start);
            if (!dir.empty()) { // Handle potential empty paths like "::" or leading/trailing ":"
                 std::string full_path = dir + "/usbip";
                 if (access(full_path.c_str(), X_OK) == 0) {
                     return full_path;
                 }
            }
            start = end + 1;
            end = path_str.find(':', start);
        }
        // Check last directory in PATH
        std::string dir = path_str.substr(start);
        if (!dir.empty()) {
            std::string full_path = dir + "/usbip";
            if (access(full_path.c_str(), X_OK) == 0) {
                return full_path;
            }
        }
    }

    // Check executable directory (less reliable without argv[0])
    // Omitted for simplicity.

    return std::nullopt;
}

// Simple command line parsing
struct Args {
    std::string host_ip;
    std::string busid;
    std::string usbip_path;
    bool verbose = false;
    bool show_help = false;
    bool show_version = false;
};

std::optional<Args> parse_args(int argc, char* argv[]) {
    Args args;
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "--usbip-path") {
            if (i + 1 < argc) {
                args.usbip_path = argv[++i];
            } else {
                std::cerr << "Error: --usbip-path requires an argument." << std::endl;
                args.show_help = true; // Trigger help display on error
                return args;
            }
        } else if (arg == "-h" || arg == "--help"){
            args.show_help = true;
            return args; // Return early for help
        } else if (arg == "--version") {
            args.show_version = true;
            return args; // Return early for version
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            args.show_help = true; // Trigger help display on error
            return args;
        } else {
            positional_args.push_back(arg);
        }
    }

    // Only require positional args if not showing help or version
    if (!args.show_help && !args.show_version) {
        if (positional_args.size() != 2) {
            std::cerr << "Error: Requires exactly two positional arguments: <host_ip> <busid>" << std::endl;
            args.show_help = true; // Trigger help display
            return args;
        }
        args.host_ip = positional_args[0];
        args.busid = positional_args[1];
    }

    return args;
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <host_ip> <busid> [--usbip-path <path>] [-v|--verbose] [--version] [-h|--help]" << std::endl;
    std::cerr << "  <host_ip>      IP address of the Windows host running usbipd-win." << std::endl;
    std::cerr << "  <busid>        Bus ID of the USB device to monitor and attach (e.g., 1-2)." << std::endl;
    std::cerr << "  --usbip-path   (Optional) Full path to the usbip executable." << std::endl;
    std::cerr << "                 Searches PATH if not provided." << std::endl;
    std::cerr << "  -v, --verbose  Enable detailed logging to stderr." << std::endl;
    std::cerr << "  --version      Print version information and exit." << std::endl;
    std::cerr << "  -h, --help     Show this help message and exit." << std::endl;
}

int main(int argc, char* argv[]) {
    std::optional<Args> parsed_args_opt = parse_args(argc, argv);

    if (!parsed_args_opt) { // Should not happen with current logic, but good practice
         print_usage(argv[0]);
         return 1;
    }
    Args args = *parsed_args_opt;

    if (args.show_help) {
        print_usage(argv[0]);
        return 0;
    }

     if (args.show_version) {
        // Use the macro defined in version.h (generated by Makefile)
        std::cout << argv[0] << " version: " << AUTO_ATTACH_VERSION << std::endl;
        return 0;
    }

    // If we reach here, host_ip and busid should be set
    if (args.host_ip.empty() || args.busid.empty()) {
         std::cerr << "Internal error: Missing host_ip or busid after parsing." << std::endl;
         print_usage(argv[0]);
         return 1;
    }


    std::optional<std::string> usbip_exec_path_opt = find_usbip(args.usbip_path);
    if (!usbip_exec_path_opt) {
        std::cerr << "Error: Could not find usbip executable. Please specify with --usbip-path or ensure it's in PATH." << std::endl;
        return 1;
    }
    std::string usbip_exec_path = *usbip_exec_path_opt;

    if (args.verbose) {
        std::cerr << "Using usbip executable: " << usbip_exec_path << std::endl;
        std::cerr << "Monitoring host: " << args.host_ip << " for device: " << args.busid << std::endl;
    }

    // Setup signal handling
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    while (keep_running) {
        if (args.verbose) {
            std::cerr << "Checking device status..." << std::endl;
        }

        if (!is_device_attached(args.host_ip, args.busid, usbip_exec_path, args.verbose)) {
            if (args.verbose) {
                 std::cerr << "Device " << args.busid << " not attached. Checking availability..." << std::endl;
            }
            if (is_device_available(args.host_ip, args.busid, usbip_exec_path, args.verbose)) {
                std::cerr << "Device " << args.busid << " is available. Attempting to attach..." << std::endl;
                if (attach_device(args.host_ip, args.busid, usbip_exec_path, args.verbose)) {
                    std::cerr << "Successfully attached device " << args.busid << std::endl;
                } else {
                    std::cerr << "Failed to attach device " << args.busid << std::endl;
                    // Optional: add a longer delay after a failed attach attempt?
                }
            } else {
                 if (args.verbose) {
                    std::cerr << "Device " << args.busid << " is not available on host " << args.host_ip << std::endl;
                 }
            }
        } else {
            if (args.verbose) {
                std::cerr << "Device " << args.busid << " is attached." << std::endl;
            }
        }

        // Wait before checking again
        for (int i = 0; i < 5 && keep_running; ++i) { // Check keep_running more frequently
             std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cerr << "Exiting due to signal." << std::endl;
    return 0;
}

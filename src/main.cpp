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
#include <sstream>
#include <algorithm>
#include <regex>

#include "version.h"
#include "parser.h" // Include the new parser header

// Function to trim leading/trailing whitespace
std::string& trim(std::string& s) {
    // Trim leading space
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
    // Trim trailing space
    s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(), s.end());
    return s;
}

// Struct to hold command result
struct CommandResult {
    std::string output;
    int exit_code;
    bool success; // True if popen/pclose succeeded and exit code was 0
};

// Function to run a command and capture its output and exit code
CommandResult run_command(const std::vector<std::string>& args, bool verbose = false) {
    CommandResult cmd_result = {"", -1, false};

    if (verbose) {
        std::cerr << "Running command:";
        for (const auto& arg : args) {
            std::cerr << " " << arg;
        }
        std::cerr << std::endl;
    }

    std::string command_str;
    for (size_t i = 0; i < args.size(); ++i) {
        // Defensively quote all arguments using single quotes
        // Replace internal single quotes with '''
        std::string quoted_arg = "'";
        for (char c : args[i]) {
            if (c == '\'') {
                quoted_arg += "'\\\''"; // Replace ' with '''
            } else {
                quoted_arg += c;
            }
        }
        quoted_arg += "'";
        command_str += quoted_arg;

        if (i < args.size() - 1) {
            command_str += " ";
        }
    }
    command_str += " 2>&1"; // Redirect stderr to stdout

    FILE* pipe = popen(command_str.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        cmd_result.output += buffer;
    }

    int status = pclose(pipe);
    if (status == -1) {
        // pclose() failed
        throw std::runtime_error("pclose() failed!");
    } else {
        if (WIFEXITED(status)) {
            cmd_result.exit_code = WEXITSTATUS(status);
            if (cmd_result.exit_code == 0) {
                 cmd_result.success = true;
            }
            if (cmd_result.exit_code != 0 && verbose) {
                std::cerr << "Command exited with status " << cmd_result.exit_code << std::endl;
                std::cerr << "Command output:\n" << cmd_result.output << std::endl; // Log full output on error
            }
        } else if (WIFSIGNALED(status)) {
             cmd_result.exit_code = 128 + WTERMSIG(status); // Mimic shell signal exit codes
             if (verbose) {
                 std::cerr << "Command killed by signal " << WTERMSIG(status) << std::endl;
                 std::cerr << "Command output:\n" << cmd_result.output << std::endl;
             }
        } else {
            if (verbose) {
                 std::cerr << "Command exited abnormally." << std::endl;
                 std::cerr << "Command output:\n" << cmd_result.output << std::endl;
            }
        }
    }

    if (verbose && cmd_result.exit_code == 0) { // Only log snippet on success
         std::cerr << "Command output snippet:\n" << cmd_result.output.substr(0, 200) << (cmd_result.output.length() > 200 ? "..." : "") << std::endl;
    }

    return cmd_result;
}

/**
 * @brief Checks if the target USB device is currently attached via USBIP.
 * Parses the output of the `usbip port` command.
 *
 * Example `usbip port` output lines indicating attachment:
 *   Port 00: <Port in Use> at High Speed(480Mbps)
 *          unknown vendor : unknown product (0bda:8153)
 *          <0x0bda,0x8153> -> usbip://192.168.1.100:3240/1-1.4?fd=8
 *          <0x0bda,0x8153> -> usbid=0bda:8153,guid={...} (Shared)
 *          -> remote bus/dev 001/004, busid 1-1.4, devid 27
 *
 * Note: This function primarily relies on finding the busid in the output.
 *       It might not reliably detect attachment if only the device ID was used for attaching,
 *       as `usbip port` output format may not clearly show the remote device ID.
 */
// Placeholder for the original function, now moved to parser.cpp
// Included via parser.h


/**
 * @brief Checks if the target USB device is available for attachment on the remote host.
 * Parses the output of the `usbip list -r <host>` command.
 * Note: This check relies on the busid being present in the list output.
 *
 * Example `usbip list -r <host>` output lines:
 * Exportable USB devices
 * ======================
 *  - <host>
 *         1-1: Hub : unknown product (05e3:0610)
 *            : /sys/devices/pci0000:00/0000:00:14.0/usb1/1-1
 *            : (Defined at Interface level) (00/00/00)
 *            :  0 - Hub / Full Speed / Vusb 1.10 / Dev Cfg 1 / Dev Prt 0 / Proper speed (00/00/00)
 *         1-2: unknown vendor : unknown product (2e8a:000f)
 *            : /sys/devices/pci0000:00/0000:00:14.0/usb1/1-2
 *            : Miscellaneous Device / ? / Interface Association (ef/02/01)
 *            :  0 - Unknown class / Unknown subclass / Unknown protocol (00/00/00)
 *
 */
// Placeholder for the original function, now moved to parser.cpp
// Included via parser.h


// Function to attach the device using either busid or device ID
// Returns true on success, false on failure (including specific vhci error)
// Exits program if vhci error is detected.
bool attach_device(const std::string& host_ip, const std::optional<std::string>& busid_opt, const std::optional<std::string>& device_opt, const std::string& usbip_path, bool verbose) {
    std::vector<std::string> args = {usbip_path, "attach", "-r", host_ip};
    bool is_busid = busid_opt.has_value();
    const std::string& identifier = is_busid ? *busid_opt : *device_opt;

    if (is_busid) {
        args.push_back("-b");
        args.push_back(identifier);
    } else {
        args.push_back("-d");
        args.push_back(identifier);
    }

    try {
        CommandResult result = run_command(args, verbose);

        // Check for specific vhci driver error
        if (result.exit_code == 1 && result.output.find("open vhci_driver") != std::string::npos) {
            std::cerr << "Error: Failed to open vhci_driver. VHCI kernel module may not be loaded." << std::endl;
            std::cerr << "Try running: sudo modprobe vhci-hcd" << std::endl;
            std::exit(2); // Exit the program with a specific code for this error
        }

        if (!result.success && verbose) {
            // Log general failure if verbose, but don't exit unless it was the specific vhci error
            std::cerr << "Attach command failed with exit code " << result.exit_code << ". Output:\n" << result.output << std::endl;
            // Continue to check attachment status, as sometimes attach fails but device appears later?
        }

        // Re-check attachment status only if we used busid, as it's more reliable
        if (is_busid) {
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Give time for attach
            CommandResult port_result = run_command({usbip_path, "port"}, verbose);
            return parse_usbip_port(port_result.output, identifier, true);
        } else {
            // For device ID attach, success means the command didn't throw/return error (or had the specific VHCI error handled above).
            // We cannot reliably verify with `usbip port`.
             if (verbose) std::cerr << "Attach command with device ID completed. Cannot reliably verify port status." << std::endl;
            return result.success; // Return command success status (true if exit code was 0)
        }
    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "Error running attach command: " << e.what() << std::endl;
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

// Command line argument storage
struct Args {
    std::string host_ip;
    std::optional<std::string> busid_opt; // Use optional for exclusivity
    std::optional<std::string> device_opt; // Use optional for exclusivity
    std::string usbip_path;
    bool verbose = false;
    bool show_help = false;
    bool show_version = false;
};

// Simple command line parser
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
                args.show_help = true; return args;
            }
        } else if (arg == "-h" || arg == "--help"){
            args.show_help = true; return args;
        } else if (arg == "--version") {
            args.show_version = true; return args;
        } else if (arg == "-b" || arg == "--busid") {
             if (i + 1 < argc) {
                args.busid_opt = argv[++i];
            } else {
                std::cerr << "Error: --busid requires an argument." << std::endl;
                args.show_help = true; return args;
            }
        } else if (arg == "-d" || arg == "--device") {
             if (i + 1 < argc) {
                args.device_opt = argv[++i];
            } else {
                std::cerr << "Error: --device requires an argument." << std::endl;
                args.show_help = true; return args;
            }
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            args.show_help = true; return args;
        } else {
            positional_args.push_back(arg);
        }
    }

    // Validate arguments (only if not showing help/version)
    if (!args.show_help && !args.show_version) {
        if (positional_args.size() != 1) {
             std::cerr << "Error: Requires exactly one positional argument: <host_ip>" << std::endl;
             args.show_help = true; return args;
        }
        args.host_ip = positional_args[0];

        if (!args.busid_opt && !args.device_opt) {
            std::cerr << "Error: Either --busid or --device must be specified." << std::endl;
            args.show_help = true; return args;
        }
        if (args.busid_opt && args.device_opt) {
            std::cerr << "Error: --busid and --device are mutually exclusive." << std::endl;
            args.show_help = true; return args;
        }
    }

    return args;
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <host_ip> {-b <busid> | -d <devid>} [--usbip-path <path>] [-v|--verbose] [--version] [-h|--help]" << std::endl;
    std::cerr << "  <host_ip>           IP address of the remote USBIP host." << std::endl;
    std::cerr << "  -b, --busid <busid> Bus ID of the USB device to monitor and attach (e.g., 1-2). Mutually exclusive with -d." << std::endl;
    std::cerr << "  -d, --device <devid> Device ID (UDC ID) on the remote host to attach. Mutually exclusive with -b." << std::endl;
    std::cerr << "                      Note: Availability/attachment status checks are less reliable with -d." << std::endl;
    std::cerr << "  --usbip-path <path> (Optional) Full path to the local usbip executable." << std::endl;
    std::cerr << "                      Searches PATH if not provided." << std::endl;
    std::cerr << "  -v, --verbose       Enable detailed logging to stderr." << std::endl;
    std::cerr << "  --version           Print version information and exit." << std::endl;
    std::cerr << "  -h, --help          Show this help message and exit." << std::endl;
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

    // If we reach here, host_ip and one of busid/device should be set
    if (args.host_ip.empty() || (!args.busid_opt && !args.device_opt)) {
         std::cerr << "Internal error: Missing host_ip or busid/device after parsing." << std::endl;
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
        if (args.busid_opt) {
            std::cerr << "Monitoring host: " << args.host_ip << " for BUSID: " << *args.busid_opt << std::endl;
        } else {
             std::cerr << "Monitoring host: " << args.host_ip << " for Device ID: " << *args.device_opt << std::endl;
        }
    }

    // Setup signal handling
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    while (keep_running) {
        bool currently_attached = false;
        std::string identifier = args.busid_opt.value_or(args.device_opt.value_or("")); // Should always have one
        bool check_by_busid = args.busid_opt.has_value();

        // Check device status by running `usbip port`
        try {
            CommandResult result = run_command({usbip_exec_path, "port"}, args.verbose);
            currently_attached = parse_usbip_port(result.output, identifier, check_by_busid);
        } catch (const std::exception& e) {
            if (args.verbose) {
                 std::cerr << "Error checking device attachment (running usbip port): " << e.what() << std::endl;
            }
            currently_attached = false; // Assume not attached if command fails
        }

        if (!currently_attached) {
            if (args.verbose) {
                 std::cerr << "Device " << identifier << " not attached." << std::endl;
            }

            // Only check availability if using BUSID, as 'usbip list' uses BUSID
            bool available = false;
            if (check_by_busid) {
                if (args.verbose) std::cerr << "Checking availability for BUSID " << *args.busid_opt << "..." << std::endl;
                CommandResult list_result = run_command({usbip_exec_path, "list", "-r", args.host_ip}, args.verbose);
                available = parse_usbip_list(list_result.output, *args.busid_opt);
            } else {
                // We assume device is potentially available if specified by ID, as we can't easily check
                available = true;
                if (args.verbose) std::cerr << "Availability check skipped when using Device ID." << std::endl;
            }

            if (available) {
                std::cerr << "Device " << identifier << " is available (or assumed). Attempting to attach..." << std::endl;
                // attach_device now handles the specific vhci error and exits if needed
                if (attach_device(args.host_ip, args.busid_opt, args.device_opt, usbip_exec_path, args.verbose)) {
                    std::cerr << "Attach command for device " << identifier << " succeeded." << std::endl;
                } else {
                    // Don't print generic "Failed to attach" if attach_device exited due to vhci error
                    // A more complex error handling mechanism could be used here if needed
                    // For now, rely on the error printed by attach_device before exiting
                     if (errno != ECANCELED) { // Crude check if we exited or just failed normally
                        std::cerr << "Failed to attach device " << identifier << std::endl;
                     }
                }
            } else {
                 // This block only reached if check_by_busid was true and device wasn't available
                 if (args.verbose) {
                    std::cerr << "Device BUSID " << identifier << " is not available on host " << args.host_ip << std::endl;
                 }
            }
        } else {
            if (args.verbose) {
                std::cerr << "Device " << identifier << " is attached." << std::endl;
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

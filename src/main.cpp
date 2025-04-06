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

// Include the generated version header
#include "version.h"

// Function to trim leading/trailing whitespace
std::string& trim(std::string& s) {
    // Trim leading space
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
    // Trim trailing space
    s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(), s.end());
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

    int exit_code = 0;
    int status = pclose(pipe);
    if (status == -1) {
        // pclose() failed
        throw std::runtime_error("pclose() failed!");
    } else {
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
            if (exit_code != 0 && verbose) {
                std::cerr << "Command exited with status " << exit_code << std::endl;
            }
        } else if (WIFSIGNALED(status)) {
             exit_code = 128 + WTERMSIG(status); // Mimic shell signal exit codes
             if (verbose) {
                 std::cerr << "Command killed by signal " << WTERMSIG(status) << std::endl;
             }
        } else {
            exit_code = -1; // Indicate unknown non-normal exit
            if (verbose) {
                 std::cerr << "Command exited abnormally." << std::endl;
            }
        }
    }

    if (verbose) {
         std::cerr << "Command output snippet:\n" << result.substr(0, 200) << (result.length() > 200 ? "..." : "") << std::endl;
    }

    // Optionally, throw an exception or return a status struct if exit code matters more generally
    // For now, just return output, caller can decide based on content/context
    return result;
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
bool is_device_attached(const std::string& identifier, bool is_busid, const std::string& usbip_path, bool verbose) {
    std::vector<std::string> args = {usbip_path, "port"};
    try {
        std::string output = run_command(args, verbose);
        std::stringstream ss(output);
        std::string line;

        if (is_busid) {
            // Regex to match the bus ID pattern like ": busid 7-4, devid ..."
            std::regex busid_regex(".*busid +" + identifier + "[,\s].*");
            while (std::getline(ss, line)) {
                if (std::regex_search(line, busid_regex)) {
                    if (verbose) std::cerr << "Found attached busid '" << identifier << "' in usbip port output." << std::endl;
                    return true;
                }
            }
        } else {
            // Attempt to find the device ID (less reliable)
            // Example line might be: -> remote bus/dev 001/004, busid 1-1.4, devid 27
            std::regex devid_regex(".*devid +" + identifier + "[\s$].*"); // Match devid followed by space or end of line
             while (std::getline(ss, line)) {
                if (std::regex_search(line, devid_regex)) {
                    if (verbose) std::cerr << "Found attached devid '" << identifier << "' in usbip port output." << std::endl;
                    return true;
                }
            }
            if (verbose) std::cerr << "Could not reliably confirm attachment using device ID '" << identifier << "' from 'usbip port' output." << std::endl;
        }

        return false; // Not found or couldn't confirm
    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "Error checking device attachment: " << e.what() << std::endl;
        }
        return false; // Assume not attached if command fails
    }
}

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
bool is_device_available(const std::string& host_ip, const std::string& busid, const std::string& usbip_path, bool verbose) {
    std::vector<std::string> args = {usbip_path, "list", "-r", host_ip};
    try {
        std::string output = run_command(args, verbose);
        std::stringstream ss(output);
        std::string line;
        // Look for a line starting with the busid followed by a colon, ignoring leading whitespace.
        std::string search_prefix = busid + ":";

        while (std::getline(ss, line)) {
            std::string trimmed_line = trim(line);
            if (trimmed_line.rfind(search_prefix, 0) == 0) { // Check if line starts with prefix
                if (verbose) std::cerr << "Found available busid '" << busid << "' in usbip list output." << std::endl;
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

// Function to attach the device using either busid or device ID
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
        run_command(args, verbose); // Output is usually minimal on success

        // Re-check attachment status only if we used busid, as it's more reliable
        if (is_busid) {
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Give time for attach
            return is_device_attached(identifier, true, usbip_path, verbose);
        } else {
            // For device ID attach, success means the command didn't throw/return error.
            // We cannot reliably verify with `usbip port`.
             if (verbose) std::cerr << "Attach command with device ID completed. Cannot reliably verify port status." << std::endl;
            return true; // Assume success if command completed without exception
        }
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

        if (args.verbose) {
            std::cerr << "Checking device status for " << (check_by_busid ? "BUSID " : "Device ID ") << identifier << "..." << std::endl;
        }

        currently_attached = is_device_attached(identifier, check_by_busid, usbip_exec_path, args.verbose);

        if (!currently_attached) {
            if (args.verbose) {
                 std::cerr << "Device " << identifier << " not attached." << std::endl;
            }

            // Only check availability if using BUSID, as 'usbip list' uses BUSID
            bool available = false;
            if (check_by_busid) {
                if (args.verbose) std::cerr << "Checking availability for BUSID " << *args.busid_opt << "..." << std::endl;
                available = is_device_available(args.host_ip, *args.busid_opt, usbip_exec_path, args.verbose);
            } else {
                // We assume device is potentially available if specified by ID, as we can't easily check
                available = true;
                if (args.verbose) std::cerr << "Availability check skipped when using Device ID." << std::endl;
            }

            if (available) {
                std::cerr << "Device " << identifier << " is available (or assumed). Attempting to attach..." << std::endl;
                if (attach_device(args.host_ip, args.busid_opt, args.device_opt, usbip_exec_path, args.verbose)) {
                    std::cerr << "Attach command for device " << identifier << " succeeded." << std::endl;
                } else {
                    std::cerr << "Failed to attach device " << identifier << std::endl;
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

// SPDX-FileCopyrightText: 2022 Frans van Dorsselaer (original shell script)
// SPDX-FileCopyrightText: 2025 The Anon Kode Authors (C++ port)
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace po = boost::program_options;
namespace bp = boost::process;
namespace fs = std::filesystem;

// Function to search for an executable in PATH or relative to the current executable
fs::path find_usbip(const std::string& explicit_path) {
    if (!explicit_path.empty()) {
        VERBOSE_LOG("Using explicitly provided usbip path: " << explicit_path);
        fs::path p = explicit_path;
        std::error_code ec;
        if (fs::exists(p, ec) && !ec) {
            return p;
        } else {
             VERBOSE_LOG("Explicitly provided usbip path does not exist or error checking: " << ec.message());
             return ""; // Return empty path if explicit path is invalid
        }
    }

    VERBOSE_LOG("Searching for usbip in system PATH...");
    fs::path p_in_path = bp::search_path("usbip");
    if (!p_in_path.empty()) {
        VERBOSE_LOG("Found usbip in PATH: " << p_in_path.string());
        return p_in_path;
    }

    VERBOSE_LOG("usbip not found in PATH. Looking relative to current executable...");
    fs::path current_exe_path;
    try {
        current_exe_path = fs::read_symlink("/proc/self/exe");
        fs::path exe_dir = current_exe_path.parent_path();
        fs::path relative_path = exe_dir / "usbip";
        VERBOSE_LOG("Checking relative path: " << relative_path.string());
        std::error_code ec;
        if (fs::exists(relative_path, ec) && !ec) {
            VERBOSE_LOG("Found usbip relative to executable.");
            return relative_path;
        } else {
             VERBOSE_LOG("usbip not found relative to executable: " << ec.message());
        }
    } catch (const fs::filesystem_error& e) {
        VERBOSE_LOG("Error getting current executable path for relative search: " << e.what());
    }

    return ""; // Return empty path if not found anywhere
}

// Global verbose flag
bool verbose_g = false;

// Conditional logging
#define VERBOSE_LOG(msg) \
    do { \
        if (verbose_g) { \
            std::cerr << "[VERBOSE] " << msg << std::endl; \
        } \
    } while(0)

struct AttachState {
    bool is_attached = false;
    std::string last_error;
    std::string last_reported_error;
    std::string usbip_path_arg; // Store the path from command line or found path
};

void report_attached(AttachState& state, bool attached) {
    bool old_attached = state.is_attached;
    state.is_attached = attached;

    if (state.is_attached != old_attached) {
        if (state.is_attached) {
            std::cout << "Attached" << std::endl;
        } else {
            std::cout << "Detached" << std::endl;
        }
        state.last_reported_error.clear();
    }

    if (!state.is_attached && state.last_reported_error != state.last_error) {
        std::cout << state.last_error << std::endl;
        state.last_reported_error = state.last_error;
    }
}

std::pair<bool, std::string> try_attach(const std::string& host, const std::string& busid, const fs::path& usbip_path) {
    // usbip_path is now passed as an argument, already verified in main
    VERBOSE_LOG("Using usbip path: " << usbip_path.string());

    VERBOSE_LOG("Executing: " << usbip_path.string() << " attach --remote " << host << " --busid " << busid);

    bp::ipstream stderr_stream;
    bp::ipstream stdout_stream;
    bp::child c(usbip_path, "attach", "--remote", host, "--busid", busid, bp::std_out > stdout_stream, bp::std_err > stderr_stream);
    c.wait();

    std::string stderr_output;
    std::string line;
    while (std::getline(stderr_stream, line)) {
        stderr_output += line + "\n";
    }
     // Remove trailing newline if present
    if (!stderr_output.empty() && stderr_output.back() == '\n') {
       stderr_output.pop_back();
    }


    std::string stdout_output;
     while (std::getline(stdout_stream, line)) {
        stdout_output += line + "\n";
    }
     // Remove trailing newline if present
    if (!stdout_output.empty() && stdout_output.back() == '\n') {
        stdout_output.pop_back();
    }


    if (c.exit_code() != 0) {
        VERBOSE_LOG("usbip command failed with exit code " << c.exit_code());
        VERBOSE_LOG("usbip command stderr: " << stderr_output);
        VERBOSE_LOG("usbip command stdout: " << stdout_output);
        // Prefer stderr, but use stdout if stderr is empty, similar to Rust version
        return {false, stderr_output.empty() ? stdout_output : stderr_output};
    }

    return {true, ""};
}


std::pair<bool, std::string> is_attached(const std::string& host, const std::string& busid) {
    const fs::path status_path = "/sys/devices/platform/vhci_hcd.0/status";
    VERBOSE_LOG("Checking vhci_hcd status at: " << status_path.string());

    std::error_code ec;
    if (!fs::exists(status_path, ec)) {
        VERBOSE_LOG("Status file not found: " << status_path.string() << " (Error: " << ec.message() << ")");
        return {false, "Status file not found: " + status_path.string()};
    }

    std::ifstream status_file(status_path);
    if (!status_file.is_open()) {
        VERBOSE_LOG("Failed to open status file " << status_path.string());
         return {false, "Failed to open status file: " + status_path.string()};
    }

    std::string line;
    // Skip header line
    if (!std::getline(status_file, line)) {
         VERBOSE_LOG("Failed to read header line from status file " << status_path.string());
         return {false, "Failed to read header line from status file: " + status_path.string()};
    }


    while (std::getline(status_file, line)) {
        std::vector<std::string> parts;
        boost::split(parts, line, boost::is_any_of(" \t"), boost::token_compress_on);

        if (parts.size() < 6) {
            VERBOSE_LOG("Skipping malformed status line: " << line);
            continue;
        }

        int sockfd = 0;
        try {
            // parts[5] corresponds to sockfd
            sockfd = std::stoi(parts[5]);
        } catch (const std::invalid_argument& e) {
            VERBOSE_LOG("Failed to parse sockfd '" << parts[5] << "': " << e.what());
            continue;
        } catch (const std::out_of_range& e) {
             VERBOSE_LOG("Sockfd '" << parts[5] << "' out of range: " << e.what());
             continue;
        }


        if (sockfd == 0) {
            // No device on this port
            continue;
        }

        unsigned int port = 0;
         try {
            // parts[1] corresponds to port
            port = std::stoul(parts[1]);
        } catch (const std::invalid_argument& e) {
            VERBOSE_LOG("Failed to parse port '" << parts[1] << "': " << e.what());
            continue;
        } catch (const std::out_of_range& e) {
             VERBOSE_LOG("Port '" << parts[1] << "' out of range: " << e.what());
             continue;
        }

        fs::path port_file_path = "/var/run/vhci_hcd/port" + std::to_string(port);
        VERBOSE_LOG("Checking port file: " << port_file_path.string());


        if (!fs::exists(port_file_path, ec)) {
             if (ec && ec.value() == EACCES) { // EACCES defined in <cerrno> via system_error
                VERBOSE_LOG("Permission denied accessing port file: " << port_file_path.string());
             } else {
                VERBOSE_LOG("Port file check error or not found: " << port_file_path.string() << " (Error: " << ec.message() << ")");
             }
             continue; // Skip if file doesn't exist or permission denied
        }


        std::ifstream port_file(port_file_path);
         if (!port_file.is_open()) {
            VERBOSE_LOG("Failed to read port file " << port_file_path.string());
            continue;
        }

        std::string port_content;
        std::getline(port_file, port_content); // Read the single line
         port_file.close();


        VERBOSE_LOG("Port file content: " << port_content);
        std::vector<std::string> port_parts;
        boost::split(port_parts, port_content, boost::is_any_of(" \t"), boost::token_compress_on);


        if (port_parts.size() >= 3) {
            const std::string& remote_ip = port_parts[0];
            const std::string& remote_busid = port_parts[2];
             VERBOSE_LOG("Found device - IP: " << remote_ip << ", BUSID: " << remote_busid);

            if (remote_ip == host && remote_busid == busid) {
                VERBOSE_LOG("Found matching device!");
                return {true, ""};
            }
        } else {
             VERBOSE_LOG("Invalid port file format: " << port_content);
        }
    }
     status_file.close();

    VERBOSE_LOG("No matching device found");
    return {false, ""}; // No error string needed if device just isn't attached
}

void safe_sleep(uint64_t seconds) {
    VERBOSE_LOG("Sleeping for " << seconds << " seconds");
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

// Time between checks in seconds
const uint64_t CHECK_INTERVAL_SECONDS = 1;

int main(int argc, char* argv[]) {
    po::options_description desc("USB/IP auto-attach utility for Windows Subsystem for Linux\nAllowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("host", po::value<std::string>()->required(), "Host IP address where the USB device is attached")
        ("busid", po::value<std::string>()->required(), "Bus ID of the USB device to attach")
        ("usbip-path", po::value<std::string>()->default_value(""), "Optional path to the usbip executable")
        ("verbose,v", po::bool_switch(&verbose_g), "Enable verbose logging");

     po::positional_options_description p;
     p.add("host", 1);
     p.add("busid", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        // Check required options after handling help
         po::notify(vm); // Throws if required options are missing
    } catch (const po::error& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cerr << desc << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
         return 1;
    }


    const std::string& host = vm["host"].as<std::string>();
    const std::string& busid = vm["busid"].as<std::string>();
    const std::string& usbip_path_arg = vm["usbip-path"].as<std::string>();

    VERBOSE_LOG("Starting auto-attach with host=" << host << ", busid=" << busid);

    AttachState state;

    // Find the usbip executable path
    fs::path usbip_executable_path = find_usbip(usbip_path_arg);
    if (usbip_executable_path.empty()) {
        std::cerr << "Error: Could not find the 'usbip' executable." << std::endl;
        std::cerr << "Please ensure it is in the system PATH, next to this executable, or specify its location with --usbip-path." << std::endl;
        return 1;
    }
    state.usbip_path_arg = usbip_executable_path.string(); // Store the found path

    while (true) {
        VERBOSE_LOG("Checking if device is attached...");
        auto [attached, attach_check_error] = is_attached(host, busid);

         if (!attach_check_error.empty()) {
             // Error occurred during check (e.g., file not found, permission denied)
             VERBOSE_LOG("Error checking attachment status: " << attach_check_error);
             state.last_error = attach_check_error;
             report_attached(state, false); // Report as detached on error
         } else if (attached) {
            VERBOSE_LOG("Device is attached");
            state.last_error.clear(); // Clear error if successfully checked and found attached
            report_attached(state, true);
         } else {
             VERBOSE_LOG("Device is not attached");
             report_attached(state, false); // Report current detached state (error state managed within report_attached)

             // Always try to attach when the device is not found, like in the bash/rust script
             VERBOSE_LOG("Attempting to attach device");
             auto [attach_success, attach_cmd_error] = try_attach(host, busid, state.usbip_path_arg); // Pass the path
             if (attach_success) {
                 VERBOSE_LOG("Attachment command succeeded (may take time to reflect in status)");
                 state.last_error.clear(); // Clear error after successful attach command
                // Don't immediately report attached, wait for next is_attached check
             } else {
                 VERBOSE_LOG("Attachment command failed: " << attach_cmd_error);
                 state.last_error = attach_cmd_error; // Store the attach command error
                 report_attached(state, false); // Report detached state with the new error
            }
        }

        safe_sleep(CHECK_INTERVAL_SECONDS);
    }

    return 0; // Should not be reached
}

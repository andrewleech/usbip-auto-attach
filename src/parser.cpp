#include "parser.h"
#include <sstream>
#include <regex>
#include <iostream> // Required for std::cerr in case of regex error
#include <string>   // For std::string
#include <vector>   // For std::vector
#include <algorithm> // For std::find_if_not
#include <cctype>    // For ::isspace

// Helper function to trim leading/trailing whitespace (using standard functions)
std::string trim_whitespace(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : "";
}

bool parse_usbip_port(const std::string& output, const std::string& identifier, bool is_busid) {
    std::istringstream iss(output);
    std::string line;
    std::regex rgx;

    // Regex patterns looking for the attachment line:
    // Use R"(...) raw string literals to avoid double-escaping backslashes
    std::string pattern_str;
    if (is_busid) {
        // Match lines like "... -> usbip://<host>/<busid> ..."
        // Ensure busid is followed by end-of-string or whitespace
        pattern_str = R"(.* -> usbip://.+/)" + identifier + R"(($|\s+.*))";
    } else {
        // Match lines like "... -> usbip://<host>/devid=<devid> ..."
        // Ensure devid is followed by end-of-string or whitespace
        pattern_str = R"(.* -> usbip://.+/devid=)" + identifier + R"(($|\s+.*))";
    }

    try {
        rgx = std::regex(pattern_str);
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error compiling pattern '" << pattern_str << "': " << e.what() << std::endl;
        return false; // Cannot parse if regex is invalid
    }

    while (std::getline(iss, line)) {
        std::string trimmed_line = trim_whitespace(line);
        if (std::regex_match(trimmed_line, rgx)) {
            return true; // Found the line indicating the device is attached
        }
    }
    return false; // Did not find the attachment line
}


bool parse_usbip_list(const std::string& output, const std::string& busid) {
    std::istringstream iss(output);
    std::string line;
    // Look for a line starting with the busid followed by a colon, possibly indented.
    std::string prefix = busid + ":";
    while (std::getline(iss, line)) {
        std::string trimmed_line = trim_whitespace(line);
        // Use find to check if the trimmed line starts with the prefix
        if (trimmed_line.find(prefix) == 0) {
            return true;
        }
    }
    return false;
}

#ifndef PARSER_H
#define PARSER_H

#include <string>

/**
 * @brief Parses the output of `usbip port` to check if a device is attached.
 *
 * Checks for lines indicating attachment via busid (e.g., ".../X-Y") or
 * devid (e.g., ".../devid=...") based on the provided identifier.
 *
 * @param output The string output from the `usbip port` command.
 * @param identifier The busid (e.g., "1-2") or devid (e.g., "abcdef123...") to look for.
 * @param is_busid True if the identifier is a busid, false if it's a devid.
 * @return True if the specified device is found attached, false otherwise.
 */
bool parse_usbip_port(const std::string& output, const std::string& identifier, bool is_busid);

/**
 * @brief Parses the output of `usbip list -r <host>` to check if a device is available.
 *
 * Checks for lines starting with the exact busid followed by a colon (e.g., "X-Y:").
 *
 * @param output The string output from the `usbip list -r <host>` command.
 * @param busid The busid (e.g., "1-2") to look for.
 * @return True if the specified device busid is found, false otherwise.
 */
bool parse_usbip_list(const std::string& output, const std::string& busid);

#endif // PARSER_H

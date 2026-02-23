#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USBIP_DEFAULT_PORT 3240

/**
 * @brief Parses the output of `usbip port` to check if a device is attached.
 *
 * Checks for lines indicating attachment via busid (e.g., ".../X-Y") or
 * devid (e.g., ".../devid=...") based on the provided identifier.
 *
 * @param output The string output from the `usbip port` command.
 * @param identifier The busid (e.g., "1-2") or devid (e.g., "abcdef123...") to look for.
 * @param is_busid True if the identifier is a busid, false if it's a devid.
 * @return 1 if the specified device is found attached, 0 otherwise.
 */
int parse_usbip_port(const char* output, const char* identifier, int is_busid);

/**
 * @brief Parses the output of `usbip list -r <host>` to check if a device is available.
 *
 * Checks for lines starting with the exact busid followed by a colon (e.g., "X-Y:").
 *
 * @param output The string output from the `usbip list -r <host>` command.
 * @param busid The busid (e.g., "1-2") to look for.
 * @return 1 if the specified device busid is found, 0 otherwise.
 */
int parse_usbip_list(const char* output, const char* busid);

/**
 * @brief Parses a "host" or "host:port" string into separate components.
 *
 * On success, host_out and port_out are written. On error, neither output is
 * modified (port_out is set to 0 as an invalid sentinel on entry).
 *
 * IPv6 addresses are not supported. A bare IPv6 address such as "fe80::1"
 * will be split on its last colon and the result is undefined.
 *
 * @param input         Raw argument string (e.g., "192.168.1.1:63240").
 * @param host_out      Output buffer for the host portion.
 * @param host_out_size Size of host_out. Must be > 0.
 * @param port_out      Set to parsed port on success, or USBIP_DEFAULT_PORT if
 *                      no port component is present. Set to 0 on error.
 * @return  0 on success.
 *         -1 if port is out of range [1, 65535].
 *         -2 if port token is non-numeric.
 *         -3 if host portion is empty or host_out_size is 0.
 */
int parse_host_port(const char* input, char* host_out, size_t host_out_size, int* port_out);

#ifdef __cplusplus
}
#endif

#endif // PARSER_H

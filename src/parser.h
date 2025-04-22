#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // PARSER_H

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper function to trim leading whitespace
static const char* trim_leading(const char* str) {
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

// Helper function to check if a string starts with a prefix
static int starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int parse_usbip_port(const char* output, const char* identifier, int is_busid) {
    const char* line_start = output;
    const char* line_end;
    char line_buffer[512]; // Buffer for storing the current line
    size_t line_len;
    
    // Instead of regex, we'll construct search patterns
    char pattern_buffer[256];
    
    if (is_busid) {
        // Looking for: "-> usbip://<host>/<busid>"
        snprintf(pattern_buffer, sizeof(pattern_buffer), "-> usbip://%s", identifier);
    } else {
        // Looking for: "-> usbip://<host>/devid=<devid>"
        snprintf(pattern_buffer, sizeof(pattern_buffer), "-> usbip://");
    }
    
    while (line_start && *line_start) {
        // Find end of line
        line_end = strchr(line_start, '\n');
        if (!line_end) {
            // Last line without newline
            line_len = strlen(line_start);
            line_end = line_start + line_len;
        } else {
            line_len = line_end - line_start;
        }
        
        // Copy line to buffer and null-terminate it
        if (line_len < sizeof(line_buffer) - 1) {
            memcpy(line_buffer, line_start, line_len);
            line_buffer[line_len] = '\0';
            
            // Trim leading whitespace
            const char* trimmed_line = trim_leading(line_buffer);
            
            if (is_busid) {
                // For busid, check if line contains "-> usbip://" followed by identifier
                const char* usbip_pos = strstr(trimmed_line, "-> usbip://");
                if (usbip_pos) {
                    const char* path_pos = strchr(usbip_pos + 11, '/'); // 11 is length of "-> usbip://"
                    if (path_pos) {
                        // Extract the path part after the slash
                        path_pos++; // Move past the slash
                        
                        // Check if path starts with identifier and is followed by end or whitespace
                        size_t id_len = strlen(identifier);
                        if (strncmp(path_pos, identifier, id_len) == 0 && 
                            (path_pos[id_len] == '\0' || isspace((unsigned char)path_pos[id_len]) || 
                             path_pos[id_len] == '?' || path_pos[id_len] == ' ')) {
                            return 1; // Found match
                        }
                    }
                }
            } else {
                // For devid, check if line contains "devid=<identifier>"
                char devid_pattern[256];
                snprintf(devid_pattern, sizeof(devid_pattern), "devid=%s", identifier);
                if (strstr(trimmed_line, devid_pattern)) {
                    return 1; // Found match
                }
            }
        }
        
        // Move to next line
        line_start = line_end;
        if (line_start && *line_start == '\n') {
            line_start++; // Skip newline
        }
    }
    
    return 0; // No match found
}

int parse_usbip_list(const char* output, const char* busid) {
    const char* line_start = output;
    const char* line_end;
    char line_buffer[512]; // Buffer for storing the current line
    size_t line_len;
    
    // Prepare the prefix to search for (busid:)
    char prefix_buffer[256];
    snprintf(prefix_buffer, sizeof(prefix_buffer), "%s:", busid);
    
    while (line_start && *line_start) {
        // Find end of line
        line_end = strchr(line_start, '\n');
        if (!line_end) {
            // Last line without newline
            line_len = strlen(line_start);
            line_end = line_start + line_len;
        } else {
            line_len = line_end - line_start;
        }
        
        // Copy line to buffer and null-terminate it
        if (line_len < sizeof(line_buffer) - 1) {
            memcpy(line_buffer, line_start, line_len);
            line_buffer[line_len] = '\0';
            
            // Trim leading whitespace
            const char* trimmed_line = trim_leading(line_buffer);
            
            // Check if line starts with the busid:
            if (starts_with(trimmed_line, prefix_buffer)) {
                return 1; // Found match
            }
        }
        
        // Move to next line
        line_start = line_end;
        if (line_start && *line_start == '\n') {
            line_start++; // Skip newline
        }
    }
    
    return 0; // No match found
}

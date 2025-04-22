#include "../src/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper macro for assertions with messages */
#define ASSERT_MSG(condition, message) \
    if (!(condition)) { \
        fprintf(stderr, "Assertion failed: (" #condition "), %s\n", message); \
        exit(1); /* Exit with failure */ \
    }

void test_parse_usbip_port() {
    printf("Running test_parse_usbip_port...\n");
    const char* attached_output_busid = 
        "Imported USB devices\n"
        "====================\n"
        "Port 00: <Port in Use> at Full Speed(12Mbps)\n"
        "       unknown vendor : unknown product (1234:5678)\n"
        "       1-1 -> usbip://192.168.1.1:3240/7-4\n"
        "           -> remote bus/dev 007/004\n"
        "Port 01: <Port in Use> at High Speed(480Mbps)\n"
        "       Other Vendor : Other Product (aaaa:bbbb)\n"
        "        2-2 -> usbip://192.168.1.1:3240/8-1 bus/dev 008/002\n";
    
    const char* attached_output_devid = 
        "Imported USB devices\n"
        "====================\n"
        "Port 01: <Port in Use> at High Speed(480Mbps)\n"
        "       Example Corp : Example Device (abcd:ef01)\n"
        "       3-2 -> usbip://10.0.0.5:3240/devid=0123456789abcdef\n"
        "           -> remote bus/dev 001/002\n"
        "Port 02: <Port in Use> at Super Speed(5Gbps)\n"
        "        Another Corp : Another Device (beef:cafe)\n"
        "         4-1 -> usbip://10.0.0.5:3240/devid=fedcba9876543210 bus/dev 002/003\n";
    
    const char* not_attached_output = 
        "Imported USB devices\n"
        "====================\n";
    
    const char* not_attached_output_with_error = 
        "Imported USB devices\n"
        "====================\n"
        "usbip: error: failed to open /usr/share/hwdata//usb.ids\n";

    /* Test busid attach */
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "7-4", 1) == 1, "BusID 7-4 should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "8-1", 1) == 1, "BusID 8-1 should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "1-1", 1) == 0, "BusID 1-1 should NOT be attached (it's the local port)");
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "9-9", 1) == 0, "BusID 9-9 should NOT be attached (doesn't exist)");
    ASSERT_MSG(parse_usbip_port(not_attached_output, "7-4", 1) == 0, "BusID 7-4 should NOT be attached (empty output)");
    ASSERT_MSG(parse_usbip_port(not_attached_output_with_error, "7-4", 1) == 0, "BusID 7-4 should NOT be attached (error output)");

    /* Test devid attach */
    ASSERT_MSG(parse_usbip_port(attached_output_devid, "0123456789abcdef", 0) == 1, "DevID ...abcdef should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_devid, "fedcba9876543210", 0) == 1, "DevID ...3210 should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_devid, "deadbeefdeadbeef", 0) == 0, "DevID deadbeef... should NOT be attached (doesn't exist)");
    ASSERT_MSG(parse_usbip_port(not_attached_output, "0123456789abcdef", 0) == 0, "DevID ...abcdef should NOT be attached (empty output)");
    ASSERT_MSG(parse_usbip_port(not_attached_output_with_error, "0123456789abcdef", 0) == 0, "DevID ...abcdef should NOT be attached (error output)");

    printf("test_parse_usbip_port PASSED\n");
}

void test_parse_usbip_list() {
    printf("Running test_parse_usbip_list...\n");
    
    const char* available_output = 
        "Exportable USB devices\n"
        "======================\n"
        " - 127.0.0.1\n"
        "        7-4: unknown vendor : unknown product (2e8a:000f)\n"
        "           : USB\\VID_2E8A&PID_000F\\D83ACDDEF8D410EB\n"
        "           : (Defined at Interface level) (00/00/00)\n"
        "        1-2: Some other device (1111:2222)\n"
        "           : ...\n"
        "usbip: error: failed to open /usr/share/hwdata//usb.ids\n";
    
    const char* not_available_output = 
        "Exportable USB devices\n"
        "======================\n"
        " - 127.0.0.1\n"
        "        1-2: Some other device (1111:2222)\n"
        "           : ...\n"
        "usbip: error: failed to open /usr/share/hwdata//usb.ids\n";
    
    const char* empty_output = 
        "Exportable USB devices\n"
        "======================\n"
        " - 127.0.0.1\n"
        "usbip: error: failed to open /usr/share/hwdata//usb.ids\n";
    
    const char* empty_output_no_host = 
        "Exportable USB devices\n"
        "======================\n"
        "usbip: error: failed to open /usr/share/hwdata//usb.ids\n";

    ASSERT_MSG(parse_usbip_list(available_output, "7-4") == 1, "BusID 7-4 should be available");
    ASSERT_MSG(parse_usbip_list(available_output, "1-2") == 1, "BusID 1-2 should be available");
    ASSERT_MSG(parse_usbip_list(available_output, "9-9") == 0, "BusID 9-9 should NOT be available");

    ASSERT_MSG(parse_usbip_list(not_available_output, "7-4") == 0, "BusID 7-4 should NOT be available");
    ASSERT_MSG(parse_usbip_list(not_available_output, "1-2") == 1, "BusID 1-2 should be available (other device)");

    ASSERT_MSG(parse_usbip_list(empty_output, "7-4") == 0, "BusID 7-4 should NOT be available (empty list)");
    ASSERT_MSG(parse_usbip_list(empty_output_no_host, "7-4") == 0, "BusID 7-4 should NOT be available (no host output)");

    printf("test_parse_usbip_list PASSED\n");
}

int main() {
    test_parse_usbip_port();
    test_parse_usbip_list();
    printf("All tests PASSED\n");
    return 0;
}

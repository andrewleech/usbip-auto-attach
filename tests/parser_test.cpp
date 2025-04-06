#include "../src/parser.h" // Assuming parser.h is in src
#include <cassert>
#include <string>
#include <iostream>
#include <stdexcept> // For runtime_error

// Helper macro for assertions with messages
#define ASSERT_MSG(condition, message) \
    if (!(condition)) { \
        std::cerr << "Assertion failed: (" #condition "), " << message << std::endl; \
        throw std::runtime_error("Assertion failed: " + std::string(message)); \
    }

void test_parse_usbip_port() {
    std::cout << "Running test_parse_usbip_port..." << std::endl;
    std::string attached_output_busid = R"(Imported USB devices
====================
Port 00: <Port in Use> at Full Speed(12Mbps)
       unknown vendor : unknown product (1234:5678)
       1-1 -> usbip://192.168.1.1:3240/7-4
           -> remote bus/dev 007/004
Port 01: <Port in Use> at High Speed(480Mbps)
       Other Vendor : Other Product (aaaa:bbbb)
        2-2 -> usbip://192.168.1.1:3240/8-1 bus/dev 008/002
)";
    std::string attached_output_devid = R"(Imported USB devices
====================
Port 01: <Port in Use> at High Speed(480Mbps)
       Example Corp : Example Device (abcd:ef01)
       3-2 -> usbip://10.0.0.5:3240/devid=0123456789abcdef
           -> remote bus/dev 001/002
Port 02: <Port in Use> at Super Speed(5Gbps)
        Another Corp : Another Device (beef:cafe)
         4-1 -> usbip://10.0.0.5:3240/devid=fedcba9876543210 bus/dev 002/003
)";
    std::string not_attached_output = R"(Imported USB devices
====================
)";
    std::string not_attached_output_with_error = R"(Imported USB devices
====================
usbip: error: failed to open /usr/share/hwdata//usb.ids
)";

    // Test busid attach
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "7-4", true) == true, "BusID 7-4 should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "8-1", true) == true, "BusID 8-1 should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "1-1", true) == false, "BusID 1-1 should NOT be attached (it's the local port)");
    ASSERT_MSG(parse_usbip_port(attached_output_busid, "9-9", true) == false, "BusID 9-9 should NOT be attached (doesn't exist)");
    ASSERT_MSG(parse_usbip_port(not_attached_output, "7-4", true) == false, "BusID 7-4 should NOT be attached (empty output)");
    ASSERT_MSG(parse_usbip_port(not_attached_output_with_error, "7-4", true) == false, "BusID 7-4 should NOT be attached (error output)");

    // Test devid attach
    ASSERT_MSG(parse_usbip_port(attached_output_devid, "0123456789abcdef", false) == true, "DevID ...abcdef should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_devid, "fedcba9876543210", false) == true, "DevID ...3210 should be attached");
    ASSERT_MSG(parse_usbip_port(attached_output_devid, "deadbeefdeadbeef", false) == false, "DevID deadbeef... should NOT be attached (doesn't exist)");
    ASSERT_MSG(parse_usbip_port(not_attached_output, "0123456789abcdef", false) == false, "DevID ...abcdef should NOT be attached (empty output)");
    ASSERT_MSG(parse_usbip_port(not_attached_output_with_error, "0123456789abcdef", false) == false, "DevID ...abcdef should NOT be attached (error output)");


    std::cout << "test_parse_usbip_port PASSED" << std::endl;
}

void test_parse_usbip_list() {
    std::cout << "Running test_parse_usbip_list..." << std::endl;
    std::string available_output = R"(Exportable USB devices
======================
 - 127.0.0.1
        7-4: unknown vendor : unknown product (2e8a:000f)
           : USB\\VID_2E8A&PID_000F\\D83ACDDEF8D410EB
           : (Defined at Interface level) (00/00/00)
        1-2: Some other device (1111:2222)
           : ...
usbip: error: failed to open /usr/share/hwdata//usb.ids
)";
    std::string not_available_output = R"(Exportable USB devices
======================
 - 127.0.0.1
        1-2: Some other device (1111:2222)
           : ...
usbip: error: failed to open /usr/share/hwdata//usb.ids
)";
     std::string empty_output = R"(Exportable USB devices
======================
 - 127.0.0.1
usbip: error: failed to open /usr/share/hwdata//usb.ids
)";
    std::string empty_output_no_host = R"(Exportable USB devices
======================
usbip: error: failed to open /usr/share/hwdata//usb.ids
)";


    ASSERT_MSG(parse_usbip_list(available_output, "7-4") == true, "BusID 7-4 should be available");
    ASSERT_MSG(parse_usbip_list(available_output, "1-2") == true, "BusID 1-2 should be available");
    ASSERT_MSG(parse_usbip_list(available_output, "9-9") == false, "BusID 9-9 should NOT be available");

    ASSERT_MSG(parse_usbip_list(not_available_output, "7-4") == false, "BusID 7-4 should NOT be available");
    ASSERT_MSG(parse_usbip_list(not_available_output, "1-2") == true, "BusID 1-2 should be available (other device)");

    ASSERT_MSG(parse_usbip_list(empty_output, "7-4") == false, "BusID 7-4 should NOT be available (empty list)");
    ASSERT_MSG(parse_usbip_list(empty_output_no_host, "7-4") == false, "BusID 7-4 should NOT be available (no host output)");


    std::cout << "test_parse_usbip_list PASSED" << std::endl;
}


int main() {
    try {
        test_parse_usbip_port();
        test_parse_usbip_list();
        std::cout << "All tests PASSED" << std::endl;
        return 0;
    } catch (const std::runtime_error& e) {
        std::cerr << "A test failed: " << e.what() << std::endl;
        return 1; // Indicate failure
    } catch (...) {
        std::cerr << "An unknown error occurred during testing." << std::endl;
        return 1; // Indicate failure
    }
}

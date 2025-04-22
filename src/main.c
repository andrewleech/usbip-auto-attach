#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>

/* Manual declaration of functions that might be missing from headers */
extern FILE *popen(const char *command, const char *type);
extern int pclose(FILE *stream);
extern char *strdup(const char *s);

/* Let's use a simpler approach and change the code to use signal() instead of sigaction() */

#include "version.h"
#include "parser.h" 

/* Max command length */
#define MAX_CMD_LEN 4096
/* Max command output buffer */
#define MAX_OUTPUT_LEN 16384  
/* Max path length */
#define MAX_PATH_LEN 256
/* Buffer size for reading command output */
#define READ_BUFFER_SIZE 128

/* Struct to hold command result */
typedef struct {
    char* output;        /* Command output (stdout+stderr) */
    int exit_code;       /* Command exit code */
    int success;         /* 1 if popen/pclose succeeded and exit code was 0 */
} CommandResult;

/* Args struct to store command line arguments */
typedef struct {
    char host_ip[MAX_PATH_LEN];
    char busid[MAX_PATH_LEN];     /* Bus ID if specified */
    char device[MAX_PATH_LEN];    /* Device ID if specified */
    char usbip_path[MAX_PATH_LEN];
    int has_busid;               /* 1 if busid is specified */
    int has_device;              /* 1 if device is specified */
    int verbose;                 /* 1 if verbose mode enabled */
    int show_help;               /* 1 if help should be shown */
    int show_version;            /* 1 if version should be shown */
} Args;

/* Signal handler for graceful shutdown */
volatile sig_atomic_t keep_running = 1;

/* Function to trim leading/trailing whitespace */
char* trim(char* str) {
    char* end;

    /* Trim leading space */
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  /* All spaces? */
        return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    /* Write new null terminator */
    *(end+1) = 0;

    return str;
}

/* Quote a shell argument, handling single quotes */
char* quote_arg(const char* arg, char* buffer, size_t buffer_size) {
    size_t pos = 0;
    size_t i;
    
    /* Start with a single quote */
    if (pos < buffer_size - 1) buffer[pos++] = '\'';
    
    /* Process each character in the argument */
    for (i = 0; arg[i] != '\0' && pos < buffer_size - 4; i++) {
        if (arg[i] == '\'') {
            /* Close the quote, insert escaped quote, reopen quote */
            buffer[pos++] = '\'';
            buffer[pos++] = '\\';
            buffer[pos++] = '\'';
            buffer[pos++] = '\'';
        } else {
            buffer[pos++] = arg[i];
        }
    }
    
    /* End with a single quote */
    if (pos < buffer_size - 1) buffer[pos++] = '\'';
    
    /* Null terminate */
    buffer[pos] = '\0';
    
    return buffer;
}

/* Function to run a command and capture its output and exit code */
CommandResult run_command(const char** args, int arg_count, int verbose) {
    CommandResult cmd_result;
    char command_str[MAX_CMD_LEN] = {0};
    char quoted_arg[MAX_PATH_LEN * 2];
    char buffer[READ_BUFFER_SIZE];
    size_t current_len = 0;
    FILE* pipe;
    int i, status;
    
    /* Initialize result */
    cmd_result.output = malloc(MAX_OUTPUT_LEN);
    if (!cmd_result.output) {
        fprintf(stderr, "Error: Failed to allocate memory for command output\n");
        exit(1);
    }
    cmd_result.output[0] = '\0';
    cmd_result.exit_code = -1;
    cmd_result.success = 0;
    
    /* Log the command if verbose */
    if (verbose) {
        fprintf(stderr, "Running command:");
        for (i = 0; i < arg_count; i++) {
            fprintf(stderr, " %s", args[i]);
        }
        fprintf(stderr, "\n");
    }
    
    /* Build command string with proper quoting */
    for (i = 0; i < arg_count; i++) {
        quote_arg(args[i], quoted_arg, sizeof(quoted_arg));
        
        if (i > 0) {
            strcat(command_str, " ");
        }
        strcat(command_str, quoted_arg);
    }
    
    /* Redirect stderr to stdout */
    strcat(command_str, " 2>&1");
    
    /* Run the command */
    pipe = popen(command_str, "r");
    if (!pipe) {
        sprintf(cmd_result.output, "popen() failed!");
        return cmd_result;
    }
    
    /* Read command output */
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        
        /* Ensure we don't overflow the output buffer */
        if (current_len + len >= MAX_OUTPUT_LEN - 1) {
            len = MAX_OUTPUT_LEN - current_len - 1;
            if (len <= 0) break;
        }
        
        memcpy(cmd_result.output + current_len, buffer, len);
        current_len += len;
        cmd_result.output[current_len] = '\0';
    }
    
    /* Close the pipe and get exit status */
    status = pclose(pipe);
    if (status == -1) {
        sprintf(cmd_result.output, "pclose() failed!");
        return cmd_result;
    }
    
    /* Process exit status */
    if (WIFEXITED(status)) {
        cmd_result.exit_code = WEXITSTATUS(status);
        if (cmd_result.exit_code == 0) {
            cmd_result.success = 1;
        }
        if (cmd_result.exit_code != 0 && verbose) {
            fprintf(stderr, "Command exited with status %d\n", cmd_result.exit_code);
            fprintf(stderr, "Command output:\n%s\n", cmd_result.output);
        }
    } else if (WIFSIGNALED(status)) {
        cmd_result.exit_code = 128 + WTERMSIG(status); /* Mimic shell signal exit codes */
        if (verbose) {
            fprintf(stderr, "Command killed by signal %d\n", WTERMSIG(status));
            fprintf(stderr, "Command output:\n%s\n", cmd_result.output);
        }
    } else {
        if (verbose) {
            fprintf(stderr, "Command exited abnormally.\n");
            fprintf(stderr, "Command output:\n%s\n", cmd_result.output);
        }
    }
    
    /* Log snippet on success if verbose */
    if (verbose && cmd_result.exit_code == 0) {
        size_t len = strlen(cmd_result.output);
        fprintf(stderr, "Command output snippet:\n%.*s%s\n", 
                len > 200 ? 200 : (int)len, 
                cmd_result.output, 
                len > 200 ? "..." : "");
    }
    
    return cmd_result;
}

/* Function to attach the device using either busid or device ID */
int attach_device(const char* host_ip, const char* busid, const char* device, const char* usbip_path, int verbose) {
    const char* args[7]; /* Max command args */
    int arg_count = 0;
    CommandResult result;
    int is_busid = busid && *busid; /* 1 if busid is specified, 0 if device */
    const char* identifier = is_busid ? busid : device;
    
    /* Prepare command args */
    args[arg_count++] = usbip_path;
    args[arg_count++] = "attach";
    args[arg_count++] = "-r";
    args[arg_count++] = host_ip;
    
    if (is_busid) {
        args[arg_count++] = "-b";
        args[arg_count++] = identifier;
    } else {
        args[arg_count++] = "-d";
        args[arg_count++] = identifier;
    }
    
    args[arg_count] = NULL; /* Terminate the array */
    
    /* Run the command */
    result = run_command(args, arg_count, verbose);
    
    /* Check for specific vhci driver error */
    if (result.exit_code == 1 && strstr(result.output, "open vhci_driver") != NULL) {
        fprintf(stderr, "Error: Failed to open vhci_driver. VHCI kernel module may not be loaded.\n");
        fprintf(stderr, "Try running: sudo modprobe vhci-hcd\n");
        free(result.output);
        exit(2); /* Exit with specific code for this error */
    }
    
    if (!result.success && verbose) {
        fprintf(stderr, "Attach command failed with exit code %d. Output:\n%s\n", 
                result.exit_code, result.output);
    }
    
    /* Re-check attachment status only if we used busid (more reliable) */
    if (is_busid) {
        /* Wait 2 seconds for attach to complete */
        sleep(2);
        
        /* Check port status */
        const char* port_args[2] = {usbip_path, "port"};
        CommandResult port_result = run_command(port_args, 2, verbose);
        int attached = parse_usbip_port(port_result.output, identifier, 1);
        free(port_result.output);
        free(result.output);
        return attached;
    } else {
        /* For device ID attach, rely on command success */
        if (verbose) {
            fprintf(stderr, "Attach command with device ID completed. Cannot reliably verify port status.\n");
        }
        int success = result.success;
        free(result.output);
        return success;
    }
}

/* Signal handler function */
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        keep_running = 0;
    }
}

/* Find usbip executable in PATH */
int find_usbip(const char* user_path, char* found_path, size_t path_size) {
    char path_buffer[MAX_PATH_LEN];
    const char* path_env;
    char* path_copy;
    char* token;
    
    /* Initialize output path */
    if (path_size > 0) {
        found_path[0] = '\0';
    }
    
    /* Check user-provided path first */
    if (user_path && *user_path) {
        if (access(user_path, X_OK) == 0) {
            strncpy(found_path, user_path, path_size - 1);
            found_path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    /* Get PATH environment variable */
    path_env = getenv("PATH");
    if (!path_env) {
        return 0;
    }
    
    /* Make a copy of PATH to tokenize */
    path_copy = strdup(path_env);
    if (!path_copy) {
        return 0;
    }
    
    /* Search each directory in PATH */
    token = strtok(path_copy, ":");
    while (token) {
        if (*token) { /* Skip empty path components */
            snprintf(path_buffer, sizeof(path_buffer), "%s/usbip", token);
            if (access(path_buffer, X_OK) == 0) {
                strncpy(found_path, path_buffer, path_size - 1);
                found_path[path_size - 1] = '\0';
                free(path_copy);
                return 1;
            }
        }
        token = strtok(NULL, ":");
    }
    
    free(path_copy);
    return 0;
}

/* Parse command line arguments */
void parse_args(int argc, char* argv[], Args* args) {
    int i;
    int positional_count = 0;
    
    /* Initialize args with defaults */
    memset(args, 0, sizeof(Args));
    
    /* Parse args */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            args->verbose = 1;
        } else if (strcmp(argv[i], "--usbip-path") == 0) {
            if (i + 1 < argc) {
                strncpy(args->usbip_path, argv[++i], sizeof(args->usbip_path) - 1);
            } else {
                fprintf(stderr, "Error: --usbip-path requires an argument.\n");
                args->show_help = 1;
                return;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->show_help = 1;
            return;
        } else if (strcmp(argv[i], "--version") == 0) {
            args->show_version = 1;
            return;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--busid") == 0) {
            if (i + 1 < argc) {
                strncpy(args->busid, argv[++i], sizeof(args->busid) - 1);
                args->has_busid = 1;
            } else {
                fprintf(stderr, "Error: --busid requires an argument.\n");
                args->show_help = 1;
                return;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) {
                strncpy(args->device, argv[++i], sizeof(args->device) - 1);
                args->has_device = 1;
            } else {
                fprintf(stderr, "Error: --device requires an argument.\n");
                args->show_help = 1;
                return;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            args->show_help = 1;
            return;
        } else {
            /* Positional argument (host_ip) */
            if (positional_count == 0) {
                strncpy(args->host_ip, argv[i], sizeof(args->host_ip) - 1);
                positional_count++;
            } else {
                fprintf(stderr, "Error: Unexpected positional argument: %s\n", argv[i]);
                args->show_help = 1;
                return;
            }
        }
    }
    
    /* Validate arguments */
    if (!args->show_help && !args->show_version) {
        if (positional_count != 1) {
            fprintf(stderr, "Error: Requires exactly one positional argument: <host_ip>\n");
            args->show_help = 1;
            return;
        }
        
        if (!args->has_busid && !args->has_device) {
            fprintf(stderr, "Error: Either --busid or --device must be specified.\n");
            args->show_help = 1;
            return;
        }
        
        if (args->has_busid && args->has_device) {
            fprintf(stderr, "Error: --busid and --device are mutually exclusive.\n");
            args->show_help = 1;
            return;
        }
    }
}

/* Print usage information */
void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s <host_ip> {-b <busid> | -d <devid>} [--usbip-path <path>] [-v|--verbose] [--version] [-h|--help]\n", prog_name);
    fprintf(stderr, "  <host_ip>           IP address of the remote USBIP host.\n");
    fprintf(stderr, "  -b, --busid <busid> Bus ID of the USB device to monitor and attach (e.g., 1-2). Mutually exclusive with -d.\n");
    fprintf(stderr, "  -d, --device <devid> Device ID (UDC ID) on the remote host to attach. Mutually exclusive with -b.\n");
    fprintf(stderr, "                      Note: Availability/attachment status checks are less reliable with -d.\n");
    fprintf(stderr, "  --usbip-path <path> (Optional) Full path to the local usbip executable.\n");
    fprintf(stderr, "                      Searches PATH if not provided.\n");
    fprintf(stderr, "  -v, --verbose       Enable detailed logging to stderr.\n");
    fprintf(stderr, "  --version           Print version information and exit.\n");
    fprintf(stderr, "  -h, --help          Show this help message and exit.\n");
}

/* Device status constants */
#define STATUS_UNKNOWN       0
#define STATUS_ATTACHED      1
#define STATUS_NOT_ATTACHED  2
#define STATUS_NOT_AVAILABLE 3
#define STATUS_AVAILABLE     4
#define STATUS_ATTACH_FAIL   5
#define STATUS_ATTACH_SUCCESS 6

/* Main function */
int main(int argc, char* argv[]) {
    Args args;
    char usbip_exec_path[MAX_PATH_LEN] = {0};
    int last_status = STATUS_UNKNOWN;
    char timestamp[32] = {0};
    time_t now;
    struct tm *tm_info;
    
    /* Parse command-line arguments */
    parse_args(argc, argv, &args);
    
    /* Handle help/version requests */
    if (args.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (args.show_version) {
        printf("%s version: %s\n", argv[0], AUTO_ATTACH_VERSION);
        return 0;
    }
    
    /* Validate arguments */
    if (args.host_ip[0] == '\0' || (!args.has_busid && !args.has_device)) {
        fprintf(stderr, "Internal error: Missing host_ip or busid/device after parsing.\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Find usbip executable */
    if (!find_usbip(args.usbip_path, usbip_exec_path, sizeof(usbip_exec_path))) {
        fprintf(stderr, "Error: Could not find usbip executable. Please specify with --usbip-path or ensure it's in PATH.\n");
        return 1;
    }
    
    /* Print initial status information */
    if (args.has_busid) {
        fprintf(stderr, "Monitoring host %s for BUSID: %s\n", args.host_ip, args.busid);
    } else {
        fprintf(stderr, "Monitoring host %s for Device ID: %s\n", args.host_ip, args.device);
    }
    
    if (args.verbose) {
        fprintf(stderr, "Using usbip executable: %s\n", usbip_exec_path);
        fprintf(stderr, "Running in verbose mode\n");
    }
    
    /* Setup signal handling using standard signal() */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Main loop */
    while (keep_running) {
        int current_status;
        int currently_attached = 0;
        int available = 0;
        const char* identifier = args.has_busid ? args.busid : args.device; /* Should always have one */
        int check_by_busid = args.has_busid;
        int status_changed = 0;
        
        /* Generate timestamp for logs */
        now = time(NULL);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        
        /* Check device status by running `usbip port` */
        {
            const char* port_args[2] = {usbip_exec_path, "port"};
            CommandResult result = run_command(port_args, 2, args.verbose);
            
            if (result.success) {
                currently_attached = parse_usbip_port(result.output, identifier, check_by_busid);
            } else if (args.verbose) {
                fprintf(stderr, "%s Error checking device attachment (running usbip port): Command failed\n", timestamp);
            }
            
            free(result.output);
        }
        
        if (currently_attached) {
            current_status = STATUS_ATTACHED;
            
            if (args.verbose) {
                fprintf(stderr, "%s Device %s is attached.\n", timestamp, identifier);
            }
        } else {
            /* First check - Not attached */
            current_status = STATUS_NOT_ATTACHED;
            
            /* Log when a device transitions from attached to detached */
            if (last_status == STATUS_ATTACHED) {
                fprintf(stderr, "%s Device %s is now detached.\n", timestamp, identifier);
                status_changed = 1;
            } else if (args.verbose) {
                fprintf(stderr, "%s Device %s not attached.\n", timestamp, identifier);
            }
            
            /* Only check availability if using BUSID, as 'usbip list' uses BUSID */
            if (check_by_busid) {
                if (args.verbose) {
                    fprintf(stderr, "%s Checking availability for BUSID %s...\n", timestamp, args.busid);
                }
                
                const char* list_args[4] = {usbip_exec_path, "list", "-r", args.host_ip};
                CommandResult list_result = run_command(list_args, 4, args.verbose);
                
                available = parse_usbip_list(list_result.output, args.busid);
                free(list_result.output);
            } else {
                /* We assume device is potentially available if specified by ID */
                available = 1;
                if (args.verbose) {
                    fprintf(stderr, "%s Availability check skipped when using Device ID.\n", timestamp);
                }
            }
            
            if (available) {
                current_status = STATUS_AVAILABLE;
                
                /* Always show when a device becomes available */
                if (last_status != STATUS_AVAILABLE) {
                    fprintf(stderr, "%s Device %s is available. Attempting to attach...\n", timestamp, identifier);
                    status_changed = 1;
                } else if (args.verbose) {
                    fprintf(stderr, "%s Device %s is available. Attempting to attach...\n", timestamp, identifier);
                }
                
                if (attach_device(args.host_ip, args.has_busid ? args.busid : NULL, 
                                 args.has_device ? args.device : NULL, 
                                 usbip_exec_path, args.verbose)) {
                    current_status = STATUS_ATTACH_SUCCESS;
                    fprintf(stderr, "%s Attach command for device %s succeeded.\n", timestamp, identifier);
                } else {
                    /* Don't print generic failure message if attach_device exited due to vhci error */
                    if (errno != ECANCELED) {
                        current_status = STATUS_ATTACH_FAIL;
                        fprintf(stderr, "%s Failed to attach device %s\n", timestamp, identifier);
                    }
                }
            } else {
                /* This block only reached if check_by_busid was true and device wasn't available */
                current_status = STATUS_NOT_AVAILABLE;
                
                /* Always show when a device is not available (status change) */
                if (last_status != STATUS_NOT_AVAILABLE) {
                    fprintf(stderr, "%s Device BUSID %s is not available on host %s\n", 
                            timestamp, identifier, args.host_ip);
                    status_changed = 1;
                } else if (args.verbose) {
                    fprintf(stderr, "%s Device BUSID %s is not available on host %s\n", 
                            timestamp, identifier, args.host_ip);
                }
            }
        }
        
        /* Remember the last status */
        if (current_status != last_status) {
            if (!status_changed && current_status == STATUS_ATTACHED) {
                /* Print attachment status change if not already printed */
                fprintf(stderr, "%s Device %s is now attached.\n", timestamp, identifier);
            }
            
            /* Update last status */
            last_status = current_status;
        }
        
        /* Wait before checking again, checking keep_running more frequently */
        for (int i = 0; i < 5 && keep_running; i++) {
            sleep(1);
        }
    }
    
    fprintf(stderr, "Exiting due to signal.\n");
    return 0;
}

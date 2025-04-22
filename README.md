# usbip-auto-attach

A Linux utility designed to automatically attach a specified USB device from a remote USBIP host (running `usbipd` or similar) to the local machine using the `usbip` command.

When a specific USB device is detached from the host, this tool monitors for its reappearance and automatically attaches it back to the local machine. This is useful for devices that need to be persistently available locally via USBIP, even if they are temporarily disconnected or the connection is reset.

It produces small, static MUSL-linked C executables for maximum portability across Linux distributions.

**Pre-compiled static binaries for `linux/amd64` and `linux/arm64` are available from the [GitHub Releases page](https://github.com/corona10/usbip-auto-attach/releases).**

## Usage

Download the appropriate binary for your architecture (`usbip-auto-attach-amd64` or `usbip-auto-attach-arm64`) from the Releases page and make it executable (`chmod +x <binary_name>`).

The command-line arguments are as follows:

```
Usage: ./usbip-auto-attach <host_ip> {-b <busid> | -d <devid>} [--usbip-path <path>] [-v|--verbose] [--version] [-h|--help]
  <host_ip>           IP address of the remote USBIP host.
  -b, --busid <busid> Bus ID of the USB device to monitor and attach (e.g., 1-2). Mutually exclusive with -d.
  -d, --device <devid> Device ID (UDC ID) on the remote host to attach. Mutually exclusive with -b.
                      Note: Availability/attachment status checks are less reliable with -d.
  --usbip-path <path> (Optional) Full path to the local usbip executable.
                      Searches PATH if not provided.
  -v, --verbose       Enable detailed logging to stderr.
  --version           Print version information and exit.
  -h, --help          Show this help message and exit.
```

*   `<host_ip>`: IP address of the remote host sharing the USB device.
*   `-b <busid>` or `-d <devid>`: You must specify *one* of these options to identify the target device.
    *   `busid`: The bus ID (e.g., `1-2`) is generally preferred as status checking is more reliable. Find this using `usbip list -r <host_ip>` on the local machine *before* the device is attached.
    *   `devid`: The device ID (UDC ID) on the remote host (e.g., `foo_udc.0`). Availability checking is less reliable with this option.
*   `--usbip-path`: (Optional) Specify the full path to the `usbip` executable on the local machine if it's not in the system `PATH`.
*   `-v`, `--verbose`: Enable detailed logging.
*   `--version`: Print version information.
*   `-h`, `--help`: Show usage information.

**Example:**

Assuming the pre-compiled amd64 binary is in the current directory, the remote host IP is `192.168.1.100`, and the device you want to keep attached has bus ID `1-2`:

```bash
./usbip-auto-attach-amd64 192.168.1.100 -b 1-2 --verbose
```

This command will continuously check if device `1-2` is attached from host `192.168.1.100`. If it's not attached but is available (listed), it will attempt to attach it using the local `usbip` command.

## Building (Recommended: Using Docker)

If you prefer to build from source, the easiest way to build the static MUSL executables for `linux/amd64` and `linux/arm64` is using Docker. This ensures a consistent build environment with all necessary cross-compilers and tools.

**Prerequisites:**

*   Docker installed and running.

**Steps:**

1.  **Navigate to the directory:**
    ```bash
    cd usbip-auto-attach
    ```

2.  **Build the Docker image:**
    ```bash
    docker build -t usbip-auto-attach-builder .
    ```

3.  **Compile the executables using the Docker image:**
    This command runs `make all` inside a container based on the image you just built. It mounts the current host directory (containing source and Makefile) to the same path inside the container and sets it as the working directory.

    ```bash
    # On Linux/macOS
    docker run --rm --user "$(id -u):$(id -g)" -v "$(pwd):$(pwd)" -w "$(pwd)" usbip-auto-attach-builder make all

    # On Windows (Command Prompt/PowerShell) - Use %cd% for current directory
    # docker run --rm -v "%cd%:%cd%" -w "%cd%" usbip-auto-attach-builder make all
    ```
    *   `--rm`: Removes the container after it exits.
    *   `--user "$(id -u):$(id -g)"`: Runs the container as the current user/group to maintain file permissions.
    *   `-v "$(pwd):$(pwd)"`: Mounts the current host directory to the same path inside the container.
    *   `-w "$(pwd)"`: Sets the working directory inside the container.
    *   `usbip-auto-attach-builder`: The name of the Docker image built in the previous step.
    *   `make all`: Runs the default make target. The Makefile outputs to `./build` relative to the working directory.

    After this command completes, you will find the static executables:
    *   `./build/x64/usbip-auto-attach` (for amd64)
    *   `./build/arm64/usbip-auto-attach` (for arm64)

4.  **Run unit tests using the Docker image:**
    This command runs `make test` inside a container based on the image you just built. It mounts the current host directory (containing source and Makefile) to the same path inside the container and sets it as the working directory.

    ```bash
    docker run --rm --user "$(id -u):$(id -g)" -v "$(pwd):$(pwd)" -w "$(pwd)" usbip-auto-attach-builder make test
    ```
    The results are printed to the console.
    
## Why Static Linking with MUSL?

This project aims to create truly portable static executables. This is achieved by linking against the [MUSL C library](https://musl.libc.org/) instead of the more common GNU C Library (glibc).

*   **Portability:** Standard static linking (`gcc -static`) usually links against glibc. While this includes many libraries, glibc itself relies on certain system services (like NSS for name lookups) that might differ between Linux distributions or even kernel versions. This can lead to runtime errors on systems different from the build system.
*   **MUSL Approach:** MUSL is designed from the ground up for static linking. It avoids dependencies on specific system services where possible, implementing functionality directly within the library. Linking against MUSL (`musl-gcc -static` or using a MUSL cross-compiler) produces a binary with fewer external runtime dependencies, making it much more likely to run correctly across a wide range of Linux systems without needing specific library versions installed on the target machine.
*   **Build Process:** The included `Dockerfile` sets up an environment with MUSL cross-compilers, ensuring the resulting `usbip-auto-attach` binaries are linked against MUSL for maximum portability.

## GitHub Actions

This repository includes GitHub Actions workflows (`.github/workflows/`) for automation:

*   **`build.yml`**: Builds the static executables for amd64 and arm64 on every push and pull request, uploading them as build artifacts.
*   **`release.yml`**: Builds the static executables and creates a GitHub Release (attaching the compiled binaries as assets and generating release notes) whenever a tag matching the pattern `v*.*.*` (e.g., `v1.0.0`) is pushed.

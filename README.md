# auto-attach-cpp

A C++ utility designed to automatically attach a specified USB device from a Windows host running `usbipd-win` to a WSL instance using the `usbip` command. When a specific USB device is detached from the Windows host, this tool monitors for its reappearance and automatically attaches it back to the WSL instance. This is useful for devices that need to be persistently available within WSL, even if they are temporarily disconnected or the connection is reset.

It uses Boost for command-line parsing and process management and produces static MUSL-linked executables for portability.

## Building (Recommended: Using Docker)

The easiest way to build the static MUSL executables for `linux/amd64` and `linux/arm64` is using Docker. This ensures a consistent build environment with all necessary cross-compilers and libraries.

**Prerequisites:**

*   Docker installed and running.

**Steps:**

1.  **Navigate to the directory:**
    ```bash
    cd auto-attach-cpp
    ```

2.  **Build the Docker image:**
    ```bash
    docker build -t auto-attach-builder .
    ```

3.  **Compile the executables using the Docker image:**
    This command runs `make all` inside a container based on the image you just built. It mounts the parent directory (`../`) of your current `auto-attach-cpp` directory into `/build_output` inside the container. The Makefile is configured to place the compiled binaries in `../x64/` and `../arm64/` relative to its own location, which corresponds to `/build_output/x64/` and `/build_output/arm64/` inside the container. This mount ensures the compiled files appear in the correct location on your host system.

    ```bash
    # On Linux/macOS
    docker run --rm -v "$(pwd):/app" -v "$(pwd)/..:/build_output" auto-attach-builder make -C /app all OUT_DIR=/build_output

    # On Windows (Command Prompt/PowerShell) - Use %cd% for current directory
    # docker run --rm -v "%cd%:/app" -v "%cd%\..:/build_output" auto-attach-builder make -C /app all OUT_DIR=/build_output
    ```
    *   `--rm`: Removes the container after it exits.
    *   `-v "$(pwd)/..:/build_output"` (or `"%cd%\..:/build_output"`): Mounts the parent directory of your project into `/build_output` in the container.
    *   `auto-attach-builder`: The name of the Docker image built in the previous step.
    *   `make -C /app all OUT_DIR=/build_output`: Runs make inside the container's `/app` directory (where the source code is) and overrides the `OUT_DIR` Makefile variable to point to the mounted volume, ensuring output goes to your host machine's parent directory.

    After this command completes, you will find the static executables:
    *   `../x64/auto-attach` (for amd64)
    *   `../arm64/auto-attach` (for arm64)

## Building (Manual - Requires MUSL Toolchain)

If you prefer not to use Docker, you can build manually, but you **must** have MUSL-based cross-compilers installed and available in your `PATH`.

**Prerequisites:**

*   `make`
*   `x86_64-linux-musl-g++` (MUSL C++ compiler for amd64)
*   `aarch64-linux-musl-g++` (MUSL C++ compiler for arm64)
*   Boost development libraries (program_options, system, process). The specific method to get MUSL-compatible static Boost libraries depends on your system setup (e.g., compiling Boost against MUSL yourself or finding pre-built packages).

**Steps:**

1.  **Navigate to the directory:**
    ```bash
    cd auto-attach-cpp
    ```
2.  **Ensure MUSL cross-compilers are in PATH.**
3.  **Build:**
    ```bash
    make all
    ```
    This uses the `Makefile` directly, which is configured to use the MUSL compilers (`x86_64-linux-musl-g++` and `aarch64-linux-musl-g++`). It will create the static executables in the parent directory:
    *   `../x64/auto-attach`
    *   `../arm64/auto-attach`

## Usage

The command-line arguments are straightforward:

```
<path_to_executable>/auto-attach <host_ip> <busid> [--usbip-path <path_to_usbip>] [-v|--verbose]
```

*   `<path_to_executable>`: Either `../x64/auto-attach` or `../arm64/auto-attach` depending on your architecture.
*   `<host_ip>`: IP address of the Windows host running `usbipd-win`.
*   `<busid>`: Bus ID of the USB device to monitor and attach (e.g., `1-2`). You can find this using `usbip list -r <host_ip>` *before* attaching the device for the first time.
*   `--usbip-path`: (Optional) Specify the full path to the `usbip` executable within WSL. If not provided, it searches the system `PATH` and then the directory containing the `auto-attach` executable.
*   `-v`, `--verbose`: Enable detailed logging to stderr, showing checks and command executions.

**Example:**

Assuming you are on an amd64 WSL instance and your Windows host IP is `172.20.0.1`, and the device you want to keep attached has bus ID `1-2`:

```bash
../x64/auto-attach 172.20.0.1 1-2 --verbose
```

This command will continuously check if device `1-2` is attached from host `172.20.0.1`. If it's not attached but is available (listed), it will attempt to attach it using the `usbip` command.

## GitLab CI

This repository includes a `.gitlab-ci.yml` file that automates the build process:
1.  It builds the Docker build environment image and pushes it to the project's container registry.
2.  It runs two parallel jobs using the built image to compile the `amd64` and `arm64` static MUSL executables.
3.  These executables are saved as downloadable artifacts for each CI pipeline run.

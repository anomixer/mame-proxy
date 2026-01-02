# MameProxy

MameProxy is a Windows virtual file system (using WinFsp) designed for MAME ROM management. It implements a **Lazy Download** mechanism: when MAME requests a ROM that doesn't exist locally, MameProxy automatically downloads it from a configured remote server and serves it seamlessly.

## Key Features

*   **On-Demand Download**: Files are only downloaded when accessed, saving local disk space.
*   **WinFsp Integration**: High-performance virtual disk driver for a smooth experience.
*   **Smart Routing**: Automatically handles `Standalone` (7z) and `Split` (zip) directory structures for `mdk.cab`.
*   **Local Caching**: Downloaded files are stored locally; subsequent access is instant.
*   **Non-Admin Execution**: Supports Disk Mode mounting, allowing usage without administrative privileges.

## Prerequisites

1.  **Windows 10/11**
2.  **WinFsp**: You must install the WinFsp driver.
    *   Download: [WinFsp Official Website](https://winfsp.dev/)
3.  **Visual Studio 2022**: Required to build the project (with C++ Desktop Development workload).

## Build Instructions

This project uses CMake for building:

1.  Open the project folder in Visual Studio 2022.
2.  VS will automatically configure CMake.
3.  Select the `Release` configuration and "Build All".
4.  The executable will be generated at `build/Release/MameProxy.exe`.

## Usage

Start the proxy from the command line:

```cmd
MameProxy.exe -m <MountPoint> -c <CacheDir> -u <RemoteURL>
```

### Example: Smart Routing Mode

```cmd
MameProxy.exe -m Z: -c C:\MameCache -u https://mdk.cab/download/
```

*   **Auto-Detection**: The application automatically appends `split/` or `standalone/` to the URL based on whether MAME requests a `.zip` or `.7z` file.
*   `-m Z:`: Mounts the virtual drive as `Z:`.
*   `-c C:\MameCache`: Local directory for stored files.
*   `-u ...`: The base URL for MAME ROM sources.

## MAME Configuration

Once MameProxy is running, point MAME's `rompath` to the mount point:

```cmd
mame.exe -rompath Z:\ pacman
```

## Important Notes

*   **Directory Listing**: For performance, `dir Z:\` only shows locally cached files. If you know the ROM name, running it directly will trigger the download.
*   **Termination**: Closing the `MameProxy.exe` window will automatically unmount the virtual drive.

## Technical Architecture

*   **WinFsp C++ API**: Core file system logic.
*   **WinHTTP**: Reliable asynchronous file transfers.
*   **Disk Mode Fallback**: Automatically switches to highly compatible Disk Mode if the Launcher service is unavailable.

---
Developer: Antigravity (Advanced Agentic Coding Team)
Repository: [GitHub - anomixer/mame-proxy](https://github.com/anomixer/mame-proxy)

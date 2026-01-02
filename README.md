# MameCloudRompath (MCR)

[繁體中文版](./README-TW.md)

MameCloudRompath (MCR) is a Windows virtual file system (using WinFsp) designed for MAME ROM management. It implements a **Lazy Download** mechanism: when MAME requests a ROM that doesn't exist locally, MCR automatically downloads it from a configured remote server and serves it seamlessly.

## Motivation

MAME updates almost monthly, and with each update, ROM filenames and contents can change. This creates a vicious cycle where players are forced to download tens of gigabytes of update packages just to keep their sets compatible with the latest version. There is nothing more frustrating than wanting to play a quick game like `pacman`, only to find it won't start due to a version mismatch in your ROM set.

Most players only frequently play a small handful of games. Services like `mdk.cab` stay current with the latest MAME-to-ROM mappings. MCR was born from this frustration: **"The proxy only fetches the correct ROM when you actually want to play the game."**

It's designed to solve the "ROM Version Hell" faced by 95% of players. Stop managing files and start playing.

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
    *   **Note**: Make sure to select the **"Developer"** component during installation (includes necessary headers and libraries).
3.  **Visual Studio 2022**: Required to build the project (with C++ Desktop Development workload).

## Quick Setup (Recommended)

To quickly configure and build the project, run the included configuration script:

1.  Run `config.bat`.
2.  Follow the prompts to set your cache directory and drive letter.
3.  The script will automatically build the project if needed and create a `mcr.bat` file.
4.  Run `mcr.bat` to start the virtual drive.

## Build Instructions (Manual)

This project uses CMake for building:

1.  Open the project folder in Visual Studio 2022.
2.  VS will automatically configure CMake.
3.  Select the `Release` configuration and "Build All".
4.  The executable will be generated at `build/Release/mcr.exe`.

## Usage

Start the program from the command line:

```cmd
mcr.exe -m <MountPoint> -c <CacheDir> -u <RemoteURL>
```

### Example: Smart Routing Mode

```cmd
mcr.exe -m Z: -c C:\MameCache -u https://mdk.cab/download/
```

*   **Auto-Detection**: The application automatically appends `split/` or `standalone/` to the URL based on whether MAME requests a `.zip` or `.7z` file.
*   `-m Z:`: Mounts the virtual drive as `Z:`.
*   `-c C:\MameCache`: Local directory for stored files.
*   `-u ...`: The base URL for MAME ROM sources.

## MAME Configuration

Once MCR is running, point MAME's `rompath` to the mount point:

```cmd
mame.exe -rompath Z:\ pacman
```

## How it Works

MameCloudRompath acts as an intelligent intermediary. Here is the sequence of events during a game startup:

1.  **Interception**: When MAME searches for a ROM in its `-rompath Z:\`, it sends a standard file open request to Windows.
2.  **WinFsp Hand-off**: WinFsp intercepts this request and passes it to the user-mode MCR application.
3.  **Local Cache Check**: MCR checks your local cache directory (provided via `-c`).
    *   **Cache Hit**: If the file already exists locally, it is served immediately from your disk.
    *   **Cache Miss**: If the file is missing, MCR proceeds to the next step.
4.  **On-the-Fly Download**: MCR constructs the correct URL based on the file extension and fetches it from the remote server (e.g., `mdk.cab`).
5.  **Seamless Delivery**: Once the download completes, MCR provides the file handle back to MAME. MAME continues to load the game as if the file had always been there.

## Supported Scope & Limitations

To keep the application lightweight and efficient, MCR focus on files that change most frequently:

*   **Supported**: Game ROMs (`.zip`, `.7z`) and various device/BIOS files requested via `rompath`.
*   **Unsupported**:
    *   **Large CHD Files**: Due to their massive size and the fact they change less frequently, it is recommended to manage CHD files manually.
    *   **Auxiliary Directories**: Such as `snap`, `icons`, `samples`, `software` lists, etc. These assets are generally stable and should be prepared by the player.

## Important Notes

*   **Directory Listing**: For performance, `dir Z:\` only shows locally cached files. If you know the ROM name, running it directly will trigger the download.
*   **Termination**: Closing the `mcr.exe` window will automatically unmount the virtual drive.

## Technical Architecture

*   **WinFsp C++ API**: Core file system logic.
*   **WinHTTP**: Reliable asynchronous file transfers.
*   **Disk Mode Fallback**: Automatically switches to highly compatible Disk Mode if the Launcher service is unavailable.

---
Developer: Antigravity (Advanced Agentic Coding Team)
Repository: [GitHub - anomixer/mamecloudrom](https://github.com/anomixer/mamecloudrom)

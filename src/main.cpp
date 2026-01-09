#include "MameFs.h"
#include <iostream>
#include <string>
#include <vector>

void print_usage() {
  std::cout << "Usage: mcr -m <MountPoint> -c <CacheDir> -u <BaseUrl> [-7z]"
            << std::endl;
  std::cout << "\nOptions:" << std::endl;
  std::cout << "  -m   Mount point (e.g. Z:)" << std::endl;
  std::cout << "  -c   Cache directory (local storage)" << std::endl;
  std::cout << "  -u   Base URL (download source)" << std::endl;
  std::cout << "  -7z  Enable .7z file support (default: disabled)"
            << std::endl;
  std::cout << "\nExample: mcr -m Z: -c C:\\MAME\\romcache -u "
               "https://mdk.cab/download/ -7z"
            << std::endl;
}

int main(int argc, char *argv[]) {
  // Defines defaults
  std::wstring mountPoint = L"Z:";
  std::wstring cacheDir = L"C:\\MameCache";
  std::wstring baseUrl = L"https://mdk.cab/download/";
  bool enable7z = false;

  // Parse args
  // Since main gives char*, convert to wstring.
  // Minimially we expect Ascii args mostly.

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-m" && i + 1 < argc) {
      std::string val = argv[++i];
      mountPoint = std::wstring(val.begin(), val.end());
    } else if (arg == "-c" && i + 1 < argc) {
      std::string val = argv[++i];
      cacheDir = std::wstring(val.begin(), val.end());
    } else if (arg == "-u" && i + 1 < argc) {
      std::string val = argv[++i];
      baseUrl = std::wstring(val.begin(), val.end());
    } else if (arg == "-7z") {
      enable7z = true;
    } else {
      print_usage();
      return 1;
    }
  }

  std::wcout << L"Starting MameCloudRompath (MCR) v0.2..." << std::endl;
  std::wcout << L"Mount Point: " << mountPoint << std::endl;
  std::wcout << L"Cache Dir: " << cacheDir << std::endl;
  std::wcout << L"Base URL: " << baseUrl << std::endl;
  if (enable7z)
    std::wcout << L"7z Support: Enabled" << std::endl;

  return MameFs::Run(mountPoint, cacheDir, baseUrl, enable7z);
}

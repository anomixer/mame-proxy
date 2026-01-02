#include "MameFs.h"
#include <iostream>
#include <string>
#include <vector>

void print_usaeg() {
  std::cout << "Usage: mcr -m <MountPoint> -c <CacheDir> -u <BaseUrl>"
            << std::endl;
  std::cout << "Example: mcr -m Z: -c C:\\MAME\\romcache -u "
               "https://mdk.cab/download/"
            << std::endl;
}

int main(int argc, char *argv[]) {
  // Defines defaults
  std::wstring mountPoint = L"Z:";
  std::wstring cacheDir = L"C:\\MameCache";
  std::wstring baseUrl = L"https://mdk.cab/download/";

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
    } else {
      print_usaeg();
      return 1;
    }
  }

  std::wcout << L"Starting MameCloudRompath (MCR)..." << std::endl;
  std::wcout << L"Mount Point: " << mountPoint << std::endl;
  std::wcout << L"Cache Dir: " << cacheDir << std::endl;
  std::wcout << L"Base URL: " << baseUrl << std::endl;

  return MameFs::Run(mountPoint, cacheDir, baseUrl);
}

#include "Downloader.h"
#include <filesystem>
#include <fstream>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

bool Downloader::Download(const std::wstring &url,
                          const std::wstring &destination) {
  std::wstring hostname = GetHostname(url);
  std::wstring path = GetPath(url);

  std::wcout << L"Downloader: URL=" << url << std::endl;
  std::wcout << L"Downloader: Hostname=" << hostname << L", Path=" << path
             << std::endl;

  HINTERNET hSession =
      WinHttpOpen(L"MameCloudRompath/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    std::cerr << "WinHttpOpen failed: " << GetLastError() << std::endl;
    return false;
  }

  HINTERNET hConnect = WinHttpConnect(hSession, hostname.c_str(),
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    std::cerr << "WinHttpConnect failed: " << GetLastError() << std::endl;
    WinHttpCloseHandle(hSession);
    return false;
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    std::cerr << "WinHttpOpenRequest failed: " << GetLastError() << std::endl;
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  bool bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  if (bResults) {
    bResults = WinHttpReceiveResponse(hRequest, NULL);
  } else {
    std::cerr << "WinHttpSendRequest failed: " << GetLastError() << std::endl;
  }

  if (!bResults) {
    std::cerr << "WinHttpReceiveResponse failed: " << GetLastError()
              << std::endl;
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                      WINHTTP_NO_HEADER_INDEX);

  if (dwStatusCode != 200) {
    std::wcerr << L"HTTP Error: " << dwStatusCode << L" for " << path
               << std::endl;
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Get Content-Length for debugging and validation
  DWORD dwContentLength = 0;
  DWORD dwCLSize = sizeof(dwContentLength);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &dwContentLength, &dwCLSize,
                      WINHTTP_NO_HEADER_INDEX);
  std::wcout << L"Content-Length: " << dwContentLength << std::endl;

  // Abort if Content-Length is 0
  if (dwContentLength == 0) {
    std::wcerr << L"Error: Content-Length is 0. Aborting download."
               << std::endl;
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Simple skip: If file exists and has data, assume it's good.
  // This prevents MAME from seeing file changes/timestamp updates during
  // re-runs.
  try {
    if (std::filesystem::exists(destination) &&
        std::filesystem::file_size(destination) > 0) {
      std::wcout << L"Skipping download (file exists): " << destination
                 << std::endl;
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return true;
    }
  } catch (...) {
    // Ignore errors, proceed to download
  }

  // Ensure directory exists ONLY after successful header check
  std::filesystem::path destPath(destination);
  std::filesystem::path dirPath = destPath.parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }

  std::ofstream outFile(destination, std::ios::binary);
  if (!outFile.is_open()) {
    std::wcerr << L"Failed to open local file: " << destination << std::endl;
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD dwDownloaded = 0;
  DWORD dwTotalDownloaded = 0;
  do {
    dwSize = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
      break;
    if (dwSize == 0)
      break;

    char *pszOutBuffer = new char[dwSize];
    if (!pszOutBuffer)
      break;

    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize,
                        &dwDownloaded)) {
      outFile.write(pszOutBuffer, dwDownloaded);
      dwTotalDownloaded += dwDownloaded;
    }
    delete[] pszOutBuffer;
  } while (dwSize > 0);

  outFile.close();

  if (dwTotalDownloaded == 0) {
    std::wcerr << L"Error: Downloaded file is empty (0 bytes). Deleting."
               << std::endl;
    std::filesystem::remove(destination);

    // Attempt to remove the parent directory if it's empty
    try {
      if (std::filesystem::exists(dirPath) &&
          std::filesystem::is_empty(dirPath)) {
        std::filesystem::remove(dirPath);
        std::wcerr << L"Removed empty parent directory: " << dirPath.wstring()
                   << std::endl;
      }
    } catch (...) {
      // Ignore errors during cleanup
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  std::cout << "Download completed successfully. Total bytes: "
            << dwTotalDownloaded << std::endl;

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return true;
}

bool Downloader::ExtractFileFromZip(const std::wstring &zipPath,
                                    const std::wstring &fileName,
                                    const std::wstring &destPath) {
  // Ensure destination directory exists
  std::filesystem::path dest(destPath);
  std::filesystem::path destDir = dest.parent_path();
  std::filesystem::create_directories(destDir);

  // Construct tar command: tar -xf zipPath -C destDir fileName
  // Note: tar on Windows handles .zip files well.
  std::wstring command = L"tar -xf \"" + zipPath + L"\" -C \"" +
                         destDir.wstring() + L"\" \"" + fileName + L"\"";

  // Use CreateProcess to run tar safely (or system for simplicity in this
  // context) For robustness, we'll use _wsystem but hide output effectively?
  // Actually, seeing output is good for now.

  // std::wcout << L"Attempting extraction: " << command << std::endl;
  int result = _wsystem(command.c_str());

  if (result == 0 && std::filesystem::exists(dest)) {
    std::wcout << L"Extracted: " << fileName << std::endl;
    return true;
  }

  std::wcerr << L"Extraction failed for: " << fileName << L" from " << zipPath
             << std::endl;
  return false;
}

std::wstring Downloader::GetHostname(const std::wstring &url) {
  size_t start = 0;
  if (url.find(L"https://") == 0)
    start = 8;
  else if (url.find(L"http://") == 0)
    start = 7;

  size_t end = url.find(L"/", start);
  if (end == std::wstring::npos)
    return url.substr(start);
  return url.substr(start, end - start);
}

std::wstring Downloader::GetPath(const std::wstring &url) {
  size_t start = 0;
  if (url.find(L"https://") == 0)
    start = 8;
  else if (url.find(L"http://") == 0)
    start = 7;

  size_t end = url.find(L"/", start);
  if (end == std::wstring::npos)
    return L"/";
  return url.substr(end);
}

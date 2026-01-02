#include "Downloader.h"
#include <fstream>
#include <iostream>
#include <vector>


#pragma comment(lib, "winhttp.lib")

bool Downloader::Download(const std::wstring &url,
                          const std::wstring &destination) {
  std::wstring hostname = GetHostname(url);
  std::wstring path = GetPath(url);

  std::wcout << L"Downloader: URL=" << url << std::endl;
  std::wcout << L"Downloader: Hostname=" << hostname << L", Path=" << path
             << std::endl;

  HINTERNET hSession =
      WinHttpOpen(L"MameProxy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
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

  std::ofstream outFile(destination, std::ios::binary);
  if (!outFile.is_open()) {
    std::wcerr << L"Failed to open local file: " << destination << std::endl;
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD dwDownloaded = 0;
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
    }
    delete[] pszOutBuffer;
  } while (dwSize > 0);

  outFile.close();
  std::cout << "Download completed successfully." << std::endl;

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return true;
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

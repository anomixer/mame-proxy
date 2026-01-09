#pragma once
#include <string>
#include <windows.h>
#include <winhttp.h>

class Downloader {
public:
  static bool Download(const std::wstring &url,
                       const std::wstring &destination);
  static bool ExtractFileFromZip(const std::wstring &zipPath,
                                 const std::wstring &fileName,
                                 const std::wstring &destPath);

private:
  static bool SaveToFile(const std::wstring &path, const std::string &data);
  static std::wstring GetHostname(const std::wstring &url);
  static std::wstring GetPath(const std::wstring &url);
};

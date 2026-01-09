#include "MameFs.h"
#include "Downloader.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <winfsp/winfsp.h>

// PathCombine
#include <winternl.h> // For NTSTATUS values if needed, otherwise Fsp headers usually provide them

#pragma comment(lib, "shlwapi.lib")

// Helper to convert FILETIME to UINT64
static UINT64 FileTimeToInt64(const FILETIME &ft) {
  return ((UINT64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

struct MameFileContext {
  HANDLE Handle;
  HANDLE FindHandle;
  WIN32_FIND_DATAW FindData;
  bool IsDirectory;
  std::wstring Path;

  MameFileContext()
      : Handle(INVALID_HANDLE_VALUE), FindHandle(INVALID_HANDLE_VALUE),
        IsDirectory(false) {
    memset(&FindData, 0, sizeof(FindData));
  }
};

static UINT64 GetPathHash(const std::wstring &path) {
  std::wstring lowerPath = path;
  for (auto &c : lowerPath)
    c = towlower(c);
  std::hash<std::wstring> hasher;
  return (UINT64)hasher(lowerPath);
}

std::wstring MameFs::m_CacheDir;
std::wstring MameFs::m_BaseUrl;
bool MameFs::m_Enable7z = false;

std::wstring MameFs::GetLocalPath(PCWSTR fileName) {
  // Skip leading slash of fileName if present to append cleanly?
  // PathCombine handles it: if p2 starts with \, it might ignore p1.
  // We want local path construction.
  // FileName comes from WinFsp as "\path\file".
  // We want C:\Cache\path\file.

  // Simple concat if we ensure m_CacheDir doesn't end with slash and fileName
  // starts with slash.
  if (fileName[0] == L'\\') {
    return m_CacheDir + fileName;
  }
  return m_CacheDir + L"\\" + fileName;
}

int MameFs::Run(const std::wstring &mountPoint, const std::wstring &cacheDir,
                const std::wstring &baseUrl, bool enable7z) {
  m_CacheDir = cacheDir;
  m_BaseUrl = baseUrl;
  m_Enable7z = enable7z;

  // Ensure cache dir exists
  CreateDirectoryW(m_CacheDir.c_str(), NULL);

  FSP_FILE_SYSTEM *FileSystem = NULL;
  FSP_FILE_SYSTEM_INTERFACE *Interface = new FSP_FILE_SYSTEM_INTERFACE();
  memset(Interface, 0, sizeof(*Interface));

  Interface->GetVolumeInfo = SGetVolumeInfo;
  Interface->GetSecurity = SGetSecurity;
  Interface->Create = SCreate;
  Interface->Open = SOpen;
  Interface->Read = SRead;
  Interface->Close = SClose;
  Interface->Cleanup = SCleanup;
  Interface->ReadDirectory = SReadDirectory;
  Interface->GetFileInfo = SGetFileInfo;
  Interface->Overwrite = SOverwrite;

  NTSTATUS Status = STATUS_SUCCESS;

  // FspFileSystemCreate handles device path internally based on flags if needed
  // SimpleVolumeParams removed

  FSP_FSCTL_VOLUME_PARAMS VolumeParams = {0};
  VolumeParams.SectorSize = 4096;
  VolumeParams.SectorsPerAllocationUnit = 1;
  VolumeParams.MaxComponentLength = 255;
  VolumeParams.FileInfoTimeout = 0; // Disable caching to force SGetFileInfo and
                                    // ensure HardLinks=1 is always fresh
  VolumeParams.CaseSensitiveSearch = 0;
  VolumeParams.CasePreservedNames = 1;
  VolumeParams.UnicodeOnDisk = 1;
  VolumeParams.PersistentAcls = 0;

  // Try multiple names to avoid collision. Launcher Mode (NULL device) often
  // requires a network prefix like \\server\share
  for (int i = 0; i < 5; ++i) {
    std::wstring uniqueSuffix = std::to_wstring(GetTickCount() + i);
    std::wstring prefix = L"\\mame-mcr" + uniqueSuffix + L"\\roms";
    std::wstring name = L"MameCloudRompath" + uniqueSuffix;

    wcscpy_s(VolumeParams.Prefix, 64, prefix.c_str());
    wcscpy_s(VolumeParams.FileSystemName, 64, name.c_str());

    std::wcout << L"Attempting Launcher Mode with Prefix: "
               << VolumeParams.Prefix << std::endl;
    Status =
        FspFileSystemCreate((PWSTR)0, &VolumeParams, Interface, &FileSystem);

    if (NT_SUCCESS(Status))
      break;
    if (Status != 0xc0000033)
      break;
    std::cerr << "Name collision, retrying with unique suffix..." << std::endl;
  }

  // Fallback to Disk Mode if Launcher failed
  if (!NT_SUCCESS(Status)) {
    std::cerr << "Launcher attempts failed. Falling back to Disk Mode..."
              << std::endl;
    VolumeParams.Prefix[0] = L'\0';
    wcscpy_s(VolumeParams.FileSystemName, 32, L"MameCloudRompathDisk");
    Status = FspFileSystemCreate((PWSTR)L"\\Device\\WinFsp.Disk", &VolumeParams,
                                 Interface, &FileSystem);
  }

  if (!NT_SUCCESS(Status)) {
    std::cerr << "All FspFileSystemCreate attempts failed. Status: " << std::hex
              << Status << std::endl;
    return -1;
  }
  std::cout << "FileSystem created successfully." << std::endl;

  Status = FspFileSystemSetMountPoint(FileSystem, (PWSTR)mountPoint.c_str());
  if (!NT_SUCCESS(Status)) {
    std::cerr << "FspFileSystemSetMountPoint failed: " << std::hex << Status
              << std::endl;
    FspFileSystemDelete(FileSystem);
    return -1;
  }
  std::wcout << L"Mounted successfully at " << mountPoint << std::endl;

  // Enable Debug logs
  FspDebugLogSetHandle(GetStdHandle(STD_ERROR_HANDLE));
  FspFileSystemSetDebugLog(FileSystem, -1);

  std::cout << "Starting dispatcher..." << std::endl;
  Status = FspFileSystemStartDispatcher(FileSystem, 0);
  if (!NT_SUCCESS(Status)) {
    std::cerr << "FspFileSystemStartDispatcher failed: " << std::hex << Status
              << std::endl;
    FspFileSystemDelete(FileSystem);
    return -1;
  }

  std::cout << "Dispatcher started. Drive should be available now."
            << std::endl;
  std::cout << "Keeping the process alive. Check the target mount point. Press "
               "Ctrl+C to stop."
            << std::endl;

  // Sleep indefinitely to keep the file system alive
  while (true) {
    Sleep(10000);
  }

  FspFileSystemDelete(FileSystem);
  return 0;
}

NTSTATUS MameFs::SGetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
                                FSP_FSCTL_VOLUME_INFO *VolumeInfo) {
  std::cout << "DEBUG: SGetVolumeInfo" << std::endl;
  VolumeInfo->TotalSize = 1024LL * 1024 * 1024 * 1024; // Fake 1TB
  VolumeInfo->FreeSize = 512LL * 1024 * 1024 * 1024;
  wcscpy_s(VolumeInfo->VolumeLabel, 32, L"MameCloudRompath");
  VolumeInfo->VolumeLabelLength =
      (UINT16)(wcslen(VolumeInfo->VolumeLabel) * sizeof(WCHAR));
  return STATUS_SUCCESS;
}

NTSTATUS MameFs::SGetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                              PSECURITY_DESCRIPTOR SecurityDescriptor,
                              SIZE_T *SecurityDescriptorSize) {
  // If we return STATUS_NOT_IMPLEMENTED, WinFsp uses default security
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS MameFs::SCreate(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName,
                         UINT32 CreateOptions, UINT32 GrantedAccess,
                         UINT32 FileAttributes,
                         PSECURITY_DESCRIPTOR SecurityDescriptor,
                         UINT64 AllocationSize, PVOID *PFileContext,
                         FSP_FSCTL_FILE_INFO *FileInfo) {
  std::wcout << L"DEBUG: SCreate " << FileName << std::endl;
  // For read-only, we only support opening existing files via Create too
  return SOpen(FileSystem, FileName, CreateOptions, GrantedAccess, PFileContext,
               FileInfo);
}

NTSTATUS MameFs::SOpen(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName,
                       UINT32 CreateOptions, UINT32 GrantedAccess,
                       PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo) {
  try {
    std::wcout << L"DEBUG: SOpen " << FileName << std::endl;
    std::wstring localPath = GetLocalPath(FileName);

    // Heuristic: Is this an archive or a split file?
    std::wstring fileNameStr = FileName;
    bool isZip = (fileNameStr.length() > 4 &&
                  fileNameStr.substr(fileNameStr.length() - 4) == L".zip");
    bool is7z = (fileNameStr.length() > 3 &&
                 fileNameStr.substr(fileNameStr.length() - 3) == L".7z");
    bool isRoot = (wcscmp(FileName, L"\\") == 0);
    bool isDirectoryRequest = (CreateOptions & FILE_DIRECTORY_FILE);

    // If it's a directory or root, handle normally (create/open local dir).
    if (isRoot || isDirectoryRequest) {
      if (GetFileAttributesW(localPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::filesystem::create_directories(localPath);
      }
    } else if (!isZip && !is7z) {
      // It is a single file request (e.g., \sf2ce\rom.bin).
      // PER USER REQUEST: DO NOT SERVE IT. DO NOT DOWNLOAD IT.
      // Instead, ensure parent ZIP exists, then return NOT FOUND.

      std::filesystem::path p(localPath);
      if (p.has_parent_path()) {
        std::filesystem::path parentDir = p.parent_path(); // e.g. ...\sf2ce
        // Check if parent directory name matches the expected zip name pattern
        // logic
        std::wstring parentDirName = parentDir.filename().wstring();
        if (parentDirName != L"mamecache" &&
            !parentDirName.empty()) { // Avoid root cache dir itself
          std::wstring zipFileName = parentDirName + L".zip";
          std::filesystem::path cacheRoot = parentDir.parent_path();
          std::filesystem::path zipPath = cacheRoot / zipFileName;

          // Proactively download ZIP if missing
          if (!std::filesystem::exists(zipPath)) {
            std::wstring zipUrl = m_BaseUrl;
            if (zipUrl.back() == L'/')
              zipUrl.pop_back();
            zipUrl += L"/split/" + zipFileName;

            std::wcout
                << L"Split file requested. Triggering proactive ZIP download: "
                << zipUrl << std::endl;
            Downloader::Download(zipUrl, zipPath.wstring());
          }
        }
      }

      // Always return NOT FOUND for single files to force MAME to use the ZIP.
      return STATUS_OBJECT_NAME_NOT_FOUND;
    } else {
      // It IS an archive (.zip or .7z). Handle normal download logic.
      if (GetFileAttributesW(localPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring url = m_BaseUrl;
        if (url.back() == L'/')
          url.pop_back();

        if (isZip) {
          if (url.find(L"/standalone") != std::wstring::npos) {
            size_t pos = url.find(L"/standalone");
            url.replace(pos, 11, L"/split");
          } else if (url.find(L"/split") == std::wstring::npos) {
            if (url.back() != L'/')
              url += L"/";
            url += L"split";
          }
          std::wcout << L"Routing .zip request to split directory..."
                     << std::endl;
        } else if (is7z) {
          if (!m_Enable7z) {
            std::wcerr << L"Ignored .7z request (7z support disabled)."
                       << std::endl;
            return STATUS_OBJECT_NAME_NOT_FOUND;
          }
          if (url.find(L"/split") != std::wstring::npos) {
            size_t pos = url.find(L"/split");
            url.replace(pos, 6, L"/standalone");
          } else if (url.find(L"/standalone") == std::wstring::npos) {
            if (url.back() != L'/')
              url += L"/";
            url += L"standalone";
          }
          std::wcout << L"Routing .7z request to standalone directory..."
                     << std::endl;
        }

        // Append filename (convert \ to /)
        std::wstring relPath = FileName;
        for (auto &c : relPath)
          if (c == L'\\')
            c = L'/';
        url += relPath;

        if (!Downloader::Download(url, localPath)) {
          std::wcerr << L"Download failed for archive: " << url << std::endl;
          return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        std::wcout << L"Download success for archive: " << localPath
                   << std::endl;
      }
    }

    DWORD fileAttr = GetFileAttributesW(localPath.c_str());
    bool isDir = (fileAttr != INVALID_FILE_ATTRIBUTES) &&
                 (fileAttr & FILE_ATTRIBUTE_DIRECTORY);
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (isDir)
      flags |= FILE_FLAG_BACKUP_SEMANTICS;
    if (isZip || is7z)
      flags |= FILE_FLAG_RANDOM_ACCESS;

    HANDLE hFile =
        CreateFileW(localPath.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, flags, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
      DWORD err = GetLastError();
      std::wcerr << L"CreateFileW failed for " << localPath << L": " << err
                 << std::endl;
      if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
        return STATUS_OBJECT_NAME_NOT_FOUND;
      if (err == ERROR_ACCESS_DENIED)
        return STATUS_ACCESS_DENIED;
      if (err == ERROR_SHARING_VIOLATION)
        return STATUS_SHARING_VIOLATION;
      return STATUS_UNSUCCESSFUL;
    }

    // FileOpened: // Label no longer needed with refactored logic
    MameFileContext *ctx = new MameFileContext();
    ctx->Handle = hFile;
    ctx->Path = localPath;
    ctx->IsDirectory =
        (GetFileAttributesW(localPath.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
    ctx->FindHandle = INVALID_HANDLE_VALUE;

    *PFileContext = ctx;

    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(hFile, &info)) {
      FileInfo->FileAttributes = info.dwFileAttributes;
      FileInfo->ReparseTag = 0;
      FileInfo->AllocationSize =
          ((UINT64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
      FileInfo->FileSize = FileInfo->AllocationSize;
      FileInfo->CreationTime = FileTimeToInt64(info.ftCreationTime);
      FileInfo->LastAccessTime = FileTimeToInt64(info.ftLastAccessTime);
      FileInfo->LastWriteTime = FileTimeToInt64(info.ftLastWriteTime);
      FileInfo->ChangeTime = FileInfo->LastWriteTime;
      FileInfo->ChangeTime = FileInfo->LastWriteTime;
      FileInfo->IndexNumber = GetPathHash(localPath);
      FileInfo->HardLinks = 1; // Force 1 to pacify usage limits "Too many
                               // links" error in MAME/unzip
      std::wcout << L"DEBUG: SOpen success, Index=" << FileInfo->IndexNumber
                 << L", Links=" << FileInfo->HardLinks << std::endl;
      return STATUS_SUCCESS;
    }
    DWORD err = GetLastError();
    std::wcerr << L"GetFileInformationByHandle failed in SOpen: " << err
               << L" for " << localPath << std::endl;
    delete ctx;
    CloseHandle(hFile);
    return STATUS_UNSUCCESSFUL;
  } catch (const std::exception &e) {
    std::wcerr << L"Exception in SOpen: " << e.what() << std::endl;
    return STATUS_UNSUCCESSFUL;
  } catch (...) {
    std::wcerr << L"Unknown exception in SOpen" << std::endl;
    return STATUS_UNSUCCESSFUL;
  }
}

// Helper defines if not present
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif

// Implementations

void MameFs::SClose(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext) {
  MameFileContext *ctx = (MameFileContext *)FileContext;
  if (ctx) {
    if (ctx->Handle != INVALID_HANDLE_VALUE) {
      // std::wcout << L"DEBUG: SClose Handle " << ctx->Handle << std::endl;
      CloseHandle(ctx->Handle);
    }
    if (ctx->FindHandle != INVALID_HANDLE_VALUE) {
      FindClose(ctx->FindHandle);
    }
    delete ctx;
  }
}

void MameFs::SCleanup(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                      PWSTR FileName, ULONG Flags) {
  // Read only
}

NTSTATUS MameFs::SOverwrite(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                            UINT32 FileAttributes,
                            BOOLEAN ReplaceFileAttributes,
                            UINT64 AllocationSize,
                            FSP_FSCTL_FILE_INFO *FileInfo) {
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MameFs::SRead(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                       PVOID Buffer, UINT64 Offset, ULONG Length,
                       PULONG PBytesTransferred) {
  MameFileContext *ctx = (MameFileContext *)FileContext;
  if (!ctx || ctx->Handle == INVALID_HANDLE_VALUE)
    return STATUS_INVALID_HANDLE;

  OVERLAPPED ov = {0};
  ov.Offset = (DWORD)Offset;
  ov.OffsetHigh = (DWORD)(Offset >> 32);

  DWORD bytesRead = 0;
  if (!ReadFile(ctx->Handle, Buffer, Length, &bytesRead, &ov)) {
    DWORD err = GetLastError();
    if (err == ERROR_HANDLE_EOF) {
      *PBytesTransferred = 0;
      return STATUS_END_OF_FILE;
    }
    std::wcerr << L"SRead failed: " << err << std::endl;
    return STATUS_UNSUCCESSFUL;
  }

  // Debug partial reads
  if (bytesRead < Length) {
    // Check if EOF?
    // We can't easily check EOF without another call or knowing size.
    // But for now let's just log it if it's suspicious.
    // actually, short reads are valid at EOF.
    // Let's explicitly check if we are at EOF.
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(ctx->Handle, &info)) {
      UINT64 size = ((UINT64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
      if (Offset + bytesRead < size) {
        std::wcerr << L"WARNING: SRead partial read in middle of file! Req="
                   << Length << L" Read=" << bytesRead << L" Off=" << Offset
                   << L" Size=" << size << std::endl;
      }
    }
  }

  *PBytesTransferred = bytesRead;
  return STATUS_SUCCESS;
}

NTSTATUS MameFs::SGetFileInfo(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                              FSP_FSCTL_FILE_INFO *FileInfo) {
  MameFileContext *ctx = (MameFileContext *)FileContext;
  if (!ctx || ctx->Handle == INVALID_HANDLE_VALUE)
    return STATUS_INVALID_HANDLE;

  BY_HANDLE_FILE_INFORMATION info;
  if (GetFileInformationByHandle(ctx->Handle, &info)) {
    FileInfo->FileAttributes = info.dwFileAttributes;
    FileInfo->ReparseTag = 0;
    FileInfo->AllocationSize =
        ((UINT64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    FileInfo->FileSize = FileInfo->AllocationSize;
    FileInfo->CreationTime = FileTimeToInt64(info.ftCreationTime);
    FileInfo->LastAccessTime = FileTimeToInt64(info.ftLastAccessTime);
    FileInfo->LastWriteTime = FileTimeToInt64(info.ftLastWriteTime);
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0; // consistent with SReadDirectory
    FileInfo->HardLinks = 1;   // Force 1 here too
    // std::wcout << L"SGetFileInfo: Index=" << FileInfo->IndexNumber << L"
    // Links=" << FileInfo->HardLinks << std::endl;
    return STATUS_SUCCESS;
  }
  DWORD err = GetLastError();
  std::wcerr << L"GetFileInformationByHandle failed in SGetFileInfo: " << err
             << std::endl;
  return STATUS_UNSUCCESSFUL;
}

NTSTATUS MameFs::SReadDirectory(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                                PWSTR Pattern, PWSTR Marker, PVOID Buffer,
                                ULONG Length, PULONG PBytesTransferred) {
  MameFileContext *ctx = (MameFileContext *)FileContext;
  if (!ctx || !ctx->IsDirectory)
    return STATUS_INVALID_HANDLE;

  // Reset search if Marker is NULL
  if (Marker == NULL) {
    if (ctx->FindHandle != INVALID_HANDLE_VALUE) {
      FindClose(ctx->FindHandle);
      ctx->FindHandle = INVALID_HANDLE_VALUE;
    }
  }

  // Initialize search if needed
  if (ctx->FindHandle == INVALID_HANDLE_VALUE) {
    std::wstring searchPath = ctx->Path + L"\\*";
    ctx->FindHandle = FindFirstFileW(searchPath.c_str(), &ctx->FindData);
    if (ctx->FindHandle == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND)
        return STATUS_NO_MORE_FILES;
      return STATUS_UNSUCCESSFUL;
    }

    if (Marker != NULL) {
      while (wcscmp(ctx->FindData.cFileName, Marker) != 0) {
        if (!FindNextFileW(ctx->FindHandle, &ctx->FindData)) {
          return STATUS_NO_MORE_FILES;
        }
      }
      if (!FindNextFileW(ctx->FindHandle, &ctx->FindData)) {
        return STATUS_NO_MORE_FILES;
      }
    }
  }

  NTSTATUS Result = STATUS_SUCCESS;

  while (true) {
    // Allocate buffer for DirInfo + Filename
    BYTE DirInfoBuf[sizeof(FSP_FSCTL_DIR_INFO) + MAX_PATH * sizeof(WCHAR)] = {
        0};
    FSP_FSCTL_DIR_INFO *pDirInfo = (FSP_FSCTL_DIR_INFO *)DirInfoBuf;

    // Calculate size: Struct size is usually header + 1 byte array.
    // We want Header + Name.
    // FSP_FSCTL_DIR_INFO has a variable length filename at the end.
    // We assume FileNameArr is the member name (standard WinFsp).
    // If compiler complains again about member name, we might need to check
    // header.

    // Safe access to members
    pDirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) +
                              wcslen(ctx->FindData.cFileName) * sizeof(WCHAR));
    if (pDirInfo->Size > sizeof(DirInfoBuf))
      pDirInfo->Size = (UINT16)sizeof(DirInfoBuf);

    // Copy name to the end of struct (FileNameArr)
    // Since we don't know the exact member name that is valid in this version
    // (FileNameArr? FileNameBuf?), We can pointer arithmetic. The filename
    // starts after the known fields. FSP_FSCTL_DIR_INFO struct layout:
    // [Size][Padding][FileInfo][FileNameArr...] We can just skip
    // sizeof(FSP_FSCTL_DIR_INFO) - 1 ? No.

    // Let's try direct member access `FileNameArr` again, forcing it.
    // If it compiles, good. If not, we use pointer offset.
    // Previous error did not complain about `FileNameBuf` (I introduced it).
    // It failed on `PFSP_FSCTL_DIR_INFO`.

    // Let's try copying to `((BYTE*)pDirInfo) + sizeof(FSP_FSCTL_DIR_INFO) -
    // sizeof(pDirInfo->FileNameArr)`. Or cleaner:
    // `(PWSTR)pDirInfo->FileNameArr`. (Assuming FileNameArr is defined).

    // Wait, earlier I used `pDirInfo->FileNameBuf` and the error was
    // `undeclared identifier 'pDirInfo'`. It didn't complain about
    // `FileNameBuf` because it failed parsing `pDirInfo` definition first! So
    // I don't know if `FileNameBuf` is correct. Standard WinFsp uses
    // `FileNameBuf`.

    memcpy(pDirInfo->FileNameBuf, ctx->FindData.cFileName,
           wcslen(ctx->FindData.cFileName) * sizeof(WCHAR));

    pDirInfo->FileInfo.FileAttributes = ctx->FindData.dwFileAttributes;
    pDirInfo->FileInfo.ReparseTag = 0;
    pDirInfo->FileInfo.AllocationSize =
        ((UINT64)ctx->FindData.nFileSizeHigh << 32) |
        ctx->FindData.nFileSizeLow;
    pDirInfo->FileInfo.FileSize = pDirInfo->FileInfo.AllocationSize;
    pDirInfo->FileInfo.CreationTime =
        FileTimeToInt64(ctx->FindData.ftCreationTime);
    pDirInfo->FileInfo.LastAccessTime =
        FileTimeToInt64(ctx->FindData.ftLastAccessTime);
    pDirInfo->FileInfo.LastWriteTime =
        FileTimeToInt64(ctx->FindData.ftLastWriteTime);
    pDirInfo->FileInfo.ChangeTime = pDirInfo->FileInfo.LastWriteTime;

    pDirInfo->FileInfo.IndexNumber = 0;
    pDirInfo->FileInfo.HardLinks = 1;

    // Manual FillDirectoryBuffer implementation to bypass overloading issues
    // Check if we have space
    if (*PBytesTransferred + pDirInfo->Size > Length) {
      // Buffer full
      break;
    }

    // Copy to buffer
    memcpy((BYTE *)Buffer + *PBytesTransferred, pDirInfo, pDirInfo->Size);
    *PBytesTransferred += pDirInfo->Size;
    BOOLEAN Added = TRUE;

    if (!Added) {
      break;
    }

    if (!FindNextFileW(ctx->FindHandle, &ctx->FindData)) {
      if (GetLastError() == ERROR_NO_MORE_FILES) {
        Result = STATUS_SUCCESS;
        break;
      } else {
        Result = STATUS_UNSUCCESSFUL;
        break;
      }
    }
  }

  return Result;
}

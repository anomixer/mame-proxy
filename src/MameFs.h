#pragma once
#include <string>
#include <winfsp/winfsp.h>

class MameFs {
public:
  static int Run(const std::wstring &mountPoint, const std::wstring &cacheDir,
                 const std::wstring &baseUrl);

private:
  static NTSTATUS SGetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
                                 FSP_FSCTL_VOLUME_INFO *VolumeInfo);
  static NTSTATUS SGetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                               PSECURITY_DESCRIPTOR SecurityDescriptor,
                               SIZE_T *SecurityDescriptorSize);
  static NTSTATUS SCreate(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName,
                          UINT32 CreateOptions, UINT32 GrantedAccess,
                          UINT32 FileAttributes,
                          PSECURITY_DESCRIPTOR SecurityDescriptor,
                          UINT64 AllocationSize, PVOID *PFileContext,
                          FSP_FSCTL_FILE_INFO *FileInfo);
  static NTSTATUS SOpen(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName,
                        UINT32 CreateOptions, UINT32 GrantedAccess,
                        PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo);
  static NTSTATUS SRead(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                        PVOID Buffer, UINT64 Offset, ULONG Length,
                        PULONG PBytesTransferred);
  static void SClose(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext);
  static void SCleanup(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                       PWSTR FileName, ULONG Flags);
  static NTSTATUS SReadDirectory(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                                 PWSTR Pattern, PWSTR Marker, PVOID Buffer,
                                 ULONG Length, PULONG PBytesTransferred);
  static NTSTATUS SGetFileInfo(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                               FSP_FSCTL_FILE_INFO *FileInfo);
  static NTSTATUS SOverwrite(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext,
                             UINT32 FileAttributes,
                             BOOLEAN ReplaceFileAttributes,
                             UINT64 AllocationSize,
                             FSP_FSCTL_FILE_INFO *FileInfo);

  static std::wstring m_CacheDir;
  static std::wstring m_BaseUrl;

  static std::wstring GetLocalPath(PCWSTR fileName);
};

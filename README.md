# MameProxy

MameProxy 是一個基於 Windows WinFsp (Windows File System Proxy) 的虛擬檔案系統，專為 MAME ROM 管理設計。它實現了「延遲下載 (Lazy Download)」機制：當 MAME 請求一個本地不存在的 ROM 時，MameProxy 會自動從指定的遠端伺服器下載該檔案，並無縫提供給 MAME 使用。

## 主要特色

*   **即時下載**：只有在開啟檔案時才會觸發下載，節省硬碟空間。
*   **WinFsp 整合**：高效能的虛擬磁碟驅動，操作流暢。
*   **多模式支援**：支援 `Standalone` (7z) 與 `Split` (zip) 等不同的 ROM 存放結構。
*   **本地快取**：下載過的檔案會儲存在本地目錄，下次開啟無需重新下載。
*   **非管理者執行**：支援以磁碟模式 (Disk Mode) 掛載，普通使用者權限即可運作。

## 系統需求

1.  **Windows 10/11**
2.  **WinFsp**: 必須安裝 WinFsp 驅動程式。
    *   下載地址：[WinFsp 官方網站](https://winfsp.dev/)
3.  **Visual Studio 2022**: 用於編譯專案 (需包含 C++ 桌面開發組件)。

## 編譯步驟

本專案使用 CMake 進行建置：

1.  使用 Visual Studio 2022 開啟專案資料夾。
2.  VS 會自動配置 CMake。
3.  選擇 `Release` 配置並執行「建置全部」。
4.  編譯產物將位於 `build/Release/MameProxy.exe`。

## 使用方法

使用命令列啟動代理程式：

```cmd
MameProxy.exe -m <掛載點> -c <快取路徑> -u <遠端URL>
```

### 範例：智慧路由模式

```cmd
MameProxy.exe -m Z: -c C:\MameCache -u https://mdk.cab/download/
```

*   **自動偵測**：程式會依據 MAME 請求的檔案類型（.zip 或 .7z）自動在網址後方補上 `split/` 或 `standalone/`。

*   `-m Z:`: 將虛擬磁碟掛載為 `Z:` 槽。
*   `-c C:\MameCache`: 下載的檔案將儲存於此。
*   `-u ...`: 指定 MAME ROM 的來源網址。

## MAME 設定

啟動 MameProxy 後，直接將 MAME 的 `rompath` 指向掛載點即可：

```cmd
mame.exe -rompath Z:\ pacman
```

## 注意事項

*   **目錄列表**：為了效能考量，`dir Z:\` 只會顯示「已下載」的檔案。如果您知道 ROM 名稱，直接執行即可觸發下載。
*   **中斷連線**：關閉 `MameProxy.exe` 視窗將會自動卸載虛擬磁碟。

## 技術架構

*   **WinFsp C++ API**: 核心檔案系統邏輯。
*   **WinHTTP**: 處理可靠的非同步檔案傳輸。
*   **Disk Mode Fallback**: 當 Launcher 服務不可用時，自動切換至相容性更高的磁碟模式。

---
開發者：Antigravity (Advanced Agentic Coding Team)
專案地址：[GitHub - anomixer/mame-proxy](https://github.com/anomixer/mame-proxy)

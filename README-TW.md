# MameCloudRompath (MCR)

[English Version](./README.md)

MameCloudRompath (MCR) 是一個基於 Windows WinFsp (Windows File System Proxy) 的虛擬檔案系統，專為 MAME ROM 管理設計。它實現了「延遲下載 (Lazy Download)」機制：當 MAME 請求一個本地不存在的 ROM 時，MCR 會自動從指定的遠端伺服器下載該檔案，並無縫提供給 MAME 使用。

## 開發動機

MAME 幾乎每個月都會更新，且每次更新時 ROM 的檔名與內容都可能重新編排。這導致玩家每更新一次 MAME 版本，就得對應下載幾十 GB 的 ROM 合集更新。如果你想玩某款遊戲（例如 `pacman`），卻因為新版 MAME 對應不到舊版 ROM 而無法啟動，那是很令人沮喪的。

大多數玩家經常玩的其實只有那幾款遊戲，而 `mdk.cab` 幾乎即時更新了與最新 MAME 版本對應的 ROM。這就是開發 MCR 的初衷：**「只有當你想玩某款遊戲時，代理程式才即時去抓取對應的 ROM」**。

它設計用來解決 95% 玩家都會遇到的「ROM 版本地獄」，把時間留給遊戲，而不是研究檔案。

## 主要特色

*   **即時下載**：只有在開啟檔案時才會觸發下載，節省硬碟空間。
*   **WinFsp 整合**：高效能的虛擬磁碟驅動，操作流暢。
*   **智慧路由**：針對 `mdk.cab` 自動處理 `Standalone` (7z) 與 `Split` (zip) 目錄結構。
*   **本地快取**：下載過的檔案會儲存在本地目錄，下次開啟無需重新下載。
*   **非管理者執行**：支援以磁碟模式 (Disk Mode) 掛載，普通使用者權限即可運作。

## 系統需求

1.  **Windows 10/11**
2.  **WinFsp**: 必須安裝 WinFsp 驅動程式。
    *   下載地址：[WinFsp 官方網站](https://winfsp.dev/)
    *   **重要**：安裝時請務必勾選 **「Developer」** 元件（包含開發用標頭檔與函式庫）。
3.  **MAME (最新版本)**：遊玩遊戲的核心程式。
    *   下載地址：[MAME 官方網站](https://www.mamedev.org/release.html)
4.  **Visual Studio 2022 (選配)**：僅當您想要自行從原始碼編譯專案時才需要 (需包含 C++ 桌面開發組件)。

## 快速設定 (推薦使用)

若要快速配置並編譯專案，請執行內建的設定腳本：

1.  執行 `config.bat`。
2.  按照提示設定您的快取目錄、磁碟機代號與 MAME 路徑。**(現有設定會自動載入作為預設值)**。
3.  該腳本會自動檢查環境、編譯專案，並建立一個 `mcr.bat` 啟動檔。
4.  執行 `mcr.bat`。它會啟動虛擬磁碟，並同時自動開啟一個新視窗幫您執行 MAME。

## 編譯步驟 (手動)

本專案使用 CMake 進行建置：

1.  使用 Visual Studio 2022 開啟專案資料夾。
2.  VS 會自動配置 CMake。
3.  選擇 `Release` 配置並執行「建置全部」。
4.  編譯產物將位於 `build/Release/mcr.exe`。

> [!TIP]
> **免編譯直接使用**：若您沒有安裝 Visual Studio，`build/Release` 資料夾中已包含預先編譯好的 `mcr.exe` 與必要檔案。您可以跳過編譯步驟，直接進行 **快速設定**。

## 使用方法

使用命令列啟動程式：

```cmd
mcr.exe -m <掛載點> -c <快取路徑> -u <遠端URL> [-7z]

```

### 範例：智慧路由模式

```cmd
mcr.exe -m Z: -c C:\MameCache -u https://mdk.cab/download/ -7z

```

*   **自動偵測**：程式會依據 MAME 請求的檔案類型（.zip 或 .7z）自動在網址後方補上 `split/` 或 `standalone/`。

*   `-m Z:`: 將虛擬磁碟掛載為 `Z:` 槽。
*   `-c C:\MameCache`: 下載的檔案將儲存於此。
*   `-u ...`: 指定 MAME ROM 的來源網址。
*   `-7z`: (選用) 啟用 .7z 檔案支援。啟用後，對 .7z 的請求會被導向伺服器的 `standalone/` 目錄。若省略，則忽略所有 .7z 請求（回傳 NOT FOUND）。

## MAME 設定

啟動 MCR 後，**請勿關閉該視窗**。請另外**開啟一個新的命令提示字元 (CMD)**，並將 MAME 的 `rompath` 指向掛載點即可：

```cmd
mame.exe -rompath Z:\
```

*(您也可以直接指定遊戲啟動：`mame.exe -rompath Z:\ pacman`)*

## 工作原理

MameCloudRompath 運作的流程如下，讓您了解它是如何實現「免除 ROM 版本煩惱」的：

1.  **攔截請求**：當您在 MAME 指定 `-rompath Z:\` 並啟動遊戲時，MAME 會嘗試從 `Z:` 槽讀取 ROM 檔案。
2.  **WinFsp 介入**：Windows 核心會將讀取請求轉交給掛載 `Z:` 的 MCR。
3.  **本地快取檢查**：MCR 首先查看您的本地快取目錄 (`-c` 參數指定的路徑)。
    *   **如果檔案已存在**：直接從硬碟讀取並回傳給 MAME（就像一般磁碟一樣快）。
    *   **如果檔案不存在**：進入下一步。
4.  **即時線上下載**：MCR 會根據請求的檔名，自動判定類別（`.zip` 或 `.7z`），並從遠端伺服器（預設 `mdk.cab`）下載正確的對應檔案。
5.  **無縫銜接**：下載完成後，MCR 會開啟檔案控制權並回傳。MAME 完全不會感覺到中間經過了網路下載，遊戲隨即啟動。

## 支援範圍與限制

為了保持輕量化，MCR 專注於解決變動最頻繁的 ROM 檔案：

*   **支援下載**：位於 `rompath` 下的遊戲 ROM (`.zip`, `.7z`) 以及各式匯流排裝置、BIOS 檔案。
*   **不支援下載**：
    *   **大型 CHD 檔案**：由於 CHD 體積龐大且通常不隨 MAME 版本頻繁變動，建議玩家自行準備。
    *   **輔助目錄**：如 `snap` (截圖)、`icons` (圖示)、`samples` (取樣)、`software` (軟體清單) 等。這些資源變動較小，建議手動管理。

## 注意事項

*   **目錄列表**：為了效能考量，`dir Z:\` 只會顯示「已下載」的檔案。如果您知道 ROM 名稱，直接執行即可觸發下載。
*   **維持視窗開啟**：遊玩期間 MCR 必須持續執行。關閉 `mcr.exe` 視窗將會自動卸載虛擬磁碟。

## 檔案架構

- `build/Release/`: 包含預先編譯好的 `mcr.exe` 與 `winfsp-x64.dll`。
- `src/`: 專案原始碼 (C++)。
- `config.bat`: 互動式設定工具 (自動產生 `mcr.ini` 與 `mcr.bat`)。
- `build.bat`: 供開發者使用的手動編譯腳本。
- `mcr.ini`: (產出物) 儲存您的快取路徑、磁碟機代號與 MAME 目錄設定。
- `mcr.bat`: (產出物) 主啟動程式，負責掛載虛擬磁碟並連動開啟 MAME。

## 技術架構

*   **WinFsp C++ API**: 核心檔案系統邏輯。
*   **WinHTTP**: 處理可靠的非同步檔案傳輸。
*   **Disk Mode Fallback**: 當 Launcher 服務不可用時，自動切換至相容性更高的磁碟模式。

## 版本歷史

*   **v0.2 (2026-01-10)**:
    *   **穩定性更新**：修復了導致遊戲無法二次啟動的「Required files are missing」嚴重錯誤。
    *   **核心修正**：強制統一檔案 ID 生成邏輯，防止 MAME 快取失效。
    *   **日誌增強**：加入更詳細的錯誤代碼映射以利除錯。
    *   **安全下載**：改善下載邏輯，避免不必要的檔案覆蓋。

*   **v0.1 (2026-01-02)**:
    *   **初版發布**：正式更名為 "MameCloudRompath"。
    *   **智慧路由**：新增針對 Split (.zip) 與 Standalone (.7z) 檔案庫的自動路徑判斷。
    *   **自動化工具**：內建 `config.bat` 快速設定與啟動腳本生成器。

## 參與貢獻與免責聲明

MCR 目前處於 **v0.2 (穩定性更新)**。我們大幅改善了檔案處理的可靠性，並透過一致的檔案 ID 管理與安全下載邏輯，徹底解決了「Required files are missing」的錯誤。本專案按「原樣」提供，不負任何擔保責任。我們非常歡迎社群的參與：
*   **回報問題**：如果您發現任何 Bug，請提交 Issue。
*   **想法建議**：如果您有任何新功能的提案，歡迎與我們討論！
*   **程式碼貢獻**：如果您想改進程式碼，歡迎提交 Pull Request。

讓我們一起把 MCR 打造得更好！

---
開發者：anomixer + Antigravity (Gemini 3 Pro/Flash)

專案地址：[GitHub - anomixer/MameCloudRom](https://github.com/anomixer/MameCloudRom)

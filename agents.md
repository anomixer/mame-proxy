# 2026-01-02 MameCloudRompath (MCR) v0.1 開發摘要

今天我們完成了一個重大版本的開發與轉型，從核心功能修復到品牌重塑，全面提升了玩家的使用體驗。

## 1. 核心功能開發 (智慧路由技術)
*   **智慧延伸路徑路由**：在 `SOpen` 階段實作了自動判斷機制。當 MAME 請求 `.zip` 檔案時，程式會自動導向 `mdk.cab` 的 `/split` 目錄；請求 `.7z` 時則導向 `/standalone` 目錄。這徹底解決了之前 MAME 因為抓錯檔案格式而導致的 `unzip: couldn't find ECD` 損壞報錯。
*   **URL 自動修正**：現在使用者只需帶入 `https://mdk.cab/download/` 根目錄，程式會根據檔案副檔名自動補全正確的子路徑。

## 2. 自動化與使用者體驗提升 (v0.1)
*   **`config.bat` 智慧組態**：新增偵測 `mcr.ini` 功能，自動導入上次設定值作為預設。除了快取與磁碟代號，現在也會詢問 MAME 安裝路徑。
*   **全自動雙視窗啟動**：`mcr.bat` 現在會自動執行雙視窗策略：一個視窗執行 MCR 本體，並同步開啟另一個視窗自動 `cd` 進 MAME 目錄並執行 `mame.exe -rompath`。
*   **免編譯分發**：調整 `.gitignore` 策略，將 `build/Release/` 下的執行檔納入版本控制，方便沒有開發環境的玩家直接下載使用。
*   **互動語義化**：主程式新增 v0.1 版本顯示，並在Dispatcher啟動後加入終止操作提示 (Ctrl+C)。
*   **需求文檔優化**：在系統需求中正式加入 MAME 下載連結，並將 Visual Studio 2022 標記為原始碼編譯專用的「選配」組件。

## 3. 專案品牌重塑 (Rebranding)
*   **正式更名**：專案名稱正式從 `MameProxy` 更改為 **MameCloudRompath (MCR)**，精確傳達了「雲端 ROM 路徑」的核心價值。
*   **執行檔精簡**：將輸出的執行檔名稱由原本冗長的名稱精簡為 **`mcr.exe`**。
*   **國際化文檔**：建立了高品質的繁體中文 (`README-TW.md`) 與英文 (`README.md`) 說明文件，並加入「開發動機」與「工作原理」章節，內容更豐富專業。

## 4. 維護與清理
*   **快取清理機制**：手動清理了快取中因舊版本產生的 0 位元組損壞檔案，確保新版本的測試環境純淨。
*   **Git 儲存庫遷移**：協助處理 Git Remote 網址更新與 GitHub 遷移所需的文檔同步。

---
**開發者**：anomixer + Antigravity (Gemini 3 Pro/Flash)

**總結**：今天不僅修復了 MAME 無法讀取的問題，更將專案提升到了一個可供大眾玩家輕鬆使用的「產品級」水準。

# 2026-01-10 MameCloudRompath (MCR) v0.2 穩定性更新開發摘要

今天我們解決了困擾許久的「Required files are missing」問題，並發布了 v0.2 版本，大幅提升了程式的穩定性。

## 1. 核心穩定性修復 (Required files are missing)
*   **檔案 ID 一致性 (SReadDirectory & SOpen)**：
    *   **問題**：MAME 在掃描目錄與實際開啟檔案時，如果看到不同的檔案 ID (IndexNumber)，會判定檔案已變更或無效，導致快取失效並報錯。
    *   **解決方案**：強制將 `SReadDirectory` (目錄列表) 和 `SGetFileInfo` (檔案資訊) 的 `IndexNumber` 統一設定為 `0`。這消除了因為動態計算路徑雜湊或 WinFsp 預設行為導致的不一致。
*   **安全下載機制 (Safe Download Skip)**：
    *   **問題**：舊版即使檔案已存在，仍可能因 MAME 的存取模式觸發重複下載並覆蓋檔案，改變了檔案屬性 (`LastWriteTime`)，進一步觸發 MAME 的錯誤處理。
    *   **解決方案**：在 `Downloader::Download` 加入檢查，若目標檔案已存在且大小 > 0，則直接回報成功並**略過下載**。這保護了已存在 ROM 的完整性與時間戳記，確保 MAME 重複執行時能穩定讀取快取。

## 2. 錯誤處理與日誌優化
*   **Granular Win32 Error Mapping**：將 `CreateFileW` 的錯誤代碼精確映射到 NTSTATUS (如 `STATUS_SHARING_VIOLATION`, `STATUS_ACCESS_DENIED`)，讓 MAME 能獲得更準確的系統回饋。
*   **Debug Logging**：在 `SRead`, `SOpen`, `Downloader` 等關鍵路徑加入詳細的 Log，有助於未來排查問題。

## 3. 版本更新
*   主程式 (`main.cpp`) 與說明文件 (`README.md`, `README-TW.md`) 已全面更新為 **v0.2**。
*   重新編譯並輸出了最新的 `Release/mcr.exe`。

---
**開發者**：anomixer + Antigravity (Gemini 3 Pro/Flash)

**總結**：v0.2 版本標誌著 MCR 從「能用」邁向「穩定」。玩家現在可以放心地重複啟動遊戲，而不會遭遇惱人的缺檔錯誤。

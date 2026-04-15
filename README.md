# 影片失真檢測與修復專案

本專案使用 C++ 與 OpenCV，對輸入影片進行自動失真判斷，並依結果套用對應修復流程。  
目前支援的主要情境為：

- 雜訊偏高：執行去雜訊（Non-Local Means）
- 霧化偏高：執行去霧（Dark Channel Prior + Guided Filter）
- 模糊偏高：目前僅偵測與提示，尚未提供去模糊演算法

---

## 專案目標

針對單支影片做「先判斷、再修復」的流程：

1. 擷取第一幀，計算失真指標
2. 自動判斷主要失真類型（Blur / Noise / Haze）
3. 套用對應修復法並輸出新影片
4. 支援多輪修復（每輪可選擇是否繼續）

---

## 專案結構

```text
.
├─ main.cpp       # 主程式入口、失真決策、多輪修復流程控制
├─ denoise.h      # 去雜訊模組介面
├─ denoise.cpp    # 去雜訊與失真指標計算實作
├─ dehaze.h       # 去霧類別介面
└─ dehaze.cpp     # 去霧演算法實作（Dark Channel + Guided Filter）
```

---

## 技術與相依套件

- 語言：C++
- 影像處理函式庫：OpenCV
- 主要使用模組：`core`、`imgproc`、`photo`、`videoio`、`highgui`

建議環境：

- C++11 以上編譯器（建議 C++14/17）
- OpenCV 4.x

---

## 核心流程說明

主流程位於 `main.cpp`，每一輪大致執行如下：

1. 讀取影片與基本資訊（解析度、FPS、總幀數）
2. 取第一幀計算三個指標
3. 決定本輪要執行的修復類型
4. 執行修復並輸出影片
5. 詢問是否進入下一輪修復

### 失真判斷指標

程式使用三個指標：

1. **Laplacian Variance（模糊指標）**
   - 函式：`computeBlurVariance`
   - 邏輯：越小通常越模糊
2. **Noise Std Dev（雜訊指標）**
   - 函式：`computeNoiseStd`
   - 邏輯：原圖灰階與輕度 Gaussian 模糊差值的標準差，越大通常噪聲越高
3. **Dark Channel Score（霧化參考分數）**
   - 函式：`computeHazeScore`
   - 備註：目前主要用作參考輸出，不直接參與最終分支條件

### 決策規則（目前版本）

- 若 `lapVar < 25.0`，判定為 `BLUR`
- 否則若 `noiseStd > 2.0`，判定為 `NOISE`
- 其他情況判定為 `HAZE`

另外，程式會避免連續兩輪選到同一類型；若重複，會切換到其他修復類型。

---

## 去雜訊模組（`denoise.cpp`）

去雜訊核心使用 OpenCV：

- `fastNlMeansDenoisingColored(frame, denoisedFrame, 10, 10, 7, 21)`

處理方式：

- 逐幀讀取影片
- 每幀進行 Non-Local Means 去噪
- 寫入輸出影片
- 於終端顯示進度條

適用情境：

- 顆粒噪聲、彩色噪聲明顯影片

---

## 去霧模組（`dehaze.cpp`）

去霧流程以 Dark Channel Prior 為主，搭配 Guided Filter 細化透光率：

1. 計算暗通道圖 `darkChannelProcess`
2. 估計全域大氣光 `getGlobalAtmosphericLight`
3. 估計粗略透光率 `getRoughT`
4. 以灰階引導濾波細化透光率 `guidedFilter1Channel`
5. 使用成像模型回復清晰影像 `imgRecover`
6. 最後做亮度/對比補償（`alpha=3`, `beta=60`）

### 主要參數（目前硬編碼）

- 暗通道視窗：`darkWindowSize = 15`
- 引導濾波視窗：`filterWindowSize = 41`
- 透光率係數：`omega = 0.95`
- 透光率下限：`t0 = 0.1`
- 大氣光候選比例：暗通道前 `0.1%` 像素

---

## 建置方式

目前專案未附 `CMakeLists.txt` 或 Visual Studio 專案檔，需手動建置。

### 方式 A：g++（MSYS2 / MinGW 或 Linux）

```bash
g++ -std=c++17 main.cpp denoise.cpp dehaze.cpp -o video_repair $(pkg-config --cflags --libs opencv4)
```

若你的環境是 OpenCV 3，請將 `opencv4` 改為 `opencv`。

### 方式 B：Visual Studio（Windows）

1. 建立 C++ 主控台專案
2. 將 `main.cpp`、`denoise.cpp`、`dehaze.cpp` 加入專案
3. 設定 OpenCV 的 Include/Library 路徑
4. 在 Linker 設定 OpenCV 對應 `.lib`
5. 確保執行時可找到 OpenCV 的 `.dll`

---

## 執行方式

執行程式後會要求輸入影片檔名：

```text
Please enter the file name:
```

輸入例如：

```text
input.mp4
```

程式會顯示：

- 影片資訊（寬高、FPS、總幀數）
- 三個失真指標
- 本輪決策結果
- 修復進度條

---

## 輸出檔案規則

程式內部有多輪修復與檔名後綴機制（`_denoise` / `_dehaze` / `_deblur`）的設計。  
目前實作下，第一輪輸出檔預設從 `output.mp4` 開始，後續輪次會更新下一輪目標檔名。

若你希望輸出命名更直覺（例如直接輸出到 `outPath`），可再調整 `VideoWriter` 開啟時使用的路徑。

---

## 目前限制與注意事項

1. **去模糊未實作**  
   偵測到 `BLUR` 時目前直接提示無法修復並結束。

2. **判斷以第一幀為主**  
   若影片內容變化大，第一幀可能無法代表整段影片品質。

3. **參數為固定值**  
   閾值與演算法參數目前未提供外部設定檔或命令列參數。

4. **效能依影片解析度而異**  
   去雜訊與去霧皆為逐幀運算，高解析度影片耗時較長。

---

## 建議後續優化

- 新增真正的去模糊模組（Deblur）
- 將閾值與演算法參數外部化（CLI 或設定檔）
- 支援批次處理多支影片
- 補上單元測試與範例輸入輸出
- 加入 CMake 建置腳本，降低跨平台建置門檻

---

## 授權

目前專案未提供授權條款。  
若要公開或共用，建議補上 `LICENSE`（例如 MIT、Apache-2.0）。

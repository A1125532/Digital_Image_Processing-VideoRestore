// 檔名：main.cpp
//#include <iostream>
//#include <iomanip>
//#include <time.h>               // 用於 clock_gettime 高精度計時
//#include <opencv2/opencv.hpp>
#include "denoise.h"
#include "dehaze.h"

using namespace std;
using namespace cv;

enum ActionType { NONE, NOISE, HAZE, BLUR };

inline string actName(ActionType a) {
    switch (a) {
        case NOISE: 
            return "NOISE";
        case HAZE:  
            return "HAZE";
        case BLUR:  
            return "BLUR";
        default:      
            return "NONE";
    }
}


// 依據三個指標決定「最適修復」；再檢查是否重覆
ActionType decideAction(const Mat& f, ActionType last) {
    double lapVar = computeBlurVariance(f);
    double noiseStd = computeNoiseStd(f);
    double hazeScr = computeHazeScore(f);

    cout << fixed << setprecision(4);
    cout << "\n===== Distortion Metrics (larger means stronger effect) =====\n";
    cout << "Laplacian Variance (varianceLap) : " << lapVar << endl;
    cout << "Noise Std Dev (noiseStd)         : " << noiseStd << endl;
    cout << "Dark Channel Score (hazeScore)   : " << hazeScr << "  (for reference only)\n\n";

    bool isBlur = lapVar < 25.0;      // ★ 閥值沿用你的設定
    bool isNoise = !isBlur && noiseStd > 2.0;

    ActionType best = isBlur ? BLUR :
        isNoise ? NOISE : HAZE;

    // 不可跟上一輪相同 → 簡單換到其它類型
    if (best == last) {
        if (best == HAZE)  best = NOISE;
        else if (best == NOISE) best = HAZE;
        else best = NOISE;          // DEBLUR 重覆時改 DENOISE
    }
    return best;
}


// ─────────── 2. 保留原有修復演算法的呼叫包裝 ─────────
// (A) Dehaze：把你原本 main 裏那段 while 迴圈原樣搬進來
void runDehaze(VideoCapture& cap, VideoWriter& writer, long total) {
    deHaze dhObj;
    bool firstLoaded = false;
    const int barW = 50;
    long done = 0;
    Mat frame, res;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        if (!firstLoaded) {
            if (!dhObj.load(frame)) {
                cerr << "Error: failed to load first frame for dehazing. Aborting.\n";
                break;
            }
            firstLoaded = true;
        }
        else dhObj.srcImg = frame.clone();

        res = dhObj.practicalHazeRemoval(15, 41);
        writer.write(res);

        if (total > 0) {
            done++;
            float p = float(done) / total;  
            if (p > 1.0f) {
                p = 1.0f;
            }
            int pos = int(barW * p);
            cout << "\r[";
            for (int i = 0; i < barW; ++i) cout << (i < pos ? '=' : (i == pos ? '>' : ' '));
            cout << "] " << setw(3) << int(p * 100) << '%'; cout.flush();
        }
    }
    if (total > 0) cout << '\n';
}

int main(int argc, char** argv)
{
    string inputVideoPath;
    cout << "Please enter the file name: ";
    cin >> inputVideoPath;
    string currentPath = "output.mp4";
    string temp;
    // 1. 讀取命令列參數，或互動式提示
    /*if (argc >= 2) {
        inputVideoPath = argv[1];
    }
    else {
        cout << "Please enter the full path of the video to repair (e.g., .mp4): " << endl;
        getline(cin, inputVideoPath);
        if (inputVideoPath.empty()) {
            cerr << "Error: no path entered. Exiting.\n";
            return -1;
        }
    }*/
    if (inputVideoPath.empty()) {
        cerr << "Error: no path entered. Exiting.\n";
        return -1;
    }

    bool firstRun = true;
    ActionType lastAct = NONE;
    string outputVideoPath = "output_repaired.mp4";
    VideoWriter writer;

    while (true) {
        // 2. Open VideoCapture
        VideoCapture cap(inputVideoPath);
        if (!cap.isOpened()) {
            cerr << "Error: cannot open video: " << inputVideoPath << endl;
            return -1;
        }

        // 3. Retrieve properties
        int width = static_cast<int>(cap.get(CAP_PROP_FRAME_WIDTH));
        int height = static_cast<int>(cap.get(CAP_PROP_FRAME_HEIGHT));
        double fps = cap.get(CAP_PROP_FPS);
        long totalFrames = static_cast<long>(cap.get(CAP_PROP_FRAME_COUNT));

        cout << "Video resolution: " << width << " x " << height << endl;
        cout << "Video FPS:        " << fps << endl;
        cout << "Total frames:     " << totalFrames << endl;

        // 4. Read first frame and compute distortion metrics
        Mat firstFrame;
        if (!cap.read(firstFrame) || firstFrame.empty()) {
            cerr << "Error: cannot read first frame. Exiting.\n";
            return -1;
        }

        ActionType act = decideAction(firstFrame, lastAct);
        cout << "===== This Round's Decision =====\n";
        cout << "Primary distortion: "<< actName(act) << "\n";

        

        // 3) ★★ 首輪不詢問；次輪開始詢問 ─────────────────
        /*if (!firstRun) {
            char go;
            cout << "Do you want to continue repairing? (Y/y to continue; others to stop)";
            if (act == HAZE) {
                cout << "Video may be Damage.\n";
                cout << "Do you want to proceed.\n";
            }

            cin >> go;
            if (go != 'Y' && go != 'y') {
                cout << "Processing is complete, output file:" << outputVideoPath << '\n';
                cout << "Total processed frames: " << totalFrames << endl;
                cout << "Output video saved as: " << outputVideoPath << endl;
                break;
            }
        }*/
        char go;
        if (act == HAZE) {
            cout << "Video may be Damage.";
            cout << " Do you want to proceed? (Y/y to continue; others to stop)\n";

            cin >> go;
            if (go != 'Y' && go != 'y') {
                cout << "Processing is complete." << '\n';
                cout << "Total processed frames: " << totalFrames << endl;
                cout << "Output video saved as: " << currentPath << endl;
                break;
            }
        }
        if (!firstRun && act !=HAZE) {
            cout << "Do you want to continue repairing? (Y/y to continue; others to stop)" << endl;

            cin >> go;
            if (go != 'Y' && go != 'y') {
                cout << "Processing is complete." << '\n';
                cout << "Total processed frames: " << totalFrames << endl;
                cout << "Output video saved as: " << currentPath << endl;
                break;
            }
        }
        
        // 準備輸出路徑，根據處理類型 act 來決定檔名後綴字（suffix）
        string suffix = (act == NOISE ? "_denoise" :
            act == HAZE ? "_dehaze" : "_deblur");

        // 找出輸入檔案路徑中「最後一個小數點」的位置 → 找到副檔名（如 .mp4）
        size_t dot = currentPath.find_last_of('.');

        // 如果找不到副檔名，就直接在後面加上 _suffix.mp4
        // 否則就把副檔名前插入後綴字
        string outPath = (dot == string::npos) ?
            currentPath + suffix + ".mp4" :
            currentPath.substr(0, dot) + suffix + currentPath.substr(dot);

        // 設定輸出的影片編碼格式：mp4v（MPEG-4 編碼）
        int fourcc = VideoWriter::fourcc('m', 'p', '4', 'v');

        //正式開啟 VideoWriter，將每一幀處理後的畫面寫入這個檔案中
        writer.open(currentPath, fourcc, fps, Size(width, height), true);

        /*int fourcc = VideoWriter::fourcc('m', 'p', '4', 'v');
        writer.open(outputVideoPath, fourcc, fps, Size(width, height), true);
        if (!writer.isOpened()) {
            cerr << "Error: cannot create output video: " << outputVideoPath << endl;
            return -1;
        }*/

        // 6. Create VideoWriter
        firstRun = false;

        if (act == BLUR) {
            cout << "Cannot repair.Please check source quality.\n";
            return 0;
        }
        else if (act == NOISE) {
            cout << "Starting denoising process...\n";
            processDenoiseVideo(cap, writer, totalFrames);
        }
        else if(act == HAZE) {
            cout << "Starting denoising process...\n";
            runDehaze(cap, writer, totalFrames);
        }

        // 重新把影片游標設回第一幀，以便後續逐幀處理
        cap.set(CAP_PROP_POS_FRAMES, 0);

        cap.release();
        writer.release();
        destroyAllWindows();

        lastAct = act;
        currentPath = outPath;
    }
    
    return 0;
    
   
}
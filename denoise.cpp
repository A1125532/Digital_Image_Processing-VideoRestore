// 檔名：denoise.cpp
#include "denoise.h"
#include <iostream>
#include <iomanip>
using namespace std;

// -------------------------------------------------------------
// Compute blur metric: variance of Laplacian
// 若變異數越小，代表影像越模糊
// -------------------------------------------------------------
double computeBlurVariance(const Mat& src) {
    Mat gray;
    if (src.channels() == 3) {
        cvtColor(src, gray, COLOR_BGR2GRAY);
    }
    else {
        gray = src.clone();
    }
    Mat lap;
    Laplacian(gray, lap, CV_64F);
    Scalar mu, sigma;
    meanStdDev(lap, mu, sigma);
    double varianceLap = sigma.val[0] * sigma.val[0];
    return varianceLap;
}

// -------------------------------------------------------------
// Compute noise metric: standard deviation of (original_gray - blurred_gray)
// 先將灰階影像做弱 GaussianBlur，再計算差值的標準差
// -------------------------------------------------------------
double computeNoiseStd(const Mat& src) {
    Mat gray;
    if (src.channels() == 3) {
        cvtColor(src, gray, COLOR_BGR2GRAY);
    }
    else {
        gray = src.clone();
    }
    Mat blurred;
    GaussianBlur(gray, blurred, Size(3, 3), 0);

    Mat gray64, blurred64, diff;
    gray.convertTo(gray64, CV_64F);
    blurred.convertTo(blurred64, CV_64F);
    diff = gray64 - blurred64;

    Scalar mu_diff, sigma_diff;
    meanStdDev(diff, mu_diff, sigma_diff);
    double noiseStd = sigma_diff.val[0];
    return noiseStd;
}

// -------------------------------------------------------------
// Compute haze metric: simplified dark channel (僅供參考，不用於最終決策)
// 將 RGB 三通道取最小，再做局部最小值膨脹（erode）→ 得到暗通道，計算其平均值
// -------------------------------------------------------------
double computeHazeScore(const Mat& src) {
    Mat img;
    if (src.channels() == 3) {
        img = src.clone();
    }
    else {
        cvtColor(src, img, COLOR_GRAY2BGR);
    }
    vector<Mat> channels;
    split(img, channels);
    Mat minRGB = min(min(channels[0], channels[1]), channels[2]);

    int patchSize = 15;
    Mat kernel = getStructuringElement(MORPH_RECT, Size(patchSize, patchSize));
    Mat darkChannel;
    erode(minRGB, darkChannel, kernel);

    Scalar meanDark = mean(darkChannel);
    double hazeScore = meanDark.val[0] / 255.0;
    return hazeScore;
}

// -------------------------------------------------------------
// processDenoiseVideo：對整支影片做 Non-Local Means 去噪
//   cap:       已經打開的 VideoCapture 實例（會從 frame 0 開始讀）
//   writer:    已經 open() 的 VideoWriter 實例
//   totalFrames: 總幀數（若 <=0 則不顯示進度條）
// -------------------------------------------------------------
void processDenoiseVideo(VideoCapture& cap, VideoWriter& writer, long totalFrames) {
    int barWidth = 50;
    long processedFrames = 0;
    bool canShowBar = (totalFrames > 0);
    Mat frame, denoisedFrame;

    while (true) {
        bool ret = cap.read(frame);
        if (!ret || frame.empty()) break;

        // Non-Local Means Colored 去噪
        fastNlMeansDenoisingColored(frame, denoisedFrame, 10, 10, 7, 21);
        writer.write(denoisedFrame);

        // 更新進度條
        if (canShowBar) {
            processedFrames++;
            float percent = float(processedFrames) / float(totalFrames);
            if (percent > 1.0f) percent = 1.0f;
            int pos = int(barWidth * percent);

            cout << "\r[";
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos)      cout << "=";
                else if (i == pos) cout << ">";
                else               cout << " ";
            }
            cout << "] " << setw(3) << int(percent * 100) << "%";
            cout.flush();
        }
        else {
            processedFrames++;
        }
    }
    if (canShowBar) cout << endl;
}


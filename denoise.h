// 檔名：denoise.hpp
#ifndef DENOISE_HPP
#define DENOISE_HPP

#include <opencv2/opencv.hpp>
using namespace cv;

// -------------------------------------------------------------
// 計算 Laplacian 變異數（用於判斷 Blur）
// -------------------------------------------------------------
double computeBlurVariance(const Mat& src);

// -------------------------------------------------------------
// 計算原始灰階與弱 Gaussian 平滑後差值的標準差（用於判斷 Noise）
// -------------------------------------------------------------
double computeNoiseStd(const Mat& src);

// -------------------------------------------------------------
// 計算簡化版 Dark Channel 分數（僅供參考，不做最終決策）
// -------------------------------------------------------------
double computeHazeScore(const Mat& src);

// -------------------------------------------------------------
// processDenoiseVideo：對整支影片做 Non-Local Means 去噪
//   cap:       已經打開的 VideoCapture 實例（會從 frame 0 開始讀）
//   writer:    已經 open() 的 VideoWriter 實例
//   totalFrames: 總幀數（若 <=0 則不顯示進度條）
// -------------------------------------------------------------
void processDenoiseVideo(VideoCapture& cap, VideoWriter& writer, long totalFrames);



#endif // DENOISE_HPP
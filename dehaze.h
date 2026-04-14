// 檔名：dehaze.hpp
#ifndef DEHAZE_HPP
#define DEHAZE_HPP

#include <opencv2/opencv.hpp>
using namespace cv;

class deHaze
{
public:
    int AR, AG, AB;       // 大氣光 A 的三個通道值
    Mat srcImg;           // 原始輸入影像
    Mat darkChannelImg;   // 暗通道圖
    Mat roughtT;          // 粗略透光率 t
    Mat findedT;          // 精細化後的透光率 t
    Mat deHazeImg;        // 去霧後的輸出影像

    deHaze();
    ~deHaze();

    // 載入影像並檢查格式是否正確
    bool load(const Mat& img);

    // 一通道引導濾波（灰階指導）
    void guidedFilter1Channel(const Mat& src, const Mat& guideImg, Mat& dst, int filterWindowSize, float eps = 0.001f);

    // 三通道引導濾波（RGB 聯合運算）
    void guidedFilter3Channel(const Mat& src,
        const Mat& floatB,
        const Mat& floatG,
        const Mat& floatR,
        Mat& dst,
        int filterWindowSize,
        float eps = 0.001f);

    // 各種去霧方法
    Mat hazeRemoval(int darkWindowSize, int filterWindowSize);
    Mat practicalHazeRemoval(int darkWindowSize, int filterWindowSize);
    Mat fastHazeRemoval(int darkWindowSize, int filterWindowSize);
    Mat superFastHazeRemoval(int darkWindowSize, int filterWindowSize);

private:
    inline int scaleRGB(int a);

    // 暗通道計算（取最小後做局部最小值）
    void darkChannelProcess(const Mat& src, Mat& dst, int darkWindowSize);
    void streamMinFilter(const Mat& src, Mat& dst, int darkWindowSize);
    void picMinFilter(const Mat& src, Mat& dst, int darkWindowSize);

    // 估計全域大氣光 A
    void getGlobalAtmosphericLight(const Mat& darkChannelSrc, int& Ar, int& Ag, int& Ab);

    // 估計粗略透光率
    void getRoughT(const Mat& normB, const Mat& normG, const Mat& normR, Mat& dst, int Ar, int Ag, int Ab);
    void fastRoughtT(const Mat& src, Mat& dst, int Ar, int Ag, int Ab);

    // 用 t 還原清晰影像
    void imgRecover(const Mat& src, const Mat& tFined, Mat& dst, int globalA);
    //void imgRecover(const Mat& src, const Mat& tFined, Mat& dst, int Ar, int Ag, int Ab);
};

#endif // DEHAZE_HPP
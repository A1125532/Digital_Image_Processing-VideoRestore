// 檔名：dehaze.cpp
#include "dehaze.h"
//#include <opencv2/ximgproc/edge_filter.hpp> // guidedFilter
#include <opencv2/opencv.hpp>
#include <deque>
#include <iostream>
using namespace std;

static const int BRIGHT_BETA = 60;
static const double BRIGHT_ALPHA = 3;   // >1 對比提升；<1 對比降低
//====================================================
//                  deHaze 方法實作
//====================================================

// 建構子：初始化不做事
deHaze::deHaze() { }

// 解構子：目前沒特別釋放資源
deHaze::~deHaze() { }

// 將值限制在 [0, 255]
inline int deHaze::scaleRGB(int a)
{
    if (a < 0)   return 0;
    if (a > 255) return 255;
    return a;
}

// 載入影像並檢查格式是否為 CV_8UC3（3 通道 8-bit）
bool deHaze::load(const Mat& img)
{
    if (img.empty())
    {
        cerr << "Error: failed to load empty image. Check the path.\n";
        return false;
    }
    if (img.type() != CV_8UC3)
    {
        cerr << "Error: unexpected image format. Should be 3-channel 8-bit (CV_8UC3).\n";
        return false;
    }
    this->srcImg = img.clone();
    return true;
}

// 一通道引導濾波（灰階指導）
void deHaze::guidedFilter1Channel(const Mat& src, const Mat& guideImg, Mat& dst, int filterWindowSize, float eps)
{
    Mat meanI, meanP, meanIP, covIP, meanII, varI, a, b;
    Size blurSize(filterWindowSize, filterWindowSize);

    // 計算各種統計量
    blur(guideImg, meanI, blurSize);
    blur(src, meanP, blurSize);
    blur(src.mul(guideImg), meanIP, blurSize);
    covIP = meanIP - meanI.mul(meanP);

    blur(guideImg.mul(guideImg), meanII, blurSize);
    varI = meanII - meanI.mul(meanI);

    // 計算 a, b
    a = covIP / (varI + eps);
    b = meanP - a.mul(meanI);

    blur(a, a, blurSize);
    blur(b, b, blurSize);

    dst = a.mul(guideImg) + b;
}

// 三通道引導濾波（RGB 三通道聯合運算）
void deHaze::guidedFilter3Channel(const Mat& src,
    const Mat& floatB,
    const Mat& floatG,
    const Mat& floatR,
    Mat& dst,
    int filterWindowSize,
    float eps)
{
    int Irow = src.rows, Icol = src.cols;
    Size blurSize(filterWindowSize, filterWindowSize);

    Mat meanP;
    blur(src, meanP, blurSize);

    // --- B 通道 ---
    Mat meanB, meanPB, covPB, varBB;
    blur(floatB, meanB, blurSize);
    blur(floatB.mul(src), meanPB, blurSize);
    covPB = meanPB - meanB.mul(meanP);
    blur(floatB.mul(floatB), varBB, blurSize);
    varBB = varBB - meanB.mul(meanB);

    // --- G 通道 ---
    Mat meanG, meanPG, covPG, varGG;
    blur(floatG, meanG, blurSize);
    blur(floatG.mul(src), meanPG, blurSize);
    covPG = meanPG - meanG.mul(meanP);
    blur(floatG.mul(floatG), varGG, blurSize);
    varGG = varGG - meanG.mul(meanG);

    // --- R 通道 ---
    Mat meanR, meanPR, covPR, varRR;
    blur(floatR, meanR, blurSize);
    blur(floatR.mul(src), meanPR, blurSize);
    covPR = meanPR - meanR.mul(meanP);
    blur(floatR.mul(floatR), varRR, blurSize);
    varRR = varRR - meanR.mul(meanR);

    // --- 雜項協方差 ---
    Mat varRG, varRB, varGB;
    blur(floatR.mul(floatG), varRG, blurSize); varRG = varRG - meanR.mul(meanG);
    blur(floatR.mul(floatB), varRB, blurSize); varRB = varRB - meanR.mul(meanB);
    blur(floatG.mul(floatB), varGB, blurSize); varGB = varGB - meanG.mul(meanB);

    // --- a(3-channel) & b(1-channel) ---
    Mat a = Mat::zeros(Irow, Icol, CV_32FC3);
    Mat b = Mat::zeros(Irow, Icol, CV_32FC1);

    Mat sigma(3, 3, CV_32FC1), covIp(1, 3, CV_32FC1), RGB(1, 3, CV_32FC1);
    for (int i = 0; i < Irow; i++)
    {
        const float* varRRData = varRR.ptr<float>(i);
        const float* varRGData = varRG.ptr<float>(i);
        const float* varRBData = varRB.ptr<float>(i);
        const float* varGGData = varGG.ptr<float>(i);
        const float* varGBData = varGB.ptr<float>(i);
        const float* varBBData = varBB.ptr<float>(i);
        const float* covPRData = covPR.ptr<float>(i);
        const float* covPGData = covPG.ptr<float>(i);
        const float* covPBData = covPB.ptr<float>(i);

        float* aData = a.ptr<float>(i);

        for (int j = 0; j < Icol; j++)
        {
            float* covIPData = covIp.ptr<float>(0);
            covIPData[0] = *covPRData++;
            covIPData[1] = *covPGData++;
            covIPData[2] = *covPBData++;

            float* sigmaData = sigma.ptr<float>(0);
            sigmaData[0] = *varRRData + eps; sigmaData[1] = *varRGData; sigmaData[2] = *varRBData;
            sigmaData = sigma.ptr<float>(1);
            sigmaData[0] = *varRGData; sigmaData[1] = *varGGData + eps; sigmaData[2] = *varGBData;
            sigmaData = sigma.ptr<float>(2);
            sigmaData[0] = *varRBData; sigmaData[1] = *varGBData; sigmaData[2] = *varBBData + eps;

            varRRData++; varRGData++; varGGData++; varRBData++; varGBData++; varBBData++;

            RGB = covIp * sigma.inv();
            aData[0] = RGB.ptr<float>(0)[0];
            aData[1] = RGB.ptr<float>(0)[1];
            aData[2] = RGB.ptr<float>(0)[2];
            aData += 3;
        }
    }

    vector<Mat> aChannels(3);
    split(a, aChannels);
    b = meanP
        - aChannels[0].mul(meanR)
        - aChannels[1].mul(meanG)
        - aChannels[2].mul(meanB);

    blur(aChannels[0], aChannels[0], blurSize);
    blur(aChannels[1], aChannels[1], blurSize);
    blur(aChannels[2], aChannels[2], blurSize);
    blur(b, b, blurSize);

    dst = aChannels[0].mul(floatR)
        + aChannels[1].mul(floatG)
        + aChannels[2].mul(floatB)
        + b;
}

//------------------------------------------------------------
// 暗通道計算：先取 RGB 最小，再做局部 min filter
//------------------------------------------------------------
void deHaze::darkChannelProcess(const Mat& src, Mat& dst, int darkWindowSize)
{
    int Irow = src.rows, Icol = src.cols;
    Mat minImg = Mat::zeros(Irow, Icol, CV_8UC1);

    if (src.isContinuous() && minImg.isContinuous())
    {
        Icol = Irow * Icol;
        Irow = 1;
    }

    for (int i = 0; i < Irow; i++)
    {
        const uchar* inData = src.ptr<uchar>(i);
        uchar* outData = minImg.ptr<uchar>(i);
        for (int j = 0; j < Icol; j++)
        {
            uchar v = min(inData[0], inData[1]);
            v = min(v, inData[2]);
            *outData++ = v;
            inData += 3;
        }
    }

    picMinFilter(minImg, dst, darkWindowSize);
}

//------------------------------------------------------------
// 水平滑動最小值（O(n)）→ 垂直再做一次
//------------------------------------------------------------
void deHaze::streamMinFilter(const Mat& src, Mat& dst, int darkWindowSize)
{
    int half = darkWindowSize / 2 + 1;
    int Irow = src.rows, Icol = src.cols;
    dst = 255 * Mat::ones(Irow, Icol, CV_8UC1);

    for (int i = 0; i < Irow; i++)
    {
        deque<int> L;
        const uchar* inData = src.ptr<uchar>(i);
        uchar* outData = dst.ptr<uchar>(i);

        L.push_back(0);
        for (int j = 1; j < Icol + 1; j++)
        {
            if (j >= half)
            {
                uchar minimum = *(inData + L.front());
                for (int k = 1; k < half + 1; k++)
                {
                    uchar tmp = *(outData - k);
                    if (minimum < tmp) *(outData - k) = minimum;
                }
            }

            while (!L.empty() && (j < Icol) && (*(inData + j) < *(inData + L.back())))
            {
                L.pop_back();
            }
            if (j < Icol) L.push_back(j);
            if (j == half + L.front()) L.pop_front();

            outData++;
        }
    }
}

//------------------------------------------------------------
// 垂直滑動最小值：轉置後再呼叫水平，再轉置回來
//------------------------------------------------------------
void deHaze::picMinFilter(const Mat& src, Mat& dst, int darkWindowSize)
{
    Mat tmp;
    streamMinFilter(src, tmp, darkWindowSize);
    streamMinFilter(tmp.t(), dst, darkWindowSize);
    dst = dst.t();
}

//------------------------------------------------------------
// 估計全域大氣光：暗通道排名前 0.1% 的像素，取原影像的最大 RGB
//------------------------------------------------------------
void deHaze::getGlobalAtmosphericLight(const Mat& darkChannelSrc, int& Ar, int& Ag, int& Ab)
{
    int Irow = darkChannelSrc.rows, Icol = darkChannelSrc.cols;
    vector<int> BucketSort(256, 0);

    if (darkChannelSrc.isContinuous() && srcImg.isContinuous())
    {
        Icol = Irow * Icol;
        Irow = 1;
    }

    for (int i = 0; i < Irow; i++)
    {
        const uchar* inData = darkChannelSrc.ptr<uchar>(i);
        for (int j = 0; j < Icol; j++)
        {
            BucketSort[*inData++]++;
        }
    }

    int totalPixels = Irow * Icol;
    int thresholdCount = static_cast<int>(0.001 * totalPixels);
    int sum = 0, AThreshold = 255;
    for (int i = 255; i >= 0; i--)
    {
        sum += BucketSort[i];
        if (sum >= thresholdCount)
        {
            AThreshold = i;
            break;
        }
    }

    Ar = Ag = Ab = 0;
    for (int i = 0; i < Irow; i++)
    {
        const uchar* inData = srcImg.ptr<uchar>(i);
        const uchar* indexData = darkChannelSrc.ptr<uchar>(i);
        for (int j = 0; j < Icol; j++)
        {
            if (*indexData++ < AThreshold)
            {
                Ab = max(Ab, static_cast<int>(*inData));
                Ag = max(Ag, static_cast<int>(*(inData + 1)));
                Ar = max(Ar, static_cast<int>(*(inData + 2)));
            }
            inData += 3;
        }
    }
}

//------------------------------------------------------------
// 估計粗略透光率 t(x) = 1 - ω·min_{c}(I_c(x)/A_c)
//------------------------------------------------------------
void deHaze::getRoughT(const Mat& normB, const Mat& normG, const Mat& normR, Mat& dst, int Ar, int Ag, int Ab)
{
    int Irow = normB.rows, Icol = normB.cols;
    dst = Mat::zeros(Irow, Icol, CV_32FC1);
    float Arr = static_cast<float>(Ar), Agg = static_cast<float>(Ag), Abb = static_cast<float>(Ab);

    for (int i = 0; i < Irow; i++)
    {
        const uchar* bData = normB.ptr<uchar>(i);
        const uchar* gData = normG.ptr<uchar>(i);
        const uchar* rData = normR.ptr<uchar>(i);
        float* tData = dst.ptr<float>(i);

        for (int j = 0; j < Icol; j++)
        {
            float minRatio = min(*bData / Abb, *gData / Agg);
            minRatio = min(*rData / Arr, minRatio);
            *tData++ = max(0.0f, 1.0f - 0.95f * minRatio); // ω = 0.95
            bData++; gData++; rData++;
        }
    }
}

//------------------------------------------------------------
// 快速估計透光率（僅用暗通道圖做估計），不做局部最小值
//------------------------------------------------------------
void deHaze::fastRoughtT(const Mat& src, Mat& dst, int Ar, int Ag, int Ab)
{
    int Irow = src.rows, Icol = src.cols;
    dst = Mat::zeros(Irow, Icol, CV_32FC1);
    float Ac = (Ar + Ag + Ab) / 3.0f;

    for (int i = 0; i < Irow; i++)
    {
        const uchar* inData = src.ptr<uchar>(i);
        float* outData = dst.ptr<float>(i);
        for (int j = 0; j < Icol; j++)
        {
            *outData++ = 1.0f - 0.95f * (*inData++) / Ac;
        }
    }
}

//------------------------------------------------------------
// 用 tFined 還原清晰影像：J(x) = (I(x)-A)/max(t(x), t0) + A
//------------------------------------------------------------
void deHaze::imgRecover(const Mat& src, const Mat& tFined, Mat& dst, int globalA)
{
    int Irow = src.rows, Icol = src.cols;
    const float t0 = 0.1f;
    dst = Mat::zeros(Irow, Icol, CV_8UC3);

    for (int i = 0; i < Irow; i++)
    {
        const uchar* inData = src.ptr<uchar>(i);
        const float* tData = tFined.ptr<float>(i);
        uchar* outData = dst.ptr<uchar>(i);

        for (int j = 0; j < Icol; j++)
        {
            float tVal = max(*tData++, t0);
            outData[0] = static_cast<uchar>(scaleRGB(static_cast<int>((*(inData)-globalA) / tVal + globalA)));
            inData++;
            outData[1] = static_cast<uchar>(scaleRGB(static_cast<int>((*(inData)-globalA) / tVal + globalA)));
            inData++;
            outData[2] = static_cast<uchar>(scaleRGB(static_cast<int>((*(inData)-globalA) / tVal + globalA)));
            inData++;
            outData += 3;
        }
    }
}

//====================================================
//                    各種去霧方法實作
//====================================================

// Practical haze removal (one-channel guided filter using grayscale)
Mat deHaze::practicalHazeRemoval(int darkWindowSize, int filterWindowSize)
{
    vector<Mat> channels(3);
    split(srcImg, channels);

    Mat NormB, NormG, NormR;
    Mat floatB, floatG, floatR, floatGuideGray;

    // 步驟 1：計算暗通道圖 & 大氣光 A
    darkChannelProcess(srcImg, darkChannelImg, darkWindowSize);
    getGlobalAtmosphericLight(darkChannelImg, AR, AG, AB);

    // 步驟 2：對 B, G, R 通道做局部最小值 + 轉浮點
    picMinFilter(channels[0], NormB, darkWindowSize);
    channels[0].convertTo(floatB, CV_32FC1, 0.0039215686f);
    picMinFilter(channels[1], NormG, darkWindowSize);
    channels[1].convertTo(floatG, CV_32FC1, 0.0039215686f);
    picMinFilter(channels[2], NormR, darkWindowSize);
    channels[2].convertTo(floatR, CV_32FC1, 0.0039215686f);

    // 步驟 3：估計粗略透光率
    getRoughT(NormB, NormG, NormR, roughtT, AR, AG, AB);

    // 步驟 4：以灰階當指導做一通道 Guided Filter
    {
        Mat grayFloat;
        srcImg.convertTo(grayFloat, CV_32FC1, 0.0039215686f);
        cvtColor(grayFloat, floatGuideGray, COLOR_BGR2GRAY);
    }
    guidedFilter1Channel(roughtT, floatGuideGray, findedT, filterWindowSize);

    // 步驟 5：還原清晰影像
    int globalA = max(AR, max(AG, AB));
    imgRecover(srcImg, findedT, deHazeImg, globalA);
    if (BRIGHT_BETA != 0) {
        deHazeImg.convertTo(deHazeImg, -1, BRIGHT_ALPHA, BRIGHT_BETA);
    }
    return deHazeImg;
}


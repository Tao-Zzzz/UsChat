#ifndef FACEAUTHMGR_H
#define FACEAUTHMGR_H

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <QString>

class FaceAuthMgr
{
public:
    static FaceAuthMgr* GetInstance() {
        static FaceAuthMgr instance;
        return &instance;
    }

    // 初始化加载 AI 模型
    bool Init(const std::string& det_model_path, const std::string& rec_model_path);

    // 从图像中提取人脸特征 (512维向量)
    cv::Mat ExtractFeature(const cv::Mat& image);

    // 比对两个人脸特征，返回相似度 (0.0 ~ 1.0)
    float Match(const cv::Mat& feat1, const cv::Mat& feat2);

private:
    FaceAuthMgr() = default;
    ~FaceAuthMgr() = default;

    cv::Ptr<cv::FaceDetectorYN> m_detector;     // 人脸检测器
    cv::Ptr<cv::FaceRecognizerSF> m_recognizer; // 人脸特征提取器
};

#endif // FACEAUTHMGR_H

#include "FaceAuthMgr.h"
#include <QDebug>

bool FaceAuthMgr::Init(const std::string& det_model_path, const std::string& rec_model_path)
{
    try {
        // 创建 YuNet 检测器，初始尺寸设为 320x320
        m_detector = cv::FaceDetectorYN::create(det_model_path, "", cv::Size(320, 320));
        // 创建 SFace 识别器
        m_recognizer = cv::FaceRecognizerSF::create(rec_model_path, "");
        return true;
    } catch (const cv::Exception& e) {
        qDebug() << "AI Model Init Failed: " << e.what();
        return false;
    }
}

cv::Mat FaceAuthMgr::ExtractFeature(const cv::Mat& image)
{
    cv::Mat feature;
    if (image.empty() || m_detector.empty() || m_recognizer.empty()) {
        return feature;
    }

    // 1. 动态设置输入图像尺寸
    m_detector->setInputSize(image.size());

    // 2. 检测人脸
    cv::Mat faces;
    m_detector->detect(image, faces);

    // 如果没有检测到人脸，直接返回空特征
    if (faces.rows < 1) {
        return feature;
    }

    // 3. 截取最大/最中心的人脸并对其 (Face Alignment)
    cv::Mat aligned_face;
    m_recognizer->alignCrop(image, faces.row(0), aligned_face);

    // 4. 提取特征向量 (Feature Extraction)
    m_recognizer->feature(aligned_face, feature);

    return feature; // 返回 1x128 浮点数矩阵
}

float FaceAuthMgr::Match(const cv::Mat& feat1, const cv::Mat& feat2)
{
    if (feat1.empty() || feat2.empty()) return 0.0f;
    // 使用余弦相似度比对 (Cosine Similarity)
    return m_recognizer->match(feat1, feat2, cv::FaceRecognizerSF::DisType::FR_COSINE);
}

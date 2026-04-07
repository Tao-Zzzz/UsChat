#include "FaceAuthMgr.h"
#include <QDebug>
#include <QJsonArray>


QJsonArray MatToJsonArray(const cv::Mat& mat) {
    QJsonArray arr;
    // SFace 返回的特征通常是连续的 CV_32F (float) 类型矩阵
    if (mat.isContinuous() && mat.type() == CV_32F) {
        const float* ptr = mat.ptr<float>();
        for (int i = 0; i < mat.cols * mat.rows; ++i) {
            arr.append(static_cast<double>(ptr[i]));
        }
    }
    return arr;
}

bool FaceAuthMgr::Init(const std::string& det_model_path,
                       const std::string& rec_model_path,
                       const std::string& liveness_model_path)
{
    try {
        m_detector = cv::FaceDetectorYN::create(det_model_path, "", cv::Size(320, 320));
        m_recognizer = cv::FaceRecognizerSF::create(rec_model_path, "");

        // ================= 新增：加载活体检测 ONNX 模型 =================
        if (!liveness_model_path.empty()) {
            m_livenessNet = cv::dnn::readNetFromONNX(liveness_model_path);
            if (m_livenessNet.empty()) {
                qDebug() << "Liveness Model loaded failed!";
                return false;
            }
        }
        // ================================================================
        return true;
    } catch (const cv::Exception& e) {
        qDebug() << "AI Model Init Failed: " << e.what();
        return false;
    }
}

// ================= 新增：活体检测核心逻辑 =================
float FaceAuthMgr::CheckLiveness(const cv::Mat& image)
{
    if (image.empty() || m_detector.empty() || m_livenessNet.empty()) {
        return 0.0f; // 模型未初始化或图像空，默认不通过
    }

    // 1. 检测人脸获取边界框
    m_detector->setInputSize(image.size());
    cv::Mat faces;
    m_detector->detect(image, faces);

    if (faces.rows < 1) {
        return 0.0f; // 没脸
    }

    // 2. 裁剪人脸区域 (增加 BBox 放大逻辑)
    float x = faces.at<float>(0, 0);
    float y = faces.at<float>(0, 1);
    float w = faces.at<float>(0, 2);
    float h = faces.at<float>(0, 3);

    // 计算人脸中心点
    float center_x = x + w / 2.0f;
    float center_y = y + h / 2.0f;

    // 设置放大倍数，通常活体检测模型推荐 2.7 或 4.0
    float scale = 2.7f;

    // 因为通常活体模型的输入是正方形，我们取宽高的最大值作为基准
    float side = std::max(w, h) * scale;

    // 计算新的左上角坐标
    int crop_x = static_cast<int>(center_x - side / 2.0f);
    int crop_y = static_cast<int>(center_y - side / 2.0f);
    int crop_w = static_cast<int>(side);
    int crop_h = static_cast<int>(side);

    // 严谨的越界保护：如果越界，补充黑边(padding)是更好的做法，
    // 但为了简单，这里直接做截断操作 (注意截断可能会稍微改变宽高比)
    int safe_x = std::max(0, crop_x);
    int safe_y = std::max(0, crop_y);
    int safe_w = std::min(image.cols - safe_x, crop_x + crop_w - safe_x);
    int safe_h = std::min(image.rows - safe_y, crop_y + crop_h - safe_y);

    cv::Rect faceRect(safe_x, safe_y, safe_w, safe_h);
    if (faceRect.area() <= 0) return 0.0f;

    cv::Mat faceCrop = image(faceRect);

    // 3. 构建 DNN 输入 Blob
    // 【注意】：这里的 Size(80, 80) 和 Scale (1.0/255.0) 必须与你实际下载的 ONNX 模型要求完全一致！
    // 很多 MiniFASNet 的输入是 80x80，并且做了 RGB 归一化。
    cv::Mat blob = cv::dnn::blobFromImage(faceCrop, 1.0 / 255.0, cv::Size(80, 80),
                                          cv::Scalar(0, 0, 0), true, false);

    // 4. 前向传播推理
    m_livenessNet.setInput(blob);
    cv::Mat prob = m_livenessNet.forward();

    // 5. 解析输出结果 (增加 Softmax 处理)
    if (prob.cols >= 2) {
        // 获取原始 Logits (通常 MiniFASNet 输出: 0是假，1是真。请根据你的模型确认)
        float logit_fake = prob.at<float>(0, 0);
        float logit_live = prob.at<float>(0, 1);

        // 为了防止数值溢出，计算 Softmax 前通常先减去最大值
        float max_val = std::max(logit_fake, logit_live);
        float exp_fake = std::exp(logit_fake - max_val);
        float exp_live = std::exp(logit_live - max_val);

        // 计算真人概率 (0.0 ~ 1.0)
        float live_probability = exp_live / (exp_fake + exp_live);

        qDebug() << "Logits [" << logit_fake << ", " << logit_live
                 << "] -> Prob:" << live_probability;

        return live_probability;
    } else if (prob.cols == 1) {
        // 如果模型是单输出，通常需要过一个 Sigmoid 函数
        float logit = prob.at<float>(0, 0);
        float live_probability = 1.0f / (1.0f + std::exp(-logit));
        return live_probability;
    }

    return 0.0f;
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

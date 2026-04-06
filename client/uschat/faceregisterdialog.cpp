#include "faceregisterdialog.h"
#include <QMessageBox>
#include <QSettings>
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include "httpmgr.h"

FaceRegisterDialog::FaceRegisterDialog(int uid, QWidget *parent)
    : QDialog(parent), m_uid(uid)
{
    // 1. 窗口基础设置
    this->setWindowTitle("录入人脸");
    this->setFixedSize(450, 450);

    // 2. 界面布局初始化
    m_videoLabel = new QLabel(this);
    m_videoLabel->setFixedSize(400, 300);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setStyleSheet("background-color: black; color: white;");
    m_videoLabel->setText("正在打开摄像头...");

    m_captureBtn = new QPushButton("拍照并绑定", this);
    m_cancelBtn = new QPushButton("取消", this);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(m_captureBtn);
    btnLayout->addWidget(m_cancelBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_videoLabel, 0, Qt::AlignCenter);
    mainLayout->addLayout(btnLayout);

    // 3. 信号槽绑定
    connect(m_captureBtn, &QPushButton::clicked, this, &FaceRegisterDialog::onCaptureClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 4. 打开摄像头
    if (!m_cap.open(0)) {
        m_videoLabel->setText("无法打开摄像头！请检查设备。");
        m_captureBtn->setEnabled(false);
    } else {
        // 使用 QTimer 每 30ms (约 33帧/秒) 刷新一次画面，防止主线程卡死
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &FaceRegisterDialog::updateFrame);
        m_timer->start(30);
    }
}

FaceRegisterDialog::~FaceRegisterDialog()
{
    // 释放摄像头和定时器
    if (m_cap.isOpened()) m_cap.release();
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
    }
}



// 刷新画面
void FaceRegisterDialog::updateFrame()
{
    m_cap >> m_currentFrame;
    if (m_currentFrame.empty()) return;

    // OpenCV 默认是 BGR 格式，而 Qt 显示需要 RGB 格式，所以要做转换
    cv::Mat rgbFrame;
    cv::cvtColor(m_currentFrame, rgbFrame, cv::COLOR_BGR2RGB);

    // 将 cv::Mat 转为 QImage
    QImage img(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
    // 缩放并显示在 QLabel 上
    m_videoLabel->setPixmap(QPixmap::fromImage(img).scaled(m_videoLabel->size(), Qt::KeepAspectRatio));
}

extern QJsonArray MatToJsonArray(const cv::Mat& mat);

// 点击拍照录入
void FaceRegisterDialog::onCaptureClicked()
{
    if (m_currentFrame.empty()) return;

    // 先暂停定时器，定格画面，让用户知道拍下来了
    m_timer->stop();

    // 1. AI 提取特征
    cv::Mat feature = FaceAuthMgr::GetInstance()->ExtractFeature(m_currentFrame);
    if (feature.empty()) {
        QMessageBox::warning(this, "提示", "未检测到人脸，请重试！");
        m_timer->start(30);
        return;
    }

    // 2. 转换数据并组装 JSON
    QJsonArray featureArray = MatToJsonArray(feature);
    QJsonObject jsonObj;
    jsonObj["uid"] = m_uid; // 传入进来的用户 UID
    jsonObj["feature"] = featureArray;

    // 3. 发送给 Python AI 微服务 (注意端口是 8000)
    HttpMgr::GetInstance()->PostHttpReq(QUrl("http://127.0.0.1:8010/api/face/register"),
                                        jsonObj,
                                        ReqId::ID_FACE_REGISTER,
                                        Modules::LOGINMOD); // 借用 LOGINMOD 模块发送

    QMessageBox::information(this, "成功", "人脸特征正在上传云端绑定！");
    this->accept();
}

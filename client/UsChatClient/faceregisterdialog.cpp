#include "faceregisterdialog.h"
#include <QMessageBox>
#include <QSettings>
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include "httpmgr.h"
#include "faceauthmgr.h"

extern QJsonArray MatToJsonArray(const cv::Mat& mat);

FaceRegisterDialog::FaceRegisterDialog(int uid, QWidget *parent)
    : QDialog(parent), m_uid(uid)
{
    // 1. 窗口基础设置
    this->setWindowTitle("录入人脸");
    this->setFixedSize(450, 480); // 稍微加高一点容纳新按钮

    // 2. 界面布局初始化
    m_videoLabel = new QLabel(this);
    m_videoLabel->setFixedSize(400, 300);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setStyleSheet("background-color: black; color: white;");
    m_videoLabel->setText("正在打开摄像头...");

    m_captureBtn = new QPushButton("拍照", this);
    m_retakeBtn = new QPushButton("重拍", this);
    m_confirmBtn = new QPushButton("确认绑定", this);
    m_cancelBtn = new QPushButton("取消", this);

    // 初始状态下隐藏“重拍”和“确认”
    m_retakeBtn->hide();
    m_confirmBtn->hide();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(m_captureBtn);
    btnLayout->addWidget(m_retakeBtn);
    btnLayout->addWidget(m_confirmBtn);
    btnLayout->addWidget(m_cancelBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_videoLabel, 0, Qt::AlignCenter);
    mainLayout->addLayout(btnLayout);

    // 3. 信号槽绑定
    connect(m_captureBtn, &QPushButton::clicked, this, &FaceRegisterDialog::onCaptureClicked);
    connect(m_retakeBtn, &QPushButton::clicked, this, &FaceRegisterDialog::onRetakeClicked);
    connect(m_confirmBtn, &QPushButton::clicked, this, &FaceRegisterDialog::onConfirmClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 4. 打开摄像头
    if (!m_cap.open(0)) {
        m_videoLabel->setText("无法打开摄像头！请检查设备。");
        m_captureBtn->setEnabled(false);
    } else {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &FaceRegisterDialog::updateFrame);
        m_timer->start(30);
    }
}

FaceRegisterDialog::~FaceRegisterDialog()
{
    if (m_cap.isOpened()) m_cap.release();
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
    }
}

void FaceRegisterDialog::updateFrame()
{
    m_cap >> m_currentFrame;
    if (m_currentFrame.empty()) return;

    cv::Mat rgbFrame;
    cv::cvtColor(m_currentFrame, rgbFrame, cv::COLOR_BGR2RGB);

    QImage img(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
    m_videoLabel->setPixmap(QPixmap::fromImage(img).scaled(m_videoLabel->size(), Qt::KeepAspectRatio));
}

// 点击拍照：定格画面，展示新按钮
void FaceRegisterDialog::onCaptureClicked()
{
    if (m_currentFrame.empty()) return;

    m_timer->stop(); // 定格画面

    m_captureBtn->hide();
    m_retakeBtn->show();
    m_confirmBtn->show();
}

// 点击重拍：恢复画面刷新
void FaceRegisterDialog::onRetakeClicked()
{
    m_retakeBtn->hide();
    m_confirmBtn->hide();
    m_captureBtn->show();

    m_timer->start(30); // 恢复刷新
}

// 点击确认绑定：提取特征并上传
void FaceRegisterDialog::onConfirmClicked()
{
    // 1. AI 提取特征
    cv::Mat feature = FaceAuthMgr::GetInstance()->ExtractFeature(m_currentFrame);
    if (feature.empty()) {
        QMessageBox::warning(this, "提示", "未检测到清晰的人脸，请重拍！");
        onRetakeClicked(); // 自动回到重拍状态
        return;
    }

    // 2. 转换数据并上传
    QJsonArray featureArray = MatToJsonArray(feature);
    QJsonObject jsonObj;
    jsonObj["uid"] = m_uid;
    jsonObj["feature"] = featureArray;

    HttpMgr::GetInstance()->PostHttpReq(QUrl("http://127.0.0.1:8010/api/face/register"),
                                        jsonObj,
                                        ReqId::ID_FACE_REGISTER,
                                        Modules::LOGINMOD);

    QMessageBox::information(this, "成功", "人脸特征正在上传云端绑定！");
    this->accept();
}

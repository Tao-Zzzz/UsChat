#ifndef FACEREGISTERDIALOG_H
#define FACEREGISTERDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include "FaceAuthMgr.h" // 引入我们上一轮写的 AI 管理类

class FaceRegisterDialog : public QDialog
{
    Q_OBJECT
public:
    // 构造时需要传入当前用户的账号和密码，以便绑定人脸
    explicit FaceRegisterDialog(int uid, QWidget *parent = nullptr);
    ~FaceRegisterDialog();

private slots:
    void updateFrame();        // 刷新摄像头画面的槽函数
    void onCaptureClicked();   // 点击“拍照录入”的槽函数
    void onRetakeClicked();
    void onConfirmClicked();
private:
    QPushButton *m_retakeBtn; //重取
    QPushButton *m_confirmBtn; // 确定

    QLabel *m_videoLabel;      // 显示视频画面的控件
    QPushButton *m_captureBtn; // 拍照按钮
    QPushButton *m_cancelBtn;  // 取消按钮

    cv::VideoCapture m_cap;    // OpenCV 摄像头对象
    QTimer *m_timer;           // 定时器，用于定时抓取画面
    cv::Mat m_currentFrame;    // 保存当前帧

    int m_uid; // 【修改点】：保存待绑定的 uid
};

#endif // FACEREGISTERDIALOG_H

#include "PictureBubble.h"
#include "ImagePreviewDialog.h"

#include <QLabel>
#include <QTimer>
#include <QStyle>
#include <QFileInfo>

#define PIC_MAX_WIDTH 160
#define PIC_MAX_HEIGHT 90

PictureBubble::PictureBubble(const QPixmap &picture, ChatRole role, int total, QWidget *parent)
    : BubbleFrame(role, parent), m_state(TransferState::None), m_total_size(total)
{
    // 加载图标
    m_pauseIcon = style()->standardIcon(QStyle::SP_MediaPause);
    m_playIcon = style()->standardIcon(QStyle::SP_MediaPlay);
    m_downloadIcon = style()->standardIcon(QStyle::SP_ArrowDown);

    m_originalPixmap = picture;

    // 创建容器
    QWidget* container = new QWidget();
    m_vLayout = new QVBoxLayout(container);
    m_vLayout->setContentsMargins(0, 0, 0, 0);
    m_vLayout->setSpacing(5);

    // 创建可点击的图片标签
    m_picLabel = new ClickableLabel();
    m_picLabel->setScaledContents(true);
    QPixmap pix = picture.scaled(QSize(PIC_MAX_WIDTH, PIC_MAX_HEIGHT),
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_pixmapSize = pix.size();
    m_picLabel->setPixmap(pix);
    m_picLabel->setFixedSize(pix.size());

    connect(m_picLabel, &ClickableLabel::clicked,
            this, &PictureBubble::onPictureClicked);

    // 创建进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setFixedWidth(pix.width());
    m_progressBar->setFixedHeight(10);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    setState(TransferState::None);

    // 样式美化
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "   border: 1px solid #ccc;"
        "   border-radius: 3px;"
        "   text-align: center;"
        "   background-color: #f0f0f0;"
        "   font-size: 10px;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "       stop:0 #4CAF50, stop:1 #45a049);"
        "   border-radius: 2px;"
        "}"
        );

    m_vLayout->addWidget(m_picLabel);
    m_vLayout->addWidget(m_progressBar);
    this->setWidget(container);
    adjustSize();
}

void PictureBubble::adjustSize()
{
    int left_margin = this->layout()->contentsMargins().left();
    int right_margin = this->layout()->contentsMargins().right();
    int v_margin = this->layout()->contentsMargins().bottom();

    int width = m_pixmapSize.width() + left_margin + right_margin;
    int height = m_pixmapSize.height() + v_margin * 2;

    if (m_progressBar->isHidden() == false) {
        height += m_progressBar->height() + m_vLayout->spacing();
    }

    setFixedSize(width, height);
}

void PictureBubble::setProgress(int value, int total_value)
{
    if (m_total_size != total_value) {
        m_total_size = total_value;
    }

    if (m_total_size <= 0) {
        m_progressBar->setValue(0);
        return;
    }

    float percent = (value / (m_total_size * 1.0)) * 100.0f;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    m_progressBar->setValue(static_cast<int>(percent));
    if (percent >= 100) {
        setState(TransferState::Completed);
    }
}

void PictureBubble::showProgress(bool show)
{
    if (show) {
        m_progressBar->show();
    } else {
        m_progressBar->hide();
    }

    adjustSize();
}

void PictureBubble::resumeState()
{
    if (_msg_info->_transfer_type == TransferType::Download) {
        _msg_info->_transfer_state = TransferState::Downloading;
        m_state = TransferState::Downloading;
        updateIconOverlay();
        return;
    }

    if (_msg_info->_transfer_type == TransferType::Upload) {
        _msg_info->_transfer_state = TransferState::Uploading;
        m_state = TransferState::Uploading;
        updateIconOverlay();
        return;
    }
}

void PictureBubble::setState(TransferState state)
{
    m_state = state;
    if (_msg_info) {
        _msg_info->_transfer_state = state;
    }

    switch (state) {
    case TransferState::Downloading:
    case TransferState::Uploading:
    case TransferState::Paused:
        showProgress(true);
        break;

    case TransferState::Completed:
        QTimer::singleShot(1000, this, [this]() {
            showProgress(false);
        });
        break;

    case TransferState::None:
    case TransferState::Failed:
        showProgress(false);
        break;
    }

    updateIconOverlay();
}

void PictureBubble::setMsgInfo(std::shared_ptr<MsgInfo> msg)
{
    _msg_info = msg;
    setProgress(_msg_info->_current_size, _msg_info->_total_size);

    if (_msg_info->_transfer_state == TransferState::Uploading) {
        setState(TransferState::Uploading);
        return;
    }

    if (_msg_info->_transfer_state == TransferState::Downloading) {
        setState(TransferState::Downloading);
        return;
    }

    if (_msg_info->_transfer_state == TransferState::Completed) {
        setState(TransferState::Completed);
        return;
    }

    if (_msg_info->_transfer_state == TransferState::Paused) {
        setState(TransferState::Paused);
        return;
    }
}

void PictureBubble::setDownloadFinish(std::shared_ptr<MsgInfo> msg, QString file_path)
{
    _msg_info = msg;
    m_localFilePath = file_path;

    m_progressBar->setValue(100);
    setState(TransferState::Completed);

    QPixmap picture(file_path);
    if (!picture.isNull()) {
        m_originalPixmap = picture;
        QPixmap pix = picture.scaled(QSize(PIC_MAX_WIDTH, PIC_MAX_HEIGHT),
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_pixmapSize = pix.size();
        m_picLabel->setPixmap(pix);
        m_picLabel->setFixedSize(pix.size());
        adjustSize();
    }

    updateIconOverlay();
}

void PictureBubble::setUploadFinish(QString file_path)
{
    m_localFilePath = file_path;

    QPixmap picture(file_path);
    if (!picture.isNull()) {
        m_originalPixmap = picture;
        QPixmap pix = picture.scaled(QSize(PIC_MAX_WIDTH, PIC_MAX_HEIGHT),
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_pixmapSize = pix.size();
        m_picLabel->setPixmap(pix);
        m_picLabel->setFixedSize(pix.size());
        adjustSize();
    }

    setState(TransferState::Completed);
    updateIconOverlay();
}

void PictureBubble::showPreviewDialog()
{
    if (!m_previewDialog) {
        m_previewDialog = new ImagePreviewDialog(this);
    }

    if (!m_localFilePath.isEmpty() && QFileInfo::exists(m_localFilePath)) {
        m_previewDialog->setImage(m_localFilePath);
    } else if (!m_originalPixmap.isNull()) {
        m_previewDialog->setImage(m_originalPixmap, m_localFilePath);
    } else {
        return;
    }

    m_previewDialog->show();
    m_previewDialog->raise();
    m_previewDialog->activateWindow();
}

void PictureBubble::onPictureClicked()
{
    if (_msg_info == nullptr) {
        // 没消息信息时，若已有图片，也允许预览
        if (m_state == TransferState::Completed || !m_originalPixmap.isNull()) {
            showPreviewDialog();
        }
        return;
    }

    switch (m_state) {
    case TransferState::Downloading:
    case TransferState::Uploading:
        setState(TransferState::Paused);
        emit pauseRequested(_msg_info->_unique_name, _msg_info->_transfer_type);
        break;

    case TransferState::Paused:
        resumeState();
        emit resumeRequested(_msg_info->_unique_name, _msg_info->_transfer_type);
        break;

    case TransferState::Failed:
        emit resumeRequested(_msg_info->_unique_name, _msg_info->_transfer_type);
        break;

    case TransferState::Completed:
        showPreviewDialog();
        break;

    default:
        // 其他状态默认也尝试预览
        showPreviewDialog();
        break;
    }
}

void PictureBubble::updateIconOverlay()
{
    switch (m_state) {
    case TransferState::Downloading:
    case TransferState::Uploading:
        m_picLabel->setIconOverlay(m_pauseIcon);
        m_picLabel->showIconOverlay(true);
        break;

    case TransferState::Paused:
        m_picLabel->setIconOverlay(m_playIcon);
        m_picLabel->showIconOverlay(true);
        break;

    case TransferState::Failed:
        m_picLabel->setIconOverlay(m_downloadIcon);
        m_picLabel->showIconOverlay(true);
        break;

    default:
        m_picLabel->showIconOverlay(false);
        break;
    }
}

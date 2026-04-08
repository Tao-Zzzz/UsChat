#include "ImagePreviewDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QTransform>

ScribbleLabel::ScribbleLabel(QWidget *parent)
    : QLabel(parent),
    m_scribbleEnabled(false),
    m_drawing(false)
{
    setMouseTracking(true);
}

void ScribbleLabel::setBaseImage(const QImage &img)
{
    m_baseImage = img;
    if (m_overlay.size() != img.size()) {
        m_overlay = QImage(img.size(), QImage::Format_ARGB32_Premultiplied);
        m_overlay.fill(Qt::transparent);
    }
    update();
}

void ScribbleLabel::setDisplayPixmap(const QPixmap &pixmap)
{
    QLabel::setPixmap(pixmap);
    resize(pixmap.size());
    update();
}

void ScribbleLabel::setScribbleEnabled(bool enabled)
{
    m_scribbleEnabled = enabled;
}

void ScribbleLabel::clearOverlay()
{
    if (!m_overlay.isNull()) {
        m_overlay.fill(Qt::transparent);
        update();
    }
}

bool ScribbleLabel::hasOverlay() const
{
    if (m_overlay.isNull()) return false;

    for (int y = 0; y < m_overlay.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(m_overlay.constScanLine(y));
        for (int x = 0; x < m_overlay.width(); ++x) {
            if (qAlpha(line[x]) != 0) {
                return true;
            }
        }
    }
    return false;
}

QImage ScribbleLabel::mergeToImage(const QImage &baseImage) const
{
    if (baseImage.isNull()) return baseImage;

    QImage out = baseImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawImage(0, 0, m_overlay);
    painter.end();
    return out;
}

QPoint ScribbleLabel::mapToImagePoint(const QPoint &pos) const
{
    // 在 Qt 6 中，pixmap() 直接返回 QPixmap 对象，使用 .isNull() 判断即可
    if (pixmap().isNull() || m_baseImage.isNull()) {
        return QPoint(-1, -1);
    }

    QSize shownSize = pixmap().size(); // 使用 . 访问成员
    if (shownSize.width() <= 0 || shownSize.height() <= 0) {
        return QPoint(-1, -1);
    }

    double scaleX = m_baseImage.width() * 1.0 / shownSize.width();
    double scaleY = m_baseImage.height() * 1.0 / shownSize.height();

    int x = int(pos.x() * scaleX);
    int y = int(pos.y() * scaleY);

    if (x < 0 || x >= m_baseImage.width() || y < 0 || y >= m_baseImage.height()) {
        return QPoint(-1, -1);
    }

    return QPoint(x, y);
}

void ScribbleLabel::mousePressEvent(QMouseEvent *event)
{
    if (m_scribbleEnabled && event->button() == Qt::LeftButton) {
        QPoint imgPt = mapToImagePoint(event->pos());
        if (imgPt.x() >= 0) {
            m_drawing = true;
            m_lastPoint = imgPt;
            event->accept();
            return;
        }
    }
    QLabel::mousePressEvent(event);
}

void ScribbleLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_scribbleEnabled && m_drawing && (event->buttons() & Qt::LeftButton)) {
        QPoint imgPt = mapToImagePoint(event->pos());
        if (imgPt.x() >= 0) {
            QPainter painter(&m_overlay);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(QColor(255, 0, 0), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawLine(m_lastPoint, imgPt);
            painter.end();

            m_lastPoint = imgPt;
            update();
            event->accept();
            return;
        }
    }
    QLabel::mouseMoveEvent(event);
}

void ScribbleLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_scribbleEnabled && m_drawing && event->button() == Qt::LeftButton) {
        QPoint imgPt = mapToImagePoint(event->pos());
        if (imgPt.x() >= 0) {
            QPainter painter(&m_overlay);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(QColor(255, 0, 0), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawLine(m_lastPoint, imgPt);
            painter.end();
        }

        m_drawing = false;
        update();
        emit scribbleFinished();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

void ScribbleLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event); // 先让 QLabel 绘制原本的图片

    // 在 Qt 6 中判断 pixmap 是否有效
    if (!m_overlay.isNull() && !pixmap().isNull()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawImage(rect(), m_overlay); // 把涂鸦层覆盖上去
    }
}

ImagePreviewDialog::ImagePreviewDialog(QWidget *parent)
    : QDialog(parent),
    m_imageLabel(nullptr),
    m_scrollArea(nullptr),
    m_zoomInBtn(nullptr),
    m_zoomOutBtn(nullptr),
    m_normalBtn(nullptr),
    m_fitBtn(nullptr),
    m_rotateLeftBtn(nullptr),
    m_rotateRightBtn(nullptr),
    m_flipBtn(nullptr),
    m_grayBtn(nullptr),
    m_invertBtn(nullptr),
    m_binaryBtn(nullptr),
    m_mosaicBtn(nullptr),
    m_blurBtn(nullptr),
    m_sharpenBtn(nullptr),
    m_scribbleBtn(nullptr),
    m_clearScribbleBtn(nullptr),
    m_resetBtn(nullptr),
    m_restoreOriginalBtn(nullptr),
    m_copyBtn(nullptr),
    m_saveBtn(nullptr),
    m_brightnessSlider(nullptr),
    m_contrastSlider(nullptr),
    m_scaleFactor(1.0),
    m_fitWindowMode(true),
    m_firstShowAfterSetImage(false),
    m_isProcessing(false),
    m_scribbleMode(false)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(QStringLiteral("图片预览"));
    setWindowFlag(Qt::Window, true);
    setWindowModality(Qt::NonModal);
    resize(1000, 700);

    initUi();
}

ImagePreviewDialog::~ImagePreviewDialog()
{
}

void ImagePreviewDialog::initUi()
{
    m_imageLabel = new ScribbleLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_imageLabel->setScaledContents(false);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setBackgroundRole(QPalette::Dark);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    m_zoomInBtn = new QToolButton(this);
    m_zoomInBtn->setText(QStringLiteral("放大"));
    m_zoomOutBtn = new QToolButton(this);
    m_zoomOutBtn->setText(QStringLiteral("缩小"));
    m_normalBtn = new QToolButton(this);
    m_normalBtn->setText(QStringLiteral("1:1"));
    m_fitBtn = new QToolButton(this);
    m_fitBtn->setText(QStringLiteral("适应窗口"));

    m_rotateLeftBtn = new QToolButton(this);
    m_rotateLeftBtn->setText(QStringLiteral("左旋"));
    m_rotateRightBtn = new QToolButton(this);
    m_rotateRightBtn->setText(QStringLiteral("右旋"));
    m_flipBtn = new QToolButton(this);
    m_flipBtn->setText(QStringLiteral("镜像"));

    m_grayBtn = new QToolButton(this);
    m_grayBtn->setText(QStringLiteral("灰度"));
    m_invertBtn = new QToolButton(this);
    m_invertBtn->setText(QStringLiteral("反相"));
    m_binaryBtn = new QToolButton(this);
    m_binaryBtn->setText(QStringLiteral("二值化"));
    m_mosaicBtn = new QToolButton(this);
    m_mosaicBtn->setText(QStringLiteral("马赛克"));
    m_blurBtn = new QToolButton(this);
    m_blurBtn->setText(QStringLiteral("模糊"));
    m_sharpenBtn = new QToolButton(this);
    m_sharpenBtn->setText(QStringLiteral("锐化"));

    m_scribbleBtn = new QToolButton(this);
    m_scribbleBtn->setText(QStringLiteral("涂鸦"));
    m_scribbleBtn->setCheckable(true);

    m_clearScribbleBtn = new QToolButton(this);
    m_clearScribbleBtn->setText(QStringLiteral("清空涂鸦"));

    m_resetBtn = new QToolButton(this);
    m_resetBtn->setText(QStringLiteral("复位"));
    m_restoreOriginalBtn = new QToolButton(this);
    m_restoreOriginalBtn->setText(QStringLiteral("恢复原图"));
    m_copyBtn = new QToolButton(this);
    m_copyBtn->setText(QStringLiteral("复制"));
    m_saveBtn = new QToolButton(this);
    m_saveBtn->setText(QStringLiteral("另存为"));

    m_brightnessSlider = new QSlider(Qt::Horizontal, this);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);

    m_contrastSlider = new QSlider(Qt::Horizontal, this);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);

    connect(m_zoomInBtn, &QToolButton::clicked, this, &ImagePreviewDialog::zoomIn);
    connect(m_zoomOutBtn, &QToolButton::clicked, this, &ImagePreviewDialog::zoomOut);
    connect(m_normalBtn, &QToolButton::clicked, this, &ImagePreviewDialog::normalSize);
    connect(m_fitBtn, &QToolButton::clicked, this, &ImagePreviewDialog::fitToWindow);

    connect(m_rotateLeftBtn, &QToolButton::clicked, this, &ImagePreviewDialog::rotateLeft);
    connect(m_rotateRightBtn, &QToolButton::clicked, this, &ImagePreviewDialog::rotateRight);
    connect(m_flipBtn, &QToolButton::clicked, this, &ImagePreviewDialog::flipHorizontal);

    connect(m_grayBtn, &QToolButton::clicked, this, &ImagePreviewDialog::toGray);
    connect(m_invertBtn, &QToolButton::clicked, this, &ImagePreviewDialog::invertColor);
    connect(m_binaryBtn, &QToolButton::clicked, this, &ImagePreviewDialog::binaryzation);
    connect(m_mosaicBtn, &QToolButton::clicked, this, &ImagePreviewDialog::mosaicImage);
    connect(m_blurBtn, &QToolButton::clicked, this, &ImagePreviewDialog::blurImage);
    connect(m_sharpenBtn, &QToolButton::clicked, this, &ImagePreviewDialog::sharpenImage);

    connect(m_scribbleBtn, &QToolButton::clicked, this, &ImagePreviewDialog::toggleScribbleMode);
    connect(m_clearScribbleBtn, &QToolButton::clicked, this, &ImagePreviewDialog::clearScribble);
    connect(m_imageLabel, &ScribbleLabel::scribbleFinished, this, &ImagePreviewDialog::applyScribbleToImage);

    connect(m_resetBtn, &QToolButton::clicked, this, &ImagePreviewDialog::resetImage);
    connect(m_restoreOriginalBtn, &QToolButton::clicked, this, &ImagePreviewDialog::restoreOriginalImage);
    connect(m_copyBtn, &QToolButton::clicked, this, &ImagePreviewDialog::copyImage);
    connect(m_saveBtn, &QToolButton::clicked, this, &ImagePreviewDialog::saveAs);

    connect(m_brightnessSlider, &QSlider::valueChanged, this, &ImagePreviewDialog::onBrightnessChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &ImagePreviewDialog::onContrastChanged);

    QHBoxLayout *toolLayout1 = new QHBoxLayout();
    toolLayout1->addWidget(m_zoomInBtn);
    toolLayout1->addWidget(m_zoomOutBtn);
    toolLayout1->addWidget(m_normalBtn);
    toolLayout1->addWidget(m_fitBtn);
    toolLayout1->addSpacing(10);
    toolLayout1->addWidget(m_rotateLeftBtn);
    toolLayout1->addWidget(m_rotateRightBtn);
    toolLayout1->addWidget(m_flipBtn);
    toolLayout1->addSpacing(10);
    toolLayout1->addWidget(m_grayBtn);
    toolLayout1->addWidget(m_invertBtn);
    toolLayout1->addWidget(m_binaryBtn);
    toolLayout1->addWidget(m_mosaicBtn);
    toolLayout1->addWidget(m_blurBtn);
    toolLayout1->addWidget(m_sharpenBtn);
    toolLayout1->addSpacing(10);
    toolLayout1->addWidget(m_scribbleBtn);
    toolLayout1->addWidget(m_clearScribbleBtn);
    toolLayout1->addStretch();
    toolLayout1->addWidget(m_copyBtn);
    toolLayout1->addWidget(m_saveBtn);
    toolLayout1->addWidget(m_resetBtn);
    toolLayout1->addWidget(m_restoreOriginalBtn);

    QHBoxLayout *toolLayout2 = new QHBoxLayout();
    toolLayout2->addWidget(new QLabel(QStringLiteral("亮度"), this));
    toolLayout2->addWidget(m_brightnessSlider);
    toolLayout2->addSpacing(10);
    toolLayout2->addWidget(new QLabel(QStringLiteral("对比度"), this));
    toolLayout2->addWidget(m_contrastSlider);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(toolLayout1);
    mainLayout->addLayout(toolLayout2);
    mainLayout->addWidget(m_scrollArea);
    setLayout(mainLayout);
}

void ImagePreviewDialog::setImage(const QString &filePath)
{
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("图片路径为空"));
        return;
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("图片加载失败：%1").arg(filePath));
        return;
    }

    m_sourcePath = filePath;
    m_initialImage = image;
    m_originalImage = image;
    m_currentImage = image;
    m_scaleFactor = 1.0;
    m_fitWindowMode = true;
    m_firstShowAfterSetImage = true;
    m_isProcessing = false;
    m_scribbleMode = false;

    m_scribbleBtn->setChecked(false);
    m_imageLabel->setScribbleEnabled(false);
    m_imageLabel->clearOverlay();

    m_brightnessSlider->blockSignals(true);
    m_contrastSlider->blockSignals(true);
    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(0);
    m_brightnessSlider->blockSignals(false);
    m_contrastSlider->blockSignals(false);

    updateImageDisplay();
}

void ImagePreviewDialog::setImage(const QPixmap &pixmap, const QString &sourcePath)
{
    if (pixmap.isNull()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("图片为空"));
        return;
    }

    m_sourcePath = sourcePath;
    m_initialImage = pixmap.toImage();
    m_originalImage = m_initialImage;
    m_currentImage = m_initialImage;
    m_scaleFactor = 1.0;
    m_fitWindowMode = true;
    m_firstShowAfterSetImage = true;
    m_isProcessing = false;
    m_scribbleMode = false;

    m_scribbleBtn->setChecked(false);
    m_imageLabel->setScribbleEnabled(false);
    m_imageLabel->clearOverlay();

    m_brightnessSlider->blockSignals(true);
    m_contrastSlider->blockSignals(true);
    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(0);
    m_brightnessSlider->blockSignals(false);
    m_contrastSlider->blockSignals(false);

    updateImageDisplay();
}

void ImagePreviewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (m_firstShowAfterSetImage) {
        m_fitWindowMode = true;
        updateImageDisplay();
        m_firstShowAfterSetImage = false;
    }
}

void ImagePreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (m_fitWindowMode) {
        updateImageDisplay();
    }
}

void ImagePreviewDialog::updateImageDisplay()
{
    if (m_currentImage.isNull()) {
        m_imageLabel->clear();
        return;
    }

    QPixmap pix = QPixmap::fromImage(m_currentImage);

    if (m_fitWindowMode) {
        QSize viewportSize = m_scrollArea->viewport()->size();
        QPixmap scaled = pix.scaled(viewportSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setDisplayPixmap(scaled);
    } else {
        QSize targetSize(
            qMax(1, int(pix.width() * m_scaleFactor)),
            qMax(1, int(pix.height() * m_scaleFactor))
            );
        QPixmap scaled = pix.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setDisplayPixmap(scaled);
    }

    m_imageLabel->setBaseImage(m_currentImage);

    setWindowTitle(QStringLiteral("图片预览 - %1 x %2")
                       .arg(m_currentImage.width())
                       .arg(m_currentImage.height()));
}

void ImagePreviewDialog::scaleImage(double factor)
{
    if (m_currentImage.isNull() || m_isProcessing) {
        return;
    }

    if (m_fitWindowMode) {
        m_fitWindowMode = false;
        m_scaleFactor = 1.0;
    }

    double newFactor = m_scaleFactor * factor;
    if (newFactor < 0.05) newFactor = 0.05;
    if (newFactor > 10.0) newFactor = 10.0;
    setScaleFactor(newFactor);
}

void ImagePreviewDialog::setScaleFactor(double factor)
{
    m_scaleFactor = factor;
    updateImageDisplay();
}

void ImagePreviewDialog::zoomIn() { scaleImage(1.25); }
void ImagePreviewDialog::zoomOut() { scaleImage(0.8); }

void ImagePreviewDialog::normalSize()
{
    if (m_isProcessing) return;
    m_fitWindowMode = false;
    m_scaleFactor = 1.0;
    updateImageDisplay();
}

void ImagePreviewDialog::fitToWindow()
{
    if (m_isProcessing) return;
    m_fitWindowMode = true;
    updateImageDisplay();
}

void ImagePreviewDialog::setProcessing(bool processing)
{
    m_isProcessing = processing;

    m_grayBtn->setEnabled(!processing);
    m_invertBtn->setEnabled(!processing);
    m_binaryBtn->setEnabled(!processing);
    m_mosaicBtn->setEnabled(!processing);
    m_blurBtn->setEnabled(!processing);
    m_sharpenBtn->setEnabled(!processing);
    m_rotateLeftBtn->setEnabled(!processing);
    m_rotateRightBtn->setEnabled(!processing);
    m_flipBtn->setEnabled(!processing);
    m_resetBtn->setEnabled(!processing);
    m_restoreOriginalBtn->setEnabled(!processing);
    m_copyBtn->setEnabled(!processing);
    m_saveBtn->setEnabled(!processing);
    m_brightnessSlider->setEnabled(!processing);
    m_contrastSlider->setEnabled(!processing);
    m_scribbleBtn->setEnabled(!processing);
    m_clearScribbleBtn->setEnabled(!processing);
}

void ImagePreviewDialog::runImageOp(const std::function<QImage(const QImage&)> &op)
{
    if (m_isProcessing || m_originalImage.isNull()) {
        return;
    }

    setProcessing(true);
    QImage dst = op(m_originalImage);

    if (!dst.isNull()) {
        m_originalImage = dst;
        applyAdjustments();
        autoCopyEditedImage();
    }

    m_imageLabel->clearOverlay();
    setProcessing(false);
}

void ImagePreviewDialog::rotateLeft()
{
    runImageOp([](const QImage &src) {
        QTransform t; t.rotate(-90);
        return src.transformed(t, Qt::SmoothTransformation);
    });
}

void ImagePreviewDialog::rotateRight()
{
    runImageOp([](const QImage &src) {
        QTransform t; t.rotate(90);
        return src.transformed(t, Qt::SmoothTransformation);
    });
}

void ImagePreviewDialog::flipHorizontal()
{
    runImageOp([](const QImage &src) {
        return src.mirrored(true, false);
    });
}

void ImagePreviewDialog::toGray()
{
    runImageOp([this](const QImage &src) {
        return makeGray(src);
    });
}

void ImagePreviewDialog::invertColor()
{
    runImageOp([](const QImage &src) {
        QImage img = src.convertToFormat(QImage::Format_ARGB32);
        img.invertPixels(QImage::InvertRgb);
        return img;
    });
}

void ImagePreviewDialog::binaryzation()
{
    runImageOp([this](const QImage &src) {
        return makeBinary(src, 128);
    });
}

void ImagePreviewDialog::mosaicImage()
{
    runImageOp([this](const QImage &src) {
        return makeMosaic(src, 12);
    });
}

void ImagePreviewDialog::blurImage()
{
    runImageOp([this](const QImage &src) {
        return makeBlurFast(src);
    });
}

void ImagePreviewDialog::sharpenImage()
{
    runImageOp([this](const QImage &src) {
        return makeSharpenFast(src);
    });
}

void ImagePreviewDialog::toggleScribbleMode()
{
    if (m_isProcessing) return;

    m_scribbleMode = m_scribbleBtn->isChecked();
    m_imageLabel->setScribbleEnabled(m_scribbleMode);
}

void ImagePreviewDialog::clearScribble()
{
    if (m_isProcessing) return;
    m_imageLabel->clearOverlay();
}

void ImagePreviewDialog::applyScribbleToImage()
{
    if (m_isProcessing || !m_imageLabel->hasOverlay() || m_currentImage.isNull()) {
        return;
    }

    m_originalImage = m_imageLabel->mergeToImage(m_currentImage);
    m_currentImage = m_originalImage;
    m_imageLabel->clearOverlay();
    updateImageDisplay();
    autoCopyEditedImage();
}

void ImagePreviewDialog::onBrightnessChanged(int)
{
    if (m_isProcessing) return;
    applyAdjustments();
    autoCopyEditedImage();
}

void ImagePreviewDialog::onContrastChanged(int)
{
    if (m_isProcessing) return;
    applyAdjustments();
    autoCopyEditedImage();
}

void ImagePreviewDialog::applyAdjustments()
{
    if (m_originalImage.isNull()) return;

    QImage img = m_originalImage.convertToFormat(QImage::Format_ARGB32);
    int brightness = m_brightnessSlider->value();
    int contrast = m_contrastSlider->value();

    double c = 1.0;
    if (contrast != 0) {
        c = (100.0 + contrast) / 100.0;
        c *= c;
    }

    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            int a = qAlpha(line[x]);
            int r = clamp(qRed(line[x]) + brightness);
            int g = clamp(qGreen(line[x]) + brightness);
            int b = clamp(qBlue(line[x]) + brightness);

            if (contrast != 0) {
                double rf = ((r / 255.0 - 0.5) * c + 0.5) * 255.0;
                double gf = ((g / 255.0 - 0.5) * c + 0.5) * 255.0;
                double bf = ((b / 255.0 - 0.5) * c + 0.5) * 255.0;
                r = clamp(int(rf));
                g = clamp(int(gf));
                b = clamp(int(bf));
            }

            line[x] = qRgba(r, g, b, a);
        }
    }

    m_currentImage = img;
    updateImageDisplay();
}

int ImagePreviewDialog::clamp(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

QImage ImagePreviewDialog::makeGray(const QImage &src)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            int a = qAlpha(line[x]);
            int gray = qGray(line[x]);
            line[x] = qRgba(gray, gray, gray, a);
        }
    }
    return img;
}

QImage ImagePreviewDialog::makeBinary(const QImage &src, int threshold)
{
    QImage img = makeGray(src).convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            int a = qAlpha(line[x]);
            int v = qRed(line[x]) >= threshold ? 255 : 0;
            line[x] = qRgba(v, v, v, a);
        }
    }
    return img;
}

QImage ImagePreviewDialog::makeBlurFast(const QImage &src)
{
    QImage in = src.convertToFormat(QImage::Format_ARGB32);
    QImage out(in.size(), QImage::Format_ARGB32);
    int w = in.width(), h = in.height();

    for (int y = 0; y < h; ++y) {
        const QRgb *prev = reinterpret_cast<const QRgb *>(in.constScanLine(y > 0 ? y - 1 : y));
        const QRgb *curr = reinterpret_cast<const QRgb *>(in.constScanLine(y));
        const QRgb *next = reinterpret_cast<const QRgb *>(in.constScanLine(y < h - 1 ? y + 1 : y));
        QRgb *dst = reinterpret_cast<QRgb *>(out.scanLine(y));

        for (int x = 0; x < w; ++x) {
            int xl = (x > 0 ? x - 1 : x);
            int xr = (x < w - 1 ? x + 1 : x);

            QRgb p1 = prev[xl], p2 = prev[x], p3 = prev[xr];
            QRgb p4 = curr[xl], p5 = curr[x], p6 = curr[xr];
            QRgb p7 = next[xl], p8 = next[x], p9 = next[xr];

            int r = (qRed(p1)+qRed(p2)+qRed(p3)+qRed(p4)+qRed(p5)+qRed(p6)+qRed(p7)+qRed(p8)+qRed(p9))/9;
            int g = (qGreen(p1)+qGreen(p2)+qGreen(p3)+qGreen(p4)+qGreen(p5)+qGreen(p6)+qGreen(p7)+qGreen(p8)+qGreen(p9))/9;
            int b = (qBlue(p1)+qBlue(p2)+qBlue(p3)+qBlue(p4)+qBlue(p5)+qBlue(p6)+qBlue(p7)+qBlue(p8)+qBlue(p9))/9;
            dst[x] = qRgba(r, g, b, qAlpha(p5));
        }
    }
    return out;
}

QImage ImagePreviewDialog::makeSharpenFast(const QImage &src)
{
    QImage in = src.convertToFormat(QImage::Format_ARGB32);
    QImage out(in.size(), QImage::Format_ARGB32);
    int w = in.width(), h = in.height();

    for (int y = 0; y < h; ++y) {
        const QRgb *prev = reinterpret_cast<const QRgb *>(in.constScanLine(y > 0 ? y - 1 : y));
        const QRgb *curr = reinterpret_cast<const QRgb *>(in.constScanLine(y));
        const QRgb *next = reinterpret_cast<const QRgb *>(in.constScanLine(y < h - 1 ? y + 1 : y));
        QRgb *dst = reinterpret_cast<QRgb *>(out.scanLine(y));

        for (int x = 0; x < w; ++x) {
            int xl = (x > 0 ? x - 1 : x);
            int xr = (x < w - 1 ? x + 1 : x);

            QRgb left = curr[xl];
            QRgb right = curr[xr];
            QRgb up = prev[x];
            QRgb down = next[x];
            QRgb mid = curr[x];

            int r = -qRed(left) - qRed(right) - qRed(up) - qRed(down) + 5 * qRed(mid);
            int g = -qGreen(left) - qGreen(right) - qGreen(up) - qGreen(down) + 5 * qGreen(mid);
            int b = -qBlue(left) - qBlue(right) - qBlue(up) - qBlue(down) + 5 * qBlue(mid);

            dst[x] = qRgba(clamp(r), clamp(g), clamp(b), qAlpha(mid));
        }
    }
    return out;
}

QImage ImagePreviewDialog::makeMosaic(const QImage &src, int blockSize)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    int w = img.width(), h = img.height();

    for (int y = 0; y < h; y += blockSize) {
        for (int x = 0; x < w; x += blockSize) {
            int bw = qMin(blockSize, w - x);
            int bh = qMin(blockSize, h - y);

            long long rs = 0, gs = 0, bs = 0, as = 0;
            int count = 0;

            for (int yy = 0; yy < bh; ++yy) {
                const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y + yy));
                for (int xx = 0; xx < bw; ++xx) {
                    QRgb p = line[x + xx];
                    rs += qRed(p);
                    gs += qGreen(p);
                    bs += qBlue(p);
                    as += qAlpha(p);
                    ++count;
                }
            }

            QRgb avg = qRgba(int(rs / count), int(gs / count), int(bs / count), int(as / count));

            for (int yy = 0; yy < bh; ++yy) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y + yy));
                for (int xx = 0; xx < bw; ++xx) {
                    line[x + xx] = avg;
                }
            }
        }
    }
    return img;
}

void ImagePreviewDialog::resetImage()
{
    if (m_originalImage.isNull()) return;

    m_brightnessSlider->blockSignals(true);
    m_contrastSlider->blockSignals(true);
    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(0);
    m_brightnessSlider->blockSignals(false);
    m_contrastSlider->blockSignals(false);

    m_currentImage = m_originalImage;
    m_imageLabel->clearOverlay();
    m_fitWindowMode = true;
    m_scaleFactor = 1.0;
    updateImageDisplay();
}

void ImagePreviewDialog::restoreOriginalImage()
{
    if (m_initialImage.isNull() || m_isProcessing) return;

    m_originalImage = m_initialImage;
    m_currentImage = m_initialImage;

    m_brightnessSlider->blockSignals(true);
    m_contrastSlider->blockSignals(true);
    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(0);
    m_brightnessSlider->blockSignals(false);
    m_contrastSlider->blockSignals(false);

    m_imageLabel->clearOverlay();
    m_fitWindowMode = true;
    m_scaleFactor = 1.0;
    updateImageDisplay();
    autoCopyEditedImage();
}

void ImagePreviewDialog::copyImage()
{
    if (!m_currentImage.isNull()) {
        QApplication::clipboard()->setImage(m_currentImage);
    }
}

void ImagePreviewDialog::autoCopyEditedImage()
{
    copyImage();
}

QString ImagePreviewDialog::buildDefaultSaveName() const
{
    QFileInfo info(m_sourcePath);
    QString baseName = info.completeBaseName();
    if (baseName.isEmpty()) baseName = QStringLiteral("image");
    QString suffix = info.suffix();
    if (suffix.isEmpty()) suffix = QStringLiteral("png");

    return QString("%1_edited_%2.%3")
        .arg(baseName)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
        .arg(suffix);
}

void ImagePreviewDialog::saveAs()
{
    if (m_currentImage.isNull()) return;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("另存为"),
        buildDefaultSaveName(),
        QStringLiteral("PNG Files (*.png);;JPEG Files (*.jpg *.jpeg);;BMP Files (*.bmp);;All Files (*)")
        );

    if (fileName.isEmpty()) return;

    if (!m_currentImage.save(fileName)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("保存失败"));
    }
}

void ImagePreviewDialog::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}

void ImagePreviewDialog::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

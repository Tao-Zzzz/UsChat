#ifndef IMAGEPREVIEWDIALOG_H
#define IMAGEPREVIEWDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPoint>
#include <functional>
#include <QLabel>

class QLabel;
class QScrollArea;
class QToolButton;
class QSlider;
class QShowEvent;
class QResizeEvent;
class QWheelEvent;
class QCloseEvent;
class QMouseEvent;

class ScribbleLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ScribbleLabel(QWidget *parent = nullptr);

    void setBaseImage(const QImage &img);
    void setDisplayPixmap(const QPixmap &pixmap);
    void setScribbleEnabled(bool enabled);
    void clearOverlay();
    bool hasOverlay() const;
    QImage mergeToImage(const QImage &baseImage) const;

signals:
    void scribbleFinished();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint mapToImagePoint(const QPoint &pos) const;

private:
    bool m_scribbleEnabled;
    bool m_drawing;
    QPoint m_lastPoint;

    QImage m_baseImage;
    QImage m_overlay;
};

class ImagePreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImagePreviewDialog(QWidget *parent = nullptr);
    ~ImagePreviewDialog();

    void setImage(const QString &filePath);
    void setImage(const QPixmap &pixmap, const QString &sourcePath = QString());

protected:
    void wheelEvent(QWheelEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void zoomIn();
    void zoomOut();
    void normalSize();
    void fitToWindow();

    void rotateLeft();
    void rotateRight();
    void flipHorizontal();

    void toGray();
    void invertColor();
    void binaryzation();
    void mosaicImage();
    void blurImage();
    void sharpenImage();

    void toggleScribbleMode();
    void clearScribble();
    void applyScribbleToImage();

    void resetImage();
    void restoreOriginalImage();
    void copyImage();
    void saveAs();

    void onBrightnessChanged(int value);
    void onContrastChanged(int value);

private:
    void initUi();
    void updateImageDisplay();
    void scaleImage(double factor);
    void setScaleFactor(double factor);
    void autoCopyEditedImage();
    QString buildDefaultSaveName() const;

    void applyAdjustments();
    void setProcessing(bool processing);
    void runImageOp(const std::function<QImage(const QImage&)> &op);

    static int clamp(int v);
    QImage makeGray(const QImage &src);
    QImage makeBinary(const QImage &src, int threshold = 128);
    QImage makeBlurFast(const QImage &src);
    QImage makeSharpenFast(const QImage &src);
    QImage makeMosaic(const QImage &src, int blockSize = 12);

private:
    ScribbleLabel* m_imageLabel;
    QScrollArea* m_scrollArea;

    QToolButton* m_zoomInBtn;
    QToolButton* m_zoomOutBtn;
    QToolButton* m_normalBtn;
    QToolButton* m_fitBtn;

    QToolButton* m_rotateLeftBtn;
    QToolButton* m_rotateRightBtn;
    QToolButton* m_flipBtn;

    QToolButton* m_grayBtn;
    QToolButton* m_invertBtn;
    QToolButton* m_binaryBtn;
    QToolButton* m_mosaicBtn;
    QToolButton* m_blurBtn;
    QToolButton* m_sharpenBtn;

    QToolButton* m_scribbleBtn;
    QToolButton* m_clearScribbleBtn;

    QToolButton* m_resetBtn;
    QToolButton* m_restoreOriginalBtn;
    QToolButton* m_copyBtn;
    QToolButton* m_saveBtn;

    QSlider* m_brightnessSlider;
    QSlider* m_contrastSlider;

    QImage m_initialImage;
    QImage m_originalImage;
    QImage m_currentImage;
    QString m_sourcePath;

    double m_scaleFactor;
    bool m_fitWindowMode;
    bool m_firstShowAfterSetImage;
    bool m_isProcessing;
    bool m_scribbleMode;
};

#endif // IMAGEPREVIEWDIALOG_H

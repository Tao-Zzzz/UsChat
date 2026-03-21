#include "MessageTextEdit.h"
#include <QDebug>
#include <QMessageBox>
#include <QTextBlock>
#include <QTextFragment>
#include "global.h"

static const int kEmojiUserProperty = QTextFormat::UserProperty + 1;

MessageTextEdit::MessageTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    this->setMaximumHeight(60);
}

MessageTextEdit::~MessageTextEdit()
{
}

QString MessageTextEdit::emojiTokenToPath(const QString &token) const
{
    static const QMap<QString, QString> emojiMap = {
        {"[yd_smile]", ":/res/emoji/yd_smile.png"},
        {"[yd_happy]", ":/res/emoji/yd_happy.png"},
        {"[yd_love]",  ":/res/emoji/yd_love.png"},
        {"[yd_shy]",   ":/res/emoji/yd_shy.png"},
        {"[yd_cry]",   ":/res/emoji/yd_cry.png"},
        {"[yd_angry]", ":/res/emoji/yd_angry.png"},
        {"[yd_sleep]", ":/res/emoji/yd_sleep.png"},
        {"[yd_ok]",    ":/res/emoji/yd_ok.png"},
        {"[yd_wow]",   ":/res/emoji/yd_wow.png"},
        {"[yd_sweat]", ":/res/emoji/yd_sweat.png"},
        {"[yd_think]", ":/res/emoji/yd_think.png"},
        {"[yd_bye]",   ":/res/emoji/yd_bye.png"}
    };

    return emojiMap.value(token);
}

QString MessageTextEdit::emojiPathToToken(const QString &path) const
{
    static const QMap<QString, QString> reverseMap = {
        {":/res/emoji/yd_smile.png", "[yd_smile]"},
        {":/res/emoji/yd_happy.png", "[yd_happy]"},
        {":/res/emoji/yd_love.png",  "[yd_love]"},
        {":/res/emoji/yd_shy.png",   "[yd_shy]"},
        {":/res/emoji/yd_cry.png",   "[yd_cry]"},
        {":/res/emoji/yd_angry.png", "[yd_angry]"},
        {":/res/emoji/yd_sleep.png", "[yd_sleep]"},
        {":/res/emoji/yd_ok.png",    "[yd_ok]"},
        {":/res/emoji/yd_wow.png",   "[yd_wow]"},
        {":/res/emoji/yd_sweat.png", "[yd_sweat]"},
        {":/res/emoji/yd_think.png", "[yd_think]"},
        {":/res/emoji/yd_bye.png",   "[yd_bye]"}
    };

    return reverseMap.value(path);
}

QVector<std::shared_ptr<MsgInfo>> MessageTextEdit::getMsgList()
{
    _total_msg_list.clear();

    QString text;
    int mediaIndex = 0;

    QTextDocument *doc = this->document();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
    {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
        {
            QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }

            QTextCharFormat fmt = fragment.charFormat();
            if (fmt.isImageFormat())
            {
                QTextImageFormat imgFmt = fmt.toImageFormat();
                QString token = imgFmt.property(kEmojiUserProperty).toString();
                QString name = imgFmt.name();

                // 先判断是不是 emoji
                if (!token.isEmpty()) {
                    text += token;   // 直接拼进当前文本，不要 flush
                    continue;
                }

                // 普通图片/文件，才把前面积累的文本吐出去
                if (!text.isEmpty()) {
                    insertMsgList(_total_msg_list, MsgType::TEXT_MSG, text, QPixmap(), "", 0, "");
                    text.clear();
                }

                while (mediaIndex < _img_or_file_list.size())
                {
                    auto msg = _img_or_file_list[mediaIndex++];
                    if (msg && msg->_text_or_url == name) {
                        _total_msg_list.append(msg);
                        break;
                    }
                }
            }
            else
            {
                text += fragment.text();
            }
        }

        if (block.next().isValid()) {
            text += "\n";
        }
    }

    if (!text.isEmpty()) {
        insertMsgList(_total_msg_list, MsgType::TEXT_MSG, text, QPixmap(), "", 0, "");
        text.clear();
    }

    _img_or_file_list.clear();
    this->clear();
    return _total_msg_list;
}

void MessageTextEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->source()==this)
        event->ignore();
    else
        event->accept();
}

void MessageTextEdit::dropEvent(QDropEvent *event)
{
    insertFromMimeData(event->mimeData());
    event->accept();
}

void MessageTextEdit::keyPressEvent(QKeyEvent *e)
{
    if((e->key()==Qt::Key_Enter||e->key()==Qt::Key_Return)&& !(e->modifiers() & Qt::ShiftModifier))
    {
        emit send();
        return;
    }
    QTextEdit::keyPressEvent(e);
}

void MessageTextEdit::insertFileFromUrl(const QStringList &urls)
{
    if(urls.isEmpty())
        return;

    foreach (QString url, urls){
        if(isImage(url))
            insertImages(url);
        else
            insertFiles(url);
    }
}

void MessageTextEdit::insertImages(const QString &url)
{
    QFileInfo  fileInfo(url);
    if (fileInfo.isDir())
    {
        QMessageBox::information(this, "提示", "只允许拖拽单个文件!");
        return;
    }

    auto total_size = fileInfo.size();
    qint64 max_size = qint64(2) * 1024 * 1024 * 1024;

    if (total_size > max_size)
    {
        QMessageBox::information(this, "提示", "发送的文件大小不能大于2G");
        return;
    }

    QString fileMd5 = calculateFileHash(url);
    if (fileMd5.isEmpty())
    {
        QMessageBox::warning(this, "错误", "无法计算文件MD5");
        return;
    }

    QImage image(url);
    if(image.width()>120||image.height()>80)
    {
        if(image.width()>image.height())
            image = image.scaledToWidth(120,Qt::SmoothTransformation);
        else
            image = image.scaledToHeight(80,Qt::SmoothTransformation);
    }

    QTextCursor cursor = this->textCursor();
    cursor.insertImage(image, url);

    QString origin_name = fileInfo.fileName();
    QString unique_name = generateUniqueFileName(origin_name);
    insertMsgList(_img_or_file_list, MsgType::IMG_MSG, url, QPixmap::fromImage(image),
                  unique_name, total_size, fileMd5);
}

void MessageTextEdit::insertFiles(const QString& url)
{
    QFileInfo fileInfo(url);
    if (fileInfo.isDir())
    {
        QMessageBox::information(this, "提示", "只允许拖拽单个文件!");
        return;
    }

    auto total_size = fileInfo.size();
    qint64 max_size = qint64(2) * 1024 * 1024 * 1024;

    if (total_size > max_size)
    {
        QMessageBox::information(this, "提示", "发送的文件大小不能大于100M");
        return;
    }

    QString fileMd5 = calculateFileHash(url);
    if (fileMd5.isEmpty())
    {
        QMessageBox::warning(this, "错误", "无法计算文件MD5");
        return;
    }

    QPixmap pix = getFileIconPixmap(url);
    QTextCursor cursor = this->textCursor();
    cursor.insertImage(pix.toImage(), url);

    QString origin_name = fileInfo.fileName();
    QString unique_name = generateUniqueFileName(origin_name);
    insertMsgList(_img_or_file_list, MsgType::FILE_MSG, url, pix,
                  unique_name, total_size, fileMd5);
}

bool MessageTextEdit::canInsertFromMimeData(const QMimeData *source) const
{
    return QTextEdit::canInsertFromMimeData(source);
}

void MessageTextEdit::insertFromMimeData(const QMimeData *source)
{
    // 优先处理真正的文件 URL
    if (source->hasUrls()) {
        QStringList urls;
        for (const QUrl &url : source->urls()) {
            if (url.isLocalFile()) {
                urls << url.toLocalFile();
            }
        }

        if (!urls.isEmpty()) {
            for (const QString &url : urls) {
                if (isImage(url))
                    insertImages(url);
                else
                    insertFiles(url);
            }
            return;
        }
    }

    // 兼容你原来这种 "file:///" 文本格式
    QStringList urls = getUrl(source->text());
    if (!urls.isEmpty()) {
        for (const QString &url : urls) {
            if (isImage(url))
                insertImages(url);
            else
                insertFiles(url);
        }
        return;
    }

    // 普通文本/富文本交给基类
    QTextEdit::insertFromMimeData(source);
}

bool MessageTextEdit::isImage(QString url)
{
    QString imageFormat = "bmp,jpg,png,tif,gif,pcx,tga,exif,fpx,svg,psd,cdr,pcd,dxf,ufo,eps,ai,raw,wmf,webp";
    QStringList imageFormatList = imageFormat.split(",");
    QFileInfo fileInfo(url);
    QString suffix = fileInfo.suffix();
    if(imageFormatList.contains(suffix,Qt::CaseInsensitive)){
        return true;
    }
    return false;
}

void MessageTextEdit::insertMsgList(QVector<std::shared_ptr<MsgInfo>> &list, MsgType msgtype,
                                    QString text_or_url, QPixmap preview_pix,
                                    QString unique_name, uint64_t total_size, QString md5)
{
    auto msg_info = std::make_shared<MsgInfo>(msgtype, text_or_url, preview_pix, unique_name, total_size, md5);
    list.append(msg_info);
}

QStringList MessageTextEdit::getUrl(QString text)
{
    QStringList urls;
    if(text.isEmpty()) return urls;

    QStringList list = text.split("\n");
    foreach (QString url, list) {
        if(!url.isEmpty()){
            QStringList str = url.split("///");
            if(str.size()>=2)
                urls.append(str.at(1));
        }
    }
    return urls;
}

QPixmap MessageTextEdit::getFileIconPixmap(const QString &url)
{
    QFileIconProvider provder;
    QFileInfo fileinfo(url);
    QIcon icon = provder.icon(fileinfo);

    QString strFileSize = getFileSize(fileinfo.size());

    QFont font(QString("宋体"),10,QFont::Normal,false);
    QFontMetrics fontMetrics(font);
    QSize textSize = fontMetrics.size(Qt::TextSingleLine, fileinfo.fileName());
    QSize FileSize = fontMetrics.size(Qt::TextSingleLine, strFileSize);
    int maxWidth = textSize.width() > FileSize.width() ? textSize.width() : FileSize.width();

    QPixmap pix(50 + maxWidth + 10, 50);
    pix.fill();

    QPainter painter;
    painter.begin(&pix);
    QRect rect(0, 0, 50, 50);
    painter.drawPixmap(rect, icon.pixmap(40,40));
    painter.setPen(Qt::black);

    QRect rectText(60, 3, textSize.width(), textSize.height());
    painter.drawText(rectText, fileinfo.fileName());

    QRect rectFile(60, textSize.height()+5, FileSize.width(), FileSize.height());
    painter.drawText(rectFile, strFileSize);
    painter.end();
    return pix;
}

QString MessageTextEdit::getFileSize(qint64 size)
{
    QString Unit;
    double num;
    if(size < 1024){
        num = size;
        Unit = "B";
    }
    else if(size < 1024 * 1224){
        num = size / 1024.0;
        Unit = "KB";
    }
    else if(size < 1024 * 1024 * 1024){
        num = size / 1024.0 / 1024.0;
        Unit = "MB";
    }
    else{
        num = size / 1024.0 / 1024.0 / 1024.0;
        Unit = "GB";
    }
    return QString::number(num,'f',2) + " " + Unit;
}

void MessageTextEdit::textEditChanged()
{
}

void MessageTextEdit::insertEmojiToken(const QString& token)
{
    QString path = emojiTokenToPath(token);
    if (path.isEmpty()) {
        return;
    }

    QTextImageFormat imageFormat;
    imageFormat.setName(path);
    imageFormat.setWidth(22);
    imageFormat.setHeight(22);
    imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    imageFormat.setProperty(kEmojiUserProperty, token);

    QTextCursor cursor = textCursor();
    cursor.insertImage(imageFormat);
    setTextCursor(cursor);
    setFocus();
}

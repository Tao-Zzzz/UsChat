#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include "httpmgr.h"
#include "tcpmgr.h"
#include <QRegularExpression>
#include <QRegularExpression>
#include <QPainter>
#include "filetcpmgr.h"
#include <QPainterPath>
#include "aimgr.h"
#include <QMessageBox>
#include <opencv2/opencv.hpp>
#include "faceauthmgr.h"

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    connect(ui->reg_btn, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");
    ui->forget_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    ui->face_label->SetState("normal","hover","","selected","selected_hover","");
    ui->face_label->setCursor(Qt::PointingHandCursor);
    connect(ui->face_label, &ClickedLabel::clicked, this, &LoginDialog::slot_face_login);


    initHttpHandlers();
    //连接登录回包信号
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_login_mod_finish, this,
            &LoginDialog::slot_login_mod_finish);
    //连接tcp连接请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_tcp, TcpMgr::GetInstance().get(), &TcpMgr::slot_tcp_connect);
    //连接tcp管理者发出的连接成功信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    //连接tcp管理者发出的登陆失败信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_login_failed, this, &LoginDialog::slot_login_failed);

    //连接tcp连接资源服务器请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_res_server,
            FileTcpMgr::GetInstance().get(), &FileTcpMgr::slot_tcp_connect);

    //连接资源管理tcp发出的连接成功信号
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_con_success, this, &LoginDialog::slot_res_con_finish);
    initHead();
}

LoginDialog::~LoginDialog()
{
    qDebug()<<"destruct LoginDlg";
    delete ui;
}

void LoginDialog::initHead()
{
    // 加载图片
    QPixmap originalPixmap(":/res/head_1.jpg");
      // 设置图片自动缩放
    qDebug()<< originalPixmap.size() << ui->head_label->size();
    originalPixmap = originalPixmap.scaled(ui->head_label->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建一个和原始图片相同大小的QPixmap，用于绘制圆角图片
    QPixmap roundedPixmap(originalPixmap.size());
    roundedPixmap.fill(Qt::transparent); // 用透明色填充

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing); // 设置抗锯齿，使圆角更平滑
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 使用QPainterPath设置圆角
    QPainterPath path;
    path.addRoundedRect(0, 0, originalPixmap.width(), originalPixmap.height(), 10, 10); // 最后两个参数分别是x和y方向的圆角半径
    painter.setClipPath(path);

    // 将原始图片绘制到roundedPixmap上
    painter.drawPixmap(0, 0, originalPixmap);

    // 设置绘制好的圆角图片到QLabel上
    ui->head_label->setPixmap(roundedPixmap);

}

void LoginDialog::initHttpHandlers()
{
    //注册获取登录回包逻辑
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            enableBtn(true);
            return;
        }
        auto email = jsonObj["email"].toString();

        //发送信号通知tcpMgr发送长链接
        _si = std::make_shared<ServerInfo>();

        _si->_uid = jsonObj["uid"].toInt();
        _si->_chat_host = jsonObj["chathost"].toString();
        _si->_chat_port = jsonObj["chatport"].toString();
        _si->_token = jsonObj["token"].toString();

        _si->_res_host = jsonObj["reshost"].toString();
        _si->_res_port = jsonObj["resport"].toString();


        qDebug()<< "email is " << email << " uid is " << _si->_uid <<" chat host is "
                << _si->_chat_host << " chat port is "
                << _si->_chat_port << " token is " << _si->_token
                << " res host is " << _si->_res_host
                << " res port is " << _si->_res_port;
        emit sig_connect_tcp(_si);
       // qDebug() << "send thread is " << QThread::currentThread();
       // emit sig_test();
    });
}

void LoginDialog::showTip(QString str, bool b_ok)
{
    if(b_ok){
         ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }

    ui->err_tip->setText(str);

    repolish(ui->err_tip);
}

void LoginDialog::slot_forget_pwd()
{
    qDebug()<<"slot forget pwd";
    emit switchReset();
}

void LoginDialog::slot_face_login()
{
    // 1. 从本地获取之前绑定的“标准人脸特征” (假设你用 xml 保存了 cv::Mat)
    // 【注意】：你需要先做一个“录入人脸”的功能，把用户的特征提取出来存在本地
    cv::Mat mySavedFeature;
    cv::FileStorage fs("static/my_face_feature.xml", cv::FileStorage::READ);
    if (fs.isOpened()) {
        fs["feature"] >> mySavedFeature;
        fs.release();
    }

    if (mySavedFeature.empty()) {
        QMessageBox::warning(this, "提示", "您尚未录入人脸，请先使用密码登录并绑定人脸！");
        return;
    }

    // 2. 打开摄像头
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        QMessageBox::warning(this, "错误", "无法打开摄像头！");
        return;
    }

    cv::Mat frame;
    bool login_success = false;


    cv::namedWindow("Face Scan Login (Press ESC to cancel)", cv::WINDOW_AUTOSIZE);
    // 3. 开始实时扫描识别
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        // 1. 先把当前画面显示出来
        cv::putText(frame, "Scanning face...", cv::Point(50, 50),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
        cv::imshow("Face Scan Login (Press ESC to cancel)", frame);

        // 刷新一下 Qt 事件循环，确保画面被绘制
        cv::waitKey(1);

        // 2. 再做 AI 提取与比对
        cv::Mat currentFeature = FaceAuthMgr::GetInstance()->ExtractFeature(frame);
        if (!currentFeature.empty()) {
            float score = FaceAuthMgr::GetInstance()->Match(mySavedFeature, currentFeature);
            if (score > 0.363f) {
                login_success = true;
                // 为了让用户看清楚绿字，这里甚至可以加一个短暂的延时，比如 QThread::msleep(500);
                break;
            }
        }

        // 3. 处理按键退出
        if (cv::waitKey(30) == 27) { // 按 ESC 键退出
            break;
        }
    }

    // 4. 清理摄像头和窗口
    cv::destroyAllWindows();
    cap.release();
    cv::waitKey(1); // 【关键修复 1】：必须加这一行！给 OpenCV 1毫秒的时间去彻底回收底层窗口句柄

    // 5. 如果识别成功，复用你的 HTTP 登录逻辑
    if (login_success) {
        QSettings settings("MyCompany", "MyApp");
        int uid = settings.value("saved_uid", -1).toInt();

        if (uid == -1) {
            QMessageBox::warning(this, "提示", "本地账号凭据丢失，请先用密码登录并重新绑定人脸！");
            return;
        }

        // 组装并发送登录请求
        QJsonObject json_obj;
        json_obj["uid"] = uid;

        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/face_login"),
                                            json_obj,
                                            ReqId::ID_LOGIN_USER,
                                            Modules::LOGINMOD);

    }
}

bool LoginDialog::checkUserValid(){

    auto email = ui->email_edit->text();
    if(email.isEmpty()){
        qDebug() << "email empty " ;
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool LoginDialog::checkPwdValid(){
    auto pwd = ui->pass_edit->text();
    if(pwd.length() < 6 || pwd.length() > 15){
        qDebug() << "Pass length invalid";
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*.]{6,15}$");
    bool match = regExp.match(pwd).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符且长度为(6~15)"));
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    return true;
}

bool LoginDialog::enableBtn(bool enabled)
{
    ui->login_btn->setEnabled(enabled);
    ui->reg_btn->setEnabled(enabled);
    return true;
}

void LoginDialog::on_login_btn_clicked()
{
    qDebug()<<"login btn clicked";
    if(checkUserValid() == false){
        return;
    }

    if(checkPwdValid() == false){
        return ;
    }

    enableBtn(false);
    auto email = ui->email_edit->text();
    auto pwd = ui->pass_edit->text();
    //发送http请求登录
    QJsonObject json_obj;
    json_obj["email"] = email;
    json_obj["passwd"] = xorString(pwd);
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_login"),
                                        json_obj, ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
}

void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    // 解析 JSON 字符串,res需转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"),false);
        return;
    }


    //调用对应的逻辑,根据id回调。
    _handlers[id](jsonDoc.object());

    return;
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if(bsuccess){
        showTip(tr("聊天服务连接成功，正在连接资源服务器..."),true);
        emit sig_connect_res_server(_si);
    }else{
        showTip(tr("网络异常"), false);
    }
}

void LoginDialog::slot_login_failed(int err)
{
    QString result = QString("登录失败, err is %1")
                             .arg(err);
    showTip(result,false);
    enableBtn(true);
}

void LoginDialog::slot_res_con_finish(bool bsuccess)
{
       if(bsuccess){
          showTip(tr("聊天服务连接成功，正在登录..."),true);
          QJsonObject jsonObj;
          jsonObj["uid"] = _si->_uid;
          jsonObj["token"] = _si->_token;

          QJsonDocument doc(jsonObj);
          QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

          //发送tcp请求给chat server
         emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN, jsonData);

         // 获取AIServer
        AIMgr::GetInstance()->SetAIHost("127.0.0.1");
        AIMgr::GetInstance()->SetAIPort(8070);
        AIMgr::GetInstance()->SetAIScheme("http");

       }else{
          showTip(tr("网络异常"),false);
          enableBtn(true);
       }

}

void LoginDialog::AddTipErr(TipErr te,QString tips){
    _tip_errs[te] = tips;
    showTip(tips, false);
}
void LoginDialog::DelTipErr(TipErr te){
    _tip_errs.remove(te);
    if(_tip_errs.empty()){
      ui->err_tip->clear();
      return;
    }

    showTip(_tip_errs.first(), false);
}

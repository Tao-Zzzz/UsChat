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
#include "faceauthmgr.h"
#include <QTimer>

// 矩阵转Json
extern QJsonArray MatToJsonArray(const cv::Mat& mat);


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


    // 注册处理: Python 1:N 搜索结果回包
    _handlers.insert(ReqId::ID_FACE_SEARCH, [this](QJsonObject jsonObj){
        // 【核心防御】：如果用户已经关闭了人脸识别窗口，直接屏蔽丢弃此响应！
        if (!m_bFaceLoginActive) {
            qDebug() << "Face scan was cancelled by user, ignoring server response.";
            return;
        }

        int error = jsonObj["error"].toInt();

        if(error != 0) {
            // 匹配失败，解锁请求状态，让定时器自动去抓取下一帧重试
            qDebug() << "匹配失败或模糊，准备自动重试下一帧...";
            m_bRequestingFace = false;
            return;
        }

        // 匹配成功！拿到 Python 服务器算出来的 UID
        int uid = jsonObj["uid"].toInt();
        qDebug() << "Face match success! UID:" << uid;

        // 成功后，立刻关闭摄像头窗口
        stop_face_scan();
        showTip(tr("身份确认成功，正在连接聊天服务..."), true);

        // 组装新的请求，发给你原有的 C++ 后端走登录长连接逻辑
        QJsonObject loginObj;
        loginObj["uid"] = uid;

        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/face_login"),
                                            loginObj,
                                            ReqId::ID_LOGIN_USER,
                                            Modules::LOGINMOD);
    });

    // 注册处理: 人脸录入结果回包 (可选，用于在界面提示)
    _handlers.insert(ReqId::ID_FACE_REGISTER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error == 0) {
            // 你也可以用 QMessageBox 提示
            qDebug() << "云端人脸特征录入成功!";
        } else {
            qDebug() << "云端人脸特征录入失败!";
        }
    });
}

// 辅助函数：安全关闭摄像头和清理定时器
void LoginDialog::stop_face_scan()
{
    m_bFaceLoginActive = false; // 关闭状态
    if (m_faceTimer) {
        m_faceTimer->stop();
    }
    if (m_faceCap.isOpened()) {
        m_faceCap.release();
    }
    cv::destroyAllWindows();
    cv::waitKey(1); // 让 OpenCV 处理完销毁事件
    enableBtn(true);
}



// 定时器槽函数：处理每一帧画面
void LoginDialog::slot_process_face_frame()
{
    // 1. 检查用户是否手动关闭了 OpenCV 的窗口 (点击了 X)
    // getWindowProperty 如果返回 -1 表示窗口不存在
    if (cv::getWindowProperty("Face Scan Login", cv::WND_PROP_AUTOSIZE) == -1) {
        stop_face_scan();
        showTip(tr("人脸登录已取消"), false);
        return;
    }

    cv::Mat frame;
    m_faceCap >> frame;
    if (frame.empty()) return;

    // 画面提示文字：如果正在发请求就显示 Verifying
    QString statusTxt = m_bRequestingFace ? "Verifying..." : "Scanning...";
    cv::putText(frame, statusTxt.toStdString(), cv::Point(50, 50),
                cv::FONT_HERSHEY_SIMPLEX, 1,
                m_bRequestingFace ? cv::Scalar(0, 255, 255) : cv::Scalar(0, 255, 0), 2);
    cv::imshow("Face Scan Login", frame);

    // 检查是否按了 ESC 键
    if (cv::waitKey(1) == 27) {
        stop_face_scan();
        showTip(tr("人脸登录已取消"), false);
        return;
    }

    // 2. 如果正在等待云端结果，只刷新画面，不去提取特征和发送网络请求
    if (m_bRequestingFace) return;

    // ================= 新增：活体检测 =================
    // 假设你在 FaceAuthMgr 中实现了 CheckLiveness
    float liveScore = FaceAuthMgr::GetInstance()->CheckLiveness(frame);

    qDebug() << "live Score = " << liveScore;

    // 设置一个阈值，比如 < 0.8 认为是假脸（照片/屏幕）
    if (liveScore < 0.8f) {
        cv::putText(frame, "Spoofing Detected!", cv::Point(50, 100),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
        cv::imshow("Face Scan Login", frame);
        return; // 直接 return，不提取特征，也不发请求
    }
    // =================================================

    // 3. 提取特征
    cv::Mat targetFeature = FaceAuthMgr::GetInstance()->ExtractFeature(frame);

    // 如果这一帧没抓到脸，直接返回，等下一个 100ms
    if (targetFeature.empty()) return;

    // 抓到脸了！锁住请求状态，发送给服务器
    m_bRequestingFace = true;

    QJsonArray featureArray = MatToJsonArray(targetFeature);
    QJsonObject jsonObj;
    jsonObj["feature"] = featureArray;

    HttpMgr::GetInstance()->PostHttpReq(QUrl("http://127.0.0.1:8010/api/face/search"),
                                        jsonObj,
                                        ReqId::ID_FACE_SEARCH,
                                        Modules::LOGINMOD);
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
    if (m_bFaceLoginActive) return; // 防止重复点击

    if (!m_faceCap.open(0)) {
        QMessageBox::warning(this, "错误", "无法打开摄像头！");
        return;
    }

    cv::namedWindow("Face Scan Login", cv::WINDOW_AUTOSIZE);

    // 初始化状态
    m_bFaceLoginActive = true;
    m_bRequestingFace = false;
    enableBtn(false);
    showTip(tr("请正对摄像头..."), true);

    if (!m_faceTimer) {
        m_faceTimer = new QTimer(this);
        connect(m_faceTimer, &QTimer::timeout, this, &LoginDialog::slot_process_face_frame);
    }
    m_faceTimer->start(100); // 每 100ms 抓一帧
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

#include "mainwidget.h"
#include "ui_mainwidget.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    // 禁止窗口改变尺寸大小
    this->setFixedSize(this->geometry().size());

    // 去掉标题栏
    this->setWindowFlag(Qt::FramelessWindowHint);


    // 1:判断数据库连接是否存在，存在就得到连接，如果不存在添加得到链接
    if(QSqlDatabase::contains("sql_default_connection"))
    {
        // 根据数据库默认连接名称得到连接
        db=QSqlDatabase::database("sql_default_connection");
    }
    else
    {
        db=QSqlDatabase::addDatabase("QSQLITE"); // 添加数据库，得到该数据库的默认连接
        db.setDatabaseName("mp3listdatabase.db"); // 设置数据库文件名称
    }

    // 2:打开数据库，打开标识（QSqlQuery类
    if(!db.open())
    {
        QMessageBox::critical(0,QObject::tr("Open Data Error."),db.lastError().text());
    }
    else
    {
        // 3:定义query对象，得到打开的数据库标识
        QSqlQuery query;

        QString sql="create table if not exists searchlist(id integer,songname text,singername text,album_id text,hash text)";

        if(!query.exec(sql))
        {
            QMessageBox::critical(0,QObject::tr("create searchlist Error."),db.lastError().text());
        }

        // 歌曲痕迹数据表
        sql="create table if not exists historysong(id integer primary key autoincrement,songname text,singername text,album_id text,hash text)";

        if(!query.exec(sql))
        {
            QMessageBox::critical(0,QObject::tr("create historysong Error."),db.lastError().text());
        }

        // 查询历史数据表中的插入歌曲数据
        sql="select *from historysong;";
        if(!query.exec(sql))
        {
            QMessageBox::critical(0,QObject::tr("select historysong Error."),db.lastError().text());
        }

        while(query.next())
        {
            QString songname,singername;
            QSqlRecord rec=query.record();
            int ablumkey=rec.indexOf("songname");
            int hashkey=rec.indexOf("singername");
            songname=query.value(ablumkey).toString();
            singername=query.value(hashkey).toString();

            QString strshow=songname + "--" +singername;

            QListWidgetItem *item=new QListWidgetItem(strshow);
            ui->listWidget_History->addItem(item);
        }
    }

    // 播放操作
    player=new QMediaPlayer;
    playerlist=new QMediaPlaylist;

    connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(updateDuration(qint64)));
    connect(this, SIGNAL(lyricShow(QString)), this, SLOT(lyricTextShow(QString)));

    connect(ui->listWidget_Search, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(playSearchMusic()));
    connect(ui->listWidget_History, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(playHistoryMusic()));

    num=0;



    // “更换软件皮肤”的菜单项
    QAction *actionbackgroundtoDefault=new QAction(QIcon(":/new/prefix1/images/default.png"),u8"Default");
    connect(actionbackgroundtoDefault,&QAction::triggered,this,&MainWidget::backgroundtoDefault);


    QAction *actionbackgroundtoSetting=new QAction(QIcon(":/new/prefix1/images/setting.png"),u8"Custom");
    connect(actionbackgroundtoSetting,&QAction::triggered,this,&MainWidget::backgroundtoSetting);


    menuchangeSkin=new QMenu(this);
    menuchangeSkin->addAction(actionbackgroundtoDefault);
    menuchangeSkin->addAction(actionbackgroundtoSetting);

    backgroundtoDefault(); // 更换到默认背景皮肤



    // 系统托盘的初始化操作
    initsystemtrayIcon();

}

//播放进度调整
static bool i = true;

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::paintEvent(QPaintEvent *event)
{
   // QPainter painter(this);

   // painter.drawPixmap(0,0,width(),height(),QPixmap(":/new/prefix1/images/musicplayer_ui.jpg"));

}



void MainWidget::on_pushButton_Close_clicked()
{
    // 关闭窗口
    close();
}

void MainWidget::on_pushButton_Skin_clicked() // OK
{
    // 自已完成
    menuchangeSkin->exec(QCursor::pos());

}

void MainWidget::on_pushButton_About_clicked() // OK
{
    QMessageBox::about(this,"About","MP3音乐播放器搜索引擎\n");

}

// 搜索音乐
void MainWidget::on_pushButton_Search_clicked() // OK
{
    // 将原有歌曲数据清空
    ui->listWidget_Search->clear();

    // 先清理数据库中已经存储的 hash等数据
    QSqlQuery query;
    QString sql = "delete from searchlist;";

    if(!query.exec(sql))
    {
        QMessageBox::critical(0,QObject::tr("Delete searchlist Error."),db.lastError().text());
    }

    // 根据用户输入的MP3音乐名称，发起请求操作
    QString url = kugouSearchApi + QString("format=json&keyword=%1&page=1&pagesize=20&showtype=1").arg(ui->lineEdit_Search->text());

    httpAccess(url);

    QByteArray JsonData;
    QEventLoop loop;

    auto c = connect(this, finish, [&](const QByteArray & data)
    {
        JsonData = data;
        loop.exit(1);
    });

    loop.exec();
    disconnect(c);

    //解析网页回复的数据，将搜索得到的音乐hash和album_id与列表的索引值存放到数据库
    hashJsonAnalysis(JsonData);

}

void MainWidget::on_VopSlider_valueChanged(int value) // 音量调整 // OK
{
    player->setVolume(value);
    ui->label_Vop->setText(QString::number(value));

}


void MainWidget::on_progressSlider_valueChanged(int value) // OK
{
    QTime time(0,value/60000,qRound((value%60000)/1000.0));

    ui->label_Time->setText(time.toString("mm:ss"));
    if(i==false)
    {
        player->setPosition(qint64(value));
    }

}

void MainWidget::on_progressSlider_sliderPressed() // OK
{
    i=false;
}

void MainWidget::on_progressSlider_sliderReleased() // OK
{
    i=true;
}

// 上一曲
void MainWidget::on_pushButton_Front_clicked() // OK
{
    row--;
    if(row<0)
    {
        row=ui->listWidget_History->count();
    }

    // 查询搜索数据库历史记录表当中存储音乐数据信息
    QSqlQuery query;
    QString sql=QString("select *from historysong where id=%1;").arg(row+1);
    if(!query.exec(sql))
    {
        QMessageBox::critical(0,QObject::tr("select historysong Error."),db.lastError().text());
    }

    QString album_id,hash;
    while(query.next())
    {
        QSqlRecord recd=query.record();
        int ablumkey=recd.indexOf("album_id");
        int hashkey=recd.indexOf("hash");

        album_id=query.value(ablumkey).toString();
        hash=query.value(hashkey).toString();
    }
    downloadPlayer(album_id,hash);

}

void MainWidget::on_pushButton_Playpause_clicked() // OK
{
    // 播放 暂停图标大家自已去完成（实现切换）
    if(player->state()==QMediaPlayer::PlayingState)
    {
        player->pause();
    }
    else if(player->state()==QMediaPlayer::PausedState)
    {
        player->play();
    }

}

void MainWidget::on_pushButton_Next_clicked() // OK
{
    row++;
    if(row>ui->listWidget_History->count())
    {
        row=0;
    }

    // 查询搜索数据库历史记录表当中存储音乐数据信息
    QSqlQuery query;
    QString sql=QString("select *from historysong where id=%1;").arg(row+1);
    if(!query.exec(sql))
    {
        QMessageBox::critical(0,QObject::tr("select historysong Error."),db.lastError().text());
    }

    QString album_id,hash;
    while(query.next())
    {
        QSqlRecord recd=query.record();
        int ablumkey=recd.indexOf("album_id");
        int hashkey=recd.indexOf("hash");

        album_id=query.value(ablumkey).toString();
        hash=query.value(hashkey).toString();
    }
    downloadPlayer(album_id,hash);

}

void MainWidget::on_pushButton_Loopplay_clicked()
{
    // Todo 循环播放

}


// 更新播放的进度条和显示的时间
void MainWidget::updateDuration(qint64 value) // OK
{
    ui->progressSlider->setRange(0, player->duration());
    ui->progressSlider->setValue(value);
}

// 读取网络数据的槽函数
void MainWidget::netReply(QNetworkReply *reply) // OK
{
    // 获取响应的信息，状态码为200属于正常
    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    qDebug() << status_code;

    reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    // 没有错误返回
    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray data = reply->readAll();
        emit finish(data);
    }
    else
    {
        qDebug()<< reply->errorString();
    }

    reply->deleteLater();

}

// 显示歌词的槽函数
void MainWidget::lyricTextShow(QString lycris) // OK
{   
    ui->textBrowser_Lyric->setFont(QFont("宋体", 10, QFont::Bold));
    ui->textBrowser_Lyric->setTextColor(Qt::white);
    ui->textBrowser_Lyric->setText(lycris);

}

// 音乐歌曲下载和播放
void MainWidget::downloadPlayer(QString album_id,QString hash) // OK
{
    QString url = kugouDownloadApi + QString("r=play/getdata"
                                             "&hash=%1&album_id=%2"
                                             "&dfid=1spkkh3QKS9P0iJupz0oTy5G"
                                             "&mid=de94e92794f31e9cd6ff4cb309d2baa2"
                                             "&platid=4").arg(hash).arg(album_id);

    httpAccess(url);

    QByteArray JsonData;
    QEventLoop loop;
    auto d = connect(this, finish, [&](const QByteArray & data){
        JsonData = data;
        loop.exit(1);
    });
    loop.exec();
    disconnect(d);

    // 解析将要播放音乐
    QString music = musicJsonAnalysis(JsonData);

    player->setMedia(QUrl(music));

    // 设置音量
    player->setVolume(100);

    // 设置音量的滑动条
    ui->VopSlider->setValue(100);

    // 播放音乐
    player->play();

}

// 访问HTTP网页
void MainWidget::httpAccess(QString url) // OK
{
    // 实例化网络请求操作事项
    request = new QNetworkRequest;

    // 将url网页地址存入 request请求当中
    request->setUrl(url);

    // 实例化网络管理（访问）
    manager = new QNetworkAccessManager;

    // 通过get方法，上传具体的请求
    manager->get(*request);

    // 当网页回复消息时，触发 finished信号，我们才能够读取数据信息
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(netReply(QNetworkReply*)));
}

// 音乐的hash和ablum_id值解析，使用JSON
void MainWidget::hashJsonAnalysis(QByteArray JsonData) // OK
{
    QJsonDocument document = QJsonDocument::fromJson(JsonData);

    if ( document.isObject())
    {
        QJsonObject data = document.object();

        if (data.contains("data"))
        {
            QJsonObject objectInfo = data.value("data").toObject();
            if (objectInfo.contains("info"))
            {
                QJsonArray objectHash = objectInfo.value("info").toArray();

                for (int i = 0; i < objectHash.count(); i++)
                {
                    QString songname, singername, album_id, hash;
                    QJsonObject album = objectHash.at(i).toObject();
                    if (album.contains("album_id"))
                    {
                        album_id = album.value("album_id").toString();

                    }

                    if (album.contains("singername"))
                    {
                        singername = album.value("singername").toString();

                    }

                    if (album.contains("songname"))
                    {
                        songname = album.value("songname").toString(); // 歌曲名称

                    }

                    if (album.contains("hash"))
                    {
                        hash = album.value("hash").toString();

                    }



                    QSqlQuery query;
                    QString sql = QString("insert into searchList values(%1, '%2', '%3', '%4', '%5')").arg(QString::number(i)).arg(songname).arg(singername).arg(album_id).arg(hash);

                    if ( !query.exec(sql))
                    {
                        QMessageBox::critical(0, QObject::tr("insert Error"),db.lastError().text());
                    }

                    // 将解析的音乐名称，存入listWidget_Search控件列表进行显示
                    QString show = songname + "--" + singername;
                    QListWidgetItem *item = new QListWidgetItem(show);
                    ui->listWidget_Search->addItem(item);
                }
            }
        }
    }

    if(document.isArray())
    {
        qDebug()<<"Array";
    }


}

// 搜索的音乐数据信息JSON解析，解析出真正的音乐文件和歌词
QString MainWidget::musicJsonAnalysis(QByteArray JsonData) // OK
{
    QJsonDocument document = QJsonDocument::fromJson(JsonData);
    if ( document.isObject())
    {
        QJsonObject data = document.object();
        if (data.contains("data"))
        {
            QJsonObject objectPlayUrl = data.value("data").toObject();

            if (objectPlayUrl.contains("lyrics"))
            {

                emit lyricShow(objectPlayUrl.value("lyrics").toString());
            }

            if (objectPlayUrl.contains("play_url"))
            {
                return objectPlayUrl.value("play_url").toString();
            }
        }

        if (document.isArray())
        {
            qDebug() << "Array.";
        }
    }

}


void MainWidget::playSearchMusic() // 双击搜索列表，播放音乐 // OK
{    
    // 获取双击的歌曲对应索引，就是数据库当中数据表的ID号
    int row=ui->listWidget_Search->currentRow();
    qDebug()<<"row-->"<<row;

    // 查询搜索数据库中数据表中存储的音乐的数据信息
    QSqlQuery query;
    QString sql=QString("select *from searchlist where id=%1;").arg(row);
    if(!query.exec(sql))
    {
        QMessageBox::critical(0,QObject::tr("select searchlist table Error."),db.lastError().text());

    }

    // 将选中的音乐的数据信息存入历史数据表
    QString songname,singername,album_id,hash;
    while(query.next())
    {
        QSqlRecord recd=query.record();
        int songkey=recd.indexOf("songname");
        int singerkey=recd.indexOf("singername");
        int ablumkey=recd.indexOf("album_id");
        int hashkey=recd.indexOf("hash");

        songname=query.value(songkey).toString();
        singername=query.value(singerkey).toString();
        album_id=query.value(ablumkey).toString();
        hash=query.value(hashkey).toString();

        sql=QString("select hash from historysong where hash='%1';").arg(hash);
        if(!query.exec(sql))
        {
            QMessageBox::critical(0,QObject::tr("select hash Error."),db.lastError().text());

        }

        if(query.next()==NULL)
        {
            sql=QString("insert into historysong values(NULL,'%1','%2','%3','%4')").arg(songname).arg(singername).arg(album_id).arg(hash);
            if(!query.exec(sql))
            {
                QMessageBox::critical(0,QObject::tr("insert historysong Error."),db.lastError().text());
            }

            // 将解析的音乐名称，保存listWidget_History列表控件当中
            QString show=songname + "--" +singername;
            QListWidgetItem *item=new QListWidgetItem(show);
            ui->listWidget_History->addItem(item);
        }
    }

    downloadPlayer(album_id,hash);

}

void MainWidget::playHistoryMusic() // 双击历史播放列表，播放音乐 // OK
{
    // 我们要获取双击的哪一行的索引，其实就是数据库中数据表的id编号
    row=ui->listWidget_History->currentRow();
    // qDebug()<<"row-->"<<row;

    // 查询搜索数据库中数据表的历史记录存储的音乐数据信息
    QSqlQuery query;
    QString sql=QString("select *from historysong where id = %1;").arg(row+1);
    if(!query.exec(sql))
    {
        QMessageBox::critical(0,QObject::tr("select historysong Error."),db.lastError().text());

    }

    QString album_id,hash;
    while(query.next())
    {
        QSqlRecord recd=query.record();
        int ablumkey=recd.indexOf("album_id");
        int hashkey=recd.indexOf("hash");

        album_id=query.value(ablumkey).toString();
        hash=query.value(hashkey).toString();
    }
    downloadPlayer(album_id,hash);

}







void MainWidget::mouseMoveEvent(QMouseEvent *event) // 移动事件 // OK
{
    if(mousePress)
    {
        QPoint movepos=event->globalPos(); // 窗口初始位置
        move(movepos-movePoint);
    }

}

void MainWidget::mouseReleaseEvent(QMouseEvent *event) // 释放事件 // OK
{
    Q_UNUSED(event)
    mousePress=false;
}

void MainWidget::mousePressEvent(QMouseEvent *event) // 按下事件 // OK
{
    if(event->button()==Qt::LeftButton)
    {
        mousePress=true;
    }

    movePoint=event->globalPos()-pos();
}


void MainWidget::backgroundtoDefault() // 更换到默认背景皮肤
{
    // 获取widget的palette
    QPalette palette=this->palette();
    palette.setBrush(QPalette::Window,
                     QBrush(QPixmap(":/new/prefix1/images/musicplayer_ui.jpg").scaled( // 维族背景图
                                this->size(),Qt::IgnoreAspectRatio, // 不限制原图片的长宽比例
                                 Qt::SmoothTransformation))); // 使用平滑的缩放方式

    this->setPalette(palette);

}

void MainWidget::backgroundtoSetting() // 自户自定义背景
{
    // 选择打开图片作为皮肤
    QString strFileName=QFileDialog::getOpenFileName(this,"请选择自定义背景图片",
            QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).first(),u8"图片(*.jpg *png)");

    // 获取widget的palette
    QPalette palette=this->palette();
    palette.setBrush(QPalette::Window,
                     QBrush(QPixmap(strFileName).scaled( // 维族背景图
                                this->size(),Qt::IgnoreAspectRatio, // 不限制原图片的长宽比例
                                 Qt::SmoothTransformation))); // 使用平滑的缩放方式

    this->setPalette(palette);

}


// 响应系统托盘的动作（双击操作）
void MainWidget::systemtrayiconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::DoubleClick:
        // 显示 隐藏 界面
        if(isHidden())
        {
            show();
        }
        else
        {
            hide();
        }
        break;

    default:
        break;
    }
}

// 系统托盘的初始化操作
void MainWidget::initsystemtrayIcon()
{
    mysystemTray=new QSystemTrayIcon(this);
    mysystemTray->setIcon(QIcon(":new/prefix1/images/pase-hover.png"));
    connect(mysystemTray,&QSystemTrayIcon::activated,this,&MainWidget::systemtrayiconActivated);


    // 添加退出应用程序菜单
    QAction *actionsystemquit=new QAction(QIcon(":/new/prefix1/images/quit.png"),u8"退出程序");
    connect(actionsystemquit,&QAction::triggered,this,&MainWidget::quitmusicplayer);

    QMenu *pcontextmenu=new QMenu(this);
    pcontextmenu->addAction(actionsystemquit);
    mysystemTray->setContextMenu(pcontextmenu);
    mysystemTray->show();
}

// 退出应用程序
void MainWidget::quitmusicplayer()
{
    QCoreApplication::quit();

}

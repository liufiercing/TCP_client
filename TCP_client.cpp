// TCP_client.cpp
#include "TCP_client.h"
#include "ui_TCP_client.h"

Widget::Widget(QWidget *parent) : QWidget(parent),
                                  ui(new Ui::Widget)
{
    ui->setupUi(this);
    setWindowTitle("TCP 客户端");

    fileThread = new QThread(this);
    connect(fileThread, SIGNAL(started()), this, SLOT(onThreadStarted()));
    connect(fileThread, SIGNAL(finished()), this, SLOT(onThreadFinished()));

    connect(this, SIGNAL(runBufToFile(const int *, int)), this, SLOT(bufToFile(const int *, int)));

    // 启动线程
    fileThread->start();

    tcpSocket = new QTcpSocket(this);
    SaveBuf = QVector<QVector<int>>(2, QVector<int>(DATA_BUF_LEN, 0));

    // 默认设置
    ui->IPEdit->setText("192.168.2.101");
    //    ui->IPEdit->setText("127.0.0.1");
    ui->portEdit->setText("1973");

    ui->closeBT->setEnabled(false);
    ui->sendBT->setEnabled(false);

    ui->checkBox_hex->setChecked(true);
    ui->checkBox_new_line->setChecked(true);
    ui->checkBox_hex_send->setChecked(true);

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(connected_Slot()));

    // connect(ui->checkBox_save_file, SIGNAL(clicked()), this, SLOT(recvtofile()));
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(read_to_buf_and_show()));

    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(displayError(QAbstractSocket::SocketError)));

    connect(tcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
            this, SLOT(socketStateChange(QAbstractSocket::SocketState)));
}
Widget::~Widget()
{
    // refreshibuftofile();
    delete ui;
}

void Widget::displayError(QAbstractSocket::SocketError socketError)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<QAbstractSocket::SocketError>();
    QString errorString = metaEnum.valueToKey(socketError);

    // 将错误字符串追加到 ui->logEdit
    ui->logEdit->appendPlainText("套接字错误：" + errorString);
}

// 连接服务器

void Widget::on_openBT_clicked()
{
    // 取消已有的连接
    tcpSocket->abort();

    if (tcpSocket->state() != QAbstractSocket::ConnectedState)
    {
        QString targetHostname = ui->IPEdit->text();
        quint16 targetPort = ui->portEdit->text().toUInt();
        tcpSocket->connectToHost(targetHostname, targetPort);

        // 等待连接成功
        if (!tcpSocket->waitForConnected(2000))
        {
            ui->logEdit->appendPlainText("Connection failed!");
            return;
        }
        ui->openBT->setEnabled(false);
        ui->closeBT->setEnabled(true);
        ui->sendBT->setEnabled(true);
    }
}

void Widget::connected_Slot()
{
    if (tcpSocket->state() == QAbstractSocket::ConnectedState)
    {
        ui->logEdit->appendPlainText("连接服务器成功");
    }
    else
    {
        QString errorString = tcpSocket->errorString();
        ui->logEdit->appendPlainText("连接服务器失败，错误信息：" + errorString);
    }
}

void Widget::on_closeBT_clicked()
{
    if (tcpSocket->state() == QAbstractSocket::ConnectedState)
    {
        // 如果处于连接状态，则断开连接
        tcpSocket->disconnectFromHost();
    }
    ui->logEdit->appendPlainText("关闭服务器");
    ui->openBT->setEnabled(true);
    ui->closeBT->setEnabled(false);

    // refreshibuftofile();

    if (fileThread->isRunning())
    {
        fileThread->quit();
        fileThread->wait();
    }
    delete fileThread;
}

//// 接收 仅显示在UI上
// void Widget::readyRead_Slot()
//{
//     QByteArray data = tcpSocket->readAll();
//     recvtofile_update_ui(data);
// }

void Widget::getsavefilepath()
{
    // 打开文件
    // QMessageBox::information(this,"成功","选择文件");
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    currentFilePath = QFileDialog::getSaveFileName(this, "选择保存文件路径", desktopPath, "Dat files (*.dat)");
    if (currentFilePath.isEmpty())
    {
        QMessageBox::warning(this, "警告", "选择文件");
        ui->checkBox_save_file->setChecked(false);
        //       ui->closeBT->setEnabled(true);
    }
    else
    {

        QString show_name_org = QString("文件路径：%1").arg(currentFilePath);
        ui->logEdit->appendPlainText(show_name_org);
        ui->closeBT->setEnabled(false);
    }
}

void Widget::buf_to_file(const int *data, int length)
{
    // 加锁
    QMutexLocker locker(&fileMutex);

    // 打开文件
    QFile file(currentFilePath);
    if (file.open(QFileDevice::Append))
    {

        createNewFileIfNecessary(file);

        // 循环将每个元素转换为quint8并写入文件
        for (int i = 0; i < length; ++i)
        {
            quint8 byteToWrite = static_cast<quint8>(data[i]);
            file.write(reinterpret_cast<const char *>(&byteToWrite), sizeof(quint8));
        }

        // 手动关闭文件
        file.close();
    }

    // 解锁将在 QMutexLocker 超出范围时自动执行
}

void Widget::read_to_buf_and_show()
{
    QByteArray data = tcpSocket->readAll();
    dataSize = data.size();
    if (ui->checkBox_save_file->isChecked())
    {
        AppendData(data, dataSize);
    }
    else
    {
        // 显示在 UI 上
        recvtofile_update_ui(data);
    }
}

void Widget::refreshibuftofile()
{
    if (SaveBufPtr)
    {
        // QtConcurrent::run(this, &Widget::buf_to_file, SaveBuf[SaveBufNo].data(), SaveBufPtr);
        emit runBufToFile(SaveBuf[SaveBufNo].data(), SaveBufPtr);
    }
    SaveBufPtr = 0;
}

void Widget::createNewFileIfNecessary(QFile &file)
{

    one_file_size = ui->file_size->currentText().toInt();
    one_file_unit = get_one_file_units();

    // 检查文件大小 如果大了就新建 更新路径
    if (file.size() > (one_file_size * one_file_unit))
    {
        file.close();

        // 获取文件名和后缀
        QFileInfo fileInfo(currentFilePath);
        QString basePath = fileInfo.path(); // 获取文件所在路径
        QString baseName = fileInfo.baseName();
        QString suffix = fileInfo.completeSuffix();

        // 提取数字部分
        QRegularExpression regex("_(\\d+)");
        QRegularExpressionMatch match = regex.match(baseName);

        // 如果能够匹配到数字部分
        if (match.hasMatch())
        {
            QString numberStr = match.captured(1);
            int number = numberStr.toInt();
            // 生成新的文件名
            QString newFilePath = QString("%1/%2_%3.%4").arg(basePath).arg(baseName.left(match.capturedStart())).arg(number + 1).arg(suffix);
            // 打开新文件
            file.setFileName(newFilePath);
            qDebug() << "新文件路径：" << file.fileName(); // 打印新文件路径
            file.open(QFileDevice::WriteOnly | QFileDevice::Append);
            currentFilePath = newFilePath;
        }
        else
        {
            // 如果无法匹配到数字部分，使用默认的递增方式
            QString newFilePath = QString("%1/%2_%3.%4").arg(basePath).arg(baseName).arg(count).arg(suffix);

            // 打开新文件
            file.setFileName(newFilePath);
            qDebug() << "新文件路径：" << file.fileName(); // 打印新文件路径
            file.open(QFileDevice::WriteOnly | QFileDevice::Append);
            // 递增 count
            count++;
            currentFilePath = newFilePath;
        }
        // 更新当前文件路径

        QString show_name = QString("新建文件：%1").arg(file.fileName());
        ui->logEdit->appendPlainText(show_name);
    }
}

long long Widget::get_one_file_units()
{
    long long one_file_size_un;

    QString selectedUnit = ui->file_size_units->currentText().toLower().trimmed();

    // 根据选择的单位判断大小
    if (selectedUnit == "gb")
    {
        one_file_size_un = 1LL << 30; // 1 GB = 2^30 bytes
    }
    else if (selectedUnit == "mb")
    {
        one_file_size_un = 1LL << 20; // 1 MB = 2^20 bytes
    }
    else if (selectedUnit == "kb")
    {
        one_file_size_un = 1LL << 10; // 1 KB = 2^10 bytes
    }
    else
    {
        one_file_size_un = 1LL; // 默认为字节
    }

    return one_file_size_un;
}

void Widget::recvtofile_update_ui(QByteArray data)
{
    if (ui->checkBox_hex->isChecked())
    {
        QString hexString = QString::fromLatin1(data.toHex(' ').toUpper());
        if (ui->checkBox_new_line->isChecked())
        {

            ui->recvEdit->appendPlainText(hexString);
            qDebug() << "Hex String: " << hexString; // 添加这行输出
        }
        else
        {
            // 如果不需要换行显示，直接追加内容到当前行
            ui->recvEdit->textCursor().insertText(hexString);
        }
    }
    else
    {
        if (ui->checkBox_new_line->isChecked())
        {
            ui->recvEdit->appendPlainText(data);
        }
        else
        {
            // 如果不需要换行显示，直接追加内容到当前行
            ui->recvEdit->textCursor().insertText(QString::fromLocal8Bit(data));
        }
    }
}

void Widget::on_sendBT_clicked()
{
    if (tcpSocket->state() == QAbstractSocket::ConnectedState)
    {
        if (ui->checkBox_hex_send->isChecked())
        {
            // 获取用户输入的文本
            QString userInput = ui->sendEdit->text().trimmed();

            // 判断是否为十六进制
            bool isHex = isHexInput(userInput);

            // 如果是十六进制则发送
            if (isHex)
            {
                QByteArray hexData;
                QStringList hexBytes = userInput.split(' ');

                for (const QString &hexByte : hexBytes)
                {
                    hexData.append(static_cast<char>(hexByte.toUShort(nullptr, 16)));
                }

                tcpSocket->write(hexData);
            }
            else
            {
                // 如果不是十六进制则提示输入错误
                ui->logEdit->appendPlainText("输入错误，请输入十六进制数据，每个字节间隔空格，例如：12 A2 FF");
            }
        }
        else
        {
            tcpSocket->write(ui->sendEdit->text().toLocal8Bit());
        }
    }
    else
    {
        ui->logEdit->appendPlainText("未连接到服务器，无法发送数据。");
    }
}

bool Widget::isHexInput(const QString &input)
{
    bool isHex = true;
    QStringList hexBytes = input.split(' ');

    for (const QString &hexByte : hexBytes)
    {
        bool ok;
        hexByte.toUShort(&ok, 16);

        if (!ok)
        {
            isHex = false;
            break;
        }
    }
    return isHex;
}

void Widget::on_file_size_currentTextChanged(const QString &arg1)
{
    one_file_size = ui->file_size->currentText().toInt();
}

void Widget::on_file_size_units_currentTextChanged(const QString &arg1)
{
    one_file_unit = get_one_file_units();
}

void Widget::socketStateChange(QAbstractSocket::SocketState state)
{
    switch (state) // 不同状态打印不同信息
    {
    case QAbstractSocket::UnconnectedState:
        ui->logEdit->appendPlainText("scoket 状态：UnconnectedState");
        break;
    case QAbstractSocket::ConnectedState:
        ui->logEdit->appendPlainText("scoket 状态：ConnectedState");
        break;
    case QAbstractSocket::ConnectingState:
        ui->logEdit->appendPlainText("scoket 状态：ConnectingState");
        break;
    case QAbstractSocket::HostLookupState:
        ui->logEdit->appendPlainText("scoket 状态：HostLookupState");
        break;
    case QAbstractSocket::ClosingState:
        ui->logEdit->appendPlainText("scoket 状态：ClosingState");
        ui->closeBT->setEnabled(false);
        ui->sendBT->setEnabled(false);
        ui->openBT->setEnabled(true);
        // ui->checkBox_hex_send->setChecked(true);
        ui->checkBox_save_file->setChecked(false);
        refreshibuftofile();

        break;
    case QAbstractSocket::ListeningState:
        ui->logEdit->appendPlainText("scoket 状态：ListeningState");
        break;
    case QAbstractSocket::BoundState:
        ui->logEdit->appendPlainText("scoket 状态：BoundState");
        break;
    default:
        break;
    }
}

void Widget::on_checkBox_hex_send_clicked()
{
    QString userInput = ui->sendEdit->text().trimmed();
    bool isHex = isHexInput(userInput);

    if (!isHex)
    {
        // 将 ui->sendEdit 内容转换为十六进制，并替换 ui->sendEdit
        QByteArray hexData;
        for (QChar ch : userInput)
        {
            hexData.append(QString("%1 ").arg(static_cast<quint8>(ch.toLatin1()), 2, 16, QLatin1Char('0')));
        }

        ui->sendEdit->setText(hexData.trimmed().toUpper());
    }
}

void Widget::on_checkBox_save_file_clicked(bool checked)
{
    if (checked)
    {
        // disconnect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readyRead_Slot()));
        getsavefilepath();
        // 断开 readyRead 信号的连接

        // 禁用断开
    }
    else
    {

        ui->closeBT->setEnabled(true);

        refreshibuftofile();
    }
}

// 新增一个槽函数，该函数会在线程启动时调用
void Widget::onThreadStarted()
{
    // 将 buf_to_file 方法移动到新线程中执行
    moveToThread(fileThread);
}

// 新增一个槽函数，该函数会在线程结束时调用
void Widget::onThreadFinished()
{
    // 清理线程相关的资源
    // 比如，如果 buf_to_file 中使用了QObject子类，需要在这里销毁
}

// 修改 AppendData 方法，将 buf_to_file 方法发送到新线程执行
void Widget::AppendData(QByteArray source, int sourceLength)
{
    for (int n = 0; n < sourceLength; n++)
    {
        SaveBuf[SaveBufNo][SaveBufPtr++] = source.at(n);
        if (SaveBufPtr == DATA_BUF_LEN)
        {
            // 使用信号槽将 buf_to_file 方法发送到新线程执行
            emit runBufToFile(SaveBuf[SaveBufNo].data(), DATA_BUF_LEN);
            SaveBufPtr = 0;
            SaveBufNo ^= 1; // 切换子数组
        }
    }
}

// 新增一个槽函数，用于执行 buf_to_file 方法
void Widget::bufToFile(const int *data, int length)
{
    // 在这里调用 buf_to_file 方法
    buf_to_file(data, length);
}

void Widget::on_pushButton_clicked()
{
    ui->sendEdit->setText("3A 01 81");
}

void Widget::on_pushButton_2_clicked()
{
    ui->sendEdit->setText("3A 01 82");
}

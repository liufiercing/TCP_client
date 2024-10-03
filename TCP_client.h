#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <QWidget>
#include <QTcpSocket>

#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QMetaEnum>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QVector>
#include <QtConcurrent>
#include <QMutex>
#include <QThread>


#define DATA_BUF_LEN   0.5*1024*1024
//#define DATA_BUF_LEN   128
namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

    QTcpSocket *tcpSocket;

    void createNewFileIfNecessary(QFile& file);
    long long get_one_file_units();
    bool isHexInput(const QString& input);


private slots:
    void on_openBT_clicked();
    void connected_Slot();
    //void readyRead_Slot();

    void on_closeBT_clicked();

    void on_sendBT_clicked();
    void displayError(QAbstractSocket::SocketError socketError);  
    void getsavefilepath();
    void read_to_buf_and_show();
    void recvtofile_update_ui(QByteArray data);
    void buf_to_file(const int* data, int length);


    void on_file_size_currentTextChanged(const QString &arg1);

    void on_file_size_units_currentTextChanged(const QString &arg1);
    void socketStateChange(QAbstractSocket::SocketState state);

    void on_checkBox_hex_send_clicked();
    void AppendData(QByteArray source, int sourceLength);
    void refreshibuftofile();

    //void on_checkBox_save_file_released();

    void on_checkBox_save_file_clicked(bool checked);

//    void onThreadFinished();
//    void onThreadStarted();

    void bufToFile(const int* data, int length);

    void onThreadFinished();
   
    void onThreadStarted();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::Widget *ui;
    QString currentFilePath;
    int count = 1;
    int  one_file_size;
    long long one_file_unit;
    // 创建一个3行4列的二维数组
    QVector<QVector<int>> SaveBuf;
    int SaveBufNo = 0;
    int SaveBufPtr = 0;
    QMutex fileMutex;

    long long tcp_size_all = 0;
    int dataSize = 0;

    QThread* fileThread;

signals:
    void runBufToFile(const int* data, int length);

};

#endif // TCP_CLIENT_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <zmq.h>
#include <QSocketNotifier>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QPixmap>
#include <QVector>
#include "progressdialog.h"
#include "subscriber.h"
#include "dataprocesser.h"
namespace Ui {
class MainWindow;
}

#define MAX_ENVELOPE_LENGTH 255
#define MAX_DATA_LENGTH 1000

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public:
	/*
	cmd:strings prescribed in SDK document
	Encapsulate a ZMQ function without data
	*/
	bool request(const QString& cmd);
	/*
	cmd:strings prescribed in SDK document
	jsonObj:jsonpbject prescribed in SDK document
	Encapsulate a ZMQ function to send data
	*/
	bool request(const QString& cmd, const QJsonObject& jsonObj);
	/*
	socket:ZMQ socket
	Check if there is any data received
	*/
	bool hasMore(void* socket);
private slots:
//There are some SDK test function ,  refer to SDK Document
	void on_pushButton_DeviceCheck_clicked();// The button on the interface press to trigger,refer to SDK Doc
	
	void on_pushButton_pro_clicked();
	void on_pushButton_pro_plus_clicked();

	//Get cali information 
	void CaliGetTime();
	void CaliCurrentGroup();
	void CaliCurrentDist();
	void on_pushButton_GetInformation_clicked();
	void on_pushButton_enterCali_clicked();
	void on_pushButton_CaliExit_clicked();

	//set some cali status
	void on_pushButton_SetSnapEnabled_clicked();
	//void on_pushButton_CaliSetType_clicked();


    void onHeartbeat();//When the heartbeat stops,count to zero and start reporting errors
	/*
	majorCmd: main command
	minorCmd:secondary command
	data:Refer to SDK document,the data is different
	SDK publish informations,in this function to handle
	*/
    void onPublishReceived(QString majorCmd, QString minorCmd, QByteArray data);
	/*
	camID: image area displayed on the main interface
	pixmap:image data
	This function to show video
	*/
	void onVideoImageReady(int camID, QPixmap pixmap);

	void on_pushButton_Step1Next_clicked();
	void on_pushButton_Step2Next_clicked();
	void on_pushButton_Step2Back_clicked();
	void on_pushButton_Step3Next_clicked();
	void on_pushButton_Step3Back_clicked();
	void on_pushButton_Step4Back_clicked();

private:
	void resetCaliStatus();
	QVector<QWidget*> lineEdit_Group;
	QVector<QWidget*> widget_Step;

	void nextStep(int num);
	void backStep(int num);

public:
	/*
	socket:ZMQ socket
	cmd:envelop cmd
	data:if you need to send additional parameters ,data is not empty.
	Encapsulate a ZMQ function to send data
	*/
	bool sendData(void* socket, const QString& cmd, const QByteArray& data);
	static inline QByteArray jsonStr(const QJsonObject& jo){ return QJsonDocument(jo).toJson(QJsonDocument::Compact); }
    static inline QJsonObject jsonObject(const QByteArray& data) {auto doc = QJsonDocument::fromJson(data); return doc.object();}
private:
    Ui::MainWindow *ui;
	void* m_zmqContext = nullptr;
    void* m_zmqReqSocket = nullptr;
	void* m_zmqDataProcesserSocket = nullptr;

	QTimer* m_heartbeatTimer = nullptr;

    QThread* m_subscriberThread = nullptr;
    Subscriber* m_subscriber = nullptr;
    ProgressDialog* m_progressDialog = nullptr;
	QThread* m_dataProcesserThread = nullptr;
	DataProcesser* m_dataProcesser = nullptr;

	

public slots:



protected:
	void closeEvent(QCloseEvent *event);//When the main thread  exit,it exits the sub-thread
	
	
};

#endif // MAINWINDOW_H

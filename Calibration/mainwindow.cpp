#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtDebug>
#include <cassert>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QSharedMemory>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_progressDialog = new ProgressDialog(this);

	m_zmqContext = zmq_ctx_new();
	auto err = zmq_strerror(zmq_errno());

    m_subscriberThread = new QThread(this);
    m_subscriber = new Subscriber(this, m_zmqContext);
    m_subscriber->moveToThread(m_subscriberThread);
    connect(m_subscriberThread, &QThread::finished, m_subscriber, &QObject::deleteLater);
    connect(m_subscriber, &Subscriber::heartbeat, this, &MainWindow::onHeartbeat, Qt::QueuedConnection);
	connect(m_subscriber, &Subscriber::publishReceived, this, &MainWindow::onPublishReceived, Qt::QueuedConnection);
    m_subscriberThread->start();
	

	m_dataProcesserThread = new QThread(this);
	m_dataProcesser = new DataProcesser(this, m_zmqContext);
	m_dataProcesser->moveToThread(m_dataProcesserThread);
	connect(m_dataProcesserThread, &QThread::finished, m_dataProcesser, &QObject::deleteLater);
	connect(m_dataProcesser, &DataProcesser::videoImageReady, this, &MainWindow::onVideoImageReady, Qt::QueuedConnection);
	m_dataProcesserThread->start();

	QMetaObject::invokeMethod(m_subscriber, "setup", Qt::QueuedConnection, Q_ARG(QString, "tcp://localhost:11398"));

	m_zmqReqSocket = zmq_socket(m_zmqContext, ZMQ_REQ);
    auto rc = zmq_connect(m_zmqReqSocket, "tcp://localhost:11399");
	assert(!rc);


	m_heartbeatTimer = new QTimer(this);
	m_heartbeatTimer->setInterval(210 );
	connect(m_heartbeatTimer, &QTimer::timeout, this, [&]{
		auto currentCount = ui->lcdNumber->intValue();
		if (currentCount == 0){
			m_heartbeatTimer->stop();
			QMessageBox::critical(this, "ERROR", "The platform died!");
		}
		else{
			currentCount--;
			ui->lcdNumber->display(currentCount);
		}
	});
	m_heartbeatTimer->start();

	QTimer::singleShot(0, this, [&]{
		m_progressDialog->onBeginAsync("Pulling...");
		QCoreApplication::processEvents();
		const char *sendData = "v1.0/pull";
		int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);


		char buf[MAX_DATA_LENGTH * 2+ 1] = { 0 };
		nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH * 2, 0);
		auto jsonDoc = QJsonDocument::fromJson(buf);
		qDebug() << "pull results:" << jsonDoc;
		assert(!hasMore(m_zmqReqSocket));
		m_progressDialog->onFinishAsync();
		QCoreApplication::processEvents();
	});
	//init
	ui->widget->setEnabled(false);
	ui->comboBox_CaliType->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
	zmq_close(m_zmqReqSocket);
	zmq_ctx_destroy(m_zmqContext);
}

bool MainWindow::request(const QString& cmd, const QJsonObject& jsonObj)
{
	return sendData(m_zmqReqSocket, cmd, jsonStr(jsonObj));
}

bool MainWindow::request(const QString& cmd)
{
	return sendData(m_zmqReqSocket, cmd, "");
}

bool MainWindow::hasMore(void* socket)
{
	int more = 0;
	size_t moreSize = sizeof(more);
	int rc = zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &moreSize);
	qDebug() << "hasMore:" << more <<"andRc:"<<rc<< endl;

	return more != 0;
}


void MainWindow::on_pushButton_DeviceCheck_clicked()
{
	const char *sendData = "v1.0/device/check";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);


	int result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	qDebug() << "recv reply data:" << (result == 0 ? false : true);
	if (result != 0)
	{
		ui->widget->setEnabled(true);
	}
	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::CaliGetTime()
{
	const char *sendData = "v1.0/cali/time";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);
	qlonglong result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	auto dt = QDateTime::fromSecsSinceEpoch(result);
	ui->label_CaliTime->setText(dt.toString("HH:MM:ss yyyy-MM-dd"));
	qDebug() << "recv reply data:" << dt.toString("HH:MM:ss yyyy-MM-dd");
	assert(!hasMore(m_zmqReqSocket));
}
//
void MainWindow::CaliCurrentGroup()
{
	const char *sendData = "v1.0/cali/currentCaliGroup";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);
	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH, 0);
	int num;
	memcpy(&num, buf, sizeof(int));
	qDebug() << "cali currentCaliGroup:" << num;
	ui->label_CaliGroup->setText(QString::number(num));
	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::CaliCurrentDist()
{
	const char *sendData = "v1.0/cali/currentCaliDist";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);
	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH, 0);
	int num;
	memcpy(&num, buf, sizeof(int));
	qDebug() << "cali currentCaliDist:" << num;
	ui->label_CaliDistance->setText(QString::number(num));
	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::on_pushButton_GetInformation_clicked()
{
	 CaliGetTime();
	 CaliCurrentGroup();
	 CaliCurrentDist();
}

void MainWindow::on_pushButton_enterCali_clicked()
{
	const char *sendData = "v1.0/cali/enter";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);
	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH, 0);
	int num;
	memcpy(&num, buf, sizeof(int));
	auto valBool = num == 0 ? false : true;
	qDebug() << "cali enterCali:" << valBool;

	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::on_pushButton_CaliExit_clicked()
{
	const char *sendData = "v1.0/cali/exit";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);
	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH, 0);
	int num;
	memcpy(&num, buf, sizeof(int));
	auto valBool = num == 0 ? false : true;
	qDebug() << "cali exitCali:" << valBool;

	assert(!hasMore(m_zmqReqSocket));
	resetCaliStatus();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	
	m_subscriberThread->quit();
	m_subscriberThread->exit(0);
	connect(m_subscriberThread, SIGNAL(finished()), m_subscriberThread, SLOT(deleteLater()));
	
	exit(0);
	MainWindow::closeEvent(event);
}



void MainWindow::on_pushButton_SetSnapEnabled_clicked()
{
	char set;
	set = '1';
	QString  cmd = QStringLiteral("v1.0/cali/snapEnabled/set") ;
	qDebug() << "cmd:" << cmd.toStdString().c_str() << endl;
	char buf[MAX_DATA_LENGTH + 1] = { "v1.0/cali/snapEnabled/set"};
	const char *sendData = buf;
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), ZMQ_SNDMORE);
	char buf2[MAX_DATA_LENGTH + 1] = { 0 };
	buf2[0] = set;
	const char* sendData2 = buf2;
	nbytes = zmq_send(m_zmqReqSocket, sendData2, strlen(sendData2), 0);
	if (nbytes <=0) {
		qWarning() << "cannot send SetSnapEnabled!";
		return;
	}

	int result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	bool setResult = (result == 0 ? false : true);
	qDebug() << "pushButton_SetSnapEnabled recv reply data:" << (bool)setResult;
	assert(!hasMore(m_zmqReqSocket));
	
}


void MainWindow::on_pushButton_CaliSetType_clicked()
{
	QString set = ui->comboBox_CaliType->currentText();
	
	char buf[MAX_DATA_LENGTH + 1] = { "v1.0/cali/type/set" };
	const char *sendData = buf;
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), ZMQ_SNDMORE);
	char buf2[MAX_DATA_LENGTH + 1] = { 0 };
	for (int i = 0; i < set.size(); i++)
	{
		buf2[i] = set.at(i).toLatin1();
	}
	const char *sendData2 = buf2;
	nbytes = zmq_send(m_zmqReqSocket, sendData2, strlen(sendData2), 0);
	if (nbytes <= 0) {
		qWarning() << "cannot send CaliSetType!";
		return;
	}

	int result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	bool setResult = (result == 0 ? false : true);
	qDebug() << "CaliSetType recv reply data:" << (bool)setResult;
	assert(!hasMore(m_zmqReqSocket));

}

void MainWindow::onHeartbeat()
{
    m_heartbeatTimer->start();
    ui->lcdNumber->display(10);
}

void MainWindow::onPublishReceived(QString majorCmd, QString minorCmd, QByteArray data)
{
	if (majorCmd == QStringLiteral("beginAsyncAction")) {
		auto jsonObj = jsonObject(data);
		qDebug() << "beginAsyncAction json object:" << jsonObj;
		auto type = jsonObj["type"].toString();
		auto props = jsonObj["props"].toObject();

		ui->label_AsyBeginTypeR->setText(type);
		QJsonDocument document;
		document.setObject(props);
		QByteArray propsByte = document.toJson();

		ui->label_AsyBeginPropsR->setText(propsByte);
		m_progressDialog->onBeginAsync(type);
		qDebug() << "type" << type << "\n" << "props" << propsByte << endl;
	}
	else if (majorCmd == QStringLiteral("finishAsyncAction")) {
		auto jsonObj = jsonObject(data);
		qDebug() << "finishAsyncAction json object:" << jsonObj;
		auto type = jsonObj["type"].toString();
		auto props = jsonObj["props"].toObject();
		auto result = jsonObj["result"].toString();
		//note: “finishAsyncAction”信号不去处理Cali-type
		if (props["type"] != QJsonValue::Undefined) {
			
		}
		ui->label_AsyFinishTypeR->setText(type);
		QJsonDocument document;
		document.setObject(props);
		QByteArray propsByte = document.toJson();

		ui->label_AsyFinishPropsR->setText(propsByte);
		m_progressDialog->onFinishAsync();
		resetCaliStatus();
		qDebug() << "type" << type << "\n" << "props" << propsByte << endl;
	}
	else if (majorCmd == QStringLiteral("progress")) {
		int  value = 0;
		memcpy(&value, data.constData(), data.size());
		m_progressDialog->onProgress(value);
	}
	else if (majorCmd == QStringLiteral("cali")) {
		if (minorCmd == QStringLiteral("time")) {
			qDebug() << data << "size:"<<data.size()<< endl;
			qint64 valLL = 0;
			memcpy(&valLL, data.constData(), data.size());
			auto dt = QDateTime::fromSecsSinceEpoch(valLL);
			ui->label_CaliTime->setText(dt.toString("HH:MM:ss yyyy-MM-dd"));
			qDebug() << "onPublishReceived  cali//time:" << endl;
		}
		//2019.3.21 cali-type
		if (minorCmd == QStringLiteral("type"))
		{
			qDebug() << "public type" << data << endl;
		}
		if (minorCmd == QStringLiteral("snapEnabled")) {
			auto valInt = data.toInt();

			auto valBool = valInt == 0 ? false : true;
			//ui->checkBox_SnapEnabled->setChecked(valBool);
		}
		if (minorCmd == QStringLiteral("currentCaliGroup")) {
			int  value = 0;
			memcpy(&value, data.constData(), data.size());
			ui->label_CaliGroup->setText(QString::number(value));
		}
		if (minorCmd == QStringLiteral("currentCaliDist")) {
			int  value = 0;
			memcpy(&value, data.constData(), data.size());
			ui->label_CaliDistance->setText(QString::number(value));
		}
		if (minorCmd == QStringLiteral("caliDistStates")) {
			//QMessageBox::information(NULL, "caliDistStates", data, QMessageBox::Yes);
		
			QJsonDocument jsondocument = QJsonDocument::fromJson(data);
			QJsonObject jsonObject = jsondocument.object();
			QJsonArray array = jsonObject["states"].toArray();
			if (array.count() != 5) {
				qDebug() << "qjsonarray_Count:" << array.count();
			}
			else
			{
				//qDebug() << "qjsonarray_Count:" << array.count();
				bool cali1 = array[0].toBool();
				bool cali2 = array[1].toBool();
				bool cali3 = array[2].toBool();
				bool cali4 = array[3].toBool();
				bool cali5 = array[4].toBool();
				if (cali1 == true) {
					ui->label_Cali1->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali1->setStyleSheet("background-color:red");
				}
				if (cali2 == true) {
					ui->label_Cali2->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali2->setStyleSheet("background-color:red");
				}
				if (cali3 == true) {
					ui->label_Cali3->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali3->setStyleSheet("background-color:red");
				}
				if (cali4 == true) {
					ui->label_Cali4->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali4->setStyleSheet("background-color:red");
				}

				if (cali5 == true) {
					ui->label_Cali5->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali5->setStyleSheet("background-color:red");
				}
			}
		}	
	}
}

void MainWindow::resetCaliStatus()
{
	ui->label_Cali1->setStyleSheet("background-color:red");
	ui->label_Cali2->setStyleSheet("background-color:red");
	ui->label_Cali3->setStyleSheet("background-color:red");
	ui->label_Cali4->setStyleSheet("background-color:red");
	ui->label_Cali5->setStyleSheet("background-color:red");
}


void MainWindow::onVideoImageReady(int camID, QPixmap pixmap)
{
	
}

bool MainWindow::sendData(void* socket, const QString& cmd, const QByteArray& data)
{
	int nbytes = 0;

	auto envelop = ("v1.0/" + cmd).toLocal8Bit();
	if (data != ""){
		nbytes = zmq_send(socket, envelop.constData(), envelop.size(), ZMQ_SNDMORE);
		if (nbytes != envelop.size()){
			qCritical() << "Send envelop error! nbytes:" << nbytes;
			return false;
		}
		nbytes = zmq_send(socket, data.constData(), data.size(), 0);
		if (nbytes != data.size()){
			qCritical() << "Send data error!"
				<< " nbytes:" << nbytes
				<< " data size:" << data.size()
				<< " data : " << data;

			return false;
		}
	}
	else{
		nbytes = zmq_send(socket, envelop.constData(), envelop.size(), 0);
		if (nbytes != envelop.size()){
			qCritical() << "Send envelop error! nbytes:" << nbytes;
			return false;
		}
	}

	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(socket, buf, MAX_DATA_LENGTH, 0);
	return true;
}




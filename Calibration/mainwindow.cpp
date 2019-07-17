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

	lineEdit_Group.resize(5);
	lineEdit_Group[0] = ui->lineEdit_Group1;
	lineEdit_Group[1] = ui->lineEdit_Group2;
	lineEdit_Group[2] = ui->lineEdit_Group3;
	lineEdit_Group[3] = ui->lineEdit_Group4;
	lineEdit_Group[4] = ui->lineEdit_Group5;

	widget_Step.resize(4);
	widget_Step[0] = ui->widget_Step1;
	widget_Step[1] = ui->widget_Step2;
	widget_Step[2] = ui->widget_Step3;
	widget_Step[3] = ui->widget_Step4;

	ui->widget_Calibration5->hide();
	ui->widget_Calibration7->hide();
	ui->pushButton_GetInformation->setEnabled(false);

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
	m_progressDialog->setWindowTitle("Check Device");

	int result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	qDebug() << "recv reply data:" << (result == 0 ? false : true);
	if (result != 0)
	{
		ui->widget->setEnabled(true);
	}
	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::on_pushButton_pro_clicked()
{
	const char *sendData = "v1.0/device/devSubType/set";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), ZMQ_SNDMORE);

	QByteArray proType("DST_PRO");
	nbytes = zmq_send(m_zmqReqSocket, proType, proType.size(), 0);

	int result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	qDebug() << "recv reply data:" << (result == 0 ? false : true);
	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::on_pushButton_pro_plus_clicked()
{
	const char *sendData = "v1.0/device/devSubType/set";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), ZMQ_SNDMORE);

	QByteArray proType("DST_PRO_PLUS");
	nbytes = zmq_send(m_zmqReqSocket, proType, proType.size(), 0);

	int result = 0;
	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
	qDebug() << "recv reply data:" << (result == 0 ? false : true);
	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::CaliGetTime()
{
	const char *sendData = "v1.0/cali/time";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);

	//qlonglong result = 0;
	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH, 0);

	QString strDate(buf);
	//auto dt = QDateTime::fromSecsSinceEpoch(result);
	ui->label_CaliTime->setText(strDate);
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
	//lineEdit_Group[num-1]->setStyleSheet("background-color:gray");
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
	m_progressDialog->setWindowTitle("Enter calibration");
	const char *sendData = "v1.0/cali/enter";
	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), 0);
	char buf[MAX_DATA_LENGTH + 1] = { 0 };
	nbytes = zmq_recv(m_zmqReqSocket, buf, MAX_DATA_LENGTH, 0);
	int num;
	memcpy(&num, buf, sizeof(int));
	auto valBool = num == 0 ? false : true;
	qDebug() << "cali enterCali:" << valBool;
	ui->pushButton_GetInformation->setEnabled(true);

	assert(!hasMore(m_zmqReqSocket));
}

void MainWindow::on_pushButton_CaliExit_clicked()
{
	m_progressDialog->setWindowTitle("Exit calibration");
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

//void MainWindow::on_pushButton_CaliSetType_clicked()
//{
//	QString set = ui->comboBox_CaliType->currentText();
//	
//	char buf[MAX_DATA_LENGTH + 1] = { "v1.0/cali/type/set" };
//	const char *sendData = buf;
//	int nbytes = zmq_send(m_zmqReqSocket, sendData, strlen(sendData), ZMQ_SNDMORE);
//	char buf2[MAX_DATA_LENGTH + 1] = { 0 };
//	for (int i = 0; i < set.size(); i++)
//	{
//		buf2[i] = set.at(i).toLatin1();
//	}
//	const char *sendData2 = buf2;
//	nbytes = zmq_send(m_zmqReqSocket, sendData2, strlen(sendData2), 0);
//	if (nbytes <= 0) {
//		qWarning() << "cannot send CaliSetType!";
//		return;
//	}
//
//	int result = 0;
//	nbytes = zmq_recv(m_zmqReqSocket, &result, sizeof(int), 0);
//	bool setResult = (result == 0 ? false : true);
//	qDebug() << "CaliSetType recv reply data:" << (bool)setResult;
//	assert(!hasMore(m_zmqReqSocket));
//
//}


void MainWindow::nextStep(int num)
{
	ui->tabWidget->setCurrentIndex(num + 1);
	for (int i = 0; i < ui->tabWidget->count(); i++)
	{
		if (i != num + 1)
		{
			widget_Step[i]->setEnabled(false);
		}
		else
		{
			widget_Step[i]->setEnabled(true);
		}
	}

	if (ui->widget_Step1->isEnabled())
	{
		ui->label_DeviceStatus->setText("Status");
	}
	else if (ui->widget_Step2->isEnabled())
	{
		ui->label_EnterStatus->setText("Status");
	}
	else if (ui->widget_Step4->isEnabled())
	{
		lineEdit_Group[0]->setStyleSheet("background-color:white");
		lineEdit_Group[1]->setStyleSheet("background-color:white");
		lineEdit_Group[2]->setStyleSheet("background-color:white");
		lineEdit_Group[3]->setStyleSheet("background-color:white");
		lineEdit_Group[4]->setStyleSheet("background-color:white");
	}
}

void MainWindow::backStep(int num)
{
	ui->tabWidget->setCurrentIndex(num - 1);
	for (int i = 0; i < ui->tabWidget->count(); i++)
	{
		if (i != num - 1)
		{
			widget_Step[i]->setEnabled(false);
		}
		else
		{
			widget_Step[i]->setEnabled(true);
		}
	}

	if (ui->widget_Step1->isEnabled())
	{
		ui->label_DeviceStatus->setText("Status");
	}
	else if (ui->widget_Step2->isEnabled())
	{
		ui->label_EnterStatus->setText("Status");
	}

}

void MainWindow::on_pushButton_Step1Next_clicked()
{
	int index_tab = 0;
	nextStep(index_tab);
}
void MainWindow::on_pushButton_Step2Next_clicked()
{
	int index_tab = 1;
	nextStep(index_tab);
}
void MainWindow::on_pushButton_Step2Back_clicked()
{
	int index_tab = 1;
	backStep(index_tab);
}
void MainWindow::on_pushButton_Step3Next_clicked()
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

	if (ui->comboBox_CaliType->currentIndex() == 0)
	{
		ui->widget->show();
		ui->widget_Calibration5->setGeometry(90, 40, 191, 96);
	}
	else
	{
		ui->widget->hide();
		ui->widget_Calibration5->setGeometry(90, 20, 191, 96);
		ui->widget_Calibration7->setGeometry(100, 20, 174, 120);
	}
	if (ui->comboBox_CaliType->currentIndex() == 1 || ui->comboBox_CaliType->currentIndex() == 2)
	{
		ui->widget_Calibration5->hide();
		ui->widget_Calibration7->show();
	}
	else if (ui->comboBox_CaliType->currentIndex() == 0 || ui->comboBox_CaliType->currentIndex() == 3)
	{
		ui->widget_Calibration5->show();
		ui->widget_Calibration7->hide();
	}
	int index_tab = 2;
	nextStep(index_tab);
}
void MainWindow::on_pushButton_Step3Back_clicked()
{
	int index_tab = 2;
	backStep(index_tab);
}
void MainWindow::on_pushButton_Step4Back_clicked()
{
	int index_tab = 3;
	backStep(index_tab);
}


void MainWindow::onHeartbeat()
{
    m_heartbeatTimer->start();
    ui->lcdNumber->display(10);
}

void MainWindow::onPublishReceived(QString majorCmd, QString minorCmd, QByteArray data)
{
	CaliCurrentDist();
	CaliCurrentGroup();
	CaliGetTime();
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
		m_progressDialog->setWindowTitle("Data processing");
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

		if (ui->widget_Step1->isEnabled())
		{
			ui->label_DeviceStatus->setText("Check Successful");
			ui->pushButton_Step1Next->setEnabled(true);
		}
		else if (ui->widget_Step2->isEnabled())
		{
			ui->label_EnterStatus->setText("Enter Successful");
			ui->pushButton_Step2Back->setEnabled(true);
			ui->pushButton_Step2Next->setEnabled(true);
		}

		m_progressDialog->onFinishAsync();
		resetCaliStatus();
		qDebug() << "type" << type << "\n" << "props" << propsByte << endl;
	}
	else if (majorCmd == QStringLiteral("progress")) {
		int  value = 0;
		memcpy(&value, data.constData(), data.size());
		m_progressDialog->onProgress(value);
	}
	else if (majorCmd == QStringLiteral("device")){
		if (minorCmd == QStringLiteral("event")){
			qDebug() << "device/event";
			QString deviceEvent = data;
			//memcpy(&deviceEvent, data.constData(), data.size());
			if (deviceEvent == "DE_DOUBLECLICK")
			{
				qDebug() << "DE_DOUBLECLICK";
			}
			else if (deviceEvent == "DE_CLICK")
			{
				on_pushButton_SetSnapEnabled_clicked();
				qDebug() << "DE_CLICK";
			}
			else if (deviceEvent == "DE_PLUS")
			{
				qDebug() << "DE_PLUS";
			}
			else if (deviceEvent == "DE_SUB")
			{
				qDebug() << "DE_SUB";
			}
		}
	}
	else if (majorCmd == QStringLiteral("cali")) {
		if (minorCmd == QStringLiteral("time")) {
			// 			qDebug() << data << "size:"<<data.size()<< endl;
			// 			qint64 valLL = 0;
			// 			memcpy(&valLL, data.constData(), data.size());
			// 			auto dt = QDateTime::fromSecsSinceEpoch(valLL);
			// 			ui->label_CaliTime->setText(dt.toString("HH:MM:ss yyyy-MM-dd"));

			ui->label_CaliTime->setText(QString(data));
			qDebug() << "onPublishReceived  cali//time: " << data;
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
			lineEdit_Group[value-1]->setStyleSheet("background-color:gray");
			if (value>1)
			{
				lineEdit_Group[value - 2]->setStyleSheet("background-color:white");
			}
			else if (value == 1)
			{
				lineEdit_Group[4]->setStyleSheet("background-color:white");
			}
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
			if (array.count() != 5&&array.count() != 7) 
			{
				qDebug() << "qjsonarray_Count:" << array.count();
			}
			else if (array.count() == 5 )
			{
				qDebug() << "qjsonarray_Count:" << array.count();
				bool cali1 = array[0].toBool();
				bool cali2 = array[1].toBool();
				bool cali3 = array[2].toBool();
				bool cali4 = array[3].toBool();
				bool cali5 = array[4].toBool();
				if (cali1 == true) {
					ui->label_Cali1->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali1->setStyleSheet("background-color:gray");
				}
				if (cali2 == true) {
					ui->label_Cali2->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali2->setStyleSheet("background-color:gray");
				}
				if (cali3 == true) {
					ui->label_Cali3->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali3->setStyleSheet("background-color:gray");
				}
				if (cali4 == true) {
					ui->label_Cali4->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali4->setStyleSheet("background-color:gray");
				}

				if (cali5 == true) {
					ui->label_Cali5->setStyleSheet("background-color:green");
				}
				else {
					ui->label_Cali5->setStyleSheet("background-color:gray");
				}
			}
			else if (array.count() == 7)
			{
				qDebug() << "qjsonarray_Count:" << array.count();
				bool cali1 = array[0].toBool();
				bool cali2 = array[1].toBool();
				bool cali3 = array[2].toBool();
				bool cali4 = array[3].toBool();
				bool cali5 = array[4].toBool();
				bool cali6 = array[5].toBool();
				bool cali7 = array[6].toBool();
				if (cali1 == true) {
					ui->label_7Cali1->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali1->setStyleSheet("background-color:gray");
				}
				if (cali2 == true) {
					ui->label_7Cali2->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali2->setStyleSheet("background-color:gray");
				}
				if (cali3 == true) {
					ui->label_7Cali3->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali3->setStyleSheet("background-color:gray");
				}
				if (cali4 == true) {
					ui->label_7Cali4->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali4->setStyleSheet("background-color:gray");
				}
				if (cali5 == true) {
					ui->label_7Cali5->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali5->setStyleSheet("background-color:gray");
				}
				if (cali6 == true) {
					ui->label_7Cali6->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali6->setStyleSheet("background-color:gray");
				}
				if (cali7 == true) {
					ui->label_7Cali7->setStyleSheet("background-color:green");
				}
				else {
					ui->label_7Cali7->setStyleSheet("background-color:gray");
				}
			}
		}	
	}
}

void MainWindow::resetCaliStatus()
{
	if (ui->widget_Calibration5->isVisible())
	{
		ui->label_Cali1->setStyleSheet("background-color:gray");
		ui->label_Cali2->setStyleSheet("background-color:gray");
		ui->label_Cali3->setStyleSheet("background-color:gray");
		ui->label_Cali4->setStyleSheet("background-color:gray");
		ui->label_Cali5->setStyleSheet("background-color:gray");
	}
	else if (ui->widget_Calibration7->isVisible())
	{
		ui->label_7Cali1->setStyleSheet("background-color:gray");
		ui->label_7Cali2->setStyleSheet("background-color:gray");
		ui->label_7Cali3->setStyleSheet("background-color:gray");
		ui->label_7Cali4->setStyleSheet("background-color:gray");
		ui->label_7Cali5->setStyleSheet("background-color:gray");
		ui->label_7Cali6->setStyleSheet("background-color:gray");
		ui->label_7Cali7->setStyleSheet("background-color:gray");
	}
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




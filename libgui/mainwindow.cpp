// #include "arch/common/hardware.h"
#include "src/mainworker.h"
#include "ui.h"
#include "config.h"
#include "gui_defines.h"

/**
  * Global variables: The commandline arguments have to be global
  * to be accessable from both threads (gui and simulation).
  */
int _argc;
char** _argv;

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>
#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>
#include <QQueue>
#include <QMutexLocker>
#include <QTime>

Worker* simulationWorker;

void Worker::runSimulation()
{
  shutdown = false;
  framebufferData = 0;
  qDebug() << "start sim";
  startWorker(_argc, _argv);
  qDebug() << "stop sim";
  shutdown = false;
  emit finished();
}

void Worker::stopSimulation()
{
	QMutexLocker l(&mMutex);
  shutdown = true;
}

void Worker::setWindowData(int w, int h, const char* title, int memsize)
{
  this->memsize = memsize;
  if (framebufferData)
    free(framebufferData);
  framebufferData = (uint8_t*)malloc(memsize);
  emit setGuiSimData(title, w, h, memsize);
}
 
void Worker::copyBitmap(uint8_t* data)
{
  memcpy(framebufferData, data, memsize);
  emit displayBitmap(framebufferData);
}
 
void Worker::setButtonUp(uint32_t b)
{
	QMutexLocker l(&mMutex);
  buttonUp = b;
}

void Worker::setButtonDown(uint32_t b)
{
	QMutexLocker l(&mMutex);
  buttonDown = b;
}

void Worker::getButtonState(uint32_t& up,uint32_t& down,bool& shutdown)
{
	QMutexLocker l(&mMutex);
	up = buttonUp;
	down = buttonDown;
	shutdown = this->shutdown;
	
	buttonUp = 0;
	buttonDown = 0;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), currentTimerID(0)
{
    ui->setupUi(this);
	ui->actionStop->setEnabled(false);
    worker.moveToThread(&workerThread);
    connect(this, SIGNAL(startSimulation()), &worker, SLOT(runSimulation()),Qt::QueuedConnection);
    connect(&worker, SIGNAL(finished()), this, SLOT(finishedSimulation()),Qt::QueuedConnection);
    connect(&worker, SIGNAL(setGuiSimData(const QString&, int, int, int)), this, SLOT(setGuiSimData(const QString&, int, int, int)),Qt::QueuedConnection);
    connect(&worker, SIGNAL(displayBitmap(uint8_t*)), this, SLOT(displayBitmap(uint8_t*)),Qt::QueuedConnection);
    // set global object simulationWorker. This is used by the simulation c code
    // to communicate with the gui main thread
    simulationWorker = &worker;
	
	on_action_Re_Start_triggered();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
	worker.stopSimulation();
	if (workerThread.isRunning()) {
		workerThread.wait(2000);
		workerThread.terminate();
		workerThread.wait(100);
	}
	if (workerThread.isRunning()) {
		qWarning() << "Could not finish simulation thread!";
	}
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}

void MainWindow::on_action_Re_Start_triggered()
{
	statusBar()->showMessage(tr("Start simulator..."),1000);
	ui->actionStop->setEnabled(true);
	ui->action_Re_Start->setEnabled(false);
	workerThread.start();
    emit startSimulation();
}

void MainWindow::on_actionStop_triggered()
{
	worker.stopSimulation();
}

void MainWindow::finishedSimulation()
{
  statusBar()->showMessage(tr("Simulator finished!"),1000);
  ui->action_Re_Start->setEnabled(true);
  ui->actionStop->setEnabled(false);
}

/**
  * Override QApplication timerEvent. Called
  * by the Qt event system after setup with startTimer(ms).
  * By default, this method is called every 50ms if new
  * data is available to draw.
  */
void MainWindow::timerEvent ( QTimerEvent * event ) {
  // Only do something if the timerid is correct
  if (event->timerId() != currentTimerID)
    return;
  // We reestablish the timer event every time,
  // if no new data is available, we will not
  // start the timer.
  killTimer(currentTimerID);
  currentTimerID = 0;
  displayBitmap(0);
}
  
void MainWindow::displayBitmap(uint8_t* data)
{
	if (data != 0) {
		framebufferData = data;
		// if the gui pixmap update timer is already running, do nothing
		if (currentTimerID)
            return;
		// New data is available: Start the timer and also show the
		// new pixmap immediatelly.
		currentTimerID = startTimer(50);
	}
    for(int y=0; y < h; y++) {
      int idx_buff =  y * w * 3;
      for(int x=0; x < w; x++)
      {
        image.setPixel( x,y, qRgb(framebufferData[idx_buff + 0], framebufferData[idx_buff + 1], framebufferData[idx_buff + 2] ));
        idx_buff += 3;
      }
    }
    ui->centralWidget->setPixmap(QPixmap::fromImage(image));
}

void MainWindow::setGuiSimData(const QString& title, int w, int h, int memsize)
{
  this->memsize = memsize;
  this->title = title;
  this->w = w;
  this->h = h;
  resize(w, h);
  image = QImage(w,h, QImage::Format_RGB888);
  image.fill(Qt::white);
  setWindowTitle(title);
  // Add buttons
  QToolBar *bar = ui->mainToolBar;
  bar->clear();
  qDeleteAll(mButtons);
  mButtons.clear();
  
  struct ui_t* machine_ui = get_machine_ui();
  for (int i=0;i<8;++i) {
    QString title = QString::fromUtf8(machine_ui->buttons[i].name);
	if (title.isEmpty())
		continue;
    wsimButton* btn = new wsimButton(title, machine_ui->buttons[i].pin, this);
	mButtons.append(btn);
    connect(btn, SIGNAL(pressed()), SLOT(btnpressed()));
    connect(btn, SIGNAL(released()), SLOT(btnreleased()));
    bar->addWidget(btn);
  }
  
  ui->centralWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  ui->centralWidget->setScaledContents(true);
  ui->centralWidget->resize(w,h);
  
}

void MainWindow::btnpressed()
{
  wsimButton* btn = (wsimButton*)sender();
  buttonDown |= btn->buttoncode;
  worker.setButtonDown(buttonDown);
}

void MainWindow::btnreleased()
{
  wsimButton* btn = (wsimButton*)sender();
  buttonUp |= btn->buttoncode;
  worker.setButtonUp(buttonUp);
}

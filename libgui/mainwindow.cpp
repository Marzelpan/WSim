// #include "arch/common/hardware.h"
#include "src/mainworker.h"
#include "ui.h"
#include "config.h"
#include "gui_defines.h"
#include "simulationthread.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>
#include <QDebug>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), currentTimerID(0)
{
    ui->setupUi(this);
	ui->actionStop->setEnabled(false);
	mSimulationThread = 0;
	mCloseOnFinish = false;
	on_action_Re_Start_triggered();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
	if (mSimulationThread) {
		mCloseOnFinish = true;
		mSimulationThread->stopSimulation();
		return;
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
	mSimulationThread = new SimulationThread;
	connect(mSimulationThread, SIGNAL(finished()), mSimulationThread, SLOT(deleteLater()) );
	connect(mSimulationThread, SIGNAL(finished()), this, SLOT(finishedSimulation()) );
    connect(mSimulationThread, SIGNAL(setGuiSimData(const QString&, int, int, int)), this, SLOT(setGuiSimData(const QString&, int, int, int)),Qt::QueuedConnection);
    connect(mSimulationThread, SIGNAL(displayBitmap(uint8_t*)), this, SLOT(displayBitmap(uint8_t*)),Qt::QueuedConnection);
    // set global object simulationWorker. This is used by the simulation c code
    // to communicate with the gui main thread
    simulationWorker = mSimulationThread;
	
	mSimulationThread->start();
}

void MainWindow::on_actionStop_triggered()
{
	if (mSimulationThread)
		mSimulationThread->stopSimulation();
}

void MainWindow::finishedSimulation()
{
  statusBar()->showMessage(tr("Simulator finished!"),3000);
  ui->action_Re_Start->setEnabled(true);
  ui->actionStop->setEnabled(false);
  // Disable buttons if simulator finished
  for (int i=0;i<mButtons.size();++i)
	  mButtons[i]->setEnabled(false);
  // delete worker
  mSimulationThread = 0;
  simulationWorker = 0;
  if (mCloseOnFinish)
	  close();
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
	if (!mSimulationThread)
		return;
	wsimButton* btn = (wsimButton*)sender();
	buttonDown |= btn->buttoncode;
	mSimulationThread->setButtonDown(buttonDown);
}

void MainWindow::btnreleased()
{
	if (!mSimulationThread)
		return;
	wsimButton* btn = (wsimButton*)sender();
	buttonUp |= btn->buttoncode;
	mSimulationThread->setButtonUp(buttonUp);
}

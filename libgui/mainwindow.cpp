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
#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>

Worker* simulationWorker;

void Worker::runSimulation()
{
  shutdown = false;
  framebufferData = 0;
  startWorker(_argc, _argv);
  shutdown = false;
  emit finished();
}

void Worker::stopSimulation()
{
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
  buttonUp = b;
}

void Worker::setButtonDown(uint32_t b)
{
  buttonDown = b;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    worker.moveToThread(&workerThread);
    connect(this, SIGNAL(startSimulation()), &worker, SLOT(runSimulation()));
    connect(this, SIGNAL(stopSimulation()), &worker, SLOT(stopSimulation()));
    connect(this, SIGNAL(setButtonUp(uint32_t)), &worker, SLOT(setButtonUp(uint32_t)));
    connect(this, SIGNAL(setButtonDown(uint32_t)), &worker, SLOT(setButtonDown(uint32_t)));
    connect(&worker, SIGNAL(finished()), this, SLOT(finishedSimulation()));
    connect(&worker, SIGNAL(setGuiSimData(const QString&, int, int, int)), this, SLOT(setGuiSimData(const QString&, int, int, int)));
    connect(&worker, SIGNAL(displayBitmap(uint8_t*)), this, SLOT(displayBitmap(uint8_t*)));
    // set global object simulationWorker. This is used by the simulation c code
    // to communicate with the gui main thread
    simulationWorker = &worker;
}

MainWindow::~MainWindow()
{
    delete ui;
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
    emit startSimulation();
}

void MainWindow::on_actionStop_triggered()
{
	emit stopSimulation();
}

void MainWindow::finishedSimulation()
{
  qDebug() << "Emulator beendet!";
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
  displayBitmap(framebufferData);
}
  
void MainWindow::displayBitmap(uint8_t* data)
{
    framebufferData = data;
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
  
  struct ui_t* machine_ui = get_machine_ui();
  for (int i=0;i<8;++i) {
    QString title = QString::fromUtf8(machine_ui->buttons[i].name);
	if (title.isEmpty())
		continue;
    wsimButton* btn = new wsimButton(title, machine_ui->buttons[i].pin, this);
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
  emit setButtonDown(buttonDown);
}

void MainWindow::btnreleased()
{
  wsimButton* btn = (wsimButton*)sender();
  buttonUp |= btn->buttoncode;
  emit setButtonUp(buttonUp);
}

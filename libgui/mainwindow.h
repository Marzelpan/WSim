#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QImage>

#include <QPushButton>
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>

#include <inttypes.h>

#include "gui_defines.h"
/**
  * Global variables: The commandline arguments have to be global
  * to be accessable from both threads (gui and simulation).
  */
extern int _argc;
extern char** _argv;


namespace Ui {
    class MainWindow;
}

class QTimerEvent;
class Worker : public QObject
{
    Q_OBJECT
public:
  // core to gui thread communication
  void setWindowData(int w, int h, const char* title, int memsize);
  
  // core to gui thread communication
  // slow down to 50ms update time, we do no
  // draw the pixmap immediatelly here, but notify
  // the Qt event system of new pixmap data
  void copyBitmap(uint8_t* data);
public Q_SLOTS:
    void runSimulation();
    void setButtonUp(uint32_t);
    void setButtonDown(uint32_t);
public:
    uint32_t buttonUp;
    uint32_t buttonDown;
    int memsize;
private:
    uint8_t* framebufferData;
Q_SIGNALS:
    void finished();
    void setGuiSimData(const QString& title, int w, int h, int memsize);
    void displayBitmap(uint8_t* data);
};
 
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void changeEvent(QEvent *e);
  /**
    * Override QApplication timerEvent. Called
    * by the Qt event system after setup with startTimer(ms).
    * By default, this method is called every 50ms if new
    * data is available to draw.
    */
  void timerEvent ( QTimerEvent * event );
private:
    Ui::MainWindow *ui;
    QThread workerThread;
    Worker worker;
    QImage image;
    // fast
    int currentTimerID;
    // core to gui thread transport variables
    int w, h;
    QString title;
    int memsize;
    // gui to core thread
    uint32_t buttonUp;
    uint32_t buttonDown;
    uint8_t* framebufferData;
    
  class wsimButton : public QPushButton {
    public:
      int buttoncode;
      /**
        * Construtor: Set the title, a unique buttoncode
        */
      wsimButton(const QString& title, int buttoncode, QWidget* parent = 0) : QPushButton(title, parent) {this->buttoncode = buttoncode;}
  };

private Q_SLOTS:
    void on_actionStop_triggered();
    void on_action_Re_Start_triggered();
    void on_actionQuit_triggered();
    
    void finishedSimulation();
    void displayBitmap(uint8_t* data);
    void setGuiSimData(const QString& title, int w, int h, int memsize);
    
    void btnpressed();
    void btnreleased();
Q_SIGNALS:
    void startSimulation();
    void setButtonUp(uint32_t);
    void setButtonDown(uint32_t);
};


// We have to know the worker object, from where we where started.
// We will use that object to communicate with the gui main thread.
extern Worker* simulationWorker;
#endif // MAINWINDOW_H

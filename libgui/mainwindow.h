
/**
 *  \file   mainwindow.h
 *  \brief  WorldSens Simulator UI Definition
 *  \author David Gräff
 *  \date   2013
 **/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QImage>
#include <QPushButton>

#include <inttypes.h>

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
class QCloseEvent;
#include <QMutex>
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
	void stopSimulation();
    void setButtonUp(uint32_t);
    void setButtonDown(uint32_t);
	void getButtonState(uint32_t&,uint32_t&,bool&);
public Q_SLOTS:
    void runSimulation();
public:
    int memsize;
private:
    uint8_t* framebufferData;
	QMutex mMutex;
    uint32_t buttonUp;
    uint32_t buttonDown;
	bool shutdown;
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
	void closeEvent(QCloseEvent* e);
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
    int buttonUp;
    int buttonDown;
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
};


// We have to know the worker object, from where we where started.
// We will use that object to communicate with the gui main thread.
extern Worker* simulationWorker;
#endif // MAINWINDOW_H


/**
 *  \file   mainwindow.h
 *  \brief  WorldSens Simulator UI Definition
 *  \author David Gräff
 *  \date   2013
 **/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QPushButton>
#include <QList>

#include <inttypes.h>

namespace Ui {
    class MainWindow;
}
 
class SimulationThread;
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
	QList<QWidget*> mButtons;
    SimulationThread* mSimulationThread;
	bool mCloseOnFinish;
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
};
#endif // MAINWINDOW_H

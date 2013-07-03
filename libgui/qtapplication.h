
/**
 *  \brief  Qt QApplication class
 *  \author David Gräff
 *  \date   2013, June
 **/

#pragma once

#include <inttypes.h>

#include <QApplication>
#include <QWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>

#include "gui_defines.h"

/**
  * We use Qt here in a non common way without the moc precompiler.
  * Therefore the signal/slots concept cannot be used and we have to 
  * subclass QApplication and intercept all window system events.
  * This is done in  bool event ( QEvent * e ).
  *
  * Additionally the gui is running in a separate thread and we have
  * to pass variables and the simulation bitmap in setWindowData(...)
  * and copyBitmap(...) via postMessage.
  */
class wsimQtApplication: public QApplication {
  public:
    /**
      * Constructor: Pass command line arguments here
      */
  wsimQtApplication(int argc, char* argv[]) : QApplication(argc, argv), framebufferData(0),currentTimerID(0) {}
  // GUI
  QWidget window;
  QImage image;
  QLabel imageLabel;
  // fast
  int currentTimerID;
  // core to gui thread transport variables
  int w, h;
  QString title;
  uint8_t* framebufferData;
  int memsize;
  // gui to core thread
  uint32_t buttonUp;
  uint32_t buttonDown;
  
  /**
    * We need to subclass a QPushButton to intercept the 
    * window system events in bool event ( QEvent * e ),
    * because we cannot use the signal/slot concept.
    */
  class wsimButton : public QPushButton {
    public:
      int buttoncode;
      wsimQtApplication* app;
      /**
        * Construtor: Set the title, a unique buttoncode and the subclassed QtApplication object
        */
      wsimButton(const QString& title, int buttoncode, wsimQtApplication* app) : QPushButton(title) {this->buttoncode = buttoncode;this->app=app;}
      bool event ( QEvent * e ) {
        if (e->type() == QEvent::MouseButtonPress) {
          app->setbuttonDown(buttoncode);
        }
        else if (e->type() == QEvent::MouseButtonRelease) {
          app->setbuttonUp(buttoncode);
        }
        QPushButton::event(e);
      }
  };
  
  enum wsimGUIEvents {GUIWindowEvent, CloseWindowEvent, ShowData};

  /**
    * We define an own QEvent for message passing between threads
    */
  class wsimEvent : public QEvent {
    private:
      wsimGUIEvents mType;
    public:
      wsimEvent(wsimGUIEvents type) : QEvent(QEvent::User) {
        mType = type;
      }
      int wsimtype() {return mType;}
  };
  
  /**
    * setbuttonDown and setbuttonUp are called from the subclassed QPushButton. They
    * modify variables called buttonDown and buttonUp.
    */
  void setbuttonDown(int code) {
    buttonDown |= code;
  }
  void setbuttonUp(int code) {
    buttonUp |= code;
  }
  
  /**
    * This method sets up the gui
    */
  void setupGUI() {
    image = QImage(w,h, QImage::Format_RGB888);
    image.fill(Qt::white);
    window.setWindowTitle(title);
    window.resize(w, h);
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(new wsimButton("#", UI_BUTTON_2, this));
    buttonLayout->addWidget(new wsimButton("*", UI_BUTTON_1, this));
    buttonLayout->addWidget(new wsimButton("Licht", UI_BUTTON_4, this));
    buttonLayout->addWidget(new wsimButton("Oben", UI_BUTTON_3, this));
    buttonLayout->addWidget(new wsimButton("Unten", UI_BUTTON_5, this));
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addLayout(buttonLayout);
    imageLabel.setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel.setScaledContents(true);
    imageLabel.resize(w,h);
    layout->addWidget(&imageLabel);
    window.setLayout(layout);
    window.show();
  }
  
  /**
    * This method copies the simulation pixmap data to a QLabel, pixel by pixel (unfortunatelly
    * we cannot use memcpy here, because the memory representation of the
    * simulation pixmap and Qt one's is different).
    * It is called by a timer event every 50ms, We do not need to redraw the image faster than we can display it.
    */
  void showData() {
    for(int y=0; y < h; y++) {
      int idx_buff =  y * w * 3;
      for(int x=0; x < w; x++)
      {
        image.setPixel( x,y, qRgb(framebufferData[idx_buff + 0], framebufferData[idx_buff + 1], framebufferData[idx_buff + 2] ));
        idx_buff += 3;
      }
    }
    imageLabel.setPixmap(QPixmap::fromImage(image));
  }

  /**
    * Override QApplication timerEvent. Called
    * by the Qt event system after setup with startTimer(ms).
    * By default, this method is called every 50ms if new
    * data is available to draw.
    */
  void timerEvent ( QTimerEvent * event ) {
    // Only do something if the timerid is correct
    if (event->timerId() != currentTimerID)
      return;
    // We reestablish the timer event every time,
    // if no new data is available, we will not
    // start the timer.
    killTimer(currentTimerID);
    currentTimerID = 0;
    showData();
  }

  // event handling
  bool event ( QEvent * e ) {
    if (e->type()==QEvent::User) {
      wsimEvent* ee = static_cast<wsimEvent*>(e);
      switch(ee->wsimtype()) {
        case GUIWindowEvent:
          // Window is ready: Setup the gui
          setupGUI(); break;
        case CloseWindowEvent:
          // Close button has been clicked: close window and exit the gui main loop
          window.close();
          this->exit(0);
        break;
        case ShowData: {
          // if the gui pixmap update timer is already running, do nothing
          if (currentTimerID)
            break;
          // New data is available: Start the timer and also show the
          // new pixmap immediatelly.
          currentTimerID = startTimer(50);
          showData();
        }
        break;
      }
    }
    return QApplication::event(e);
  }
    
  // core to gui thread communication
  void setWindowData(int w, int h, const char* title, int memsize) {
    this->w = w;
    this->h = h;
    this->title = title;
    this->memsize = memsize;
      if (!framebufferData)
        framebufferData = new uint8_t[memsize];
    
    this->postEvent(this, new wsimEvent(GUIWindowEvent));
  }
  
  // core to gui thread communication
  void close() {
    this->postEvent(this, new wsimEvent(CloseWindowEvent));
  }
  
  // core to gui thread communication
  // slow down to 50ms update time, we do no
  // draw the pixmap immediatelly here, but notify
  // the Qt event system of new pixmap data
  void copyBitmap(uint8_t* data) {
    framebufferData = data;
    this->postEvent(this, new wsimEvent(ShowData));
  }
};
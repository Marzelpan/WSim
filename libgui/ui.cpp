
/**
 *  \file   ui.cpp
 *  \brief  Graphical UI definition using the Qt Toolkit (version 4.7+)
 *  \author David Graeff, based on Antoine Fraboulet work
 *  \date   2005, 2013
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#ifdef __cplusplus
extern "C" {
#endif
#include "arch/common/hardware.h"
#include "devices/devices.h"
#include "src/mgetopt.h"
#include "src/options.h"
#ifdef __cplusplus
}
#endif

#include "ui.h"
#include <pthread.h>

#define UNUSED __attribute__((unused))  

extern "C" {
#include "src/mainworker.h"
}

/**
 * global variables
 **/

#define GUI_DATA_MACHINE   machine.ui

static struct moption_t gui_opt = {
  "ui",
  no_argument,
  "enable GUI",
  0,
  0
};

static struct moption_t title_opt = {
  "ui_title",
  required_argument,
  "GUI window title",
  0, 0
};

int _argc;
char** _argv;
class wsimQtApplication: public QApplication {
	public:
	wsimQtApplication(int argc, char* argv[]) : QApplication(argc, argv), framebufferData(0),currentTimerID(0) {}
	// GUI
	QWidget window;
	QImage image;
	QLabel imageLabel;
	// fast
// 	QTime timer;
	int currentTimerID;
	// core to gui thread transport variables
	int w, h;
	QString title;
	uint8_t* framebufferData;
	int memsize;
	// gui to core thread
	uint32_t buttonUp;
	uint32_t buttonDown;
	
	class wsimButton : public QPushButton {
		public:
			int buttoncode;
			wsimQtApplication* app;
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

	// own events
	class wsimEvent : public QEvent {
		private:
			wsimGUIEvents mType;
		public:
			wsimEvent(wsimGUIEvents type) : QEvent(QEvent::User) {
				mType = type;
			}
			int wsimtype() {return mType;}
	};
	
	void setbuttonDown(int code) {
		buttonDown |= code;
	}
	void setbuttonUp(int code) {
		buttonUp |= code;
	}
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
	
	// event handling
	bool event ( QEvent * e ) {
		if (e->type()==QEvent::User) {
			wsimEvent* ee = static_cast<wsimEvent*>(e);
			switch(ee->wsimtype()) {
				case GUIWindowEvent:
					setupGUI(); break;
				case CloseWindowEvent:
					window.close();
					this->exit(0);
				break;
				case ShowData: {
					if (currentTimerID)
						break;
					currentTimerID = startTimer(50);
					showData();
				}
				break;
			}
		}
		return QApplication::event(e);
	}
	
	void timerEvent ( QTimerEvent * event ) {
		if (event->timerId() != currentTimerID)
			return;
		killTimer(currentTimerID);
		currentTimerID = 0;
		showData();
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
	
	void close() {
		this->postEvent(this, new wsimEvent(CloseWindowEvent));
	}
	
	// slow down to 50ms update time
	void copyBitmap(uint8_t* data) {
		framebufferData = data;
 		this->postEvent(this, new wsimEvent(ShowData));
	}
};
wsimQtApplication* app;

void *StartQAppThread(void*) {
  startWorker(_argc, _argv);
  qDebug() << "Emulator beendet!";
  pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
  app = new wsimQtApplication(argc, argv);
  
	_argc = argc;
	_argv = argv;
   pthread_t thread1;  
   int rc = pthread_create(&thread1, NULL, StartQAppThread, 0);

  app->exec();
  return 0;
}

int ui_options_add(void)
{
  options_add(& gui_opt            );
  options_add(& title_opt          );
  return 0;
}

int ui_create(int w, int h, int id)
{
	


#define MAX_TITLE_SIZE 20
  char name[MAX_TITLE_SIZE];

  if (id != -1)
    {
      if (title_opt.isset && (title_opt.value != NULL))
	{
	  strncpyz(name, title_opt.value, MAX_TITLE_SIZE);
	}
      else
	{
	  snprintf(name,MAX_TITLE_SIZE,"WSim %d",id);
	}
    }
  else
    {
      strncpyz(name, "WSim", MAX_TITLE_SIZE);
    }


  app->setWindowData(w, h, name, GUI_DATA_MACHINE.framebuffer_size);
  GUI_DATA_MACHINE.b_up   = 0;
  GUI_DATA_MACHINE.b_down = 0;
  return UI_OK;
}

void ui_delete(void)
{
   app->close();
}

int ui_refresh(int modified)
{
  uint8_t *fb;
  static int loop = 0;

  if (modified)
    {
		app->copyBitmap(GUI_DATA_MACHINE.framebuffer);
    }
  return UI_OK;
}

int ui_getevent(void)
{
	if (!app->buttonDown && !app->buttonUp)
	  return UI_EVENT_NONE;
	else {
		GUI_DATA_MACHINE.b_up = app->buttonUp;
		GUI_DATA_MACHINE.b_down = app->buttonDown;
		app->buttonUp = 0;
		app->buttonDown = 0;
		return UI_EVENT_USER;
	}
}

void ui_default_input (const char *name)
{
  int ev;
  switch ((ev = ui_getevent()))
    {
    case UI_EVENT_USER:
      DMSG_LIB_UI("%s: UI event %04x %04x\n", name, machine.ui.b_up,machine.ui.b_down);
      break;

    case UI_EVENT_QUIT:
      DMSG_LIB_UI("%s: UI event QUIT\n",name);
      mcu_signal_add(SIG_HOST | SIGTERM);
      break;

    case UI_EVENT_NONE:
      break;
	
    default:
      ERROR("%s: unknown ui event\n",name);
      break;
    }
}

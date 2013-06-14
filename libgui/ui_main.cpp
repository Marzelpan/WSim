
/**
 *  \brief  Graphical UI definition using the Qt Toolkit (version 4.7+)
 *  \author David Graeff
 *  \date   2013, June
 **/

#include "qtapplication.h"
#include "ui.h"
// TODO: we are using pthread here, this may not be available on all supported plattforms
#include <pthread.h>

extern "C" {
#include "src/mainworker.h"
}

/**
  * Global variables: The commandline arguments have to be global
  * to be accessable from both threads (gui and simulation).
  */
int _argc;
char** _argv;
wsimQtApplication* app;

/**
 * This method will be executed in another thread. 
 * startWorker was the original main method: The simulation
 * have to run as a second thread because Qt needs the main thread.
 */
void *StartQAppThread(void*) {
  startWorker(_argc, _argv);
  qDebug() << "Emulator beendet!";
  pthread_exit(NULL);
}

/**
  * This main method is used if the gui is enabled
  */
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


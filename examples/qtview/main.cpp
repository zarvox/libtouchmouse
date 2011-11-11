#include <QApplication>
#include "mousepoller.h"
#include "mouseviewer.h"

int main(int argc, char** argv) {
	QApplication* app = new QApplication(argc, argv);

	MousePoller* poller = new MousePoller();

	MouseViewer* viewer = new MouseViewer();
	QObject::connect(poller, SIGNAL(mouseUpdate(QByteArray,QDateTime)),
			viewer, SLOT(imageUpdate(QByteArray,QDateTime)));
	viewer->resize(15 * 12, 13 * 12 );
	viewer->show();
	poller->startPolling();
	
	int retval = app->exec();
	poller->stopPolling();
	return retval;
}

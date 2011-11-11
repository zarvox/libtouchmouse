#ifndef __MOUSETHREAD_H__
#define __MOUSETHREAD_H__

#include <QThread>

class MouseThread : public QThread {
	Q_OBJECT
public:
	MouseThread(QObject* parent = 0);
	~MouseThread();

	void run();
public slots:
	void shutdown();
private:
	bool shutdownRequested;
};

#endif // __MOUSETHREAD_H__


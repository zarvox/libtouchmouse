#ifndef __MOUSEPOLLER_H__
#define __MOUSEPOLLER_H__

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <libtouchmouse/libtouchmouse.h>

class QTimer;

class MousePoller : public QObject {
	Q_OBJECT
public:
	MousePoller(int index = 0, QObject* parent = 0);
	~MousePoller();
	static void callback(touchmouse_callback_info *cbdata);
	void memberCallback(touchmouse_callback_info* cbdata);

public slots:
	void startPolling();
	void stopPolling();
	void pollMouse();

signals:
	void mouseUpdate(QByteArray image, QDateTime timestamp);

private:
	QTimer* timer;
	touchmouse_device *dev;
	bool touchmouse_okay;
};

#endif // __MOUSEPOLLER_H__

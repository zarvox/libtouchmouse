#include "mousepoller.h"
#include <QTimer>
#include <libtouchmouse/libtouchmouse.h>
#include <QDebug>

MousePoller::MousePoller(int index, QObject* parent) : QObject(parent) {
	touchmouse_okay = false;
	timer = new QTimer(this);
	timer->setInterval(1);
	connect(timer, SIGNAL(timeout()),
			this, SLOT(pollMouse()));
	int res;
	res = touchmouse_init();
	if (res != 0) {
		qDebug() << "Failed to initialize libtouchmouse";
	}
	int devs_found = 0;
	touchmouse_device_info* devs = touchmouse_enumerate_devices();
	touchmouse_device_info* d = devs;
	while(d) {
		d = d->next;
		devs_found++;
	}
	if (devs_found < index) {
		qDebug() << "Didn't see enough Touchmouse devices to open device" << index;
		touchmouse_free_enumeration(devs);
		return;
	}
	dev = NULL;
	d = devs;
	for(int i = 0; i < index ; i++) {
		d = d->next;
	}
	res = touchmouse_open(&dev, d);
	touchmouse_free_enumeration(devs);
	if (res != 0) {
		qDebug() << "Failed to open Touchmouse device";
		return;
	}
	res = touchmouse_set_image_update_callback(dev, MousePoller::callback);
	if (res != 0) {
		qDebug() << "Failed to set Touchmouse device to full updates mode";
		return;
	}
	res = touchmouse_set_device_userdata(dev, this);
	if (res != 0) {
		qDebug() << "Failed to set device userdata";
		return;
	}
	touchmouse_okay = true;
}

MousePoller::~MousePoller() {
	touchmouse_close(dev);
	touchmouse_shutdown();
}

void MousePoller::callback(touchmouse_callback_info *cbdata) {
	//qDebug() << "static callback triggered";
	MousePoller* poller = static_cast<MousePoller*>(cbdata->userdata);
	poller->memberCallback(cbdata);
}

void MousePoller::memberCallback(touchmouse_callback_info *cbdata) {
	QByteArray ba((const char*)cbdata->image, 195);
	// TODO: aggregate timestamps using mouse clock for precision.
	QDateTime timestamp = QDateTime::currentDateTime();
	emit mouseUpdate(ba, timestamp);
}

void MousePoller::pollMouse() {
	int res;
	res = touchmouse_process_events_timeout(dev, 0);
	if (res < 0) {
		if (res != -1) // -1 is returned on recoverable errors, -2 is a fatal hid_read failure
			// TODO: make an error enumeration
			touchmouse_okay = false;
	}
}

void MousePoller::startPolling() {
	if (touchmouse_okay) {
		int res;
		res = touchmouse_set_device_mode(dev, TOUCHMOUSE_RAW_IMAGE);
		if (res != 0) {
			qDebug() << "Failed to enable full image updates";
			touchmouse_okay = false;
			return;
		}
		timer->start();
	} else {
		qDebug() << "MousePoller::startPolling() called, but libtouchmouse not okay";
	}
}

void MousePoller::stopPolling() {
	if (touchmouse_okay) {
		int res;
		res = touchmouse_set_device_mode(dev, TOUCHMOUSE_DEFAULT);
		if (res != 0) {
			qDebug() << "Failed to disable full image updates";
			touchmouse_okay = false;
			return;
		}
		timer->stop();
	} else {
		qDebug() << "MousePoller::stopPolling() called, but libtouchmouse not okay";
	}
}


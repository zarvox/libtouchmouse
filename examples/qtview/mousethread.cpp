#include "mousethread.h"

MouseThread::MouseThread(QObject* parent) : QThread(parent) {
	shutdownRequested = false;
	moveToThread(this);
}



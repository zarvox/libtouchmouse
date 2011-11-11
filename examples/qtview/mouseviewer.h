#ifndef __MOUSEVIEWER_H__
#define __MOUSEVIEWER_H__

#include <QWidget>
#include <QByteArray>
#include <QDateTime>

class QPaintEvent;

class MouseViewer : public QWidget {
	Q_OBJECT
public:
	MouseViewer(QWidget* parent = 0);
	~MouseViewer();
	void paintEvent(QPaintEvent* e);
public slots:
	void imageUpdate(QByteArray image, QDateTime timestamp);
private:
	QByteArray lastImage;
};

#endif // __MOUSEVIEWER_H__

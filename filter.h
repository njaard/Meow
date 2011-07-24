#ifndef MEOW_FILTER_H
#define MEOW_FILTER_H

#include <qwidget.h>

class QLineEdit;

namespace Meow
{

class Filter : public QWidget
{
	Q_OBJECT
	QLineEdit *editor;
	
public:
	Filter(QWidget *parent);

signals:
	void textChanged(const QString &text);
	void done();

protected:
	void keyPressEvent(QKeyEvent *event);
	void showEvent(QShowEvent *event);
};


}

#endif

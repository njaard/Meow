#include "filter.h"

#include <qlineedit.h>
#include <qboxlayout.h>
#include <qevent.h>

Meow::Filter::Filter(QWidget *parent)
	: QWidget(parent)
{
	QHBoxLayout *l = new QHBoxLayout(this);
	editor = new QLineEdit(this);
	l->addWidget(editor);
	
	l->setContentsMargins(0, 0, 0, 0);
	
	connect(
			editor,
			SIGNAL(textChanged(QString)),
			SIGNAL(textChanged(QString))
		);
}

void Meow::Filter::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape)
	{
		emit done();
		hide();
	}
}


void Meow::Filter::showEvent(QShowEvent *event)
{
	editor->setFocus(Qt::OtherFocusReason);
	editor->selectAll();
	emit textChanged(editor->text());
}

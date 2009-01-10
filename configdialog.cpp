#include "configdialog.h"

Meow::ConfigWidget::ConfigWidget(QWidget *parent)
	: QWidget(parent)
{
}

void Meow::ConfigWidget::modified()
{

}

Meow::ConfigDialog::ConfigDialog(QWidget *parent)
	: KPageDialog(parent)
{
	connect(this, SIGNAL(applyClicked()), SLOT(apply()));
	connect(this, SIGNAL(okClicked()), SLOT(apply()));
}

void Meow::ConfigDialog::addPage(ConfigWidget *widget, const QString &name)
{
	KPageDialog::addPage(widget, name);
	pages.append(widget);
}


void Meow::ConfigDialog::apply()
{
	for (
			QList<ConfigWidget*>::iterator i = pages.begin();
			i != pages.end(); ++i
		)
	{
		(*i)->apply();
	}
}

void Meow::ConfigDialog::show()
{
	for (
			QList<ConfigWidget*>::iterator i = pages.begin();
			i != pages.end(); ++i
		)
	{
		(*i)->load();
	}
	KPageDialog::show();
}

// kate: space-indent off; replace-tabs off;

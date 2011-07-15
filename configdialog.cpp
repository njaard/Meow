#include "configdialog.h"

#include <qpushbutton.h>
#include <qgridlayout.h>

Meow::ConfigWidget::ConfigWidget(QWidget *parent)
	: QWidget(parent)
{
}

void Meow::ConfigWidget::modified()
{

}

Meow::ConfigDialog::ConfigDialog(QWidget *parent)
	: ConfigPageBase(parent)
{
#ifdef MEOW_WITH_KDE
	connect(this, SIGNAL(applyClicked()), SLOT(apply()));
	connect(this, SIGNAL(okClicked()), SLOT(apply()));
#else
	setWindowTitle(tr("Configure Meow"));
	QGridLayout *layout = new QGridLayout(this);

	tabs = new QTabWidget(this);
	connect(this, SIGNAL(accepted()), SLOT(apply()));
	layout->addWidget(tabs, 0, 0, 1, 3);
	
	QPushButton *ok = new QPushButton(tr("Ok"), this);
	connect(ok, SIGNAL(clicked()), SLOT(accept()));
	layout->addWidget(ok, 1, 1);

	QPushButton *cancel = new QPushButton(tr("&Cancel"), this);
	connect(cancel, SIGNAL(clicked()), SLOT(reject()));
	layout->addWidget(cancel, 1, 2);
#endif

}

void Meow::ConfigDialog::addPage(ConfigWidget *widget, const QString &name)
{
#ifdef MEOW_WITH_KDE
	KPageDialog::addPage(widget, name);
#else
	tabs->addTab(widget, name);
#endif
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

#ifdef MEOW_WITH_KDE
void Meow::ConfigDialog::show()
#else
void Meow::ConfigDialog::showEvent(QShowEvent*e)
#endif
{
	for (
			QList<ConfigWidget*>::iterator i = pages.begin();
			i != pages.end(); ++i
		)
	{
		(*i)->load();
	}
#ifdef MEOW_WITH_KDE
	ConfigPageBase::show();
#else
	ConfigPageBase::showEvent(e);
#endif
}

// kate: space-indent off; replace-tabs off;

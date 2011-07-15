#ifndef MEOW_CONFIGDIALOG_H
#define MEOW_CONFIGDIALOG_H

#ifdef MEOW_WITH_KDE
#include <kpagedialog.h>
typedef KPageDialog ConfigPageBase;
#else
#include <Qt/qdialog.h>
#include <Qt/qtabwidget.h>
typedef QDialog ConfigPageBase;
#endif

namespace Meow
{


class ConfigWidget : public QWidget
{
	Q_OBJECT
	
public:
	ConfigWidget(QWidget *parent);

	virtual void load()=0;
	virtual void apply()=0;

protected slots:
	void modified();
};


class ConfigDialog : public ConfigPageBase
{
	Q_OBJECT
	
	QList<ConfigWidget*> pages;
	
#ifndef MEOW_WITH_KDE
	QTabWidget *tabs;
#endif

	
public:
	ConfigDialog(QWidget *parent);
	
	void addPage(ConfigWidget *widget, const QString &name);

#ifdef MEOW_WITH_KDE
public:
	void show();
#endif

#ifndef MEOW_WITH_KDE
protected:
	void showEvent(QShowEvent*e);
#endif

private slots:
	void apply();
};

}

#endif
// kate: space-indent off; replace-tabs off;

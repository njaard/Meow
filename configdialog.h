#ifndef MEOW_CONFIGDIALOG_H
#define MEOW_CONFIGDIALOG_H

#include <kpagedialog.h>

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

class ConfigDialog : public KPageDialog
{
	Q_OBJECT
	
	QList<ConfigWidget*> pages;
public:
	ConfigDialog(QWidget *parent);
	
	void addPage(ConfigWidget *widget, const QString &name);
	
	void show();
	
private slots:
	void apply();
};

}

#endif
// kate: space-indent off; replace-tabs off;

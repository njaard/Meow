#ifndef KITTENPLAYER_MAINWINDOW_H
#define KITTENPLAYER_MAINWINDOW_H

#include <kxmlguiwindow.h>

class KSystemTrayIcon;
class KUrl;

namespace KittenPlayer
{

class TreeView;
class Player;
class Collection;
class DirectoryAdder;

class MainWindow : public KXmlGuiWindow
{
	Q_OBJECT
	struct MainWindowPrivate;
	MainWindowPrivate *d;

public:
	MainWindow();
	~MainWindow();

public slots:
	void addFiles();
	void addDirectory();
	void addFile(const KUrl &url);

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void adderDone();
	void showItemContext(const QPoint &at);

private:
	void beginDirectoryAdd(const KUrl &url);
	QIcon renderIcon(const QString& baseIcon, const QString &overlayIcon) const;
};


}

#endif
 
// kate: space-indent off; replace-tabs off;

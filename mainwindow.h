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
	TreeView *view;
	Player *player;
	KSystemTrayIcon *tray;
	Collection *collection;
	DirectoryAdder *mAdder;

public:
	MainWindow();

public slots:
	void addFiles();
	void addDirectory();
	void addFile(const KUrl &url);

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void adderDone();

private:
	void beginDirectoryAdd(const KUrl &url);
	QIcon renderIcon(const QString& baseIcon, const QString &overlayIcon) const;
};


}

#endif
 
// kate: space-indent off; replace-tabs off;

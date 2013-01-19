#ifndef MEOW_MAINWINDOW_H
#define MEOW_MAINWINDOW_H

#include <qmainwindow.h>

#include <qsystemtrayicon.h>
#include <db/file.h>

class QSlider;
class QSignalMapper;
class QUrl;

namespace Meow
{

class TreeView;
class Player;
class Collection;
class DirectoryAdder;
class File;

class MainWindow : public QMainWindow
{
	Q_OBJECT
	struct MainWindowPrivate;
	MainWindowPrivate *d;

public:
	MainWindow();
	~MainWindow();

private slots:
	void reloadCollections();

public slots:
	void addFiles();
	void addDirs();
	void addFile(const QUrl &url);
	void toggleVisible();

protected:
	virtual void closeEvent(QCloseEvent *event);
	virtual void wheelEvent(QWheelEvent *event);
	virtual void dropEvent(QDropEvent *event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual bool eventFilter(QObject *object, QEvent *event);
	
private slots:
	void adderDone();
	void showItemContext(const QPoint &at);
	void changeCaption(const File &f);
	void systemTrayClicked(QSystemTrayIcon::ActivationReason reason);
	void itemProperties();
	void selectorActivated(QAction*);
	
	void fileDialogAccepted();
	void fileDialogClosed();

	void showSettings();
	void showAbout();
	void toggleToolBar();
	void toggleMenuBar();
	void quitting();
	void showVolume();
	void isPlaying(bool pl);

	void newCollection();
	void copyCollection();
	void renameCollection();
	void deleteCollection();

	void loadCollection(const QString &collection, FileId first);
	void loadCollection(const QString &collection) { loadCollection(collection, 0); }

private:
	void beginDirectoryAdd(const QString &url);
	static bool globalEventFilter(void *_m);
	QIcon renderIcon(const QIcon& baseIcon, const QIcon &overlayIcon) const;
};

}

#endif
 
// kate: space-indent off; replace-tabs off;

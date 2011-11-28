#ifndef MEOW_MAINWINDOW_H
#define MEOW_MAINWINDOW_H

#include <kurl.h>
#include <kxmlguiwindow.h>
#include <ktoolbarpopupaction.h>
#include <kdialog.h>
typedef KXmlGuiWindow MeowMainWindowType;
typedef KUrl MeowUrlType;
class KUrl;

#include <qsystemtrayicon.h>

#include <db/file.h>

class QSlider;
class QSignalMapper;
class QLineEdit;

namespace Meow
{

class TreeView;
class Player;
class Collection;
class DirectoryAdder;
class File;

class MainWindow : public MeowMainWindowType
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
	void addFile(const KUrl &url);
	void toggleVisible();

protected:
	virtual void closeEvent(QCloseEvent *event);
	virtual void wheelEvent(QWheelEvent *event);
	virtual void dropEvent(QDropEvent *event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual bool eventFilter(QObject *object, QEvent *event);
	virtual bool queryExit();


private slots:
	void adderDone();
	void showItemContext(const QPoint &at);
	void changeCaption(const File &f);
	void systemTrayClicked(QSystemTrayIcon::ActivationReason reason);
	void changePlaybackOrder(int index);

	void groupByAlbum(bool x);
	void itemProperties();
	
	void fileDialogAccepted();
	void fileDialogClosed();

	void showSettings();
	void toggleToolBar();
	void toggleMenuBar();

	void configureShortcuts();

	void isPlaying(bool v);

	void newCollection();
	void copyCollection();
	void renameCollection();
	void deleteCollection();
	void loadCollection(const QString &collection, FileId first);
	void loadCollection(const QString &collection) { loadCollection(collection, 0); }

#ifdef MEOW_WITH_KDE
	void openWith(const QString &desktopEntryName);
#endif

private:
	void beginDirectoryAdd(const KUrl &url);
	QIcon renderIcon(const QString& baseIcon, const QString &overlayIcon) const;
};

#ifdef MEOW_WITH_KDE
class VolumeAction : public KToolBarPopupAction
{
	Q_OBJECT
	QSlider *slider;
	QSignalMapper *signalMapper;
	
public:
	VolumeAction(const KIcon& icon, const QString& text, QObject *parent);
	~VolumeAction();
	virtual QWidget* createWidget(QWidget* parent);
	
public slots:
	void setVolume(int percent);
	
signals:
	void volumeChanged(int percent);

private slots:
	void showPopup(QWidget *button);
};
#endif

}

#endif
 
// kate: space-indent off; replace-tabs off;

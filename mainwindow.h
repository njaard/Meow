#ifndef MEOW_MAINWINDOW_H
#define MEOW_MAINWINDOW_H

#include <kxmlguiwindow.h>
#include <ktoolbarpopupaction.h>

#include <qsystemtrayicon.h>

class KSystemTrayIcon;
class KUrl;
class QSlider;
class QSignalMapper;

namespace Meow
{

class TreeView;
class Player;
class Collection;
class DirectoryAdder;
class File;

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
	void addFile(const KUrl &url);
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
	void changePlaybackOrder(int index);

	void groupByAlbum(bool x);
	void itemProperties();
	
	void fileDialogAccepted();
	void fileDialogClosed();

	void showSettings();
	void toggleToolBar();
	void toggleMenuBar();

private:
	void beginDirectoryAdd(const KUrl &url);
	QIcon renderIcon(const QString& baseIcon, const QString &overlayIcon) const;
};

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

}

#endif
 
// kate: space-indent off; replace-tabs off;

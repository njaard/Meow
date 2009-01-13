#include "mainwindow.h"
#include "treeview.h"
#include "player.h"
#include "directoryadder.h"
#include "scrobble.h"

#include <db/file.h>
#include <db/base.h>
#include <db/collection.h>

#include <klocale.h>
#include <kactioncollection.h>
#include <kfiledialog.h>
#include <ksystemtrayicon.h>
#include <kstandarddirs.h>
#include <kiconeffect.h>
#include <kxmlguifactory.h>
#include <kconfig.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>
#include <kwallet.h>

#include <qpixmap.h>
#include <qicon.h>
#include <qevent.h>
#include <qmenu.h>
#include <qslider.h>
#include <qsignalmapper.h>
#include <qtoolbutton.h>
#include <qapplication.h>

struct Meow::MainWindow::MainWindowPrivate
{
	TreeView *view;
	Player *player;
	KSystemTrayIcon *tray;
	Base db;
	Collection *collection;
	DirectoryAdder *adder;
	
	KAction *itemProperties, *itemRemove;
	
	bool nowFiltering;
	
	ConfigDialog *settingsDialog;
	Scrobble *scrobble;
};

Meow::MainWindow::MainWindow()
{
	d = new MainWindowPrivate;
	d->adder = 0;
	d->settingsDialog = 0;
	d->nowFiltering = false;
	
	d->db.open(KGlobal::dirs()->saveLocation("data", "meow/")+"collection");
	
	d->collection = new Collection(&d->db);

	d->player = new Player;
	d->view = new TreeView(this, d->player, d->collection);
	d->view->installEventFilter(this);
	
	d->scrobble = new Scrobble(this, d->player, d->collection);
	if ( d->scrobble->isEnabled() )
	{
		KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
				KWallet::Wallet::NetworkWallet(), effectiveWinId()
			);
		if (wallet)
		{
			if ( !wallet->hasFolder( "Meow" ) )
				wallet->createFolder( "Meow" );
			wallet->setFolder( "Meow" );

			QString retrievedPass;
			QByteArray retrievedUser;
			if ( !wallet->readEntry( "AudioScrobbler Username", retrievedUser ) )
				d->scrobble->setUsername(QString::fromUtf8(retrievedUser));
			if ( !wallet->readPassword( "AudioScrobbler Password", retrievedPass ) )
				d->scrobble->setPassword(retrievedPass);
			d->scrobble->begin();
		}
	}
	setCentralWidget(d->view);
	
	d->tray = new KSystemTrayIcon("speaker", this);
	d->tray->installEventFilter(this);
	d->tray->show();
	
	QMenu *const trayMenu = d->tray->contextMenu();
	
	{
		KAction *ac;
		ac = actionCollection()->addAction("add_files", this, SLOT(addFiles()));
		ac->setText(i18n("Add &Files..."));
		ac->setIcon(KIcon("list-add"));
				
		ac = actionCollection()->addAction("pause", d->player, SLOT(playpause()));
		ac->setText(i18n("Paws"));
		ac->setIcon(KIcon("media-playback-pause"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::Key_P), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		trayMenu->addAction(ac);
		
		ac = actionCollection()->addAction("next", d->view, SLOT(nextSong()));
		ac->setText(i18n("Next Song"));
		ac->setIcon(KIcon("media-skip-forward"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::Key_Right), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		trayMenu->addAction(ac);
		
		ac = actionCollection()->addAction("previous", d->view, SLOT(nextSong()));
		ac->setText(i18n("Previous Song"));
		ac->setIcon(KIcon("media-skip-backward"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::Key_Left), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		trayMenu->addAction(ac);
		
		ac = actionCollection()->addAction("volumeup", d->player, SLOT(volumeUp()));
		ac->setText(i18n("Volume Up"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::SHIFT+Qt::Key_Up), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		
		ac = actionCollection()->addAction("volumedown", d->player, SLOT(volumeDown()));
		ac->setText(i18n("Volume Down"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::SHIFT+Qt::Key_Down));
		
		ac = actionCollection()->addAction("seekforward", d->player, SLOT(seekForward()));
		ac->setText(i18n("Seek Forward"));
		ac->setIcon(KIcon("media-seek-forward"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::SHIFT+Qt::Key_Right), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		
		ac = actionCollection()->addAction("seekbackward", d->player, SLOT(seekBackward()));
		ac->setText(i18n("Seek Backward"));
		ac->setIcon(KIcon("media-seek-forward"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::SHIFT+Qt::Key_Left), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		
		ac = actionCollection()->addAction("togglegui", this, SLOT(toggleVisible()));
		ac->setText(i18n("Show/Hide Main Window"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::SHIFT+Qt::Key_L), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		
		VolumeAction *va = new VolumeAction(KIcon("player-volume"), i18n("Volume"), actionCollection());
		ac = actionCollection()->addAction("volume", va);
		connect(va, SIGNAL(volumeChanged(int)), d->player, SLOT(setVolume(int)));
		connect(d->player, SIGNAL(volumeChanged(int)), va, SLOT(setVolume(int)));
		
		ac = actionCollection()->addAction("add_dir", this, SLOT(addDirectory()));
		ac->setText(i18n("Add Fol&ders..."));
		ac->setIcon(KIcon("folder"));
		
		ac = actionCollection()->addAction(
				KStandardAction::Close,
				this,
				SLOT(deleteLater())
			);
		
		ac = actionCollection()->addAction(
				KStandardAction::Preferences,
				this,
				SLOT(showSettings())
			);
	}
	
	{ // context menu
		d->itemProperties = actionCollection()->addAction("remove_item", d->view, SLOT(removeSelected()));
		d->itemProperties->setText(i18n("&Remove from playlist"));
		d->itemProperties->setIcon(KIcon("edit-delete"));
		d->itemProperties->setShortcut(Qt::Key_Delete);
		
		d->itemProperties = actionCollection()->addAction("item_properties", this, SLOT(itemProperties()));
		d->itemProperties->setText(i18n("&Properties"));
	}
	
	connect(d->collection, SIGNAL(added(File)), d->view, SLOT(addFile(File)));
	connect(d->view, SIGNAL(kdeContextMenu(QPoint)), SLOT(showItemContext(QPoint)));
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(changeCaption(File)));
	
	
	createGUI();

	KConfigGroup meow = KGlobal::config()->group("state");
	d->player->setVolume(meow.readEntry<int>("volume", 50));
	
	FileId first = meow.readEntry<FileId>("lastPlayed", 0);
	
	d->collection->getFilesAndFirst(first);
	if (first)
	{
		// clever trick here:
		// if first is valid, that means that getFilesAndFirst has loaded it first
		// and it did so right now (not later in the event loop)
		// furthermore, it will also load the rest of the files later on 
		// in the event loop, which means that right now, first is the only
		// item in the list
		d->view->nextSong();
	}
}

Meow::MainWindow::~MainWindow()
{
	KConfigGroup meow = KGlobal::config()->group("state");
	meow.writeEntry<int>("volume", d->player->volume());
	meow.writeEntry<FileId>("lastPlayed", d->player->currentFile().fileId());

	if ( d->scrobble->isEnabled() )
	{
		KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
				KWallet::Wallet::NetworkWallet(), effectiveWinId()
			);
		if (wallet)
		{
			// use the KPdf folder (and create if missing)
			if ( !wallet->hasFolder( "Meow" ) )
				wallet->createFolder( "Meow" );
			wallet->setFolder( "Meow" );

			// look for the pass in that folder
			wallet->writeEntry( "AudioScrobbler Username", d->scrobble->username().toUtf8() );
			wallet->writePassword( "AudioScrobbler Password", d->scrobble->password() );
		}
	}
	
	delete d->collection;
	delete d;
}

void Meow::MainWindow::addFile(const KUrl &url)
{
	if (url.isLocalFile())
		d->collection->add( url.path() );
}

void Meow::MainWindow::addFiles()
{
	KUrl::List files = KFileDialog::getOpenUrls(
			KUrl("kfiledialog:///mediadir"), d->player->mimeTypes().join(" "),
			this, i18n("Select Files to Add")
		);

	for(KUrl::List::Iterator it=files.begin(); it!=files.end(); ++it)
		addFile(*it);
}

void Meow::MainWindow::addDirectory()
{
	QString folder = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///mediadir"), this,
		i18n("Select Folder to Add"));
	
	if (folder.isEmpty())
		return;

	KUrl url;
	url.setPath(folder);
	beginDirectoryAdd(url);
}

void Meow::MainWindow::toggleVisible()
{
	d->tray->toggleActive();
}
	
void Meow::MainWindow::closeEvent(QCloseEvent *event)
{
	toggleVisible();
	event->ignore();
}

void Meow::MainWindow::wheelEvent(QWheelEvent *event)
{
	if (!d->nowFiltering)
		d->player->setVolume(d->player->volume() + event->delta()*10/120);
}
bool Meow::MainWindow::eventFilter(QObject *object, QEvent *event)
{
	if (!d->nowFiltering && object == d->view && event->type() == QEvent::Wheel)
	{
		d->nowFiltering = true;
		QApplication::sendEvent(d->view, event);
		d->nowFiltering = false;
		return true;
	}
	else if (object == d->tray && event->type() == QEvent::Wheel)
	{
		QApplication::sendEvent(this, event);
	}
	return false;
}

void Meow::MainWindow::adderDone()
{
	delete d->adder;
	d->adder = 0;
}

void Meow::MainWindow::beginDirectoryAdd(const KUrl &url)
{
	if (d->adder)
	{
		d->adder->add(url);
	}
	else
	{
		d->adder = new DirectoryAdder(url, this);
		connect(d->adder, SIGNAL(done()), SLOT(adderDone()));
		connect(d->adder, SIGNAL(addFile(KUrl)), SLOT(addFile(KUrl)));
	}
}

void Meow::MainWindow::showItemContext(const QPoint &at)
{
	QMenu *const menu = static_cast<QMenu*>(factory()->container("item_context", this));
	menu->popup(at);
}

QIcon Meow::MainWindow::renderIcon(const QString& baseIcon, const QString &overlayIcon) const
{
	if (overlayIcon.isNull())
		return KSystemTrayIcon::loadIcon(baseIcon);

	QImage baseImg = KSystemTrayIcon::loadIcon(baseIcon).pixmap(22).toImage();
	QImage overlayImg = KSystemTrayIcon::loadIcon(overlayIcon).pixmap(22).toImage();
	KIconEffect::overlay(baseImg, overlayImg);

	QPixmap base;
	base.fromImage(baseImg);
	return base;
}

void Meow::MainWindow::changeCaption(const File &f)
{
	setCaption(f.title());
}

void Meow::MainWindow::showSettings()
{
	if (!d->settingsDialog)
	{
		d->settingsDialog = new ConfigDialog(this);
		ScrobbleConfigure *sc=new ScrobbleConfigure(d->settingsDialog, d->scrobble);
		d->settingsDialog->addPage(sc, i18n("AudioScrobbler"));
	}
	
	d->settingsDialog->show();

}


class SpecialSlider : public QSlider
{
public:
	SpecialSlider()
		: QSlider(Qt::Vertical, 0)
	{
		setWindowFlags(Qt::Popup);
		setRange(0, 100);
	}
	virtual void mousePressEvent(QMouseEvent *event)
	{
		if (!rect().contains(event->pos()))
			hide();
		QSlider::mousePressEvent(event);
	}
	
	virtual void keyPressEvent(QKeyEvent *event)
	{
		if (event->key() == Qt::Key_Escape)
			hide();
		QSlider::keyPressEvent(event);
	}
	virtual void hideEvent(QHideEvent *event)
	{
		releaseMouse();
		QSlider::hideEvent(event);
	}
};

Meow::VolumeAction::VolumeAction(const KIcon& icon, const QString& text, QObject *parent)
	: KToolBarPopupAction(icon, text, parent)
{
	signalMapper = new QSignalMapper(this);
	
	slider = new SpecialSlider;
	connect(slider, SIGNAL(valueChanged(int)), SIGNAL(volumeChanged(int)));
	connect(
			signalMapper, SIGNAL(mapped(QWidget*)),
			this, SLOT(showPopup(QWidget*))
		);
	setMenu(0);
}

Meow::VolumeAction::~VolumeAction()
{
	delete slider;
}

QWidget* Meow::VolumeAction::createWidget(QWidget* parent)
{
	QWidget *w = KToolBarPopupAction::createWidget(parent);
	if (QToolButton *b = qobject_cast<QToolButton*>(w))
	{
		b->setPopupMode(QToolButton::DelayedPopup);
		connect(b, SIGNAL(triggered(QAction*)), signalMapper, SLOT(map()));
		signalMapper->setMapping(b, b);
	}
	return w;
}
void Meow::VolumeAction::setVolume(int percent)
{
	slider->setValue(percent);
}

void Meow::VolumeAction::showPopup(QWidget *button)
{
	slider->move(button->mapToGlobal(button->rect().bottomLeft()));
	slider->show();
	slider->raise();
	slider->grabMouse();
}


// kate: space-indent off; replace-tabs off;

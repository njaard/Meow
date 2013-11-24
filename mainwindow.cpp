#include "mainwindow.h"
#include "treeview.h"
#include "player.h"
#include "directoryadder.h"
#include "scrobble.h"
#include "fileproperties.h"
#include "filter.h"

#include <db/file.h>
#include <db/base.h>
#include <db/collection.h>

#include <kdeversion.h>
#include <klocale.h>
#include <kactioncollection.h>
#include <kselectaction.h>
#include <kfiledialog.h>
#include <ksystemtrayicon.h>
#include <kstandarddirs.h>
#include <kiconeffect.h>
#include <kxmlguifactory.h>
#include <kconfig.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>
#include <kwindowsystem.h>
#include <ktoolbar.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <kservice.h>
#include <kmimetypetrader.h>
#include <kshortcutsdialog.h>
#include <kactionmenu.h>
#include <krun.h>
#include <kmenu.h>

#include <qpixmap.h>
#include <qicon.h>
#include <qevent.h>
#include <qmenu.h>
#include <qslider.h>
#include <qsignalmapper.h>
#include <qtoolbutton.h>
#include <qapplication.h>
#include <qpainter.h>
#include <qboxlayout.h>
#include <qlineedit.h>
#include <qlabel.h>

struct Meow::MainWindow::MainWindowPrivate
{
	TreeView *view;
	Player *player;
	KSystemTrayIcon *tray;
	Base db;
	Collection *collection;
	DirectoryAdder *adder;
	
	KAction *itemProperties;
	KAction *playPauseAction;
	KSelectAction *playbackOrder;
	KActionMenu *openWith, *collectionsAction;
	QActionGroup *collectionsActionGroup;
	
	QList<KAction*> collectionActions;
	
	bool nowFiltering;
	
	ConfigDialog *settingsDialog;
	Scrobble *scrobble;
	
	KFileDialog *openFileDialog;
	Filter *filter;
};

#include "mainwindow_common.cpp"

Meow::MainWindow::MainWindow(bool dontPlayLastPlayed)
{
	d = new MainWindowPrivate;
	d->adder = 0;
	d->settingsDialog = 0;
	d->nowFiltering = false;
	d->openFileDialog = 0;
	
	d->collection = new Collection(&d->db);

	QWidget *owner = new QWidget(this);
	QVBoxLayout *ownerLayout = new QVBoxLayout(owner);
	ownerLayout->setContentsMargins(0, 0, 0, 0);
	ownerLayout->setSpacing(0);

	d->player = new Player;
	d->view = new TreeView(owner, d->player, d->collection);
	d->view->installEventFilter(this);
	ownerLayout->addWidget(d->view);
	
	d->filter = new Filter(owner);
	d->filter->hide();
	connect(d->filter, SIGNAL(textChanged(QString)), d->view, SLOT(filter(QString)));
	connect(d->filter, SIGNAL(done()), d->view, SLOT(stopFilter()));
	ownerLayout->addWidget(d->filter);
	
	d->scrobble = new Scrobble(this, d->player, d->collection);
	setCentralWidget(owner);

	d->tray = new KSystemTrayIcon("speaker", this);
	d->tray->installEventFilter(this);
	d->tray->show();
	connect(
			d->tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			SLOT(systemTrayClicked(QSystemTrayIcon::ActivationReason))
		);
	
	QMenu *const trayMenu = d->tray->contextMenu();
	
	KAction *toggleToolbarAction, *toggleMenubarAction;
	
	{
		KAction *ac;
		ac = actionCollection()->addAction("add_files", this, SLOT(addFiles()));
		ac->setText(i18n("Add &Files..."));
		ac->setIcon(KIcon("list-add"));

		ac = actionCollection()->addAction("find", d->filter, SLOT(show()));
		ac->setText(i18n("&Find"));
		{
			QList<QKeySequence> shortcuts;
			shortcuts.append(QKeySequence("/"));
			shortcuts.append(QKeySequence("Ctrl+F"));
			ac->setShortcuts(shortcuts);
		}
		
		d->playPauseAction = actionCollection()->addAction("playpause", d->player, SLOT(playpause()));
		d->playPauseAction->setText(i18n("Play"));
		d->playPauseAction->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::Key_P), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		d->playPauseAction->setIcon(KIcon("media-playback-start"));
		trayMenu->addAction(d->playPauseAction);

		connect(d->player, SIGNAL(playing(bool)), SLOT(isPlaying(bool)));
		
		ac = actionCollection()->addAction("next", d->view, SLOT(nextSong()));
		ac->setText(i18n("Next Song"));
		ac->setIcon(KIcon("media-skip-forward"));
		ac->setGlobalShortcut(KShortcut(Qt::CTRL+Qt::ALT+Qt::Key_Right), KAction::ActiveShortcut | KAction::DefaultShortcut, KAction::NoAutoloading);
		trayMenu->addAction(ac);
		
		ac = actionCollection()->addAction("previous", d->view, SLOT(previousSong()));
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
		
		{
			QStringList playbackOrderItems;
			// this order is significant
			playbackOrderItems << i18n("Each File") << i18n("Random Song")
				<< i18n("Random Album") << i18n("Random Artist");
			d->playbackOrder = new KSelectAction(i18n("Playback Order"), this);
			d->playbackOrder->setItems(playbackOrderItems);
			actionCollection()->addAction("playbackorder", d->playbackOrder);
			connect(d->playbackOrder, SIGNAL(triggered(int)), SLOT(changePlaybackOrder(int)));
		}
		
		{
			d->collectionsActionGroup = new QActionGroup(this);
			
			d->collectionsAction = new KActionMenu(i18n("&Collection"), this);
			KAction *newCol = new KAction(i18n("&New Collection"), this);
			connect(newCol, SIGNAL(activated()), this, SLOT(newCollection()));
			KAction *copyCol = new KAction(i18n("&Copy Collection"), this);
			connect(copyCol, SIGNAL(activated()), this, SLOT(copyCollection()));
			KAction *renameCol = new KAction(i18n("&Rename Collection"), this);
			connect(renameCol, SIGNAL(activated()), this, SLOT(renameCollection()));
			KAction *delCol = new KAction(i18n("&Delete Collection"), this);
			connect(delCol, SIGNAL(activated()), this, SLOT(deleteCollection()));
			d->collectionsAction->addAction(newCol);
			d->collectionsAction->addAction(copyCol);
			d->collectionsAction->addAction(renameCol);
			d->collectionsAction->addAction(delCol);
			d->collectionsAction->addSeparator();
			actionCollection()->addAction("collections", d->collectionsAction);
			reloadCollections();
		}

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
#ifndef Q_WS_MAC
		toggleMenubarAction = ac = actionCollection()->addAction(
				KStandardAction::ShowMenubar,
				this,
				SLOT(toggleMenuBar())
			);
#endif
		toggleToolbarAction = ac = actionCollection()->addAction(
				KStandardAction::ShowToolbar,
				this,
				SLOT(toggleToolBar())
			);

		KStandardAction::keyBindings(guiFactory(), SLOT(configureShortcuts()), actionCollection());
	}
	
	{ // context menu
		KAction *const remove = actionCollection()->addAction("remove_item", d->view, SLOT(removeSelected()));
		remove->setText(i18n("&Remove from playlist"));
		remove->setIcon(KIcon("edit-delete"));
		remove->setShortcut(Qt::Key_Delete);

		KAction *const albumGroup = actionCollection()->addAction("group_by_album");
		albumGroup->setCheckable(true);
		connect(albumGroup, SIGNAL(toggled(bool)), this, SLOT(groupByAlbum(bool)));
		albumGroup->setText(i18n("&Group by album"));

		d->openWith = new KActionMenu(i18n("Open &with"), this);
		actionCollection()->addAction("open_with", d->openWith);

		d->itemProperties = actionCollection()->addAction("item_properties", this, SLOT(itemProperties()));
		d->itemProperties->setText(i18n("&Properties"));
	}
	
	connect(d->view, SIGNAL(kdeContextMenu(QPoint)), SLOT(showItemContext(QPoint)));
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(changeCaption(File)));

	setAcceptDrops(true);
	setAutoSaveSettings();
	createGUI();
	
	toggleToolbarAction->setChecked(toolBar("mainToolBar")->isVisibleTo(this));
	toggleMenubarAction->setChecked(menuBar()->isVisibleTo(this));
	
	KConfigGroup meow = KGlobal::config()->group("state");
	{
		const int v = meow.readEntry<int>("volume", 50);
		d->player->setVolume(v);
	}
	{
		const std::string device = meow.readEntry<QString>("device", "").toUtf8().constData();
		d->player->setCurrentDevice(device);
	}
	{
		QString order = meow.readEntry<QString>("selector", "linear");
		int index;
		if (order == "randomartist")
			index = TreeView::RandomArtist;
		else if (order == "randomalbum")
			index = TreeView::RandomAlbum;
		else if (order == "randomsong")
			index = TreeView::RandomSong;
		else
			index = TreeView::Linear;
		d->playbackOrder->setCurrentItem(index);
		// why does setCurrentItem above not cause changePlaybackOrder slot to be called?
		changePlaybackOrder(index);
	}
	
	FileId first = dontPlayLastPlayed ? 0 : meow.readEntry<FileId>("lastPlayed", 0);
	
	loadCollection("collection", first);
	
	d->scrobble->begin();
	
	connect(qApp, SIGNAL(aboutToQuit()), SLOT(quitting()));
}

Meow::MainWindow::~MainWindow()
{
	quitting();
	delete d->collection;
	delete d;
}

void Meow::MainWindow::quitting()
{
	KConfigGroup meow = KGlobal::config()->group("state");
	meow.writeEntry<int>("volume", d->player->volume());
	meow.writeEntry<FileId>("lastPlayed", d->player->currentFile().fileId());

	if (d->playbackOrder->currentItem() == TreeView::RandomArtist)
		meow.writeEntry("selector", "randomartist");
	else if (d->playbackOrder->currentItem() == TreeView::RandomAlbum)
		meow.writeEntry("selector", "randomalbum");
	else if (d->playbackOrder->currentItem() == TreeView::RandomSong)
		meow.writeEntry("selector", "randomsong");
	else
		meow.writeEntry("selector", "linear");
	
	meow.sync();
}


void Meow::MainWindow::addFile(const KUrl &url)
{
	if (url.isLocalFile())
		d->collection->add( url.path(), false );
}

void Meow::MainWindow::addAndPlayFile(const KUrl &url)
{
	d->collection->add( url.path(), true );
}

void Meow::MainWindow::addFiles()
{
	if (d->openFileDialog)
	{
		d->openFileDialog->show();
		d->openFileDialog->raise();
		KWindowSystem::forceActiveWindow( d->openFileDialog->winId() );
		return;
	}
	d->openFileDialog = new KFileDialog(
			KUrl("kfiledialog:///mediadir"),
			d->player->mimeTypes().join(" "),
			this
		);
	d->openFileDialog->setOperationMode( KFileDialog::Opening );
	
	d->openFileDialog->setCaption(i18n("Add Files and Folders"));
	d->openFileDialog->setMode(KFile::Files | KFile::Directory | KFile::ExistingOnly | KFile::LocalOnly);
	
	connect(d->openFileDialog, SIGNAL(accepted()), SLOT(fileDialogAccepted()));
	connect(d->openFileDialog, SIGNAL(rejected()), SLOT(fileDialogClosed()));
	d->openFileDialog->show();
}

void Meow::MainWindow::fileDialogAccepted()
{
	if (!d->openFileDialog) return;

	KUrl::List files = d->openFileDialog->selectedUrls();
	fileDialogClosed();
	d->collection->startJob();
	for(KUrl::List::Iterator it=files.begin(); it!=files.end(); ++it)
		beginDirectoryAdd(*it);
	d->collection->scheduleFinishJob();
}

void Meow::MainWindow::fileDialogClosed()
{
	d->openFileDialog->deleteLater();
	d->openFileDialog = 0;
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

void Meow::MainWindow::dropEvent(QDropEvent *event)
{
	d->collection->startJob();
	KUrl::List files = KUrl::List::fromMimeData(event->mimeData());
	for(KUrl::List::Iterator it=files.begin(); it!=files.end(); ++it)
		beginDirectoryAdd(*it);
	d->collection->scheduleFinishJob();
}

void Meow::MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (KUrl::List::canDecode(event->mimeData()))
		event->acceptProposedAction();
}


void Meow::MainWindow::adderDone()
{
	d->collection->scheduleFinishJob();
	delete d->adder;
	d->adder = 0;
}

void Meow::MainWindow::beginDirectoryAdd(const KUrl &url)
{
	if (!d->adder)
	{
		d->collection->startJob();
		d->adder = new DirectoryAdder(this);
		connect(d->adder, SIGNAL(done()), SLOT(adderDone()));
		connect(d->adder, SIGNAL(addFile(KUrl)), SLOT(addFile(KUrl)));
	}
	d->adder->add(url);
}

void Meow::MainWindow::showItemContext(const QPoint &at)
{
	const char *menuName = "album_context";
	QList<QString> albums = d->view->selectedAlbums();
	bool isGrouping=false;
	if (!albums.isEmpty())
	{
		for (QList<QString>::const_iterator i = albums.begin(); i != albums.end(); ++i)
		{
			if (d->collection->groupByAlbum(*i))
			{
				isGrouping = true;
				break;
			}
		}
		
		actionCollection()->action("group_by_album")->setChecked(isGrouping);
		menuName = "album_context";
	}

	QList<File> f = d->view->selectedFiles();
	if (!f.isEmpty())
	{
		menuName = "item_context";
		d->openWith->menu()->clear();
		QSignalMapper *mapper=0;
		
		KMimeType::Ptr p = KMimeType::findByPath(f[0].file());
		KService::List apps = KMimeTypeTrader::self()->query(p->name());
		for (int i=0; i < apps.size(); i++)
		{
			KAction *const a = new KAction(KIcon(apps[i]->icon()), apps[i]->name(), d->openWith);
			if (!mapper)
			{
				mapper = new QSignalMapper(a);
				connect(mapper, SIGNAL(mapped(QString)), SLOT(openWith(QString)));
			}
			d->openWith->addAction(a);
			mapper->setMapping(a, apps[i]->entryPath());
			connect(a, SIGNAL(activated()), mapper, SLOT(map()));
		}
	}

	QMenu *const menu = static_cast<QMenu*>(factory()->container(menuName, this));
	menu->popup(at);
}

void Meow::MainWindow::changeCaption(const File &f)
{
	if (f)
		setCaption(f.title());
	else
		setCaption("");
}

void Meow::MainWindow::toggleToolBar()
{
	bool showing = actionCollection()->action(
			KStandardAction::name(KStandardAction::ShowToolbar)
		)->isChecked();
	if (toolBar("mainToolBar")->isVisible() == showing)
		return;
	toolBar("mainToolBar")->setVisible(showing);
	saveAutoSaveSettings();
}

void Meow::MainWindow::toggleMenuBar()
{
	QAction *const action = actionCollection()->action(
			KStandardAction::name(KStandardAction::ShowMenubar)
		);
	bool showing = action->isChecked();
	if (menuBar()->isVisible() == showing)
		return;
	menuBar()->setVisible(showing);

	if (!showing)
		KMessageBox::information(
				this,
				i18n(
						"If you want to show the menubar again, press %1", 
						action->shortcut().toString()
					),
				i18n("Hiding Menubar"),
				"hiding menu bar info"
			);
	saveAutoSaveSettings();
}

void Meow::MainWindow::configureShortcuts()
{
	KShortcutsDialog e;
	e.addCollection(actionCollection());
	e.configure();
}

void Meow::MainWindow::isPlaying(bool pl)
{
	if (pl)
	{
		d->playPauseAction->setIcon(KIcon("media-playback-pause"));
		d->playPauseAction->setText(i18n("Paws"));
		d->tray->setIcon(renderIcon("speaker", "media-playback-start"));
	}
	else
	{
		d->playPauseAction->setIcon(KIcon("media-playback-start"));
		d->playPauseAction->setText(i18n("Play"));
		d->tray->setIcon(renderIcon("speaker", "media-playback-pause"));
	}
}

void Meow::MainWindow::openWith(const QString &desktopEntryPath)
{
	KService svc(desktopEntryPath);
	QList<File> f = d->view->selectedFiles();
	KUrl::List urls;
	for (int i=0; i < f.size(); i++)
		urls += KUrl(f[i].file());
	KRun::run(svc, urls, this);
}

void Meow::MainWindow::systemTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::MiddleClick)
		d->player->playpause();
}

void Meow::MainWindow::changePlaybackOrder(int index)
{
	d->view->setSelector( static_cast<TreeView::SelectorType>(index) );
}

void Meow::MainWindow::groupByAlbum(bool x)
{
	QList<QString> albums = d->view->selectedAlbums();
	for (QList<QString>::const_iterator i = albums.begin(); i != albums.end(); ++i)
	{
		d->collection->setGroupByAlbum(*i, x);
	}
}

void Meow::MainWindow::itemProperties()
{
	QList<File> files = d->view->selectedFiles();
	if (!files.isEmpty())
		new FileProperties(files, d->collection, this);
}

QIcon Meow::MainWindow::renderIcon(const QString& baseIcon, const QString &overlayIcon) const
{
	QPixmap iconPixmap = KIcon(baseIcon).pixmap(KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium);
	if (!overlayIcon.isNull())
	{
		QPixmap overlayPixmap
			= KIcon(overlayIcon)
				.pixmap(KIconLoader::SizeSmallMedium/2, KIconLoader::SizeSmallMedium/2);
		QPainter p(&iconPixmap);
		p.drawPixmap(
				iconPixmap.width()-overlayPixmap.width(),
				iconPixmap.height()-overlayPixmap.height(),
				overlayPixmap
			);
		p.end();
	}
	return iconPixmap;
}


Meow::VolumeAction::VolumeAction(const KIcon& icon, const QString& text, QObject *parent)
	: KToolBarPopupAction(icon, text, parent)
{
	signalMapper = new QSignalMapper(this);
	
	slider = new SpecialSlider(0);
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

#include "mainwindow-qt.h"
#include "treeview.h"
#include "player.h"
#include "scrobble.h"

#include <db/file.h>
#include <db/base.h>
#include <db/collection.h>

#include <qpixmap.h>
#include <qicon.h>
#include <qevent.h>
#include <qmenu.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qslider.h>
#include <qsignalmapper.h>
#include <qtoolbutton.h>
#include <qapplication.h>
#include <qfiledialog.h>
#include <qsettings.h>
#include <qurl.h>
#include <qmessagebox.h>
#include <qabstracteventdispatcher.h>

#include <map>
#include <iostream>

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <winuser.h>
#endif

struct Meow::MainWindow::MainWindowPrivate
{
	inline MainWindowPrivate() : selectorActions(0) { }
	TreeView *view;
	Player *player;
	QSystemTrayIcon *tray;
	Base db;
	Collection *collection;
	DirectoryAdder *adder;
	
	QAction *itemProperties, *itemRemove;
	QMenu *playbackOrder;
	QToolBar *topToolbar;
	
	QAction *pauseAction, *prevAction, *nextAction, *volumeUpAction, *volumeDownAction, *muteAction;
	
	QAction *toggleToolbarAction, *toggleMenubarAction;
	
	bool nowFiltering, quitting;
	
	ConfigDialog *settingsDialog;
	Scrobble *scrobble;

	QFileDialog *openFileDialog;
	
	QMenu *contextMenu;
	QActionGroup selectorActions;
	std::map<QAction*, TreeView::SelectorType> selectors;
};

#ifdef _WIN32
static Meow::MainWindow *mwEvents=0;

bool Meow::MainWindow::globalEventFilter(void *_m)
{
	MSG *const m = (MSG*)_m;
	if (m->message == WM_HOTKEY)
	{
		const quint32 keycode = HIWORD(m->lParam);
		if (keycode == VK_MEDIA_PLAY_PAUSE || keycode == VK_MEDIA_STOP)
			mwEvents->d->pauseAction->trigger();
		else if (keycode == VK_MEDIA_NEXT_TRACK)
			mwEvents->d->nextAction->trigger();
		else if (keycode == VK_MEDIA_PREV_TRACK)
			mwEvents->d->prevAction->trigger();
		else if (keycode == VK_VOLUME_UP)
			mwEvents->d->volumeUpAction->trigger();
		else if (keycode == VK_VOLUME_DOWN)
			mwEvents->d->volumeDownAction->trigger();
		else if (keycode == VK_VOLUME_MUTE)
			mwEvents->d->muteAction->trigger();
	}
	return false;
}

#endif

Meow::MainWindow::MainWindow()
{
#ifdef _WIN32
	mwEvents = this;
	{
		RegisterHotKey(winId(), VK_MEDIA_PLAY_PAUSE, 0, VK_MEDIA_PLAY_PAUSE);
		RegisterHotKey(winId(), VK_MEDIA_STOP, 0, VK_MEDIA_STOP);
		RegisterHotKey(winId(), VK_MEDIA_NEXT_TRACK, 0, VK_MEDIA_NEXT_TRACK);
		RegisterHotKey(winId(), VK_MEDIA_PREV_TRACK, 0, VK_MEDIA_PREV_TRACK);
		RegisterHotKey(winId(), VK_VOLUME_UP, 0, VK_VOLUME_UP);
		RegisterHotKey(winId(), VK_VOLUME_DOWN, 0, VK_VOLUME_DOWN);
		RegisterHotKey(winId(), VK_VOLUME_MUTE, 0, VK_VOLUME_MUTE);
		QAbstractEventDispatcher::instance()->setEventFilter(globalEventFilter);
	}
#endif
	setWindowTitle(tr("Meow"));
	d = new MainWindowPrivate;
	d->adder = 0;
	d->nowFiltering = false;
	d->openFileDialog = 0;
	d->quitting=false;
	d->settingsDialog=0;
	
	d->db.open(QDir::homePath() + "\\meow collection");
	
	d->collection = new Collection(&d->db);

	d->player = new Player;
	d->view = new TreeView(this, d->player, d->collection);
	d->view->installEventFilter(this);

	d->scrobble = new Scrobble(this, d->player, d->collection);

	setCentralWidget(d->view);
	
	QMenu *const trayMenu = new QMenu(this);
	d->tray = new QSystemTrayIcon(QIcon(":/meow.png"), this);
	d->tray->setContextMenu(trayMenu);
	d->tray->installEventFilter(this);
	d->tray->show();
	connect(
			d->tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			SLOT(systemTrayClicked(QSystemTrayIcon::ActivationReason))
		);

	QMenuBar *mbar = menuBar();
	
	QMenu *fileMenu = mbar->addMenu(tr("File"));
	QMenu *settingsMenu = mbar->addMenu(tr("Settings"));
	QMenu *helpMenu = mbar->addMenu(tr("Help"));
	
	QToolBar *topToolbar = d->topToolbar = addToolBar(tr("Main"));
		
	{
		QAction *ac;
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(addFiles()));
		ac->setText(tr("Add &Files..."));
		ac->setIcon(QIcon(":/list-add.png"));
		topToolbar->addAction(ac);
		fileMenu->addAction(ac);
		
		d->pauseAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->player, SLOT(playpause()));
		ac->setText(tr("Paws"));
		ac->setIcon(QIcon(":/media-playback-pause.png"));
		topToolbar->addAction(ac);
		trayMenu->addAction(ac);
		
		d->prevAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->view, SLOT(previousSong()));
		ac->setText(tr("Previous Song"));
		ac->setIcon(QIcon(":/media-skip-backward.png"));
		topToolbar->addAction(ac);
		trayMenu->addAction(ac);

		d->nextAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->view, SLOT(nextSong()));
		ac->setText(tr("Next Song"));
		ac->setIcon(QIcon(":/media-skip-forward.png"));
		topToolbar->addAction(ac);
		
		trayMenu->addAction(d->nextAction);
		trayMenu->addAction(d->prevAction);

		{
			struct Selector
			{
				QString name;
				TreeView::SelectorType selectorType;
			};
			
			const Selector selectors[] = 
			{
				{ tr("Each File"), TreeView::Linear },
				{ tr("Random Song"), TreeView::RandomSong },
				{ tr("Random Album"), TreeView::RandomAlbum },
				{ tr("Random Artist"), TreeView::RandomArtist }
			};
			
			d->playbackOrder = fileMenu->addMenu(tr("Playback Order"));
			
			for (int i=0; i < sizeof(selectors)/sizeof(Selector); i++)
			{
				ac = d->playbackOrder->addAction(selectors[i].name);
				d->selectors[ac] = selectors[i].selectorType;
				d->selectorActions.addAction(ac);
			}
		
			connect(&d->selectorActions, SIGNAL(triggered(QAction*)), SLOT(selectorActivated(QAction*)));
		}
		
		fileMenu->addSeparator();
		trayMenu->addSeparator();
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), this, SLOT(quitting()));
		ac->setText(tr("&Quit"));
		trayMenu->addAction(ac);
		fileMenu->addAction(ac);
		
		
#ifndef Q_WS_MAC
		ac = d->toggleMenubarAction = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(toggleMenuBar()));
		ac->setShortcut(QKeySequence("Ctrl+M"));
		ac->setText(tr("Show &Menubar"));
		settingsMenu->addAction(ac);
#endif
		
		ac = d->toggleToolbarAction = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(toggleToolBar()));
		ac->setText(tr("Show &Toolbar..."));
		settingsMenu->addAction(ac);
		
		settingsMenu->addSeparator();
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(showSettings()));
		ac->setText(tr("&Configure Meow..."));
		settingsMenu->addAction(ac);

		d->volumeUpAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->player, SLOT(volumeUp()));
		ac->setText(tr("&Volume Up"));
		
		d->volumeDownAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->player, SLOT(volumeDown()));
		ac->setText(tr("&Volume Down"));
		
		d->muteAction = ac = new QAction(this);
//		connect(ac, SIGNAL(triggered()), d->player, SLOT(volumeMute()));
		ac->setText(tr("&Mute"));
	}
	
	{
		QAction *ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(showAbout()));
		ac->setText(tr("&About Meow..."));
		helpMenu->addAction(ac);
	}
	
	{ // context menu
		d->contextMenu = new QMenu(this);
	
		QAction *ac = d->contextMenu->addAction(
				tr("&Remove from playlist"), d->view, SLOT(removeSelected())
			);
		ac->setShortcut(Qt::Key_Delete);
		
		d->itemProperties = d->contextMenu->addAction(
				tr("&Properties"), this, SLOT(itemProperties())
			);
		d->itemProperties->setText(tr("&Properties"));
	}
	
	connect(d->view, SIGNAL(kdeContextMenu(QPoint)), SLOT(showItemContext(QPoint)));
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(changeCaption(File)));
	
	setAcceptDrops(true);
	
	d->toggleToolbarAction->setChecked(topToolbar->isVisibleTo(this));
	d->toggleMenubarAction->setChecked(menuBar()->isVisibleTo(this));
		
	QSettings settings;
	d->player->setVolume(settings.value("state/volume", 50).toInt());
	
	{
		QString order = settings.value("state/selector", "linear").toString();
		int index;
		if (order == "randomartist")
			index = TreeView::RandomArtist;
		else if (order == "randomalbum")
			index = TreeView::RandomAlbum;
		else if (order == "randomsong")
			index = TreeView::RandomSong;
		else
			index = TreeView::Linear;
		for (
				std::map<QAction*, TreeView::SelectorType>::iterator i = d->selectors.begin();
				i != d->selectors.end(); ++i
			)
		{
			if (d->selectors[i->first] == index)
			{
				i->first->setChecked(true);
				selectorActivated(i->first);
				break;
			}
		}
	}
	
	FileId first = settings.value("state/lastPlayed", 0).toInt();
	
	d->collection->getFilesAndFirst(first);
	if (first)
	{
		// clever trick here:
		// if first is valid, that means that getFilesAndFirst has loaded it first
		// and it did so right now (not later in the event loop)
		// furthermore, it will also load the rest of the files later on 
		// in the event loop, which means that right now, first is the only
		// item in the list
		d->view->playFirst();
	}
	
}

Meow::MainWindow::~MainWindow()
{
//	delete d->collection;
//	delete d;
}

void Meow::MainWindow::addFile(const QUrl &url)
{
	d->collection->add( url.toLocalFile() );
}

void Meow::MainWindow::addFiles()
{
	if (d->openFileDialog)
	{
		d->openFileDialog->show();
		d->openFileDialog->raise();
		return;
	}
	d->openFileDialog = new QFileDialog(
			this,
			tr("Add Files"),
			QString()
		);
	
	d->openFileDialog->setFileMode(QFileDialog::ExistingFiles);
	
	connect(d->openFileDialog, SIGNAL(accepted()), SLOT(fileDialogAccepted()));
	connect(d->openFileDialog, SIGNAL(rejected()), SLOT(fileDialogClosed()));
	d->openFileDialog->show();
}

void Meow::MainWindow::addDirs()
{
	if (d->openFileDialog)
	{
		d->openFileDialog->show();
		d->openFileDialog->raise();
		return;
	}
	d->openFileDialog = new QFileDialog(
			this,
			tr("Add Folder"),
			QString(),
			d->player->mimeTypes().join(";")
		);
	
	d->openFileDialog->setFileMode(QFileDialog::DirectoryOnly);
	
	connect(d->openFileDialog, SIGNAL(accepted()), SLOT(fileDialogAccepted()));
	connect(d->openFileDialog, SIGNAL(rejected()), SLOT(fileDialogClosed()));
	d->openFileDialog->show();
}

void Meow::MainWindow::fileDialogAccepted()
{
	if (!d->openFileDialog) return;

	QStringList files = d->openFileDialog->selectedFiles();
	fileDialogClosed();
	
	for(QStringList::Iterator it=files.begin(); it!=files.end(); ++it)
		beginDirectoryAdd(*it);
}

void Meow::MainWindow::fileDialogClosed()
{
	d->openFileDialog->deleteLater();
	d->openFileDialog = 0;
}

void Meow::MainWindow::toggleVisible()
{
	isVisible() ? hide() : show();
}
	
void Meow::MainWindow::closeEvent(QCloseEvent *event)
{
	if (!d->quitting)
	{
		toggleVisible();
		event->ignore();
		return;
	}
	QSettings settings;
	settings.setValue("state/volume", d->player->volume());
	settings.setValue("state/lastPlayed", d->player->currentFile().fileId());

	TreeView::SelectorType selector = d->selectors[d->selectorActions.checkedAction()];
	if (selector == TreeView::RandomArtist)
		settings.setValue("state/selector", "randomartist");
	else if (selector == TreeView::RandomAlbum)
		settings.setValue("state/selector", "randomalbum");
	else if (selector == TreeView::RandomSong)
		settings.setValue("state/selector", "randomsong");
	else
		settings.setValue("state/selector", "linear");
	QMainWindow::closeEvent(event);
}

void Meow::MainWindow::quitting()
{
	d->quitting=true;
	close();
}

void Meow::MainWindow::wheelEvent(QWheelEvent *event)
{
	if (!d->nowFiltering)
		d->player->setVolume(d->player->volume() + event->delta()*10/120);
}

void Meow::MainWindow::dropEvent(QDropEvent *event)
{
	QList<QUrl> files = event->mimeData()->urls();
	for(QList<QUrl>::Iterator it=files.begin(); it!=files.end(); ++it)
		beginDirectoryAdd(it->toLocalFile());
}

void Meow::MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/uri-list"))
		event->acceptProposedAction();
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
//	delete d->adder;
//	d->adder = 0;
}

void Meow::MainWindow::beginDirectoryAdd(const QString &file)
{
	if (QFileInfo(file).isFile())
	{
		addFile(QUrl::fromLocalFile(file));
		return;
	}	
	
/*	if (!d->adder)
	{
		d->adder = new DirectoryAdder(this);
		connect(d->adder, SIGNAL(done()), SLOT(adderDone()));
		connect(d->adder, SIGNAL(addFile(QUrl)), SLOT(addFile(QUrl)));
	}
	d->adder->add(file);
*/}

void Meow::MainWindow::showItemContext(const QPoint &at)
{
	d->contextMenu->popup(at);
}

void Meow::MainWindow::changeCaption(const File &f)
{
	setWindowTitle(tr("%1 - Meow").arg(f.title()));
}

void Meow::MainWindow::showSettings()
{
	if (!d->settingsDialog)
	{
		d->settingsDialog = new ConfigDialog(this);
		ScrobbleConfigure *sc=new ScrobbleConfigure(d->settingsDialog, d->scrobble);
		d->settingsDialog->addPage(sc, tr("AudioScrobbler"));
	}
	
	d->settingsDialog->show();
}

void Meow::MainWindow::showAbout()
{
	QMessageBox::about(
			this, tr("About Meow"),
			tr(
					"<qt>This is Meow 1.0. The cutest music player ever.<br/><br/>"
					"By <a href=\"mailto:charles@kde.org\">Charles Samuels</a>. He likes cats.<br/><br/>"
					"<a href=\"http://derkarl.org/meow\">http://derkarl.org/meow</a><br/><br/>"
					"Copyright (c) 2008-2011 Charles Samuels<br/>"
					"Copyright (c) 2004-2006 Allen Sandfeld Jensen (Akode backend)<br/>"
					"Copyright (c) 2000-2007 Josh Coalson (FLAC decoder)<br/>"
					"Copyright (c) 1994-2010 the Xiph.Org Foundation (Vorbis decoder)<br/>"
					"Copyright (c) 1999-2002 Buschmann/Klemm/Piecha/Wolf (Musepack decoder)<br/>"
					"Copyright (c) 2003-2004 Peter Pawlowski (Musepack decoder)</qt>"
					"Copyright (c) 2001 Ross P. Johnson (Posix threads library for Windows)</qt>"
				)
		);
}

void Meow::MainWindow::toggleToolBar()
{
	d->topToolbar->setVisible(d->toggleToolbarAction->isChecked());
}

void Meow::MainWindow::toggleMenuBar()
{
	const bool showing = d->toggleMenubarAction->isChecked();
	menuBar()->setVisible(showing);

	if (!showing)
		QMessageBox::information(
				this,
				tr("Hiding Menubar"),
				tr("If you want to show the menubar again, press %1").arg 
					(d->toggleMenubarAction->shortcut().toString(QKeySequence::NativeText))
			);
}



void Meow::MainWindow::systemTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::MiddleClick)
		d->player->playpause();
	else if (reason == QSystemTrayIcon::Trigger)
		isVisible() ? hide() : show();
}

void Meow::MainWindow::itemProperties()
{
	QList<File> files = d->view->selectedFiles();
	if (!files.isEmpty())
		;
}

void Meow::MainWindow::selectorActivated(QAction* action)
{
	d->view->setSelector( d->selectors[action] );
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


// kate: space-indent off; replace-tabs off;

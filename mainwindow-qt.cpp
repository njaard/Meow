#include "mainwindow-qt.h"
#include "treeview.h"
#include "player.h"
#include "scrobble.h"
#include "directoryadder.h"
#include "filter.h"
#include "shortcut.h"

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
#include <qpainter.h>
#include <qboxlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>

#include <map>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <winuser.h>
#endif


#define i18n tr
class SpecialSlider;

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
	
	QAction *collectionsAction;
	QActionGroup *collectionsActionGroup;
	
	QAction *playPauseAction, *prevAction, *nextAction, *volumeUpAction, *volumeDownAction, *volumeAction;
	
	QAction *toggleToolbarAction, *toggleMenubarAction, *shortcutConfigAction;
	
	bool nowFiltering, quitting;
	
	ConfigDialog *settingsDialog;
	Scrobble *scrobble;

	QFileDialog *openFileDialog;
	
	QMenu *contextMenu;
	QActionGroup selectorActions;
	std::map<QAction*, TreeView::SelectorType> selectors;
	
	SpecialSlider *volumeSlider;
	Filter *filter;

	std::map<QString, Shortcut*> shortcuts;
};

typedef QIcon KIcon;

// open an icon, either get it from the "home directory" or
// look inside meow if the file is not available
static QIcon iconByName(const QString &name)
{
#if defined(_WIN32)
	const QString iconpath = QDir::homePath() + "\\meowplayer.org\\";
#else
	const QString iconpath = QDir::homePath() + "/.config/meowplayer.org/";
#endif

	if (QFile::exists(iconpath + name))
		return QIcon(iconpath + name);
	else
		return QIcon(":/" + name);
}




#include "mainwindow_common.cpp"


Meow::MainWindow::MainWindow()
{
	setWindowTitle(tr("Meow"));
	d = new MainWindowPrivate;
	d->adder = 0;
	d->nowFiltering = false;
	d->openFileDialog = 0;
	d->quitting=false;
	d->settingsDialog=0;

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
	
	QMenu *const trayMenu = new QMenu(this);
	d->tray = new QSystemTrayIcon(iconByName("meow.png"), this);
	d->tray->setContextMenu(trayMenu);
	d->tray->installEventFilter(this);
	d->tray->show();
	connect(
			d->tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			SLOT(systemTrayClicked(QSystemTrayIcon::ActivationReason))
		);

	QMenuBar *mbar = menuBar();
	
	QMenu *fileMenu = mbar->addMenu(tr("&File"));
	QMenu *settingsMenu = mbar->addMenu(tr("&Settings"));
	QMenu *helpMenu = mbar->addMenu(tr("&Help"));
	
	QToolBar *topToolbar = d->topToolbar = addToolBar(tr("Main"));
		
	{
		QAction *ac;
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(addFiles()));
		ac->setText(tr("Add &Files..."));
		ac->setIcon(iconByName("list-add.png"));
		topToolbar->addAction(ac);
		fileMenu->addAction(ac);
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->filter, SLOT(show()));
		ac->setText(tr("&Find"));
		{
			QList<QKeySequence> shortcuts;
			shortcuts.append(QKeySequence("/"));
			shortcuts.append(QKeySequence("Ctrl+F"));
			ac->setShortcuts(shortcuts);
		}
		fileMenu->addAction(ac);

		d->prevAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->view, SLOT(previousSong()));
		ac->setText(tr("Previous Song"));
		ac->setIcon(iconByName("media-skip-backward.png"));
		topToolbar->addAction(ac);
		trayMenu->addAction(ac);
		
		d->playPauseAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->player, SLOT(playpause()));
		ac->setText(tr("Paws"));
		ac->setIcon(iconByName("media-playback-pause.png"));
		topToolbar->addAction(ac);
		trayMenu->addAction(ac);
		
		d->nextAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->view, SLOT(nextSong()));
		ac->setText(tr("Next Song"));
		ac->setIcon(iconByName("media-skip-forward.png"));
		topToolbar->addAction(ac);
		
		d->volumeAction = ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), this, SLOT(showVolume()));
		ac->setText(tr("Volume"));
		ac->setIcon(iconByName("player-volume.png"));
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
		
		{
			d->collectionsActionGroup = new QActionGroup(this);
			
			d->collectionsAction = new QAction(i18n("&Collection"), this);
			d->collectionsAction->setMenu(new QMenu(this));
			QAction *newCol = new QAction(i18n("&New Collection"), this);
			connect(newCol, SIGNAL(activated()), this, SLOT(newCollection()));
			QAction *copyCol = new QAction(i18n("&Copy Collection"), this);
			connect(copyCol, SIGNAL(activated()), this, SLOT(copyCollection()));
			QAction *renameCol = new QAction(i18n("&Rename Collection"), this);
			connect(renameCol, SIGNAL(activated()), this, SLOT(renameCollection()));
			QAction *delCol = new QAction(i18n("&Delete Collection"), this);
			connect(delCol, SIGNAL(activated()), this, SLOT(deleteCollection()));
			d->collectionsAction->menu()->addAction(newCol);
			d->collectionsAction->menu()->addAction(copyCol);
			d->collectionsAction->menu()->addAction(renameCol);
			d->collectionsAction->menu()->addAction(delCol);
			d->collectionsAction->menu()->addSeparator();
			fileMenu->addAction(d->collectionsAction);
			reloadCollections();
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
		connect(ac, SIGNAL(toggled(bool)), SLOT(toggleMenuBar()));
		ac->setShortcut(QKeySequence("Ctrl+M"));
		ac->setText(tr("Show &Menubar"));
		ac->setCheckable(true);
		settingsMenu->addAction(ac);
		addAction(ac);
#endif
		
		ac = d->shortcutConfigAction = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(showShortcutSettings()));
		ac->setText(tr("Configure &Shortcuts"));
		settingsMenu->addAction(ac);

		
		ac = d->toggleToolbarAction = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(toggleToolBar()));
		ac->setText(tr("Show &Toolbar"));
		ac->setCheckable(true);
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
	connect(d->player, SIGNAL(playing(bool)), SLOT(isPlaying(bool)));

	d->volumeSlider = new SpecialSlider(this);
	connect(d->volumeSlider, SIGNAL(sliderMoved(int)), d->player, SLOT(setVolume(int)));
	connect(d->player, SIGNAL(volumeChanged(int)), d->volumeSlider, SLOT(setValue(int)));

	setAcceptDrops(true);
	
	d->toggleToolbarAction->setChecked(topToolbar->isVisibleTo(this));
	d->toggleMenubarAction->setChecked(menuBar()->isVisibleTo(this));

	QSettings settings;
	{
		const int v = settings.value("state/volume", 50).toInt();
		d->player->setVolume(v);
		d->volumeSlider->setValue(v);
	}
	{
		const std::string device = settings.value("state/device", "").toString().toUtf8().constData();
		d->player->setCurrentDevice(device);
	}
	
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

	registerShortcuts();
	
	FileId first = settings.value("state/lastPlayed", 0).toInt();
	
	loadCollection("collection", first);
	
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
			QString()
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
	
	d->collection->startJob();
	for(QStringList::Iterator it=files.begin(); it!=files.end(); ++it)
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
		
	delete d->tray;
	QMainWindow::closeEvent(event);
}

void Meow::MainWindow::quitting()
{
	d->quitting=true;
	close();
}


void Meow::MainWindow::showVolume()
{
	QPoint at = d->topToolbar->mapToGlobal(d->topToolbar->widgetForAction(d->volumeAction)->frameGeometry().bottomLeft());
	d->volumeSlider->move(at);
	d->volumeSlider->show();
}


void Meow::MainWindow::dropEvent(QDropEvent *event)
{
	d->collection->startJob();
	QList<QUrl> files = event->mimeData()->urls();
	for(QList<QUrl>::Iterator it=files.begin(); it!=files.end(); ++it)
		beginDirectoryAdd(it->toLocalFile());
	d->collection->scheduleFinishJob();
}

void Meow::MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/uri-list"))
		event->acceptProposedAction();
}


void Meow::MainWindow::adderDone()
{
	d->collection->scheduleFinishJob();
	delete d->adder;
	d->adder = 0;
}

void Meow::MainWindow::beginDirectoryAdd(const QString &file)
{
	if (QFileInfo(file).isFile())
	{
		addFile(QUrl::fromLocalFile(file));
		return;
	}
	
	if (!d->adder)
	{
		d->collection->startJob();
		d->adder = new DirectoryAdder(this);
		connect(d->adder, SIGNAL(done()), SLOT(adderDone()));
		connect(d->adder, SIGNAL(addFile(QUrl)), SLOT(addFile(QUrl)));
	}
	d->adder->add(QUrl::fromLocalFile(file));
}

void Meow::MainWindow::showItemContext(const QPoint &at)
{
	d->contextMenu->popup(at);
}

void Meow::MainWindow::changeCaption(const File &f)
{
	if (f)
		setWindowTitle(tr("%1 - Meow").arg(f.title()));
	else
		setWindowTitle(tr("Meow"));
}


Meow::ShortcutDialog::ShortcutDialog(std::map<QString, Shortcut*>& shortcuts, QWidget *w)
	: QDialog(w)
{
	setWindowTitle(tr("Meow Shortcuts"));
	QGridLayout *l = new QGridLayout(this);
	QLabel *b;
	ShortcutInput *s;
	int row=0;
	
	b = new QLabel(tr("Play/Pause"), this);
	s = new ShortcutInput(shortcuts["playpause"],this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["playpause"] = s;
	row++;
	
	b = new QLabel(tr("Next Track"), this);
	s = new ShortcutInput(shortcuts["next"], this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["next"] = s;
	row++;
	
	b = new QLabel(tr("Previous Track"), this);
	s = new ShortcutInput(shortcuts["previous"],this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["previous"] = s;
	row++;
	
	b = new QLabel(tr("Increase Volume"), this);
	s = new ShortcutInput(shortcuts["volume_up"],this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["volume_up"] = s;
	row++;
	
	b = new QLabel(tr("Decrease Volume"), this);
	s = new ShortcutInput(shortcuts["volume_down"], this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["volume_down"] = s;
	row++;
	
	b = new QLabel(tr("Seek Forward"), this);
	s = new ShortcutInput(shortcuts["seekforward"], this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["seekforward"] = s;
	row++;
	
	b = new QLabel(tr("Seek Backward"), this);
	s = new ShortcutInput(shortcuts["seekbackward"], this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["seekbackward"] = s;
	row++;
	
	b = new QLabel(tr("Show/Hide main window"), this);
	s = new ShortcutInput(shortcuts["togglegui"], this);
	l->addWidget(b, row, 0);
	l->addWidget(s, row, 1);
	inputs["togglegui"] = s;
	row++;
	
	
	QPushButton *ok = new QPushButton("OK", this);
	ok->setDefault(true);
	connect(ok, SIGNAL(clicked()), SLOT(accept()));
	
	QPushButton *cancel = new QPushButton("Cancel", this);
	connect(cancel, SIGNAL(clicked()), SLOT(reject()));
	
	QHBoxLayout *oklayout=new QHBoxLayout;
	l->addLayout(oklayout, row, 0, 1, -1);
	oklayout->addStretch();
	oklayout->addWidget(ok);
	oklayout->addWidget(cancel);
}


void Meow::MainWindow::showShortcutSettings()
{
	ShortcutDialog dlg(d->shortcuts, this);
	if (!dlg.exec())
		return;
	
	QSettings settings;
	for (std::map<QString, ShortcutInput*>::iterator i = dlg.inputs.begin(); i != dlg.inputs.end(); ++i)
	{
		settings.setValue("shortcuts/" + i->first, i->second->key().toString(QKeySequence::PortableText));
		d->shortcuts[i->first]->setKey(i->second->key());
	}
}

void Meow::MainWindow::showAbout()
{
	QDialog msgBox(this);
	msgBox.setWindowTitle(tr("About Meow"));
	
	QLabel *label = new QLabel(
			tr(
				"This is Meow %1. A cute music player.<br/><br/>"
				"By <a href='mailto:charles@meowplayer.org'>Charles Samuels</a>. He likes cats.<br/><br/>"
				"<a href='http://meowplayer.org'>http://meowplayer.org/</a><br/><br/>"
				"Bug reports to <a href='mailto:bugs@meowplayer.org'>bugs@meowplayer.org</a><br/><br/>"
				"Copyright (c) 2008-2013 Charles Samuels<br/>"
				"Copyright (c) 2004-2006 Allen Sandfeld Jensen (Most of Akode backend)<br/>"
				"Copyright (c) 2000-2007 Stefan Gehn, Charles Samuels (Portions of playback controller)<br/>"
				"Copyright (c) 2000-2007 Josh Coalson (FLAC decoder)<br/>"
				"Copyright (c) 1994-2010 the Xiph.Org Foundation (Vorbis decoder)<br/>"
#ifdef AKODE_WITH_OPUS
				"Copyright (c) 1994-2013 the Xiph.Org Foundation and contributors (Opus decoder)<br/>"
#endif
				"Copyright (c) ?-2013 Michael Hipp and the mpg123 project (mp3 decoder)<br/>"
				"Copyright (c) 2005 The Musepack Development Team (Musepack decoder)<br/>"
#ifdef _WIN32
				"Copyright (c) 2001 Ross P. Johnson (Posix threads library for Windows)<br/>"
#endif
				"Copyright (c) 2007-2009 Oxygen project (Icons)<br/>"
				"The Public Domain's SQLite<br/><br/>"
				"Meow is Free software, you may modify and share it under the "
				"<a href=\"http://www.gnu.org/licenses/gpl-3.0.html\">terms of the GPL version 3</a>."
			).arg(MEOW_VERSION),
			&msgBox
		);
	label->setOpenExternalLinks(true);
	label->setTextFormat(Qt::RichText);
	
	QPushButton *ok = new QPushButton("OK", &msgBox);
	ok->setDefault(true);
	connect(ok, SIGNAL(clicked()), &msgBox, SLOT(accept()));
	
	QVBoxLayout *layout = new QVBoxLayout(&msgBox);
	layout->addWidget(label);
	
	QHBoxLayout *oklayout=new QHBoxLayout;
	layout->addLayout(oklayout);
	oklayout->addStretch();
	oklayout->addWidget(ok);

	msgBox.setLayout(layout);
	msgBox.exec();
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

void Meow::MainWindow::isPlaying(bool pl)
{
	if (pl)
	{
		d->playPauseAction->setIcon(iconByName("media-playback-pause.png"));
		d->playPauseAction->setText(tr("Paws"));
		d->tray->setIcon(renderIcon(iconByName("meow.png"), iconByName("media-playback-start.png")));
	}
	else
	{
		d->playPauseAction->setIcon(iconByName("media-playback-start"));
		d->playPauseAction->setText(tr("Play"));
		d->tray->setIcon(renderIcon(iconByName("meow.png"), iconByName("media-playback-pause.png")));
	}
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
	{
#ifdef _WIN32
		SHELLEXECUTEINFOW in = {0};
		in.cbSize = sizeof(in);
		in.fMask = SEE_MASK_INVOKEIDLIST;
		in.hwnd = effectiveWinId();
		in.lpVerb = L"properties";
		std::cerr << "file: " << files[0].file().toUtf8().constData() << std::endl;
		in.lpFile = (const WCHAR*)files[0].file().utf16();
		ShellExecuteExW(&in);
#endif
	}
}

void Meow::MainWindow::selectorActivated(QAction* action)
{
	d->view->setSelector( d->selectors[action] );
}

QIcon Meow::MainWindow::renderIcon(const QIcon& baseIcon, const QIcon &overlayIcon) const
{
	QPixmap iconPixmap = baseIcon.pixmap(16);
	if (!overlayIcon.isNull())
	{
		QPixmap overlayPixmap
			= overlayIcon.pixmap(22/2);
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

void Meow::MainWindow::registerShortcuts()
{
	while (!d->shortcuts.empty())
	{
		delete d->shortcuts.begin()->second;
		d->shortcuts.erase(d->shortcuts.begin());
	}
	QSettings settings;
	QString x;
	Shortcut *s;
	
	x=settings.value("shortcuts/playpause", "Media Play").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->player, SLOT(playpause()));
	d->shortcuts["playpause"] = s;
	
	x=settings.value("shortcuts/next", "Media Next").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->view, SLOT(nextSong()));
	d->shortcuts["next"] = s;
	
	x=settings.value("shortcuts/previous", "Media Previous").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->view, SLOT(previousSong()));
	d->shortcuts["previous"] = s;
	
	x=settings.value("shortcuts/volume_up", "Ctrl+Alt+Shift+Up").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->player, SLOT(volumeUp()));
	d->shortcuts["volume_up"] = s;
	
	x=settings.value("shortcuts/volume_down", "Ctrl+Alt+Shift+Down").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->player, SLOT(volumeDown()));
	d->shortcuts["volume_down"] = s;
	
	x=settings.value("shortcuts/seekforward", "Ctrl+Alt+Shift+Left").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->player, SLOT(seekForward()));
	d->shortcuts["seekforward"] = s;
	
	x=settings.value("shortcuts/seekbackward", "Ctrl+Alt+Shift+Right").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), d->player, SLOT(seekBackward()));
	d->shortcuts["seekbackward"] = s;
	
	x=settings.value("shortcuts/togglegui", "Ctrl+Alt+M").toString();
	s = new Shortcut(QKeySequence::fromString(x, QKeySequence::PortableText), this);
	connect(s, SIGNAL(activated()), this, SLOT(toggleVisible()));
	d->shortcuts["togglegui"] = s;
}



// kate: space-indent off; replace-tabs off;

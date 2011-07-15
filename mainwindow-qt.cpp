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

#include <map>

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
	
	QAction *toggleToolbarAction, *toggleMenubarAction;
	
	bool nowFiltering;
	
	ConfigDialog *settingsDialog;
	Scrobble *scrobble;

	QFileDialog *openFileDialog;
	
	QMenu *contextMenu;
	QActionGroup selectorActions;
	std::map<QAction*, TreeView::SelectorType> selectors;
};

Meow::MainWindow::MainWindow()
{
	setWindowTitle(tr("Meow"));
	d = new MainWindowPrivate;
	d->adder = 0;
	d->nowFiltering = false;
	d->openFileDialog = 0;
	
	d->db.open(QDir::homePath() + "\\meow collection");
	
	d->collection = new Collection(&d->db);

	d->player = new Player;
	d->view = new TreeView(this, d->player, d->collection);
	d->view->installEventFilter(this);

	d->scrobble = new Scrobble(this, d->player, d->collection);

	setCentralWidget(d->view);
	
	d->tray = new QSystemTrayIcon(this);
	d->tray->setContextMenu(new QMenu(this));
	d->tray->installEventFilter(this);
	d->tray->show();
	connect(
			d->tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			SLOT(systemTrayClicked(QSystemTrayIcon::ActivationReason))
		);
	
	QMenu *const trayMenu = d->tray->contextMenu();
	
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
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->player, SLOT(playpause()));
		ac->setText(tr("Paws"));
		ac->setIcon(QIcon(":/media-playback-pause.png"));
		trayMenu->addAction(ac);
		topToolbar->addAction(ac);
		
		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->view, SLOT(previousSong()));
		ac->setText(tr("Previous Song"));
		ac->setIcon(QIcon(":/media-skip-backward.png"));
		trayMenu->addAction(ac);
		topToolbar->addAction(ac);

		ac = new QAction(this);
		connect(ac, SIGNAL(triggered()), d->view, SLOT(nextSong()));
		ac->setText(tr("Next Song"));
		ac->setIcon(QIcon(":/media-skip-forward.png"));
		trayMenu->addAction(ac);
		topToolbar->addAction(ac);
		
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
		connect(ac, SIGNAL(triggered()), SLOT(deleteLater()));
		ac->setText(tr("&Quit"));
		trayMenu->addAction(ac);
		fileMenu->addAction(ac);
		
		
#ifndef Q_WS_MAC
		ac = d->toggleMenubarAction = new QAction(this);
		connect(ac, SIGNAL(triggered()), SLOT(toggleMenuBar()));
		ac->setShortcut(Qt::Key_Control + Qt::Key_M);
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
	
	delete d->collection;
	delete d;
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
	toggleVisible();
	event->ignore();
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

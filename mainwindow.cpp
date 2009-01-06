#include "mainwindow.h"
#include "treeview.h"
#include "player.h"
#include "directoryadder.h"

#include <db/file.h>
#include <db/base.h>
#include <db/collection.h>

#include <kaction.h>
#include <klocale.h>
#include <kactioncollection.h>
#include <kfiledialog.h>
#include <ksystemtrayicon.h>
#include <kstandarddirs.h>
#include <kiconeffect.h>
#include <kxmlguifactory.h>

#include <qpixmap.h>
#include <qicon.h>
#include <qevent.h>
#include <qmenu.h>

struct KittenPlayer::MainWindow::MainWindowPrivate
{
	TreeView *view;
	Player *player;
	KSystemTrayIcon *tray;
	Base db;
	Collection *collection;
	DirectoryAdder *adder;
	
	KAction *itemProperties, *itemRemove;
};

KittenPlayer::MainWindow::MainWindow()
{
	d = new MainWindowPrivate;
	d->adder = 0;
	
	d->db.open(KStandardDirs::locate("data", "kittenplayer/")+"collection");
	
	d->collection = new Collection(&d->db);

	d->player = new Player;
	d->player->setVolume(50);
	d->view = new TreeView(this, d->player, d->collection);
	setCentralWidget(d->view);
	
	d->tray = new KSystemTrayIcon("speaker", this);
	d->tray->show();
	
	{ // file menu
		KAction *ac;
		ac = actionCollection()->addAction("add_files");
		ac->setText(i18n("Add &Files..."));
		ac->setIcon(KIcon("list-add"));
		connect(ac, SIGNAL(triggered()), SLOT(addFiles()));
		
		ac = actionCollection()->addAction("add_dir");
		ac->setText(i18n("Add Fol&ders..."));
		ac->setIcon(KIcon("folder"));
		connect(ac, SIGNAL(triggered()), SLOT(addDirectory()));
		
		ac = actionCollection()->addAction(
				KStandardAction::Close,
				this,
				SLOT(deleteLater())
			);
	}
	
	{ // context menu
		d->itemProperties = actionCollection()->addAction("remove_item");
		d->itemProperties->setText(i18n("&Remove from playlist"));
		d->itemProperties->setIcon(KIcon("edit-delete"));
		d->itemProperties->setShortcut(Qt::Key_Delete);
		connect(d->itemProperties, SIGNAL(triggered()), d->view, SLOT(removeSelected()));
		
		d->itemProperties = actionCollection()->addAction("item_properties");
		d->itemProperties->setText(i18n("&Properties"));
		connect(d->itemProperties, SIGNAL(triggered()), SLOT(itemProperties()));
	}
	
	connect(d->collection, SIGNAL(added(File)), d->view, SLOT(addFile(File)));
	connect(d->view, SIGNAL(kdeContextMenu(QPoint)), SLOT(showItemContext(QPoint)));
	
	d->collection->getFiles();
	
	createGUI();
}

KittenPlayer::MainWindow::~MainWindow()
{
	delete d->collection;
	delete d;
}

void KittenPlayer::MainWindow::addFile(const KUrl &url)
{
	if (url.isLocalFile())
		d->collection->add( url.path() );
}

void KittenPlayer::MainWindow::addFiles()
{
	KUrl::List files = KFileDialog::getOpenUrls(
			KUrl("kfiledialog:///mediadir"), d->player->mimeTypes().join(" "),
			this, i18n("Select Files to Add")
		);

	for(KUrl::List::Iterator it=files.begin(); it!=files.end(); ++it)
		addFile(*it);
}

void KittenPlayer::MainWindow::addDirectory()
{
	QString folder = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///mediadir"), this,
		i18n("Select Folder to Add"));
	
	if (folder.isEmpty())
		return;

	KUrl url;
	url.setPath(folder);
	beginDirectoryAdd(url);
}

void KittenPlayer::MainWindow::closeEvent(QCloseEvent *event)
{
	d->tray->toggleActive();
	event->ignore();
}

void KittenPlayer::MainWindow::adderDone()
{
	delete d->adder;
	d->adder = 0;
}

void KittenPlayer::MainWindow::beginDirectoryAdd(const KUrl &url)
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

void KittenPlayer::MainWindow::showItemContext(const QPoint &at)
{
	QMenu *const menu = static_cast<QMenu*>(factory()->container("item_context", this));
	menu->popup(at);
}

QIcon KittenPlayer::MainWindow::renderIcon(const QString& baseIcon, const QString &overlayIcon) const
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


// kate: space-indent off; replace-tabs off;

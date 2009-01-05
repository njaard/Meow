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

#include <qpixmap.h>
#include <qicon.h>
#include <qevent.h>
#include <kiconeffect.h>

KittenPlayer::MainWindow::MainWindow()
{
	mAdder = 0;
	
	Base *db = new Base;
	db->open(KStandardDirs::locate("data", "kittenplayer/")+"collection");
	
	collection = new Collection(db);

	player = new Player;
	player->setVolume(50);
	view = new TreeView(this, player);
	setCentralWidget(view);
	
	tray = new KSystemTrayIcon(this);
	tray->show();
	
	
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
	
	connect(collection, SIGNAL(added(File)), view, SLOT(addFile(File)));
	
	collection->getFiles();
	
	createGUI();
}

void KittenPlayer::MainWindow::addFile(const KUrl &url)
{
	if (url.isLocalFile())
		collection->add( url.path() );
}

void KittenPlayer::MainWindow::addFiles()
{
	KUrl::List files = KFileDialog::getOpenUrls(
			KUrl("kfiledialog:///mediadir"), player->mimeTypes().join(" "),
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
	tray->toggleActive();
	event->ignore();
}

void KittenPlayer::MainWindow::adderDone()
{
	delete mAdder;
	mAdder = 0;
}

void KittenPlayer::MainWindow::beginDirectoryAdd(const KUrl &url)
{
	if (mAdder)
	{
		mAdder->add(url);
	}
	else
	{
		mAdder = new DirectoryAdder(url, this);
		connect(mAdder, SIGNAL(done()), SLOT(adderDone()));
		connect(mAdder, SIGNAL(addFile(KUrl)), SLOT(addFile(KUrl)));
	}
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

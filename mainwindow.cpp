#include "mainwindow.h"
#include "treeview.h"
#include "player.h"
#include "directoryadder.h"

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
};

Meow::MainWindow::MainWindow()
{
	d = new MainWindowPrivate;
	d->adder = 0;
	d->nowFiltering = false;
	
	d->db.open(KGlobal::dirs()->saveLocation("data", "meow/", false)+"collection");
	
	d->collection = new Collection(&d->db);

	d->player = new Player;
	d->player->setVolume(50);
	d->view = new TreeView(this, d->player, d->collection);
	d->view->installEventFilter(this);
	setCentralWidget(d->view);
	
	d->tray = new KSystemTrayIcon("speaker", this);
	d->tray->installEventFilter(this);
	d->tray->show();
	
	{ // file menu
		KAction *ac;
		ac = actionCollection()->addAction("add_files");
		ac->setText(i18n("Add &Files..."));
		ac->setIcon(KIcon("list-add"));
		connect(ac, SIGNAL(triggered()), SLOT(addFiles()));
		
		VolumeAction *va = new VolumeAction(KIcon("speaker"), i18n("Volume"), actionCollection());
		ac = actionCollection()->addAction("volume", va);
		connect(va, SIGNAL(volumeChanged(int)), d->player, SLOT(setVolume(int)));
		connect(d->player, SIGNAL(volumeChanged(int)), va, SLOT(setVolume(int)));
		
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
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(changeCaption(File)));
	
	d->collection->getFiles();
	
	createGUI();
}

Meow::MainWindow::~MainWindow()
{
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

void Meow::MainWindow::closeEvent(QCloseEvent *event)
{
	d->tray->toggleActive();
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

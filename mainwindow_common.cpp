#include <qinputdialog.h>
#include <qtimer.h>

static QString collectionPath()
{
#if defined(MEOW_WITH_KDE)
	return KGlobal::dirs()->saveLocation("data", "meow/collections");
#elif defined(_WIN32)
	QDir(QDir::homePath()).mkpath("meowplayer.org\\collections");
	return QDir::homePath() + "\\meowplayer.org\\collections\\";
#else
	QDir(QDir::homePath() + "/.config").mkpath("meowplayer.org/collections");
	return QDir::homePath() + "/.config/meowplayer.org/collections/";
#endif
}


void Meow::MainWindow::reloadCollections()
{
	while (!d->collectionsActionGroup->actions().isEmpty())
		delete d->collectionsActionGroup->actions().front();

#ifdef MEOW_WITH_KDE
	KConfigGroup meow = KGlobal::config()->group("state");
	KConfigGroup coll = KGlobal::config()->group("collections");
	QStringList v = coll.keyList();
#else
	QSettings meow; meow.beginGroup("state");
	QSettings coll; coll.beginGroup("collections");
	QStringList v = coll.allKeys();
	typedef QAction KAction;
#endif
	v.sort();
	if (v.isEmpty())
	{
#ifdef MEOW_WITH_KDE
		coll.writeEntry("collection", i18n("Default Collection"));
#else
		coll.setValue("collection", tr("Default Collection"));
#endif
		v += "collection";
	}
	
	d->collectionsActionGroup->setExclusive(true);
	
	QSignalMapper *map=0;
	
	QString currentPlaylist
#ifdef MEOW_WITH_KDE
		= meow.readEntry("collection", "collection");
#else
		= meow.value("collection", "collection").toString();
#endif
	for (int i =0; i < v.size(); i++)
	{
		const QString name = v[i];
		const QString title
#ifdef MEOW_WITH_KDE
			= coll.readEntry(name, i18n("Unnamed Collection"));
#else
			= coll.value(name, tr("Unnamed Collection")).toString();
#endif
		KAction *a = new KAction(title, this);
#ifdef MEOW_WITH_KDE
		d->collectionsAction->addAction(a);
#else
		d->collectionsAction->menu()->addAction(a);
#endif
		d->collectionsActionGroup->addAction(a);
		a->setCheckable(true);
		if (currentPlaylist == name)
			a->setChecked(true);
		if (!map)
			map = new QSignalMapper(a);
		map->setMapping(a, name);
		connect(a, SIGNAL(triggered(bool)), map, SLOT(map()));
	}
	
	connect(map, SIGNAL(mapped(QString)), this, SLOT(loadCollection(QString)));
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

void Meow::MainWindow::wheelEvent(QWheelEvent *event)
{
	if (!d->nowFiltering)
		d->player->setVolume(d->player->volume() + event->delta()*10/120);
}

class SpecialSlider : public QSlider
{
public:
	SpecialSlider(QWidget *parent)
		: QSlider(Qt::Vertical, parent)
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

void Meow::MainWindow::loadCollection(const QString &name, Meow::FileId first)
{
	d->collection->stop();
	d->view->clear();

#if defined(MEOW_WITH_KDE)
	KConfigGroup meow = KGlobal::config()->group("state");
	meow.writeEntry("collection", name);
#else
	QSettings settings;
	settings.setValue("state/collection", name);
#endif
	d->db.open( collectionPath() + name);
	
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

#ifdef MEOW_WITH_KDE
	meow.sync();
#endif
	QTimer::singleShot(0, this, SLOT(reloadCollections()));
}


void Meow::MainWindow::newCollection()
{
	bool ok;
	const QString name = QInputDialog::getText(
			this,
			i18n("Name the new collection"),
			i18n("Name"),
			QLineEdit::Normal,
			i18n("New collection"),
			&ok
		);
	if (!ok)
		return;

	QString filteredName = name;
	filteredName.replace("[^a-zA-Z_0-9]+", "_");
	
	QString filename;
	
	for (int i=1; i < 100000; i++)
	{
		filename = "collection_"+filteredName+"_"+QString::number(i);
		QString path = collectionPath()+filename;
		if (!QFile::exists(path))
		{
			loadCollection(filename);
			break;
		}
	}
#ifdef MEOW_WITH_KDE
	KConfigGroup collections = KGlobal::config()->group("collections");
	collections.writeEntry(filename, name);
#else
	QSettings settings;
	settings.setValue("collections/" + filename, name);
#endif
	reloadCollections();
}

void Meow::MainWindow::copyCollection()
{
#ifdef MEOW_WITH_KDE
	KConfigGroup meow = KGlobal::config()->group("state");
	const QString currentPlaylist = meow.readEntry("collection", "collection");
	
	KConfigGroup collections = KGlobal::config()->group("collections");
	const QString nameOfCurrentPlaylist = collections.readEntry(currentPlaylist, i18n("Unknown playlist"));
#else
	QSettings meow;
	meow.beginGroup("state");
	const QString currentPlaylist = meow.value("collection", "collection").toString();
	
	QSettings collections;
	collections.beginGroup("collections");
	const QString nameOfCurrentPlaylist = collections.value(currentPlaylist, i18n("Unknown playlist")).toString();
#endif

	bool ok;
	const QString name = QInputDialog::getText(
			this,
			i18n("Name the new collection"),
			i18n("Name"),
			QLineEdit::Normal,
			i18n("Copy of %1").arg(nameOfCurrentPlaylist),
			&ok
		);
	if (!ok)
		return;

	d->db.close();
	
	QString origPath = collectionPath()+currentPlaylist;
	
	QString filteredName = name;
	filteredName.replace("[^a-zA-Z_0-9]+", "_");
	
	QString filename;
	
	for (int i=1; i < 100000; i++)
	{
		filename = "collection_"+filteredName+"_"+QString::number(i);
		QString path = collectionPath()+filename;
		if (!QFile::exists(path))
		{
			QFile(origPath).copy(path);
			d->db.open(path);
			break;
		}
	}
#ifdef MEOW_WITH_KDE
	collections.writeEntry(filename, name);
	meow.writeEntry("collection", filename);
	KGlobal::config()->sync();
#else
	collections.setValue(filename, name);
	meow.setValue("collection", filename);
#endif
	reloadCollections();
}

void Meow::MainWindow::renameCollection()
{
#ifdef MEOW_WITH_KDE
	KConfigGroup meow = KGlobal::config()->group("state");
	const QString currentPlaylist = meow.readEntry("collection", "collection");
	
	KConfigGroup collections = KGlobal::config()->group("collections");
	const QString nameOfCurrentPlaylist = collections.readEntry(currentPlaylist, i18n("Unknown playlist"));
#else
	QSettings meow;
	meow.beginGroup("state");
	const QString currentPlaylist = meow.value("collection", "collection").toString();
	
	QSettings collections;
	collections.beginGroup("collections");
	const QString nameOfCurrentPlaylist = collections.value(currentPlaylist, i18n("Unknown playlist")).toString();
#endif

	bool ok;
	const QString name = QInputDialog::getText(
			this,
			i18n("Name the collection"),
			i18n("Name"),
			QLineEdit::Normal,
			nameOfCurrentPlaylist,
			&ok
		);
	if (!ok)
		return;

	d->db.close();
	
	QString origPath = collectionPath()+currentPlaylist;
	
	QString filteredName = name;
	filteredName.replace("[^a-zA-Z_0-9]+", "_");
	
	QString filename;
	
	for (int i=1; i < 100000; i++)
	{
		filename = "collection_"+filteredName+"_"+QString::number(i);
		QString path = collectionPath()+filename;
		if (!QFile::exists(path))
		{
			QFile(origPath).rename(path);
			d->db.open(path);
			break;
		}
	}
#ifdef MEOW_WITH_KDE
	collections.writeEntry(filename, name);
	meow.writeEntry("collection", filename);
	KGlobal::config()->sync();
#else
	collections.setValue(filename, name);
	meow.setValue("collection", filename);
#endif
	reloadCollections();
}

void Meow::MainWindow::deleteCollection()
{
#ifdef MEOW_WITH_KDE
	KConfigGroup meow = KGlobal::config()->group("state");
	const QString currentPlaylist = meow.readEntry("collection", "collection");
	
	KConfigGroup collections = KGlobal::config()->group("collections");
	const QString nameOfCurrentPlaylist = collections.readEntry(currentPlaylist, i18n("Unknown playlist"));
#else
	QSettings meow;
	meow.beginGroup("state");
	const QString currentPlaylist = meow.value("collection", "collection").toString();
	
	QSettings collections;
	collections.beginGroup("collections");
	const QString nameOfCurrentPlaylist = collections.value(currentPlaylist, i18n("Unknown playlist")).toString();
#endif
	
	if ( QMessageBox::Ok != QMessageBox::question(
			this,
			i18n("Delete collection"),
			i18n("Are you sure you want to delete the collection \"%1\"?").arg(nameOfCurrentPlaylist),
			QMessageBox::Ok | QMessageBox::Cancel
		))
	{
		return;
	}

#ifdef MEOW_WITH_KDE
	collections.deleteEntry(currentPlaylist);
	const QStringList keys = collections.keyList();
#else
	collections.remove(currentPlaylist);
	const QStringList keys =collections.allKeys();
#endif
	if (keys.isEmpty())
		loadCollection("collection");
	else
		loadCollection(keys[0]);
#ifdef MEOW_WITH_KDE
	KGlobal::config()->sync();
#endif
}



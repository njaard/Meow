#include "collection.h"
#include "file.h"
#include "sqlt.h"

#include <taglib/tag.h>
#include <taglib/fileref.h>

#include <qfile.h>
#include <qtimer.h>
#include <qevent.h>
#include <qapplication.h>

#include <vector>
#include <map>


namespace
{

class AddFileEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+1);
	AddFileEvent(const QString &file)
		: QEvent(type), file(file)
	{}
	
	const QString file;
};

class ReloadFileEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+2);
	ReloadFileEvent(const Meow::File &file)
		: QEvent(type), file(file)
	{}
	
	const Meow::File file;
};

class FileReloadedEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+3);
	FileReloadedEvent(const Meow::File &file, TagLib::FileRef *const f)
		: QEvent(type), file(file), f(f)
	{}
	
	~FileReloadedEvent()
	{
		delete f;
	}
	
	const Meow::File file;
	TagLib::FileRef *const f;
};

class FileAddedEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+4);
	FileAddedEvent(const QString &file, TagLib::FileRef *const f)
		: QEvent(type), file(file), f(f)
	{}
	
	~FileAddedEvent()
	{
		delete f;
	}
	
	const QString file;
	TagLib::FileRef *const f;
};

class DoneWithJobEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+5);
	DoneWithJobEvent()
		: QEvent(type)
	{}
};
class FinishJobEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+6);
	FinishJobEvent()
		: QEvent(type)
	{}
};


}

// album must be tags[1]
static const char *const tags[] = { "artist", "album", "title", "track" };
static const int numTags = sizeof(tags)/sizeof(tags[0]);


class Meow::Collection::AddThread : public QThread
{
	Collection *const c;

public:
	AddThread(Collection *c)
		: c(c)
	{
	}
	virtual void run()
	{
		exec();
	}
	
	virtual bool event(QEvent *e)
	{
		if (e->type() == AddFileEvent::type)
		{
			const QString file = static_cast<AddFileEvent*>(e)->file;
		
			TagLib::FileRef *const f = new TagLib::FileRef(QFile::encodeName(file).data());
			if (f->isNull() || !f->file() || !f->file()->isValid())
			{
				delete f;
				return true;
			}
			
			QApplication::postEvent(c, new FileAddedEvent(file, f));
		}
		else if (e->type() == ReloadFileEvent::type)
		{
			const File file = static_cast<ReloadFileEvent*>(e)->file;
		
			TagLib::FileRef *const f = new TagLib::FileRef(QFile::encodeName(file.file()).data());
			if (f->isNull() || !f->file() || !f->file()->isValid())
			{
				delete f;
				return true;
			}
			
			QApplication::postEvent(c, new FileReloadedEvent(file, f));
		}
		else if (e->type() == FinishJobEvent::type)
		{
			QApplication::postEvent(c, new DoneWithJobEvent());
		}
		
		return true;
	}
};



struct Meow::Collection::Private
{
	QString bigSelectJoin;
	Base::Statement selectOneSql;

	Base::Statement updateUrlSql, deleteTagsSql, insertSql, insertTagsSql;
	
	LoadAll *allLoader;
};

Meow::Collection::Collection(Base *base)
	: base(base), addThread(0)
{
	d = new Private;
	d->allLoader=0;


	addThread = new AddThread(this);
	addThread->moveToThread(addThread);
	addThread->start(AddThread::LowestPriority);
}

void Meow::Collection::newDatabase()
{
	{
		QString statement = "select songs.song_id, songs.url, albums.flags";
		for (int i=0; i < numTags; i++)
		{
			QString tagCol = "tag_";
			tagCol += 'a'+i;
			statement += ", " + tagCol + ".value";
		}
		
		statement += " from songs";
		for (int i=0; i < numTags; i++)
		{
			QString tagCol = "tag_";
			tagCol += 'a'+i;
			statement += " left outer join tags as " + tagCol;
			statement += " on " + tagCol + ".song_id=songs.song_id and " + tagCol + ".tag='" + Base::escape(tags[i]) +"'";
		}

		statement += " left outer join albums on tag_b.value=albums.album";
		d->bigSelectJoin = statement;
	}
	
	d->selectOneSql = base->sql(d->bigSelectJoin + " where songs.song_id=?");
	d->updateUrlSql = base->sql("update songs set url=? where song_id=?");
	d->deleteTagsSql = base->sql("delete from tags where song_id=?");
	d->insertSql = base->sql("insert into songs values(null, 0, ?)");
	d->insertTagsSql = base->sql("insert into tags values(?, ?, ?)");
}


Meow::Collection::~Collection()
{
	if (addThread)
	{
		addThread->quit();
		addThread->wait();
	}
	delete d;
}

void Meow::Collection::add(const QString &file)
{
	QApplication::postEvent(addThread, new AddFileEvent(file));
}

void Meow::Collection::reload(const Meow::File &file)
{
	QApplication::postEvent(addThread, new ReloadFileEvent(file));
}


void Meow::Collection::remove(const std::vector<FileId> &files)
{
	if (files.size() == 0)
		return;
	base->exec("savepoint remove");
	
	QString songids;
	for (std::vector<FileId>::const_iterator i=files.begin(); i != files.end(); ++i)
	{
		if (songids.length() >0)
			songids += " or ";
		songids += "song_id=" + QString::number(*i);
	}
	
	base->exec("delete from songs where " + songids);
	base->exec("delete from tags where " + songids);
	base->exec("release savepoint remove");
}

class Meow::Collection::BasicLoader
{
public:
	struct SongEntry
	{
		FileId songid;
		QString url;
		QString tags[numTags];
		unsigned flags;
	};
	
	static void toSongEntry(const std::vector<QString> &vals, SongEntry &e)
	{
		if (vals.size() != 3 + numTags)
		{
			std::cerr << "Vals had " << vals.size() << " item"<< std::endl;
			return;
		}

		FileId id = vals[0].toLongLong();
		e.songid = id;
		e.url = vals[1];
		e.flags = vals[2].toInt();
		for (int i=0; i < numTags; i++)
			e.tags[i] = vals[3+i];
	}

	static File toFile(const SongEntry &entry)
	{
		File f;
		f.id = entry.songid;
		f.mFile = entry.url;
		for (int tagi=0; tagi < numTags; ++tagi)
			f.tags[tagi] = entry.tags[tagi];
		if (entry.flags & 1)
			f.mDisplayByAlbum = true;
		return f;
	}
};


class Meow::Collection::LoadAll
	: public QObject, private Meow::Collection::BasicLoader
{
	struct AddEachFile
	{
		Collection *const collection;
		const FileId exceptThisOne;

		AddEachFile(Collection *collection, FileId exceptThisOne)
			: collection(collection), exceptThisOne(exceptThisOne)
		{ }
		void operator() (const std::vector<QString> &vals)
		{
			SongEntry e;
			toSongEntry(vals, e);
			if (exceptThisOne == e.songid)
				return;
			File f = toFile(e);
			emit collection->added(f);
			qApp->processEvents();
		}
	};

	AddEachFile loader;
	Base::Statement selectAll;

public:
	LoadAll(Collection *collection, Base::Statement selectAll, FileId exceptThisOne)
		: loader(collection, exceptThisOne), selectAll(selectAll)
	{
		startTimer(5);
	}
	
protected:
	virtual void timerEvent(QTimerEvent *e)
	{
		killTimer(e->timerId());
		selectAll.exec(loader);
	}
};

void Meow::Collection::getFilesAndFirst(Meow::FileId id)
{
	newDatabase();
	if (id != 0)
	{
		File f = getSong(id);
		if (f)
			emit added(f);
	}
	d->allLoader = new LoadAll(this, base->sql(d->bigSelectJoin), id);
}

void Meow::Collection::stop()
{
	delete d->allLoader;
	d->allLoader=0;
}


struct Meow::Collection::ReloadEachFile : public BasicLoader
{
	Collection *const collection;

	ReloadEachFile(Collection *collection)
		: collection(collection)
	{ }
	void operator() (const std::vector<QString> &vals)
	{
		SongEntry e;
		toSongEntry(vals, e);
		File f = toFile(e);
		emit collection->reloaded(f);
		qApp->processEvents();
	}
};


void Meow::Collection::setGroupByAlbum(const QString &album, bool yes)
{
	if (yes)
		base->sql("insert or replace into albums (album, flags) values(?, 1)").arg(album).exec();
	else
		base->sql("delete from albums where album=?").arg(album).exec();

	ReloadEachFile loader(this);
	Base::Statement statement = base->sql(d->bigSelectJoin + " where tag_b.value=?");
	statement.arg(album).exec(loader);
}

bool Meow::Collection::groupByAlbum(const QString &album)
{
	return 1 & base->sql("select flags from albums where album=?").arg(album).execValue().toInt();
}

void Meow::Collection::startJob()
{
	base->exec("savepoint job");
}
void Meow::Collection::scheduleFinishJob()
{
	QApplication::postEvent(addThread, new FinishJobEvent());
}

struct Meow::Collection::OneFile : public BasicLoader
{
	Collection *const collection;
	File f;
	bool gotOne;

	OneFile(Collection *collection)
		: collection(collection), gotOne(false)
	{ }
	void operator() (const std::vector<QString> &vals)
	{
		gotOne = true;
		SongEntry e;
		toSongEntry(vals, e);
		f = toFile(e);
	}
};


Meow::File Meow::Collection::getSong(FileId id)
{
	OneFile loader(this);
	d->selectOneSql.arg(id).exec(loader);
	
	return loader.f;
}


bool Meow::Collection::event(QEvent *e)
{
	File fff;
	
	if (e->type() == FileAddedEvent::type)
		fff.mFile = static_cast<FileAddedEvent*>(e)->file;
	else if (e->type() == FileReloadedEvent::type)
		fff = static_cast<FileReloadedEvent*>(e)->file;
	else if (e->type() == DoneWithJobEvent::type)
	{
		base->exec("release savepoint job");
		return true;
	}
	else
		return false;
	
	struct Map { TagLib::String (TagLib::Tag::*fn)() const; const char *sql; int tagIndex; };
	static const Map propertyMap[] =
//	

	{
		{ &TagLib::Tag::title, "title", 2 },
		{ &TagLib::Tag::artist, "artist", 0 },
		{ &TagLib::Tag::album, "album", 1 },
		{ 0, 0, 0 }
	};

	const TagLib::FileRef *f=0;
	if (e->type() == FileAddedEvent::type)
		f = static_cast<FileAddedEvent*>(e)->f;
	else if (e->type() == FileReloadedEvent::type)
		f = static_cast<FileReloadedEvent*>(e)->f;
	else
	{
		std::cerr << "Impossible." << std::endl;
		return false;
	}

	const TagLib::Tag *const tag = f->tag();
	
	FileId last;
	
	if (e->type() == FileReloadedEvent::type)
	{
		last = fff.fileId();
		d->updateUrlSql.arg(fff.mFile).arg(fff.fileId()).exec();
		d->deleteTagsSql.arg(fff.fileId()).exec();
	}
	else
	{
		last = d->insertSql.arg(fff.mFile).exec();
		fff.id = last;
	}
	
	for (int i=0; propertyMap[i].sql; i++)
	{
		QString x = QString::fromUtf8(
				(tag->*propertyMap[i].fn)().toCString(true)
			);
		d->insertTagsSql.arg(last).arg(propertyMap[i].sql).arg(x).exec();
		if (propertyMap[i].tagIndex != -1)
			fff.tags[propertyMap[i].tagIndex] = x;
	}
	
	if (tag->track() > 0)
	{
		d->insertTagsSql.arg(last).arg("track").arg(int(tag->track())).exec();
		fff.tags[3] = QString::number(tag->track());
	}

	if (e->type() == FileReloadedEvent::type)
		emit reloaded(fff);
	else
		emit added(fff);
	return true;
}


// kate: space-indent off; replace-tabs off;

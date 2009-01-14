#include "collection.h"
#include "file.h"
#include "sqlt.h"

#include <tag.h>
#include <fileref.h>

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

class FileAddedEvent : public QEvent
{
public:
	static const Type type = QEvent::Type(QEvent::User+2);
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
}


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
		if (e->type() != AddFileEvent::type)
			return false;
			
		const QString file = static_cast<AddFileEvent*>(e)->file;
	
		TagLib::FileRef *const f = new TagLib::FileRef(QFile::encodeName(file).data());
		if (f->isNull() || !f->file() || !f->file()->isValid())
		{
			delete f;
			return true;
		}
		
		QApplication::postEvent(c, new FileAddedEvent(file, f));
		
		return true;
	}
};



Meow::Collection::Collection(Base *base)
	: base(base), addThread(0)
{
		addThread = new AddThread(this);
		addThread->start(AddThread::LowestPriority);
		addThread->moveToThread(addThread);
}


Meow::Collection::~Collection()
{
	if (addThread)
	{
		addThread->quit();
		addThread->wait();
	}
}



void Meow::Collection::add(const QString &file)
{
	QApplication::postEvent(addThread, new AddFileEvent(file));
}

void Meow::Collection::remove(const std::vector<FileId> &files)
{
	if (files.size() == 0)
		return;
	base->sql("begin transaction");
	
	QString songids;
	for (std::vector<FileId>::const_iterator i=files.begin(); i != files.end(); ++i)
	{
		if (songids.length() >0)
			songids += " or ";
		songids += "song_id=" + QString::number(*i);
	}
	
	base->sql("delete from songs where " + songids);
	base->sql("delete from tags where " + songids);
	base->sql("commit transaction");
}

static const char *const tags[] = { "artist", "album", "title", "track" };
static const int numTags = sizeof(tags)/sizeof(tags[0]);

class Meow::Collection::BasicLoader
{
public:
	struct SongEntry
	{
		FileId songid;
		QString url;
		QString tags[numTags];
	};
	
	static const int SIZE_OF_CHUNK_TO_LOAD = 32;
	SongEntry songEntriesInChunk[SIZE_OF_CHUNK_TO_LOAD];
	int numberInChunk;
	
	static File toFile(const SongEntry &entry)
	{
		File f;
		f.id = entry.songid;
		f.mFile = entry.url;
		for (int tagi=0; tagi < numTags; ++tagi)
		{
			f.tags[tagi] = entry.tags[tagi];
		}
		return f;
	}
	
	struct LoadEachFile
	{
		BasicLoader *const loader;
		FileId lastId;
		
		LoadEachFile(BasicLoader *loader) : loader(loader), lastId(0)
		{
			loader->numberInChunk = -1;
		}
		void operator() (const std::vector<QString> &vals)
		{
			if (vals.size() != 4)
				return;
			
			FileId id = vals[0].toLongLong();
			if (id != lastId)
			{
				loader->numberInChunk++;
				loader->songEntriesInChunk[loader->numberInChunk].songid = id;
				loader->songEntriesInChunk[loader->numberInChunk].url = vals[1];
				lastId = id;
			}
			
			QString tag = vals[2];
			
			for (int i=0; i < numTags; ++i)
			{
				if (tags[i] == tag)
				{
					loader->songEntriesInChunk[loader->numberInChunk].tags[i] = vals[3];
					break;
				}
			}
			
		}
	};
	
};

class Meow::Collection::LoadAll
	: public QObject, private Meow::Collection::BasicLoader
{
	int index;
	int count;
	Base *const b;
	Collection *const collection;
	const FileId exceptThisOne;

public:
	LoadAll(Base *b, Collection *collection, FileId exceptThisOne)
		: b(b), collection(collection), exceptThisOne(exceptThisOne)
	{
		index=0;
		numberInChunk = 0;
		
		count = b->sqlValue("select COUNT(*) from songs").toInt();
		
		startTimer(5);
	}
	
protected:
	virtual void timerEvent(QTimerEvent *e)
	{
	
		QString select = "select songs.song_id, songs.url, tags.tag, tags.value "
			"from songs natural join tags where songs.song_id > "
			+ QString::number(index) + " and songs.song_id < "
			+ QString::number(index+SIZE_OF_CHUNK_TO_LOAD);
			
		LoadEachFile l(this);
		b->sql(select, l);
		
		for (int i=0; i < numberInChunk; i++)
		{
			if (songEntriesInChunk[i].songid != exceptThisOne)
			{
				File f = toFile(songEntriesInChunk[i]);
				emit collection->added(f);
			}
		}
		
		index += SIZE_OF_CHUNK_TO_LOAD;
		if (index >= count)
		{
			killTimer(e->timerId());
			deleteLater();
		}
	}
};

void Meow::Collection::getFilesAndFirst(Meow::FileId id)
{
	if (id != 0)
	{
		File f = getSong(id);
		emit added(f);
	}
	new LoadAll(base, this, id);
}

Meow::File Meow::Collection::getSong(FileId id) const
{
	File file;
	QString select = "select songs.song_id, songs.url, tags.tag, tags.value "
		"from songs natural join tags where songs.song_id="
		+ QString::number(id);
	
	BasicLoader loader;
	BasicLoader::LoadEachFile l(&loader);
	base->sql(select, l);
	if (loader.numberInChunk == 0)
	{
		file = BasicLoader::toFile(loader.songEntriesInChunk[0]);
	}
	return file;
}


bool Meow::Collection::event(QEvent *e)
{
	if (e->type() != FileAddedEvent::type)
		return false;
	
	File fff;
	fff.mFile = static_cast<FileAddedEvent*>(e)->file;
	struct Map { TagLib::String (TagLib::Tag::*fn)() const; const char *sql; int tagIndex; };
	static const Map propertyMap[] =
	{
		{ &TagLib::Tag::title, "title", 2 },
		{ &TagLib::Tag::artist, "artist", 0 },
		{ &TagLib::Tag::album, "album", 1 },
		{ 0, 0, 0 }
	};


	const TagLib::FileRef *const f = static_cast<FileAddedEvent*>(e)->f;
	const TagLib::Tag *const tag = f->tag();
	
	base->sql("begin transaction");
	const int64_t last = base->sql(
			"insert into songs values(null, 0, '"
				+ Base::escape(fff.mFile)
				+ "')"
		);
	fff.id = last;
	for (int i=0; propertyMap[i].sql; i++)
	{
		QString x = QString::fromUtf8(
				(tag->*propertyMap[i].fn)().toCString(true)
			);
		base->sql(
				"insert into tags values("
					+ QString::number(last) + ", '"+ propertyMap[i].sql + "', '"
					+ Base::escape(x) + "')"
			);
		if (propertyMap[i].tagIndex != -1)
			fff.tags[propertyMap[i].tagIndex] = x;
	}
	
	if (tag->track() > 0)
	{
		base->sql(
				"insert into tags values("
					+ QString::number(last) + ", 'track', '"
					+ QString::number(tag->track()) + "')"
			);
		fff.tags[3] = QString::number(tag->track());
	}
	base->sql("commit transaction");

	emit added(fff);
	return true;
}


// kate: space-indent off; replace-tabs off;

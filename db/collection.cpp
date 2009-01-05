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


class KittenPlayer::Collection::AddThread : public QThread
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



KittenPlayer::Collection::Collection(Base *base)
	: base(base), addThread(0)
{
		addThread = new AddThread(this);
		addThread->start(AddThread::LowestPriority);
		addThread->moveToThread(addThread);
}


KittenPlayer::Collection::~Collection()
{
	if (addThread)
	{
		addThread->quit();
		addThread->wait();
	}
}



void KittenPlayer::Collection::add(const QString &file)
{
	QApplication::postEvent(addThread, new AddFileEvent(file));
	
}

#define SIZE_OF_CHUNK_TO_LOAD 32

class KittenPlayer::Collection::LoadAll : public QObject
{
	int index;
	int count;
	Base *const b;
	Collection *const collection;
	
	uint64_t idsInChunk[SIZE_OF_CHUNK_TO_LOAD];
	QString urlsInChunk[SIZE_OF_CHUNK_TO_LOAD];
	int numberInChunk;
	
	struct LoadEachFile
	{
		Base *const b;
		LoadAll *const loader;
		LoadEachFile(Base *b, LoadAll *loader) : b(b), loader(loader) { }
		void operator() (const std::vector<QString> &vals)
		{
			if (vals.size() != 2)
				return;
			
			uint64_t id = vals[0].toLongLong();
			loader->idsInChunk[loader->numberInChunk] = id;
			loader->urlsInChunk[loader->numberInChunk] = vals[1];
			loader->numberInChunk++;
		}
	};
	
	struct LoadTags
	{
		std::map<QString, QString> vals;
		void operator() (const std::vector<QString> &v)
		{
			if (v.size() != 2)
				return;
			vals.insert(std::make_pair(v[0], v[1]));
		}
	};

public:
	LoadAll(Base *b, Collection *collection)
		: b(b), collection(collection)
	{
		index=0;
		numberInChunk = 0;
		
		count = b->sqlValue("select COUNT(*) from songs").toInt();
		
		startTimer(5);
	}
	
protected:
	virtual void timerEvent(QTimerEvent *e)
	{
		LoadEachFile l(b, this);
#define xstr(s) str(s)
#define str(s) #s
		b->sql(
				"select id,url from songs order by id "
					"limit " xstr(SIZE_OF_CHUNK_TO_LOAD) " offset "
					+ QString::number(index), l
			);
#undef str
#undef xstr
		for (int i=0; i < numberInChunk; i++)
		{
			File f;
			f.id = idsInChunk[i];
			LoadTags tags;
			b->sql(
					"select tag,value from tags where song_id=" + QString::number(f.id),
					tags
				);
			f.mFile = urlsInChunk[i];
			f.mArtist = tags.vals["artist"];
			f.mAlbum = tags.vals["album"];
			f.mTitle = tags.vals["title"];
			f.mTrack = tags.vals["track"];
			
			emit collection->added(f);
		}
		numberInChunk=0;
		
		index += 32;
		if (index >= count)
		{
			killTimer(e->timerId());
			deleteLater();
		}
	}
};

#undef SIZE_OF_CHUNK_TO_LOAD

void KittenPlayer::Collection::getFiles()
{
	new LoadAll(base, this);
}

bool KittenPlayer::Collection::event(QEvent *e)
{
	if (e->type() != FileAddedEvent::type)
		return false;
	
	File fff;
	fff.mFile = static_cast<FileAddedEvent*>(e)->file;
	struct Map { TagLib::String (TagLib::Tag::*fn)() const; const char *sql; QString *f; };
	Map propertyMap[] =
	{
		{ &TagLib::Tag::title, "title", &fff.mTitle },
		{ &TagLib::Tag::artist, "artist", &fff.mArtist },
		{ &TagLib::Tag::album, "album", &fff.mAlbum },
		{ &TagLib::Tag::genre, "genre", 0 },
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
		if (propertyMap[i].f)
			*propertyMap[i].f = x;
	}
	
	if (tag->track() > 0)
	{
		base->sql(
				"insert into tags values("
					+ QString::number(last) + ", 'track', '"
					+ QString::number(tag->track()) + "')"
			);
		fff.mTrack = tag->track();
	}
	base->sql("commit transaction");

	emit added(fff);
	return true;
}


// kate: space-indent off; replace-tabs off;

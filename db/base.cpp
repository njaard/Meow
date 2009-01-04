#include "base.h"
#include "file.h"

#include <sqlite3.h>

#include <tag.h>
#include <fileref.h>

#include <qfile.h>
#include <qtimer.h>
#include <qevent.h>

#include <vector>

struct KittenPlayer::Base::BasePrivate
{
	sqlite3 *db;
};

template<class T>
inline int64_t KittenPlayer::Base::sql(const QString &s, T &function)
{
	const QByteArray utf8 = s.toUtf8();
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(d->db, utf8.data(), utf8.length(), &stmt, 0);
	
	int x;
	while (1)
	{
		x = sqlite3_step(stmt);
		if (x == SQLITE_ROW)
		{
			std::vector<QString> vars;
			const int cols = sqlite3_column_count(stmt);
			for (int i=0; i < cols; ++i)
			{
				const void *bytes = sqlite3_column_blob(stmt, i);
				const int count = sqlite3_column_bytes(stmt, i);
				QString s = QString::fromUtf8(static_cast<const char*>(bytes), count);
				vars.push_back(s);
			}
			
			function(vars);
		}
		else if (x == SQLITE_BUSY)
			continue;
		else
			break;
	}
	
	if (x == SQLITE_ERROR)
	{
		std::cerr << "SQLite error: " << sqlite3_errmsg(d->db) << std::endl;
	}
	
	sqlite3_finalize(stmt);
	return sqlite3_last_insert_rowid(d->db);
}


KittenPlayer::Base::Base()
{
	d = new BasePrivate;
}


KittenPlayer::Base::~Base()
{
	delete d;
}

bool KittenPlayer::Base::open(const QString &database)
{
	int rc = sqlite3_open(QFile::encodeName(database).data(), &d->db);
	initialize();

	return true;
}

struct Map { TagLib::String (TagLib::Tag::*fn)() const; const char *sql; };
static const Map propertyMap[] =
{
	{ &TagLib::Tag::title, "title" },
	{ &TagLib::Tag::artist, "artist" },
	{ &TagLib::Tag::album, "album" },
	{ &TagLib::Tag::genre, "genre" },
	{ 0, 0 }
};


void KittenPlayer::Base::add(const QString &file)
{
	
	const int64_t last = sql("insert into songs values(null, 0, '" + escape(file) + "')");
	
	const TagLib::FileRef f(QFile::encodeName(file).data());
	if (f.isNull())
		return;
	const TagLib::Tag *const tag = f.tag();
	
	for (int i=0; propertyMap[i].sql; i++)
	{
		sql(
				"insert into tags values("
					+ QString::number(last) + ", '"+ propertyMap[i].sql + "', '"
					+ escape(QString::fromUtf8( (tag->*propertyMap[i].fn)().toCString(true))) + "')"
			);
	}
	
	if (tag->track() > 0)
	{
		sql(
				"insert into tags values("
					+ QString::number(last) + ", 'track', '"
					+ QString::number(tag->track()) + "')"
			);
	}
	
	File fff(this, last);
	emit added(fff);
}

void KittenPlayer::Base::initialize()
{
	static const char *tables[] =
		{
			"create table songs ("
				"id integer primary key autoincrement, "
				"length int not null, "
				"url char(255))",
			"create table tags ("
				"song_id integer not null, "
				"tag char(32), "
				"value char(255))",
			0
		};

	for (int i=0; tables[i]; i++)
	{
		sql(tables[i]);
	}
}

namespace
{
struct Nothing { void operator() (const std::vector<QString> &) { } };
}

int64_t KittenPlayer::Base::sql(const QString &s)
{
	Nothing n;
	return sql(s, n);
}

namespace
{
struct Keep1
{
	bool got;
	Keep1() { got = false; }
	QString first;
	void operator() (const std::vector<QString> &vals)
	{
		if (got) return;
		got = true;
		if (vals.size() > 0)
			first=vals[0];
	}
};
}

QString KittenPlayer::Base::sqlValue(const QString &s) const
{
	const_cast<Base*>(this)->sql("begin transaction");
	std::vector<QString> vals;
	Keep1 k;
	const_cast<Base*>(this)->sql(s, k);
	const_cast<Base*>(this)->sql("rollback transaction");
	return k.first;
}

#define SIZE_OF_CHUNK_TO_LOAD 32

class KittenPlayer::Base::LoadAll : public QObject
{
	int index;
	int count;
	Base *const b;
	
	uint64_t idsInChunk[SIZE_OF_CHUNK_TO_LOAD];
	int numberInChunk;
	
	struct LoadEachFile
	{
		Base *const b;
		LoadAll *const loader;
		LoadEachFile(Base *b, LoadAll *loader) : b(b), loader(loader) { }
		void operator() (const std::vector<QString> &vals)
		{
			if (vals.size() == 0)
				return;
			
			uint64_t id = vals[0].toLongLong();
			loader->idsInChunk[loader->numberInChunk++] = id;
		}
	};

public:
	LoadAll(Base *b)
		: b(b)
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
				"select id from songs order by id "
					"limit " xstr(SIZE_OF_CHUNK_TO_LOAD) " offset "
					+ QString::number(index), l
			);
#undef str
#undef xstr
		for (int i=0; i < numberInChunk; i++)
		{
			File f(b, idsInChunk[i]);
			emit b->added(f);
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

void KittenPlayer::Base::getFiles()
{
	new LoadAll(this);
}

QString KittenPlayer::Base::escape(const QString &s)
{
	char *e = sqlite3_mprintf("%q", s.toUtf8().data());
	QString x = QString::fromUtf8(e);
	sqlite3_free(e);
	return x;
}

// kate: space-indent off; replace-tabs off;

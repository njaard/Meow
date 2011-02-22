#include "base.h"

#include "sqlt.h"


Meow::Base::Base()
{
	d = new BasePrivate;
}

Meow::Base::~Base()
{
	sqlite3_close(d->db);
	delete d;
}

bool Meow::Base::open(const QString &database)
{
	int rc = sqlite3_open(
			QFile::encodeName(database).data(), &d->db
		);
	if (rc != SQLITE_OK)
	{
		std::cerr << "Failed to open " << database.toLatin1().data() << std::endl;
		return false;
	}
	if (0 == sqlite3_threadsafe())
	{
		std::cerr << "Failing because sqlite is not threadsafe" << std::endl;
		return false;
	}
		
	initialize();

	return true;
}

void Meow::Base::initialize()
{
	bool doMigrate=false;
	exec("begin transaction");
	
	exec("create table if not exists version (version integer primary key not null)");
	if (0 == execValue("select count(version) from version").toInt())
	{ }
	else if ( 2 < execValue("select max(version) from version").toInt() )
	{
		exec("delete from version");
		doMigrate=true;
	}
	
	exec("insert into version values(2)");

	if (doMigrate)
	{
		exec("alter table songs rename to songs_migrate");
		exec("alter table tags rename to tags_migrate");
		exec("alter table albums rename to albums_migrate");
	}

	static const char *const tables[] =
		{
			"create table if not exists songs ("
				"song_id integer primary key autoincrement, "
				"length int not null, "
				"url text not null)",
			"create table if not exists tags ("
				"song_id integer not null, "
				"tag text not null, "
				"value text not null, "
				"primary key (song_id, tag))",
			"create table if not exists albums ("
				"album text not null primary key, "
				"flags integer not null)",
			0
		};

	for (int i=0; tables[i]; i++)
		exec(tables[i]);

	if (doMigrate)
	{
		exec("insert into songs (song_id, length, url) select song_id, length, url from songs_migrate");
		exec("insert into tags (song_id, tag, value) select song_id, tag, value from tags_migrate");
		exec("insert into albums (album, flags) select album, flags from albums_migrate");
		exec("drop table songs_migrate");
		exec("drop table tags_migrate");
		exec("drop table albums_migrate");
		exec("vacuum");
	}
	
	exec("commit transaction");
}

Meow::Base::Statement::Shared::Shared(sqlite3 *db, sqlite3_stmt *statement)
	: db(db), statement(statement)
{
	refs = 1;
	statement = 0;
	bindingIndex = 0;
}
Meow::Base::Statement::Shared::~Shared()
{
	sqlite3_finalize(statement);
}


Meow::Base::Statement::~Statement()
{
	if (shared && --shared->refs==0)
		delete shared;
}
Meow::Base::Statement::Statement(const Statement &copy)
	: shared(copy.shared)
{
	if (shared)
		shared->refs++;
}

Meow::Base::Statement &Meow::Base::Statement::operator=(const Statement &o)
{
	if (o.shared == shared) return *this;
	if (shared && --shared->refs==0)
		delete shared;
	shared = o.shared;
	shared->refs++;
	return *this;
}

Meow::Base::Statement& Meow::Base::Statement::arg(const QString &string)
{
	QByteArray bytes = string.toUtf8();
	sqlite3_bind_text(shared->statement, ++shared->bindingIndex, bytes.constData(), bytes.length(), SQLITE_TRANSIENT);
	return *this;
}

Meow::Base::Statement& Meow::Base::Statement::arg(long long n)
{
	sqlite3_bind_int64(shared->statement, ++shared->bindingIndex, n);
	return *this;
}

Meow::Base::Statement& Meow::Base::Statement::arg(int n)
{
	sqlite3_bind_int(shared->statement, ++shared->bindingIndex, n);
	return *this;
}

namespace
{
struct Nothing { void operator() (const std::vector<QString> &) { } };
}

int64_t Meow::Base::Statement::exec()
{
	Nothing n;
	return exec(n);
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

QString Meow::Base::Statement::execValue()
{
	Keep1 k;
	exec(k);
	return k.first;
}

Meow::Base::Statement Meow::Base::sql(const QString &s)
{
	sqlite3_stmt *stmt;
	QByteArray utf8 = s.toUtf8();
	if (SQLITE_OK != sqlite3_prepare_v2(d->db, utf8.constData(), utf8.length(), &stmt, 0))
	{
		std::cerr << "SQLite error (prepare): " << sqlite3_errmsg(d->db) << ": <<<" << utf8.constData() << ">>>" << std::endl;
	}
	Statement st;
	st.shared = new Statement::Shared(d->db, stmt);
	return st;
}



QString Meow::Base::execValue(const QString &s)
{
	return sql(s).execValue();
}

QString Meow::Base::escape(const QString &s)
{
	char *e = sqlite3_mprintf("%q", s.toUtf8().data());
	QString x = QString::fromUtf8(e);
	sqlite3_free(e);
	return x;
}

// kate: space-indent off; replace-tabs off;

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
	static const char *const tables[] =
		{
			"create table if not exists songs ("
				"song_id integer primary key autoincrement, "
				"length int not null, "
				"url char(255) not null)",
			"create table if not exists tags ("
				"song_id integer not null, "
				"tag char(32) not null, "
				"value char(255) not null)",
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

int64_t Meow::Base::sql(const QString &s)
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

QString Meow::Base::sqlValue(const QString &s) const
{
	const_cast<Base*>(this)->sql("begin transaction");
	std::vector<QString> vals;
	Keep1 k;
	const_cast<Base*>(this)->sql(s, k);
	const_cast<Base*>(this)->sql("rollback transaction");
	return k.first;
}

QString Meow::Base::escape(const QString &s)
{
	char *e = sqlite3_mprintf("%q", s.toUtf8().data());
	QString x = QString::fromUtf8(e);
	sqlite3_free(e);
	return x;
}

// kate: space-indent off; replace-tabs off;

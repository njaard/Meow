#include "base.h"

#include "sqlt.h"


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
	int rc = sqlite3_open(
			QFile::encodeName(database).data(), &d->db
		);
	if (0 == sqlite3_threadsafe())
		std::cerr << "Failing because sqlite is not threadsafe" << std::endl;
	initialize();

	return true;
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

QString KittenPlayer::Base::escape(const QString &s)
{
	char *e = sqlite3_mprintf("%q", s.toUtf8().data());
	QString x = QString::fromUtf8(e);
	sqlite3_free(e);
	return x;
}

// kate: space-indent off; replace-tabs off;

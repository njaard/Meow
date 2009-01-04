#include <db/file.h>
#include <db/base.h>

KittenPlayer::File::File(Base *base, int64_t id)
	: base(base), id(id)
{
}

QString KittenPlayer::File::artist() const
{
	return base->sqlValue(
			"select value from tags where song_id=" + QString::number(id)
				+ " and tag='artist'");
}

QString KittenPlayer::File::album() const
{
	return base->sqlValue(
			"select value from tags where song_id=" + QString::number(id)
				+ " and tag='album'");
}

QString KittenPlayer::File::title() const
{
	return base->sqlValue(
			"select value from tags where song_id=" + QString::number(id)
				+ " and tag='title'");
}

QString KittenPlayer::File::file() const
{
	return base->sqlValue("select url from songs where id=" + QString::number(id));
}

QString KittenPlayer::File::track() const
{
	return base->sqlValue(
			"select value from tags where song_id=" + QString::number(id)
				+ " and tag='track'");
}

// kate: space-indent off; replace-tabs off;

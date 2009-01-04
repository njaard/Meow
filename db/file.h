#ifndef KITTENPLAYER_FILE_H
#define KITTENPLAYER_FILE_H

#include <qstring.h>

#include <stdint.h>

namespace KittenPlayer
{
class Base;

class File
{
	Base *const base;
	const int64_t id;
	
public:
	File(Base *base, int64_t id);
	
	QString artist() const;
	QString album() const;
	QString title() const;
	QString file() const;
	QString track() const;
};


}

#endif

// kate: space-indent off; replace-tabs off;

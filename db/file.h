#ifndef KITTENPLAYER_FILE_H
#define KITTENPLAYER_FILE_H

#include <qstring.h>
#include <stdint.h>


namespace KittenPlayer
{
class Collection;
typedef uint64_t FileId;

class File
{
	friend class Collection;
	FileId id;
	
	
	QString mFile;
	// refer to KittenPlayer::Collection::LoadAll
	QString tags[4];
	
public:
	FileId fileId() const { return id; }
	
	QString file() const { return mFile; }
	
	QString artist() const { return tags[0]; }
	QString album() const { return tags[1]; }
	QString title() const { return tags[2]; }
	QString track() const { return tags[3]; }
};


}

#endif

// kate: space-indent off; replace-tabs off;

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
	
	QString mArtist, mAlbum, mTitle, mFile, mTrack;
	
public:
	FileId fileId() const { return id; }
	
	QString artist() const { return mArtist; }
	QString album() const { return mAlbum; }
	QString title() const { return mTitle; }
	QString file() const { return mFile; }
	QString track() const { return mTrack; }
};


}

#endif

// kate: space-indent off; replace-tabs off;

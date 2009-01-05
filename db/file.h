#ifndef KITTENPLAYER_FILE_H
#define KITTENPLAYER_FILE_H

#include <qstring.h>

#include <stdint.h>

namespace KittenPlayer
{
class Collection;

class File
{
	friend class Collection;
	int64_t id;
	
	QString mArtist, mAlbum, mTitle, mFile, mTrack;
	
public:
	int64_t fileId() const { return id; }
	
	QString artist() const { return mArtist; }
	QString album() const { return mAlbum; }
	QString title() const { return mTitle; }
	QString file() const { return mFile; }
	QString track() const { return mTrack; }
};


}

#endif

// kate: space-indent off; replace-tabs off;

#ifndef MEOW_FILE_H
#define MEOW_FILE_H

#include <qstring.h>
#include <stdint.h>


namespace Meow
{
class Collection;
typedef unsigned long long FileId;

class File
{
	friend class Collection;
	FileId id;
	
	
	QString mFile;
	// refer to Meow::Collection::LoadAll
	QString tags[4];
	
public:
	File() { id = 0; }
	FileId fileId() const { return id; }
	operator bool() const { return !!fileId(); }
	bool operator==(const File &other) const { return id == other.id; }
	bool operator!=(const File &other) const { return id != other.id; }
	
	QString file() const { return mFile; }
	
	QString artist() const { return tags[0]; }
	QString album() const { return tags[1]; }
	QString title() const { return tags[2]; }
	QString track() const { return tags[3]; }
};


}

#endif

// kate: space-indent off; replace-tabs off;

#ifndef KITTENPLAYER_COLLECTION_H
#define KITTENPLAYER_COLLECTION_H

#include <qobject.h>
#include <qthread.h>

namespace KittenPlayer
{

class File;
class Base;

class Collection : public QObject
{
	Q_OBJECT
	
	Base *const base;
	
	class LoadAll;
	class AddThread;
	
	AddThread *addThread;
	
public:
	Collection(Base *base);
	~Collection();

	void add(const QString &file);
	
	/**
	 * emit @ref added for all files in the database
	 **/
	void getFiles();

signals:
	void added(const File &file);
	void removed(const File &file);
	void modified(const File &file);

	/**
	 * emitted when something of the slices gets modified
	 * @ref Slice calls this itself via a friendship
	 **/
	void slicesModified();

protected:
	virtual bool event(QEvent *e);
};

}

#endif
 
// kate: space-indent off; replace-tabs off;
// kate: space-indent off; replace-tabs off;

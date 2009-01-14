#ifndef MEOW_COLLECTION_H
#define MEOW_COLLECTION_H

#include <qobject.h>
#include <qthread.h>

#include <vector>

#include <db/file.h>

namespace Meow
{

class File;
class Base;

class Collection : public QObject
{
	Q_OBJECT
	
	Base *const base;
	
	class BasicLoader;
	class LoadAll;
	class AddThread;
	
	AddThread *addThread;
	
public:
	Collection(Base *base);
	~Collection();

	void add(const QString &file);
	void reload(const File &file);
	
	void remove(const std::vector<FileId> &files);
	
	/**
	 * emit @ref added for all files in the database
	 **/
	void getFilesAndFirst(FileId id);
	
	/**
	 * gets just this file by id. This function is slow
	 **/
	File getSong(FileId id) const;
	

signals:
	void added(const File &file);
	void reloaded(const File &file);

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

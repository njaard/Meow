#ifndef KITTENPLAYER_BASE_H
#define KITTENPLAYER_BASE_H

#include <qtreeview.h>

namespace KittenPlayer
{

typedef unsigned int FileId;
class File;

class Base : public QObject
{
	Q_OBJECT
	
	struct BasePrivate;
	BasePrivate *d;
	
	class LoadAll;
	
public:
	Base();
	~Base();

	bool open(const QString &database);
	void add(const QString &file);
	
	QString sqlValue(const QString &s) const;
	
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

private:
	void initialize();
	int64_t sql(const QString &s);
	template<class T>
	int64_t sql(const QString &s, T &function);
	
	
	static QString escape(const QString &s);
	
	
};


}

#endif
 
// kate: space-indent off; replace-tabs off;
// kate: space-indent off; replace-tabs off;

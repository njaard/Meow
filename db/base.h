#ifndef MEOW_BASE_H
#define MEOW_BASE_H

#include <qstring.h>

#include <stdint.h>

namespace Meow
{

class Base
{
	struct BasePrivate;
	BasePrivate *d;
	
public:
	Base();
	~Base();

	bool open(const QString &database);
	
	QString sqlValue(const QString &s) const;
	
	static QString escape(const QString &s);
	
	int64_t sql(const QString &s);
	template<class T>
	int64_t sql(const QString &s, T &function);
private:
	void initialize();
};



}

#endif
 
// kate: space-indent off; replace-tabs off;

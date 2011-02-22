#ifndef MEOW_BASE_H
#define MEOW_BASE_H

#include <qstring.h>
#include <stdint.h>

#include <sqlite3.h>

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

	struct Statement
	{
		struct Shared
		{
			int refs;
			sqlite3 *const db;
			sqlite3_stmt *const statement;
			int bindingIndex;
			
			Shared(sqlite3 *db, sqlite3_stmt *statement);
			~Shared();
		};
		Shared *shared;

		friend class Base;
		
	public:
		Statement() : shared(0) { }
		~Statement();
		Statement(const Statement &copy);
		Statement &operator=(const Statement &o);

		Statement& arg(const QString &string);
		Statement& arg(long long n);
		Statement& arg(unsigned long long n)
		{
			return arg( static_cast<long long>(n) );
		}
		Statement& arg(int n);

		template<class T>
		int64_t exec(T &function);
		
		int64_t exec();
		QString execValue();
	};
	
	QString execValue(const QString &s);
	
	static QString escape(const QString &s);

	Statement sql(const QString &s);
	
	int64_t exec(const QString &s)
	{
		return sql(s).exec();
	}
	template<class T>
	int64_t exec(const QString &s, T &function)
	{
		return sql(s).exec(function);
	}
	
private:
	void initialize();
};



}

#endif
 
// kate: space-indent off; replace-tabs off;

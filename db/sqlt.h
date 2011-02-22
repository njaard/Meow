#ifndef MEOW_SQLT_H 
#define MEOW_SQLT_H 

#include "base.h"

#include <qfile.h>

#include <vector>
#include <iostream>


struct Meow::Base::BasePrivate
{
	sqlite3 *db;
};

template<class T>
inline int64_t Meow::Base::Statement::exec(T &function)
{
	sqlite3_stmt *const stmt = shared->statement;
//	std::cerr << "Q: " << sqlite3_sql(stmt) << std::endl;
	int x;
	while (1)
	{
		x = sqlite3_step(stmt);
		if (x == SQLITE_ROW)
		{
			std::vector<QString> vars;
			const int cols = sqlite3_column_count(stmt);
			for (int i=0; i < cols; ++i)
			{
				const void *bytes = sqlite3_column_blob(stmt, i);
				const int count = sqlite3_column_bytes(stmt, i);
				QString s = QString::fromUtf8(static_cast<const char*>(bytes), count);
				vars.push_back(s);
			}
			
			function(vars);
		}
		else if (x == SQLITE_BUSY)
			continue;
		else
			break;
	}
	
	if (x == SQLITE_ERROR)
	{
		std::cerr << "SQLite error: " << sqlite3_errmsg(shared->db) << ": <<<" << sqlite3_sql(stmt) << ">>>" << std::endl;
	}
	
	sqlite3_reset(stmt);
	shared->bindingIndex=0;
	return sqlite3_last_insert_rowid(shared->db);
}

#endif
// kate: space-indent off; replace-tabs off;

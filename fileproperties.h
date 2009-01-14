#ifndef MEOW_FILEPROPERTIES_H
#define MEOW_FILEPROPERTIES_H

#include <kpropertiesdialog.h>
#include <qlist.h>

#include "db/file.h"

namespace Meow
{

class Collection;

class FileProperties : public KPropertiesDialog
{
	Q_OBJECT
	QList<File> mFiles;
	Collection *const collection;

public:
	FileProperties(const QList<File> &files, Collection *collection, QWidget *parent);

private:
	static KFileItemList makeItems(const QList<File> &files);

private slots:
	void modified();
};
}
#endif

// kate: space-indent off; replace-tabs off;

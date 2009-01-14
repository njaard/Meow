#include "fileproperties.h"
#include "db/collection.h"

Meow::FileProperties::FileProperties(
		const QList<File> &files, Collection *collection,
		QWidget *parent
	)
	: KPropertiesDialog(makeItems(files), parent)
	, mFiles(files), collection(collection)
{
	connect(this, SIGNAL(propertiesClosed()), SLOT(deleteLater()));
	connect(this, SIGNAL(applied()), SLOT(modified()));

	show();
}

void Meow::FileProperties::modified()
{
	// TODO reload the file's info
	for (QList<File>::iterator i(mFiles.begin()); i != mFiles.end(); ++i)
	{
		collection->reload(*i);
	}
}

KFileItemList Meow::FileProperties::makeItems(const QList<File> &files)
{
	KFileItemList kl;
	for (QList<File>::const_iterator i(files.begin()); i != files.end(); ++i)
	{
		const File &f = *i;
		kl.append(KFileItem(
				f.file(), 
				KMimeType::findByPath(f.file())->name(),
				KFileItem::Unknown
			));
	}
	return kl;
}

// kate: space-indent off; replace-tabs off;

#ifndef MEOW_MODEL_H
#define MEOW_MODEL_H

#include <qtreeview.h>

namespace KittenPlayer
{

class Model : public QAbstractItemModel
{
    Q_OBJECT
    struct ModelPrivate;
    ModelPrivate *d;

public:
	Model();
	QVariant data(const QModelIndex &index, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	QVariant headerData(int section, Qt::Orientation orientation,
	                    int role = Qt::DisplayRole) const;
	QModelIndex index(int row, int column,
	                  const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

public:
	void addFile(const File &file);

};


}

#endif

// kate: space-indent off; replace-tabs off;

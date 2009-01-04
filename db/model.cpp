#include <db/model.h>

namespace
{
class Node
{
public:
	virtual ~Node() { }
	virtual QString text()=0;

};



struct Song : public Node
{
	const KittenPlayer::File mFile;
public:
	Song(const KittenPlayer::File &file)
		: mFile(file)
	{ }
	virtual QString text() { return mFile.artist(); }
};

struct Album : public Node
{
	std::set<Song> children;
	QString mAlbum;
	virtual QString text() { return mAlbum; }
};

struct Artist : public Node
{
	std::set<Album> children;
	QString mArtist;
	virtual QString text() { return mArtist; }
};


}


struct KittenPlayer::Model::ModelPrivate
{
    std::set<Artist> artists;
};

KittenPlayer::Model::Model()
{
	d = new ModelPrivate;
}

QVariant KittenPlayer::Model::data(const QModelIndex &index, int role) const
{
}

Qt::ItemFlags KittenPlayer::Model::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant KittenPlayer::Model::headerData(
		int section, Qt::Orientation orientation, int role
	) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case 0:
			return tr("Title");
		case 1:
			return tr("Length");
		}
	}
	
	return QVariant();
}

QModelIndex KittenPlayer::Model::index(
		int row, int column,
		const QModelIndex &parent
	) const
{

}

QModelIndex KittenPlayer::Model::parent(const QModelIndex &child) const
{

}

int KittenPlayer::Model::rowCount(const QModelIndex &parent) const
{

}

int KittenPlayer::Model::columnCount(const QModelIndex &parent) const
{
	return 2;
}


static QString canonical(const QString &t)
{
	return t.toCaseFolded().simplified();
}

template<typename T>
static T& fold(std::set<T> &into, const QString &label)
{
	const QString artist = canonical(file.artist());
	T *branch=0;
	for (std::set<T>::iterator i = into.begin(); i != into.end(); ++i)
	{
		if (canonical(i->mArtist) == artist)
		{
			branch = &*i;
			break;
		}
	}
	
	if (!branch)
		branch = &* into.insert(as).first;
	return *branch;
}

void KittenPlayer::Model::addFile(const File &file)
{
	Artist &artist = fold(d->artists, file.artist());
	Album &album   = fold(artist.children, file.album());
	
	album.children.insert(Song(file));
}


// kate: space-indent off; replace-tabs off;

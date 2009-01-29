#ifndef MEOW_TREEVIEW_H
#define MEOW_TREEVIEW_H

#include <qtreewidget.h>

namespace Meow
{
class File;
class Player;
class Collection;

class TreeView : public QTreeWidget
{
	Q_OBJECT
	
	struct Node;
	struct Artist;
	struct Album;
	struct Song;
	
	class Selector;
	class LinearSelector;
	class RandomSongSelector;
	class RandomAlbumSelector;
	class RandomArtistSelector;
	
	class SongWidget;
	
	Player *const player;
	Collection *const collection;
	// mRandomPrevious is here so that when removing items, it's fast to check
	Song *mCurrent, *mRandomPrevious;
	
	Selector *mSelector;
	
	bool currentlyProcessingAutomaticExpansion;
	
public:
	TreeView(QWidget *parent, Player *player, Collection *collection);
	
	QList<File> selectedFiles();
	
	int childCount() const { return topLevelItemCount(); }
	QTreeWidgetItem *child(int i) const { return topLevelItem(i); }
	
	enum SelectorType
	{ // this order is significant
		Linear=0, RandomSong
	};
	
	void setSelector(SelectorType t);
	
	
public slots:
	void removeSelected();
	
	void previousSong();
	void nextSong();
	
protected slots:
	void addFile(const File &file);
	void reloadFile(const File &file);
	
	void playAt(QTreeWidgetItem *);
	void manuallyExpanded(QTreeWidgetItem *);

signals:
	void kdeActivated(QTreeWidgetItem*);
	void kdeContextMenu(QTreeWidgetItem*, const QPoint &at);
	void kdeContextMenu(const QPoint &at);

protected:
	virtual void mousePressEvent(QMouseEvent *e);

private:
	Song *findAfter(QTreeWidgetItem *);
	
	QTreeWidgetItem *siblingAfter(QTreeWidgetItem *item);
	QTreeWidgetItem *nonChildAfter(QTreeWidgetItem *item);
	inline static void callOnArtist(QTreeWidgetItem *parentOf, void (Artist::*function)());
};


}

#endif
 
// kate: space-indent off; replace-tabs off;

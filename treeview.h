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
	
	class SongWidget;
	struct Node;
	struct Artist;
	struct Album;
	struct Song;
	
	Player *const player;
	Collection *const collection;
	Song *mCurrent;
	
	bool currentlyProcessingAutomaticExpansion;
	
public:
	TreeView(QWidget *parent, Player *player, Collection *collection);
	
	
	int childCount() const { return topLevelItemCount(); }
	QTreeWidgetItem *child(int i) const { return topLevelItem(i); }
	
public slots:
	void addFile(const File &file);
	void removeSelected();
	
protected slots:
	void playAt(QTreeWidgetItem *);
	void nextSong();
	void manuallyExpanded(QTreeWidgetItem *);

signals:
	void kdeActivated(QTreeWidgetItem*);
	void kdeContextMenu(QTreeWidgetItem*, const QPoint &at);
	void kdeContextMenu(const QPoint &at);

protected:
	virtual void mousePressEvent(QMouseEvent *e);

private:
	Song *findAfter(QTreeWidgetItem *);

};


}

#endif
 
// kate: space-indent off; replace-tabs off;

#ifndef KITTENPLAYER_TREEVIEW_H
#define KITTENPLAYER_TREEVIEW_H

#include <qtreewidget.h>

namespace KittenPlayer
{
class File;
class Player;

class TreeView : public QTreeWidget
{
	Q_OBJECT
	
	class SongWidget;
	struct Node;
	struct Artist;
	struct Album;
	struct Song;
	
	Player *const player;
	Song *mCurrent;
	
	bool currentlyProcessingAutomaticExpansion;
	
public:
	TreeView(QWidget *parent, Player *player);
	
	
	int childCount() const { return topLevelItemCount(); }
	QTreeWidgetItem *child(int i) const { return topLevelItem(i); }
	
public slots:
	void addFile(const File &file);
	
protected slots:
	void playAt(QTreeWidgetItem *, int);
	void nextSong();
	void manuallyExpanded(QTreeWidgetItem *);

private:
	Song *findAfter(QTreeWidgetItem *);

};


}

#endif
 
// kate: space-indent off; replace-tabs off;

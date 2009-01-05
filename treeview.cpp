#include "treeview.h"
#include "player.h"
#include <db/file.h>

#include <qpainter.h>
#include <qitemdelegate.h>
#include <qevent.h>

#include <set>
#include <limits>

struct KittenPlayer::TreeView::Node : public QTreeWidgetItem
{
	bool autoExpanded:1;
	QColor oldColor;
	Node(TreeView *parent, int type)
		: QTreeWidgetItem(parent, type)
	{
		autoExpanded = false;
	}
	Node(Node *parent, int type)
		: QTreeWidgetItem(parent, type)
	{
		autoExpanded = false;
	}
	
	Node *parent() { return static_cast<Node*>(QTreeWidgetItem::parent()); }
	
	void setWasAutoExpanded(bool exp)
	{
		if (exp == autoExpanded)
			return;
		autoExpanded = exp;
		if (exp)
		{
			const QColor bg = treeWidget()->palette().base().color();
			QBrush brush = foreground(0);
			QColor text = oldColor = brush.color();
	
			int r = text.red() + bg.red();
			int g = text.green() + bg.green();
			int b = text.blue() + bg.blue();
			text.setRgb(r/2,g/2,b/2);
			
			brush.setColor(text);
			setForeground(0, brush);
		}
		else
		{
			QBrush b = foreground(0);
			b.setColor(oldColor);
			setForeground(0, b);
		}
	}
};

struct KittenPlayer::TreeView::Artist : public Node
{
	Artist(TreeView *parent, const QString &artist)
		: Node(parent, UserType+1)
	{
		setText(0, artist);
	}
};


struct KittenPlayer::TreeView::Album : public Node
{
public:
	Album(Artist *parent, const QString &album)
		: Node(parent, UserType+2)
	{
		setText(0, album);
	}
};

struct KittenPlayer::TreeView::Song : public Node
{
	const KittenPlayer::File mFile;
public:
	Song(Album *parent, const KittenPlayer::File &file)
		: Node(parent, UserType+3), mFile(file)
	{
		QString title = mFile.title();
		const QString track = mFile.track();
		if (!track.isEmpty())
			title = track + ". " + title;
		setText(0, title);
	}
	
	const KittenPlayer::File &file() const { return mFile; }
};


class KittenPlayer::TreeView::SongWidget : public QWidget
{
	TreeView *const mOwner;
	KittenPlayer::Player *const player;
public:
	SongWidget(QWidget *parent, TreeView *owner, KittenPlayer::Player *player)
		: QWidget(parent), mOwner(owner), player(player)
	{
		connect(player, SIGNAL(positionChanged(int)), SLOT(update()));
		
		setCursor(Qt::PointingHandCursor);
	}
	
	virtual void mouseReleaseEvent(QMouseEvent *event)
	{
		if (event->button() == Qt::LeftButton)
		{
			unsigned int len = player->currentLength();
			player->setPosition(event->pos().x()*len / width());
		}
	}

	virtual void paintEvent(QPaintEvent *)
	{
		QPainter p(this);
		QPainter *painter = &p;
		unsigned int pos = player->position();
		unsigned int len = player->currentLength();
		if (len == 0) len = std::numeric_limits<int>::max();
		QRect rect = this->rect();
		rect.setWidth(pos*rect.width()/len);
		
		const QColor hl = palette().highlight().color();
		QBrush brush = palette().base().color();
		QColor bg = brush.color();

		int r = bg.red() + hl.red();
		int g = bg.green() + hl.green();
		int b = bg.blue() + hl.blue();
		bg.setRgb(r/2,g/2,b/2);
		brush.setColor(bg);
		painter->fillRect(rect, bg);
		
		QFont font = painter->font();
		font.setUnderline(true);
		painter->setFont(font);
		
		painter->drawText(this->rect(), Qt::AlignVCenter, mOwner->mCurrent->text(0));
	}
};



KittenPlayer::TreeView::TreeView(QWidget *parent, Player *player)
	: QTreeWidget(parent), player(player)
{
	currentlyProcessingAutomaticExpansion = false;
	connect(
			this, SIGNAL(kdeActivated(QTreeWidgetItem*)),
			SLOT(playAt(QTreeWidgetItem*))
		);
	connect(
			this, SIGNAL(itemExpanded(QTreeWidgetItem*)), 
			SLOT(manuallyExpanded(QTreeWidgetItem*))
		);
	connect(player, SIGNAL(finished()), SLOT(nextSong()));
	
	headerItem()->setHidden(true);
	mCurrent = 0;
	
	// the SongWidget takes over painting
	class CurrentItemDelegate : public QItemDelegate
	{
		TreeView *const mOwner;
	public:
		CurrentItemDelegate(TreeView *owner)
			: QItemDelegate(owner), mOwner(owner) { }
			
		virtual void paint(
				QPainter *painter, const QStyleOptionViewItem &option,
				const QModelIndex &index
			) const
		{
			if (mOwner->itemFromIndex(index) != mOwner->mCurrent)
				QItemDelegate::paint(painter, option, index);
		}
		
	};
	setItemDelegate(new CurrentItemDelegate(this));
}

void KittenPlayer::TreeView::playAt(QTreeWidgetItem *_item)
{
	Song *const cur = findAfter(_item);
	if (!cur) return;

	std::set<Node*> expanded;
	if (mCurrent)
	{
		for (Node *up = mCurrent; up; up = up->parent())
		{
			if (up->autoExpanded)
				expanded.insert(up);
		}
	}
	currentlyProcessingAutomaticExpansion = true;
	for (Node *up = cur; up; up = up->parent())
	{
		if (up->autoExpanded)
			expanded.erase(up);
		up->setExpanded(true);
		up->setWasAutoExpanded(true);
	}
	for (std::set<Node*>::iterator i = expanded.begin(); i != expanded.end(); ++i)
	{
		(*i)->setExpanded(false);
		(*i)->setWasAutoExpanded(false);
	}
	currentlyProcessingAutomaticExpansion = false;
		
	removeItemWidget(mCurrent, 0);
	mCurrent = cur;
	player->play(cur->file());
	scrollToItem(cur);
	setItemWidget(cur, 0, new SongWidget(this, this, player));
}

void KittenPlayer::TreeView::nextSong()
{
	QTreeWidgetItemIterator it(mCurrent);
	Song *const next = findAfter(*++it);
	playAt(next);
}

void KittenPlayer::TreeView::manuallyExpanded(QTreeWidgetItem *_item)
{
	if (currentlyProcessingAutomaticExpansion)
		return;
	if (Node *n = dynamic_cast<Node*>(_item))
		n->setWasAutoExpanded(false);
}

void KittenPlayer::TreeView::mousePressEvent(QMouseEvent *e)
{
	QTreeWidgetItem *const at = itemAt(e->pos());
	if (e->button() == Qt::LeftButton && !itemWidget(at, 0))
		emit kdeActivated(at);
	else if (e->button() == Qt::RightButton)
		emit kdeContextMenu(at, e->pos());
}


KittenPlayer::TreeView::Song* KittenPlayer::TreeView::findAfter(QTreeWidgetItem *_item)
{
	for (QTreeWidgetItemIterator it(_item); *it; ++it)
	{
		QTreeWidgetItem *n = *it;
		if (Song *s = dynamic_cast<Song*>(n))
			return s;
	}
	
	return 0;
}

static QString canonical(const QString &t)
{
	return t.toCaseFolded().simplified();
}

template<typename Parent, typename Child>
static Child* fold(Parent *into, const QString &label)
{
	const QString clabel = canonical(label);
	const int children = into->childCount();
	for (int i=0; i != children; ++i)
	{
		QTreeWidgetItem *const ch = into->child(i);
		if (canonical(ch->text(0)) == clabel)
		{
			return static_cast<Child*>(ch);
			break;
		}
	}
	
	return new Child(into, label);
}

void KittenPlayer::TreeView::addFile(const File &file)
{
	Artist *artist = fold<TreeView, Artist>(this, file.artist());
	Album *album   = fold<Artist, Album>(artist, file.album());
	
	new Song(album, file);
}

// kate: space-indent off; replace-tabs off;

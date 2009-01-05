#include "treeview.h"
#include "player.h"
#include <db/file.h>

#include <qpainter.h>
#include <qitemdelegate.h>
#include <qevent.h>

#include <set>
#include <limits>

static void pad(QString &str)
{
	int len=str.length();
	int at = 0;
	int blocklen=0;

	static const int paddingsize=12;

	// not static for reason
	const QChar chars[paddingsize] =
	{
		QChar('0'), QChar('0'), QChar('0'), QChar('0'),
		QChar('0'), QChar('0'), QChar('0'), QChar('0'),
		QChar('0'), QChar('0'), QChar('0'), QChar('0')
	};

	for (int i=0; i < len; i++)
	{
		if (str[i].isNumber())
		{
			if (!blocklen)
				at = i;
			blocklen++;
		}
		else if (blocklen)
		{
			int pads=paddingsize;
			pads -= blocklen;
			str.insert(at, chars, pads);
			i += pads;
			blocklen = 0;
		}
	}
	if (blocklen)
	{
		int pads=paddingsize;
		pads -= blocklen;
		str.insert(at, chars, pads);
	}
}

struct KittenPlayer::TreeView::Node : public QTreeWidgetItem
{
	bool autoExpanded:1;
	QColor oldColor;
	Node(int type)
		: QTreeWidgetItem(type)
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
	Artist(const QString &artist)
		: Node(UserType+1)
	{
		setText(0, artist);
	}
};


struct KittenPlayer::TreeView::Album : public Node
{
public:
	Album(const QString &album)
		: Node(UserType+2)
	{
		setText(0, album);
	}
};

struct KittenPlayer::TreeView::Song : public Node
{
	const KittenPlayer::File mFile;
public:
	Song(const KittenPlayer::File &file)
		: Node(UserType+3), mFile(file)
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
	QTreeWidget::mousePressEvent(e);
	QModelIndex index = indexAt(e->pos());
	if (index.column() != 0) return;
	QTreeWidgetItem *const at = itemAt(e->pos());
	
	int indent=0;
	for (QTreeWidgetItem *up = at; up ; up = up->parent())
		indent++;
	
	if (e->pos().x() < indent*indentation())
		return;
	
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
	QString s = t.toCaseFolded().simplified();
	pad(s);
	return s;
}

static void insertSorted(
		QTreeWidgetItem *into, QTreeWidgetItem *child,
		const QString &childCanonicalLabel)
{
	const int children = into->childCount();
	int upper=children;
	int lower=0;
	while (upper!=lower)
	{ // a binary search
		int i = lower+(upper-lower)/2;
		QTreeWidgetItem *const ch = into->child(i);
		QString t = canonical(ch->text(0));
		if (childCanonicalLabel > t)
			lower = i+1;
		else
			upper = i;
	}
	into->insertChild(lower, child);
}

template<typename Child, typename Parent>
static Child* fold(Parent *into, const QString &label)
{
	const QString clabel = canonical(label);
	const int children = into->childCount();
	for (int i=0; i != children; ++i)
	{ // return the child that already exists
		QTreeWidgetItem *const ch = into->child(i);
		if (canonical(ch->text(0)) == clabel)
		{
			return static_cast<Child*>(ch);
			break;
		}
	}
	
	// or make a new one
	Child *const c = new Child(label);
	insertSorted(into, c, clabel);
	return c;
}

void KittenPlayer::TreeView::addFile(const File &file)
{
	Artist *artist = fold<Artist>(invisibleRootItem(), file.artist());
	Album *album   = fold<Album>(artist, file.album());
	
	Song *const song = new Song(file);
	
	insertSorted(album, song, canonical(song->text(0)));
}

// kate: space-indent off; replace-tabs off;

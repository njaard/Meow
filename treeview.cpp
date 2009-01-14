#include "treeview.h"
#include "player.h"
#include <db/file.h>
#include <db/collection.h>

#include <qpainter.h>
#include <qitemdelegate.h>
#include <qevent.h>
#include <qscrollbar.h>
#include <qcursor.h>
#include <qapplication.h>

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

struct Meow::TreeView::Node : public QTreeWidgetItem
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

struct Meow::TreeView::Artist : public Node
{
	Artist(const QString &artist)
		: Node(UserType+1)
	{
		setText(0, artist);
	}
};


struct Meow::TreeView::Album : public Node
{
public:
	Album(const QString &album)
		: Node(UserType+2)
	{
		setText(0, album);
	}
};

struct Meow::TreeView::Song : public Node
{
	const FileId mFileId;
public:
	Song(const Meow::File &file)
		: Node(UserType+3), mFileId(file.fileId())
	{
		setText(file);
	}
	
	void setText(const Meow::File &file)
	{
		QString title = file.title();
		const QString track = file.track();
		if (!track.isEmpty())
			title = track + ". " + title;
		Node::setText(0, title);
	}
	
	FileId fileId() const { return mFileId; }
	
};


class Meow::TreeView::SongWidget : public QWidget
{
	TreeView *const mOwner;
	Meow::Player *const player;
public:
	SongWidget(QWidget *parent, TreeView *owner, Meow::Player *player)
		: QWidget(parent), mOwner(owner), player(player)
	{
		connect(player, SIGNAL(positionChanged(int)), SLOT(update()));
		
		setCursor(Qt::PointingHandCursor);
	}
	
	QRect drawArea() const
	{
		QRect rect = this->rect();
		if (mOwner->horizontalScrollBar()->value() == 0)
		{
			rect.setWidth(rect.width() - mOwner->horizontalScrollBar()->maximum());
		}
		return rect;
	}
	
	virtual void mousePressEvent(QMouseEvent *event)
	{
		if (event->button() == Qt::LeftButton)
		{
			QRect rect = drawArea();
			unsigned int len = player->currentLength();
			player->setPosition(event->pos().x()*len / rect.width());
		}
		
		QPoint xlated = mapTo(mOwner, event->pos());
		xlated = mOwner->viewport()->mapFrom(mOwner, xlated);
		
		
		//forward this mouse event to the list
		QMouseEvent e2(
				event->type(),
				xlated,
				event->globalPos(), event->button(),
				event->buttons(), event->modifiers()
			);
		QApplication::sendEvent(mOwner->viewport(), &e2);
	}

	virtual void paintEvent(QPaintEvent *)
	{
		QPainter p(this);
		unsigned int pos = player->position();
		unsigned int len = player->currentLength();
		if (len == 0) len = std::numeric_limits<int>::max();
		
		QRect rect = drawArea();
		rect.setWidth(pos*rect.width()/len);
		
		const QColor hl = palette().highlight().color();
		QBrush brush = palette().base().color();
		QColor bg = brush.color();

		int r = bg.red() + hl.red();
		int g = bg.green() + hl.green();
		int b = bg.blue() + hl.blue();
		bg.setRgb(r/2,g/2,b/2);
		brush.setColor(bg);
		p.fillRect(rect, bg);
		
		QFont font = p.font();
		font.setUnderline(true);
		p.setFont(font);
		
		p.drawText(this->rect(), Qt::AlignVCenter, mOwner->mCurrent->text(0));
	}
};



Meow::TreeView::TreeView(QWidget *parent, Player *player, Collection *collection)
	: QTreeWidget(parent), player(player), collection(collection)
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
	setAlternatingRowColors(true);
	// required for keeping the current item under the cursor while adding items
	setVerticalScrollMode(ScrollPerPixel);
	
	connect(collection, SIGNAL(added(File)), SLOT(addFile(File)));
	connect(collection, SIGNAL(reloaded(File)), SLOT(reloadFile(File)));
}

QList<Meow::File> Meow::TreeView::selectedFiles()
{
	QList<File> files;
	QList<QTreeWidgetItem*> selected = selectedItems();
	for (QList<QTreeWidgetItem*>::iterator i = selected.begin(); i != selected.end(); ++i)
	{
		if (Song *s = dynamic_cast<Song*>(*i))
			files += collection->getSong(s->fileId());
	}
	return files;
}

void Meow::TreeView::playAt(QTreeWidgetItem *_item)
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
	File curFile = collection->getSong(cur->fileId());
	player->play(curFile);
	scrollToItem(cur);
	setItemWidget(cur, 0, new SongWidget(this, this, player));

}

void Meow::TreeView::nextSong()
{
	if (!mCurrent)
	{
		playAt(invisibleRootItem());
	}
	else
	{
		QTreeWidgetItemIterator it(mCurrent);
		playAt(*++it);
	}
}

void Meow::TreeView::previousSong()
{
	if (!mCurrent)
		return;
	QTreeWidgetItemIterator it(mCurrent);
	--it;
	for (; *it; --it)
	{
		QTreeWidgetItem *n = *it;
		if (Song *s = dynamic_cast<Song*>(n))
		{
			playAt(s);
			break;
		}
	}
}

void Meow::TreeView::manuallyExpanded(QTreeWidgetItem *_item)
{
	if (currentlyProcessingAutomaticExpansion)
		return;
	if (Node *n = dynamic_cast<Node*>(_item))
		n->setWasAutoExpanded(false);
}

void Meow::TreeView::mousePressEvent(QMouseEvent *e)
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
	{
		emit kdeContextMenu(at, e->globalPos());
		emit kdeContextMenu(e->globalPos());
	}
}

Meow::TreeView::Song* Meow::TreeView::findAfter(QTreeWidgetItem *_item)
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

void Meow::TreeView::addFile(const File &file)
{
	QPoint under = QCursor::pos();
	under = mapFromGlobal(under);
	
	// keep the current item under the cursor while adding items
	QTreeWidgetItem *const itemUnder = itemAt(under);
	int oldPos;
	if (itemUnder)
		oldPos = visualItemRect(itemUnder).top();

	Artist *artist = fold<Artist>(invisibleRootItem(), file.artist());
	Album *album   = fold<Album>(artist, file.album());
	
	Song *const song = new Song(file);
	
	insertSorted(album, song, canonical(song->text(0)));
	
	if (itemUnder)
	{
		// requires: setVerticalScrollMode(ScrollPerPixel); in the ctor
		QRect newArea = visualItemRect(itemUnder);
		//now scroll vertically so that newArea is oldArea
		int diff = newArea.top() - oldPos;
		QScrollBar *const vs = verticalScrollBar();
		vs->setValue(vs->value() + diff);
	}
	
}

static void deleteBranch(QTreeWidgetItem *parent)
{
	while (parent)
	{
		QTreeWidgetItem *p = parent->parent();
		if (parent->childCount() == 0)
			delete parent;
		else
			break;
		parent = p;
	}
}

void Meow::TreeView::reloadFile(const File &file)
{
	Song *s=0;
	//we have to find the song representing file
	
	QList<QTreeWidgetItem*> selected = selectedItems();
	// maybe the reloaded file is currently playing...
	if (mCurrent && mCurrent->fileId() == file.fileId())
		s = mCurrent;
	for (QList<QTreeWidgetItem*>::iterator i = selected.begin(); i != selected.end(); )
	{ // ... or selected
		if (Song *_s = dynamic_cast<Song*>(*i))
			if (_s->fileId() == file.fileId())
			{
				s = _s;
				break;
			}
	}
	
	if (!s)
	{ // find it the slow way, then
		for (QTreeWidgetItemIterator it(this); *it; ++it)
		{
			QTreeWidgetItem *n = *it;
			if (Song *_s = dynamic_cast<Song*>(n))
				if (_s->fileId() == file.fileId())
				{
					s = _s;
					break;
				}
		}
	}
	
	if (s == mCurrent)
		removeItemWidget(mCurrent, 0);
	
	// remove it
	QTreeWidgetItem *parent = s->parent();
	if (!parent) parent = invisibleRootItem();
	int index = parent->indexOfChild(s);
	parent->takeChild(index);
	deleteBranch(parent);
	
	// ok now insert it again
	Artist *artist = fold<Artist>(invisibleRootItem(), file.artist());
	Album *album   = fold<Album>(artist, file.album());
	
	insertSorted(album, s, canonical(s->text(0)));
	s->setText(file);
	if (s == mCurrent)
		setItemWidget(s, 0, new SongWidget(this, this, player));
	
}

void Meow::TreeView::removeSelected()
{
	QList<QTreeWidgetItem*> selected = selectedItems();
	
	// first pass, go over selected making sure it doesn't contain any
	// children of its own items. this is O(n²) but that's ok, because
	// n should be pretty small, and it will get smaller every iteration
	for (QList<QTreeWidgetItem*>::iterator i = selected.begin(); i != selected.end(); )
	{
		QList<QTreeWidgetItem*>::iterator prevI = i;
		QTreeWidgetItem*const item  = *i;
		++i;
		
		for (QTreeWidgetItem *up = item->parent(); up; up = up->parent())
		{
			if (selected.contains(up))
				selected.erase(prevI);
		}
	}
	
	QTreeWidgetItem *nextToBePlaying=0;
	// also consider the situation in which I delete the currently playing item
	for (QTreeWidgetItem *up = mCurrent; up; up = up->parent())
	{
		if (selected.contains(up))
		{
			// so as up is in the "to be deleted" list
			// then the first sibling of up should be playable
			// if it is also not in that list
			nextToBePlaying = up;
			do
			{
				nextToBePlaying = nonChildAfter(nextToBePlaying);
			} while (selected.contains(nextToBePlaying));
			break;
		}
	}
	if (nextToBePlaying)
	{
		mCurrent = 0;
		player->stop();
	}
	
	
	std::vector<FileId> files;
	// second pass: delete the stuff
	for (QList<QTreeWidgetItem*>::iterator i = selected.begin(); i != selected.end(); )
	{
		QTreeWidgetItem *const item = *i;
		++i; // access the next item before we delete the old one
		QTreeWidgetItem *const next = nonChildAfter(item);
		
		for (QTreeWidgetItemIterator it(item); *it != next; ++it)
		{
			if (Song *s = dynamic_cast<Song*>(*it))
				files.push_back(s->fileId());
		}
	}
	collection->remove(files);
	while (!selected.isEmpty())
	{
		QTreeWidgetItem *const item = selected.takeFirst();
		QTreeWidgetItem *const parent = item->parent();
		delete item;
		deleteBranch(parent);
	}
	
	if (nextToBePlaying)
		playAt(nextToBePlaying);
}

QTreeWidgetItem *Meow::TreeView::siblingAfter(QTreeWidgetItem *item)
{
	QTreeWidgetItem *parent = item->parent();
	if (!parent) parent = invisibleRootItem();
	int itemindex = parent->indexOfChild(item);
	if (itemindex+1 == parent->childCount())
		return 0;
	return parent->child(itemindex+1);
}

QTreeWidgetItem *Meow::TreeView::nonChildAfter(QTreeWidgetItem *item)
{
	QTreeWidgetItem *next;
	while (! (next = siblingAfter(item)))
	{
		item = item->parent();
		if (!item)
			return 0;
	}
	
	return next;
}


// kate: space-indent off; replace-tabs off;

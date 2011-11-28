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

#ifdef MEOW_WITH_KDE
#include <krandom.h>
#endif

#include <set>
#include <limits>
#include <iostream>

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
	bool wasAutoExpanded:1;
	Node(int type)
		: QTreeWidgetItem(type), mNumSongs(0)
	{
		wasAutoExpanded = false;
	}
	Node *parent() { return static_cast<Node*>(QTreeWidgetItem::parent()); }
	
	void setWasAutoExpanded(bool yes)
	{
		if (yes == wasAutoExpanded) return;
		wasAutoExpanded = yes;
		handleAutoExpansionOfChildren();
	}
	
	void handleAutoExpansionOfChildren()
	{
		const bool yes = wasAutoExpanded;
		const int children = childCount();
		for (int i=0; i < children; i++)
		{
			Node *const ch = static_cast<Node*>(child(i));
			ch->changeColorsToReflectAutoExpansion(yes);
		}
	}
	
	void changeColorsToReflectAutoExpansion()
	{
		if (parent())
			changeColorsToReflectAutoExpansion(parent()->wasAutoExpanded);
	}
	
	int numSongs() const { return mNumSongs; }

	void songAdded() { mNumSongs++; }
	void songRemoved() { mNumSongs--; }


private:
	void changeColorsToReflectAutoExpansion(bool parentState)
	{
		if (parentState)
		{
			const QColor bg = treeWidget()->palette().base().color();
			QBrush brush = foreground(0);
			QColor text = brush.color();
	
			int r = text.red() + bg.red();
			int g = text.green() + bg.green();
			int b = text.blue() + bg.blue();
			text.setRgb(r/2,g/2,b/2);
			
			brush.setColor(text);
			setForeground(0, brush);
		}
		else
		{
			QBrush b = treeWidget()->palette().text().color();
			setForeground(0, b);
		}
	}

	int mNumSongs;
};

struct Meow::TreeView::Artist : public Node
{
	Artist(const QString &artist)
		: Node(UserType+1)
	{
		setText(0, artist);
	}
	
private:
	int mNumSongs;
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


class Meow::TreeView::Selector
{
	TreeView *const mTree;
public:
	Selector(TreeView *tv) : mTree(tv) { }
	virtual ~Selector() { }

	virtual QTreeWidgetItem *nextSong()=0;
	virtual QTreeWidgetItem *previousSong()=0;

protected:
	TreeView *tree() { return mTree; }
	Song *current() { return mTree->mCurrent; }
};

class Meow::TreeView::LinearSelector : public Meow::TreeView::Selector
{
public:
	LinearSelector(TreeView *tv) : Selector(tv) { }
	virtual QTreeWidgetItem *nextSong()
	{
		if (!current())
			return tree()->invisibleRootItem();
		
		QTreeWidgetItemIterator it(tree()->mCurrent, QTreeWidgetItemIterator::NotHidden);
		return *++it;
	}
	virtual QTreeWidgetItem *previousSong()
	{
		if (!current())
			return 0;
		
		QTreeWidgetItemIterator it(current(), QTreeWidgetItemIterator::NotHidden);
		--it;
		for (; *it; --it)
		{
			QTreeWidgetItem *n = *it;
			if (Song *s = dynamic_cast<Song*>(n))
				return s;
		}
		return 0;
	}
};


class Meow::TreeView::RandomSongSelector : public Meow::TreeView::Selector
{
public:
	RandomSongSelector(TreeView *tv) : Selector(tv) { }
	virtual QTreeWidgetItem *nextSong()
	{
		int totalSongs=0;
		const int totalRoot = tree()->childCount();
		for (int i=0; i < totalRoot; i++)
		{
			if (Node *n = dynamic_cast<Node*>(tree()->child(i)))
				totalSongs += n->numSongs();
		}
#ifdef MEOW_WITH_KDE
		const int songIndex = KRandom::random() % totalSongs;
#else
		const int songIndex = rand() % totalSongs;
#endif
		int atSong=0;
		
		for (int i=0; i < totalRoot; i++)
		{
			Node *const node = dynamic_cast<Node*>(tree()->child(i));
			if (!node)
				continue;
				
			const int afterNode = atSong + node->numSongs();
			if (songIndex < afterNode)
			{
				QTreeWidgetItemIterator it(node);
				++i;
				for (; *it; ++it)
				{
					if (Song *s = dynamic_cast<Song*>(*it))
					{
						if (atSong == songIndex)
						{
							tree()->mRandomPrevious = tree()->mCurrent;
							return s;
						}
						atSong++;
					}
				}
				// should never get here
				std::cerr << "Missed one?" << std::endl;
			}
			else
			{
				atSong = afterNode;
			}
		}
		return 0;
	}
	virtual QTreeWidgetItem *previousSong()
	{
		Song *const p = tree()->mRandomPrevious;
		tree()->mRandomPrevious = 0;
		return p;
	}
};

template<class BranchType>
class Meow::TreeView::RandomBranchSelector : public Meow::TreeView::Selector
{
	// tree()->mRandomPrevious in this case stores the previous artist's last song
public:
	RandomBranchSelector(TreeView *tv) : Selector(tv) { }
	virtual QTreeWidgetItem *nextSong()
	{
		Song *cur = current();
		BranchType *curArtist = inside<BranchType>(cur);
		
		QTreeWidgetItemIterator it(cur);
		while (*++it && inside<BranchType>(*it) == curArtist)
		{
			if (Song *s = dynamic_cast<Song*>(*it))
				return s;
		}
		
		return randomBranch();
	}
	virtual QTreeWidgetItem *previousSong()
	{
		Song *cur = current();
		BranchType *curArtist = inside<BranchType>(cur);
		QTreeWidgetItemIterator it(cur);
		while (*--it && inside<BranchType>(*it) == curArtist)
		{
			if (Song *s = dynamic_cast<Song*>(*it))
				return s;
		}
		
		Song *const p = tree()->mRandomPrevious;
		tree()->mRandomPrevious = 0;
		return p;
	}

private:
	QTreeWidgetItem *randomBranch()
	{
		const int totalArtists = tree()->childCount();
		
#ifdef MEOW_WITH_KDE
		const int artistIndex = KRandom::random() % totalArtists;
#else
		const int artistIndex = rand() % totalArtists;
#endif
		
		tree()->mRandomPrevious = current();
		
		return tree()->child(artistIndex);
	}
};



class Meow::TreeView::SongWidget : public QWidget
{
	TreeView *const mOwner;
	Meow::Player *const player;
    
    bool mShowTime;
    
public:
	SongWidget(QWidget *parent, TreeView *owner, Meow::Player *player)
		: QWidget(parent), mOwner(owner), player(player), mShowTime(false)
	{
		connect(player, SIGNAL(positionChanged(int)), SLOT(update()));
		
		setCursor(Qt::PointingHandCursor);
		
		connect(player, SIGNAL(positionChanged(int)), SLOT(update()));
        connect(player, SIGNAL(lengthChanged(int)), SLOT(update()));
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
    
    virtual void enterEvent(QEvent*)
    {
		mShowTime = true;
		update();
    }
    virtual void leaveEvent(QEvent*)
    {
		mShowTime = false;
		update();
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
		
		const QString timeText
			= player->positionString() + "/" + player->lengthString();
		
		QString titleText
			= mOwner->mCurrent->text(0);
		
		int timeTextSize=0;
		
		QRect titleArea = rect();
		QRect timeTextArea;
		
		if (mShowTime)
		{
			timeTextSize = p.fontMetrics().width(timeText);
			timeTextSize += p.fontMetrics().averageCharWidth()*2;
			
			titleArea.setWidth(titleArea.width()-timeTextSize);
			
			titleText = p.fontMetrics().elidedText(
					titleText, Qt::ElideRight, titleArea.width()
				);
			
			timeTextArea = rect();
		}
		
		const unsigned int pos = player->position();
		unsigned int len = player->currentLength();
		if (len == 0) len = std::numeric_limits<int>::max();
		
		QRect timeBoxRect = rect();
		timeBoxRect.setWidth(pos*timeBoxRect.width()/len);
		
		const QColor hl = palette().highlight().color();
		QBrush brush = palette().base().color();
		QColor bg = brush.color();

		int r = bg.red() + hl.red();
		int g = bg.green() + hl.green();
		int b = bg.blue() + hl.blue();
		bg.setRgb(r/2,g/2,b/2);
		brush.setColor(bg);
		p.fillRect(timeBoxRect, bg);
		
		QFont font = p.font();
		
		if (mShowTime)
		{
			p.drawText(timeTextArea, Qt::AlignRight|Qt::AlignVCenter, timeText);
		}
		
		font.setUnderline(true);
		p.setFont(font);
		p.drawText(titleArea, Qt::AlignVCenter, titleText);
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
	mRandomPrevious = 0;
	
	mSelector = 0;
	setSelector(Linear);
	
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
	
	// tooltips
	setMouseTracking(true);
	
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

QList<QString> Meow::TreeView::selectedAlbums()
{
	QList<QString> albums;
	QList<QTreeWidgetItem*> selected = selectedItems();
	for (QList<QTreeWidgetItem*>::iterator i = selected.begin(); i != selected.end(); ++i)
	{
		if (Album *s = dynamic_cast<Album*>(*i))
			albums += s->text(0);
	}
	return albums;
}

void Meow::TreeView::setSelector(SelectorType t)
{
	delete mSelector;
	if (t == Linear)
		mSelector = new LinearSelector(this);
	else if (t == RandomSong)
		mSelector = new RandomSongSelector(this);
	else if (t == RandomAlbum)
		mSelector = new RandomAlbumSelector(this);
	else if (t == RandomArtist)
		mSelector = new RandomArtistSelector(this);
}

void Meow::TreeView::playFirst()
{
	if (QTreeWidgetItem *item = invisibleRootItem())
		playAt(item);
}

void Meow::TreeView::clear()
{
	player->stop();
	mCurrent = 0;
	mRandomPrevious = 0;
	QTreeWidget::clear();
}

void Meow::TreeView::playAt(QTreeWidgetItem *_item)
{
	Song *const cur = findAfter(_item);
	if (!cur) return;

	// see who is already auto-expanded
	std::set<Node*> previouslyExpanded;
	if (mCurrent)
	{
		for (Node *up = mCurrent; up; up = up->parent())
		{
			if (up->wasAutoExpanded)
				previouslyExpanded.insert(up);
		}
	}

	currentlyProcessingAutomaticExpansion = true;

	for (Node *up = cur; up; up = up->parent())
	{
		if (up->wasAutoExpanded)
		{ // remove those from the list that will remain autoexpanded
			previouslyExpanded.erase(up);
		}
		else
		{ // and actually expand the rest
			up->setExpanded(true);
			up->setWasAutoExpanded(true);
		}
	}

	// those that were expanded but no longer need to be are collapsed
	for (
			std::set<Node*>::iterator i = previouslyExpanded.begin();
			i != previouslyExpanded.end(); ++i
		)
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
	if (QTreeWidgetItem *item = mSelector->nextSong())
		playAt(item);
}

void Meow::TreeView::filter(const QString &text)
{
	QTreeWidgetItem *item = invisibleRootItem();
	if (!item) return;
	
	for (int i=0; i < item->childCount(); i++)
		filter(item->child(i), text);
}

void Meow::TreeView::stopFilter()
{
	filter("");
}

bool Meow::TreeView::filter(QTreeWidgetItem *branch, const QString &text)
{
	if (text.isEmpty() || branch->text(0).contains(text, Qt::CaseInsensitive))
	{
		branch->setHidden(false);
		for (int i=0; i < branch->childCount(); i++)
			filter(branch->child(i), "");
		return true;
	}
	else
	{
		bool has = false;
		for (int i=0; i < branch->childCount(); i++)
			has = filter(branch->child(i), text) || has;
		branch->setHidden(!has);
		return has;
	}
}

void Meow::TreeView::previousSong()
{
	if (QTreeWidgetItem *item = mSelector->previousSong())
		playAt(item);
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
	if (!_item)
		return 0;
	
	for (QTreeWidgetItemIterator it(_item, QTreeWidgetItemIterator::NotHidden); *it; ++it)
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
	c->changeColorsToReflectAutoExpansion();
	return c;
}

void Meow::TreeView::addFile(const File &file)
{
	QPoint under = QCursor::pos();
	under = mapFromGlobal(under);
	
	// keep the current item under the cursor while adding items
	QTreeWidgetItem *const itemUnder = itemAt(under);
	int oldPos=0;
	if (itemUnder)
		oldPos = visualItemRect(itemUnder).top();

	Song *const song = new Song(file);

	if (file.displayByAlbum())
	{
		Album *album   = fold<Album>(invisibleRootItem(), file.album());
		insertSorted(album, song, canonical(song->text(0)));
		album->songAdded();
	}
	else
	{
		Artist *artist = fold<Artist>(invisibleRootItem(), file.artist());
		Album *album  = fold<Album>(artist, file.album());
		insertSorted(album, song, canonical(song->text(0)));
		artist->songAdded();
	}
	
	song->changeColorsToReflectAutoExpansion();
	
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

template <class T>
inline T* Meow::TreeView::inside(QTreeWidgetItem *p)
{
	while (p)
	{
		if (T* t = dynamic_cast<T*>(p))
			return t;
		p = p->parent();
	}
	return 0;
}

template <class T, class RetType>
inline RetType Meow::TreeView::callOn(QTreeWidgetItem *p, RetType (T::*function)())
{
	if (T *t = inside<T>(p))
		return (t->*function)();
	
	return RetType();
}

void Meow::TreeView::reloadFile(const File &file)
{
	Song *s=0;
	//we have to find the song representing file
	
	QList<QTreeWidgetItem*> selected = selectedItems();
	// maybe the reloaded file is currently playing...
	if (mCurrent && mCurrent->fileId() == file.fileId())
		s = mCurrent;
	for (QList<QTreeWidgetItem*>::iterator i = selected.begin(); i != selected.end(); ++i)
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
	if (parent)
		callOn(parent, &Artist::songRemoved);
	if (!parent) parent = invisibleRootItem();
	int index = parent->indexOfChild(s);
	parent->takeChild(index);
	deleteBranch(parent);
	
	// ok now insert it again
	if (file.displayByAlbum())
	{
		Album *album   = fold<Album>(invisibleRootItem(), file.album());
		insertSorted(album, s, canonical(s->text(0)));
		album->songAdded();
	}
	else
	{
		Artist *artist = fold<Artist>(invisibleRootItem(), file.artist());
		Album *album  = fold<Album>(artist, file.album());
		insertSorted(album, s, canonical(s->text(0)));
		artist->songAdded();
	}
	
	s->changeColorsToReflectAutoExpansion();
	s->setText(file);
	if (s == mCurrent)
		setItemWidget(s, 0, new SongWidget(this, this, player));
}

static QTreeWidgetItem* hasAsParent(QTreeWidgetItem *item, const QList<QTreeWidgetItem*> &oneOfThese)
{
	for (QTreeWidgetItem *up = item; up; up = up->parent())
	{
		if (oneOfThese.contains(up))
			return up;
	}
	return 0;
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
	
	QTreeWidgetItem *nextToBePlaying;
	// also consider the situation in which I delete the currently playing item
	if ((nextToBePlaying = hasAsParent(mCurrent, selected)))
	{
		// so as up is in the "to be deleted" list
		// then the first sibling of up should be playable
		// if it is also not in that list
		do
		{
			nextToBePlaying = nonChildAfter(nextToBePlaying);
		} while (selected.contains(nextToBePlaying));
		mCurrent = 0;
		player->stop();
	}
	if (mRandomPrevious && hasAsParent(mRandomPrevious, selected))
		mRandomPrevious = 0;
	
	
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
		if (parent)
		{
			if (dynamic_cast<Song*>(item))
				callOn(parent, &Artist::songRemoved);
		}

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

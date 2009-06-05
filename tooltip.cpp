#include "tooltip.h"
#include "player.h"

#include <qgridlayout.h>
#include <qlabel.h>
#include <qtextdocument.h>
#include <qevent.h>
#include <qdir.h>
#include <qtimer.h>
#include <qapplication.h>

#include <klocale.h>

static Meow::Tooltip *mOnlyOne=0;


Meow::TooltipWidget::TooltipWidget(
		const File &file, const Player *player, QWidget *parent
	)
	: QWidget(parent)
	, mFile(file), player(player)
{
	setMouseTracking(true);
	QGridLayout *l = new QGridLayout(this);
	
	{ // album artwork
		QStringList filters;
		filters
			<< "cover.jpeg" << "cover.jpg" << "cover.png"
			<< "album.jpeg" << "album.jpg" << "album.png"
			<< "front.jpeg" << "front.jpg" << "front.png"
			<< "folder.jpeg" << "folder.jpg" << "folder.png"
			<< "*.png" << "*.jpg" << "*.jpeg";
			
		const QDir fileDir = QFileInfo(file.file()).dir();
		const QStringList entries = fileDir.entryList(filters);
		
		if (!entries.isEmpty())
		{
			QLabel *image = new QLabel(this);
			
			QImage im(fileDir.filePath(entries[0]));
			if (!im.isNull())
			{
				im = im.scaledToWidth(128);
				image->setPixmap(QPixmap::fromImage(im));
				
				l->addWidget(image, 0, 0, 4, 1);
			}
		}
	}
	
	{ // other details
		int row=0;
		QWidget *w;
		
		w = new QLabel("<qt><big>" + Qt::escape(file.title()) + "</big></qt>", this);
		l->addWidget(w, row++, 1);
		
		w = new QLabel(
				"<qt>"
					+ i18n("From <b>%1</b>", Qt::escape(file.album()))
					+ "</qt>"
				, this);
		l->addWidget(w, row++, 1);
		
		w = new QLabel(
				"<qt>"
					+ i18n("By <b>%1</b>", Qt::escape(file.artist()))
					+ "</qt>"
				, this);
		l->addWidget(w, row++, 1);
		
		w = mLengthInfo = new QLabel(this);
		l->addWidget(w, row++, 1);
	}
	
	connect(player, SIGNAL(positionChanged(int)), SLOT(updateLengthInfo()));
	connect(player, SIGNAL(lengthChanged(int)), SLOT(updateLengthInfo()));
	connect(player, SIGNAL(currentItemChanged(File)), SLOT(updateLengthInfo()));
}

void Meow::TooltipWidget::updateLengthInfo()
{
	if (player->currentFile() == mFile)
	{
		mLengthInfo->setText(
				player->positionString() + "/" + player->lengthString()
			);
	}
}

void Meow::TooltipWidget::mouseMoveEvent(QMouseEvent *event)
{
	QPoint xlated = mapTo(parentWidget(), event->pos());
	xlated = parentWidget()->mapFrom(parentWidget(), xlated);
	
	//forward this mouse event to the list
	QMouseEvent e2(
			event->type(),
			xlated,
			event->globalPos(), event->button(),
			event->buttons(), event->modifiers()
		);
	QApplication::sendEvent(parentWidget(), &e2);
}

Meow::Tooltip::Tooltip(
		const QRect &viewObjectRect,
		const File &file, const Player *player, QWidget *parent)
	: KPassivePopup(parent)
	, viewObjectRect(viewObjectRect.adjusted(0, -1, 0, 1))
		// necessary to keep from flickering on the border
	, fileId(file.fileId())
	, ownerWidget(parent)
{
	destroySoon = 0;
	
	setMouseTracking(true);
	
	setView(new TooltipWidget(file, player, this));
	setTimeout(0);
	
	show(viewObjectRect.bottomLeft());
	
	parent->installEventFilter(this);
}



void Meow::Tooltip::create(
		const QRect &viewObjectRect,
		const File &file, const Player *player, QWidget *parent
	)
{
	if (mOnlyOne && file.fileId() == mOnlyOne->fileId)
		return;
	destroy();
	mOnlyOne = new Tooltip(viewObjectRect, file, player, parent);
}

void Meow::Tooltip::destroy()
{
	if (mOnlyOne)
	{
		mOnlyOne->deleteLater();
		mOnlyOne = 0;
	}
}

Meow::Tooltip::~Tooltip()
{
	if ( mOnlyOne == this )
		mOnlyOne = 0;
}

void Meow::Tooltip::mouseMoveEvent(QMouseEvent *event)
{
	if (!viewObjectRect.contains(event->globalPos()))
		deleteLater();
}

void Meow::Tooltip::enterEvent(QEvent *event)
{
	delete destroySoon;
	destroySoon = 0;
	KPassivePopup::enterEvent(event);
}

bool Meow::Tooltip::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == ownerWidget && event->type() == QEvent::Leave)
	{
		if (!underMouse())
		{
			// the mouse may have passed from the widget
			// to the tooltip, but that doesn't mean it should be hidden
			// by its leave event, make a timer to do so later
			delete destroySoon;
			destroySoon = new QTimer(this);
			connect(destroySoon, SIGNAL(timeout()), SLOT(deleteLater()));
			destroySoon->start(1000);
		}
	}
	return KPassivePopup::eventFilter(obj, event);
}

// kate: space-indent off; replace-tabs off;


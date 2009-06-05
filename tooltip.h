#ifndef MEOW_TOOLTIP_H
#define MEOW_TOOLTIP_H

#include <db/file.h>

#include <kpassivepopup.h>

class QLabel;

namespace Meow
{

class Player;

class TooltipWidget : public QWidget
{
	Q_OBJECT
	const File mFile;
	const Player *const player;
	QLabel *mLengthInfo;
	
public:
	TooltipWidget(const File &file, const Player *player, QWidget *parent);

protected:
	void mouseMoveEvent(QMouseEvent *event);

private slots:
	void updateLengthInfo();
};


class Tooltip : public KPassivePopup
{
	const QRect viewObjectRect;
	const FileId fileId;
	QWidget *const ownerWidget;
	
	QTimer *destroySoon;
	
	Tooltip(
			const QRect &viewObjectRect,
			const File &file, const Player *player, QWidget *parent
		);
public:
	~Tooltip();
	
	static void create(
			const QRect &viewObjectRect,
			const File &file, const Player *player, QWidget *parent
		);
	static void destroy();
	
protected:
	void mouseMoveEvent(QMouseEvent *event);
	void enterEvent(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
};

}

// kate: space-indent off; replace-tabs off;

#endif

#ifndef MEOW_SCROBBLE_H
#define MEOW_SCROBBLE_H

#include <qwidget.h>

#include "configdialog.h"
namespace KIO
{
class Job;
}

namespace Meow
{

class Player;
class File;
class Scrobble;

class ScrobbleConfigure : public ConfigWidget
{
	Q_OBJECT
	struct ScrobbleConfigurePrivate;
	ScrobbleConfigurePrivate *d;

public:
	ScrobbleConfigure(QWidget *parent, Scrobble *scrobble);
	~ScrobbleConfigure();
	
	virtual void load();
	virtual void apply();

private slots:
	void setEnablement(bool);
};


class Scrobble : public QObject
{
	Q_OBJECT
	struct ScrobblePrivate;
	ScrobblePrivate *d;

public:
	enum HandshakeState
	{
		HandshakeOk,
		HandshakeClientBanned,
		HandshakeAuth,
		HandshakeTime,
		HandshakeFailure
	};
	
	Scrobble(QObject *parent, Player *player);
	~Scrobble();
	
	bool isEnabled() const;
	void setEnabled(bool);
	
	QString username() const;
	QString password() const;
	
	void setUsername(const QString &);
	void setPassword(const QString &);

public slots:
	void begin();
	
signals:
	void handshakeState(HandshakeState error);

private slots:
	void announceNowPlaying(const File &file);
	void announceNowPlayingFromQueue();

	void handshakeData(KIO::Job*, const QByteArray &data);
	void slotHandshakeResult();
	void nowPlayingData(KIO::Job*, const QByteArray &data);
	void nowPlayingResult();
	
};



}

#endif
 
// kate: space-indent off; replace-tabs off;

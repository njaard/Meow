#ifndef MEOW_SCROBBLE_H
#define MEOW_SCROBBLE_H

#include <qobject.h>

namespace KIO
{
	class Job;
}

namespace Meow
{

class Player;
class File;

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

signals:
	void handshakeState(HandshakeState error);

private slots:
	void begin();
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

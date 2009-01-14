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
class Collection;

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
	
	Scrobble(QObject *parent);
	Scrobble(QWidget *parent, Player *player, Collection *collection);
	~Scrobble();
	
	bool isEnabled() const;
	void setEnabled(bool);
	
	QString username() const;
	
	void setUsername(const QString &);
	void setPassword(const QString &);

public slots:
	void begin();
	
signals:
	void handshakeState(Scrobble::HandshakeState error);

private slots:
	void currentItemChanged(const File &file);
	void announceNowPlayingFromQueue();
	void sendSubmissions();
	void sendSubmissionsRetry();

	void handshakeData(KIO::Job*, const QByteArray &data);
	void slotHandshakeResult();
	void nowPlayingData(KIO::Job*, const QByteArray &data);
	void nowPlayingResult();
	void submissionData(KIO::Job*, const QByteArray &data);
	void submissionResult();
	
	void lastSongFinishedPlaying();
	void stopCountingTime();
	void startCountingTimeAgain();
};


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
	void verify();
	void showResults(Scrobble::HandshakeState state);
};


}

#endif
 
// kate: space-indent off; replace-tabs off;

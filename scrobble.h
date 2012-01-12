#ifndef MEOW_SCROBBLE_H
#define MEOW_SCROBBLE_H

#include <qwidget.h>

#include "configdialog.h"

class QDomDocument;
class QDomElement;

namespace KIO
{
class Job;
}

namespace Meow
{

class Player;
class File;
class Collection;

class ScrobbleSession : public QObject
{
	Q_OBJECT

	struct ScrobbleSessionPrivate;
	ScrobbleSessionPrivate *d;
	typedef QMap<QString,QString> Query;
	
public:
	enum HandshakeState
	{
		HandshakeOk,
		HandshakeClientBanned,
		HandshakeAuth,
		HandshakeFailure
	};

	ScrobbleSession(QObject *parent);
	~ScrobbleSession();

	void startSession(const QString &username, const QString &passwordMd5);
	void submitTracks(const QList<QStringList> trackKeys);
	void nowPlaying(const QStringList trackKeys);
	
signals:
	void submitCompleted(bool success);
	void handshakeState(ScrobbleSession::HandshakeState error);
	
private:
	void startSessionRes(QDomElement root);
	void submitTrackRes(QDomElement root);
	void nowPlayingRes(QDomElement root);

	void error(QDomElement root, const QString &e);
	void invalidResponse(QDomElement root);
	
private:
	void makeRequest(bool post, Query &query, void (ScrobbleSession::*response)(QDomElement));

private slots:
#ifdef MEOW_WITH_KDE
	void handshakeData(KIO::Job*, const QByteArray &data);
	void slotHandshakeResult();
#else
	void handshakeData();
	void slotHandshakeResult();
#endif
};

class Scrobble : public QObject
{
	Q_OBJECT
	struct ScrobblePrivate;
	ScrobblePrivate *d;

public:
	
	Scrobble(QWidget *parent, Player *player, Collection *collection);
	~Scrobble();
	
	bool isEnabled() const;
	void setEnabled(bool);
	
	QString username() const;
	QString passwordIfKnown() const;
	
	void setUsername(const QString &);
	void setPassword(const QString &);

private:
	QStringList trackInfo(File f);
	
public slots:
	void begin();
	
private slots:
	void currentItemChanged(const File &file);
	void announceNowPlaying();
	void sendSubmissions();
	void sendSubmissionsRetry();
	void submitCompleted(bool success);

	void lastSongFinishedPlaying();
	void stopCountingTime();
	void startCountingTimeAgain();
	
	void knowLengthOfCurrentSong(int msec);
	
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
	void showResults(ScrobbleSession::HandshakeState state);
};


}

#endif
 
// kate: space-indent off; replace-tabs off;

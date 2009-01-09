#include "scrobble.h"
#include "player.h"
#include "db/file.h"

#include <kurl.h>
#include <kio/job.h>

#include <qprocess.h>
#include <qbytearray.h>
#include <qtimer.h>
#include <qdatetime.h>

#include <iostream>

static const char handshakeUrl[] = "http://post.audioscrobbler.com/";
static const char clientId[] = "tst";
static const char clientVersion[] = "1.0";
static const char clientProtocol[] = "1.2.1";

static QByteArray md5(const QByteArray &data)
{
	QStringList args;
	args << "-";
	QProcess proc;
	proc.start("md5sum", args);
	proc.write(data);
	proc.closeWriteChannel();
	if (!proc.waitForFinished())
		return false;
	QByteArray sum = proc.readAll();
	int x;
	if ( (x = sum.indexOf(' ')) == -1)
		if ( (x = sum.indexOf('\t')) == -1)
			return QByteArray();
	return sum.left(x);
}


struct Meow::Scrobble::ScrobblePrivate
{
	Player *player;
	
	QString username;
	QString password;
	
	QByteArray sessionId;
	QByteArray recievedData;
	
	KUrl nowPlaying;
	KUrl submission;
	
	QList<File> nowPlayingQueue;
	
};

Meow::Scrobble::Scrobble(QObject *parent, Player *player)
	: QObject(parent)
{
	d = new ScrobblePrivate;
	d->player = player;
	d->username = "njaard";
	d->password = "";
	
	QTimer::singleShot(10, this, SLOT(begin()));
}

Meow::Scrobble::~Scrobble()
{
	delete d;
}

void Meow::Scrobble::begin()
{
	d->recievedData.clear();
	
	QString timestamp = QString::number(QDateTime::currentDateTime().toTime_t());
	
	QString authToken;
	authToken = md5(d->password.toUtf8()) + timestamp;
	authToken = md5(authToken.toUtf8());

	KUrl handshake(handshakeUrl);
	handshake.addQueryItem("hs", "true");
	handshake.addQueryItem("p", clientProtocol);
	handshake.addQueryItem("c", clientId);
	handshake.addQueryItem("v", clientVersion);
	handshake.addQueryItem("u", d->username);
	handshake.addQueryItem("t", timestamp);
	handshake.addQueryItem("a", authToken);
	
	KIO::TransferJob *job = KIO::http_post(handshake, QByteArray(), KIO::HideProgressInfo);
	connect(job, SIGNAL(data(KIO::Job*, QByteArray)), SLOT(handshakeData(KIO::Job*, QByteArray)));
	connect(job, SIGNAL(result(KJob*)), SLOT(slotHandshakeResult()));
}

void Meow::Scrobble::handshakeData(KIO::Job*, const QByteArray &data)
{
	d->recievedData += data;
}

void Meow::Scrobble::slotHandshakeResult()
{
	QList<QByteArray> lines = d->recievedData.split('\n');
	if (lines.size() < 1)
	{
		emit handshakeState(HandshakeFailure);
		std::cerr << "Meow: scrobbler session: major error" << std::endl;
		return;
	}
	
	std::cerr << "Meow: scrobbler session: " << lines[0].data() << std::endl;
	
	if (lines[0] == "OK")
		emit handshakeState(HandshakeOk);
	else
	{
		if (lines[0] == "BANNED")
			emit handshakeState(HandshakeClientBanned);
		else if (lines[0] == "BADAUTH")
			emit handshakeState(HandshakeAuth);
		else if (lines[0] == "BADTIME")
			emit handshakeState(HandshakeTime);
		else
			emit handshakeState(HandshakeFailure);
		return;
	}
	
	d->sessionId = lines[1];
	
	d->nowPlaying = KUrl(lines[2]);
	d->submission = KUrl(lines[3]);
	
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(announceNowPlaying(File)));
}

void Meow::Scrobble::announceNowPlaying(const File &file)
{
	d->nowPlayingQueue += file;
	if (d->nowPlayingQueue.count() > 1)
		announceNowPlayingFromQueue();
}

void Meow::Scrobble::announceNowPlayingFromQueue()
{
	File file = d->nowPlayingQueue.takeLast();
	d->nowPlayingQueue.clear();
	
	KUrl np = d->nowPlaying;
	np.addQueryItem("s", d->sessionId);
	np.addQueryItem("a", file.artist());
	np.addQueryItem("t", file.title());
	np.addQueryItem("b", file.album());
	np.addQueryItem("l", QString::number(d->player->currentLength()));
	np.addQueryItem("n", file.track());
	np.addQueryItem("m", "");
	
	d->recievedData.clear();
	KIO::TransferJob *job = KIO::http_post(np, QByteArray(), KIO::HideProgressInfo);
	connect(job, SIGNAL(data(KIO::Job*, QByteArray)), SLOT(nowPlayingData(KIO::Job*, QByteArray)));
	connect(job, SIGNAL(result(KJob*)), SLOT(nowPlayingResult()));
}

void Meow::Scrobble::nowPlayingData(KIO::Job*, const QByteArray &data)
{
	d->recievedData += data;
	if (d->nowPlayingQueue.count() > 0)
		announceNowPlayingFromQueue();
}

void Meow::Scrobble::nowPlayingResult()
{
	QList<QByteArray> lines = d->recievedData.split('\n');
	if (lines.size() < 1)
	{
		std::cerr << "Meow: scrobbler now playing: major error" << std::endl;
		return;
	}
	
	std::cerr << "Meow: scrobbler now playing: " << lines[0].data() << std::endl;
}

// kate: space-indent off; replace-tabs off;

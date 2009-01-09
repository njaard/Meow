#include "scrobble.h"
#include "player.h"
#include "db/file.h"

#include <kurl.h>
#include <kio/job.h>
#include <klocale.h>

#include <qprocess.h>
#include <qbytearray.h>
#include <qtimer.h>
#include <qdatetime.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qgridlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>

#include <iostream>

struct Meow::ScrobbleConfigure::ScrobbleConfigurePrivate
{
	Scrobble *scrobble;
	
	QCheckBox *isEnabled;
	QLineEdit *username, *password;
	QLabel *diagnostics;
	QPushButton *test;
};

Meow::ScrobbleConfigure::ScrobbleConfigure(QWidget *parent, Scrobble *scrobble)
	: ConfigWidget(parent)
{
	d = new ScrobbleConfigurePrivate;
	d->scrobble = scrobble;
	
	QGridLayout *layout = new QGridLayout(this);
	
	d->isEnabled = new QCheckBox(
			i18n("&Enable scrobbling with AudioScrobbler"),
			this
		);
	layout->addWidget(d->isEnabled, 0, 0, 1, 2);
	
	{
		QLabel *label = new QLabel(i18n("&Username"), this);
		layout->addWidget(label, 1, 0);
		
		d->username = new QLineEdit(this);
		label->setBuddy(d->username);
		layout->addWidget(d->username, 1, 1);
	}
	{
		QLabel *label = new QLabel(i18n("&Password"), this);
		layout->addWidget(label, 2, 0);
		
		d->password = new QLineEdit(this);
		d->password->setEchoMode(QLineEdit::Password);
		label->setBuddy(d->password);
		layout->addWidget(d->password, 2, 1);
	}
	
	{
		QHBoxLayout *rowlayout = new QHBoxLayout;
		layout->addLayout(rowlayout, 3, 0, 1, 2);
		
		d->diagnostics = new QLabel(this);
		d->test = new QPushButton(i18n("&Check Login"), this);
		
		rowlayout->addWidget(d->diagnostics);
		rowlayout->addWidget(d->test);
	}
	connect(d->isEnabled, SIGNAL(toggled(bool)), SLOT(setEnablement(bool)));
	setEnablement(false);
	d->isEnabled->setChecked(false);
}

Meow::ScrobbleConfigure::~ScrobbleConfigure()
{
	delete d;
}

void Meow::ScrobbleConfigure::load()
{
	d->isEnabled->setChecked(d->scrobble->isEnabled());
	d->username->setText(d->scrobble->username());
	d->password->setText(d->scrobble->password());
}

void Meow::ScrobbleConfigure::apply()
{
	d->scrobble->setEnabled(d->isEnabled->isChecked());
	d->scrobble->setUsername(d->username->text());
	d->scrobble->setPassword(d->username->text());
	if (d->isEnabled->isChecked())
		d->scrobble->begin();
}


void Meow::ScrobbleConfigure::setEnablement(bool on)
{
	d->username->setEnabled(on);
	d->password->setEnabled(on);
	d->diagnostics->setEnabled(on);
	d->test->setEnabled(on);
	
	modified();
}


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
	
	bool isEnabled;
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
	d->isEnabled = false;
	
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(announceNowPlaying(File)));
}

Meow::Scrobble::~Scrobble()
{
	delete d;
}

bool Meow::Scrobble::isEnabled() const
{
	return d->isEnabled;
}

void Meow::Scrobble::setEnabled(bool en)
{
	d->isEnabled = en;
}

QString Meow::Scrobble::username() const
{
	return d->username;
}

QString Meow::Scrobble::password() const
{
	return d->password;
}

void Meow::Scrobble::setUsername(const QString &u)
{
	d->username = u;
}

void Meow::Scrobble::setPassword(const QString &p)
{
	d->username = p;
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
}

void Meow::Scrobble::announceNowPlaying(const File &file)
{
	if (!isEnabled())
		return;

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

#include "scrobble.h"
#include "player.h"
#include "db/file.h"
#include "db/collection.h"

#include <kurl.h>
#include <kio/job.h>
#include <klocale.h>
#include <kconfig.h>
#include <kconfiggroup.h>

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
		connect(d->test, SIGNAL(clicked()), SLOT(verify()));
		
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
	d->scrobble->setPassword(d->password->text());
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


void Meow::ScrobbleConfigure::verify()
{
	Scrobble *scr = new Scrobble(this, 0, 0);
	scr->setEnabled(false);
	scr->setUsername(d->username->text());
	scr->setPassword(d->password->text());
	connect(
			scr, SIGNAL(handshakeState(Scrobble::HandshakeState)), 
			SLOT(showResults(Scrobble::HandshakeState))
		);
	connect(
			scr, SIGNAL(handshakeState(Scrobble::HandshakeState)),
			scr, SLOT(deleteLater())
		);
	scr->begin();
}

void Meow::ScrobbleConfigure::showResults(Scrobble::HandshakeState state)
{
	QString str;
	if (state == Scrobble::HandshakeOk)
		str = i18n("Ok");
	if (state == Scrobble::HandshakeClientBanned)
		str = i18n("Meow is banned from AudioScrobbler (upgrade Meow)");
	if (state == Scrobble::HandshakeAuth)
		str = i18n("Bad username or password");
	if (state == Scrobble::HandshakeTime)
		str = i18n("Your system clock is too inaccurate");
	if (state == Scrobble::HandshakeFailure)
		str = i18n("Generic failure handshaking (try later)");
	d->diagnostics->setText(str);
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
	Collection *collection;
	
	bool isEnabled;
	QString username;
	QString password;
	
	QByteArray sessionId;
	QByteArray recievedData, recievedDataSubmission;
	
	KUrl nowPlaying;
	KUrl submission;
	
	QList<File> nowPlayingQueue;
	struct Submission
	{
		File file;
		FileId unknownFile;
		time_t timestamp;
		int length;
	};
	
	int numTracksSubmitting;
	bool failureSubmitting;
	
	QList<Submission> submissionQueue;
	File currentlyPlaying;
	time_t startedPlayingLast, beginDurationOfPlayback, pausedPlayingLast;
	int lengthOfLastSong;
};

Meow::Scrobble::Scrobble(QObject *parent, Player *player, Collection *collection)
	: QObject(parent)
{
	d = new ScrobblePrivate;
	d->player = player;
	d->collection = collection;
	d->isEnabled = false;
	d->numTracksSubmitting = 0;
	d->failureSubmitting = false;
	
	if (player)
	{
		KConfigGroup meow = KGlobal::config()->group("audioscrobbler");
		d->isEnabled = meow.readEntry<bool>("enabled", false);
		connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(currentItemChanged(File)));
		connect(d->player, SIGNAL(playing()), SLOT(startCountingTimeAgain()));
		connect(d->player, SIGNAL(paused()), SLOT(stopCountingTime()));
		
		int index=0;
		while (meow.hasKey("qi" + QString::number(index)))
		{
			ScrobblePrivate::Submission s = {
					File(),
					meow.readEntry<FileId>("qi" + QString::number(index), 0),
					meow.readEntry<int>("qt" + QString::number(index), 0),
					meow.readEntry<int>("ql" + QString::number(index), 0)
				};
			d->submissionQueue += s;
			meow.deleteEntry("qi" + QString::number(index));
			meow.deleteEntry("qt" + QString::number(index));
			meow.deleteEntry("ql" + QString::number(index));
			index++;
		}
	
	}
}

Meow::Scrobble::~Scrobble()
{
	KConfigGroup meow = KGlobal::config()->group("audioscrobbler");
	meow.writeEntry<bool>("enabled", d->isEnabled);
	
	int index=0;
	for (
			QList<ScrobblePrivate::Submission>::iterator i = d->submissionQueue.begin();
			i != d->submissionQueue.end(); ++i
		)
	{
		meow.writeEntry("qi" + QString::number(index), i->file.fileId());
		meow.writeEntry("qt" + QString::number(index), QString::number(i->timestamp));
		meow.writeEntry("ql" + QString::number(index), i->length);
		index++;
	}
	
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
	d->password = p;
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
	d->failureSubmitting = false;
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

void Meow::Scrobble::currentItemChanged(const File &file)
{
	if (!isEnabled())
		return;

	std::cerr << "Something is playing now" << std::endl;
	d->nowPlayingQueue += file;
	if (d->nowPlayingQueue.count() > 1)
		announceNowPlayingFromQueue();
	
	if (d->currentlyPlaying)
		lastSongFinishedPlaying();
	
	d->lengthOfLastSong = d->player->currentLength();
	if (d->lengthOfLastSong > 30)
	{
		d->currentlyPlaying = file;
		d->startedPlayingLast = QDateTime::currentDateTime().toTime_t();
		d->beginDurationOfPlayback = d->startedPlayingLast;
		stopCountingTime();
	}
	
}


void Meow::Scrobble::lastSongFinishedPlaying()
{
	time_t now = QDateTime::currentDateTime().toTime_t();
	int duration = now - d->beginDurationOfPlayback;
	if (duration >= 240 || duration > d->lengthOfLastSong/2)
	{
		ScrobblePrivate::Submission s = {
				d->currentlyPlaying, 0,
				d->startedPlayingLast,
				d->lengthOfLastSong
			};
		std::cerr << "Submitting that last song" << std::endl;
		d->submissionQueue += s;
		sendSubmissions();
	}
	d->currentlyPlaying = File();
}

void Meow::Scrobble::stopCountingTime()
{
	d->pausedPlayingLast = QDateTime::currentDateTime().toTime_t();
}

void Meow::Scrobble::startCountingTimeAgain()
{
	time_t now = QDateTime::currentDateTime().toTime_t();
	d->beginDurationOfPlayback += now - d->pausedPlayingLast;
}

void Meow::Scrobble::sendSubmissions()
{
	if (d->numTracksSubmitting > 0 || d->failureSubmitting)
		return;
		
	QByteArray submissionData;
	submissionData = "s=" + d->sessionId;
	int index=0;
	for (
			QList<ScrobblePrivate::Submission>::iterator i = d->submissionQueue.begin();
			i != d->submissionQueue.end() && index < 50; ++i
		)
	{
		ScrobblePrivate::Submission &s = *i;
		
		File &f = s.file;
		if (!f)
			f = d->collection->getSong(s.unknownFile);
		
		const QByteArray aindex = QByteArray::number(index);
		
		submissionData += "&a[" + aindex + "]="
				+ f.artist().toUtf8().toPercentEncoding()
			+ "&t[" + aindex + "]="
				+ f.title().toUtf8().toPercentEncoding()
			+ "&i[" + aindex + "]="
				+ QByteArray::number((int)s.timestamp)
			+ "&o[" + aindex + "]=P"
			+ "&r[" + aindex + "]="
			+ "&l[" + aindex + "]="
				+ QByteArray::number(s.length/1000)
			+ "&b[" + aindex + "]="
				+ f.album().toUtf8().toPercentEncoding()
			+ "&n[" + aindex + "]="
				+ f.track().toUtf8().toPercentEncoding()
			+ "&m[" + aindex + "]=";
		
		index++;
	}
	d->numTracksSubmitting = index;
	
	std::cerr << "Posting: " << submissionData.data() << std::endl;
	
	KIO::TransferJob *job = KIO::http_post(
			d->submission, submissionData, KIO::HideProgressInfo
		);
	job->addMetaData( "content-type", "Content-type: application/x-www-form-urlencoded" );
	job->addMetaData( "accept", "" );
	connect(
			job, SIGNAL(data(KIO::Job*, QByteArray)),
			SLOT(submissionData(KIO::Job*, QByteArray))
		);
	connect(
			job, SIGNAL(result(KJob*)),
			SLOT(submissionResult())
		);
}

void Meow::Scrobble::submissionData(KIO::Job*, const QByteArray &data)
{
	d->recievedDataSubmission += data;
}

void Meow::Scrobble::submissionResult()
{
	QList<QByteArray> lines = d->recievedDataSubmission.split('\n');
	if (lines.size() < 1)
	{
		std::cerr << "Meow: scrobbler submitting: major error" << std::endl;
		return;
	}
	
	if (lines[0] == "OK")
	{
		while (d->numTracksSubmitting--)
			d->submissionQueue.removeFirst();
	}
	else
	{
		std::cerr << "Meow: scrobbler submitting error: " << lines[0].data() << std::endl;
		d->failureSubmitting = true;
		QTimer::singleShot(240*1000, this, SLOT(sendSubmissionsRetry()));
		return;
	}
	
	d->recievedDataSubmission.clear();
	QTimer::singleShot(10*1000, this, SLOT(sendSubmissions()));
}

void Meow::Scrobble::sendSubmissionsRetry()
{
	d->failureSubmitting = false;
	submissionResult();
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
	
	KIO::TransferJob *job = KIO::http_post(np, QByteArray(), KIO::HideProgressInfo);
	connect(job, SIGNAL(data(KIO::Job*, QByteArray)), SLOT(nowPlayingData(KIO::Job*, QByteArray)));
	connect(job, SIGNAL(result(KJob*)), SLOT(nowPlayingResult()));
}

void Meow::Scrobble::nowPlayingData(KIO::Job*, const QByteArray &data)
{
	d->recievedData += data;
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
	d->recievedData.clear();
	if (d->nowPlayingQueue.count() > 0)
		announceNowPlayingFromQueue();
}

// kate: space-indent off; replace-tabs off;

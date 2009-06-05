#include "scrobble.h"
#include "player.h"
#include "db/file.h"
#include "db/collection.h"

#include <kurl.h>
#include <kio/job.h>
#include <klocale.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kwallet.h>
#include <kmessagebox.h>
#include <kcodecs.h>
#include <kdeversion.h>

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

static QByteArray md5(const QByteArray &data)
{
	return KMD5(data).hexDigest();
}


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
	
	QLabel *info = new QLabel(
			i18n(
					"If you enable scrobbling, but for whatever reason, "
					"the track submission could not be made (such as if "
					"the password was incorrect or unavailable), it will be "
					"saved for later submission."
				),
			this
		);
	info->setWordWrap(true);
	
	layout->addWidget(info, 1, 0, 1, 2);
	
	{
		QLabel *label = new QLabel(i18n("&Username"), this);
		layout->addWidget(label, 2, 0);
		
		d->username = new QLineEdit(this);
		label->setBuddy(d->username);
		layout->addWidget(d->username, 2, 1);
	}
	{
		QLabel *label = new QLabel(i18n("&Password"), this);
		layout->addWidget(label, 3, 0);
		
		d->password = new QLineEdit(this);
		d->password->setEchoMode(QLineEdit::Password);
		label->setBuddy(d->password);
		layout->addWidget(d->password, 3, 1);
	}
	
	{
		QHBoxLayout *rowlayout = new QHBoxLayout;
		layout->addLayout(rowlayout, 4, 0, 1, 2);
		
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
	d->password->setText(d->scrobble->passwordIfKnown());
	d->diagnostics->setText("");
}

void Meow::ScrobbleConfigure::apply()
{
	d->scrobble->setEnabled(d->isEnabled->isChecked());
	d->scrobble->setUsername(d->username->text());
	d->scrobble->setPassword(d->password->text());
	KConfigGroup conf = KGlobal::config()->group("audioscrobbler");
	conf.writeEntry<bool>("enabled", d->isEnabled->isChecked());
	conf.deleteEntry("password");
	conf.deleteEntry("usename");
	conf.deleteEntry("password source");
	
	if (!d->isEnabled->isChecked())
	{
		conf.sync();
		return;
	}
	
	if (
			KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
					KWallet::Wallet::NetworkWallet(), effectiveWinId()
				)
		)
	{
		// use the KPdf folder (and create if missing)
		if ( !wallet->hasFolder( "Meow" ) )
			wallet->createFolder( "Meow" );
		wallet->setFolder( "Meow" );

		// look for the pass in that folder
		wallet->writeEntry( "AudioScrobbler Username", d->username->text().toUtf8() );
		wallet->writePassword( "AudioScrobbler Password", d->password->text().toUtf8() );
		
		conf.writeEntry("password source", "wallet");
	}
	else
	{
	
		KGuiItem yes = KStandardGuiItem::yes();
		yes.setText(i18n("Store password"));
		KGuiItem no = KStandardGuiItem::no();
		no.setText(i18n("Only use my password this session"));
		const int question = KMessageBox::questionYesNo(
				this, 
				i18n(
						"As KWallet is not available, Meow can store your "
						"AudioScrobbler password in its config file. It will "
						"be encrypted, so others will not be able to determine "
						"your password, but they could use it in order to "
						"make rogue and potentially embarrassing track "
						"submissions.\n\n"
						"If you opt to not store the password, you will have to "
						"manually reenable Scrobbler support next time you "
						"start Meow."
					),
				i18n("Store the password"),
				yes, no
			);
		if (question == KMessageBox::Yes)
		{
			conf.writeEntry("password source", "here");
			conf.writeEntry("password", md5(d->password->text().toUtf8()));
			conf.writeEntry("username", d->username->text());
		}
		else
		{
			conf.writeEntry("password source", "nowhere");
			conf.deleteEntry("password");
		}
	}
	
	conf.sync();
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
	Scrobble *scr = new Scrobble(this);
	scr->setEnabled(true);
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
static QString userAgent()
{
	QString agent("Meow/1.0 (KDE %1.%2.%3)");
	agent = agent.arg(KDE_VERSION_MAJOR).arg(KDE_VERSION_MINOR).arg(KDE_VERSION_RELEASE);
	return agent;
}


struct Meow::Scrobble::ScrobblePrivate
{
	Player *player;
	Collection *collection;
	
	bool isEnabled;
	QString username, passwordIfKnown, passwordMd5;
	
	QByteArray sessionId;
	QByteArray recievedData, recievedDataSubmission;
	
	KUrl nowPlaying;
	KUrl submission;
	
	QList<File> nowPlayingQueue;
	
	int numTracksSubmitting;
	bool failureSubmitting;
	
	QList<QStringList > submissionQueue;
	File currentlyPlaying;
	time_t startedPlayingLast, beginDurationOfPlayback, pausedPlayingLast;
	int lengthOfLastSong;
};

Meow::Scrobble::Scrobble(QObject *parent)
	: QObject(parent)
{
	d = new ScrobblePrivate;
	d->player = 0;
	d->collection = 0;
	d->isEnabled = false;
	d->numTracksSubmitting = 0;
	d->failureSubmitting = false;
}

Meow::Scrobble::Scrobble(QWidget *parent, Player *player, Collection *collection)
	: QObject(parent)
{
	d = new ScrobblePrivate;
	d->player = player;
	d->collection = collection;
	d->isEnabled = false;
	d->numTracksSubmitting = 0;
	d->failureSubmitting = false;
	
	KConfigGroup conf = KGlobal::config()->group("audioscrobbler");
	d->isEnabled = conf.readEntry<bool>("enabled", false);
	
	if (d->isEnabled)
	{
		QString passwordSource = conf.readEntry("password source", "nowhere");
		
		if (passwordSource == "wallet")
		{
			KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
					KWallet::Wallet::NetworkWallet(), parent->effectiveWinId()
				);
			if (wallet)
			{
				if ( !wallet->hasFolder( "Meow" ) )
					wallet->createFolder( "Meow" );
				wallet->setFolder( "Meow" );
	
				QString retrievedPass;
				QByteArray retrievedUser;
				if ( !wallet->readEntry( "AudioScrobbler Username", retrievedUser ) )
					setUsername(QString::fromUtf8(retrievedUser));
				if ( !wallet->readPassword( "AudioScrobbler Password", retrievedPass ) )
					setPassword(retrievedPass);
			}
		}
		else if (passwordSource == "here")
		{
			d->passwordMd5 = conf.readEntry("password", "");
			d->username = conf.readEntry("username", "");
		}
	}
	
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(currentItemChanged(File)));
	connect(d->player, SIGNAL(lengthChanged(int)), SLOT(knowLengthOfCurrentSong(int)));
	connect(d->player, SIGNAL(playing()), SLOT(startCountingTimeAgain()));
	connect(d->player, SIGNAL(paused()), SLOT(stopCountingTime()));
	
	int index=0;
	while (conf.hasKey("qi" + QString::number(index)))
	{
		QStringList s = conf.readEntry<QStringList>("qi" + QString::number(index), QStringList());
		if ( s.count()!=0 && (s.count() % 2) == 0)
			d->submissionQueue += s;
		index++;
	}

}

Meow::Scrobble::~Scrobble()
{
	KConfigGroup meow = KGlobal::config()->group("audioscrobbler");
	meow.writeEntry<bool>("enabled", d->isEnabled);
	
	int index=0;
	for (
			QList<QStringList>::iterator i = d->submissionQueue.begin();
			i != d->submissionQueue.end(); ++i
		)
	{
		meow.writeEntry("qi" + QString::number(index), *i);
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

QString Meow::Scrobble::passwordIfKnown() const
{
	return d->passwordIfKnown;
}

void Meow::Scrobble::setUsername(const QString &u)
{
	d->username = u;
}

void Meow::Scrobble::setPassword(const QString &p)
{
	d->passwordIfKnown = p;
	d->passwordMd5 = md5(p.toUtf8());
}


void Meow::Scrobble::begin()
{
	if (!isEnabled())
		return;
	
	d->recievedData.clear();
	
	QString timestamp = QString::number(QDateTime::currentDateTime().toTime_t());
	
	QString authToken;
	authToken = d->passwordMd5.toUtf8() + timestamp;
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
	job->addMetaData( "UserAgent", "User-Agent: " + userAgent());
	connect(job, SIGNAL(data(KIO::Job*, QByteArray)), SLOT(handshakeData(KIO::Job*, QByteArray)));
	connect(job, SIGNAL(result(KJob*)), SLOT(slotHandshakeResult()));
	d->failureSubmitting = false;
	d->numTracksSubmitting = 0;
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
	else
		std::cerr << "No previous song to submit" << std::endl;
	
	d->currentlyPlaying = file;
	d->startedPlayingLast = QDateTime::currentDateTime().toTime_t();
	d->beginDurationOfPlayback = d->startedPlayingLast;
	stopCountingTime(); // I'm going to get d->playing() right away
}

void Meow::Scrobble::knowLengthOfCurrentSong(int msec)
{
	d->lengthOfLastSong = msec/1000;
}


void Meow::Scrobble::lastSongFinishedPlaying()
{
	time_t now = QDateTime::currentDateTime().toTime_t();
	int duration = now - d->beginDurationOfPlayback;
	if (duration >= 240 || duration > d->lengthOfLastSong/2)
	{
		std::cerr << "Submitting that last song" << std::endl;
		const File &f = d->currentlyPlaying;
		
		QList<QString> variables;
		variables
			<< "a" << f.artist()
			<< "t" << f.title()
			<< "i" << QString::number((int)now)
			<< "o" << "P"
			<< "r" << ""
			<< "l" << QString::number(d->lengthOfLastSong)
			<< "b" << f.album()
			<< "n" << f.track()
			<< "m" << "";
		
		d->submissionQueue.append(variables);
		sendSubmissions();
	}
	else
	{
		std::cerr << "Didn't play that last song long enough to submit it" << std::endl;
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
		
	if (d->submissionQueue.count() == 0)
		return;
	
	QByteArray submissionData;
	submissionData = "s=" + d->sessionId;
	int index=0;
	for (
			QList<QStringList>::iterator i = d->submissionQueue.begin();
			i != d->submissionQueue.end() && index < 50; ++i
		)
	{
		const QStringList &s = *i;
		
		const QByteArray aindex = QByteArray::number(index);
		
		for (QStringList::const_iterator vi = s.begin(); vi != s.end(); )
		{
			submissionData +=
				"&" + vi->toUtf8().toPercentEncoding()
				+ "[" + aindex + "]=";
			++vi;
			submissionData += vi->toUtf8().toPercentEncoding();
			++vi;
		}
		
		index++;
	}
	d->numTracksSubmitting = index;
	
	std::cerr << "Posting: " << submissionData.data() << std::endl;
	
	KIO::TransferJob *job = KIO::http_post(
			d->submission, submissionData, KIO::HideProgressInfo
		);
	job->addMetaData( "content-type", "Content-type: application/x-www-form-urlencoded" );
	job->addMetaData( "accept", "" );
	job->addMetaData( "UserAgent", "User-Agent: " + userAgent());
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
	
	d->recievedDataSubmission.clear();
	
	if (lines[0] == "OK")
	{
		while (d->numTracksSubmitting--)
			d->submissionQueue.removeFirst();
		QTimer::singleShot(10*1000, this, SLOT(sendSubmissions()));
	}
	else if (lines[0] == "BADSESSION")
	{
		d->sessionId = "";
		// try logging again in 30 seconds
		QTimer::singleShot(30*1000, this, SLOT(begin()));
	}
	else
	{
		d->numTracksSubmitting = 0;
		std::cerr << "Meow: scrobbler submitting error: " << lines[0].data() << std::endl;
		d->failureSubmitting = true;
		QTimer::singleShot(240*1000, this, SLOT(sendSubmissionsRetry()));
		return;
	}
	
}

void Meow::Scrobble::sendSubmissionsRetry()
{
	d->failureSubmitting = false;
	sendSubmissions();
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
	np.addQueryItem("l", QString::number(d->player->currentLength()/1000));
	np.addQueryItem("n", file.track());
	np.addQueryItem("m", "");
	
	KIO::TransferJob *job = KIO::http_post(np, QByteArray(), KIO::HideProgressInfo);
	job->addMetaData( "UserAgent", "User-Agent: " + userAgent());
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
	
	d->recievedData.clear();
	std::cerr << "Meow: scrobbler now playing: " << lines[0].data() << std::endl;
	
	if (lines[0] == "BADSESSION")
	{
		d->sessionId = "";
		// try logging again in 30 seconds
		QTimer::singleShot(30*1000, this, SLOT(begin()));
	}
	else
	{
		if (d->nowPlayingQueue.count() > 0)
			announceNowPlayingFromQueue();
	}
}

// kate: space-indent off; replace-tabs off;

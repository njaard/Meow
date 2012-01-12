#include "scrobble.h"
#include "player.h"
#include "db/file.h"
#include "db/collection.h"

#ifdef MEOW_WITH_KDE
#include <kurl.h>
#include <kio/job.h>
#include <klocale.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kwallet.h>
#include <kmessagebox.h>
#include <kcodecs.h>
#include <kdeversion.h>
#else
#include <qurl.h>
#include <qsettings.h>
#include <qmessagebox.h>
#include <qnetworkaccessmanager.h>
#include <qbuffer.h>
#include <qnetworkrequest.h>
#include <qnetworkreply.h>
#include <memory>
#endif

#include <qdom.h>
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


#ifdef MEOW_WITH_KDE
static QString md5(const QByteArray &data)
{
	return KMD5(data).hexDigest();
}
#else
#include "md5.h"
static QString md5(const QByteArray &data)
{
	md5_state_t s;
	md5_init(&s);
	md5_append(&s, (const unsigned char*)data.constData(), data.length());
	
	md5_byte_t digest[16];
	md5_finish(&s, digest);
	return QByteArray((const char*)digest, 16).toHex();
}

#endif

#ifdef MEOW_WITH_KDE
typedef KUrl MeowUrlType;
#else
typedef QUrl MeowUrlType;
#endif

struct Meow::ScrobbleConfigure::ScrobbleConfigurePrivate
{
	Scrobble *scrobble;
	
	QCheckBox *isEnabled;
	QLineEdit *username, *password;
	QLabel *diagnostics;
	QPushButton *test;
};

#ifndef MEOW_WITH_KDE
#define i18n tr
#endif



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

#ifdef MEOW_WITH_KDE
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
						"your password, but they could use the encrypted password to "
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
#else
	QSettings conf;
	conf.setValue("audioscrobbler/enabled", d->isEnabled->isChecked());
	conf.remove("audioscrobbler/password");
	conf.remove("audioscrobbler/usename");
	conf.remove("audioscrobbler/password source");
	
	if (!d->isEnabled->isChecked())
		return;
	
	{
		QMessageBox messageBox(
				QMessageBox::Question, 
				tr("Store the password"),
				tr(
						"Meow can store your "
						"AudioScrobbler password in its config file. It will "
						"be encrypted, so others will not be able to determine "
						"your password, but they could use the encrypted password "
						"make rogue and potentially embarrassing track "
						"submissions.\n\n"
						"If you opt to not store the password, you will have to "
						"manually reenable Scrobbler support next time you "
						"start Meow."
					),
				QMessageBox::Yes|QMessageBox::No,
				this
			);
		messageBox.button(QMessageBox::Yes)->setText(tr("Store password"));
		messageBox.button(QMessageBox::No)->setText(tr("Only use my password this session"));
		messageBox.exec();
		if (messageBox.standardButton(messageBox.clickedButton()) == QMessageBox::Yes)
		{
			conf.setValue("audioscrobbler/password source", "here");
			conf.setValue("audioscrobbler/password", md5(d->password->text().toUtf8()));
			conf.setValue("audioscrobbler/username", d->username->text());
		}
		else
		{
			conf.setValue("audioscrobbler/password source", "nowhere");
			conf.remove("audioscrobbler/password");
		}
	}

#endif

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
	ScrobbleSession *scr = new ScrobbleSession(this);
	connect(
			scr, SIGNAL(handshakeState(ScrobbleSession::HandshakeState)), 
			SLOT(showResults(ScrobbleSession::HandshakeState))
		);
	connect(
			scr, SIGNAL(handshakeState(ScrobbleSession::HandshakeState)),
			scr, SLOT(deleteLater())
		);
	scr->startSession(d->username->text(), md5(d->password->text().toUtf8()));
}

void Meow::ScrobbleConfigure::showResults(ScrobbleSession::HandshakeState state)
{
	QString str;
	if (state == ScrobbleSession::HandshakeOk)
		str = i18n("Ok");
	if (state == ScrobbleSession::HandshakeClientBanned)
		str = i18n("Meow is banned from AudioScrobbler (upgrade Meow)");
	if (state == ScrobbleSession::HandshakeAuth)
		str = i18n("Bad username or password");
	if (state == ScrobbleSession::HandshakeFailure)
		str = i18n("Generic failure handshaking (try later)");
	d->diagnostics->setText(str);
}



static const char handshakeUrl[] = "http://ws.audioscrobbler.com/2.0/";
static const char apiKey[] = "e31674916d417e952120fc56b53750b0";
static const char sharedSecret[] = "2dd588e3897657af9422bf4a467f9d8d"; // this api is retarded

#ifdef MEOW_WITH_KDE
static QString userAgent()
{
	QString agent("Meow/%1 (KDE %2.%3.%4)");
	agent = agent.arg(MEOW_VERSION).arg(KDE_VERSION_MAJOR).arg(KDE_VERSION_MINOR).arg(KDE_VERSION_RELEASE);
	return agent;
}
#else
static QString userAgent()
{
	return QString("Meow/%1 (Windows)").arg(MEOW_VERSION);
}
#endif


struct Meow::ScrobbleSession::ScrobbleSessionPrivate
{
#ifndef MEOW_WITH_KDE
	QNetworkAccessManager networkAccess;
	QByteArray postedData;
	QBuffer postedBuffer;
	QNetworkReply *currentHttp;
#endif

	void (ScrobbleSession::*response)(QDomElement);

	QString sessionKey;
	QByteArray recievedData;
	
	struct QueueEl
	{
		bool post;
		Query query;
		void (ScrobbleSession::*response)(QDomElement);
	};
	
	QList<QueueEl> queue;
};

Meow::ScrobbleSession::ScrobbleSession(QObject *parent)
	: QObject(parent)
{
	d = new ScrobbleSessionPrivate;
	d->response=0;
#ifndef MEOW_WITH_KDE
	d->currentHttp=0;
#endif
}

Meow::ScrobbleSession::~ScrobbleSession()
{
#ifndef MEOW_WITH_KDE
	if (d->currentHttp)
		d->currentHttp->deleteLater();
#endif
	delete d;
}

void Meow::ScrobbleSession::startSession(const QString &username, const QString &passwordMd5)
{
	Query q;
	q["method"]="auth.getMobileSession";
	q["username"]=username;
	q["authToken"] = md5(username.toUtf8() + passwordMd5.toUtf8());

	makeRequest(false, q, &ScrobbleSession::startSessionRes);
}

void Meow::ScrobbleSession::startSessionRes(QDomElement root)
{
	const QString status = root.attribute("status");
	if (status == "ok")
	{ }
	else
	{
		if (status == "4" || status == "9")
			handshakeState(HandshakeAuth);
		else if (status == "26")
			handshakeState(HandshakeClientBanned);
		else
			handshakeState(HandshakeFailure);
	}


	QDomElement se = root.firstChildElement("session");
	if (se.isNull())
	{
		error(root, "error starting session");
		return;
	}
	QDomElement key = se.firstChildElement("key");
	if (key.isNull())
	{
		error(root, "bad response");
		return;
	}
	d->sessionKey = key.text();
	emit handshakeState(HandshakeOk);
}

void Meow::ScrobbleSession::submitTracks(const QList<QStringList> trackKeys)
{
	Query q;
	q["method"]="track.scrobble";
	q["sk"] = d->sessionKey;
	
	unsigned index=0;
	for (QList<QStringList>::const_iterator i = trackKeys.begin(); i != trackKeys.end(); ++i)
	{
		const QStringList &l = *i;
		if (l.count() %2 != 0)
			break;
		const QString suffix = "[" + QString::number(index) + "]";
		for (QStringList::const_iterator i = l.begin(); i != l.end(); )
		{
			QString key = *i;
			QString val = *++i;
			q[ key ] = val;
			++i;
		}
	}
	
	makeRequest(true, q, &ScrobbleSession::submitTrackRes);
}

void Meow::ScrobbleSession::submitTrackRes(QDomElement root)
{
	if (root.tagName() != "lfm")
	{
		invalidResponse(root);
		emit submitCompleted(false);
		return;
	}
	if (root.attribute("status") != "ok")
	{
		error(root, "Error submitting");
		emit submitCompleted(false);
		return;
	}
	emit submitCompleted(true);
}

void Meow::ScrobbleSession::nowPlaying(const QStringList trackKeys)
{
	Query q;
	q["method"]="track.updateNowPlaying";
	q["sk"] = d->sessionKey;
	for (QStringList::const_iterator i = trackKeys.begin(); i != trackKeys.end(); )
	{
		QString key = *i;
		QString val = *++i;
		q[ key ] = val;
		++i;
	}
	makeRequest(true, q, &ScrobbleSession::nowPlayingRes);
}

void Meow::ScrobbleSession::nowPlayingRes(QDomElement)
{
}

void Meow::ScrobbleSession::error(QDomElement root, const QString &e)
{
	QDomElement el = root.isNull() ? QDomElement() : root.firstChildElement("error");
	QString x = el.isNull() ? "unspecified" : el.text();
	std::cout << "lastfm error: " << e.toUtf8().constData() << ": " << x.toUtf8().constData() << std::endl;
}

void Meow::ScrobbleSession::invalidResponse(QDomElement root)
{
	error(root, i18n("Invalid response from last.fm"));
}

void Meow::ScrobbleSession::makeRequest(bool post, Query &query, void (ScrobbleSession::*response)(QDomElement))
{
	if (d->response)
	{
		ScrobbleSessionPrivate::QueueEl ql;
		ql.post = post;
		ql.query = query;
		ql.response = response;
		d->queue.append(ql);
		return;
	}
	d->response = response;
	
#ifdef MEOW_WITH_KDE
	KUrl url(handshakeUrl);
#else
	QUrl url(handshakeUrl);
#endif

	query["api_key"]=apiKey;


	QString callsig;

	for (Query::const_iterator i = query.begin(); i != query.end(); ++i)
	{
		url.addQueryItem(i.key(), i.value());
		callsig += i.key() + i.value();
	}
	
	callsig += sharedSecret;
	callsig = md5(callsig.toUtf8());
	
	url.addQueryItem("api_sig", callsig);

	d->recievedData.clear();
	
	QByteArray posted;
	if (post)
	{
		posted = url.encodedQuery();
		url = QUrl(handshakeUrl);
	}
	
	std::cout << "Posting: " << posted.constData() << std::endl;
	
#ifdef MEOW_WITH_KDE
	KIO::TransferJob *job = KIO::http_post(url, posted, KIO::HideProgressInfo);
	job->addMetaData( "content-type", "Content-type: application/x-www-form-urlencoded" );
	job->addMetaData( "accept", "" );
	job->addMetaData( "UserAgent", "User-Agent: " + userAgent());
	connect(job, SIGNAL(data(KIO::Job*, QByteArray)), SLOT(handshakeData(KIO::Job*, QByteArray)));
	connect(job, SIGNAL(result(KJob*)), SLOT(slotHandshakeResult()));

#else

	QNetworkRequest req(url);
	
	d->postedData = posted;
	d->postedBuffer.setBuffer(&d->postedData);

	req.setRawHeader( "User-Agent", userAgent().toUtf8());
	req.setRawHeader( "Content-type", "application/x-www-form-urlencoded");
	req.setRawHeader( "accept", "");
	d->currentHttp = d->networkAccess.post(req, &d->postedBuffer);
	connect(d->currentHttp, SIGNAL(readyRead()), SLOT(handshakeData()));
	connect(d->currentHttp, SIGNAL(finished()), SLOT(slotHandshakeResult()));
#endif
}

#ifdef MEOW_WITH_KDE
void Meow::ScrobbleSession::handshakeData(KIO::Job*, const QByteArray &data)
{
	d->recievedData += data;
}

#else
void Meow::ScrobbleSession::handshakeData()
{
	d->recievedData += d->currentHttp->readAll();
}
#endif

void Meow::ScrobbleSession::slotHandshakeResult()
{
#ifndef MEOW_WITH_KDE
	if (d->currentHttp)
	{
		d->currentHttp->deleteLater();
		d->currentHttp=0;
	}
#endif

	QDomDocument doc;
	doc.setContent(d->recievedData);
	QDomElement root = doc.documentElement();
	if (root.isNull())
		;
	else if (root.tagName() != "lfm")
	{
		invalidResponse(root);
	}
	(this->*d->response)(root);
	d->response=0;
	if (!d->queue.isEmpty())
	{
		ScrobbleSessionPrivate::QueueEl el = d->queue.takeFirst();
		makeRequest(el.post, el.query, el.response);
	}
}


struct Meow::Scrobble::ScrobblePrivate
{
	Player *player;
	Collection *collection;
	
	bool isEnabled;
	QString username, passwordIfKnown, passwordMd5;
	
	MeowUrlType nowPlaying;
	MeowUrlType submission;
	
	int numTracksSubmitting;
	bool failureSubmitting;
	
	QList<QStringList > submissionQueue;
	File currentlyPlaying;
	time_t startedPlayingLast, beginDurationOfPlayback, pausedPlayingLast;
	int lengthOfLastSong;
	
	ScrobbleSession *session;
	
	File justAnnounced;
};

Meow::Scrobble::Scrobble(QWidget *parent, Player *player, Collection *collection)
	: QObject(parent)
{
	d = new ScrobblePrivate;
	d->player = player;
	d->collection = collection;
	d->isEnabled = false;
	d->numTracksSubmitting = 0;
	d->failureSubmitting = false;

	d->session = new ScrobbleSession(this);
	connect(d->session, SIGNAL(submitCompleted(bool)), SLOT(submitCompleted(bool)));

#ifdef MEOW_WITH_KDE
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
#else
	QSettings conf;
	d->isEnabled = conf.value("audioscrobbler/enabled", false).toBool();
	
	if (d->isEnabled)
	{
		QString passwordSource = conf.value("audioscrobbler/password source", "nowhere").toString();
		
		if (passwordSource == "here")
		{
			d->passwordMd5 = conf.value("audioscrobbler/password", "").toString();
			d->username = conf.value("audioscrobbler/username", "").toString();
		}
	}

#endif
	
	connect(d->player, SIGNAL(currentItemChanged(File)), SLOT(currentItemChanged(File)));
	connect(d->player, SIGNAL(lengthChanged(int)), SLOT(knowLengthOfCurrentSong(int)));
	connect(d->player, SIGNAL(playing()), SLOT(startCountingTimeAgain()));
	connect(d->player, SIGNAL(paused()), SLOT(stopCountingTime()));
	
	int index=0;
#ifdef MEOW_WITH_KDE
	while (conf.hasKey("qi" + QString::number(index)))
	{
		QStringList s = conf.readEntry<QStringList>("qi" + QString::number(index), QStringList());
		if ( s.count()!=0 && (s.count() % 2) == 0)
			d->submissionQueue += s;
		index++;
	}
#else
	while (conf.contains("qi" + QString::number(index)))
	{
		QStringList s = conf.value("qi" + QString::number(index), QStringList()).toStringList();
		if ( s.count()!=0 && (s.count() % 2) == 0)
			d->submissionQueue += s;
		index++;
	}
#endif

}

Meow::Scrobble::~Scrobble()
{
#ifdef MEOW_WITH_KDE
	KConfigGroup conf = KGlobal::config()->group("audioscrobbler");
	conf.writeEntry<bool>("enabled", d->isEnabled);
	
	int index=0;
	for (
			QList<QStringList>::iterator i = d->submissionQueue.begin();
			i != d->submissionQueue.end(); ++i
		)
	{
		conf.writeEntry("qi" + QString::number(index), *i);
		index++;
	}
	while (conf.hasKey("qi" + QString::number(index)))
	{
		conf.deleteEntry("qi" + QString::number(index));
		index++;
	}
#else
	QSettings conf;
	conf.setValue("audioscrobbler/enabled", d->isEnabled);
	
	int index=0;
	for (
			QList<QStringList>::iterator i = d->submissionQueue.begin();
			i != d->submissionQueue.end(); ++i
		)
	{
		conf.setValue("audioscrobbler/qi" + QString::number(index), *i);
		index++;
	}
	while (conf.contains("audioscrobbler/qi" + QString::number(index)))
	{
		conf.remove("audioscrobbler/qi" + QString::number(index));
		index++;
	}
#endif

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
	d->session->startSession(d->username, d->passwordMd5);
}

void Meow::Scrobble::currentItemChanged(const File &file)
{
	if (!isEnabled() && !file)
		return;

	std::cerr << "Something is playing now" << std::endl;
	// wait five seconds before announcing the now playing song
	QTimer::singleShot(5000, this, SLOT(announceNowPlaying()));
	
	if (d->currentlyPlaying)
		lastSongFinishedPlaying();
	else
		std::cerr << "No previous song to submit" << std::endl;
	
	d->currentlyPlaying = file;
	d->startedPlayingLast = QDateTime::currentDateTime().toTime_t();
	d->beginDurationOfPlayback = d->startedPlayingLast;
	stopCountingTime(); // I'm going to get d->playing() right away
}

void Meow::Scrobble::announceNowPlaying()
{
	if (d->currentlyPlaying == d->justAnnounced)
		return;
	d->justAnnounced = d->currentlyPlaying;
	d->session->nowPlaying(trackInfo(d->currentlyPlaying));
}

void Meow::Scrobble::sendSubmissions()
{
	if (d->numTracksSubmitting > 0 || d->failureSubmitting)
		return;
	if (d->submissionQueue.count() == 0)
		return;

	QList<QStringList> toSubmit = d->submissionQueue.mid(0, 50);
	d->numTracksSubmitting = toSubmit.length();
	
	d->session->submitTracks(toSubmit);
}

void Meow::Scrobble::sendSubmissionsRetry()
{
	d->failureSubmitting = false;
	sendSubmissions();
}

void Meow::Scrobble::submitCompleted(bool success)
{
	if (success)
	{
		for (; d->numTracksSubmitting; d->numTracksSubmitting--)
			d->submissionQueue.removeFirst();
		QTimer::singleShot(10*1000, this, SLOT(sendSubmissions()));
	}
	else
	{
		d->numTracksSubmitting = 0;
		d->failureSubmitting = true;
		QTimer::singleShot(240*1000, this, SLOT(sendSubmissionsRetry()));
	}
}

QStringList Meow::Scrobble::trackInfo(File f)
{
	QList<QString> variables;
	variables
		<< "album" << f.album()
		<< "track" << f.title()
		<< "artist" << f.artist()
		<< "duration" << QString::number(d->lengthOfLastSong)
		<< "trackNumber" << f.track();
	return variables;
}

void Meow::Scrobble::lastSongFinishedPlaying()
{
	const time_t now = QDateTime::currentDateTime().toTime_t();
	const int duration = now - d->beginDurationOfPlayback;
	if (duration >= 240 || duration > d->lengthOfLastSong/2)
	{
		std::cerr << "Submitting that last song" << std::endl;
		const File &f = d->currentlyPlaying;

		QList<QString> variables = trackInfo(f);
		variables << "timestamp" << QString::number((int)now);

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

void Meow::Scrobble::knowLengthOfCurrentSong(int msec)
{
	if (msec >= 0)
		d->lengthOfLastSong = msec/1000;

}

// kate: space-indent off; replace-tabs off;

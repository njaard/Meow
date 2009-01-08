/* This file is part of Noatun

  Copyright 2000-2006 by Charles Samuels <charles@kde.org>
  Copyright 2001-2007 by Stefan Gehn <mETz81@web.de>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "player_p.h"
#include "player.h"

#include <qregexp.h>
#include <qtimer.h>
#include <qstringlist.h>
#include <qlocale.h>

#include <kdebug.h>
#include <kurl.h>
#include <klocale.h>
#include <kglobal.h>


namespace Meow
{


Player::State PlayerPrivate::convertState(Phonon::State s)
{
	switch(s)
	{
	case Phonon::PlayingState:
		return Player::PlayingState;
	case Phonon::PausedState:
		return Player::PausedState;
	case Phonon::LoadingState: // map all these to stopped for now
	case Phonon::StoppedState:
	case Phonon::BufferingState:
	case Phonon::ErrorState:
		break;
	}
	return Player::StoppedState;
}

void PlayerPrivate::initPhonon()
{
	if (mediaObject)
		return;
	mediaObject = new Phonon::MediaObject(q);
	audioOutput = new Phonon::AudioOutput(Phonon::MusicCategory, q);
	//videoPath = new Phonon::VideoPath(q);

	Phonon::createPath(mediaObject, audioOutput);
	//mediaObject->addVideoPath(videoPath);
	//videoPath->addOutput(videoWidget);

	mediaObject->setTickInterval(200);
	setVolume(d->volumePercent);

	QObject::connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
		q, SLOT(_n_updateState(Phonon::State, Phonon::State)));
	QObject::connect(mediaObject, SIGNAL(finished()),
		q, SLOT(_n_finishedPlaying()));
	QObject::connect(mediaObject, SIGNAL(totalTimeChanged(qint64)),
		q, SLOT(_n_updateLength(qint64)));
	QObject::connect(mediaObject, SIGNAL(metaDataChanged()),
		q, SLOT(_n_updateMetaData()));
	QObject::connect(mediaObject, SIGNAL(tick(qint64)),
		q, SLOT(_n_updatePosition(qint64)));
	QObject::connect(audioOutput, SIGNAL(volumeChanged(qreal)),
		q, SLOT(_n_updateVolume(qreal)));
	
}

void PlayerPrivate::_n_updateState(Phonon::State newState, Phonon::State oldState)
{
	if (newState == Phonon::ErrorState)
	{
		kDebug(66666) << "error " << mediaObject->errorString();
		mediaObject->stop();
		emit q->stopped();
		emit q->stateChanged(Player::StoppedState);
		emit q->errorOccurred(Player::NormalError, mediaObject->errorString());
	}
	else
	{
		kDebug(66666) << "old: " << oldState << "; new: " << newState;
		switch (newState)
		{
		case Phonon::PlayingState:
			emit q->playing();
			break;
		case Phonon::StoppedState:
			emit q->stopped();
			break;
		case Phonon::PausedState:
			emit q->paused();
			break;
		default:
			break;
		}
		emit q->stateChanged(convertState(newState));
	}
}

void PlayerPrivate::_n_finishedPlaying()
{
	// delaying this is helpful for phonon-xine
	emit q->finished();
}

void PlayerPrivate::_n_updateMetaData()
{
/* X == mapped keys
 X ALBUM
 X ARTIST
 X DATE
 DESCRIPTION (unmapped because it contains shoutcast serverinfos, ugly)
 X GENRE
 X TITLE
 TRACKNUMBER (unmapped, did we have a standard key in noatun for that one?)
*/
	foreach(const QString key, mediaObject->metaData().keys())
		kDebug(66666) << key << " => " << mediaObject->metaData(key);
}

void PlayerPrivate::_n_updateLength(qint64 msecLen)
{
	kDebug(66666) << "new length" << msecLen;
//	currentItem.setProperty("length", QString::number(msecLen));
	emit q->lengthChanged((int)msecLen);
}

void PlayerPrivate::_n_updatePosition(qint64 msecPos)
{
	//kDebug(66666) << "new pos" << msecPos;
	emit q->positionChanged((int)msecPos);
}

void PlayerPrivate::_n_updateVolume(qreal vol)
{
	//kDebug(66666) << "new volume" << vol;
	emit q->volumeChanged((int)(100.00 * vol + 0.5));
}






// -----------------------------------------------------------------------------


Player::Player()
    : d(new PlayerPrivate)
{
	d->q = this;
	setObjectName("Player");

	d->mediaObject = 0;
	d->audioOutput = 0;
	d->volumePercent = 50;
}


Player::~Player()
{
	delete d;
	kDebug(66666) ;
}

Player::State Player::state() const
{
	if (!d->mediaObject)
	{
		return Player::StoppedState;
	}
	return d->convertState(d->mediaObject->state());
}


bool Player::isPlaying() const
{
	return state() == Player::PlayingState;
}


bool Player::isPaused() const
{
	return state() == Player::PausedState;
}


bool Player::isStopped() const
{
	return state() == Player::StoppedState;
}


bool Player::isActive() const
{
	Player::State s = state();
	return s == Player::PausedState || s == Player::PlayingState;
}


void Player::stop()
{
	if (isStopped())
		return;
	d->mediaObject->stop();
}


void Player::pause()
{
	if (!isPlaying())
		return;
	d->mediaObject->pause();
}


void Player::play()
{
	Player::State st = state();
	if (st == Player::PlayingState)
		return;
	if (st == Player::PausedState)
		d->mediaObject->play(); // unpause
	else if (d->currentItem.get())
		play(*d->currentItem);
}


void Player::play(const File &item)
{
	d->initPhonon();
	kDebug(66666) << "Attempting to play...";
	if (!d->mediaObject)
		return;

	kDebug(66666) << "Starting to play new current track";
	d->currentItem.reset(new File(item));
	d->mediaObject->setCurrentSource(item.file());
	d->mediaObject->play();
	emit currentItemChanged(*d->currentItem);
}


void Player::playpause()
{
	if (isPlaying())
		pause();
	else
		play();
}

void Player::setPosition(unsigned int msec)
{
	if (d->mediaObject && isActive())
	{
		kDebug(66666) << "msec = " << msec;
		d->mediaObject->seek(msec);
		// Phonon is async, do not expect position() to have changed immediately
	}
}

unsigned int Player::position() const
{
	if (!d->mediaObject)
	{
		kWarning(66666) << "NO MEDIAOBJECT";
		return 0;
	}
	return d->mediaObject->currentTime();
}

static QString formatDuration(int duration)
{
	duration /= 1000; // convert from milliseconds to seconds
	int days    = duration / (60 * 60 * 24);
	int hours   = duration / (60 * 60) % 24;
	int minutes = duration / 60 % 60;
	int seconds = duration % 60;

	KLocale *loc = KGlobal::locale();
	const QChar zeroDigit = QLocale::system().zeroDigit();
	KLocalizedString str;

	if (days > 0)
	{
		QString dayStr = i18np("%1 day", "%1 days", days);
		str = ki18nc("<negativeSign><day(s)> <hours>:<minutes>:<seconds>",
				"%1%2 %3:%4:%5")
				.subs(duration < 0 ? loc->negativeSign() : QString())
				.subs(dayStr)
				.subs(hours, 2, 10, zeroDigit)
				.subs(minutes, 2, 10, zeroDigit)
				.subs(seconds, 2, 10, zeroDigit);
	}
	else if (hours > 0)
	{
		str = ki18nc("<negativeSign><hours>:<minutes>:<seconds>", "%1%2:%3:%4")
				.subs(duration < 0 ? loc->negativeSign() : QString())
				.subs(hours, 2, 10, zeroDigit)
				.subs(minutes, 2, 10, zeroDigit)
				.subs(seconds, 2, 10, zeroDigit);
	}
	else
	{
		str = ki18nc("<negativeSign><minutes>:<seconds>", "%1%2:%3")
				.subs(duration < 0 ? loc->negativeSign() : QString())
				.subs(minutes, 2, 10, zeroDigit)
				.subs(seconds, 2, 10, zeroDigit);
	}
	return str.toString();
}

QString Player::positionString() const
{
	/*if (d->nInstance->config()->displayRemaining())
	{
		int len = currentLength();
		if (len > 0)
			return formatDuration(len - position());
	}*/
	return formatDuration(position());
}

unsigned int Player::currentLength() const
{
	if (!d->mediaObject)
	{
		kWarning(66666) << "NO MEDIAOBJECT";
		return 0;
	}
	return d->mediaObject->totalTime();
}

QString Player::lengthString() const
{
	return formatDuration(currentLength());
}

int Player::volume() const
{
	if (!d->audioOutput)
	{
		kWarning(66666) << "Missing AudioOutput";
		return 0u;
	}
	return (int)(100.00 * d->audioOutput->volume() + 0.5);
}

void Player::setVolume(int percent)
{
	d->volumePercent = qBound(percent, 0, 100);
	if (d->audioOutput)
		d->audioOutput->setVolume(d->volumePercent * 0.01);
}

QStringList Player::mimeTypes() const
{
	return Phonon::BackendCapabilities::availableMimeTypes();
}

} // namespace

#include "player.moc"
// kate: space-indent off; replace-tabs off;

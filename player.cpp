/* This file is part of Meow

  Copyright 2000-2009 by Charles Samuels <charles@kde.org>
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
#include <qfile.h>
#include <qtimer.h>
#include <qstringlist.h>
#include <qlocale.h>

#include <kdebug.h>
#include <kurl.h>
#include <klocale.h>
#include <kglobal.h>


#include <cmath>

namespace Meow
{


Player::State PlayerPrivate::convertState(aKode::Player::State s)
{
	switch(s)
	{
	case aKode::Player::Open:
	case aKode::Player::Loaded:
	case aKode::Player::Playing:
		return Player::PlayingState;
	case aKode::Player::Paused:
		return Player::PausedState;
	default:
		return Player::StoppedState;
	}
}

void PlayerPrivate::initAvKode()
{
	if (akPlayer)
		return;
	akPlayer = new aKode::Player;
	akPlayer->open("auto");
	akPlayer->setManager(this);

	q->setVolume(volumePercent);
	q->setSpeed(100);
	
	QObject::connect(
			q, SIGNAL(stStateChangeEvent(int)),
			q, SLOT(tStateChangeEvent(int)),
			Qt::QueuedConnection
		);
	QObject::connect(
			q, SIGNAL(stEofEvent()),
			q, SLOT(tEofEvent()),
			Qt::QueuedConnection
		);
	QObject::connect(
			q, SIGNAL(stErrorEvent()),
			q, SLOT(tErrorEvent()),
			Qt::QueuedConnection
		);
}

void PlayerPrivate::stateChangeEvent(aKode::Player::State state)
{
	emit q->stStateChangeEvent(state);
}

void PlayerPrivate::eofEvent()
{
	emit q->stEofEvent();
}

void PlayerPrivate::errorEvent()
{
	emit q->stErrorEvent();
}

void PlayerPrivate::tStateChangeEvent(int newState)
{
	kDebug(66666) << "new state: " << newState;
	switch (newState)
	{
	case aKode::Player::Playing:
		emit q->playing();
		timer->start(500);
		break;
	case aKode::Player::Paused:
		emit q->paused();
		timer->stop();
		break;
	case aKode::Player::Loaded:
		if (nowLoading)
		{
			nowLoading = false;
			akPlayer->play();
		}
		else
		{
			emit q->stopped();
			timer->stop();
		}
	default:
		break;
	}
	emit q->stateChanged(convertState(aKode::Player::State(newState)));
}

void PlayerPrivate::tEofEvent()
{
	emit q->finished();
}

void PlayerPrivate::tErrorEvent()
{
	emit q->finished();
}

void PlayerPrivate::tick()
{
	if (aKode::Decoder *dec = akPlayer->decoder())
	{
		emit q->positionChanged(dec->length());
		emit q->lengthChanged(dec->position());
	}
}

// -----------------------------------------------------------------------------

Player::Player()
    : d(new PlayerPrivate)
{
	d->q = this;
	setObjectName("Player");
	d->timer = new QTimer(this);
	connect(d->timer, SIGNAL(timeout()), SLOT(tick()));

	d->akPlayer = 0;
	d->nowLoading = false;
}


Player::~Player()
{
	delete d->akPlayer;
	delete d;
	kDebug(66666);
}

Player::State Player::state() const
{
	if (!d->akPlayer)
		return Player::StoppedState;
	return d->convertState(d->akPlayer->state());
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
	d->akPlayer->stop();
	d->currentItem.reset();
}


void Player::pause()
{
	if (!isPlaying())
		return;
	d->akPlayer->pause();
}


void Player::play()
{
	Player::State st = state();
	if (d->nowLoading)
		d->akPlayer->play();
	else if (st == Player::PlayingState)
		return;
	else if (st == Player::PausedState)
		d->akPlayer->play(); // unpause
	else if (d->currentItem.get())
		play(*d->currentItem);
}


void Player::play(const File &item)
{
	d->initAvKode();
	kDebug(66666) << "Attempting to play...";
	if (!d->akPlayer)
		return;

	kDebug(66666) << "Starting to play new current track";
	d->currentItem.reset(new File(item));
	d->nowLoading = true;
	d->akPlayer->load( QFile::encodeName(item.file()).data() );
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
	if (d->akPlayer && d->akPlayer->decoder())
	{
		kDebug(66666) << "msec = " << msec;
		d->akPlayer->decoder()->seek(msec);
	}
}

unsigned int Player::position() const
{
	if (d->akPlayer && d->akPlayer->decoder())
		return d->akPlayer->decoder()->position();
	else
		return 0;
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
	if (d->akPlayer && d->akPlayer->decoder())
		return d->akPlayer->decoder()->length();
	else
		return 0;
}

QString Player::lengthString() const
{
	return formatDuration(currentLength());
}

int Player::volume() const
{
	return d->volumePercent;
}

void Player::setVolume(int percent)
{
	percent = qBound(0, percent, 100);
	double vol = (pow(10,percent*.01)-1)/(pow(10, 1)-1);
	if (d->akPlayer)
		d->akPlayer->setVolume(vol);
	
	if (d->volumePercent != percent)
	{
		d->volumePercent = percent;
		emit volumeChanged(percent);
	}
}

void Player::setSpeed(int percent)
{
	if (percent != d->speedPercent)
		return;
	d->speedPercent = percent;
	d->akPlayer->resampler()->setSpeed( percent/100.0 );
	emit speedChanged(percent);
}
int Player::speed() const
{
	return d->speedPercent;
}

File Player::currentFile() const
{
	if (!d->currentItem.get())
		return File();
	else
		return *d->currentItem;
}

QStringList Player::mimeTypes() const
{
	std::list<std::string> plugins = aKode::DecoderPluginHandler::listDecoderPlugins();
	QStringList m;
	for (
			std::list<std::string>::iterator i =plugins.begin();
			i != plugins.end();
			++i
		)
	{
		if ( *i == "mpeg")
			m << "audio/mpeg";
		else if ( *i == "xiph")
			m << "audio/ogg" << "audio/x-flac" << "audio/x-flac-ogg"
				<< "audio/x-speex" << "audio/x-speex+ogg";
		else if ( *i == "mpc")
			m << "audio/x-musepack";
		else if ( *i == "wav")
			m << "audio/x-wav";
		else if ( *i == "ffmpeg")
			m << "audio/x-ms-wma" << "audio/vnd.rn-realaudio";
	}
	return m;
}

} // namespace

#include "player.moc"
// kate: space-indent off; replace-tabs off;

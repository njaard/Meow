/* This file is part of Noatun

  Copyright 2000-2006 by Charles Samuels <charles@kde.org>
  Copyright 2000 by Stefan Westerfeld <stefan@space.twc.de>
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
#ifndef N_PLAYER_H
#define N_PLAYER_H

#include <qobject.h>

#include <vector>
#include <string>

namespace Meow
{

class File;
class PlayerPrivate;

/**
 * @brief player backend
 * @author Charles Samuels
 * @author Stefan Gehn
 *
 * This class has slots for all the common media player buttons
 *
 **/
class Player : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("Player", "org.kde.meow")
	friend class PlayerPrivate;

public:
	enum State
	{
		/**
		 * @brief Player is currently stopped
		 * @note currentItem() may return an invalid item in this state
		 **/
		StoppedState = 0,
		/**
		 * @brief Player is currently paused
		 **/
		PausedState  = 1,
		/**
		 * @brief Player is currently playing
		 **/
		PlayingState = 2
	};

	enum ErrorType
	{
		/**
		 * @brief An error that can be recovered from
		 *
		 * Typical errors that are of this type are:
		 * - cannot open file (not found, access forbidden)
		 * - socket problems (timeout, connection refused)
		 **/
		NormalError = 0,
		/**
		 * @brief An unrecoverable error
		 *
		 * Typical fatal errors can be:
		 * - could not open output device (already in use, access forbidden)
		 * - could not initialize backend (backend itself ended up in unusable state)
		 **/
		FatalError  = 1
	};

	explicit Player();
	~Player();
	
	/**
	 * @returns a list of available output device names
	 **/
	std::vector<std::pair<std::string,std::string>> devices() const;
	
	/**
	 * @returns the current device name
	 **/
	std::string currentDevice() const;
	
	/**
	 * set the current device by name
	 **/
	void setCurrentDevice(const std::string &name);

	/**
	 * @return the output volume in percent
	 **/
	int volume() const;

	/**
	 * @return the position in milliseconds
	 **/
	unsigned int position() const;

	/**
	 * Formatted string of value returned by position()
	 **/
	QString positionString() const;

	/**
	 * @return the track-length in milliseconds
	 **/
	unsigned int currentLength() const;

	/**
	 * Formatted string of value returned by currentLength()
	 **/
	QString lengthString() const;

	/**
	 * @return true if playing
	 **/
	bool isPlaying() const;

	/**
	 * @return true if paused
	 **/
	bool isPaused() const;

	/**
	 * @return true if stopped
	 **/
	bool isStopped() const;

	/**
	 * @return true if paused or playing
	 **/
	bool isActive() const;

	Player::State state() const;

	/**
	 * @return A list of mimetypes that Noatun can play using the currently
	 *         loaded engine-plugin
	 *
	 * Use mimeTypes().join(" ") if you want to set a
	 * filter in KFileDialog
	 */
	QStringList mimeTypes() const;
	
	File currentFile() const;
	
	Q_SCRIPTABLE QString currentTitle() const;
	Q_SCRIPTABLE QString currentArtist() const;

public Q_SLOTS:
	/**
	 * @brief Stops playback
	 *
	 * Undo this with play() or playpause().
	 * The current item playing will remain the same.
	 **/
	void stop();

	/**
	 * \brief Pauses playback
	 * Undo this with either play() or playpause()
	 **/
	void pause();

	/**
	 * @brief causes the current item to be played
	 **/
	void play();

	/**
	 * @brief causes @p item to be played
	 *
	 * Any current playback is stopped when @p item is valid.
	 **/
	void play(const File &item);

	/**
	 * start playing the current PlaylistItem, or pause if we're currently
	 * playing
	 **/
	void playpause();

	/**
	 * pause if @p is true, play otherwise
	 **/
	void playpause(bool p);

	/**
	 * @brief Set a new playback position (i.e. seek)
	 * @param msec new position given in milliseconds
	 **/
	void setPosition(unsigned int msec);
	
	void seekForward() { setPosition(position() + 5000); }
	void seekBackward() { setPosition(position() - 5000); }

	/**
	 * @brief Set playback volume
	 * @param percent new playback volume in percent (0 - 100)
	 **/
	Q_SCRIPTABLE void setVolume(int percent);
	
	void volumeUp() { setVolume(volume() + 5); }
	void volumeDown() { setVolume(volume() - 5); }

Q_SIGNALS:

	void currentItemChanged(const File &);

	/**
	 * Emitted on state changes.
	 * @param newState new state of the player
	 **/
	void stateChanged(const Player::State newState);

	void errorOccurred(Player::ErrorType errorType, const QString errorString);

	/**
	 * Convenience signal that is emitted together with
	 * signal stateChanged(StoppedState)
	 **/
	Q_SCRIPTABLE void stopped();
	/**
	 * Convenience signal that is emitted together with
	 * signal stateChanged(PlayingState)
	 **/
	Q_SCRIPTABLE void playing();
	Q_SCRIPTABLE void playing(bool);
	/**
	 * Convenience signal that is emitted together with
	 * signal stateChanged(PausedState)
	 **/
	Q_SCRIPTABLE void paused();
	
	Q_SCRIPTABLE void volumeChanged(int percent);
	Q_SCRIPTABLE void speedChanged(int percent);

	Q_SCRIPTABLE void positionChanged(int msec);

	Q_SCRIPTABLE void lengthChanged(int msec);
	
	Q_SCRIPTABLE void finished();

private:
	PlayerPrivate * const d;

private:
	Q_PRIVATE_SLOT(d, void tStateChangeEvent(int))
	Q_PRIVATE_SLOT(d, void tEofEvent())
	Q_PRIVATE_SLOT(d, void tErrorEvent())
	Q_PRIVATE_SLOT(d, void tick())

signals:
	void stStateChangeEvent(int);
	void stEofEvent();
	void stErrorEvent();
};

}
#endif
// kate: space-indent off; replace-tabs off;

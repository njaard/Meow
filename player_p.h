/* This file is part of Noatun

  Copyright 2007 by Stefan Gehn <mETz81@web.de>

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
#ifndef PLAYER_P_H
#define PLAYER_P_H

#include <phonon/mediaobject.h>
#include <phonon/path.h>
#include <phonon/audiooutput.h>
//#include <phonon/videopath.h>
//#include <phonon/ui/videowidget.h>
#include <phonon/backendcapabilities.h>

#include "player.h"
#include <db/file.h>

#include <memory>

namespace Meow
{


class PlayerPrivate
{
public:
	Player              *q;
	Phonon::MediaObject *mediaObject;
	//Phonon::VideoPath *videoPath;
	Phonon::AudioOutput *audioOutput;
	//Phonon::VideoWidget *videoWidget;
	std::auto_ptr<File> currentItem; // TODO: remove
	int volumePercent;

	void initPhonon();
	Player::State convertState(Phonon::State s);

	void _n_updateState(Phonon::State, Phonon::State);
	void _n_finishedPlaying();
	void _n_updateLength(qint64);
	void _n_updateMetaData();
	void _n_updatePosition(qint64);
};

}
#endif
// kate: space-indent off; replace-tabs off;

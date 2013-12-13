/*  aKode: Player class

    Copyright (C) 2004-2005 Allan Sandfeld Jensen <kde@carewolf.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef _AKODE_PLAYER_H
#define _AKODE_PLAYER_H

#include "akode_export.h"
#include "file.h"

#include <mutex>
#include <memory>

namespace aKode {

class File;
class Sink;
class Decoder;
class ResamplerPlugin;
class Resampler;
class AudioFrame;
class DecoderPlugin;

class ExceptionBase
{
public:
    virtual ~ExceptionBase() noexcept { }
    virtual const char* what() const noexcept =0;
};

template<class Type>
class Exception : public ExceptionBase, public Type
{
public:
    template<typename ...Args>
    Exception(const Args ...a)
        : Type(a...)
    { }
    
    Exception() = delete;
    Exception(const Exception &o)
        : Type(o)
    { }
    virtual const char* what() const noexcept
    {
        return Type::what();
    }
};


//! An implementation of a multithreaded aKode-player

/*!
 * The Player interface provides a clean and simple interface to multithreaded playback.
 * Notice however that the Player interface itself is not reentrant and should thus only
 * be used by a single thread.
 */
class AKODE_EXPORT Player
{
public:
    Player();
    ~Player();

    /*!
     * Opens a player that outputs to the sink \a sink
     *
     * The object is owned by and should be destroyed by the owner after close()
     * State: \a Closed -> \a Open
     */
    void open(std::shared_ptr<Sink> sink);

    /*!
     * Closes the player and releases the sink
     * Valid in all states.
     *
     * State: \a Open -> \a Closed
     */
    void close();

    /*!
     * Loads the file \a filename and prepares for playing.
     * Returns false if the file cannot be loaded or decoded.
     *
     * State: \a Open -> \a Loaded
     */
    void load(const FileName &filename);

    /*!
     * Loads the file \a file and prepares for playing.
     * Returns false if the file cannot be loaded or decoded.
     *
     * This version allows for overloaded aKode::File objects; useful for streaming.
     *
     * State: \a Open -> \a Loaded
     */
    void load(std::shared_ptr<File> file);

    /*!
     * Unload the file and release any resources allocated while loaded
     *
     * State: \a Loaded -> \a Open
     */
    void unload();

    /*!
     * Start playing.
     *
     * State: \a Loaded -> \a Playing
     */
    void play();
    /*!
     * Stop playing and release any resources allocated while playing.
     *
     * State: \a Playing -> \a Loaded
     *        \a Paused -> \a Loaded
     */
    void stop();
    /*!
     * Pause the player.
     *
     * State: \a Playing -> \a Paused
     */
    void pause();
    /*!
     * Resume the player from paused.
     *
     * State: \a Paused -> \a Playing
     */
    void resume();

    /*!
     * Set the software-volume to \a v. Use a number between 0.0 and 1.0.
     *
     * Valid in states \a Playing and \a Paused
     */
    void setVolume(float v);
    /*!
     * Returns the current value of the software-volume.
     *
     * Valid in states \a Playing and \a Paused
     */
    float volume() const;

    std::shared_ptr<File> file() const;
    std::shared_ptr<Sink> sink() const;
    /*!
     * Returns the current decoder interface.
     * Used for seek, position and length.
     */
    std::shared_ptr<Decoder> decoder() const;
    /*!
     * Returns the current resampler interface.
     * Used for adjusting playback speed.
     */
    std::shared_ptr<Resampler> resampler() const;

    enum State { Closed  = 0,
                 Open    = 2,
                 Loaded  = 4,
                 Playing = 8,
                 Paused  = 12 };

    /*!
     * Returns the current state of the Player
     */
    State state() const;

    /*!
     * An interface for Player callbacks
     *
     * Beware the callbacks come from a private thread, and methods from
     * the Player interface should not be called from the callback.
     */
    class Manager
    {
    public:
        virtual ~Manager() { }
        /*!
         * Called for all user-generated state changes (User thread)
         */
        virtual void stateChangeEvent(Player::State)=0;
        /*!
         * Called when a decoder halts because it has reached end of file (Local thread).
         * The callee should effect a Player::stop()
         */
        virtual void eofEvent()=0;
        /*!
         * Called when a decoder halts because of a fatal error (Local thread).
         * The callee should effect a Player::stop()
         */
        virtual void errorEvent()=0;
    };

    /*!
     * Sets an associated callback interface.
     * Can only be set before open() is called.
     */
    void setManager(std::shared_ptr<Manager> manager);

    void registerDecoderPlugin(DecoderPlugin *decoder);

    /*!
     * Sets the resampler plugin to use. Default is "fast".
     */
    void setResamplerPlugin(std::shared_ptr<ResamplerPlugin> resampler);

    /*!
     * A Monitor is sink-like device.
     */
    class Monitor
    {
    public:
        virtual ~Monitor(){}
        virtual void writeFrame(AudioFrame* frame) = 0;
    };

    /*!
     * Sets a secondary sink to monitor output
     * Can only be set before play() is called.
     */
    void setMonitor(std::shared_ptr<Monitor> monitor);

private:
    struct private_data;
    private_data *d;

    void load();
    void setState(State state);
};

} // namespace

#endif

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

/**
 * @brief Enumeration of media playback states
 */
enum class PlaybackState
{
    Stopped,
    Playing,
    Paused,
    Buffering,
    Error
};

/**
 * @brief Interface for media engine implementations
 * 
 * Defines the contract for media playback engines that can be used
 * by the media player application.
 */
class IMediaEngine : public QObject
{
    Q_OBJECT

public:
    virtual ~IMediaEngine() = default;
    
    /**
     * @brief Load media from file path or URL
     * @param path File path or URL to load
     * @return true if media was loaded successfully
     */
    virtual bool loadMedia(const QString& path) = 0;
    
    /**
     * @brief Start or resume playback
     */
    virtual void play() = 0;
    
    /**
     * @brief Pause playback
     */
    virtual void pause() = 0;
    
    /**
     * @brief Stop playback
     */
    virtual void stop() = 0;
    
    /**
     * @brief Seek to specific position
     * @param position Position in milliseconds
     */
    virtual void seek(qint64 position) = 0;
    
    /**
     * @brief Set playback volume
     * @param volume Volume level (0-100)
     */
    virtual void setVolume(int volume) = 0;
    
    /**
     * @brief Get current playback state
     * @return Current playback state
     */
    virtual PlaybackState state() const = 0;
    
    /**
     * @brief Get current playback position
     * @return Position in milliseconds
     */
    virtual qint64 position() const = 0;
    
    /**
     * @brief Get media duration
     * @return Duration in milliseconds
     */
    virtual qint64 duration() const = 0;
    
    /**
     * @brief Get current volume level
     * @return Volume level (0-100)
     */
    virtual int volume() const = 0;
    
    /**
     * @brief Check if media is currently loaded
     * @return true if media is loaded
     */
    virtual bool hasMedia() const = 0;
    
    /**
     * @brief Set playback rate/speed
     * @param rate Playback rate (1.0 = normal speed, 2.0 = double speed, etc.)
     */
    virtual void setPlaybackRate(double rate) { Q_UNUSED(rate); }
    
    /**
     * @brief Get current playback rate
     * @return Current playback rate
     */
    virtual double playbackRate() const { return 1.0; }
    
    /**
     * @brief Check if the media has video content
     * @return true if media contains video
     */
    virtual bool hasVideo() const { return false; }
    
    /**
     * @brief Get video frame at specific position (for thumbnails)
     * @param position Position in milliseconds
     * @return Base64 encoded frame image or empty string
     */
    virtual QString getVideoFrame(qint64 position) { Q_UNUSED(position); return QString(); }

signals:
    /**
     * @brief Emitted when playback state changes
     * @param state New playback state
     */
    void stateChanged(PlaybackState state);
    
    /**
     * @brief Emitted when playback position changes
     * @param position Current position in milliseconds
     */
    void positionChanged(qint64 position);
    
    /**
     * @brief Emitted when media duration is available
     * @param duration Duration in milliseconds
     */
    void durationChanged(qint64 duration);
    
    /**
     * @brief Emitted when volume changes
     * @param volume New volume level (0-100)
     */
    void volumeChanged(int volume);
    
    /**
     * @brief Emitted when media loading completes
     * @param success true if loading was successful
     */
    void mediaLoaded(bool success);
    
    /**
     * @brief Emitted when an error occurs
     * @param error Error description
     */
    void errorOccurred(const QString& error);
};
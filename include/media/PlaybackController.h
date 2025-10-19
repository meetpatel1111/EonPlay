#pragma once

#include "IMediaEngine.h"
#include "IComponent.h"
#include <QObject>
#include <QTimer>
#include <QHash>
#include <memory>

/**
 * @brief Playback speed enumeration for common speeds
 */
enum class PlaybackSpeed
{
    Quarter = 25,      // 0.25x
    Half = 50,         // 0.5x
    Normal = 100,      // 1.0x
    OneAndHalf = 150,  // 1.5x
    Double = 200,      // 2.0x
    Quadruple = 400    // 4.0x
};

/**
 * @brief Playback mode enumeration
 */
enum class PlaybackMode
{
    Normal,     // Play once and stop
    RepeatOne,  // Repeat current track
    RepeatAll,  // Repeat entire playlist
    Shuffle     // Shuffle playlist order
};

/**
 * @brief Fast forward/rewind speed enumeration
 */
enum class SeekSpeed
{
    Slow = 2,      // 2x speed
    Normal = 5,    // 5x speed
    Fast = 10,     // 10x speed
    VeryFast = 20  // 20x speed
};

/**
 * @brief EonPlay core playback controller that manages media playback operations
 * 
 * Provides high-level playback control functionality built on top of
 * the IMediaEngine interface. Handles play/pause/stop operations,
 * seeking, volume control, playback speed management, and EonPlay's
 * advanced timeless features.
 */
class PlaybackController : public QObject, public IComponent
{
    Q_OBJECT

public:
    explicit PlaybackController(QObject* parent = nullptr);
    ~PlaybackController() override;
    
    // IComponent interface
    bool initialize() override;
    void shutdown() override;
    QString componentName() const override { return "PlaybackController"; }
    bool isInitialized() const override { return m_initialized; }
    
    /**
     * @brief Set the media engine to control
     * @param engine Shared pointer to media engine
     */
    void setMediaEngine(std::shared_ptr<IMediaEngine> engine);
    
    /**
     * @brief Get the current media engine
     * @return Shared pointer to media engine or nullptr
     */
    std::shared_ptr<IMediaEngine> mediaEngine() const { return m_mediaEngine; }
    
    // Core playback controls
    
    /**
     * @brief Start or resume playback
     */
    void play();
    
    /**
     * @brief Pause playback
     */
    void pause();
    
    /**
     * @brief Stop playback
     */
    void stop();
    
    /**
     * @brief Toggle between play and pause
     */
    void togglePlayPause();
    
    // Seeking operations
    
    /**
     * @brief Seek to specific position
     * @param position Position in milliseconds
     */
    void seek(qint64 position);
    
    /**
     * @brief Seek forward by specified amount
     * @param milliseconds Amount to seek forward (default: 10 seconds)
     */
    void seekForward(qint64 milliseconds = 10000);
    
    /**
     * @brief Seek backward by specified amount
     * @param milliseconds Amount to seek backward (default: 10 seconds)
     */
    void seekBackward(qint64 milliseconds = 10000);
    
    /**
     * @brief Seek to percentage of total duration
     * @param percentage Percentage (0-100)
     */
    void seekToPercentage(double percentage);
    
    // Volume control
    
    /**
     * @brief Set volume level
     * @param volume Volume level (0-100)
     */
    void setVolume(int volume);
    
    /**
     * @brief Get current volume level
     * @return Volume level (0-100)
     */
    int volume() const;
    
    /**
     * @brief Increase volume by step
     * @param step Volume step (default: 5)
     */
    void volumeUp(int step = 5);
    
    /**
     * @brief Decrease volume by step
     * @param step Volume step (default: 5)
     */
    void volumeDown(int step = 5);
    
    /**
     * @brief Toggle mute state
     */
    void toggleMute();
    
    /**
     * @brief Set mute state
     * @param muted true to mute, false to unmute
     */
    void setMuted(bool muted);
    
    /**
     * @brief Check if audio is muted
     * @return true if muted
     */
    bool isMuted() const { return m_muted; }
    
    // Playback speed control
    
    /**
     * @brief Set playback speed
     * @param speed Speed as percentage (100 = normal speed)
     */
    void setPlaybackSpeed(int speed);
    
    /**
     * @brief Set playback speed using enum
     * @param speed Predefined speed value
     */
    void setPlaybackSpeed(PlaybackSpeed speed);
    
    /**
     * @brief Get current playback speed
     * @return Speed as percentage (100 = normal speed)
     */
    int playbackSpeed() const { return m_playbackSpeed; }
    
    /**
     * @brief Reset playback speed to normal
     */
    void resetPlaybackSpeed();
    
    // Position and duration reporting
    
    /**
     * @brief Get current playback position
     * @return Position in milliseconds
     */
    qint64 position() const;
    
    /**
     * @brief Get media duration
     * @return Duration in milliseconds
     */
    qint64 duration() const;
    
    /**
     * @brief Get current playback state
     * @return Current playback state
     */
    PlaybackState state() const;
    
    /**
     * @brief Check if media is currently loaded
     * @return true if media is loaded
     */
    bool hasMedia() const;
    
    /**
     * @brief Get position as percentage of duration
     * @return Position percentage (0.0-100.0)
     */
    double positionPercentage() const;
    
    // Advanced playback features
    
    /**
     * @brief Start fast forward with specified speed
     * @param speed Fast forward speed multiplier
     */
    void startFastForward(SeekSpeed speed = SeekSpeed::Normal);
    
    /**
     * @brief Start rewind with specified speed
     * @param speed Rewind speed multiplier
     */
    void startRewind(SeekSpeed speed = SeekSpeed::Normal);
    
    /**
     * @brief Stop fast forward/rewind and return to normal playback
     */
    void stopFastSeek();
    
    /**
     * @brief Check if currently fast seeking
     * @return true if fast forwarding or rewinding
     */
    bool isFastSeeking() const { return m_fastSeekActive; }
    
    /**
     * @brief Step forward one frame (video only)
     */
    void stepForward();
    
    /**
     * @brief Step backward one frame (video only)
     */
    void stepBackward();
    
    /**
     * @brief Set playback mode (normal, repeat, shuffle)
     * @param mode Playback mode to set
     */
    void setPlaybackMode(PlaybackMode mode);
    
    /**
     * @brief Get current playback mode
     * @return Current playback mode
     */
    PlaybackMode playbackMode() const { return m_playbackMode; }
    
    /**
     * @brief Enable or disable crossfade between tracks
     * @param enabled true to enable crossfade
     * @param duration Crossfade duration in milliseconds
     */
    void setCrossfadeEnabled(bool enabled, int duration = 3000);
    
    /**
     * @brief Check if crossfade is enabled
     * @return true if crossfade is enabled
     */
    bool isCrossfadeEnabled() const { return m_crossfadeEnabled; }
    
    /**
     * @brief Get crossfade duration
     * @return Crossfade duration in milliseconds
     */
    int crossfadeDuration() const { return m_crossfadeDuration; }
    
    /**
     * @brief Enable or disable gapless playback
     * @param enabled true to enable gapless playback
     */
    void setGaplessPlayback(bool enabled);
    
    /**
     * @brief Check if gapless playback is enabled
     * @return true if gapless playback is enabled
     */
    bool isGaplessPlayback() const { return m_gaplessPlayback; }
    
    /**
     * @brief Save current playback position for resume
     * @param filePath File path to save position for
     */
    void saveResumePosition(const QString& filePath);
    
    /**
     * @brief Load and seek to saved resume position
     * @param filePath File path to load position for
     * @return true if resume position was found and applied
     */
    bool loadResumePosition(const QString& filePath);
    
    /**
     * @brief Clear saved resume position
     * @param filePath File path to clear position for
     */
    void clearResumePosition(const QString& filePath);
    
    /**
     * @brief Generate thumbnail for seek preview
     * @param position Position in milliseconds
     * @return Base64 encoded thumbnail image or empty string if not available
     */
    QString generateSeekThumbnail(qint64 position);
    
    /**
     * @brief Enable or disable seek thumbnail generation
     * @param enabled true to enable thumbnail generation
     */
    void setSeekThumbnailEnabled(bool enabled);
    
    /**
     * @brief Check if seek thumbnails are enabled
     * @return true if seek thumbnails are enabled
     */
    bool isSeekThumbnailEnabled() const { return m_seekThumbnailEnabled; }

public slots:
    /**
     * @brief Load media file or URL
     * @param path File path or URL to load
     */
    void loadMedia(const QString& path);

signals:
    // Playback control signals
    
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
     * @brief Emitted when mute state changes
     * @param muted New mute state
     */
    void muteChanged(bool muted);
    
    /**
     * @brief Emitted when playback speed changes
     * @param speed New speed as percentage
     */
    void playbackSpeedChanged(int speed);
    
    /**
     * @brief Emitted when media loading completes
     * @param success true if loading was successful
     * @param path Path of loaded media
     */
    void mediaLoaded(bool success, const QString& path);
    
    /**
     * @brief Emitted when an error occurs
     * @param error Error description
     */
    void errorOccurred(const QString& error);
    
    /**
     * @brief Emitted when playback mode changes
     * @param mode New playback mode
     */
    void playbackModeChanged(PlaybackMode mode);
    
    /**
     * @brief Emitted when fast seek state changes
     * @param active true if fast seeking is active
     * @param speed Current seek speed multiplier
     */
    void fastSeekChanged(bool active, int speed);
    
    /**
     * @brief Emitted when crossfade settings change
     * @param enabled true if crossfade is enabled
     * @param duration Crossfade duration in milliseconds
     */
    void crossfadeChanged(bool enabled, int duration);
    
    /**
     * @brief Emitted when gapless playback setting changes
     * @param enabled true if gapless playback is enabled
     */
    void gaplessPlaybackChanged(bool enabled);
    
    /**
     * @brief Emitted when a seek thumbnail is generated
     * @param position Position in milliseconds
     * @param thumbnail Base64 encoded thumbnail image
     */
    void seekThumbnailGenerated(qint64 position, const QString& thumbnail);

private slots:
    /**
     * @brief Handle media engine state changes
     * @param state New state from media engine
     */
    void onEngineStateChanged(PlaybackState state);
    
    /**
     * @brief Handle media engine position changes
     * @param position New position from media engine
     */
    void onEnginePositionChanged(qint64 position);
    
    /**
     * @brief Handle media engine duration changes
     * @param duration New duration from media engine
     */
    void onEngineDurationChanged(qint64 duration);
    
    /**
     * @brief Handle media engine volume changes
     * @param volume New volume from media engine
     */
    void onEngineVolumeChanged(int volume);
    
    /**
     * @brief Handle media engine errors
     * @param error Error message from media engine
     */
    void onEngineError(const QString& error);
    
    /**
     * @brief Handle media loading completion
     * @param success Loading success status
     */
    void onEngineMediaLoaded(bool success);
    
    /**
     * @brief Handle fast seek timer timeout
     */
    void onFastSeekTimer();
    
    /**
     * @brief Handle crossfade timer timeout
     */
    void onCrossfadeTimer();

private:
    /**
     * @brief Connect to media engine signals
     */
    void connectEngineSignals();
    
    /**
     * @brief Disconnect from media engine signals
     */
    void disconnectEngineSignals();
    
    /**
     * @brief Validate and clamp volume value
     * @param volume Volume to validate
     * @return Clamped volume value (0-100)
     */
    int clampVolume(int volume) const;
    
    /**
     * @brief Validate and clamp playback speed
     * @param speed Speed to validate
     * @return Clamped speed value
     */
    int clampPlaybackSpeed(int speed) const;
    
    std::shared_ptr<IMediaEngine> m_mediaEngine;
    bool m_initialized;
    
    // State tracking
    int m_volumeBeforeMute;
    bool m_muted;
    int m_playbackSpeed;
    QString m_currentMediaPath;
    
    // Advanced playback state
    PlaybackMode m_playbackMode;
    bool m_fastSeekActive;
    int m_fastSeekSpeed;
    bool m_fastSeekForward;
    QTimer* m_fastSeekTimer;
    
    // Crossfade settings
    bool m_crossfadeEnabled;
    int m_crossfadeDuration;
    QTimer* m_crossfadeTimer;
    bool m_crossfadeActive;
    
    // Gapless playback
    bool m_gaplessPlayback;
    
    // Resume positions (file path -> position in ms)
    QHash<QString, qint64> m_resumePositions;
    
    // Seek thumbnails
    bool m_seekThumbnailEnabled;
    QHash<qint64, QString> m_thumbnailCache; // position -> base64 thumbnail
    
    // Speed limits
    static constexpr int MIN_PLAYBACK_SPEED = 10;   // 0.1x
    static constexpr int MAX_PLAYBACK_SPEED = 1000; // 10.0x
    static constexpr int FAST_SEEK_INTERVAL_MS = 100; // Fast seek update interval
    static constexpr int FRAME_STEP_MS = 33; // Approximate frame duration for 30fps
};
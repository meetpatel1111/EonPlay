#include "media/PlaybackController.h"
#include <QLoggingCategory>
#include <QDebug>
#include <QHash>
#include <algorithm>

Q_LOGGING_CATEGORY(playbackController, "eonplay.playbackcontroller")

PlaybackController::PlaybackController(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_volumeBeforeMute(100)
    , m_muted(false)
    , m_playbackSpeed(100)
    , m_playbackMode(PlaybackMode::Normal)
    , m_fastSeekActive(false)
    , m_fastSeekSpeed(1)
    , m_fastSeekForward(true)
    , m_fastSeekTimer(new QTimer(this))
    , m_crossfadeEnabled(false)
    , m_crossfadeDuration(3000)
    , m_crossfadeTimer(new QTimer(this))
    , m_crossfadeActive(false)
    , m_gaplessPlayback(false)
    , m_seekThumbnailEnabled(true)
{
    // Set up fast seek timer
    m_fastSeekTimer->setSingleShot(false);
    m_fastSeekTimer->setInterval(FAST_SEEK_INTERVAL_MS);
    connect(m_fastSeekTimer, &QTimer::timeout, this, &PlaybackController::onFastSeekTimer);
    
    // Set up crossfade timer
    m_crossfadeTimer->setSingleShot(true);
    connect(m_crossfadeTimer, &QTimer::timeout, this, &PlaybackController::onCrossfadeTimer);
    
    qCDebug(playbackController) << "PlaybackController created";
}

PlaybackController::~PlaybackController()
{
    shutdown();
    qCDebug(playbackController) << "PlaybackController destroyed";
}

bool PlaybackController::initialize()
{
    if (m_initialized) {
        qCWarning(playbackController) << "PlaybackController already initialized";
        return true;
    }
    
    qCDebug(playbackController) << "Initializing PlaybackController";
    
    // PlaybackController can be initialized without a media engine
    // The media engine can be set later via setMediaEngine()
    
    m_initialized = true;
    qCDebug(playbackController) << "PlaybackController initialized successfully";
    
    return true;
}

void PlaybackController::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qCDebug(playbackController) << "Shutting down PlaybackController";
    
    // Stop any active timers
    m_fastSeekTimer->stop();
    m_crossfadeTimer->stop();
    
    // Save resume positions if enabled
    // This would typically be handled by the settings manager
    
    // Disconnect from media engine if connected
    if (m_mediaEngine) {
        disconnectEngineSignals();
        m_mediaEngine.reset();
    }
    
    m_initialized = false;
    qCDebug(playbackController) << "PlaybackController shut down";
}

void PlaybackController::setMediaEngine(std::shared_ptr<IMediaEngine> engine)
{
    if (m_mediaEngine == engine) {
        return;
    }
    
    // Disconnect from previous engine
    if (m_mediaEngine) {
        disconnectEngineSignals();
    }
    
    m_mediaEngine = engine;
    
    // Connect to new engine
    if (m_mediaEngine) {
        connectEngineSignals();
        qCDebug(playbackController) << "Media engine connected to PlaybackController";
    } else {
        qCDebug(playbackController) << "Media engine disconnected from PlaybackController";
    }
}

void PlaybackController::play()
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot play: No media engine available";
        emit errorOccurred("No media engine available");
        return;
    }
    
    qCDebug(playbackController) << "Starting playback";
    m_mediaEngine->play();
}

void PlaybackController::pause()
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot pause: No media engine available";
        return;
    }
    
    qCDebug(playbackController) << "Pausing playback";
    m_mediaEngine->pause();
}

void PlaybackController::stop()
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot stop: No media engine available";
        return;
    }
    
    qCDebug(playbackController) << "Stopping playback";
    m_mediaEngine->stop();
}

void PlaybackController::togglePlayPause()
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot toggle play/pause: No media engine available";
        return;
    }
    
    PlaybackState currentState = m_mediaEngine->state();
    
    switch (currentState) {
        case PlaybackState::Playing:
            pause();
            break;
            
        case PlaybackState::Paused:
        case PlaybackState::Stopped:
            play();
            break;
            
        case PlaybackState::Buffering:
            // Don't interrupt buffering
            qCDebug(playbackController) << "Cannot toggle play/pause while buffering";
            break;
            
        case PlaybackState::Error:
            qCWarning(playbackController) << "Cannot toggle play/pause: Media engine in error state";
            break;
    }
}

void PlaybackController::seek(qint64 position)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot seek: No media engine available";
        return;
    }
    
    // Clamp position to valid range
    qint64 duration = m_mediaEngine->duration();
    if (duration > 0) {
        position = std::clamp(position, 0LL, duration);
    } else {
        position = std::max(0LL, position);
    }
    
    qCDebug(playbackController) << "Seeking to position:" << position;
    m_mediaEngine->seek(position);
}

void PlaybackController::seekForward(qint64 milliseconds)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot seek forward: No media engine available";
        return;
    }
    
    qint64 currentPosition = m_mediaEngine->position();
    qint64 newPosition = currentPosition + milliseconds;
    
    qCDebug(playbackController) << "Seeking forward by" << milliseconds << "ms";
    seek(newPosition);
}

void PlaybackController::seekBackward(qint64 milliseconds)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot seek backward: No media engine available";
        return;
    }
    
    qint64 currentPosition = m_mediaEngine->position();
    qint64 newPosition = currentPosition - milliseconds;
    
    qCDebug(playbackController) << "Seeking backward by" << milliseconds << "ms";
    seek(newPosition);
}

void PlaybackController::seekToPercentage(double percentage)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot seek to percentage: No media engine available";
        return;
    }
    
    qint64 duration = m_mediaEngine->duration();
    if (duration <= 0) {
        qCWarning(playbackController) << "Cannot seek to percentage: Invalid duration";
        return;
    }
    
    // Clamp percentage to valid range
    percentage = std::clamp(percentage, 0.0, 100.0);
    
    qint64 position = static_cast<qint64>((percentage / 100.0) * duration);
    
    qCDebug(playbackController) << "Seeking to" << percentage << "% (" << position << "ms)";
    seek(position);
}

void PlaybackController::setVolume(int volume)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot set volume: No media engine available";
        return;
    }
    
    volume = clampVolume(volume);
    
    qCDebug(playbackController) << "Setting volume to:" << volume;
    
    // If we're setting volume while muted, unmute
    if (m_muted && volume > 0) {
        m_muted = false;
        emit muteChanged(false);
    }
    
    m_mediaEngine->setVolume(volume);
}

int PlaybackController::volume() const
{
    if (!m_mediaEngine) {
        return 0;
    }
    
    return m_mediaEngine->volume();
}

void PlaybackController::volumeUp(int step)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot increase volume: No media engine available";
        return;
    }
    
    int currentVolume = m_mediaEngine->volume();
    int newVolume = currentVolume + step;
    
    setVolume(newVolume);
}

void PlaybackController::volumeDown(int step)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot decrease volume: No media engine available";
        return;
    }
    
    int currentVolume = m_mediaEngine->volume();
    int newVolume = currentVolume - step;
    
    setVolume(newVolume);
}

void PlaybackController::toggleMute()
{
    setMuted(!m_muted);
}

void PlaybackController::setMuted(bool muted)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot set mute: No media engine available";
        return;
    }
    
    if (m_muted == muted) {
        return;
    }
    
    qCDebug(playbackController) << "Setting mute to:" << muted;
    
    if (muted) {
        // Store current volume and mute
        m_volumeBeforeMute = m_mediaEngine->volume();
        m_mediaEngine->setVolume(0);
    } else {
        // Restore previous volume
        m_mediaEngine->setVolume(m_volumeBeforeMute);
    }
    
    m_muted = muted;
    emit muteChanged(muted);
}

void PlaybackController::setPlaybackSpeed(int speed)
{
    speed = clampPlaybackSpeed(speed);
    
    if (m_playbackSpeed == speed) {
        return;
    }
    
    qCDebug(playbackController) << "Setting playback speed to:" << speed << "%";
    
    m_playbackSpeed = speed;
    
    // Set playback rate on media engine if available
    if (m_mediaEngine) {
        double rate = static_cast<double>(speed) / 100.0;
        m_mediaEngine->setPlaybackRate(rate);
    }
    
    emit playbackSpeedChanged(speed);
}

void PlaybackController::setPlaybackSpeed(PlaybackSpeed speed)
{
    setPlaybackSpeed(static_cast<int>(speed));
}

void PlaybackController::resetPlaybackSpeed()
{
    setPlaybackSpeed(100); // Normal speed
}

qint64 PlaybackController::position() const
{
    if (!m_mediaEngine) {
        return 0;
    }
    
    return m_mediaEngine->position();
}

qint64 PlaybackController::duration() const
{
    if (!m_mediaEngine) {
        return 0;
    }
    
    return m_mediaEngine->duration();
}

PlaybackState PlaybackController::state() const
{
    if (!m_mediaEngine) {
        return PlaybackState::Stopped;
    }
    
    return m_mediaEngine->state();
}

bool PlaybackController::hasMedia() const
{
    if (!m_mediaEngine) {
        return false;
    }
    
    return m_mediaEngine->hasMedia();
}

double PlaybackController::positionPercentage() const
{
    if (!m_mediaEngine) {
        return 0.0;
    }
    
    qint64 pos = m_mediaEngine->position();
    qint64 dur = m_mediaEngine->duration();
    
    if (dur <= 0) {
        return 0.0;
    }
    
    return (static_cast<double>(pos) / static_cast<double>(dur)) * 100.0;
}

void PlaybackController::loadMedia(const QString& path)
{
    if (!m_mediaEngine) {
        qCWarning(playbackController) << "Cannot load media: No media engine available";
        emit mediaLoaded(false, path);
        return;
    }
    
    qCDebug(playbackController) << "Loading media:" << path;
    
    // Save resume position for previous media if it was playing
    if (!m_currentMediaPath.isEmpty() && hasMedia()) {
        saveResumePosition(m_currentMediaPath);
    }
    
    // Stop any active fast seeking
    if (m_fastSeekActive) {
        stopFastSeek();
    }
    
    // Clear thumbnail cache for new media
    m_thumbnailCache.clear();
    
    m_currentMediaPath = path;
    bool success = m_mediaEngine->loadMedia(path);
    
    if (success) {
        qCDebug(playbackController) << "Media loaded successfully";
        
        // Try to load resume position after a short delay to ensure media is ready
        QTimer::singleShot(100, this, [this, path]() {
            if (m_currentMediaPath == path) {
                loadResumePosition(path);
            }
        });
    } else {
        qCWarning(playbackController) << "Failed to load media";
    }
    
    emit mediaLoaded(success, path);
}

void PlaybackController::connectEngineSignals()
{
    if (!m_mediaEngine) {
        return;
    }
    
    connect(m_mediaEngine.get(), &IMediaEngine::stateChanged,
            this, &PlaybackController::onEngineStateChanged);
    connect(m_mediaEngine.get(), &IMediaEngine::playbackPositionChanged,
            this, &PlaybackController::onEnginePositionChanged);
    connect(m_mediaEngine.get(), &IMediaEngine::playbackDurationChanged,
            this, &PlaybackController::onEngineDurationChanged);
    connect(m_mediaEngine.get(), &IMediaEngine::volumeChanged,
            this, &PlaybackController::onEngineVolumeChanged);
    connect(m_mediaEngine.get(), &IMediaEngine::errorOccurred,
            this, &PlaybackController::onEngineError);
    connect(m_mediaEngine.get(), &IMediaEngine::mediaLoaded,
            this, &PlaybackController::onEngineMediaLoaded);
}

void PlaybackController::disconnectEngineSignals()
{
    if (!m_mediaEngine) {
        return;
    }
    
    disconnect(m_mediaEngine.get(), nullptr, this, nullptr);
}

int PlaybackController::clampVolume(int volume) const
{
    return std::clamp(volume, 0, 100);
}

int PlaybackController::clampPlaybackSpeed(int speed) const
{
    return std::clamp(speed, MIN_PLAYBACK_SPEED, MAX_PLAYBACK_SPEED);
}

void PlaybackController::onEngineStateChanged(PlaybackState state)
{
    qCDebug(playbackController) << "Engine state changed to:" << static_cast<int>(state);
    emit stateChanged(state);
}

void PlaybackController::onEnginePositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void PlaybackController::onEngineDurationChanged(qint64 duration)
{
    qCDebug(playbackController) << "Engine duration changed to:" << duration;
    emit durationChanged(duration);
}

void PlaybackController::onEngineVolumeChanged(int volume)
{
    qCDebug(playbackController) << "Engine volume changed to:" << volume;
    
    // Update mute state based on volume
    if (volume == 0 && !m_muted) {
        m_muted = true;
        emit muteChanged(true);
    } else if (volume > 0 && m_muted) {
        m_muted = false;
        emit muteChanged(false);
    }
    
    emit volumeChanged(volume);
}

void PlaybackController::onEngineError(const QString& error)
{
    qCCritical(playbackController) << "Engine error:" << error;
    emit errorOccurred(error);
}

void PlaybackController::onEngineMediaLoaded(bool success)
{
    qCDebug(playbackController) << "Engine media loaded:" << success;
    emit mediaLoaded(success, m_currentMediaPath);
}

// Advanced playback features implementation

void PlaybackController::startFastForward(SeekSpeed speed)
{
    if (!m_mediaEngine || !hasMedia()) {
        qCWarning(playbackController) << "Cannot start fast forward: No media available";
        return;
    }
    
    qCDebug(playbackController) << "Starting fast forward at" << static_cast<int>(speed) << "x speed";
    
    m_fastSeekActive = true;
    m_fastSeekForward = true;
    m_fastSeekSpeed = static_cast<int>(speed);
    
    // Pause normal playback during fast seek
    if (state() == PlaybackState::Playing) {
        pause();
    }
    
    m_fastSeekTimer->start();
    emit fastSeekChanged(true, m_fastSeekSpeed);
}

void PlaybackController::startRewind(SeekSpeed speed)
{
    if (!m_mediaEngine || !hasMedia()) {
        qCWarning(playbackController) << "Cannot start rewind: No media available";
        return;
    }
    
    qCDebug(playbackController) << "Starting rewind at" << static_cast<int>(speed) << "x speed";
    
    m_fastSeekActive = true;
    m_fastSeekForward = false;
    m_fastSeekSpeed = static_cast<int>(speed);
    
    // Pause normal playback during fast seek
    if (state() == PlaybackState::Playing) {
        pause();
    }
    
    m_fastSeekTimer->start();
    emit fastSeekChanged(true, m_fastSeekSpeed);
}

void PlaybackController::stopFastSeek()
{
    if (!m_fastSeekActive) {
        return;
    }
    
    qCDebug(playbackController) << "Stopping fast seek";
    
    m_fastSeekTimer->stop();
    m_fastSeekActive = false;
    
    // Resume normal playback
    play();
    
    emit fastSeekChanged(false, 1);
}

void PlaybackController::stepForward()
{
    if (!m_mediaEngine || !hasMedia()) {
        qCWarning(playbackController) << "Cannot step forward: No media available";
        return;
    }
    
    qint64 currentPos = position();
    qint64 newPos = currentPos + FRAME_STEP_MS;
    
    qCDebug(playbackController) << "Stepping forward one frame";
    
    // Pause playback for frame stepping
    if (state() == PlaybackState::Playing) {
        pause();
    }
    
    seek(newPos);
}

void PlaybackController::stepBackward()
{
    if (!m_mediaEngine || !hasMedia()) {
        qCWarning(playbackController) << "Cannot step backward: No media available";
        return;
    }
    
    qint64 currentPos = position();
    qint64 newPos = currentPos - FRAME_STEP_MS;
    
    qCDebug(playbackController) << "Stepping backward one frame";
    
    // Pause playback for frame stepping
    if (state() == PlaybackState::Playing) {
        pause();
    }
    
    seek(newPos);
}

void PlaybackController::setPlaybackMode(PlaybackMode mode)
{
    if (m_playbackMode == mode) {
        return;
    }
    
    qCDebug(playbackController) << "Setting playback mode to:" << static_cast<int>(mode);
    
    m_playbackMode = mode;
    emit playbackModeChanged(mode);
}

void PlaybackController::setCrossfadeEnabled(bool enabled, int duration)
{
    if (m_crossfadeEnabled == enabled && m_crossfadeDuration == duration) {
        return;
    }
    
    qCDebug(playbackController) << "Setting crossfade:" << enabled << "duration:" << duration;
    
    m_crossfadeEnabled = enabled;
    m_crossfadeDuration = std::max(500, std::min(10000, duration)); // Clamp between 0.5-10 seconds
    
    emit crossfadeChanged(m_crossfadeEnabled, m_crossfadeDuration);
}

void PlaybackController::setGaplessPlayback(bool enabled)
{
    if (m_gaplessPlayback == enabled) {
        return;
    }
    
    qCDebug(playbackController) << "Setting gapless playback:" << enabled;
    
    m_gaplessPlayback = enabled;
    emit gaplessPlaybackChanged(enabled);
}

void PlaybackController::saveResumePosition(const QString& filePath)
{
    if (filePath.isEmpty() || !hasMedia()) {
        return;
    }
    
    qint64 currentPos = position();
    qint64 totalDuration = duration();
    
    // Only save if we're not at the beginning or very close to the end
    if (currentPos > 5000 && currentPos < (totalDuration - 10000)) {
        m_resumePositions[filePath] = currentPos;
        qCDebug(playbackController) << "Saved resume position for" << filePath << "at" << currentPos;
    } else {
        // Remove resume position if at beginning or end
        m_resumePositions.remove(filePath);
    }
}

bool PlaybackController::loadResumePosition(const QString& filePath)
{
    if (filePath.isEmpty() || !m_resumePositions.contains(filePath)) {
        return false;
    }
    
    qint64 resumePos = m_resumePositions[filePath];
    qCDebug(playbackController) << "Loading resume position for" << filePath << "at" << resumePos;
    
    seek(resumePos);
    return true;
}

void PlaybackController::clearResumePosition(const QString& filePath)
{
    if (m_resumePositions.remove(filePath)) {
        qCDebug(playbackController) << "Cleared resume position for" << filePath;
    }
}

QString PlaybackController::generateSeekThumbnail(qint64 position)
{
    if (!m_seekThumbnailEnabled || !hasMedia() || !m_mediaEngine) {
        return QString();
    }
    
    // Check cache first
    if (m_thumbnailCache.contains(position)) {
        return m_thumbnailCache[position];
    }
    
    // Only generate thumbnails for video content
    if (!m_mediaEngine->hasVideo()) {
        return QString();
    }
    
    // Try to get frame from media engine
    QString thumbnail = m_mediaEngine->getVideoFrame(position);
    
    if (thumbnail.isEmpty()) {
        // Fallback to placeholder if engine doesn't support frame extraction
        thumbnail = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==";
    }
    
    // Cache the thumbnail
    m_thumbnailCache[position] = thumbnail;
    
    // Limit cache size to prevent memory issues
    if (m_thumbnailCache.size() > 100) {
        // Remove oldest entries (simple approach - could be improved with LRU)
        auto it = m_thumbnailCache.begin();
        for (int i = 0; i < 20 && it != m_thumbnailCache.end(); ++i) {
            it = m_thumbnailCache.erase(it);
        }
    }
    
    qCDebug(playbackController) << "Generated thumbnail for position" << position;
    emit seekThumbnailGenerated(position, thumbnail);
    
    return thumbnail;
}

void PlaybackController::setSeekThumbnailEnabled(bool enabled)
{
    if (m_seekThumbnailEnabled == enabled) {
        return;
    }
    
    qCDebug(playbackController) << "Setting seek thumbnails:" << enabled;
    
    m_seekThumbnailEnabled = enabled;
    
    if (!enabled) {
        m_thumbnailCache.clear();
    }
}

void PlaybackController::onFastSeekTimer()
{
    if (!m_fastSeekActive || !m_mediaEngine || !hasMedia()) {
        stopFastSeek();
        return;
    }
    
    qint64 currentPos = position();
    qint64 seekAmount = (FAST_SEEK_INTERVAL_MS * m_fastSeekSpeed);
    
    if (!m_fastSeekForward) {
        seekAmount = -seekAmount;
    }
    
    qint64 newPos = currentPos + seekAmount;
    
    // Check bounds
    qint64 totalDuration = duration();
    if (newPos < 0) {
        newPos = 0;
        stopFastSeek();
    } else if (totalDuration > 0 && newPos >= totalDuration) {
        newPos = totalDuration - 1000; // Stop 1 second before end
        stopFastSeek();
    }
    
    seek(newPos);
}

void PlaybackController::onCrossfadeTimer()
{
    if (!m_crossfadeActive) {
        return;
    }
    
    qCDebug(playbackController) << "Crossfade completed";
    
    m_crossfadeActive = false;
    
    // In a real implementation, this would:
    // 1. Fade out current track
    // 2. Start next track with fade in
    // 3. Handle the transition smoothly
    
    // For now, we just signal that crossfade is done
    // The playlist manager would handle the actual track switching
}
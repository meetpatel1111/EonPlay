#include "media/VLCBackend.h"
#include "UserPreferences.h"
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QMutexLocker>
#include <QUrl>
#include <QThread>

// libVLC includes
#include <vlc/vlc.h>

// libVLC includes
#include <vlc/vlc.h>

Q_LOGGING_CATEGORY(vlcBackend, "mediaplayer.vlcbackend")

VLCBackend::VLCBackend(QObject* parent)
    : IMediaEngine(parent)
    , m_vlcInstance(nullptr)
    , m_mediaPlayer(nullptr)
    , m_currentMedia(nullptr)
    , m_eventManager(nullptr)
    , m_currentState(PlaybackState::Stopped)
    , m_currentVolume(100)
    , m_currentPosition(0)
    , m_currentDuration(0)
    , m_initialized(false)
    , m_positionTimer(new QTimer(this))
    , m_maxFileSize(10LL * 1024 * 1024 * 1024) // 10GB max file size
    , m_hardwareAcceleration(std::make_unique<HardwareAcceleration>(this))
{
    qCDebug(vlcBackend) << "VLCBackend created";
    
    // Initialize allowed file extensions for security
    m_allowedExtensions << "mp4" << "avi" << "mkv" << "mov" << "webm" << "flv" << "wmv"
                       << "mp3" << "flac" << "wav" << "aac" << "ogg" << "wma" << "m4a"
                       << "m3u" << "m3u8" << "pls" << "xspf";
    
    // Setup position update timer
    m_positionTimer->setInterval(100); // Update every 100ms
    connect(m_positionTimer, &QTimer::timeout, this, &VLCBackend::updatePosition);
}

VLCBackend::~VLCBackend()
{
    shutdown();
    qCDebug(vlcBackend) << "VLCBackend destroyed";
}

bool VLCBackend::initialize()
{
    if (m_initialized) {
        qCWarning(vlcBackend) << "VLCBackend already initialized";
        return true;
    }
    
    qCDebug(vlcBackend) << "Initializing VLCBackend";
    
    if (!isVLCAvailable()) {
        qCCritical(vlcBackend) << "libVLC is not available on this system";
        return false;
    }
    
    // Initialize hardware acceleration detection
    if (!m_hardwareAcceleration->initialize()) {
        qCWarning(vlcBackend) << "Hardware acceleration initialization failed, using software decoding";
    }
    
    if (!initializeVLC()) {
        qCCritical(vlcBackend) << "Failed to initialize libVLC";
        return false;
    }
    
    m_initialized = true;
    qCDebug(vlcBackend) << "VLCBackend initialized successfully";
    qCDebug(vlcBackend) << "libVLC version:" << vlcVersion();
    
    // Log hardware acceleration status
    if (m_hardwareAcceleration->isHardwareAccelerationEnabled()) {
        HardwareAccelerationType activeType = m_hardwareAcceleration->getPreferredAcceleration();
        qCDebug(vlcBackend) << "Hardware acceleration enabled:" 
                           << HardwareAcceleration::getAccelerationTypeName(activeType);
    } else {
        qCDebug(vlcBackend) << "Hardware acceleration disabled, using software decoding";
    }
    
    return true;
}

void VLCBackend::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qCDebug(vlcBackend) << "Shutting down VLCBackend";
    
    m_positionTimer->stop();
    cleanupVLC();
    
    m_initialized = false;
    qCDebug(vlcBackend) << "VLCBackend shut down";
}

bool VLCBackend::initializeVLC()
{
    // Create libVLC arguments for security and performance
    QStringList vlcArgs;
    
    // Security options - sandboxed decoding
    vlcArgs << "--no-video-title-show"           // Don't show video title
           << "--no-snapshot-preview"            // Disable snapshot preview
           << "--no-metadata-network-access"     // Disable network metadata access
           << "--no-lua"                         // Disable Lua scripting
           << "--no-stats"                       // Disable statistics
           << "--no-interact"                    // Disable interactive mode
           << "--intf=dummy"                     // Use dummy interface
           << "--extraintf="                     // No extra interfaces
           << "--no-sout-keep"                   // Don't keep stream output
           << "--no-disable-screensaver";        // Allow screensaver control
    
    // Performance options
    vlcArgs << "--file-caching=1000"             // 1 second file caching
           << "--network-caching=3000"           // 3 seconds network caching
           << "--clock-jitter=0"                 // Reduce clock jitter
           << "--clock-synchro=0";               // Disable clock synchronization
    
    // Add hardware acceleration arguments
    QStringList hwAccelArgs = m_hardwareAcceleration->getVLCArguments();
    vlcArgs.append(hwAccelArgs);
    
    // Convert QStringList to char* array
    QList<QByteArray> argBytes;
    QList<const char*> argPointers;
    
    for (const QString& arg : vlcArgs) {
        argBytes.append(arg.toUtf8());
        argPointers.append(argBytes.last().constData());
    }
    
    // Create libVLC instance
    m_vlcInstance = libvlc_new(argPointers.size(), argPointers.data());
    if (!m_vlcInstance) {
        qCCritical(vlcBackend) << "Failed to create libVLC instance";
        return false;
    }
    
    // Create media player
    m_mediaPlayer = libvlc_media_player_new(m_vlcInstance);
    if (!m_mediaPlayer) {
        qCCritical(vlcBackend) << "Failed to create libVLC media player";
        libvlc_release(m_vlcInstance);
        m_vlcInstance = nullptr;
        return false;
    }
    
    // Setup event callbacks
    setupEventCallbacks();
    
    // Configure additional sandboxing
    configureSandboxing();
    
    qCDebug(vlcBackend) << "libVLC initialized successfully";
    return true;
}

void VLCBackend::setupEventCallbacks()
{
    if (!m_mediaPlayer) {
        return;
    }
    
    m_eventManager = libvlc_media_player_event_manager(m_mediaPlayer);
    if (!m_eventManager) {
        qCWarning(vlcBackend) << "Failed to get event manager";
        return;
    }
    
    // Register for important events
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerPlaying, vlcEventCallback, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerPaused, vlcEventCallback, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerStopped, vlcEventCallback, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerEndReached, vlcEventCallback, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerEncounteredError, vlcEventCallback, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerBuffering, vlcEventCallback, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerLengthChanged, vlcEventCallback, this);
    
    qCDebug(vlcBackend) << "Event callbacks registered";
}

void VLCBackend::cleanupVLC()
{
    // Stop position timer
    if (m_positionTimer) {
        m_positionTimer->stop();
    }
    
    // Release current media
    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
    }
    
    // Release media player
    if (m_mediaPlayer) {
        libvlc_media_player_stop(m_mediaPlayer);
        libvlc_media_player_release(m_mediaPlayer);
        m_mediaPlayer = nullptr;
    }
    
    // Release libVLC instance
    if (m_vlcInstance) {
        libvlc_release(m_vlcInstance);
        m_vlcInstance = nullptr;
    }
    
    m_eventManager = nullptr;
    
    QMutexLocker locker(&m_stateMutex);
    m_currentState = PlaybackState::Stopped;
    m_currentPosition = 0;
    m_currentDuration = 0;
}

bool VLCBackend::loadMedia(const QString& path)
{
    if (!m_initialized || !m_mediaPlayer) {
        qCWarning(vlcBackend) << "VLCBackend not initialized";
        return false;
    }
    
    if (path.isEmpty()) {
        qCWarning(vlcBackend) << "Empty media path provided";
        return false;
    }
    
    qCDebug(vlcBackend) << "Loading media:" << path;
    
    // Validate media file for security
    if (!validateMediaFile(path)) {
        qCWarning(vlcBackend) << "Media file validation failed:" << path;
        emit errorOccurred("Invalid or unsafe media file");
        return false;
    }
    
    // Release previous media
    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
    }
    
    // Create new media
    QUrl url(path);
    if (url.isLocalFile() || !url.scheme().isEmpty()) {
        // Handle URLs (including file:// URLs)
        m_currentMedia = libvlc_media_new_location(m_vlcInstance, path.toUtf8().constData());
    } else {
        // Handle local file paths
        m_currentMedia = libvlc_media_new_path(m_vlcInstance, path.toUtf8().constData());
    }
    
    if (!m_currentMedia) {
        qCCritical(vlcBackend) << "Failed to create libVLC media for:" << path;
        emit errorOccurred("Failed to load media file");
        return false;
    }
    
    // Set media to player
    libvlc_media_player_set_media(m_mediaPlayer, m_currentMedia);
    
    qCDebug(vlcBackend) << "Media loaded successfully:" << path;
    emit mediaLoaded(true);
    
    return true;
}

void VLCBackend::play()
{
    if (!m_initialized || !m_mediaPlayer) {
        qCWarning(vlcBackend) << "Cannot play: VLCBackend not initialized";
        return;
    }
    
    if (!m_currentMedia) {
        qCWarning(vlcBackend) << "Cannot play: No media loaded";
        return;
    }
    
    qCDebug(vlcBackend) << "Starting playback";
    
    int result = libvlc_media_player_play(m_mediaPlayer);
    if (result == -1) {
        qCCritical(vlcBackend) << "Failed to start playback";
        emit errorOccurred("Failed to start playback");
        return;
    }
    
    // Start position updates
    m_positionTimer->start();
}

void VLCBackend::pause()
{
    if (!m_initialized || !m_mediaPlayer) {
        qCWarning(vlcBackend) << "Cannot pause: VLCBackend not initialized";
        return;
    }
    
    qCDebug(vlcBackend) << "Pausing playback";
    libvlc_media_player_pause(m_mediaPlayer);
}

void VLCBackend::stop()
{
    if (!m_initialized || !m_mediaPlayer) {
        qCWarning(vlcBackend) << "Cannot stop: VLCBackend not initialized";
        return;
    }
    
    qCDebug(vlcBackend) << "Stopping playback";
    
    // Stop position updates
    m_positionTimer->stop();
    
    libvlc_media_player_stop(m_mediaPlayer);
    
    QMutexLocker locker(&m_stateMutex);
    m_currentPosition = 0;
}

void VLCBackend::seek(qint64 position)
{
    if (!m_initialized || !m_mediaPlayer) {
        qCWarning(vlcBackend) << "Cannot seek: VLCBackend not initialized";
        return;
    }
    
    if (!m_currentMedia) {
        qCWarning(vlcBackend) << "Cannot seek: No media loaded";
        return;
    }
    
    qCDebug(vlcBackend) << "Seeking to position:" << position;
    
    // Convert milliseconds to libVLC time (microseconds)
    libvlc_time_t vlcTime = position * 1000;
    libvlc_media_player_set_time(m_mediaPlayer, vlcTime);
}

void VLCBackend::setVolume(int volume)
{
    if (!m_initialized || !m_mediaPlayer) {
        qCWarning(vlcBackend) << "Cannot set volume: VLCBackend not initialized";
        return;
    }
    
    // Clamp volume to valid range
    volume = qBound(0, volume, 100);
    
    qCDebug(vlcBackend) << "Setting volume to:" << volume;
    
    int result = libvlc_audio_set_volume(m_mediaPlayer, volume);
    if (result == -1) {
        qCWarning(vlcBackend) << "Failed to set volume";
        return;
    }
    
    QMutexLocker locker(&m_stateMutex);
    m_currentVolume = volume;
    
    emit volumeChanged(volume);
}

qint64 VLCBackend::position() const
{
    if (!m_initialized || !m_mediaPlayer) {
        return 0;
    }
    
    // Get time from libVLC (in microseconds) and convert to milliseconds
    libvlc_time_t vlcTime = libvlc_media_player_get_time(m_mediaPlayer);
    return vlcTime / 1000;
}

qint64 VLCBackend::duration() const
{
    if (!m_initialized || !m_mediaPlayer) {
        return 0;
    }
    
    // Get length from libVLC (in microseconds) and convert to milliseconds
    libvlc_time_t vlcLength = libvlc_media_player_get_length(m_mediaPlayer);
    return vlcLength / 1000;
}

void VLCBackend::updatePosition()
{
    if (!m_initialized || !m_mediaPlayer) {
        return;
    }
    
    qint64 newPosition = position();
    qint64 newDuration = duration();
    
    bool positionChanged = false;
    bool durationChanged = false;
    
    {
        QMutexLocker locker(&m_stateMutex);
        
        if (newPosition != m_currentPosition) {
            m_currentPosition = newPosition;
            emit positionChanged(newPosition);
        }
        
        if (newDuration != m_currentDuration && newDuration > 0) {
            m_currentDuration = newDuration;
            emit durationChanged(newDuration);
        }
    }
}

void VLCBackend::handleVLCEvent(const libvlc_event_t* event)
{
    if (!event) {
        return;
    }
    
    PlaybackState newState = m_currentState;
    
    switch (event->type) {
        case libvlc_MediaPlayerPlaying:
            qCDebug(vlcBackend) << "VLC Event: Playing";
            newState = PlaybackState::Playing;
            break;
            
        case libvlc_MediaPlayerPaused:
            qCDebug(vlcBackend) << "VLC Event: Paused";
            newState = PlaybackState::Paused;
            break;
            
        case libvlc_MediaPlayerStopped:
        case libvlc_MediaPlayerEndReached:
            qCDebug(vlcBackend) << "VLC Event: Stopped/End Reached";
            newState = PlaybackState::Stopped;
            m_positionTimer->stop();
            break;
            
        case libvlc_MediaPlayerBuffering:
            qCDebug(vlcBackend) << "VLC Event: Buffering" << event->u.media_player_buffering.new_cache << "%";
            newState = PlaybackState::Buffering;
            break;
            
        case libvlc_MediaPlayerEncounteredError:
            qCCritical(vlcBackend) << "VLC Event: Error encountered";
            newState = PlaybackState::Error;
            m_positionTimer->stop();
            emit errorOccurred("Playback error occurred");
            break;
            
        case libvlc_MediaPlayerLengthChanged:
            qCDebug(vlcBackend) << "VLC Event: Length changed";
            // Duration will be updated in updatePosition()
            break;
            
        default:
            // Ignore other events
            break;
    }
    
    // Update state if changed
    if (newState != m_currentState) {
        QMutexLocker locker(&m_stateMutex);
        m_currentState = newState;
        locker.unlock();
        
        emit stateChanged(newState);
    }
}

PlaybackState VLCBackend::convertVLCState(int vlcState) const
{
    switch (vlcState) {
        case libvlc_Playing:
            return PlaybackState::Playing;
        case libvlc_Paused:
            return PlaybackState::Paused;
        case libvlc_Stopped:
        case libvlc_Ended:
            return PlaybackState::Stopped;
        case libvlc_Buffering:
            return PlaybackState::Buffering;
        case libvlc_Error:
            return PlaybackState::Error;
        default:
            return PlaybackState::Stopped;
    }
}

bool VLCBackend::validateMediaFile(const QString& path) const
{
    // Check if path is empty
    if (path.isEmpty()) {
        return false;
    }
    
    // Handle URLs
    QUrl url(path);
    if (url.isValid() && !url.isLocalFile()) {
        // For URLs, check scheme
        QString scheme = url.scheme().toLower();
        return scheme == "http" || scheme == "https" || scheme == "ftp" || 
               scheme == "rtsp" || scheme == "rtmp" || scheme == "mms";
    }
    
    // Handle local files
    QString localPath = url.isLocalFile() ? url.toLocalFile() : path;
    QFileInfo fileInfo(localPath);
    
    // Check if file exists
    if (!fileInfo.exists()) {
        qCWarning(vlcBackend) << "File does not exist:" << localPath;
        return false;
    }
    
    // Check file size
    if (fileInfo.size() > m_maxFileSize) {
        qCWarning(vlcBackend) << "File too large:" << fileInfo.size() << "bytes";
        return false;
    }
    
    // Check file extension
    QString extension = fileInfo.suffix().toLower();
    if (!m_allowedExtensions.contains(extension)) {
        qCWarning(vlcBackend) << "File extension not allowed:" << extension;
        return false;
    }
    
    // Additional security checks could be added here
    // - File header validation
    // - Virus scanning integration
    // - Path traversal prevention
    
    return true;
}

void VLCBackend::configureSandboxing()
{
    if (!m_mediaPlayer) {
        return;
    }
    
    // Additional sandboxing configuration
    // These settings help isolate the decoding process
    
    qCDebug(vlcBackend) << "Configuring sandboxed decoding";
    
    // Note: More advanced sandboxing would require platform-specific
    // process isolation, which could be implemented in future versions
}

void VLCBackend::vlcEventCallback(const libvlc_event_t* event, void* userData)
{
    VLCBackend* backend = static_cast<VLCBackend*>(userData);
    if (backend) {
        backend->handleVLCEvent(event);
    }
}

QString VLCBackend::vlcVersion()
{
    return QString::fromUtf8(libvlc_get_version());
}

bool VLCBackend::isVLCAvailable()
{
    // Try to get version to check if libVLC is available
    try {
        const char* version = libvlc_get_version();
        return version != nullptr;
    } catch (...) {
        return false;
    }
}

void VLCBackend::setHardwareAccelerationEnabled(bool enabled)
{
    if (!m_hardwareAcceleration) {
        return;
    }
    
    bool wasEnabled = m_hardwareAcceleration->isHardwareAccelerationEnabled();
    m_hardwareAcceleration->setHardwareAccelerationEnabled(enabled);
    
    if (wasEnabled != enabled) {
        qCDebug(vlcBackend) << "Hardware acceleration" << (enabled ? "enabled" : "disabled");
        
        // If we have an active VLC instance, we need to reinitialize it
        // to apply the new hardware acceleration settings
        if (m_initialized && m_vlcInstance) {
            qCDebug(vlcBackend) << "Reinitializing VLC with new hardware acceleration settings";
            
            // Save current state
            bool wasPlaying = (m_currentState == PlaybackState::Playing);
            qint64 currentPos = position();
            QString currentMediaPath;
            
            // Get current media path if available
            if (m_currentMedia) {
                char* mrl = libvlc_media_get_mrl(m_currentMedia);
                if (mrl) {
                    currentMediaPath = QString::fromUtf8(mrl);
                    libvlc_free(mrl);
                }
            }
            
            // Clean up current VLC instance
            cleanupVLC();
            
            // Reinitialize with new settings
            if (initializeVLC()) {
                // Restore media if we had one
                if (!currentMediaPath.isEmpty()) {
                    loadMedia(currentMediaPath);
                    if (currentPos > 0) {
                        seek(currentPos);
                    }
                    if (wasPlaying) {
                        play();
                    }
                }
            } else {
                qCCritical(vlcBackend) << "Failed to reinitialize VLC with new hardware acceleration settings";
            }
        }
    }
}

void VLCBackend::setPreferredAccelerationType(HardwareAccelerationType type)
{
    if (!m_hardwareAcceleration) {
        return;
    }
    
    HardwareAccelerationType currentType = m_hardwareAcceleration->getPreferredAcceleration();
    
    if (m_hardwareAcceleration->setPreferredAcceleration(type)) {
        qCDebug(vlcBackend) << "Preferred acceleration type changed from" 
                           << HardwareAcceleration::getAccelerationTypeName(currentType)
                           << "to" << HardwareAcceleration::getAccelerationTypeName(type);
        
        // Reinitialize VLC if the type actually changed and we're initialized
        if (currentType != type && m_initialized && m_vlcInstance) {
            setHardwareAccelerationEnabled(m_hardwareAcceleration->isHardwareAccelerationEnabled());
        }
    }
}

void VLCBackend::updateHardwareAccelerationFromPreferences(const UserPreferences& preferences)
{
    if (!m_hardwareAcceleration) {
        return;
    }
    
    // Update hardware acceleration enabled state
    this->setHardwareAccelerationEnabled(preferences.hardwareAcceleration);
    
    // Update preferred acceleration type
    HardwareAccelerationType type = HardwareAcceleration::stringToAccelerationType(preferences.preferredAccelerationType);
    this->setPreferredAccelerationType(type);
    
    qCDebug(vlcBackend) << "Hardware acceleration settings updated from preferences";
    qCDebug(vlcBackend) << "Enabled:" << preferences.hardwareAcceleration;
    qCDebug(vlcBackend) << "Preferred type:" << preferences.preferredAccelerationType;
}
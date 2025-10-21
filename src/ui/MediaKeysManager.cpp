#include "ui/MediaKeysManager.h"
#include "media/IMediaEngine.h"
#include <QApplication>
#include <QWidget>
#include <QLoggingCategory>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>
#endif

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#endif

Q_LOGGING_CATEGORY(mediaKeys, "eonplay.mediakeys")

#ifdef Q_OS_LINUX
/**
 * @brief MPRIS D-Bus interface implementation for Linux media keys
 */
class MediaKeysManager::MPRISInterface : public QObject
{
    Q_OBJECT

public:
    explicit MPRISInterface(MediaKeysManager* parent);
    ~MPRISInterface();
    
    bool initialize();
    void shutdown();
    void updateMediaInfo(const QString& title, const QString& artist, const QString& album, qint64 duration);
    void updatePlaybackState(bool isPlaying, qint64 position);

private slots:
    void onPlayPause();
    void onStop();
    void onNext();
    void onPrevious();

private:
    MediaKeysManager* m_manager;
    QDBusInterface* m_mediaPlayer2Interface;
    QDBusInterface* m_playerInterface;
    bool m_registered;
};
#endif

MediaKeysManager::MediaKeysManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_enabled(false)
    , m_currentDuration(0)
    , m_currentPosition(0)
    , m_isPlaying(false)
#ifdef Q_OS_WIN
    , m_windowHandle(nullptr)
    , m_hotkeysRegistered(false)
#endif
#ifdef Q_OS_LINUX
    , m_mprisInterface(nullptr)
#endif
{
    // Create debounce timer
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DEBOUNCE_INTERVAL_MS);
    
    qCDebug(mediaKeys) << "MediaKeysManager created";
}

MediaKeysManager::~MediaKeysManager()
{
    shutdown();
    qCDebug(mediaKeys) << "MediaKeysManager destroyed";
}

bool MediaKeysManager::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    bool success = false;
    
#ifdef Q_OS_WIN
    success = initializeWindows();
#elif defined(Q_OS_LINUX)
    success = initializeLinux();
#else
    qCWarning(mediaKeys) << "Media keys not supported on this platform";
#endif
    
    if (success) {
        m_initialized = true;
        m_enabled = true;
        
        // Install native event filter
        QApplication::instance()->installNativeEventFilter(this);
        
        qCDebug(mediaKeys) << "MediaKeysManager initialized successfully";
    } else {
        qCWarning(mediaKeys) << "Failed to initialize MediaKeysManager";
    }
    
    return success;
}

void MediaKeysManager::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    // Remove native event filter
    QApplication::instance()->removeNativeEventFilter(this);
    
#ifdef Q_OS_WIN
    shutdownWindows();
#elif defined(Q_OS_LINUX)
    shutdownLinux();
#endif
    
    m_initialized = false;
    m_enabled = false;
    
    qCDebug(mediaKeys) << "MediaKeysManager shutdown";
}

void MediaKeysManager::setMediaEngine(std::shared_ptr<IMediaEngine> mediaEngine)
{
    // Disconnect from previous media engine
    if (m_mediaEngine) {
        disconnect(m_mediaEngine.get(), nullptr, this, nullptr);
    }
    
    m_mediaEngine = mediaEngine;
    
    // Connect to new media engine
    if (m_mediaEngine) {
        // TODO: Connect to media engine signals when available
        // connect(m_mediaEngine.get(), &IMediaEngine::stateChanged, 
        //         this, &MediaKeysManager::onPlaybackStateChanged);
        // connect(m_mediaEngine.get(), &IMediaEngine::positionChanged,
        //         this, &MediaKeysManager::onPositionChanged);
        
        qCDebug(mediaKeys) << "Media engine connected";
    }
}

void MediaKeysManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        qCDebug(mediaKeys) << "Media keys" << (enabled ? "enabled" : "disabled");
    }
}

bool MediaKeysManager::isSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

void MediaKeysManager::updateMediaInfo(const QString& title, const QString& artist, 
                                      const QString& album, qint64 duration)
{
    m_currentTitle = title;
    m_currentArtist = artist;
    m_currentAlbum = album;
    m_currentDuration = duration;
    
#ifdef Q_OS_LINUX
    if (m_mprisInterface) {
        m_mprisInterface->updateMediaInfo(title, artist, album, duration);
    }
#endif
    
    qCDebug(mediaKeys) << "Media info updated:" << title << "by" << artist;
}

void MediaKeysManager::updatePlaybackState(bool isPlaying, qint64 position)
{
    m_isPlaying = isPlaying;
    m_currentPosition = position;
    
#ifdef Q_OS_LINUX
    if (m_mprisInterface) {
        m_mprisInterface->updatePlaybackState(isPlaying, position);
    }
#endif
    
    qCDebug(mediaKeys) << "Playback state updated:" << (isPlaying ? "playing" : "paused");
}

void MediaKeysManager::onMediaLoaded(const QString& filePath, const QString& title)
{
    QString mediaTitle = title.isEmpty() ? QFileInfo(filePath).baseName() : title;
    updateMediaInfo(mediaTitle);
    
    qCDebug(mediaKeys) << "Media loaded:" << mediaTitle;
}

void MediaKeysManager::onPlaybackStateChanged(bool isPlaying)
{
    updatePlaybackState(isPlaying, m_currentPosition);
}

void MediaKeysManager::onPositionChanged(qint64 position)
{
    updatePlaybackState(m_isPlaying, position);
}

bool MediaKeysManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result)
    
    if (!m_enabled) {
        return false;
    }
    
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        return handleWindowsMediaKey(msg);
    }
#endif
    
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    return false;
}

void MediaKeysManager::onPlayPause()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "play_pause") {
        return;
    }
    
    m_lastKeyPressed = "play_pause";
    m_debounceTimer->start();
    
    emit playPausePressed();
    qCDebug(mediaKeys) << "Play/Pause key pressed";
}

void MediaKeysManager::onStop()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "stop") {
        return;
    }
    
    m_lastKeyPressed = "stop";
    m_debounceTimer->start();
    
    emit stopPressed();
    qCDebug(mediaKeys) << "Stop key pressed";
}

void MediaKeysManager::onNext()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "next") {
        return;
    }
    
    m_lastKeyPressed = "next";
    m_debounceTimer->start();
    
    emit nextPressed();
    qCDebug(mediaKeys) << "Next key pressed";
}

void MediaKeysManager::onPrevious()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "previous") {
        return;
    }
    
    m_lastKeyPressed = "previous";
    m_debounceTimer->start();
    
    emit previousPressed();
    qCDebug(mediaKeys) << "Previous key pressed";
}

void MediaKeysManager::onVolumeUp()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "volume_up") {
        return;
    }
    
    m_lastKeyPressed = "volume_up";
    m_debounceTimer->start();
    
    emit volumeUpPressed();
    qCDebug(mediaKeys) << "Volume Up key pressed";
}

void MediaKeysManager::onVolumeDown()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "volume_down") {
        return;
    }
    
    m_lastKeyPressed = "volume_down";
    m_debounceTimer->start();
    
    emit volumeDownPressed();
    qCDebug(mediaKeys) << "Volume Down key pressed";
}

void MediaKeysManager::onMute()
{
    if (m_debounceTimer->isActive() && m_lastKeyPressed == "mute") {
        return;
    }
    
    m_lastKeyPressed = "mute";
    m_debounceTimer->start();
    
    emit mutePressed();
    qCDebug(mediaKeys) << "Mute key pressed";
}

#ifdef Q_OS_WIN
bool MediaKeysManager::initializeWindows()
{
    // Get main window handle
    m_windowHandle = reinterpret_cast<HWND>(QApplication::activeWindow()->winId());
    
    if (!m_windowHandle) {
        qCWarning(mediaKeys) << "Failed to get window handle";
        return false;
    }
    
    // Register global hotkeys for media keys
    if (!registerWindowsHotkeys()) {
        qCWarning(mediaKeys) << "Failed to register Windows hotkeys";
        return false;
    }
    
    qCDebug(mediaKeys) << "Windows media keys initialized";
    return true;
}

void MediaKeysManager::shutdownWindows()
{
    unregisterWindowsHotkeys();
    m_windowHandle = nullptr;
    
    qCDebug(mediaKeys) << "Windows media keys shutdown";
}

bool MediaKeysManager::registerWindowsHotkeys()
{
    if (m_hotkeysRegistered) {
        return true;
    }
    
    bool success = true;
    
    // Register media key hotkeys
    success &= RegisterHotKey(m_windowHandle, HOTKEY_PLAY_PAUSE, 0, VK_MEDIA_PLAY_PAUSE);
    success &= RegisterHotKey(m_windowHandle, HOTKEY_STOP, 0, VK_MEDIA_STOP);
    success &= RegisterHotKey(m_windowHandle, HOTKEY_NEXT, 0, VK_MEDIA_NEXT_TRACK);
    success &= RegisterHotKey(m_windowHandle, HOTKEY_PREVIOUS, 0, VK_MEDIA_PREV_TRACK);
    success &= RegisterHotKey(m_windowHandle, HOTKEY_VOLUME_UP, 0, VK_VOLUME_UP);
    success &= RegisterHotKey(m_windowHandle, HOTKEY_VOLUME_DOWN, 0, VK_VOLUME_DOWN);
    success &= RegisterHotKey(m_windowHandle, HOTKEY_MUTE, 0, VK_VOLUME_MUTE);
    
    if (success) {
        m_hotkeysRegistered = true;
        qCDebug(mediaKeys) << "Windows hotkeys registered successfully";
    } else {
        qCWarning(mediaKeys) << "Failed to register some Windows hotkeys";
        unregisterWindowsHotkeys(); // Clean up partial registration
    }
    
    return success;
}

void MediaKeysManager::unregisterWindowsHotkeys()
{
    if (!m_hotkeysRegistered || !m_windowHandle) {
        return;
    }
    
    UnregisterHotKey(m_windowHandle, HOTKEY_PLAY_PAUSE);
    UnregisterHotKey(m_windowHandle, HOTKEY_STOP);
    UnregisterHotKey(m_windowHandle, HOTKEY_NEXT);
    UnregisterHotKey(m_windowHandle, HOTKEY_PREVIOUS);
    UnregisterHotKey(m_windowHandle, HOTKEY_VOLUME_UP);
    UnregisterHotKey(m_windowHandle, HOTKEY_VOLUME_DOWN);
    UnregisterHotKey(m_windowHandle, HOTKEY_MUTE);
    
    m_hotkeysRegistered = false;
    qCDebug(mediaKeys) << "Windows hotkeys unregistered";
}

bool MediaKeysManager::handleWindowsMediaKey(MSG* msg)
{
    if (msg->message != WM_HOTKEY) {
        return false;
    }
    
    int hotkeyId = static_cast<int>(msg->wParam);
    
    switch (hotkeyId) {
    case HOTKEY_PLAY_PAUSE:
        onPlayPause();
        return true;
    case HOTKEY_STOP:
        onStop();
        return true;
    case HOTKEY_NEXT:
        onNext();
        return true;
    case HOTKEY_PREVIOUS:
        onPrevious();
        return true;
    case HOTKEY_VOLUME_UP:
        onVolumeUp();
        return true;
    case HOTKEY_VOLUME_DOWN:
        onVolumeDown();
        return true;
    case HOTKEY_MUTE:
        onMute();
        return true;
    default:
        return false;
    }
}
#endif

#ifdef Q_OS_LINUX
bool MediaKeysManager::initializeLinux()
{
    m_mprisInterface = std::make_unique<MPRISInterface>(this);
    
    if (!m_mprisInterface->initialize()) {
        qCWarning(mediaKeys) << "Failed to initialize MPRIS interface";
        m_mprisInterface.reset();
        return false;
    }
    
    qCDebug(mediaKeys) << "Linux MPRIS media keys initialized";
    return true;
}

void MediaKeysManager::shutdownLinux()
{
    if (m_mprisInterface) {
        m_mprisInterface->shutdown();
        m_mprisInterface.reset();
    }
    
    qCDebug(mediaKeys) << "Linux MPRIS media keys shutdown";
}

// MPRIS Interface Implementation
MediaKeysManager::MPRISInterface::MPRISInterface(MediaKeysManager* parent)
    : QObject(parent)
    , m_manager(parent)
    , m_mediaPlayer2Interface(nullptr)
    , m_playerInterface(nullptr)
    , m_registered(false)
{
}

MediaKeysManager::MPRISInterface::~MPRISInterface()
{
    shutdown();
}

bool MediaKeysManager::MPRISInterface::initialize()
{
    // Register MPRIS service
    QDBusConnection bus = QDBusConnection::sessionBus();
    
    if (!bus.registerService("org.mpris.MediaPlayer2.eonplay")) {
        qCWarning(mediaKeys) << "Failed to register MPRIS service";
        return false;
    }
    
    // Create D-Bus interfaces
    m_mediaPlayer2Interface = new QDBusInterface(
        "org.mpris.MediaPlayer2.eonplay",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2",
        bus, this);
    
    m_playerInterface = new QDBusInterface(
        "org.mpris.MediaPlayer2.eonplay", 
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        bus, this);
    
    // Connect D-Bus signals to slots
    bus.connect("", "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player",
                "PlayPause", this, SLOT(onPlayPause()));
    bus.connect("", "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", 
                "Stop", this, SLOT(onStop()));
    bus.connect("", "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player",
                "Next", this, SLOT(onNext()));
    bus.connect("", "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player",
                "Previous", this, SLOT(onPrevious()));
    
    m_registered = true;
    return true;
}

void MediaKeysManager::MPRISInterface::shutdown()
{
    if (!m_registered) {
        return;
    }
    
    QDBusConnection::sessionBus().unregisterService("org.mpris.MediaPlayer2.eonplay");
    
    delete m_mediaPlayer2Interface;
    delete m_playerInterface;
    m_mediaPlayer2Interface = nullptr;
    m_playerInterface = nullptr;
    
    m_registered = false;
}

void MediaKeysManager::MPRISInterface::updateMediaInfo(const QString& title, const QString& artist, 
                                                      const QString& album, qint64 duration)
{
    Q_UNUSED(title)
    Q_UNUSED(artist)
    Q_UNUSED(album)
    Q_UNUSED(duration)
    
    // TODO: Update MPRIS metadata properties
    // This would involve setting D-Bus properties for the current track
}

void MediaKeysManager::MPRISInterface::updatePlaybackState(bool isPlaying, qint64 position)
{
    Q_UNUSED(isPlaying)
    Q_UNUSED(position)
    
    // TODO: Update MPRIS playback state properties
    // This would involve setting PlaybackStatus and Position properties
}

void MediaKeysManager::MPRISInterface::onPlayPause()
{
    m_manager->onPlayPause();
}

void MediaKeysManager::MPRISInterface::onStop()
{
    m_manager->onStop();
}

void MediaKeysManager::MPRISInterface::onNext()
{
    m_manager->onNext();
}

void MediaKeysManager::MPRISInterface::onPrevious()
{
    m_manager->onPrevious();
}
#endif

#include "MediaKeysManager.moc"
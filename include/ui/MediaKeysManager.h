#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class IMediaEngine;

/**
 * @brief System media keys integration manager
 * 
 * Handles global media key events (Play/Pause, Stop, Next, Previous, Volume)
 * across different platforms (Windows Media Foundation, Linux MPRIS).
 */
class MediaKeysManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit MediaKeysManager(QObject* parent = nullptr);
    ~MediaKeysManager() override;
    
    /**
     * @brief Initialize media keys support
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown media keys support
     */
    void shutdown();
    
    /**
     * @brief Set media engine for playback control
     * @param mediaEngine Shared pointer to media engine
     */
    void setMediaEngine(std::shared_ptr<IMediaEngine> mediaEngine);
    
    /**
     * @brief Enable or disable media keys handling
     * @param enabled true to enable media keys
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if media keys are enabled
     * @return true if media keys are enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Check if media keys are supported on this platform
     * @return true if media keys are supported
     */
    static bool isSupported();
    
    /**
     * @brief Update media information for system integration
     * @param title Media title
     * @param artist Media artist
     * @param album Media album
     * @param duration Media duration in milliseconds
     */
    void updateMediaInfo(const QString& title, const QString& artist = QString(), 
                        const QString& album = QString(), qint64 duration = 0);
    
    /**
     * @brief Update playback state for system integration
     * @param isPlaying Whether media is currently playing
     * @param position Current playback position in milliseconds
     */
    void updatePlaybackState(bool isPlaying, qint64 position = 0);

public slots:
    /**
     * @brief Handle media loaded event
     * @param filePath Path to loaded media
     * @param title Media title
     */
    void onMediaLoaded(const QString& filePath, const QString& title = QString());
    
    /**
     * @brief Handle playback state changes
     * @param isPlaying Whether playback started or stopped
     */
    void onPlaybackStateChanged(bool isPlaying);
    
    /**
     * @brief Handle position changes
     * @param position Current position in milliseconds
     */
    void onPositionChanged(qint64 position);

signals:
    /**
     * @brief Emitted when media key is pressed
     */
    void playPausePressed();
    void stopPressed();
    void nextPressed();
    void previousPressed();
    void volumeUpPressed();
    void volumeDownPressed();
    void mutePressed();

protected:
    /**
     * @brief Native event filter for media key events
     * @param eventType Event type
     * @param message Native message
     * @param result Result pointer
     * @return true if event was handled
     */
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    /**
     * @brief Handle media key actions
     */
    void onPlayPause();
    void onStop();
    void onNext();
    void onPrevious();
    void onVolumeUp();
    void onVolumeDown();
    void onMute();

private:
    /**
     * @brief Initialize Windows media keys support
     * @return true if successful
     */
    bool initializeWindows();
    
    /**
     * @brief Initialize Linux MPRIS support
     * @return true if successful
     */
    bool initializeLinux();
    
    /**
     * @brief Shutdown Windows media keys support
     */
    void shutdownWindows();
    
    /**
     * @brief Shutdown Linux MPRIS support
     */
    void shutdownLinux();
    
    /**
     * @brief Register Windows media key hotkeys
     * @return true if successful
     */
    bool registerWindowsHotkeys();
    
    /**
     * @brief Unregister Windows media key hotkeys
     */
    void unregisterWindowsHotkeys();
    
#ifdef Q_OS_WIN
    /**
     * @brief Handle Windows media key messages
     * @param msg Windows message
     * @return true if handled
     */
    bool handleWindowsMediaKey(MSG* msg);
#endif
    
    std::shared_ptr<IMediaEngine> m_mediaEngine;
    
    bool m_initialized;
    bool m_enabled;
    
    // Current media state
    QString m_currentTitle;
    QString m_currentArtist;
    QString m_currentAlbum;
    qint64 m_currentDuration;
    qint64 m_currentPosition;
    bool m_isPlaying;
    
    // Platform-specific data
#ifdef Q_OS_WIN
    HWND m_windowHandle;
    bool m_hotkeysRegistered;
    
    // Hotkey IDs
    static constexpr int HOTKEY_PLAY_PAUSE = 1;
    static constexpr int HOTKEY_STOP = 2;
    static constexpr int HOTKEY_NEXT = 3;
    static constexpr int HOTKEY_PREVIOUS = 4;
    static constexpr int HOTKEY_VOLUME_UP = 5;
    static constexpr int HOTKEY_VOLUME_DOWN = 6;
    static constexpr int HOTKEY_MUTE = 7;
#endif
    
#ifdef Q_OS_LINUX
    // D-Bus interface for MPRIS
    class MPRISInterface;
    std::unique_ptr<MPRISInterface> m_mprisInterface;
#endif
    
    // Debounce timer for rapid key presses
    QTimer* m_debounceTimer;
    QString m_lastKeyPressed;
    
    static constexpr int DEBOUNCE_INTERVAL_MS = 200;
};
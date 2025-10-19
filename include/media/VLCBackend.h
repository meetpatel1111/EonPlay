#pragma once

#include "IMediaEngine.h"
#include "IComponent.h"
#include "HardwareAcceleration.h"
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <memory>

// Forward declarations for libVLC types
struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_media_t;
struct libvlc_event_manager_t;

/**
 * @brief libVLC-based media engine implementation
 * 
 * Provides media playback functionality using the libVLC library.
 * Handles initialization, cleanup, and sandboxed decoding for security.
 */
class VLCBackend : public IMediaEngine, public IComponent
{
    Q_OBJECT

public:
    explicit VLCBackend(QObject* parent = nullptr);
    ~VLCBackend() override;
    
    // IComponent interface
    bool initialize() override;
    void shutdown() override;
    QString componentName() const override { return "VLCBackend"; }
    bool isInitialized() const override { return m_initialized; }
    
    // IMediaEngine interface
    bool loadMedia(const QString& path) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 position) override;
    void setVolume(int volume) override;
    
    PlaybackState state() const override { return m_currentState; }
    qint64 position() const override;
    qint64 duration() const override;
    int volume() const override { return m_currentVolume; }
    bool hasMedia() const override { return m_currentMedia != nullptr; }
    
    /**
     * @brief Get libVLC version information
     * @return Version string
     */
    static QString vlcVersion();
    
    /**
     * @brief Check if libVLC is available
     * @return true if libVLC can be loaded
     */
    static bool isVLCAvailable();
    
    /**
     * @brief Get hardware acceleration manager
     * @return Pointer to hardware acceleration manager
     */
    HardwareAcceleration* hardwareAcceleration() const { return m_hardwareAcceleration.get(); }
    
    /**
     * @brief Update hardware acceleration settings
     * @param enabled Whether hardware acceleration should be enabled
     */
    void setHardwareAccelerationEnabled(bool enabled);
    
    /**
     * @brief Set preferred hardware acceleration type
     * @param type Preferred acceleration type
     */
    void setPreferredAccelerationType(HardwareAccelerationType type);
    
    /**
     * @brief Update hardware acceleration from user preferences
     * @param preferences User preferences containing hardware settings
     */
    void updateHardwareAccelerationFromPreferences(const UserPreferences& preferences);

private slots:
    /**
     * @brief Update position and duration periodically
     */
    void updatePosition();

private:
    /**
     * @brief Initialize libVLC instance with security options
     * @return true if initialization successful
     */
    bool initializeVLC();
    
    /**
     * @brief Setup libVLC event callbacks
     */
    void setupEventCallbacks();
    
    /**
     * @brief Clean up libVLC resources
     */
    void cleanupVLC();
    
    /**
     * @brief Handle libVLC events
     * @param event libVLC event
     */
    void handleVLCEvent(const struct libvlc_event_t* event);
    
    /**
     * @brief Convert libVLC state to PlaybackState
     * @param vlcState libVLC state
     * @return Corresponding PlaybackState
     */
    PlaybackState convertVLCState(int vlcState) const;
    
    /**
     * @brief Validate media file for security
     * @param path File path to validate
     * @return true if file is safe to load
     */
    bool validateMediaFile(const QString& path) const;
    
    /**
     * @brief Set up sandboxed decoding options
     */
    void configureSandboxing();
    
    // Static callback for libVLC events
    static void vlcEventCallback(const struct libvlc_event_t* event, void* userData);
    
    // libVLC objects
    libvlc_instance_t* m_vlcInstance;
    libvlc_media_player_t* m_mediaPlayer;
    libvlc_media_t* m_currentMedia;
    libvlc_event_manager_t* m_eventManager;
    
    // State tracking
    PlaybackState m_currentState;
    int m_currentVolume;
    qint64 m_currentPosition;
    qint64 m_currentDuration;
    bool m_initialized;
    
    // Thread safety
    mutable QMutex m_stateMutex;
    
    // Position update timer
    QTimer* m_positionTimer;
    
    // Security and validation
    QStringList m_allowedExtensions;
    qint64 m_maxFileSize;
    
    // Hardware acceleration
    std::unique_ptr<HardwareAcceleration> m_hardwareAcceleration;
};
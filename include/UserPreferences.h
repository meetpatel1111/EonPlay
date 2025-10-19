#pragma once

#include <QString>
#include <QStringList>
#include <QSize>
#include <QPoint>

/**
 * @brief Data structure for EonPlay user preferences and settings
 * 
 * Contains all user-configurable settings for EonPlay - the timeless media player.
 * This structure is serialized/deserialized by the SettingsManager.
 */
struct UserPreferences
{
    // Playback settings
    int volume = 75;                    // Volume level (0-100)
    bool muted = false;                 // Mute state
    double playbackSpeed = 1.0;         // Playback speed multiplier
    bool resumePlayback = true;         // Resume from last position
    bool autoplay = false;              // Auto-start playback when file is opened
    bool loopPlaylist = false;          // Loop playlist when it ends
    bool shufflePlaylist = false;       // Shuffle playlist order
    
    // Advanced playback settings
    bool crossfadeEnabled = false;      // Enable crossfade between tracks
    int crossfadeDuration = 3000;       // Crossfade duration in milliseconds
    bool gaplessPlayback = false;       // Enable gapless playback
    bool seekThumbnailsEnabled = true;  // Enable seek preview thumbnails
    int fastSeekSpeed = 5;              // Default fast seek speed multiplier
    
    // UI settings
    QString theme = "system";           // Theme: "light", "dark", "system"
    QString language = "system";        // UI language
    QSize windowSize = QSize(1024, 768); // Main window size
    QPoint windowPosition = QPoint(-1, -1); // Main window position (-1,-1 = center)
    bool windowMaximized = false;       // Window maximized state
    bool showMenuBar = true;            // Show menu bar
    bool showStatusBar = true;          // Show status bar
    bool showPlaylist = true;           // Show playlist panel
    bool showLibrary = false;           // Show library panel
    
    // Hardware settings
    bool hardwareAcceleration = true;   // Enable hardware acceleration
    QString preferredAccelerationType = "auto"; // Preferred acceleration: "auto", "dxva", "vaapi", "vdpau", "none"
    QString audioDevice = "default";    // Audio output device
    int audioBufferSize = 1024;         // Audio buffer size
    
    // Subtitle settings
    QString subtitleFont = "Arial";     // Subtitle font family
    int subtitleFontSize = 16;          // Subtitle font size
    QString subtitleColor = "#FFFFFF";  // Subtitle text color
    QString subtitleBackgroundColor = "#80000000"; // Subtitle background color
    int subtitleDelay = 0;              // Subtitle delay in milliseconds
    bool subtitleAutoLoad = true;       // Auto-load subtitle files
    
    // Library settings
    QStringList libraryPaths;           // Media library scan paths
    bool autoScanLibrary = true;        // Auto-scan library on startup
    int scanInterval = 24;              // Library scan interval in hours
    bool extractThumbnails = true;      // Extract video thumbnails
    bool extractMetadata = true;        // Extract media metadata
    
    // Network settings
    bool enableNetworking = true;       // Enable network features
    QString proxyType = "none";         // Proxy type: "none", "http", "socks5"
    QString proxyHost;                  // Proxy host
    int proxyPort = 0;                  // Proxy port
    QString proxyUsername;              // Proxy username
    QString proxyPassword;              // Proxy password (encrypted)
    
    // Privacy settings
    bool collectUsageStats = false;     // Collect anonymous usage statistics
    bool checkForUpdates = true;        // Check for application updates
    bool rememberRecentFiles = true;    // Remember recently opened files
    int maxRecentFiles = 10;            // Maximum recent files to remember
    
    // Advanced settings
    bool enableLogging = true;          // Enable debug logging
    QString logLevel = "info";          // Log level: "debug", "info", "warning", "error"
    bool enableCrashReporting = true;   // Enable crash reporting
    QString updateChannel = "stable";   // Update channel: "stable", "beta"
    
    /**
     * @brief Reset all settings to default values
     */
    void resetToDefaults();
    
    /**
     * @brief Validate settings and fix invalid values
     * @return true if all settings are valid, false if some were corrected
     */
    bool validate();
};
#include "UserPreferences.h"
#include <QApplication>
#include <QScreen>
#include <algorithm>

void UserPreferences::resetToDefaults()
{
    // Playback settings
    volume = 75;
    muted = false;
    playbackSpeed = 1.0;
    resumePlayback = true;
    autoplay = false;
    loopPlaylist = false;
    shufflePlaylist = false;
    
    // Advanced playback settings
    crossfadeEnabled = false;
    crossfadeDuration = 3000;
    gaplessPlayback = false;
    seekThumbnailsEnabled = true;
    fastSeekSpeed = 5;
    
    // UI settings
    theme = "system";
    language = "system";
    windowSize = QSize(1024, 768);
    windowPosition = QPoint(-1, -1);
    windowMaximized = false;
    showMenuBar = true;
    showStatusBar = true;
    showPlaylist = true;
    showLibrary = false;
    
    // Hardware settings
    hardwareAcceleration = true;
    preferredAccelerationType = "auto";
    audioDevice = "default";
    audioBufferSize = 1024;
    
    // Subtitle settings
    subtitleFont = "Arial";
    subtitleFontSize = 16;
    subtitleColor = "#FFFFFF";
    subtitleBackgroundColor = "#80000000";
    subtitleDelay = 0;
    subtitleAutoLoad = true;
    
    // Library settings
    libraryPaths.clear();
    autoScanLibrary = true;
    scanInterval = 24;
    extractThumbnails = true;
    extractMetadata = true;
    
    // Network settings
    enableNetworking = true;
    proxyType = "none";
    proxyHost.clear();
    proxyPort = 0;
    proxyUsername.clear();
    proxyPassword.clear();
    
    // Privacy settings
    collectUsageStats = false;
    checkForUpdates = true;
    rememberRecentFiles = true;
    maxRecentFiles = 10;
    
    // Advanced settings
    enableLogging = true;
    logLevel = "info";
    enableCrashReporting = true;
    updateChannel = "stable";
}

bool UserPreferences::validate()
{
    bool allValid = true;
    
    // Validate volume (0-100)
    if (volume < 0 || volume > 100) {
        volume = std::clamp(volume, 0, 100);
        allValid = false;
    }
    
    // Validate playback speed (0.25-4.0)
    if (playbackSpeed < 0.25 || playbackSpeed > 4.0) {
        playbackSpeed = std::clamp(playbackSpeed, 0.25, 4.0);
        allValid = false;
    }
    
    // Validate theme
    if (theme != "light" && theme != "dark" && theme != "system") {
        theme = "system";
        allValid = false;
    }
    
    // Validate window size
    if (windowSize.width() < 400 || windowSize.height() < 300) {
        windowSize = QSize(std::max(400, windowSize.width()), 
                          std::max(300, windowSize.height()));
        allValid = false;
    }
    
    // Validate window position against screen geometry
    if (QApplication::instance()) {
        QScreen* screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            if (windowPosition.x() >= 0 && windowPosition.y() >= 0) {
                if (!screenGeometry.contains(windowPosition)) {
                    windowPosition = QPoint(-1, -1); // Reset to center
                    allValid = false;
                }
            }
        }
    }
    
    // Validate audio buffer size (power of 2, between 256 and 8192)
    if (audioBufferSize < 256 || audioBufferSize > 8192 || 
        (audioBufferSize & (audioBufferSize - 1)) != 0) {
        audioBufferSize = 1024;
        allValid = false;
    }
    
    // Validate subtitle font size (8-72)
    if (subtitleFontSize < 8 || subtitleFontSize > 72) {
        subtitleFontSize = std::clamp(subtitleFontSize, 8, 72);
        allValid = false;
    }
    
    // Validate subtitle delay (-60000 to 60000 ms)
    if (subtitleDelay < -60000 || subtitleDelay > 60000) {
        subtitleDelay = std::clamp(subtitleDelay, -60000, 60000);
        allValid = false;
    }
    
    // Validate scan interval (1-168 hours)
    if (scanInterval < 1 || scanInterval > 168) {
        scanInterval = std::clamp(scanInterval, 1, 168);
        allValid = false;
    }
    
    // Validate proxy port (0-65535)
    if (proxyPort < 0 || proxyPort > 65535) {
        proxyPort = std::clamp(proxyPort, 0, 65535);
        allValid = false;
    }
    
    // Validate proxy type
    if (proxyType != "none" && proxyType != "http" && proxyType != "socks5") {
        proxyType = "none";
        allValid = false;
    }
    
    // Validate max recent files (1-50)
    if (maxRecentFiles < 1 || maxRecentFiles > 50) {
        maxRecentFiles = std::clamp(maxRecentFiles, 1, 50);
        allValid = false;
    }
    
    // Validate log level
    if (logLevel != "debug" && logLevel != "info" && 
        logLevel != "warning" && logLevel != "error") {
        logLevel = "info";
        allValid = false;
    }
    
    // Validate update channel
    if (updateChannel != "stable" && updateChannel != "beta") {
        updateChannel = "stable";
        allValid = false;
    }
    
    // Validate preferred acceleration type
    if (preferredAccelerationType != "auto" && preferredAccelerationType != "dxva" && 
        preferredAccelerationType != "vaapi" && preferredAccelerationType != "vdpau" && 
        preferredAccelerationType != "none") {
        preferredAccelerationType = "auto";
        allValid = false;
    }
    
    // Validate advanced playback settings
    
    // Validate crossfade duration (500-10000 ms)
    if (crossfadeDuration < 500 || crossfadeDuration > 10000) {
        crossfadeDuration = std::clamp(crossfadeDuration, 500, 10000);
        allValid = false;
    }
    
    // Validate fast seek speed (1-20x)
    if (fastSeekSpeed < 1 || fastSeekSpeed > 20) {
        fastSeekSpeed = std::clamp(fastSeekSpeed, 1, 20);
        allValid = false;
    }
    
    return allValid;
}
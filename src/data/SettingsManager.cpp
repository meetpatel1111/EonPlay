#include "SettingsManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QLoggingCategory>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QCoreApplication>

Q_LOGGING_CATEGORY(settingsManager, "eonplay.settings")

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent)
    , m_autoSaveTimer(new QTimer(this))
    , m_initialized(false)
    , m_settingsChanged(false)
{
    // Set up auto-save timer
    m_autoSaveTimer->setSingleShot(false);
    m_autoSaveTimer->setInterval(AUTO_SAVE_INTERVAL_MS);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &SettingsManager::autoSaveSettings);
    
    qCDebug(settingsManager) << "SettingsManager created";
}

SettingsManager::~SettingsManager()
{
    if (m_initialized) {
        shutdown();
    }
}

bool SettingsManager::initialize()
{
    if (m_initialized) {
        qCWarning(settingsManager) << "SettingsManager already initialized";
        return true;
    }
    
    qCDebug(settingsManager) << "Initializing SettingsManager";
    
    try {
        // Initialize QSettings
        initializeQSettings();
        
        // Load existing settings or create defaults
        loadSettings();
        
        // Check if migration is needed
        if (needsMigration()) {
            qCDebug(settingsManager) << "Settings migration required";
            if (!migrateSettings()) {
                qCWarning(settingsManager) << "Settings migration failed, using defaults";
                resetToDefaults();
            }
        }
        
        // Validate loaded preferences
        if (!m_preferences.validate()) {
            qCWarning(settingsManager) << "Some preferences were invalid and have been corrected";
            saveSettings();
        }
        
        // Start auto-save timer
        m_autoSaveTimer->start();
        
        m_initialized = true;
        qCDebug(settingsManager) << "SettingsManager initialized successfully";
        
        return true;
        
    } catch (const std::exception& e) {
        qCCritical(settingsManager) << "Exception during initialization:" << e.what();
        return false;
    } catch (...) {
        qCCritical(settingsManager) << "Unknown exception during initialization";
        return false;
    }
}

void SettingsManager::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qCDebug(settingsManager) << "Shutting down SettingsManager";
    
    // Stop auto-save timer
    m_autoSaveTimer->stop();
    
    // Save any pending changes
    if (m_settingsChanged) {
        saveSettings();
    }
    
    m_settings.reset();
    m_initialized = false;
    
    qCDebug(settingsManager) << "SettingsManager shutdown complete";
}

const UserPreferences& SettingsManager::preferences() const
{
    return m_preferences;
}

void SettingsManager::setPreferences(const UserPreferences& preferences)
{
    UserPreferences validatedPrefs = preferences;
    validatedPrefs.validate();
    
    if (m_preferences.volume != validatedPrefs.volume ||
        m_preferences.theme != validatedPrefs.theme ||
        m_preferences.windowSize != validatedPrefs.windowSize) {
        // Only log if significant changes occurred
        qCDebug(settingsManager) << "Preferences updated";
    }
    
    m_preferences = validatedPrefs;
    m_settingsChanged = true;
    
    emit preferencesChanged(m_preferences);
}

QVariant SettingsManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    if (!m_settings) {
        return defaultValue;
    }
    
    return m_settings->value(key, defaultValue);
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    if (!m_settings) {
        return;
    }
    
    QVariant oldValue = m_settings->value(key);
    if (oldValue != value) {
        m_settings->setValue(key, value);
        m_settingsChanged = true;
        emit settingChanged(key, value);
    }
}

void SettingsManager::saveSettings()
{
    if (!m_settings) {
        qCWarning(settingsManager) << "Cannot save settings: QSettings not initialized";
        return;
    }
    
    qCDebug(settingsManager) << "Saving settings to" << getSettingsFilePath();
    
    try {
        savePreferencesToSettings();
        m_settings->sync();
        m_settingsChanged = false;
        
        if (m_settings->status() != QSettings::NoError) {
            qCWarning(settingsManager) << "Error saving settings:" << m_settings->status();
        } else {
            qCDebug(settingsManager) << "Settings saved successfully";
            emit settingsSaved();
        }
        
    } catch (const std::exception& e) {
        qCCritical(settingsManager) << "Exception while saving settings:" << e.what();
    } catch (...) {
        qCCritical(settingsManager) << "Unknown exception while saving settings";
    }
}

void SettingsManager::loadSettings()
{
    if (!m_settings) {
        qCWarning(settingsManager) << "Cannot load settings: QSettings not initialized";
        return;
    }
    
    qCDebug(settingsManager) << "Loading settings from" << getSettingsFilePath();
    
    try {
        loadPreferencesFromSettings();
        qCDebug(settingsManager) << "Settings loaded successfully";
        emit settingsLoaded();
        
    } catch (const std::exception& e) {
        qCWarning(settingsManager) << "Exception while loading settings:" << e.what();
        qCDebug(settingsManager) << "Using default preferences";
        m_preferences.resetToDefaults();
    } catch (...) {
        qCWarning(settingsManager) << "Unknown exception while loading settings";
        qCDebug(settingsManager) << "Using default preferences";
        m_preferences.resetToDefaults();
    }
}

void SettingsManager::resetToDefaults()
{
    qCDebug(settingsManager) << "Resetting all settings to defaults";
    
    m_preferences.resetToDefaults();
    m_settingsChanged = true;
    
    if (m_settings) {
        m_settings->clear();
        saveSettings();
    }
    
    emit preferencesChanged(m_preferences);
}

QString SettingsManager::getEncryptedValue(const QString& key, const QString& defaultValue) const
{
    QString encryptedValue = getValue(key + "_encrypted").toString();
    if (encryptedValue.isEmpty()) {
        return defaultValue;
    }
    
    return decryptString(encryptedValue);
}

void SettingsManager::setEncryptedValue(const QString& key, const QString& value)
{
    QString encryptedValue = encryptString(value);
    setValue(key + "_encrypted", encryptedValue);
}

bool SettingsManager::needsMigration() const
{
    if (!m_settings) {
        return false;
    }
    
    QString currentVersion = m_settings->value(SETTINGS_VERSION_KEY).toString();
    return currentVersion != CURRENT_SETTINGS_VERSION;
}

bool SettingsManager::migrateSettings()
{
    if (!m_settings) {
        return false;
    }
    
    QString currentVersion = m_settings->value(SETTINGS_VERSION_KEY).toString();
    qCDebug(settingsManager) << "Migrating settings from version" << currentVersion << "to" << CURRENT_SETTINGS_VERSION;
    
    try {
        if (currentVersion.isEmpty() || currentVersion.startsWith("0.")) {
            migrateFromV0ToV1();
        }
        
        // Set new version
        m_settings->setValue(SETTINGS_VERSION_KEY, CURRENT_SETTINGS_VERSION);
        m_settings->sync();
        
        qCDebug(settingsManager) << "Settings migration completed successfully";
        return true;
        
    } catch (const std::exception& e) {
        qCCritical(settingsManager) << "Exception during settings migration:" << e.what();
        return false;
    } catch (...) {
        qCCritical(settingsManager) << "Unknown exception during settings migration";
        return false;
    }
}

QString SettingsManager::getConfigurationPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString SettingsManager::getSettingsFilePath() const
{
    if (m_settings) {
        return m_settings->fileName();
    }
    return QString();
}

void SettingsManager::autoSaveSettings()
{
    if (m_settingsChanged) {
        saveSettings();
    }
}

void SettingsManager::initializeQSettings()
{
    // Ensure config directory exists
    QString configPath = getConfigurationPath();
    QDir().mkpath(configPath);
    
    // Create QSettings instance
    m_settings = std::make_unique<QSettings>(
        configPath + "/settings.ini",
        QSettings::IniFormat
    );
    
    qCDebug(settingsManager) << "QSettings initialized with file:" << m_settings->fileName();
}

void SettingsManager::loadPreferencesFromSettings()
{
    if (!m_settings) {
        return;
    }
    
    // Load playback settings
    m_preferences.volume = m_settings->value("playback/volume", 75).toInt();
    m_preferences.muted = m_settings->value("playback/muted", false).toBool();
    m_preferences.playbackSpeed = m_settings->value("playback/speed", 1.0).toDouble();
    m_preferences.resumePlayback = m_settings->value("playback/resume", true).toBool();
    m_preferences.autoplay = m_settings->value("playback/autoplay", false).toBool();
    m_preferences.loopPlaylist = m_settings->value("playback/loop", false).toBool();
    m_preferences.shufflePlaylist = m_settings->value("playback/shuffle", false).toBool();
    
    // Load advanced playback settings
    m_preferences.crossfadeEnabled = m_settings->value("playback/crossfadeEnabled", false).toBool();
    m_preferences.crossfadeDuration = m_settings->value("playback/crossfadeDuration", 3000).toInt();
    m_preferences.gaplessPlayback = m_settings->value("playback/gaplessPlayback", false).toBool();
    m_preferences.seekThumbnailsEnabled = m_settings->value("playback/seekThumbnailsEnabled", true).toBool();
    m_preferences.fastSeekSpeed = m_settings->value("playback/fastSeekSpeed", 5).toInt();
    
    // Load UI settings
    m_preferences.theme = m_settings->value("ui/theme", "system").toString();
    m_preferences.language = m_settings->value("ui/language", "system").toString();
    m_preferences.windowSize = m_settings->value("ui/windowSize", QSize(1024, 768)).toSize();
    m_preferences.windowPosition = m_settings->value("ui/windowPosition", QPoint(-1, -1)).toPoint();
    m_preferences.windowMaximized = m_settings->value("ui/windowMaximized", false).toBool();
    m_preferences.showMenuBar = m_settings->value("ui/showMenuBar", true).toBool();
    m_preferences.showStatusBar = m_settings->value("ui/showStatusBar", true).toBool();
    m_preferences.showPlaylist = m_settings->value("ui/showPlaylist", true).toBool();
    m_preferences.showLibrary = m_settings->value("ui/showLibrary", false).toBool();
    
    // Load hardware settings
    m_preferences.hardwareAcceleration = m_settings->value("hardware/acceleration", true).toBool();
    m_preferences.preferredAccelerationType = m_settings->value("hardware/preferredAccelerationType", "auto").toString();
    m_preferences.audioDevice = m_settings->value("hardware/audioDevice", "default").toString();
    m_preferences.audioBufferSize = m_settings->value("hardware/audioBufferSize", 1024).toInt();
    
    // Load subtitle settings
    m_preferences.subtitleFont = m_settings->value("subtitles/font", "Arial").toString();
    m_preferences.subtitleFontSize = m_settings->value("subtitles/fontSize", 16).toInt();
    m_preferences.subtitleColor = m_settings->value("subtitles/color", "#FFFFFF").toString();
    m_preferences.subtitleBackgroundColor = m_settings->value("subtitles/backgroundColor", "#80000000").toString();
    m_preferences.subtitleDelay = m_settings->value("subtitles/delay", 0).toInt();
    m_preferences.subtitleAutoLoad = m_settings->value("subtitles/autoLoad", true).toBool();
    
    // Load library settings
    m_preferences.libraryPaths = m_settings->value("library/paths").toStringList();
    m_preferences.autoScanLibrary = m_settings->value("library/autoScan", true).toBool();
    m_preferences.scanInterval = m_settings->value("library/scanInterval", 24).toInt();
    m_preferences.extractThumbnails = m_settings->value("library/extractThumbnails", true).toBool();
    m_preferences.extractMetadata = m_settings->value("library/extractMetadata", true).toBool();
    
    // Load network settings
    m_preferences.enableNetworking = m_settings->value("network/enabled", true).toBool();
    m_preferences.proxyType = m_settings->value("network/proxyType", "none").toString();
    m_preferences.proxyHost = m_settings->value("network/proxyHost").toString();
    m_preferences.proxyPort = m_settings->value("network/proxyPort", 0).toInt();
    m_preferences.proxyUsername = m_settings->value("network/proxyUsername").toString();
    m_preferences.proxyPassword = getEncryptedValue("network/proxyPassword");
    
    // Load privacy settings
    m_preferences.collectUsageStats = m_settings->value("privacy/collectStats", false).toBool();
    m_preferences.checkForUpdates = m_settings->value("privacy/checkUpdates", true).toBool();
    m_preferences.rememberRecentFiles = m_settings->value("privacy/rememberRecent", true).toBool();
    m_preferences.maxRecentFiles = m_settings->value("privacy/maxRecentFiles", 10).toInt();
    
    // Load advanced settings
    m_preferences.enableLogging = m_settings->value("advanced/enableLogging", true).toBool();
    m_preferences.logLevel = m_settings->value("advanced/logLevel", "info").toString();
    m_preferences.enableCrashReporting = m_settings->value("advanced/crashReporting", true).toBool();
    m_preferences.updateChannel = m_settings->value("advanced/updateChannel", "stable").toString();
}

void SettingsManager::savePreferencesToSettings()
{
    if (!m_settings) {
        return;
    }
    
    // Save playback settings
    m_settings->setValue("playback/volume", m_preferences.volume);
    m_settings->setValue("playback/muted", m_preferences.muted);
    m_settings->setValue("playback/speed", m_preferences.playbackSpeed);
    m_settings->setValue("playback/resume", m_preferences.resumePlayback);
    m_settings->setValue("playback/autoplay", m_preferences.autoplay);
    m_settings->setValue("playback/loop", m_preferences.loopPlaylist);
    m_settings->setValue("playback/shuffle", m_preferences.shufflePlaylist);
    
    // Save advanced playback settings
    m_settings->setValue("playback/crossfadeEnabled", m_preferences.crossfadeEnabled);
    m_settings->setValue("playback/crossfadeDuration", m_preferences.crossfadeDuration);
    m_settings->setValue("playback/gaplessPlayback", m_preferences.gaplessPlayback);
    m_settings->setValue("playback/seekThumbnailsEnabled", m_preferences.seekThumbnailsEnabled);
    m_settings->setValue("playback/fastSeekSpeed", m_preferences.fastSeekSpeed);
    
    // Save UI settings
    m_settings->setValue("ui/theme", m_preferences.theme);
    m_settings->setValue("ui/language", m_preferences.language);
    m_settings->setValue("ui/windowSize", m_preferences.windowSize);
    m_settings->setValue("ui/windowPosition", m_preferences.windowPosition);
    m_settings->setValue("ui/windowMaximized", m_preferences.windowMaximized);
    m_settings->setValue("ui/showMenuBar", m_preferences.showMenuBar);
    m_settings->setValue("ui/showStatusBar", m_preferences.showStatusBar);
    m_settings->setValue("ui/showPlaylist", m_preferences.showPlaylist);
    m_settings->setValue("ui/showLibrary", m_preferences.showLibrary);
    
    // Save hardware settings
    m_settings->setValue("hardware/acceleration", m_preferences.hardwareAcceleration);
    m_settings->setValue("hardware/preferredAccelerationType", m_preferences.preferredAccelerationType);
    m_settings->setValue("hardware/audioDevice", m_preferences.audioDevice);
    m_settings->setValue("hardware/audioBufferSize", m_preferences.audioBufferSize);
    
    // Save subtitle settings
    m_settings->setValue("subtitles/font", m_preferences.subtitleFont);
    m_settings->setValue("subtitles/fontSize", m_preferences.subtitleFontSize);
    m_settings->setValue("subtitles/color", m_preferences.subtitleColor);
    m_settings->setValue("subtitles/backgroundColor", m_preferences.subtitleBackgroundColor);
    m_settings->setValue("subtitles/delay", m_preferences.subtitleDelay);
    m_settings->setValue("subtitles/autoLoad", m_preferences.subtitleAutoLoad);
    
    // Save library settings
    m_settings->setValue("library/paths", m_preferences.libraryPaths);
    m_settings->setValue("library/autoScan", m_preferences.autoScanLibrary);
    m_settings->setValue("library/scanInterval", m_preferences.scanInterval);
    m_settings->setValue("library/extractThumbnails", m_preferences.extractThumbnails);
    m_settings->setValue("library/extractMetadata", m_preferences.extractMetadata);
    
    // Save network settings
    m_settings->setValue("network/enabled", m_preferences.enableNetworking);
    m_settings->setValue("network/proxyType", m_preferences.proxyType);
    m_settings->setValue("network/proxyHost", m_preferences.proxyHost);
    m_settings->setValue("network/proxyPort", m_preferences.proxyPort);
    m_settings->setValue("network/proxyUsername", m_preferences.proxyUsername);
    setEncryptedValue("network/proxyPassword", m_preferences.proxyPassword);
    
    // Save privacy settings
    m_settings->setValue("privacy/collectStats", m_preferences.collectUsageStats);
    m_settings->setValue("privacy/checkUpdates", m_preferences.checkForUpdates);
    m_settings->setValue("privacy/rememberRecent", m_preferences.rememberRecentFiles);
    m_settings->setValue("privacy/maxRecentFiles", m_preferences.maxRecentFiles);
    
    // Save advanced settings
    m_settings->setValue("advanced/enableLogging", m_preferences.enableLogging);
    m_settings->setValue("advanced/logLevel", m_preferences.logLevel);
    m_settings->setValue("advanced/crashReporting", m_preferences.enableCrashReporting);
    m_settings->setValue("advanced/updateChannel", m_preferences.updateChannel);
    
    // Save settings version
    m_settings->setValue(SETTINGS_VERSION_KEY, CURRENT_SETTINGS_VERSION);
}

QString SettingsManager::encryptString(const QString& plaintext) const
{
    if (plaintext.isEmpty()) {
        return QString();
    }
    
    // Simple XOR encryption with key (not cryptographically secure, but better than plaintext)
    QByteArray key = getEncryptionKey();
    QByteArray data = plaintext.toUtf8();
    
    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }
    
    return data.toBase64();
}

QString SettingsManager::decryptString(const QString& ciphertext) const
{
    if (ciphertext.isEmpty()) {
        return QString();
    }
    
    QByteArray data = QByteArray::fromBase64(ciphertext.toUtf8());
    QByteArray key = getEncryptionKey();
    
    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }
    
    return QString::fromUtf8(data);
}

QByteArray SettingsManager::getEncryptionKey() const
{
    // Generate a simple key based on system information
    // In a production app, you might want to use a more sophisticated approach
    QString keySource = QSysInfo::machineHostName() + 
                       QCoreApplication::applicationName() + 
                       "MediaPlayerEncryptionKey2024";
    
    return QCryptographicHash::hash(keySource.toUtf8(), QCryptographicHash::Sha256);
}

void SettingsManager::migrateFromV0ToV1()
{
    qCDebug(settingsManager) << "Migrating settings from v0.x to v1.0";
    
    // Migration logic would go here
    // For now, we'll just ensure the version is set
    // In a real migration, you would:
    // 1. Read old setting keys
    // 2. Convert to new format
    // 3. Remove old keys
    // 4. Set new version
}


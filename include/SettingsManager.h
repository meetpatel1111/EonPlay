#pragma once

#include "IComponent.h"
#include "UserPreferences.h"
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <memory>

/**
 * @brief Manages EonPlay settings and user preferences
 * 
 * Provides centralized settings management for EonPlay with automatic persistence,
 * validation, encryption for sensitive data, and migration support.
 */
class SettingsManager : public QObject, public IComponent
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject* parent = nullptr);
    ~SettingsManager();
    
    // IComponent interface
    bool initialize() override;
    void shutdown() override;
    QString componentName() const override { return "SettingsManager"; }
    bool isInitialized() const override { return m_initialized; }
    
    /**
     * @brief Get current user preferences
     * @return Reference to user preferences
     */
    const UserPreferences& preferences() const;
    
    /**
     * @brief Update user preferences
     * @param preferences New preferences to set
     */
    void setPreferences(const UserPreferences& preferences);
    
    /**
     * @brief Get a specific setting value
     * @param key Setting key
     * @param defaultValue Default value if key doesn't exist
     * @return Setting value
     */
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    /**
     * @brief Set a specific setting value
     * @param key Setting key
     * @param value Setting value
     */
    void setValue(const QString& key, const QVariant& value);
    
    /**
     * @brief Save settings immediately
     */
    void saveSettings();
    
    /**
     * @brief Load settings from storage
     */
    void loadSettings();
    
    /**
     * @brief Reset all settings to defaults
     */
    void resetToDefaults();
    
    /**
     * @brief Get encrypted setting value
     * @param key Setting key
     * @param defaultValue Default value if key doesn't exist
     * @return Decrypted setting value
     */
    QString getEncryptedValue(const QString& key, const QString& defaultValue = QString()) const;
    
    /**
     * @brief Set encrypted setting value
     * @param key Setting key
     * @param value Value to encrypt and store
     */
    void setEncryptedValue(const QString& key, const QString& value);
    
    /**
     * @brief Check if settings migration is needed
     * @return true if migration is required
     */
    bool needsMigration() const;
    
    /**
     * @brief Perform settings migration from older version
     * @return true if migration was successful
     */
    bool migrateSettings();
    
    /**
     * @brief Get platform-specific configuration directory
     * @return Configuration directory path
     */
    static QString getConfigurationPath();
    
    /**
     * @brief Get settings file path
     * @return Full path to settings file
     */
    QString getSettingsFilePath() const;

signals:
    /**
     * @brief Emitted when preferences are changed
     * @param preferences Updated preferences
     */
    void preferencesChanged(const UserPreferences& preferences);
    
    /**
     * @brief Emitted when a specific setting is changed
     * @param key Setting key that changed
     * @param value New value
     */
    void settingChanged(const QString& key, const QVariant& value);
    
    /**
     * @brief Emitted when settings are saved
     */
    void settingsSaved();
    
    /**
     * @brief Emitted when settings are loaded
     */
    void settingsLoaded();

private slots:
    /**
     * @brief Auto-save settings (called by timer)
     */
    void autoSaveSettings();

private:
    /**
     * @brief Initialize QSettings with proper configuration
     */
    void initializeQSettings();
    
    /**
     * @brief Load preferences from QSettings
     */
    void loadPreferencesFromSettings();
    
    /**
     * @brief Save preferences to QSettings
     */
    void savePreferencesToSettings();
    
    /**
     * @brief Encrypt a string value
     * @param plaintext String to encrypt
     * @return Encrypted string (base64 encoded)
     */
    QString encryptString(const QString& plaintext) const;
    
    /**
     * @brief Decrypt a string value
     * @param ciphertext Encrypted string (base64 encoded)
     * @return Decrypted string
     */
    QString decryptString(const QString& ciphertext) const;
    
    /**
     * @brief Get encryption key for sensitive data
     * @return Encryption key
     */
    QByteArray getEncryptionKey() const;
    
    /**
     * @brief Migrate from version 0.x to 1.x
     */
    void migrateFromV0ToV1();
    
    std::unique_ptr<QSettings> m_settings;
    UserPreferences m_preferences;
    QTimer* m_autoSaveTimer;
    bool m_initialized;
    bool m_settingsChanged;
    
    static constexpr int AUTO_SAVE_INTERVAL_MS = 30000; // 30 seconds
    static constexpr const char* SETTINGS_VERSION_KEY = "version";
    static constexpr const char* CURRENT_SETTINGS_VERSION = "1.0";
};
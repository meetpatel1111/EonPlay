#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QTimer>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <memory>

class QSettings;

/**
 * @brief Comprehensive security manager for EonPlay
 * 
 * Provides sandboxed decoding, malicious content protection, encrypted configuration
 * storage, plugin signing verification, and security monitoring for EonPlay media player.
 */
class SecurityManager : public QObject
{
    Q_OBJECT

public:
    enum SecurityLevel {
        SECURITY_DISABLED,
        SECURITY_BASIC,
        SECURITY_ENHANCED,
        SECURITY_PARANOID
    };

    enum ThreatType {
        THREAT_NONE,
        THREAT_MALICIOUS_FILE,
        THREAT_SUSPICIOUS_SUBTITLE,
        THREAT_INVALID_PLUGIN,
        THREAT_BUFFER_OVERFLOW,
        THREAT_DIRECTORY_TRAVERSAL,
        THREAT_NETWORK_EXPLOIT,
        THREAT_UNKNOWN
    };

    struct SecurityEvent {
        ThreatType threatType;
        QString description;
        QString filePath;
        QString sourceAddress;
        QDateTime timestamp;
        QString mitigation;
        bool blocked = false;
    };

    struct FileValidationResult {
        bool isValid = false;
        bool isSafe = false;
        QString mimeType;
        qint64 fileSize = 0;
        QStringList warnings;
        QStringList threats;
        QString hash;
    };

    struct PluginSignature {
        QString pluginPath;
        QString signature;
        QString certificate;
        bool isValid = false;
        bool isTrusted = false;
        QDateTime signedDate;
        QString issuer;
    };

    explicit SecurityManager(QObject *parent = nullptr);
    ~SecurityManager();

    // Security configuration
    void setSecurityLevel(SecurityLevel level);
    SecurityLevel getSecurityLevel() const;
    void enableSandboxedDecoding(bool enabled);
    bool isSandboxedDecodingEnabled() const;
    void enableMaliciousContentProtection(bool enabled);
    bool isMaliciousContentProtectionEnabled() const;

    // File validation and sanitization
    FileValidationResult validateMediaFile(const QString& filePath);
    bool isFilePathSafe(const QString& filePath);
    QString sanitizeFilePath(const QString& filePath);
    bool validateFileHeaders(const QString& filePath);
    QByteArray sanitizeSubtitleContent(const QByteArray& content);

    // Encrypted configuration storage
    bool storeEncryptedSetting(const QString& key, const QVariant& value);
    QVariant retrieveEncryptedSetting(const QString& key, const QVariant& defaultValue = QVariant());
    bool removeEncryptedSetting(const QString& key);
    void setEncryptionKey(const QByteArray& key);
    bool hasValidEncryptionKey() const;

    // Plugin security
    bool verifyPluginSignature(const QString& pluginPath);
    PluginSignature getPluginSignature(const QString& pluginPath);
    bool isPluginTrusted(const QString& pluginPath);
    void addTrustedPlugin(const QString& pluginPath, const QString& signature);
    void removeTrustedPlugin(const QString& pluginPath);
    QStringList getTrustedPlugins() const;

    // Sandboxed process management
    bool startSandboxedProcess(const QString& executable, const QStringList& arguments);
    void terminateSandboxedProcesses();
    bool isSandboxedProcessRunning(const QString& processName) const;
    void setSandboxPermissions(const QStringList& allowedPaths, const QStringList& allowedNetworks);

    // Security monitoring
    void startSecurityMonitoring();
    void stopSecurityMonitoring();
    bool isSecurityMonitoringActive() const;
    QList<SecurityEvent> getSecurityEvents(int maxEvents = 100) const;
    void clearSecurityEvents();

    // Threat detection and mitigation
    bool detectThreat(const QString& filePath, const QByteArray& content = QByteArray());
    void reportThreat(ThreatType type, const QString& description, const QString& source = QString());
    void blockThreat(const QString& source);
    bool isSourceBlocked(const QString& source) const;
    void unblockSource(const QString& source);

    // DRM support (optional)
    bool isDRMSupported() const;
    bool validateDRMContent(const QString& filePath);
    QString getDRMStatus(const QString& filePath) const;

    // Security utilities
    QString calculateFileHash(const QString& filePath, QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha256);
    bool verifyFileIntegrity(const QString& filePath, const QString& expectedHash);
    QByteArray generateSecureRandomBytes(int length);
    QString generateSecureToken(int length = 32);

signals:
    void securityLevelChanged(SecurityLevel newLevel);
    void threatDetected(const SecurityEvent& event);
    void threatBlocked(const SecurityEvent& event);
    void fileValidationCompleted(const QString& filePath, const FileValidationResult& result);
    void pluginVerificationCompleted(const QString& pluginPath, const PluginSignature& signature);
    void sandboxedProcessStarted(const QString& processName);
    void sandboxedProcessTerminated(const QString& processName);
    void securityMonitoringStateChanged(bool active);
    void encryptedSettingStored(const QString& key);
    void encryptedSettingRetrieved(const QString& key);

private slots:
    void onFileSystemChanged(const QString& path);
    void onSandboxedProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSecurityTimerTimeout();

private:
    // File validation helpers
    bool validateMediaFileHeaders(const QString& filePath);
    bool validateSubtitleFile(const QString& filePath);
    bool detectMaliciousPatterns(const QByteArray& content);
    QStringList scanForThreats(const QString& filePath);
    void initializeThreatDatabase();
    
    // Encryption helpers
    QByteArray encryptData(const QByteArray& data, const QByteArray& key);
    QByteArray decryptData(const QByteArray& encryptedData, const QByteArray& key);
    QByteArray deriveKey(const QString& password, const QByteArray& salt);
    QByteArray generateSalt();
    
    // Plugin verification helpers
    bool verifyCodeSignature(const QString& filePath);
    QString extractCertificateInfo(const QString& filePath);
    bool validateCertificateChain(const QString& certificate);
    
    // Sandbox helpers
    void setupSandboxEnvironment();
    void configureSandboxPermissions();
    bool createSandboxedProcess(const QString& executable, const QStringList& arguments);
    
    // Security monitoring helpers
    void monitorFileSystem();
    void monitorNetworkActivity();
    void analyzeSystemBehavior();
    void updateThreatDatabase();
    
    // Threat detection helpers
    bool isKnownMaliciousPattern(const QByteArray& pattern);
    bool detectBufferOverflowAttempt(const QByteArray& content);
    bool detectDirectoryTraversalAttempt(const QString& path);
    bool detectScriptInjection(const QString& content);
    
    // Member variables
    SecurityLevel m_securityLevel;
    bool m_sandboxedDecodingEnabled;
    bool m_maliciousContentProtectionEnabled;
    bool m_securityMonitoringActive;
    
    // Encryption
    QByteArray m_encryptionKey;
    QByteArray m_encryptionSalt;
    std::unique_ptr<QSettings> m_encryptedSettings;
    
    // Plugin management
    QMap<QString, PluginSignature> m_trustedPlugins;
    QStringList m_blockedPlugins;
    
    // Sandboxing
    QList<QProcess*> m_sandboxedProcesses;
    QStringList m_allowedPaths;
    QStringList m_allowedNetworks;
    
    // Security monitoring
    std::unique_ptr<QFileSystemWatcher> m_fileWatcher;
    QTimer* m_securityTimer;
    QList<SecurityEvent> m_securityEvents;
    QStringList m_blockedSources;
    
    // Threat detection
    QStringList m_maliciousPatterns;
    QStringList m_suspiciousExtensions;
    QMap<QString, QString> m_knownThreats;
    QMap<QString, QString> m_knownThreats;
    
    // Constants
    static const int MAX_SECURITY_EVENTS = 1000;
    static const int SECURITY_MONITORING_INTERVAL_MS = 5000;
    static const int MAX_FILE_SIZE_MB = 100;
    static const int ENCRYPTION_KEY_LENGTH = 32;
    static const int SALT_LENGTH = 16;
};
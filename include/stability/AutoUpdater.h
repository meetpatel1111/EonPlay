#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <memory>

/**
 * @brief Secure auto-updater with signature verification and rollback capability
 * 
 * Provides automatic update checking, secure download with signature verification,
 * safe installation with rollback capability, and update management for EonPlay.
 */
class AutoUpdater : public QObject
{
    Q_OBJECT

public:
    enum UpdateChannel {
        CHANNEL_STABLE,
        CHANNEL_BETA,
        CHANNEL_ALPHA,
        CHANNEL_NIGHTLY
    };

    enum UpdateState {
        UPDATE_IDLE,
        UPDATE_CHECKING,
        UPDATE_AVAILABLE,
        UPDATE_DOWNLOADING,
        UPDATE_INSTALLING,
        UPDATE_COMPLETED,
        UPDATE_FAILED,
        UPDATE_ROLLBACK
    };

    struct UpdateInfo {
        QString version;
        QString buildNumber;
        QDateTime releaseDate;
        QString downloadUrl;
        QString signature;
        QString checksum;
        qint64 fileSize = 0;
        QString releaseNotes;
        bool isSecurityUpdate = false;
        bool isCritical = false;
        QStringList supportedPlatforms;
        QString minimumVersion;
    };

    struct UpdateProgress {
        qint64 bytesReceived = 0;
        qint64 totalBytes = 0;
        int percentage = 0;
        double downloadSpeed = 0.0; // KB/s
        qint64 timeRemaining = 0; // seconds
        QString currentOperation;
    };

    explicit AutoUpdater(QObject *parent = nullptr);
    ~AutoUpdater();

    // Update checking
    void checkForUpdates();
    void setUpdateChannel(UpdateChannel channel);
    UpdateChannel getUpdateChannel() const;
    void setUpdateServer(const QString& serverUrl);
    QString getUpdateServer() const;
    void setCheckInterval(int hours);
    int getCheckInterval() const;

    // Update management
    bool isUpdateAvailable() const;
    UpdateInfo getAvailableUpdate() const;
    void downloadUpdate();
    void installUpdate();
    void cancelUpdate();
    bool rollbackUpdate();

    // Configuration
    void enableAutomaticUpdates(bool enabled);
    bool isAutomaticUpdatesEnabled() const;
    void enableAutomaticInstallation(bool enabled);
    bool isAutomaticInstallationEnabled() const;
    void setUserConsent(bool hasConsent);
    bool hasUserConsent() const;

    // Security
    void setPublicKey(const QString& publicKey);
    QString getPublicKey() const;
    bool verifyUpdateSignature(const QString& filePath, const QString& signature);
    bool verifyUpdateChecksum(const QString& filePath, const QString& expectedChecksum);

    // State and progress
    UpdateState getCurrentState() const;
    UpdateProgress getCurrentProgress() const;
    QString getLastError() const;
    QDateTime getLastCheckTime() const;

    // Version management
    QString getCurrentVersion() const;
    QString getInstalledVersion() const;
    bool isVersionNewer(const QString& version1, const QString& version2) const;
    void setCurrentVersion(const QString& version);

    // Backup and rollback
    bool createBackup();
    bool restoreBackup();
    bool hasBackup() const;
    QString getBackupPath() const;
    void cleanupOldBackups();

signals:
    void updateCheckStarted();
    void updateCheckCompleted(bool updateAvailable);
    void updateAvailable(const UpdateInfo& updateInfo);
    void updateDownloadStarted();
    void updateDownloadProgress(const UpdateProgress& progress);
    void updateDownloadCompleted(const QString& filePath);
    void updateInstallationStarted();
    void updateInstallationProgress(const UpdateProgress& progress);
    void updateInstallationCompleted();
    void updateFailed(const QString& error);
    void updateRolledBack();
    void stateChanged(UpdateState newState);

private slots:
    void onCheckTimerTimeout();
    void onUpdateCheckReplyFinished();
    void onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onUpdateDownloadFinished();
    void onInstallProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // Update checking helpers
    void performUpdateCheck();
    void parseUpdateResponse(const QByteArray& response);
    bool shouldCheckForUpdates() const;
    QString buildUpdateCheckUrl() const;
    
    // Download helpers
    void startDownload(const QString& url);
    void calculateDownloadSpeed();
    QString generateDownloadPath() const;
    
    // Installation helpers
    void prepareInstallation();
    bool validateDownloadedUpdate();
    void executeInstaller();
    void schedulePostInstallCleanup();
    
    // Security helpers
    bool verifySignature(const QByteArray& data, const QString& signature);
    QString calculateChecksum(const QString& filePath) const;
    bool isSignatureValid(const QString& signature) const;
    
    // Backup helpers
    QString generateBackupPath() const;
    bool copyCurrentInstallation(const QString& backupPath);
    bool restoreFromBackup(const QString& backupPath);
    
    // Version comparison helpers
    QList<int> parseVersion(const QString& version) const;
    int compareVersions(const QList<int>& v1, const QList<int>& v2) const;
    
    // Platform helpers
    QString getPlatformIdentifier() const;
    QString getInstallerExtension() const;
    QStringList getInstallCommand(const QString& installerPath) const;
    
    // State management
    void setState(UpdateState newState);
    void setError(const QString& error);
    void resetProgress();
    
    // Member variables
    UpdateState m_currentState;
    UpdateChannel m_updateChannel;
    QString m_updateServer;
    int m_checkIntervalHours;
    bool m_automaticUpdatesEnabled;
    bool m_automaticInstallationEnabled;
    bool m_userConsent;
    
    // Update information
    UpdateInfo m_availableUpdate;
    UpdateProgress m_currentProgress;
    QString m_lastError;
    QDateTime m_lastCheckTime;
    
    // Version tracking
    QString m_currentVersion;
    QString m_installedVersion;
    
    // Security
    QString m_publicKey;
    
    // Paths
    QString m_downloadPath;
    QString m_backupPath;
    QString m_tempDir;
    
    // Network and process management
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QNetworkReply* m_currentReply;
    std::unique_ptr<QProcess> m_installProcess;
    std::unique_ptr<QTimer> m_checkTimer;
    
    // Download tracking
    QDateTime m_downloadStartTime;
    qint64 m_lastBytesReceived;
    QDateTime m_lastProgressTime;
    
    // Constants
    static const QString DEFAULT_UPDATE_SERVER;
    static const int DEFAULT_CHECK_INTERVAL_HOURS = 24;
    static const int DOWNLOAD_TIMEOUT_MS = 300000; // 5 minutes
    static const int INSTALL_TIMEOUT_MS = 600000;  // 10 minutes
    static const int MAX_BACKUP_COUNT = 3;
};
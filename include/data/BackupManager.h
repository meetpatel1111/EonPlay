#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QStringList>
#include <QMap>

/**
 * @brief Automatic library backup and restore system
 * 
 * Provides automated backup of media library, playlists, settings, and user data
 * with scheduled backups, compression, and restore capabilities for EonPlay.
 */
class BackupManager : public QObject
{
    Q_OBJECT

public:
    enum BackupType {
        BACKUP_LIBRARY_DATABASE,
        BACKUP_PLAYLISTS,
        BACKUP_USER_SETTINGS,
        BACKUP_THUMBNAILS,
        BACKUP_COMPLETE
    };

    enum BackupStatus {
        BACKUP_IDLE,
        BACKUP_IN_PROGRESS,
        BACKUP_COMPLETED,
        BACKUP_FAILED,
        RESTORE_IN_PROGRESS,
        RESTORE_COMPLETED,
        RESTORE_FAILED
    };

    struct BackupInfo {
        QString backupId;
        BackupType type;
        QDateTime timestamp;
        QString filePath;
        qint64 fileSize = 0;
        QString checksum;
        QString description;
        bool isCompressed = true;
        bool isEncrypted = false;
        QStringList includedFiles;
        QString version;
    };

    explicit BackupManager(QObject *parent = nullptr);
    ~BackupManager();

    // Backup operations
    QString createBackup(BackupType type, const QString& description = QString());
    bool restoreBackup(const QString& backupId);
    bool deleteBackup(const QString& backupId);
    bool verifyBackup(const QString& backupId);

    // Automatic backup scheduling
    void enableAutomaticBackup(bool enabled);
    bool isAutomaticBackupEnabled() const;
    void setBackupInterval(int hours);
    int getBackupInterval() const;
    void setMaxBackupCount(int maxCount);
    int getMaxBackupCount() const;

    // Backup management
    QList<BackupInfo> getAvailableBackups() const;
    BackupInfo getBackupInfo(const QString& backupId) const;
    QStringList getBackupIds() const;
    void cleanupOldBackups();
    qint64 getTotalBackupSize() const;

    // Configuration
    void setBackupDirectory(const QString& directory);
    QString getBackupDirectory() const;
    void enableCompression(bool enabled);
    bool isCompressionEnabled() const;
    void enableEncryption(bool enabled);
    bool isEncryptionEnabled() const;
    void setEncryptionKey(const QString& key);

    // Status and progress
    BackupStatus getCurrentStatus() const;
    int getCurrentProgress() const;
    QString getLastError() const;
    QDateTime getLastBackupTime() const;
    QDateTime getNextScheduledBackup() const;

signals:
    void backupStarted(const QString& backupId, BackupType type);
    void backupProgress(const QString& backupId, int percentage);
    void backupCompleted(const QString& backupId);
    void backupFailed(const QString& backupId, const QString& error);
    void restoreStarted(const QString& backupId);
    void restoreProgress(const QString& backupId, int percentage);
    void restoreCompleted(const QString& backupId);
    void restoreFailed(const QString& backupId, const QString& error);
    void automaticBackupTriggered();
    void backupDeleted(const QString& backupId);

private slots:
    void onAutomaticBackupTimer();

private:
    // Backup creation helpers
    QString generateBackupId() const;
    QString createLibraryBackup(const QString& backupId);
    QString createPlaylistsBackup(const QString& backupId);
    QString createSettingsBackup(const QString& backupId);
    QString createThumbnailsBackup(const QString& backupId);
    QString createCompleteBackup(const QString& backupId);
    
    // Compression and encryption
    bool compressBackup(const QString& sourcePath, const QString& targetPath);
    bool decompressBackup(const QString& sourcePath, const QString& targetPath);
    bool encryptBackup(const QString& filePath);
    bool decryptBackup(const QString& filePath);
    
    // Restore helpers
    bool restoreLibraryBackup(const QString& backupPath);
    bool restorePlaylistsBackup(const QString& backupPath);
    bool restoreSettingsBackup(const QString& backupPath);
    bool restoreThumbnailsBackup(const QString& backupPath);
    bool restoreCompleteBackup(const QString& backupPath);
    
    // File operations
    bool copyDirectory(const QString& source, const QString& destination);
    bool removeDirectory(const QString& path);
    QString calculateChecksum(const QString& filePath) const;
    bool verifyChecksum(const QString& filePath, const QString& expectedChecksum) const;
    
    // Backup metadata
    void saveBackupInfo(const BackupInfo& info);
    void loadBackupInfo();
    void removeBackupInfo(const QString& backupId);
    
    // Member variables
    bool m_automaticBackupEnabled;
    int m_backupIntervalHours;
    int m_maxBackupCount;
    bool m_compressionEnabled;
    bool m_encryptionEnabled;
    
    QString m_backupDirectory;
    QString m_encryptionKey;
    
    BackupStatus m_currentStatus;
    int m_currentProgress;
    QString m_lastError;
    QDateTime m_lastBackupTime;
    
    QMap<QString, BackupInfo> m_backupDatabase;
    QTimer* m_automaticBackupTimer;
    
    // Constants
    static const int DEFAULT_BACKUP_INTERVAL_HOURS = 24;
    static const int DEFAULT_MAX_BACKUP_COUNT = 10;
    static const QString BACKUP_METADATA_FILE;
};
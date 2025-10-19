#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>

namespace EonPlay {
namespace Data {

class DatabaseManager;

/**
 * @brief Manages automatic library backups for EonPlay
 * 
 * Provides scheduled and manual backup functionality for the media library database.
 */
class BackupManager : public QObject
{
    Q_OBJECT

public:
    explicit BackupManager(DatabaseManager* dbManager, QObject* parent = nullptr);
    ~BackupManager() = default;

    // Backup configuration
    bool isAutoBackupEnabled() const { return m_autoBackupEnabled; }
    void setAutoBackupEnabled(bool enabled);

    int backupIntervalHours() const { return m_backupIntervalHours; }
    void setBackupIntervalHours(int hours);

    int maxBackupCount() const { return m_maxBackupCount; }
    void setMaxBackupCount(int count);

    QString backupDirectory() const { return m_backupDirectory; }
    void setBackupDirectory(const QString& directory);

    // Manual backup operations
    bool createBackup(const QString& backupName = QString());
    bool restoreFromBackup(const QString& backupPath);
    QStringList getAvailableBackups() const;
    bool deleteBackup(const QString& backupPath);

    // Backup information
    QDateTime lastBackupTime() const { return m_lastBackupTime; }
    QString lastBackupPath() const { return m_lastBackupPath; }
    qint64 getBackupSize(const QString& backupPath) const;

    // Maintenance
    void cleanupOldBackups();
    qint64 getTotalBackupSize() const;

signals:
    void backupCreated(const QString& backupPath);
    void backupFailed(const QString& error);
    void backupRestored(const QString& backupPath);
    void backupDeleted(const QString& backupPath);
    void autoBackupStatusChanged(bool enabled);

private slots:
    void performAutoBackup();

private:
    void setupAutoBackupTimer();
    QString generateBackupFileName() const;
    void updateLastBackupInfo(const QString& backupPath);

    DatabaseManager* m_dbManager;
    
    // Configuration
    bool m_autoBackupEnabled;
    int m_backupIntervalHours;
    int m_maxBackupCount;
    QString m_backupDirectory;
    
    // State
    QDateTime m_lastBackupTime;
    QString m_lastBackupPath;
    QTimer* m_autoBackupTimer;
};

} // namespace Data
} // namespace EonPlay
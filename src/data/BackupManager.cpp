#include "data/BackupManager.h"
#include "data/DatabaseManager.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QTimer>
#include <QLoggingCategory>
#include <QDebug>

Q_LOGGING_CATEGORY(backupManager, "eonplay.backup")

BackupManager::BackupManager(QObject* parent)
    : QObject(parent)
    , m_dbManager(nullptr)
    , m_autoBackupEnabled(true)
    , m_backupIntervalHours(24)
    , m_maxBackupCount(7)
    , m_autoBackupTimer(new QTimer(this))
{
    // Set default backup directory
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_backupDirectory = QDir(appDataPath).absoluteFilePath("backups");
    
    // Ensure backup directory exists
    QDir().mkpath(m_backupDirectory);
    
    // Setup auto backup timer
    setupAutoBackupTimer();
    
    connect(m_autoBackupTimer, &QTimer::timeout, this, &BackupManager::performAutoBackup);
    
    qCInfo(backupManager) << "BackupManager initialized with directory:" << m_backupDirectory;
}

void BackupManager::enableAutomaticBackup(bool enabled)
{
    if (m_autoBackupEnabled != enabled) {
        m_autoBackupEnabled = enabled;
        
        if (enabled) {
            setupAutoBackupTimer();
            m_autoBackupTimer->start();
        } else {
            m_autoBackupTimer->stop();
        }
        
        emit autoBackupStatusChanged(enabled);
        qCInfo(backupManager) << "Auto backup" << (enabled ? "enabled" : "disabled");
    }
}

void BackupManager::setBackupInterval(int hours)
{
    if (hours > 0 && m_backupIntervalHours != hours) {
        m_backupIntervalHours = hours;
        setupAutoBackupTimer();
        qCInfo(backupManager) << "Backup interval set to" << hours << "hours";
    }
}

void BackupManager::setMaxBackupCount(int count)
{
    if (count > 0 && m_maxBackupCount != count) {
        m_maxBackupCount = count;
        cleanupOldBackups();
        qCInfo(backupManager) << "Max backup count set to" << count;
    }
}

void BackupManager::setBackupDirectory(const QString& directory)
{
    if (!directory.isEmpty() && m_backupDirectory != directory) {
        m_backupDirectory = directory;
        
        // Ensure new directory exists
        QDir().mkpath(m_backupDirectory);
        
        qCInfo(backupManager) << "Backup directory changed to:" << directory;
    }
}

QString BackupManager::createBackup(BackupType type, const QString& description)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        QString error = "Database manager not available";
        qCWarning(backupManager) << error;
        emit backupFailed(error);
        return QString();
    }
    
    // Ensure backup directory exists
    QDir backupDir(m_backupDirectory);
    if (!backupDir.exists() && !backupDir.mkpath(".")) {
        QString error = QString("Failed to create backup directory: %1").arg(m_backupDirectory);
        qCWarning(backupManager) << error;
        emit backupFailed(error);
        return QString();
    }
    
    // Generate backup file name
    QString fileName = description.isEmpty() ? QDateTime::currentDateTime().toString("backup_yyyy-MM-dd_hh-mm-ss") : description;
    if (!fileName.endsWith(".db")) {
        fileName += ".db";
    }
    
    QString backupPath = backupDir.absoluteFilePath(fileName);
    QString backupId = QFileInfo(fileName).baseName();
    
    // Create the backup
    if (m_dbManager && m_dbManager->createBackup(backupPath)) {
        // updateLastBackupInfo(backupPath);
        cleanupOldBackups();
        
        qCInfo(backupManager) << "Backup created successfully:" << backupPath;
        emit backupCreated(backupPath);
        return backupId;
    } else {
        QString error = QString("Failed to create backup");
        qCWarning(backupManager) << error;
        emit backupFailed(error);
        return QString();
    }
}

bool BackupManager::restoreBackup(const QString& backupId)
{
    if (!m_dbManager || !m_dbManager->isInitialized()) {
        QString error = "Database manager not available";
        qCWarning(backupManager) << error;
        emit backupFailed(error);
        return false;
    }
    
    QFileInfo backupInfo(backupPath);
    if (!backupInfo.exists()) {
        QString error = QString("Backup file does not exist: %1").arg(backupPath);
        qCWarning(backupManager) << error;
        emit backupFailed(error);
        return false;
    }
    
    if (m_dbManager->restoreFromBackup(backupPath)) {
        qCInfo(backupManager) << "Database restored from backup:" << backupPath;
        emit backupRestored(backupPath);
        return true;
    } else {
        QString error = QString("Failed to restore from backup: %1").arg(m_dbManager->lastError());
        qCWarning(backupManager) << error;
        emit backupFailed(error);
        return false;
    }
}

QList<BackupManager::BackupInfo> BackupManager::getAvailableBackups() const
{
    QDir backupDir(m_backupDirectory);
    if (!backupDir.exists()) {
        return QList<BackupInfo>();
    }
    
    QStringList filters;
    filters << "*.db";
    
    QFileInfoList backupFiles = backupDir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);
    
    QList<BackupInfo> backupInfos;
    for (const QFileInfo& fileInfo : backupFiles) {
        BackupInfo info;
        info.backupId = fileInfo.baseName();
        info.filePath = fileInfo.absoluteFilePath();
        info.fileSize = fileInfo.size();
        info.timestamp = fileInfo.lastModified();
        info.type = BACKUP_COMPLETE;
        backupInfos << info;
    }
    
    return backupInfos;
}

bool BackupManager::deleteBackup(const QString& backupPath)
{
    QFileInfo backupInfo(backupPath);
    if (!backupInfo.exists()) {
        return false;
    }
    
    if (QFile::remove(backupPath)) {
        qCInfo(backupManager) << "Backup deleted:" << backupPath;
        emit backupDeleted(backupPath);
        return true;
    } else {
        qCWarning(backupManager) << "Failed to delete backup:" << backupPath;
        return false;
    }
}

// qint64 BackupManager::getBackupSize(const QString& backupPath) const
{
    QFileInfo backupInfo(backupPath);
    return backupInfo.exists() ? backupInfo.size() : 0;
}

void BackupManager::cleanupOldBackups()
{
    QList<BackupInfo> backups = getAvailableBackups();
    
    // Remove excess backups (keep only the most recent ones)
    while (backups.size() > m_maxBackupCount) {
        BackupInfo oldestBackup = backups.takeLast();
        deleteBackup(oldestBackup.backupId);
    }
}

qint64 BackupManager::getTotalBackupSize() const
{
    QList<BackupInfo> backups = getAvailableBackups();
    qint64 totalSize = 0;
    
    for (const BackupInfo& backup : backups) {
        totalSize += backup.fileSize;
    }
    
    return totalSize;
}

// void BackupManager::performAutoBackup()
{
    if (m_autoBackupEnabled) {
        qCInfo(backupManager) << "Performing automatic backup";
        createBackup();
    }
}

// void BackupManager::setupAutoBackupTimer()
{
    if (m_autoBackupTimer) {
        m_autoBackupTimer->stop();
        
        if (m_autoBackupEnabled && m_backupIntervalHours > 0) {
            int intervalMs = m_backupIntervalHours * 60 * 60 * 1000; // Convert hours to milliseconds
            m_autoBackupTimer->setInterval(intervalMs);
            m_autoBackupTimer->setSingleShot(false);
            
            if (m_autoBackupEnabled) {
                m_autoBackupTimer->start();
            }
        }
    }
}

// QString BackupManager::generateBackupFileName() const
{
    QDateTime now = QDateTime::currentDateTime();
    return QString("eonplay_backup_%1.db").arg(now.toString("yyyyMMdd_hhmmss"));
}

// void BackupManager::updateLastBackupInfo(const QString& backupPath)
{
    m_lastBackupTime = QDateTime::currentDateTime();
    m_lastBackupPath = backupPath;
}
#pragma once

#include "data/MediaFile.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QHash>
#include <QSet>

namespace EonPlay {
namespace Data {

class DatabaseManager;

/**
 * @brief Scans file system for media files and manages library updates
 * 
 * Provides recursive directory scanning, file monitoring, and automatic
 * library updates for the EonPlay media player.
 */
class MediaScanner : public QObject
{
    Q_OBJECT

public:
    enum ScanMode {
        FullScan,           // Scan all files in directory
        IncrementalScan,    // Only scan new/modified files
        QuickScan          // Fast scan without metadata extraction
    };

    struct ScanOptions {
        ScanMode mode = FullScan;
        bool recursive = true;
        bool followSymlinks = false;
        bool extractMetadata = true;
        bool detectDuplicates = true;
        bool autoAddToLibrary = true;
        int maxDepth = -1;  // -1 for unlimited depth
        QStringList excludePatterns;
        QStringList includePatterns;
    };

    struct ScanProgress {
        int totalDirectories = 0;
        int scannedDirectories = 0;
        int totalFiles = 0;
        int scannedFiles = 0;
        int addedFiles = 0;
        int updatedFiles = 0;
        int skippedFiles = 0;
        int errorFiles = 0;
        QString currentDirectory;
        QString currentFile;
        double percentage = 0.0;
        bool isRunning = false;
    };

    explicit MediaScanner(DatabaseManager* dbManager, QObject* parent = nullptr);
    ~MediaScanner();

    // Configuration
    void setScanOptions(const ScanOptions& options) { m_options = options; }
    ScanOptions scanOptions() const { return m_options; }

    void setWatchDirectories(const QStringList& directories);
    QStringList watchDirectories() const { return m_watchDirectories; }

    // Scanning operations
    void scanDirectory(const QString& directoryPath);
    void scanDirectories(const QStringList& directoryPaths);
    void scanFile(const QString& filePath);
    void rescanLibrary();

    // File monitoring
    void enableFileWatching(bool enabled);
    bool isFileWatchingEnabled() const { return m_fileWatchingEnabled; }

    // Progress and status
    bool isScanning() const { return m_isScanning; }
    ScanProgress progress() const { return m_progress; }
    void cancelScan();

    // Duplicate detection
    QList<QStringList> findDuplicateFiles();
    void removeDuplicates(const QStringList& filesToRemove);

    // Library maintenance
    void cleanupMissingFiles();
    void updateFileMetadata(const QString& filePath);
    void refreshLibrary();

    // Statistics
    int getTotalMediaFiles() const;
    qint64 getTotalLibrarySize() const;
    QStringList getLibraryDirectories() const;

signals:
    void scanStarted(const QString& directory);
    void scanProgress(const ScanProgress& progress);
    void scanCompleted(int addedFiles, int updatedFiles, int errorFiles);
    void scanCancelled();
    void scanError(const QString& error);

    void fileAdded(const QString& filePath);
    void fileUpdated(const QString& filePath);
    void fileRemoved(const QString& filePath);
    void fileError(const QString& filePath, const QString& error);

    void duplicatesFound(const QList<QStringList>& duplicateGroups);
    void libraryUpdated();

private slots:
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);
    void processWatcherQueue();

private:
    // Scanning implementation
    void scanDirectoryRecursive(const QString& directoryPath, int currentDepth = 0);
    void processFile(const QString& filePath);
    bool shouldProcessFile(const QString& filePath) const;
    bool shouldProcessDirectory(const QString& directoryPath) const;

    // File processing
    MediaFile createMediaFileFromPath(const QString& filePath);
    bool addOrUpdateMediaFile(const MediaFile& mediaFile);
    void updateScanProgress();

    // Duplicate detection helpers
    QString calculateFileHash(const QString& filePath);
    QHash<QString, QStringList> groupFilesByHash();
    QHash<QString, QStringList> groupFilesByMetadata();

    // File watching helpers
    void setupFileWatcher();
    void addDirectoryToWatcher(const QString& directoryPath);
    void removeDirectoryFromWatcher(const QString& directoryPath);

    // Utility methods
    bool isMediaFile(const QString& filePath) const;
    bool matchesPattern(const QString& filePath, const QStringList& patterns) const;
    void logScanResult(const QString& operation, const QString& filePath, bool success);

    // Member variables
    DatabaseManager* m_dbManager;
    ScanOptions m_options;
    ScanProgress m_progress;
    bool m_isScanning;
    bool m_cancelRequested;

    // File watching
    QFileSystemWatcher* m_fileWatcher;
    bool m_fileWatchingEnabled;
    QStringList m_watchDirectories;
    QTimer* m_watcherTimer;
    QStringList m_watcherQueue;
    mutable QMutex m_watcherMutex;

    // Duplicate detection cache
    QHash<QString, QString> m_fileHashCache;
    QHash<QString, QDateTime> m_fileModificationCache;

    // Threading
    QThread* m_scanThread;
    mutable QMutex m_progressMutex;

    // Statistics cache
    mutable int m_cachedFileCount;
    mutable qint64 m_cachedLibrarySize;
    mutable QDateTime m_cacheUpdateTime;
};

} // namespace Data
} // namespace EonPlay
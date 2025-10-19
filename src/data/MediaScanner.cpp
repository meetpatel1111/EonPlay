#include "data/MediaScanner.h"
#include "data/DatabaseManager.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QDebug>
#include <QThread>
#include <QMutexLocker>
#include <QApplication>
#include <algorithm>

Q_LOGGING_CATEGORY(mediaScanner, "eonplay.data.scanner")

namespace EonPlay {
namespace Data {

MediaScanner::MediaScanner(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent)
    , m_dbManager(dbManager)
    , m_isScanning(false)
    , m_cancelRequested(false)
    , m_fileWatcher(nullptr)
    , m_fileWatchingEnabled(false)
    , m_watcherTimer(new QTimer(this))
    , m_scanThread(nullptr)
    , m_cachedFileCount(-1)
    , m_cachedLibrarySize(-1)
{
    // Setup file watcher timer
    m_watcherTimer->setSingleShot(true);
    m_watcherTimer->setInterval(1000); // 1 second delay to batch file changes
    connect(m_watcherTimer, &QTimer::timeout, this, &MediaScanner::processWatcherQueue);
    
    setupFileWatcher();
    
    qCInfo(mediaScanner) << "MediaScanner initialized";
}

MediaScanner::~MediaScanner()
{
    cancelScan();
    
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->quit();
        m_scanThread->wait(3000);
    }
    
    if (m_fileWatcher) {
        delete m_fileWatcher;
    }
}

void MediaScanner::setWatchDirectories(const QStringList& directories)
{
    if (m_watchDirectories == directories) {
        return;
    }
    
    // Remove old directories from watcher
    for (const QString& dir : m_watchDirectories) {
        removeDirectoryFromWatcher(dir);
    }
    
    m_watchDirectories = directories;
    
    // Add new directories to watcher
    if (m_fileWatchingEnabled) {
        for (const QString& dir : m_watchDirectories) {
            addDirectoryToWatcher(dir);
        }
    }
    
    qCInfo(mediaScanner) << "Watch directories updated:" << directories.size() << "directories";
}

void MediaScanner::scanDirectory(const QString& directoryPath)
{
    if (m_isScanning) {
        qCWarning(mediaScanner) << "Scan already in progress";
        return;
    }
    
    QDir dir(directoryPath);
    if (!dir.exists()) {
        emit scanError("Directory does not exist: " + directoryPath);
        return;
    }
    
    m_isScanning = true;
    m_cancelRequested = false;
    
    // Reset progress
    m_progress = ScanProgress();
    m_progress.isRunning = true;
    m_progress.currentDirectory = directoryPath;
    
    emit scanStarted(directoryPath);
    
    qCInfo(mediaScanner) << "Starting scan of directory:" << directoryPath;
    
    // Count total directories and files for progress tracking
    if (m_options.mode == FullScan) {
        QDirIterator countIterator(directoryPath, 
                                  QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                  m_options.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        
        while (countIterator.hasNext()) {
            countIterator.next();
            QFileInfo info = countIterator.fileInfo();
            
            if (info.isDir()) {
                m_progress.totalDirectories++;
            } else if (isMediaFile(info.absoluteFilePath())) {
                m_progress.totalFiles++;
            }
            
            // Allow UI updates during counting
            if (m_progress.totalFiles % 100 == 0) {
                QApplication::processEvents();
            }
            
            if (m_cancelRequested) {
                m_isScanning = false;
                emit scanCancelled();
                return;
            }
        }
    }
    
    // Start actual scanning
    scanDirectoryRecursive(directoryPath);
    
    // Complete scan
    m_isScanning = false;
    m_progress.isRunning = false;
    m_progress.percentage = 100.0;
    
    // Invalidate cache
    m_cachedFileCount = -1;
    m_cachedLibrarySize = -1;
    
    emit scanCompleted(m_progress.addedFiles, m_progress.updatedFiles, m_progress.errorFiles);
    emit libraryUpdated();
    
    qCInfo(mediaScanner) << "Scan completed. Added:" << m_progress.addedFiles 
                         << "Updated:" << m_progress.updatedFiles 
                         << "Errors:" << m_progress.errorFiles;
}

void MediaScanner::scanDirectories(const QStringList& directoryPaths)
{
    for (const QString& directoryPath : directoryPaths) {
        if (m_cancelRequested) {
            break;
        }
        scanDirectory(directoryPath);
    }
}

void MediaScanner::scanFile(const QString& filePath)
{
    if (!QFile::exists(filePath)) {
        emit fileError(filePath, "File does not exist");
        return;
    }
    
    if (!isMediaFile(filePath)) {
        emit fileError(filePath, "Not a supported media file");
        return;
    }
    
    processFile(filePath);
}

void MediaScanner::rescanLibrary()
{
    if (!m_dbManager) {
        emit scanError("Database manager not available");
        return;
    }
    
    qCInfo(mediaScanner) << "Starting library rescan";
    
    // Get all media files from database
    QSqlQuery query = m_dbManager->getAllMediaFiles();
    QStringList filesToRescan;
    
    while (query.next()) {
        QString filePath = query.value("file_path").toString();
        filesToRescan << filePath;
    }
    
    // Rescan each file
    m_progress = ScanProgress();
    m_progress.totalFiles = filesToRescan.size();
    m_progress.isRunning = true;
    
    emit scanStarted("Library Rescan");
    
    for (const QString& filePath : filesToRescan) {
        if (m_cancelRequested) {
            break;
        }
        
        if (QFile::exists(filePath)) {
            processFile(filePath);
        } else {
            // File no longer exists, remove from database
            m_dbManager->removeMediaFileByPath(filePath);
            emit fileRemoved(filePath);
        }
        
        updateScanProgress();
    }
    
    m_progress.isRunning = false;
    emit scanCompleted(m_progress.addedFiles, m_progress.updatedFiles, m_progress.errorFiles);
    emit libraryUpdated();
}

void MediaScanner::enableFileWatching(bool enabled)
{
    if (m_fileWatchingEnabled == enabled) {
        return;
    }
    
    m_fileWatchingEnabled = enabled;
    
    if (enabled) {
        for (const QString& dir : m_watchDirectories) {
            addDirectoryToWatcher(dir);
        }
        qCInfo(mediaScanner) << "File watching enabled";
    } else {
        if (m_fileWatcher) {
            m_fileWatcher->removePaths(m_fileWatcher->directories());
            m_fileWatcher->removePaths(m_fileWatcher->files());
        }
        qCInfo(mediaScanner) << "File watching disabled";
    }
}

void MediaScanner::cancelScan()
{
    if (m_isScanning) {
        m_cancelRequested = true;
        qCInfo(mediaScanner) << "Scan cancellation requested";
    }
}

QList<QStringList> MediaScanner::findDuplicateFiles()
{
    QList<QStringList> duplicateGroups;
    
    if (!m_dbManager) {
        return duplicateGroups;
    }
    
    qCInfo(mediaScanner) << "Starting duplicate detection";
    
    // Group files by hash
    QHash<QString, QStringList> hashGroups = groupFilesByHash();
    
    // Find groups with more than one file
    for (auto it = hashGroups.begin(); it != hashGroups.end(); ++it) {
        if (it.value().size() > 1) {
            duplicateGroups.append(it.value());
        }
    }
    
    // Also group by metadata for files that might be identical but have different hashes
    QHash<QString, QStringList> metadataGroups = groupFilesByMetadata();
    
    for (auto it = metadataGroups.begin(); it != metadataGroups.end(); ++it) {
        if (it.value().size() > 1) {
            // Check if this group is not already in hash-based duplicates
            bool alreadyFound = false;
            for (const QStringList& existingGroup : duplicateGroups) {
                if (existingGroup.contains(it.value().first())) {
                    alreadyFound = true;
                    break;
                }
            }
            
            if (!alreadyFound) {
                duplicateGroups.append(it.value());
            }
        }
    }
    
    emit duplicatesFound(duplicateGroups);
    
    qCInfo(mediaScanner) << "Found" << duplicateGroups.size() << "duplicate groups";
    return duplicateGroups;
}

void MediaScanner::removeDuplicates(const QStringList& filesToRemove)
{
    for (const QString& filePath : filesToRemove) {
        if (m_dbManager) {
            m_dbManager->removeMediaFileByPath(filePath);
        }
        emit fileRemoved(filePath);
    }
    
    // Invalidate cache
    m_cachedFileCount = -1;
    m_cachedLibrarySize = -1;
    
    emit libraryUpdated();
    qCInfo(mediaScanner) << "Removed" << filesToRemove.size() << "duplicate files";
}

void MediaScanner::cleanupMissingFiles()
{
    if (!m_dbManager) {
        return;
    }
    
    qCInfo(mediaScanner) << "Starting cleanup of missing files";
    
    QSqlQuery query = m_dbManager->getAllMediaFiles();
    QStringList missingFiles;
    
    while (query.next()) {
        QString filePath = query.value("file_path").toString();
        if (!QFile::exists(filePath)) {
            missingFiles << filePath;
        }
    }
    
    // Remove missing files from database
    for (const QString& filePath : missingFiles) {
        m_dbManager->removeMediaFileByPath(filePath);
        emit fileRemoved(filePath);
    }
    
    // Invalidate cache
    m_cachedFileCount = -1;
    m_cachedLibrarySize = -1;
    
    emit libraryUpdated();
    qCInfo(mediaScanner) << "Cleaned up" << missingFiles.size() << "missing files";
}

void MediaScanner::updateFileMetadata(const QString& filePath)
{
    if (QFile::exists(filePath) && isMediaFile(filePath)) {
        processFile(filePath);
    }
}

void MediaScanner::refreshLibrary()
{
    // Invalidate all caches
    m_cachedFileCount = -1;
    m_cachedLibrarySize = -1;
    m_fileHashCache.clear();
    m_fileModificationCache.clear();
    
    emit libraryUpdated();
    qCInfo(mediaScanner) << "Library refreshed";
}

int MediaScanner::getTotalMediaFiles() const
{
    if (m_cachedFileCount == -1 || m_cacheUpdateTime.secsTo(QDateTime::currentDateTime()) > 300) {
        if (m_dbManager) {
            m_cachedFileCount = m_dbManager->getMediaFileCount();
            m_cacheUpdateTime = QDateTime::currentDateTime();
        }
    }
    
    return m_cachedFileCount;
}

qint64 MediaScanner::getTotalLibrarySize() const
{
    if (m_cachedLibrarySize == -1 || m_cacheUpdateTime.secsTo(QDateTime::currentDateTime()) > 300) {
        if (m_dbManager) {
            m_cachedLibrarySize = m_dbManager->getTotalLibrarySize();
            m_cacheUpdateTime = QDateTime::currentDateTime();
        }
    }
    
    return m_cachedLibrarySize;
}

QStringList MediaScanner::getLibraryDirectories() const
{
    QSet<QString> directories;
    
    if (m_dbManager) {
        QSqlQuery query = m_dbManager->getAllMediaFiles();
        while (query.next()) {
            QString filePath = query.value("file_path").toString();
            QFileInfo fileInfo(filePath);
            directories.insert(fileInfo.absolutePath());
        }
    }
    
    return directories.values();
}

void MediaScanner::scanDirectoryRecursive(const QString& directoryPath, int currentDepth)
{
    if (m_cancelRequested) {
        return;
    }
    
    if (m_options.maxDepth >= 0 && currentDepth > m_options.maxDepth) {
        return;
    }
    
    if (!shouldProcessDirectory(directoryPath)) {
        return;
    }
    
    QDir dir(directoryPath);
    if (!dir.exists()) {
        return;
    }
    
    m_progress.currentDirectory = directoryPath;
    m_progress.scannedDirectories++;
    updateScanProgress();
    
    qCDebug(mediaScanner) << "Scanning directory:" << directoryPath;
    
    // Get directory entries
    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;
    if (!m_options.followSymlinks) {
        filters |= QDir::NoSymLinks;
    }
    
    QFileInfoList entries = dir.entryInfoList(filters);
    
    for (const QFileInfo& entry : entries) {
        if (m_cancelRequested) {
            return;
        }
        
        QString entryPath = entry.absoluteFilePath();
        
        if (entry.isDir()) {
            if (m_options.recursive) {
                scanDirectoryRecursive(entryPath, currentDepth + 1);
            }
        } else if (entry.isFile()) {
            if (shouldProcessFile(entryPath)) {
                processFile(entryPath);
                updateScanProgress();
            } else {
                m_progress.skippedFiles++;
            }
        }
        
        // Allow UI updates
        if ((m_progress.scannedFiles + m_progress.skippedFiles) % 10 == 0) {
            QApplication::processEvents();
        }
    }
}

void MediaScanner::processFile(const QString& filePath)
{
    if (m_cancelRequested) {
        return;
    }
    
    m_progress.currentFile = filePath;
    m_progress.scannedFiles++;
    
    try {
        MediaFile mediaFile = createMediaFileFromPath(filePath);
        
        if (mediaFile.isValid()) {
            if (addOrUpdateMediaFile(mediaFile)) {
                if (mediaFile.id() == -1) {
                    m_progress.addedFiles++;
                    emit fileAdded(filePath);
                } else {
                    m_progress.updatedFiles++;
                    emit fileUpdated(filePath);
                }
            } else {
                m_progress.errorFiles++;
                emit fileError(filePath, "Failed to add/update file in database");
            }
        } else {
            m_progress.errorFiles++;
            emit fileError(filePath, "Invalid media file");
        }
    } catch (const std::exception& e) {
        m_progress.errorFiles++;
        emit fileError(filePath, QString("Exception: %1").arg(e.what()));
        qCWarning(mediaScanner) << "Error processing file" << filePath << ":" << e.what();
    }
    
    logScanResult("processFile", filePath, true);
}

bool MediaScanner::shouldProcessFile(const QString& filePath) const
{
    // Check if it's a media file
    if (!isMediaFile(filePath)) {
        return false;
    }
    
    // Check include patterns
    if (!m_options.includePatterns.isEmpty()) {
        if (!matchesPattern(filePath, m_options.includePatterns)) {
            return false;
        }
    }
    
    // Check exclude patterns
    if (!m_options.excludePatterns.isEmpty()) {
        if (matchesPattern(filePath, m_options.excludePatterns)) {
            return false;
        }
    }
    
    // For incremental scan, check if file has been modified
    if (m_options.mode == IncrementalScan) {
        QFileInfo fileInfo(filePath);
        QDateTime lastModified = fileInfo.lastModified();
        
        if (m_fileModificationCache.contains(filePath)) {
            QDateTime cachedModified = m_fileModificationCache[filePath];
            if (lastModified <= cachedModified) {
                return false; // File hasn't been modified
            }
        }
        
        m_fileModificationCache[filePath] = lastModified;
    }
    
    return true;
}

bool MediaScanner::shouldProcessDirectory(const QString& directoryPath) const
{
    // Check exclude patterns for directories
    if (!m_options.excludePatterns.isEmpty()) {
        if (matchesPattern(directoryPath, m_options.excludePatterns)) {
            return false;
        }
    }
    
    // Skip hidden directories unless explicitly included
    QFileInfo dirInfo(directoryPath);
    if (dirInfo.fileName().startsWith('.')) {
        return false;
    }
    
    return true;
}

MediaFile MediaScanner::createMediaFileFromPath(const QString& filePath)
{
    MediaFile mediaFile(filePath);
    
    QFileInfo fileInfo(filePath);
    mediaFile.setFileSize(fileInfo.size());
    mediaFile.setDateAdded(QDateTime::currentDateTime());
    
    // Set basic metadata from filename if not already set
    if (mediaFile.title().isEmpty()) {
        mediaFile.setTitle(fileInfo.completeBaseName());
    }
    
    // TODO: Extract metadata using MetadataExtractor when implemented
    // For now, we'll use basic file information
    
    return mediaFile;
}

bool MediaScanner::addOrUpdateMediaFile(const MediaFile& mediaFile)
{
    if (!m_dbManager) {
        return false;
    }
    
    // Check if file already exists in database
    QSqlQuery existingQuery = m_dbManager->getMediaFileByPath(mediaFile.filePath());
    
    if (existingQuery.next()) {
        // File exists, update it
        int id = existingQuery.value("id").toInt();
        return m_dbManager->updateMediaFile(id, mediaFile.title(), mediaFile.artist(), 
                                          mediaFile.album(), mediaFile.duration());
    } else {
        // New file, add it
        return m_dbManager->addMediaFile(mediaFile.filePath(), mediaFile.title(), 
                                       mediaFile.artist(), mediaFile.album(), 
                                       mediaFile.duration(), mediaFile.fileSize());
    }
}

void MediaScanner::updateScanProgress()
{
    QMutexLocker locker(&m_progressMutex);
    
    if (m_progress.totalFiles > 0) {
        m_progress.percentage = (double)(m_progress.scannedFiles + m_progress.skippedFiles) / 
                               m_progress.totalFiles * 100.0;
    }
    
    emit scanProgress(m_progress);
}

QString MediaScanner::calculateFileHash(const QString& filePath)
{
    // Check cache first
    if (m_fileHashCache.contains(filePath)) {
        QFileInfo fileInfo(filePath);
        QDateTime lastModified = fileInfo.lastModified();
        
        if (m_fileModificationCache.contains(filePath) && 
            m_fileModificationCache[filePath] == lastModified) {
            return m_fileHashCache[filePath];
        }
    }
    
    // Calculate hash
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    
    // For large files, only hash the first and last parts
    qint64 fileSize = file.size();
    if (fileSize > 10 * 1024 * 1024) { // 10MB
        // Hash first 1MB
        QByteArray data = file.read(1024 * 1024);
        hash.addData(data);
        
        // Hash last 1MB
        file.seek(fileSize - 1024 * 1024);
        data = file.read(1024 * 1024);
        hash.addData(data);
    } else {
        // Hash entire file for smaller files
        hash.addData(&file);
    }
    
    QString hashString = hash.result().toHex();
    
    // Cache the result
    QFileInfo fileInfo(filePath);
    m_fileHashCache[filePath] = hashString;
    m_fileModificationCache[filePath] = fileInfo.lastModified();
    
    return hashString;
}

QHash<QString, QStringList> MediaScanner::groupFilesByHash()
{
    QHash<QString, QStringList> hashGroups;
    
    if (!m_dbManager) {
        return hashGroups;
    }
    
    QSqlQuery query = m_dbManager->getAllMediaFiles();
    while (query.next()) {
        QString filePath = query.value("file_path").toString();
        
        if (QFile::exists(filePath)) {
            QString hash = calculateFileHash(filePath);
            if (!hash.isEmpty()) {
                hashGroups[hash].append(filePath);
            }
        }
    }
    
    return hashGroups;
}

QHash<QString, QStringList> MediaScanner::groupFilesByMetadata()
{
    QHash<QString, QStringList> metadataGroups;
    
    if (!m_dbManager) {
        return metadataGroups;
    }
    
    QSqlQuery query = m_dbManager->getAllMediaFiles();
    while (query.next()) {
        QString filePath = query.value("file_path").toString();
        QString title = query.value("title").toString();
        QString artist = query.value("artist").toString();
        qint64 duration = query.value("duration").toLongLong();
        
        // Create metadata key
        QString metadataKey = QString("%1|%2|%3").arg(title, artist).arg(duration);
        
        if (!title.isEmpty() && duration > 0) {
            metadataGroups[metadataKey].append(filePath);
        }
    }
    
    return metadataGroups;
}

void MediaScanner::setupFileWatcher()
{
    if (!m_fileWatcher) {
        m_fileWatcher = new QFileSystemWatcher(this);
        connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
                this, &MediaScanner::onDirectoryChanged);
        connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
                this, &MediaScanner::onFileChanged);
    }
}

void MediaScanner::addDirectoryToWatcher(const QString& directoryPath)
{
    if (m_fileWatcher && QDir(directoryPath).exists()) {
        if (!m_fileWatcher->directories().contains(directoryPath)) {
            m_fileWatcher->addPath(directoryPath);
            qCDebug(mediaScanner) << "Added directory to watcher:" << directoryPath;
        }
    }
}

void MediaScanner::removeDirectoryFromWatcher(const QString& directoryPath)
{
    if (m_fileWatcher && m_fileWatcher->directories().contains(directoryPath)) {
        m_fileWatcher->removePath(directoryPath);
        qCDebug(mediaScanner) << "Removed directory from watcher:" << directoryPath;
    }
}

void MediaScanner::onDirectoryChanged(const QString& path)
{
    QMutexLocker locker(&m_watcherMutex);
    
    if (!m_watcherQueue.contains(path)) {
        m_watcherQueue.append(path);
    }
    
    m_watcherTimer->start();
}

void MediaScanner::onFileChanged(const QString& path)
{
    QMutexLocker locker(&m_watcherMutex);
    
    if (!m_watcherQueue.contains(path)) {
        m_watcherQueue.append(path);
    }
    
    m_watcherTimer->start();
}

void MediaScanner::processWatcherQueue()
{
    QMutexLocker locker(&m_watcherMutex);
    
    QStringList queue = m_watcherQueue;
    m_watcherQueue.clear();
    locker.unlock();
    
    for (const QString& path : queue) {
        QFileInfo info(path);
        
        if (info.isDir()) {
            // Directory changed, scan for new files
            QDir dir(path);
            QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
            
            for (const QFileInfo& entry : entries) {
                if (isMediaFile(entry.absoluteFilePath())) {
                    processFile(entry.absoluteFilePath());
                }
            }
        } else if (info.isFile()) {
            // File changed
            if (info.exists() && isMediaFile(path)) {
                processFile(path);
            } else if (!info.exists()) {
                // File was deleted
                if (m_dbManager) {
                    m_dbManager->removeMediaFileByPath(path);
                }
                emit fileRemoved(path);
            }
        }
    }
    
    if (!queue.isEmpty()) {
        emit libraryUpdated();
    }
}

bool MediaScanner::isMediaFile(const QString& filePath) const
{
    return MediaFile::isSupportedFile(filePath);
}

bool MediaScanner::matchesPattern(const QString& filePath, const QStringList& patterns) const
{
    for (const QString& pattern : patterns) {
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern));
        if (regex.match(filePath).hasMatch()) {
            return true;
        }
    }
    return false;
}

void MediaScanner::logScanResult(const QString& operation, const QString& filePath, bool success)
{
    if (success) {
        qCDebug(mediaScanner) << operation << "succeeded for:" << filePath;
    } else {
        qCWarning(mediaScanner) << operation << "failed for:" << filePath;
    }
}

} // namespace Data
} // namespace EonPlay
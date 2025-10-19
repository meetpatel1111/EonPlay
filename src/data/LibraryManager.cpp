#include "data/LibraryManager.h"
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QLoggingCategory>
#include <QDebug>

Q_LOGGING_CATEGORY(libraryManager, "eonplay.data.library")

namespace EonPlay {
namespace Data {

LibraryManager::LibraryManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_autoScanEnabled(false)
    , m_autoScanTimer(new QTimer(this))
    , m_statisticsValid(false)
{
    // Setup auto-scan timer
    m_autoScanTimer->setSingleShot(false);
    connect(m_autoScanTimer, &QTimer::timeout, this, &LibraryManager::performAutoScan);
    
    qCInfo(libraryManager) << "LibraryManager created";
}

bool LibraryManager::initialize(const QString& databasePath)
{
    if (m_initialized) {
        qCWarning(libraryManager) << "Library already initialized";
        return true;
    }
    
    m_currentOperation = "Initializing library";
    emit operationStarted(m_currentOperation);
    
    // Setup components
    setupComponents();
    
    // Initialize database
    if (!m_dbManager->initialize(databasePath)) {
        emit operationFailed(m_currentOperation, "Failed to initialize database");
        return false;
    }
    
    // Connect signals
    connectSignals();
    
    // Load settings
    loadSettings();
    
    // Setup scanner with library paths
    if (m_scanner) {
        m_scanner->setWatchDirectories(m_settings.libraryPaths);
        m_scanner->setScanOptions(m_settings.scanOptions);
        m_scanner->enableFileWatching(true);
    }
    
    // Setup metadata extractor
    if (m_extractor) {
        m_extractor->setExtractionOptions(m_settings.extractionOptions);
    }
    
    // Enable auto-scan if configured
    if (m_settings.autoScanEnabled) {
        enableAutoScan(true);
    }
    
    m_initialized = true;
    emit operationCompleted(m_currentOperation);
    emit libraryInitialized();
    
    qCInfo(libraryManager) << "LibraryManager initialized with database:" << databasePath;
    return true;
}

void LibraryManager::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    m_currentOperation = "Shutting down library";
    emit operationStarted(m_currentOperation);
    
    // Stop auto-scan
    enableAutoScan(false);
    
    // Save settings
    saveSettings();
    
    // Shutdown components
    if (m_scanner) {
        m_scanner->cancelScan();
        m_scanner->enableFileWatching(false);
    }
    
    if (m_dbManager) {
        m_dbManager->shutdown();
    }
    
    m_initialized = false;
    emit operationCompleted(m_currentOperation);
    emit libraryShutdown();
    
    qCInfo(libraryManager) << "LibraryManager shutdown complete";
}

void LibraryManager::setLibrarySettings(const LibrarySettings& settings)
{
    m_settings = settings;
    
    // Update components with new settings
    if (m_scanner) {
        m_scanner->setScanOptions(m_settings.scanOptions);
        m_scanner->setWatchDirectories(m_settings.libraryPaths);
    }
    
    if (m_extractor) {
        m_extractor->setExtractionOptions(m_settings.extractionOptions);
    }
    
    // Update auto-scan
    if (m_settings.autoScanEnabled != m_autoScanEnabled) {
        enableAutoScan(m_settings.autoScanEnabled);
    }
    
    if (m_settings.autoScanInterval != m_autoScanTimer->interval() / 1000) {
        setAutoScanInterval(m_settings.autoScanInterval);
    }
    
    saveSettings();
    qCInfo(libraryManager) << "Library settings updated";
}

void LibraryManager::addLibraryPath(const QString& path)
{
    if (!m_settings.libraryPaths.contains(path)) {
        m_settings.libraryPaths.append(path);
        
        if (m_scanner) {
            QStringList watchDirs = m_scanner->watchDirectories();
            watchDirs.append(path);
            m_scanner->setWatchDirectories(watchDirs);
        }
        
        saveSettings();
        emit libraryChanged();
        
        qCInfo(libraryManager) << "Added library path:" << path;
    }
}

void LibraryManager::removeLibraryPath(const QString& path)
{
    if (m_settings.libraryPaths.removeOne(path)) {
        if (m_scanner) {
            QStringList watchDirs = m_scanner->watchDirectories();
            watchDirs.removeOne(path);
            m_scanner->setWatchDirectories(watchDirs);
        }
        
        saveSettings();
        emit libraryChanged();
        
        qCInfo(libraryManager) << "Removed library path:" << path;
    }
}

void LibraryManager::scanLibrary()
{
    if (!m_initialized || !m_scanner) {
        emit scanFailed("Library not initialized or scanner not available");
        return;
    }
    
    if (m_settings.libraryPaths.isEmpty()) {
        emit scanFailed("No library paths configured");
        return;
    }
    
    m_currentOperation = "Scanning library";
    qCInfo(libraryManager) << "Starting full library scan";
    
    // Set full scan mode
    MediaScanner::ScanOptions options = m_settings.scanOptions;
    options.mode = MediaScanner::FullScan;
    m_scanner->setScanOptions(options);
    
    // Start scanning all library paths
    m_scanner->scanDirectories(m_settings.libraryPaths);
}

void LibraryManager::scanPath(const QString& path)
{
    if (!m_initialized || !m_scanner) {
        emit scanFailed("Library not initialized or scanner not available");
        return;
    }
    
    m_currentOperation = QString("Scanning path: %1").arg(path);
    qCInfo(libraryManager) << "Starting scan of path:" << path;
    
    m_scanner->scanDirectory(path);
}

void LibraryManager::rescanLibrary()
{
    if (!m_initialized || !m_scanner) {
        emit scanFailed("Library not initialized or scanner not available");
        return;
    }
    
    m_currentOperation = "Rescanning library";
    qCInfo(libraryManager) << "Starting library rescan";
    
    m_scanner->rescanLibrary();
}

void LibraryManager::quickScan()
{
    if (!m_initialized || !m_scanner) {
        emit scanFailed("Library not initialized or scanner not available");
        return;
    }
    
    m_currentOperation = "Quick scanning library";
    qCInfo(libraryManager) << "Starting quick library scan";
    
    // Set quick scan mode
    MediaScanner::ScanOptions options = m_settings.scanOptions;
    options.mode = MediaScanner::QuickScan;
    options.extractMetadata = false;
    m_scanner->setScanOptions(options);
    
    // Start scanning
    m_scanner->scanDirectories(m_settings.libraryPaths);
    
    // Restore original options
    m_scanner->setScanOptions(m_settings.scanOptions);
}

void LibraryManager::addFile(const QString& filePath)
{
    if (!m_initialized || !m_scanner) {
        return;
    }
    
    qCInfo(libraryManager) << "Adding file:" << filePath;
    m_scanner->scanFile(filePath);
}

void LibraryManager::removeFile(const QString& filePath)
{
    if (!m_initialized || !m_dbManager) {
        return;
    }
    
    qCInfo(libraryManager) << "Removing file:" << filePath;
    m_dbManager->removeMediaFileByPath(filePath);
    
    // Invalidate statistics
    m_statisticsValid = false;
    
    emit fileRemoved(filePath);
    emit libraryChanged();
}

void LibraryManager::updateFile(const QString& filePath)
{
    if (!m_initialized) {
        return;
    }
    
    qCInfo(libraryManager) << "Updating file:" << filePath;
    
    // Extract metadata and update
    if (m_extractor && m_settings.extractMetadata) {
        m_extractor->extractMetadata(filePath);
    } else if (m_scanner) {
        m_scanner->scanFile(filePath);
    }
}

bool LibraryManager::containsFile(const QString& filePath)
{
    if (!m_initialized || !m_dbManager) {
        return false;
    }
    
    QSqlQuery query = m_dbManager->getMediaFileByPath(filePath);
    return query.next();
}

QList<MediaFile> LibraryManager::searchFiles(const QString& query)
{
    QList<MediaFile> results;
    
    if (!m_initialized || !m_dbManager) {
        return results;
    }
    
    QSqlQuery sqlQuery = m_dbManager->searchMediaFiles(query);
    while (sqlQuery.next()) {
        MediaFile file;
        file.setId(sqlQuery.value("id").toInt());
        file.setFilePath(sqlQuery.value("file_path").toString());
        file.setTitle(sqlQuery.value("title").toString());
        file.setArtist(sqlQuery.value("artist").toString());
        file.setAlbum(sqlQuery.value("album").toString());
        file.setDuration(sqlQuery.value("duration").toLongLong());
        file.setFileSize(sqlQuery.value("file_size").toLongLong());
        file.setDateAdded(sqlQuery.value("date_added").toDateTime());
        file.setLastPlayed(sqlQuery.value("last_played").toDateTime());
        file.setPlayCount(sqlQuery.value("play_count").toInt());
        
        results.append(file);
    }
    
    return results;
}

QList<MediaFile> LibraryManager::getFilesByArtist(const QString& artist)
{
    QList<MediaFile> results;
    
    if (!m_initialized || !m_dbManager) {
        return results;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT * FROM media_files WHERE artist = ? ORDER BY album, track_number, title");
    query.addBindValue(artist);
    query.exec();
    
    while (query.next()) {
        MediaFile file;
        file.setId(query.value("id").toInt());
        file.setFilePath(query.value("file_path").toString());
        file.setTitle(query.value("title").toString());
        file.setArtist(query.value("artist").toString());
        file.setAlbum(query.value("album").toString());
        file.setDuration(query.value("duration").toLongLong());
        
        results.append(file);
    }
    
    return results;
}

QList<MediaFile> LibraryManager::getFilesByAlbum(const QString& album)
{
    QList<MediaFile> results;
    
    if (!m_initialized || !m_dbManager) {
        return results;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT * FROM media_files WHERE album = ? ORDER BY track_number, title");
    query.addBindValue(album);
    query.exec();
    
    while (query.next()) {
        MediaFile file;
        file.setId(query.value("id").toInt());
        file.setFilePath(query.value("file_path").toString());
        file.setTitle(query.value("title").toString());
        file.setArtist(query.value("artist").toString());
        file.setAlbum(query.value("album").toString());
        file.setDuration(query.value("duration").toLongLong());
        
        results.append(file);
    }
    
    return results;
}

QList<MediaFile> LibraryManager::getFilesByGenre(const QString& genre)
{
    QList<MediaFile> results;
    
    if (!m_initialized || !m_dbManager) {
        return results;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT * FROM media_files WHERE genre = ? ORDER BY artist, album, title");
    query.addBindValue(genre);
    query.exec();
    
    while (query.next()) {
        MediaFile file;
        file.setId(query.value("id").toInt());
        file.setFilePath(query.value("file_path").toString());
        file.setTitle(query.value("title").toString());
        file.setArtist(query.value("artist").toString());
        file.setAlbum(query.value("album").toString());
        file.setGenre(query.value("genre").toString());
        file.setDuration(query.value("duration").toLongLong());
        
        results.append(file);
    }
    
    return results;
}

QList<MediaFile> LibraryManager::getRecentFiles(int limit)
{
    QList<MediaFile> results;
    
    if (!m_initialized || !m_dbManager) {
        return results;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT * FROM media_files ORDER BY date_added DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    
    while (query.next()) {
        MediaFile file;
        file.setId(query.value("id").toInt());
        file.setFilePath(query.value("file_path").toString());
        file.setTitle(query.value("title").toString());
        file.setArtist(query.value("artist").toString());
        file.setAlbum(query.value("album").toString());
        file.setDuration(query.value("duration").toLongLong());
        file.setDateAdded(query.value("date_added").toDateTime());
        
        results.append(file);
    }
    
    return results;
}

QList<MediaFile> LibraryManager::getMostPlayedFiles(int limit)
{
    QList<MediaFile> results;
    
    if (!m_initialized || !m_dbManager) {
        return results;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT * FROM media_files WHERE play_count > 0 ORDER BY play_count DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    
    while (query.next()) {
        MediaFile file;
        file.setId(query.value("id").toInt());
        file.setFilePath(query.value("file_path").toString());
        file.setTitle(query.value("title").toString());
        file.setArtist(query.value("artist").toString());
        file.setAlbum(query.value("album").toString());
        file.setDuration(query.value("duration").toLongLong());
        file.setPlayCount(query.value("play_count").toInt());
        file.setLastPlayed(query.value("last_played").toDateTime());
        
        results.append(file);
    }
    
    return results;
}

void LibraryManager::cleanupLibrary()
{
    if (!m_initialized || !m_scanner) {
        emit operationFailed("Cleanup library", "Library not initialized");
        return;
    }
    
    m_currentOperation = "Cleaning up library";
    emit operationStarted(m_currentOperation);
    
    qCInfo(libraryManager) << "Starting library cleanup";
    
    m_scanner->cleanupMissingFiles();
    
    if (m_dbManager) {
        m_dbManager->cleanupOrphanedRecords();
    }
    
    // Invalidate statistics
    m_statisticsValid = false;
    
    emit operationCompleted(m_currentOperation);
    emit libraryChanged();
}

void LibraryManager::optimizeLibrary()
{
    if (!m_initialized || !m_dbManager) {
        emit operationFailed("Optimize library", "Library not initialized");
        return;
    }
    
    m_currentOperation = "Optimizing library";
    emit operationStarted(m_currentOperation);
    
    qCInfo(libraryManager) << "Starting library optimization";
    
    m_dbManager->optimizeDatabase();
    
    emit operationCompleted(m_currentOperation);
}

void LibraryManager::rebuildLibrary()
{
    if (!m_initialized) {
        emit operationFailed("Rebuild library", "Library not initialized");
        return;
    }
    
    m_currentOperation = "Rebuilding library";
    emit operationStarted(m_currentOperation);
    
    qCInfo(libraryManager) << "Starting library rebuild";
    
    // Clear existing data
    if (m_dbManager) {
        // This would require a method to clear all media files
        // For now, we'll just rescan
    }
    
    // Rescan everything
    scanLibrary();
    
    emit operationCompleted(m_currentOperation);
}

QList<QStringList> LibraryManager::findDuplicates()
{
    if (!m_initialized || !m_scanner) {
        return QList<QStringList>();
    }
    
    qCInfo(libraryManager) << "Finding duplicate files";
    return m_scanner->findDuplicateFiles();
}

void LibraryManager::removeDuplicates(const QStringList& filesToRemove)
{
    if (!m_initialized || !m_scanner) {
        return;
    }
    
    qCInfo(libraryManager) << "Removing" << filesToRemove.size() << "duplicate files";
    m_scanner->removeDuplicates(filesToRemove);
    
    // Invalidate statistics
    m_statisticsValid = false;
    
    emit libraryChanged();
}

LibraryManager::LibraryStatistics LibraryManager::getStatistics()
{
    if (!m_statisticsValid || m_statisticsCacheTime.secsTo(QDateTime::currentDateTime()) > 300) {
        updateStatistics();
    }
    
    return m_cachedStatistics;
}

QStringList LibraryManager::getArtists()
{
    QStringList artists;
    
    if (!m_initialized || !m_dbManager) {
        return artists;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT DISTINCT artist FROM media_files WHERE artist IS NOT NULL AND artist != '' ORDER BY artist");
    query.exec();
    
    while (query.next()) {
        artists << query.value("artist").toString();
    }
    
    return artists;
}

QStringList LibraryManager::getAlbums()
{
    QStringList albums;
    
    if (!m_initialized || !m_dbManager) {
        return albums;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT DISTINCT album FROM media_files WHERE album IS NOT NULL AND album != '' ORDER BY album");
    query.exec();
    
    while (query.next()) {
        albums << query.value("album").toString();
    }
    
    return albums;
}

QStringList LibraryManager::getGenres()
{
    QStringList genres;
    
    if (!m_initialized || !m_dbManager) {
        return genres;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT DISTINCT genre FROM media_files WHERE genre IS NOT NULL AND genre != '' ORDER BY genre");
    query.exec();
    
    while (query.next()) {
        genres << query.value("genre").toString();
    }
    
    return genres;
}

void LibraryManager::enableAutoScan(bool enabled)
{
    if (m_autoScanEnabled == enabled) {
        return;
    }
    
    m_autoScanEnabled = enabled;
    m_settings.autoScanEnabled = enabled;
    
    if (enabled && m_settings.autoScanInterval > 0) {
        m_autoScanTimer->start(m_settings.autoScanInterval * 1000);
        qCInfo(libraryManager) << "Auto-scan enabled with interval:" << m_settings.autoScanInterval << "seconds";
    } else {
        m_autoScanTimer->stop();
        qCInfo(libraryManager) << "Auto-scan disabled";
    }
    
    saveSettings();
}

void LibraryManager::setAutoScanInterval(int seconds)
{
    m_settings.autoScanInterval = seconds;
    
    if (m_autoScanEnabled && seconds > 0) {
        m_autoScanTimer->setInterval(seconds * 1000);
        if (!m_autoScanTimer->isActive()) {
            m_autoScanTimer->start();
        }
    }
    
    saveSettings();
    qCInfo(libraryManager) << "Auto-scan interval set to:" << seconds << "seconds";
}

bool LibraryManager::exportLibrary(const QString& filePath, const QString& format)
{
    if (!m_initialized || !m_dbManager) {
        return false;
    }
    
    qCInfo(libraryManager) << "Exporting library to:" << filePath << "format:" << format;
    
    if (format.toLower() == "json") {
        QJsonObject root;
        QJsonArray filesArray;
        
        QSqlQuery query = m_dbManager->getAllMediaFiles();
        while (query.next()) {
            QJsonObject fileObj;
            fileObj["id"] = query.value("id").toInt();
            fileObj["file_path"] = query.value("file_path").toString();
            fileObj["title"] = query.value("title").toString();
            fileObj["artist"] = query.value("artist").toString();
            fileObj["album"] = query.value("album").toString();
            fileObj["genre"] = query.value("genre").toString();
            fileObj["duration"] = query.value("duration").toLongLong();
            fileObj["file_size"] = query.value("file_size").toLongLong();
            fileObj["date_added"] = query.value("date_added").toString();
            fileObj["play_count"] = query.value("play_count").toInt();
            
            filesArray.append(fileObj);
        }
        
        root["files"] = filesArray;
        root["export_date"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["total_files"] = filesArray.size();
        
        QJsonDocument doc(root);
        
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            return true;
        }
    }
    
    return false;
}

bool LibraryManager::importLibrary(const QString& filePath)
{
    // Placeholder for library import functionality
    Q_UNUSED(filePath)
    return false;
}

bool LibraryManager::isScanning() const
{
    return m_scanner && m_scanner->isScanning();
}

bool LibraryManager::isBusy() const
{
    bool scannerBusy = m_scanner && m_scanner->isScanning();
    // Add other busy checks as needed
    return scannerBusy;
}

void LibraryManager::setupComponents()
{
    // Create database manager
    m_dbManager = std::make_unique<DatabaseManager>(this);
    
    // Create media scanner
    m_scanner = std::make_unique<MediaScanner>(m_dbManager.get(), this);
    
    // Create metadata extractor
    m_extractor = std::make_unique<MetadataExtractor>(this);
    
    qCInfo(libraryManager) << "Library components created";
}

void LibraryManager::connectSignals()
{
    // Connect scanner signals
    if (m_scanner) {
        connect(m_scanner.get(), &MediaScanner::scanStarted,
                this, &LibraryManager::onScanStarted);
        connect(m_scanner.get(), &MediaScanner::scanProgress,
                this, &LibraryManager::onScanProgress);
        connect(m_scanner.get(), &MediaScanner::scanCompleted,
                this, &LibraryManager::onScanCompleted);
        connect(m_scanner.get(), &MediaScanner::scanError,
                this, &LibraryManager::onScanError);
        connect(m_scanner.get(), &MediaScanner::fileAdded,
                this, &LibraryManager::onFileAdded);
        connect(m_scanner.get(), &MediaScanner::fileUpdated,
                this, &LibraryManager::onFileUpdated);
        connect(m_scanner.get(), &MediaScanner::fileRemoved,
                this, &LibraryManager::onFileRemoved);
        connect(m_scanner.get(), &MediaScanner::libraryUpdated,
                this, &LibraryManager::libraryChanged);
    }
    
    // Connect metadata extractor signals
    if (m_extractor) {
        connect(m_extractor.get(), &MetadataExtractor::metadataExtracted,
                this, &LibraryManager::onMetadataExtracted);
        connect(m_extractor.get(), &MetadataExtractor::metadataExtractionFailed,
                this, &LibraryManager::onMetadataExtractionFailed);
    }
    
    qCInfo(libraryManager) << "Component signals connected";
}

void LibraryManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Library");
    
    m_settings.libraryPaths = settings.value("libraryPaths").toStringList();
    m_settings.autoScanEnabled = settings.value("autoScanEnabled", true).toBool();
    m_settings.autoScanInterval = settings.value("autoScanInterval", 3600).toInt();
    m_settings.extractMetadata = settings.value("extractMetadata", true).toBool();
    m_settings.fetchWebMetadata = settings.value("fetchWebMetadata", false).toBool();
    m_settings.autoCleanup = settings.value("autoCleanup", true).toBool();
    
    settings.endGroup();
    
    qCInfo(libraryManager) << "Library settings loaded";
}

void LibraryManager::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Library");
    
    settings.setValue("libraryPaths", m_settings.libraryPaths);
    settings.setValue("autoScanEnabled", m_settings.autoScanEnabled);
    settings.setValue("autoScanInterval", m_settings.autoScanInterval);
    settings.setValue("extractMetadata", m_settings.extractMetadata);
    settings.setValue("fetchWebMetadata", m_settings.fetchWebMetadata);
    settings.setValue("autoCleanup", m_settings.autoCleanup);
    
    settings.endGroup();
    
    qCDebug(libraryManager) << "Library settings saved";
}

void LibraryManager::updateFileWithMetadata(const QString& filePath, const MetadataExtractor::MediaMetadata& metadata)
{
    if (!m_dbManager) {
        return;
    }
    
    // Get existing file from database
    QSqlQuery query = m_dbManager->getMediaFileByPath(filePath);
    if (query.next()) {
        int id = query.value("id").toInt();
        
        // Update with extracted metadata
        m_dbManager->updateMediaFile(id, metadata.title, metadata.artist, metadata.album, metadata.duration);
        
        qCDebug(libraryManager) << "Updated file metadata:" << filePath;
    }
}

MediaFile LibraryManager::createMediaFileFromMetadata(const QString& filePath, const MetadataExtractor::MediaMetadata& metadata)
{
    MediaFile file(filePath);
    
    file.setTitle(metadata.title);
    file.setArtist(metadata.artist);
    file.setAlbum(metadata.album);
    file.setGenre(metadata.genre);
    file.setYear(metadata.year);
    file.setTrackNumber(metadata.trackNumber);
    file.setDuration(metadata.duration);
    file.setCoverArtPath(metadata.coverArtPath);
    
    return file;
}

void LibraryManager::updateStatistics()
{
    if (!m_initialized || !m_dbManager) {
        return;
    }
    
    m_cachedStatistics = LibraryStatistics();
    
    // Get basic counts
    m_cachedStatistics.totalFiles = m_dbManager->getMediaFileCount();
    m_cachedStatistics.totalSize = m_dbManager->getTotalLibrarySize();
    m_cachedStatistics.totalPlaylists = m_dbManager->getPlaylistCount();
    
    // Count audio vs video files
    QSqlQuery query = m_dbManager->getAllMediaFiles();
    while (query.next()) {
        QString filePath = query.value("file_path").toString();
        MediaFile::MediaType type = MediaFile::detectMediaType(filePath);
        
        if (type == MediaFile::Audio) {
            m_cachedStatistics.audioFiles++;
        } else if (type == MediaFile::Video) {
            m_cachedStatistics.videoFiles++;
        }
    }
    
    // Get top genres (limit to 10)
    QSqlQuery genreQuery = m_dbManager->prepareQuery(
        "SELECT genre, COUNT(*) as count FROM media_files WHERE genre IS NOT NULL AND genre != '' "
        "GROUP BY genre ORDER BY count DESC LIMIT 10");
    genreQuery.exec();
    
    while (genreQuery.next()) {
        m_cachedStatistics.topGenres << genreQuery.value("genre").toString();
    }
    
    // Get top artists (limit to 10)
    QSqlQuery artistQuery = m_dbManager->prepareQuery(
        "SELECT artist, COUNT(*) as count FROM media_files WHERE artist IS NOT NULL AND artist != '' "
        "GROUP BY artist ORDER BY count DESC LIMIT 10");
    artistQuery.exec();
    
    while (artistQuery.next()) {
        m_cachedStatistics.topArtists << artistQuery.value("artist").toString();
    }
    
    m_statisticsCacheTime = QDateTime::currentDateTime();
    m_statisticsValid = true;
    
    emit statisticsUpdated();
}

void LibraryManager::cacheStatistics()
{
    updateStatistics();
}

void LibraryManager::performAutoScan()
{
    if (!m_initialized || isScanning()) {
        return;
    }
    
    qCInfo(libraryManager) << "Performing auto-scan";
    
    // Perform incremental scan
    MediaScanner::ScanOptions options = m_settings.scanOptions;
    options.mode = MediaScanner::IncrementalScan;
    m_scanner->setScanOptions(options);
    
    m_scanner->scanDirectories(m_settings.libraryPaths);
    
    // Restore original options
    m_scanner->setScanOptions(m_settings.scanOptions);
}

// Slot implementations
void LibraryManager::onScanStarted(const QString& directory)
{
    emit scanStarted(directory);
}

void LibraryManager::onScanProgress(const MediaScanner::ScanProgress& progress)
{
    emit scanProgress(static_cast<int>(progress.percentage), progress.currentFile);
}

void LibraryManager::onScanCompleted(int addedFiles, int updatedFiles, int errorFiles)
{
    Q_UNUSED(errorFiles)
    
    // Invalidate statistics
    m_statisticsValid = false;
    
    emit scanCompleted(addedFiles, updatedFiles);
    emit libraryChanged();
    
    qCInfo(libraryManager) << "Scan completed. Added:" << addedFiles << "Updated:" << updatedFiles;
}

void LibraryManager::onScanError(const QString& error)
{
    emit scanFailed(error);
}

void LibraryManager::onFileAdded(const QString& filePath)
{
    // Extract metadata if enabled
    if (m_settings.extractMetadata && m_extractor) {
        m_extractor->extractMetadata(filePath);
    }
    
    emit fileAdded(filePath);
}

void LibraryManager::onFileUpdated(const QString& filePath)
{
    // Extract metadata if enabled
    if (m_settings.extractMetadata && m_extractor) {
        m_extractor->extractMetadata(filePath);
    }
    
    emit fileUpdated(filePath);
}

void LibraryManager::onFileRemoved(const QString& filePath)
{
    emit fileRemoved(filePath);
}

void LibraryManager::onMetadataExtracted(const QString& filePath, const MetadataExtractor::MediaMetadata& metadata)
{
    updateFileWithMetadata(filePath, metadata);
    qCDebug(libraryManager) << "Metadata extracted and updated for:" << filePath;
}

void LibraryManager::onMetadataExtractionFailed(const QString& filePath, const QString& error)
{
    qCWarning(libraryManager) << "Metadata extraction failed for" << filePath << ":" << error;
}

} // namespace Data
} // namespace EonPlay
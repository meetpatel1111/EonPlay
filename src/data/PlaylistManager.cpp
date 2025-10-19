#include "data/PlaylistManager.h"
#include "data/DatabaseManager.h"
#include "data/LibraryManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QLoggingCategory>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>
#include <random>

Q_LOGGING_CATEGORY(playlistManager, "eonplay.data.playlist")

namespace EonPlay {
namespace Data {

PlaylistManager::PlaylistManager(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent)
    , m_dbManager(dbManager)
    , m_libraryManager(nullptr)
    , m_deviceId(QUuid::createUuid().toString())
    , m_maintenanceTimer(new QTimer(this))
{
    // Setup maintenance timer (run every hour)
    m_maintenanceTimer->setInterval(3600000); // 1 hour
    m_maintenanceTimer->setSingleShot(false);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &PlaylistManager::performPeriodicMaintenance);
    m_maintenanceTimer->start();
    
    // Create required database tables
    createPlaylistTable();
    createSmartPlaylistTable();
    createPlaybackHistoryTable();
    createWatchProgressTable();
    createQueueTable();
    
    // Load current queue
    loadQueueFromDatabase();
    
    qCInfo(playlistManager) << "PlaylistManager initialized with device ID:" << m_deviceId;
}

// Manual playlist operations
int PlaylistManager::createPlaylist(const QString& name, const QString& description)
{
    if (!m_dbManager || name.isEmpty()) {
        return -1;
    }
    
    if (!isValidPlaylistName(name)) {
        qCWarning(playlistManager) << "Invalid playlist name:" << name;
        return -1;
    }
    
    if (playlistExists(name)) {
        qCWarning(playlistManager) << "Playlist already exists:" << name;
        return -1;
    }
    
    int playlistId = m_dbManager->createPlaylist(name);
    if (playlistId > 0) {
        // Update description if provided
        if (!description.isEmpty()) {
            updatePlaylistDescription(playlistId, description);
        }
        
        emit playlistCreated(playlistId, name);
        qCInfo(playlistManager) << "Created playlist:" << name << "with ID:" << playlistId;
    }
    
    return playlistId;
}

bool PlaylistManager::deletePlaylist(int playlistId)
{
    if (!m_dbManager || playlistId <= 0) {
        return false;
    }
    
    // Get playlist name for signal
    Playlist playlist = getPlaylist(playlistId);
    QString playlistName = playlist.name();
    
    if (m_dbManager->removePlaylist(playlistId)) {
        // Remove from smart playlist cache
        m_smartPlaylistCriteria.remove(playlistId);
        m_smartPlaylistLastRefresh.remove(playlistId);
        
        emit playlistDeleted(playlistId);
        qCInfo(playlistManager) << "Deleted playlist:" << playlistName << "ID:" << playlistId;
        return true;
    }
    
    return false;
}

bool PlaylistManager::renamePlaylist(int playlistId, const QString& newName)
{
    if (!m_dbManager || playlistId <= 0 || newName.isEmpty()) {
        return false;
    }
    
    if (!isValidPlaylistName(newName)) {
        return false;
    }
    
    if (playlistExists(newName)) {
        return false;
    }
    
    if (m_dbManager->updatePlaylist(playlistId, newName)) {
        emit playlistRenamed(playlistId, newName);
        qCInfo(playlistManager) << "Renamed playlist ID:" << playlistId << "to:" << newName;
        return true;
    }
    
    return false;
}bool 
PlaylistManager::updatePlaylistDescription(int playlistId, const QString& description)
{
    if (!m_dbManager || playlistId <= 0) {
        return false;
    }
    
    // This would require extending DatabaseManager to support description updates
    // For now, we'll just emit the update signal
    emit playlistUpdated(playlistId);
    return true;
}

QList<Playlist> PlaylistManager::getAllPlaylists()
{
    QList<Playlist> playlists;
    
    if (!m_dbManager) {
        return playlists;
    }
    
    QSqlQuery query = m_dbManager->getAllPlaylists();
    while (query.next()) {
        Playlist playlist;
        playlist.setId(query.value("id").toInt());
        playlist.setName(query.value("name").toString());
        playlist.setCreatedDate(query.value("created_date").toDateTime());
        playlist.setModifiedDate(query.value("modified_date").toDateTime());
        
        playlists.append(playlist);
    }
    
    return playlists;
}

Playlist PlaylistManager::getPlaylist(int playlistId)
{
    Playlist playlist;
    
    if (!m_dbManager || playlistId <= 0) {
        return playlist;
    }
    
    if (playlist.load(playlistId, m_dbManager)) {
        return playlist;
    }
    
    return Playlist(); // Return empty playlist if loading failed
}

bool PlaylistManager::playlistExists(const QString& name)
{
    if (!m_dbManager || name.isEmpty()) {
        return false;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("SELECT id FROM playlists WHERE name = ?");
    query.addBindValue(name);
    query.exec();
    
    return query.next();
}

// Playlist content management
bool PlaylistManager::addToPlaylist(int playlistId, const MediaFile& file)
{
    if (!m_dbManager || playlistId <= 0 || !file.isValid()) {
        return false;
    }
    
    if (m_dbManager->addToPlaylist(playlistId, file.id())) {
        emit itemAddedToPlaylist(playlistId, file);
        emit playlistUpdated(playlistId);
        return true;
    }
    
    return false;
}

bool PlaylistManager::addToPlaylist(int playlistId, const QList<MediaFile>& files)
{
    if (!m_dbManager || playlistId <= 0 || files.isEmpty()) {
        return false;
    }
    
    bool success = true;
    for (const MediaFile& file : files) {
        if (!m_dbManager->addToPlaylist(playlistId, file.id())) {
            success = false;
        } else {
            emit itemAddedToPlaylist(playlistId, file);
        }
    }
    
    if (success) {
        emit playlistUpdated(playlistId);
    }
    
    return success;
}

bool PlaylistManager::removeFromPlaylist(int playlistId, const MediaFile& file)
{
    if (!m_dbManager || playlistId <= 0 || !file.isValid()) {
        return false;
    }
    
    if (m_dbManager->removeFromPlaylist(playlistId, file.id())) {
        emit itemRemovedFromPlaylist(playlistId, file);
        emit playlistUpdated(playlistId);
        return true;
    }
    
    return false;
}

bool PlaylistManager::removeFromPlaylist(int playlistId, int position)
{
    if (!m_dbManager || playlistId <= 0 || position < 0) {
        return false;
    }
    
    // Get the file at this position first
    QSqlQuery query = m_dbManager->getPlaylistItems(playlistId);
    int currentPos = 0;
    MediaFile fileToRemove;
    
    while (query.next() && currentPos <= position) {
        if (currentPos == position) {
            fileToRemove.setId(query.value("media_file_id").toInt());
            fileToRemove.setFilePath(query.value("file_path").toString());
            fileToRemove.setTitle(query.value("title").toString());
            break;
        }
        currentPos++;
    }
    
    if (fileToRemove.isValid()) {
        return removeFromPlaylist(playlistId, fileToRemove);
    }
    
    return false;
}

bool PlaylistManager::moveInPlaylist(int playlistId, int fromPosition, int toPosition)
{
    if (!m_dbManager || playlistId <= 0) {
        return false;
    }
    
    if (m_dbManager->movePlaylistItem(playlistId, fromPosition, toPosition)) {
        emit playlistUpdated(playlistId);
        return true;
    }
    
    return false;
}

bool PlaylistManager::clearPlaylist(int playlistId)
{
    if (!m_dbManager || playlistId <= 0) {
        return false;
    }
    
    if (m_dbManager->clearPlaylist(playlistId)) {
        emit playlistCleared(playlistId);
        emit playlistUpdated(playlistId);
        return true;
    }
    
    return false;
}

// Smart playlists
int PlaylistManager::createSmartPlaylist(const QString& name, const SmartPlaylistCriteria& criteria)
{
    int playlistId = createPlaylist(name);
    if (playlistId > 0) {
        if (updateSmartPlaylist(playlistId, criteria)) {
            emit smartPlaylistCreated(playlistId, name);
            return playlistId;
        } else {
            // Clean up if smart playlist creation failed
            deletePlaylist(playlistId);
            return -1;
        }
    }
    
    return -1;
}

bool PlaylistManager::updateSmartPlaylist(int playlistId, const SmartPlaylistCriteria& criteria)
{
    if (!m_dbManager || playlistId <= 0) {
        return false;
    }
    
    // Store criteria in cache
    m_smartPlaylistCriteria[playlistId] = criteria;
    
    // Generate content
    QList<MediaFile> files = generateSmartPlaylistContent(criteria);
    
    // Update playlist content
    updateSmartPlaylistContent(playlistId, files);
    
    m_smartPlaylistLastRefresh[playlistId] = QDateTime::currentDateTime();
    
    qCInfo(playlistManager) << "Updated smart playlist ID:" << playlistId << "with" << files.size() << "items";
    return true;
}

void PlaylistManager::refreshSmartPlaylist(int playlistId)
{
    if (m_smartPlaylistCriteria.contains(playlistId)) {
        SmartPlaylistCriteria criteria = m_smartPlaylistCriteria[playlistId];
        updateSmartPlaylist(playlistId, criteria);
        
        emit smartPlaylistRefreshed(playlistId, getPlaylist(playlistId).itemCount());
    }
}

void PlaylistManager::refreshAllSmartPlaylists()
{
    for (auto it = m_smartPlaylistCriteria.begin(); it != m_smartPlaylistCriteria.end(); ++it) {
        refreshSmartPlaylist(it.key());
    }
    
    qCInfo(playlistManager) << "Refreshed all smart playlists";
}

bool PlaylistManager::isSmartPlaylist(int playlistId)
{
    return m_smartPlaylistCriteria.contains(playlistId);
}

PlaylistManager::SmartPlaylistCriteria PlaylistManager::getSmartPlaylistCriteria(int playlistId)
{
    return m_smartPlaylistCriteria.value(playlistId);
}

// Predefined smart playlists
int PlaylistManager::createRecentlyAddedPlaylist(int days, int limit)
{
    SmartPlaylistCriteria criteria;
    criteria.type = RecentlyAdded;
    criteria.limit = limit;
    criteria.dateFrom = QDateTime::currentDateTime().addDays(-days);
    criteria.sortBy = "date_added";
    criteria.sortOrder = Qt::DescendingOrder;
    
    return createSmartPlaylist("Recently Added", criteria);
}

int PlaylistManager::createMostPlayedPlaylist(int limit)
{
    SmartPlaylistCriteria criteria;
    criteria.type = MostPlayed;
    criteria.limit = limit;
    criteria.sortBy = "play_count";
    criteria.sortOrder = Qt::DescendingOrder;
    
    return createSmartPlaylist("Most Played", criteria);
}

int PlaylistManager::createRecentlyPlayedPlaylist(int days, int limit)
{
    SmartPlaylistCriteria criteria;
    criteria.type = RecentlyPlayed;
    criteria.limit = limit;
    criteria.dateFrom = QDateTime::currentDateTime().addDays(-days);
    criteria.sortBy = "last_played";
    criteria.sortOrder = Qt::DescendingOrder;
    
    return createSmartPlaylist("Recently Played", criteria);
}

int PlaylistManager::createNeverPlayedPlaylist(int limit)
{
    SmartPlaylistCriteria criteria;
    criteria.type = NeverPlayed;
    criteria.limit = limit;
    criteria.sortBy = "date_added";
    criteria.sortOrder = Qt::DescendingOrder;
    
    return createSmartPlaylist("Never Played", criteria);
}

int PlaylistManager::createGenrePlaylist(const QString& genre, int limit)
{
    SmartPlaylistCriteria criteria;
    criteria.type = ByGenre;
    criteria.field = "genre";
    criteria.value = genre;
    criteria.operator_ = "equals";
    criteria.limit = limit;
    criteria.sortBy = "artist";
    criteria.sortOrder = Qt::AscendingOrder;
    
    return createSmartPlaylist(QString("Genre: %1").arg(genre), criteria);
}

int PlaylistManager::createArtistPlaylist(const QString& artist, int limit)
{
    SmartPlaylistCriteria criteria;
    criteria.type = ByArtist;
    criteria.field = "artist";
    criteria.value = artist;
    criteria.operator_ = "equals";
    criteria.limit = limit;
    criteria.sortBy = "album";
    criteria.sortOrder = Qt::AscendingOrder;
    
    return createSmartPlaylist(QString("Artist: %1").arg(artist), criteria);
}//
 Playback history
void PlaylistManager::recordPlayback(const QString& filePath, qint64 position, qint64 duration)
{
    if (!m_dbManager || filePath.isEmpty()) {
        return;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "INSERT INTO playback_history (file_path, played_at, play_position, duration, device_id) "
        "VALUES (?, ?, ?, ?, ?)");
    
    query.addBindValue(filePath);
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(position);
    query.addBindValue(duration);
    query.addBindValue(m_deviceId);
    
    if (query.exec()) {
        // Update play count in media files table
        updatePlayCount(filePath);
        
        emit playbackRecorded(filePath, QDateTime::currentDateTime());
        qCDebug(playlistManager) << "Recorded playback for:" << filePath;
    }
}

void PlaylistManager::updatePlaybackPosition(const QString& filePath, qint64 position)
{
    if (!m_dbManager || filePath.isEmpty()) {
        return;
    }
    
    // Update the most recent playback entry for this file
    QSqlQuery query = m_dbManager->prepareQuery(
        "UPDATE playback_history SET play_position = ? "
        "WHERE file_path = ? AND device_id = ? "
        "ORDER BY played_at DESC LIMIT 1");
    
    query.addBindValue(position);
    query.addBindValue(filePath);
    query.addBindValue(m_deviceId);
    query.exec();
}

void PlaylistManager::markPlaybackCompleted(const QString& filePath)
{
    if (!m_dbManager || filePath.isEmpty()) {
        return;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "UPDATE playback_history SET completed = 1 "
        "WHERE file_path = ? AND device_id = ? "
        "ORDER BY played_at DESC LIMIT 1");
    
    query.addBindValue(filePath);
    query.addBindValue(m_deviceId);
    query.exec();
}

QList<PlaylistManager::PlaybackHistoryEntry> PlaylistManager::getPlaybackHistory(int limit)
{
    QList<PlaybackHistoryEntry> history;
    
    if (!m_dbManager) {
        return history;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT * FROM playback_history ORDER BY played_at DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    
    while (query.next()) {
        PlaybackHistoryEntry entry;
        entry.id = query.value("id").toInt();
        entry.filePath = query.value("file_path").toString();
        entry.playedAt = query.value("played_at").toDateTime();
        entry.playPosition = query.value("play_position").toLongLong();
        entry.duration = query.value("duration").toLongLong();
        entry.completed = query.value("completed").toBool();
        entry.deviceId = query.value("device_id").toString();
        
        history.append(entry);
    }
    
    return history;
}

QList<PlaylistManager::PlaybackHistoryEntry> PlaylistManager::getPlaybackHistoryForFile(const QString& filePath)
{
    QList<PlaybackHistoryEntry> history;
    
    if (!m_dbManager || filePath.isEmpty()) {
        return history;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT * FROM playback_history WHERE file_path = ? ORDER BY played_at DESC");
    query.addBindValue(filePath);
    query.exec();
    
    while (query.next()) {
        PlaybackHistoryEntry entry;
        entry.id = query.value("id").toInt();
        entry.filePath = query.value("file_path").toString();
        entry.playedAt = query.value("played_at").toDateTime();
        entry.playPosition = query.value("play_position").toLongLong();
        entry.duration = query.value("duration").toLongLong();
        entry.completed = query.value("completed").toBool();
        entry.deviceId = query.value("device_id").toString();
        
        history.append(entry);
    }
    
    return history;
}

PlaylistManager::PlaybackHistoryEntry PlaylistManager::getLastPlaybackEntry(const QString& filePath)
{
    PlaybackHistoryEntry entry;
    
    if (!m_dbManager || filePath.isEmpty()) {
        return entry;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT * FROM playback_history WHERE file_path = ? ORDER BY played_at DESC LIMIT 1");
    query.addBindValue(filePath);
    query.exec();
    
    if (query.next()) {
        entry.id = query.value("id").toInt();
        entry.filePath = query.value("file_path").toString();
        entry.playedAt = query.value("played_at").toDateTime();
        entry.playPosition = query.value("play_position").toLongLong();
        entry.duration = query.value("duration").toLongLong();
        entry.completed = query.value("completed").toBool();
        entry.deviceId = query.value("device_id").toString();
    }
    
    return entry;
}

void PlaylistManager::clearPlaybackHistory()
{
    if (!m_dbManager) {
        return;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("DELETE FROM playback_history");
    query.exec();
    
    qCInfo(playlistManager) << "Cleared playback history";
}

void PlaylistManager::clearPlaybackHistoryBefore(const QDateTime& date)
{
    if (!m_dbManager) {
        return;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("DELETE FROM playback_history WHERE played_at < ?");
    query.addBindValue(date);
    query.exec();
    
    qCInfo(playlistManager) << "Cleared playback history before:" << date;
}

// Watch progress tracking
void PlaylistManager::saveWatchProgress(const QString& filePath, qint64 position, qint64 duration)
{
    if (!m_dbManager || filePath.isEmpty()) {
        return;
    }
    
    double percentage = duration > 0 ? (double)position / duration * 100.0 : 0.0;
    bool completed = percentage >= 90.0; // Consider 90% as completed
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "INSERT OR REPLACE INTO watch_progress "
        "(file_path, position, duration, last_watched, percentage, completed, device_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    
    query.addBindValue(filePath);
    query.addBindValue(position);
    query.addBindValue(duration);
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(percentage);
    query.addBindValue(completed);
    query.addBindValue(m_deviceId);
    
    if (query.exec()) {
        // Update cache
        WatchProgress progress;
        progress.filePath = filePath;
        progress.position = position;
        progress.duration = duration;
        progress.lastWatched = QDateTime::currentDateTime();
        progress.percentage = percentage;
        progress.completed = completed;
        progress.deviceId = m_deviceId;
        
        m_watchProgressCache[filePath] = progress;
        
        emit watchProgressSaved(filePath, position, percentage);
        qCDebug(playlistManager) << "Saved watch progress for:" << filePath << "at" << percentage << "%";
    }
}

PlaylistManager::WatchProgress PlaylistManager::getWatchProgress(const QString& filePath)
{
    WatchProgress progress;
    
    if (filePath.isEmpty()) {
        return progress;
    }
    
    // Check cache first
    if (m_watchProgressCache.contains(filePath)) {
        return m_watchProgressCache[filePath];
    }
    
    if (!m_dbManager) {
        return progress;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT * FROM watch_progress WHERE file_path = ? ORDER BY last_watched DESC LIMIT 1");
    query.addBindValue(filePath);
    query.exec();
    
    if (query.next()) {
        progress.filePath = query.value("file_path").toString();
        progress.position = query.value("position").toLongLong();
        progress.duration = query.value("duration").toLongLong();
        progress.lastWatched = query.value("last_watched").toDateTime();
        progress.percentage = query.value("percentage").toDouble();
        progress.completed = query.value("completed").toBool();
        progress.deviceId = query.value("device_id").toString();
        
        // Cache the result
        m_watchProgressCache[filePath] = progress;
    }
    
    return progress;
}

QList<PlaylistManager::WatchProgress> PlaylistManager::getAllWatchProgress()
{
    QList<WatchProgress> progressList;
    
    if (!m_dbManager) {
        return progressList;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT * FROM watch_progress ORDER BY last_watched DESC");
    query.exec();
    
    while (query.next()) {
        WatchProgress progress;
        progress.filePath = query.value("file_path").toString();
        progress.position = query.value("position").toLongLong();
        progress.duration = query.value("duration").toLongLong();
        progress.lastWatched = query.value("last_watched").toDateTime();
        progress.percentage = query.value("percentage").toDouble();
        progress.completed = query.value("completed").toBool();
        progress.deviceId = query.value("device_id").toString();
        
        progressList.append(progress);
    }
    
    return progressList;
}

void PlaylistManager::clearWatchProgress(const QString& filePath)
{
    if (!m_dbManager || filePath.isEmpty()) {
        return;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("DELETE FROM watch_progress WHERE file_path = ?");
    query.addBindValue(filePath);
    query.exec();
    
    // Remove from cache
    m_watchProgressCache.remove(filePath);
    
    qCDebug(playlistManager) << "Cleared watch progress for:" << filePath;
}

void PlaylistManager::clearCompletedWatchProgress()
{
    if (!m_dbManager) {
        return;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery("DELETE FROM watch_progress WHERE completed = 1");
    query.exec();
    
    // Clear cache of completed items
    for (auto it = m_watchProgressCache.begin(); it != m_watchProgressCache.end();) {
        if (it.value().completed) {
            it = m_watchProgressCache.erase(it);
        } else {
            ++it;
        }
    }
    
    qCInfo(playlistManager) << "Cleared completed watch progress";
}

void PlaylistManager::clearOldWatchProgress(int days)
{
    if (!m_dbManager) {
        return;
    }
    
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-days);
    
    QSqlQuery query = m_dbManager->prepareQuery("DELETE FROM watch_progress WHERE last_watched < ?");
    query.addBindValue(cutoffDate);
    query.exec();
    
    // Clear cache of old items
    for (auto it = m_watchProgressCache.begin(); it != m_watchProgressCache.end();) {
        if (it.value().lastWatched < cutoffDate) {
            it = m_watchProgressCache.erase(it);
        } else {
            ++it;
        }
    }
    
    qCInfo(playlistManager) << "Cleared watch progress older than" << days << "days";
}// Multi
-device sync
void PlaylistManager::syncWatchProgress(const QList<WatchProgress>& remoteProgress)
{
    if (!m_dbManager) {
        return;
    }
    
    for (const WatchProgress& progress : remoteProgress) {
        // Get local progress
        WatchProgress localProgress = getWatchProgress(progress.filePath);
        
        // Sync if remote is newer or local doesn't exist
        if (!localProgress.lastWatched.isValid() || progress.lastWatched > localProgress.lastWatched) {
            saveWatchProgress(progress.filePath, progress.position, progress.duration);
        }
    }
    
    qCInfo(playlistManager) << "Synced" << remoteProgress.size() << "watch progress entries";
}

QList<PlaylistManager::WatchProgress> PlaylistManager::getWatchProgressForSync(const QDateTime& since)
{
    QList<WatchProgress> progressList;
    
    if (!m_dbManager) {
        return progressList;
    }
    
    QString queryStr = "SELECT * FROM watch_progress WHERE device_id = ?";
    if (since.isValid()) {
        queryStr += " AND last_watched > ?";
    }
    queryStr += " ORDER BY last_watched DESC";
    
    QSqlQuery query = m_dbManager->prepareQuery(queryStr);
    query.addBindValue(m_deviceId);
    if (since.isValid()) {
        query.addBindValue(since);
    }
    query.exec();
    
    while (query.next()) {
        WatchProgress progress;
        progress.filePath = query.value("file_path").toString();
        progress.position = query.value("position").toLongLong();
        progress.duration = query.value("duration").toLongLong();
        progress.lastWatched = query.value("last_watched").toDateTime();
        progress.percentage = query.value("percentage").toDouble();
        progress.completed = query.value("completed").toBool();
        progress.deviceId = query.value("device_id").toString();
        
        progressList.append(progress);
    }
    
    return progressList;
}

// Queue management
void PlaylistManager::setCurrentQueue(const QList<MediaFile>& files)
{
    m_currentQueue = files;
    saveQueueToDatabase();
    emit queueUpdated(files.size());
}

void PlaylistManager::addToQueue(const MediaFile& file)
{
    m_currentQueue.append(file);
    saveQueueToDatabase();
    emit queueUpdated(m_currentQueue.size());
}

void PlaylistManager::addToQueue(const QList<MediaFile>& files)
{
    m_currentQueue.append(files);
    saveQueueToDatabase();
    emit queueUpdated(m_currentQueue.size());
}

void PlaylistManager::removeFromQueue(int position)
{
    if (position >= 0 && position < m_currentQueue.size()) {
        m_currentQueue.removeAt(position);
        saveQueueToDatabase();
        emit queueUpdated(m_currentQueue.size());
    }
}

void PlaylistManager::clearQueue()
{
    m_currentQueue.clear();
    saveQueueToDatabase();
    emit queueCleared();
    emit queueUpdated(0);
}

void PlaylistManager::shuffleQueue()
{
    if (m_currentQueue.size() > 1) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(m_currentQueue.begin(), m_currentQueue.end(), g);
        
        saveQueueToDatabase();
        emit queueUpdated(m_currentQueue.size());
    }
}

QList<MediaFile> PlaylistManager::getCurrentQueue()
{
    return m_currentQueue;
}

// Statistics and analytics
int PlaylistManager::getTotalPlaylists()
{
    if (!m_dbManager) {
        return 0;
    }
    
    return m_dbManager->getPlaylistCount();
}

int PlaylistManager::getTotalSmartPlaylists()
{
    return m_smartPlaylistCriteria.size();
}

QStringList PlaylistManager::getMostUsedPlaylists(int limit)
{
    QStringList playlists;
    
    if (!m_dbManager) {
        return playlists;
    }
    
    // This would require tracking playlist usage
    // For now, return all playlists
    QList<Playlist> allPlaylists = getAllPlaylists();
    for (const Playlist& playlist : allPlaylists) {
        playlists << playlist.name();
        if (playlists.size() >= limit) {
            break;
        }
    }
    
    return playlists;
}

QHash<QString, int> PlaylistManager::getPlaybackStatistics()
{
    QHash<QString, int> stats;
    
    if (!m_dbManager) {
        return stats;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT COUNT(*) as total_plays, "
        "COUNT(DISTINCT file_path) as unique_files, "
        "COUNT(DISTINCT DATE(played_at)) as active_days "
        "FROM playback_history");
    query.exec();
    
    if (query.next()) {
        stats["total_plays"] = query.value("total_plays").toInt();
        stats["unique_files"] = query.value("unique_files").toInt();
        stats["active_days"] = query.value("active_days").toInt();
    }
    
    return stats;
}

QList<MediaFile> PlaylistManager::getMostPlayedFiles(int limit)
{
    QList<MediaFile> files;
    
    if (!m_dbManager) {
        return files;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT mf.* FROM media_files mf "
        "WHERE mf.play_count > 0 "
        "ORDER BY mf.play_count DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    
    while (query.next()) {
        files.append(createMediaFileFromQuery(query));
    }
    
    return files;
}

QList<MediaFile> PlaylistManager::getRecentlyPlayedFiles(int limit)
{
    QList<MediaFile> files;
    
    if (!m_dbManager) {
        return files;
    }
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT DISTINCT mf.* FROM media_files mf "
        "JOIN playback_history ph ON mf.file_path = ph.file_path "
        "ORDER BY ph.played_at DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    
    while (query.next()) {
        files.append(createMediaFileFromQuery(query));
    }
    
    return files;
}

// Import/Export
bool PlaylistManager::exportPlaylist(int playlistId, const QString& filePath, const QString& format)
{
    Playlist playlist = getPlaylist(playlistId);
    if (!playlist.isValid()) {
        return false;
    }
    
    if (format.toLower() == "m3u") {
        return playlist.exportToM3U(filePath);
    } else if (format.toLower() == "pls") {
        return playlist.exportToPLS(filePath);
    }
    
    return false;
}

int PlaylistManager::importPlaylist(const QString& filePath, const QString& name)
{
    QString playlistName = name;
    if (playlistName.isEmpty()) {
        QFileInfo fileInfo(filePath);
        playlistName = fileInfo.completeBaseName();
    }
    
    // Ensure unique name
    playlistName = generateUniquePlaylistName(playlistName);
    
    int playlistId = createPlaylist(playlistName);
    if (playlistId > 0) {
        Playlist playlist = getPlaylist(playlistId);
        
        QFileInfo fileInfo(filePath);
        QString extension = fileInfo.suffix().toLower();
        
        bool success = false;
        if (extension == "m3u") {
            success = playlist.importFromM3U(filePath);
        } else if (extension == "pls") {
            success = playlist.importFromPLS(filePath);
        }
        
        if (success) {
            playlist.save(m_dbManager);
            emit playlistUpdated(playlistId);
            return playlistId;
        } else {
            deletePlaylist(playlistId);
        }
    }
    
    return -1;
}

bool PlaylistManager::exportAllPlaylists(const QString& directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QList<Playlist> playlists = getAllPlaylists();
    bool success = true;
    
    for (const Playlist& playlist : playlists) {
        QString fileName = QString("%1.m3u").arg(playlist.name());
        QString filePath = dir.absoluteFilePath(fileName);
        
        if (!playlist.exportToM3U(filePath)) {
            success = false;
        }
    }
    
    return success;
}

// Maintenance
void PlaylistManager::cleanupOrphanedEntries()
{
    if (!m_dbManager) {
        return;
    }
    
    // Clean up orphaned playlist items
    m_dbManager->cleanupOrphanedRecords();
    
    // Clean up old playback history (older than 1 year)
    QDateTime oneYearAgo = QDateTime::currentDateTime().addYears(-1);
    clearPlaybackHistoryBefore(oneYearAgo);
    
    // Clean up old watch progress (older than 6 months for completed items)
    QDateTime sixMonthsAgo = QDateTime::currentDateTime().addMonths(-6);
    QSqlQuery query = m_dbManager->prepareQuery(
        "DELETE FROM watch_progress WHERE completed = 1 AND last_watched < ?");
    query.addBindValue(sixMonthsAgo);
    query.exec();
    
    qCInfo(playlistManager) << "Cleaned up orphaned entries";
}

void PlaylistManager::optimizeDatabase()
{
    if (m_dbManager) {
        m_dbManager->optimizeDatabase();
    }
}

void PlaylistManager::rebuildSmartPlaylists()
{
    refreshAllSmartPlaylists();
    qCInfo(playlistManager) << "Rebuilt all smart playlists";
}

// Private helper methods
bool PlaylistManager::createPlaylistTable()
{
    // Playlists table is created by DatabaseManager
    return true;
}

bool PlaylistManager::createSmartPlaylistTable()
{
    if (!m_dbManager) {
        return false;
    }
    
    QString query = R"(
        CREATE TABLE IF NOT EXISTS smart_playlists (
            playlist_id INTEGER PRIMARY KEY,
            criteria TEXT NOT NULL,
            last_refresh DATETIME,
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE
        )
    )";
    
    return m_dbManager->executeQuery(query);
}

bool PlaylistManager::createPlaybackHistoryTable()
{
    if (!m_dbManager) {
        return false;
    }
    
    QString query = R"(
        CREATE TABLE IF NOT EXISTS playback_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            played_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            play_position INTEGER DEFAULT 0,
            duration INTEGER DEFAULT 0,
            completed BOOLEAN DEFAULT 0,
            device_id TEXT
        )
    )";
    
    return m_dbManager->executeQuery(query);
}

bool PlaylistManager::createWatchProgressTable()
{
    if (!m_dbManager) {
        return false;
    }
    
    QString query = R"(
        CREATE TABLE IF NOT EXISTS watch_progress (
            file_path TEXT PRIMARY KEY,
            position INTEGER DEFAULT 0,
            duration INTEGER DEFAULT 0,
            last_watched DATETIME DEFAULT CURRENT_TIMESTAMP,
            percentage REAL DEFAULT 0.0,
            completed BOOLEAN DEFAULT 0,
            device_id TEXT
        )
    )";
    
    return m_dbManager->executeQuery(query);
}

bool PlaylistManager::createQueueTable()
{
    if (!m_dbManager) {
        return false;
    }
    
    QString query = R"(
        CREATE TABLE IF NOT EXISTS current_queue (
            position INTEGER PRIMARY KEY,
            file_path TEXT NOT NULL,
            added_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    return m_dbManager->executeQuery(query);
}

void PlaylistManager::performPeriodicMaintenance()
{
    cleanupOrphanedEntries();
    refreshAllSmartPlaylists();
    
    qCDebug(playlistManager) << "Performed periodic maintenance";
}

} // namespace Data
} // namespace EonPlay// Smart
 playlist helpers
QList<MediaFile> PlaylistManager::generateSmartPlaylistContent(const SmartPlaylistCriteria& criteria)
{
    QList<MediaFile> files;
    
    if (!m_dbManager) {
        return files;
    }
    
    QString queryStr = buildSmartPlaylistQuery(criteria);
    QSqlQuery query = m_dbManager->prepareQuery(queryStr);
    
    // Add parameters based on criteria
    if (criteria.type == ByGenre || criteria.type == ByArtist || criteria.type == ByAlbum) {
        query.addBindValue(criteria.value);
    } else if (criteria.type == RecentlyAdded || criteria.type == RecentlyPlayed) {
        if (criteria.dateFrom.isValid()) {
            query.addBindValue(criteria.dateFrom);
        }
    }
    
    if (criteria.limit > 0) {
        query.addBindValue(criteria.limit);
    }
    
    query.exec();
    
    while (query.next()) {
        files.append(createMediaFileFromQuery(query));
    }
    
    return files;
}

QString PlaylistManager::buildSmartPlaylistQuery(const SmartPlaylistCriteria& criteria)
{
    QString baseQuery = "SELECT * FROM media_files WHERE 1=1";
    
    switch (criteria.type) {
        case RecentlyAdded:
            if (criteria.dateFrom.isValid()) {
                baseQuery += " AND date_added >= ?";
            }
            break;
            
        case MostPlayed:
            baseQuery += " AND play_count > 0";
            break;
            
        case RecentlyPlayed:
            if (criteria.dateFrom.isValid()) {
                baseQuery += " AND last_played >= ?";
            }
            baseQuery += " AND last_played IS NOT NULL";
            break;
            
        case NeverPlayed:
            baseQuery += " AND (play_count = 0 OR play_count IS NULL)";
            break;
            
        case ByGenre:
            baseQuery += " AND genre = ?";
            break;
            
        case ByArtist:
            baseQuery += " AND artist = ?";
            break;
            
        case ByAlbum:
            baseQuery += " AND album = ?";
            break;
            
        case ByYear:
            baseQuery += QString(" AND year = %1").arg(criteria.value.toInt());
            break;
            
        case ByRating:
            baseQuery += QString(" AND rating >= %1").arg(criteria.value.toInt());
            break;
            
        case Custom:
            if (!criteria.field.isEmpty() && !criteria.value.isEmpty()) {
                if (criteria.operator_ == "equals") {
                    baseQuery += QString(" AND %1 = '%2'").arg(criteria.field, criteria.value);
                } else if (criteria.operator_ == "contains") {
                    baseQuery += QString(" AND %1 LIKE '%%2%'").arg(criteria.field, criteria.value);
                } else if (criteria.operator_ == "greater_than") {
                    baseQuery += QString(" AND %1 > %2").arg(criteria.field, criteria.value);
                } else if (criteria.operator_ == "less_than") {
                    baseQuery += QString(" AND %1 < %2").arg(criteria.field, criteria.value);
                }
            }
            break;
    }
    
    // Add sorting
    if (!criteria.sortBy.isEmpty()) {
        baseQuery += QString(" ORDER BY %1").arg(criteria.sortBy);
        if (criteria.sortOrder == Qt::DescendingOrder) {
            baseQuery += " DESC";
        } else {
            baseQuery += " ASC";
        }
    }
    
    // Add limit
    if (criteria.limit > 0) {
        baseQuery += " LIMIT ?";
    }
    
    return baseQuery;
}

void PlaylistManager::updateSmartPlaylistContent(int playlistId, const QList<MediaFile>& files)
{
    if (!m_dbManager) {
        return;
    }
    
    // Clear existing content
    clearPlaylist(playlistId);
    
    // Add new content
    addToPlaylist(playlistId, files);
}

// History helpers
void PlaylistManager::cleanupOldHistory()
{
    // Clean up history older than 2 years
    QDateTime twoYearsAgo = QDateTime::currentDateTime().addYears(-2);
    clearPlaybackHistoryBefore(twoYearsAgo);
}

void PlaylistManager::updatePlayCount(const QString& filePath)
{
    if (!m_dbManager || filePath.isEmpty()) {
        return;
    }
    
    // Get media file ID
    QSqlQuery fileQuery = m_dbManager->getMediaFileByPath(filePath);
    if (fileQuery.next()) {
        int fileId = fileQuery.value("id").toInt();
        m_dbManager->updatePlayCount(fileId);
        m_dbManager->updateLastPlayed(fileId);
    }
}

// Queue helpers
void PlaylistManager::saveQueueToDatabase()
{
    if (!m_dbManager) {
        return;
    }
    
    // Clear existing queue
    QSqlQuery clearQuery = m_dbManager->prepareQuery("DELETE FROM current_queue");
    clearQuery.exec();
    
    // Save current queue
    for (int i = 0; i < m_currentQueue.size(); ++i) {
        QSqlQuery insertQuery = m_dbManager->prepareQuery(
            "INSERT INTO current_queue (position, file_path) VALUES (?, ?)");
        insertQuery.addBindValue(i);
        insertQuery.addBindValue(m_currentQueue[i].filePath());
        insertQuery.exec();
    }
}

void PlaylistManager::loadQueueFromDatabase()
{
    if (!m_dbManager) {
        return;
    }
    
    m_currentQueue.clear();
    
    QSqlQuery query = m_dbManager->prepareQuery(
        "SELECT cq.file_path, mf.* FROM current_queue cq "
        "LEFT JOIN media_files mf ON cq.file_path = mf.file_path "
        "ORDER BY cq.position");
    query.exec();
    
    while (query.next()) {
        MediaFile file;
        file.setFilePath(query.value("file_path").toString());
        
        // Set additional metadata if available
        if (!query.value("title").isNull()) {
            file.setId(query.value("id").toInt());
            file.setTitle(query.value("title").toString());
            file.setArtist(query.value("artist").toString());
            file.setAlbum(query.value("album").toString());
            file.setDuration(query.value("duration").toLongLong());
        }
        
        m_currentQueue.append(file);
    }
}

// Utility methods
MediaFile PlaylistManager::createMediaFileFromQuery(const QSqlQuery& query)
{
    MediaFile file;
    file.setId(query.value("id").toInt());
    file.setFilePath(query.value("file_path").toString());
    file.setTitle(query.value("title").toString());
    file.setArtist(query.value("artist").toString());
    file.setAlbum(query.value("album").toString());
    file.setGenre(query.value("genre").toString());
    file.setYear(query.value("year").toInt());
    file.setTrackNumber(query.value("track_number").toInt());
    file.setDuration(query.value("duration").toLongLong());
    file.setFileSize(query.value("file_size").toLongLong());
    file.setDateAdded(query.value("date_added").toDateTime());
    file.setLastPlayed(query.value("last_played").toDateTime());
    file.setPlayCount(query.value("play_count").toInt());
    file.setRating(query.value("rating").toInt());
    
    return file;
}

QString PlaylistManager::generateUniquePlaylistName(const QString& baseName)
{
    QString uniqueName = baseName;
    int counter = 1;
    
    while (playlistExists(uniqueName)) {
        uniqueName = QString("%1 (%2)").arg(baseName).arg(counter);
        counter++;
    }
    
    return uniqueName;
}

bool PlaylistManager::isValidPlaylistName(const QString& name)
{
    if (name.isEmpty() || name.length() > 255) {
        return false;
    }
    
    // Check for invalid characters
    QRegularExpression invalidChars("[<>:\"/\\|?*]");
    if (invalidChars.match(name).hasMatch()) {
        return false;
    }
    
    return true;
}

} // namespace Data
} // namespace EonPlay
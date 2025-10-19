#pragma once

#include "data/Playlist.h"
#include "data/MediaFile.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QTimer>
#include <QDateTime>
#include <QSqlQuery>
#include <memory>

namespace EonPlay {
namespace Data {

class DatabaseManager;
class LibraryManager;

/**
 * @brief Manages playlists and playback history for EonPlay
 * 
 * Provides comprehensive playlist management including manual playlists,
 * smart playlists, playback history, and watch progress tracking.
 */
class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    enum SmartPlaylistType {
        RecentlyAdded,
        MostPlayed,
        RecentlyPlayed,
        NeverPlayed,
        ByGenre,
        ByArtist,
        ByAlbum,
        ByYear,
        ByRating,
        Custom
    };

    struct SmartPlaylistCriteria {
        SmartPlaylistType type = Custom;
        QString field;          // For custom criteria (e.g., "genre", "artist")
        QString value;          // Value to match
        QString operator_;      // "equals", "contains", "greater_than", etc.
        int limit = 100;        // Maximum number of items
        QString sortBy = "date_added";
        Qt::SortOrder sortOrder = Qt::DescendingOrder;
        QDateTime dateFrom;     // For date-based criteria
        QDateTime dateTo;
    };

    struct PlaybackHistoryEntry {
        int id;
        QString filePath;
        QDateTime playedAt;
        qint64 playPosition = 0;    // Position in milliseconds
        qint64 duration = 0;        // Total duration
        bool completed = false;     // Whether playback was completed
        QString deviceId;           // For multi-device sync
    };

    struct WatchProgress {
        QString filePath;
        qint64 position = 0;        // Current position in milliseconds
        qint64 duration = 0;        // Total duration
        QDateTime lastWatched;
        double percentage = 0.0;    // Completion percentage
        bool completed = false;
        QString deviceId;
    };

    explicit PlaylistManager(DatabaseManager* dbManager, QObject* parent = nullptr);
    ~PlaylistManager() = default;

    // Initialization
    void setLibraryManager(LibraryManager* libraryManager) { m_libraryManager = libraryManager; }

    // Manual playlist operations
    int createPlaylist(const QString& name, const QString& description = QString());
    bool deletePlaylist(int playlistId);
    bool renamePlaylist(int playlistId, const QString& newName);
    bool updatePlaylistDescription(int playlistId, const QString& description);
    
    QList<Playlist> getAllPlaylists();
    Playlist getPlaylist(int playlistId);
    bool playlistExists(const QString& name);
    
    // Playlist content management
    bool addToPlaylist(int playlistId, const MediaFile& file);
    bool addToPlaylist(int playlistId, const QList<MediaFile>& files);
    bool removeFromPlaylist(int playlistId, const MediaFile& file);
    bool removeFromPlaylist(int playlistId, int position);
    bool moveInPlaylist(int playlistId, int fromPosition, int toPosition);
    bool clearPlaylist(int playlistId);
    
    // Smart playlists
    int createSmartPlaylist(const QString& name, const SmartPlaylistCriteria& criteria);
    bool updateSmartPlaylist(int playlistId, const SmartPlaylistCriteria& criteria);
    void refreshSmartPlaylist(int playlistId);
    void refreshAllSmartPlaylists();
    bool isSmartPlaylist(int playlistId);
    SmartPlaylistCriteria getSmartPlaylistCriteria(int playlistId);
    
    // Predefined smart playlists
    int createRecentlyAddedPlaylist(int days = 7, int limit = 100);
    int createMostPlayedPlaylist(int limit = 50);
    int createRecentlyPlayedPlaylist(int days = 30, int limit = 100);
    int createNeverPlayedPlaylist(int limit = 100);
    int createGenrePlaylist(const QString& genre, int limit = 200);
    int createArtistPlaylist(const QString& artist, int limit = 100);
    
    // Playback history
    void recordPlayback(const QString& filePath, qint64 position = 0, qint64 duration = 0);
    void updatePlaybackPosition(const QString& filePath, qint64 position);
    void markPlaybackCompleted(const QString& filePath);
    
    QList<PlaybackHistoryEntry> getPlaybackHistory(int limit = 100);
    QList<PlaybackHistoryEntry> getPlaybackHistoryForFile(const QString& filePath);
    PlaybackHistoryEntry getLastPlaybackEntry(const QString& filePath);
    void clearPlaybackHistory();
    void clearPlaybackHistoryBefore(const QDateTime& date);
    
    // Watch progress tracking
    void saveWatchProgress(const QString& filePath, qint64 position, qint64 duration);
    WatchProgress getWatchProgress(const QString& filePath);
    QList<WatchProgress> getAllWatchProgress();
    void clearWatchProgress(const QString& filePath);
    void clearCompletedWatchProgress();
    void clearOldWatchProgress(int days = 30);
    
    // Multi-device sync
    void setDeviceId(const QString& deviceId) { m_deviceId = deviceId; }
    QString deviceId() const { return m_deviceId; }
    void syncWatchProgress(const QList<WatchProgress>& remoteProgress);
    QList<WatchProgress> getWatchProgressForSync(const QDateTime& since = QDateTime());
    
    // Queue management
    void setCurrentQueue(const QList<MediaFile>& files);
    void addToQueue(const MediaFile& file);
    void addToQueue(const QList<MediaFile>& files);
    void removeFromQueue(int position);
    void clearQueue();
    void shuffleQueue();
    QList<MediaFile> getCurrentQueue();
    
    // Statistics and analytics
    int getTotalPlaylists();
    int getTotalSmartPlaylists();
    QStringList getMostUsedPlaylists(int limit = 10);
    QHash<QString, int> getPlaybackStatistics();
    QList<MediaFile> getMostPlayedFiles(int limit = 50);
    QList<MediaFile> getRecentlyPlayedFiles(int limit = 50);
    
    // Import/Export
    bool exportPlaylist(int playlistId, const QString& filePath, const QString& format = "m3u");
    int importPlaylist(const QString& filePath, const QString& name = QString());
    bool exportAllPlaylists(const QString& directory);
    
    // Maintenance
    void cleanupOrphanedEntries();
    void optimizeDatabase();
    void rebuildSmartPlaylists();

signals:
    // Playlist signals
    void playlistCreated(int playlistId, const QString& name);
    void playlistDeleted(int playlistId);
    void playlistRenamed(int playlistId, const QString& newName);
    void playlistUpdated(int playlistId);
    
    // Content signals
    void itemAddedToPlaylist(int playlistId, const MediaFile& file);
    void itemRemovedFromPlaylist(int playlistId, const MediaFile& file);
    void playlistCleared(int playlistId);
    
    // Smart playlist signals
    void smartPlaylistCreated(int playlistId, const QString& name);
    void smartPlaylistRefreshed(int playlistId, int itemCount);
    
    // History signals
    void playbackRecorded(const QString& filePath, const QDateTime& timestamp);
    void watchProgressSaved(const QString& filePath, qint64 position, double percentage);
    
    // Queue signals
    void queueUpdated(int itemCount);
    void queueCleared();

private slots:
    void performPeriodicMaintenance();

private:
    // Database operations
    bool createPlaylistTable();
    bool createSmartPlaylistTable();
    bool createPlaybackHistoryTable();
    bool createWatchProgressTable();
    bool createQueueTable();
    
    // Smart playlist helpers
    QList<MediaFile> generateSmartPlaylistContent(const SmartPlaylistCriteria& criteria);
    QString buildSmartPlaylistQuery(const SmartPlaylistCriteria& criteria);
    void updateSmartPlaylistContent(int playlistId, const QList<MediaFile>& files);
    
    // History helpers
    void cleanupOldHistory();
    void updatePlayCount(const QString& filePath);
    
    // Queue helpers
    void saveQueueToDatabase();
    void loadQueueFromDatabase();
    
    // Utility methods
    MediaFile createMediaFileFromQuery(const QSqlQuery& query);
    QString generateUniquePlaylistName(const QString& baseName);
    bool isValidPlaylistName(const QString& name);
    
    // Member variables
    DatabaseManager* m_dbManager;
    LibraryManager* m_libraryManager;
    QString m_deviceId;
    
    // Current queue
    QList<MediaFile> m_currentQueue;
    
    // Smart playlist cache
    QHash<int, SmartPlaylistCriteria> m_smartPlaylistCriteria;
    QHash<int, QDateTime> m_smartPlaylistLastRefresh;
    
    // Maintenance timer
    QTimer* m_maintenanceTimer;
    
    // Watch progress cache
    QHash<QString, WatchProgress> m_watchProgressCache;
    QDateTime m_watchProgressCacheTime;
};

} // namespace Data
} // namespace EonPlay
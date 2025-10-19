#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVariant>
#include <QMutex>
#include <memory>

namespace EonPlay {
namespace Data {

/**
 * @brief Database manager for EonPlay media library
 * 
 * Handles SQLite database operations including schema creation,
 * migrations, and CRUD operations for media files and playlists.
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    // Database lifecycle
    bool initialize(const QString& databasePath);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Schema management
    bool createSchema();
    bool migrateDatabase();
    int getCurrentSchemaVersion();
    bool setSchemaVersion(int version);

    // Media files operations
    bool addMediaFile(const QString& filePath, const QString& title = QString(),
                     const QString& artist = QString(), const QString& album = QString(),
                     qint64 duration = 0, qint64 fileSize = 0);
    bool updateMediaFile(int id, const QString& title, const QString& artist,
                        const QString& album, qint64 duration);
    bool removeMediaFile(int id);
    bool removeMediaFileByPath(const QString& filePath);
    QSqlQuery getMediaFile(int id);
    QSqlQuery getMediaFileByPath(const QString& filePath);
    QSqlQuery getAllMediaFiles();
    QSqlQuery searchMediaFiles(const QString& searchTerm);
    bool updatePlayCount(int id);
    bool updateLastPlayed(int id, const QDateTime& timestamp = QDateTime::currentDateTime());

    // Playlist operations
    int createPlaylist(const QString& name);
    bool updatePlaylist(int id, const QString& name);
    bool removePlaylist(int id);
    QSqlQuery getPlaylist(int id);
    QSqlQuery getAllPlaylists();
    bool addToPlaylist(int playlistId, int mediaFileId, int position = -1);
    bool removeFromPlaylist(int playlistId, int mediaFileId);
    bool movePlaylistItem(int playlistId, int fromPosition, int toPosition);
    QSqlQuery getPlaylistItems(int playlistId);
    bool clearPlaylist(int playlistId);

    // Statistics and maintenance
    int getMediaFileCount();
    int getPlaylistCount();
    qint64 getTotalLibrarySize();
    bool cleanupOrphanedRecords();
    bool optimizeDatabase();

    // Backup and restore
    bool createBackup(const QString& backupPath);
    bool restoreFromBackup(const QString& backupPath);

    // Transaction support
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Error handling
    QString lastError() const { return m_lastError; }
    QSqlError lastSqlError() const;

signals:
    void mediaFileAdded(int id, const QString& filePath);
    void mediaFileRemoved(int id, const QString& filePath);
    void mediaFileUpdated(int id);
    void playlistCreated(int id, const QString& name);
    void playlistRemoved(int id);
    void playlistUpdated(int id);
    void databaseError(const QString& error);

private slots:
    void handleDatabaseError();

private:
    // Database setup
    bool setupDatabase();
    bool createTables();
    bool createIndexes();
    bool createTriggers();

    // Migration helpers
    bool migrateToVersion2();
    bool migrateToVersion3();
    // Add more migration methods as needed

public:
    // Utility methods (made public for PlaylistManager)
    bool executeQuery(const QString& query, const QVariantList& params = QVariantList());
    QSqlQuery prepareQuery(const QString& query);
    bool tableExists(const QString& tableName);
    QStringList getTableColumns(const QString& tableName);

private:
    void logError(const QString& operation, const QSqlError& error);

    // Member variables
    QSqlDatabase m_database;
    QString m_databasePath;
    bool m_initialized;
    mutable QMutex m_mutex;
    QString m_lastError;

    // Schema version constants
    static const int CURRENT_SCHEMA_VERSION = 1;
    static const QString DATABASE_CONNECTION_NAME;
};

} // namespace Data
} // namespace EonPlay
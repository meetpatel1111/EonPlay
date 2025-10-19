#include "data/DatabaseManager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QFile>
#include <QDebug>

Q_LOGGING_CATEGORY(dbManager, "eonplay.database")

namespace EonPlay {
namespace Data {

const QString DatabaseManager::DATABASE_CONNECTION_NAME = "EonPlayDatabase";

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
{
}

DatabaseManager::~DatabaseManager()
{
    shutdown();
}

bool DatabaseManager::initialize(const QString& databasePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        qCWarning(dbManager) << "Database already initialized";
        return true;
    }

    m_databasePath = databasePath;
    
    // Ensure database directory exists
    QFileInfo dbInfo(m_databasePath);
    QDir dbDir = dbInfo.absoluteDir();
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            m_lastError = QString("Failed to create database directory: %1").arg(dbDir.absolutePath());
            qCCritical(dbManager) << m_lastError;
            return false;
        }
    }

    // Setup database connection
    if (!setupDatabase()) {
        return false;
    }

    // Create schema if needed
    if (!createSchema()) {
        return false;
    }

    // Run migrations if needed
    if (!migrateDatabase()) {
        return false;
    }

    m_initialized = true;
    qCInfo(dbManager) << "Database initialized successfully:" << m_databasePath;
    return true;
}

void DatabaseManager::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        if (m_database.isOpen()) {
            m_database.close();
        }
        QSqlDatabase::removeDatabase(DATABASE_CONNECTION_NAME);
        m_initialized = false;
        qCInfo(dbManager) << "Database shutdown complete";
    }
}

bool DatabaseManager::setupDatabase()
{
    // Remove existing connection if it exists
    if (QSqlDatabase::contains(DATABASE_CONNECTION_NAME)) {
        QSqlDatabase::removeDatabase(DATABASE_CONNECTION_NAME);
    }

    // Create new SQLite connection
    m_database = QSqlDatabase::addDatabase("QSQLITE", DATABASE_CONNECTION_NAME);
    m_database.setDatabaseName(m_databasePath);

    // Configure SQLite options
    m_database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=30000");

    if (!m_database.open()) {
        m_lastError = QString("Failed to open database: %1").arg(m_database.lastError().text());
        qCCritical(dbManager) << m_lastError;
        return false;
    }

    // Enable foreign key constraints
    QSqlQuery pragmaQuery(m_database);
    if (!pragmaQuery.exec("PRAGMA foreign_keys = ON")) {
        qCWarning(dbManager) << "Failed to enable foreign keys:" << pragmaQuery.lastError().text();
    }

    // Set journal mode to WAL for better concurrency
    if (!pragmaQuery.exec("PRAGMA journal_mode = WAL")) {
        qCWarning(dbManager) << "Failed to set WAL mode:" << pragmaQuery.lastError().text();
    }

    return true;
}

bool DatabaseManager::createSchema()
{
    // Check if schema already exists
    if (tableExists("media_files")) {
        return true;
    }

    qCInfo(dbManager) << "Creating database schema";

    if (!createTables()) {
        return false;
    }

    if (!createIndexes()) {
        return false;
    }

    if (!createTriggers()) {
        return false;
    }

    // Set initial schema version
    if (!setSchemaVersion(CURRENT_SCHEMA_VERSION)) {
        return false;
    }

    return true;
}

bool DatabaseManager::createTables()
{
    QStringList tableQueries = {
        // Schema version table
        R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY
        )
        )",

        // Media files table
        R"(
        CREATE TABLE IF NOT EXISTS media_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT UNIQUE NOT NULL,
            title TEXT,
            artist TEXT,
            album TEXT,
            genre TEXT,
            year INTEGER,
            track_number INTEGER,
            duration INTEGER DEFAULT 0,
            file_size INTEGER DEFAULT 0,
            date_added DATETIME DEFAULT CURRENT_TIMESTAMP,
            date_modified DATETIME,
            last_played DATETIME,
            play_count INTEGER DEFAULT 0,
            rating INTEGER DEFAULT 0,
            cover_art_path TEXT,
            metadata_hash TEXT
        )
        )",

        // Playlists table
        R"(
        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT,
            created_date DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified_date DATETIME DEFAULT CURRENT_TIMESTAMP,
            item_count INTEGER DEFAULT 0,
            total_duration INTEGER DEFAULT 0
        )
        )",

        // Playlist items table
        R"(
        CREATE TABLE IF NOT EXISTS playlist_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            playlist_id INTEGER NOT NULL,
            media_file_id INTEGER NOT NULL,
            position INTEGER NOT NULL,
            added_date DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
            FOREIGN KEY (media_file_id) REFERENCES media_files(id) ON DELETE CASCADE,
            UNIQUE(playlist_id, position)
        )
        )"
    };

    for (const QString& query : tableQueries) {
        if (!executeQuery(query)) {
            m_lastError = QString("Failed to create table: %1").arg(lastSqlError().text());
            return false;
        }
    }

    return true;
}

bool DatabaseManager::createIndexes()
{
    QStringList indexQueries = {
        "CREATE INDEX IF NOT EXISTS idx_media_files_path ON media_files(file_path)",
        "CREATE INDEX IF NOT EXISTS idx_media_files_title ON media_files(title)",
        "CREATE INDEX IF NOT EXISTS idx_media_files_artist ON media_files(artist)",
        "CREATE INDEX IF NOT EXISTS idx_media_files_album ON media_files(album)",
        "CREATE INDEX IF NOT EXISTS idx_media_files_genre ON media_files(genre)",
        "CREATE INDEX IF NOT EXISTS idx_media_files_date_added ON media_files(date_added)",
        "CREATE INDEX IF NOT EXISTS idx_media_files_last_played ON media_files(last_played)",
        "CREATE INDEX IF NOT EXISTS idx_playlist_items_playlist ON playlist_items(playlist_id)",
        "CREATE INDEX IF NOT EXISTS idx_playlist_items_media ON playlist_items(media_file_id)",
        "CREATE INDEX IF NOT EXISTS idx_playlist_items_position ON playlist_items(playlist_id, position)"
    };

    for (const QString& query : indexQueries) {
        if (!executeQuery(query)) {
            qCWarning(dbManager) << "Failed to create index:" << lastSqlError().text();
            // Continue with other indexes even if one fails
        }
    }

    return true;
}

bool DatabaseManager::createTriggers()
{
    QStringList triggerQueries = {
        // Update playlist modified date when items are added/removed
        R"(
        CREATE TRIGGER IF NOT EXISTS update_playlist_modified
        AFTER INSERT ON playlist_items
        BEGIN
            UPDATE playlists 
            SET modified_date = CURRENT_TIMESTAMP,
                item_count = (SELECT COUNT(*) FROM playlist_items WHERE playlist_id = NEW.playlist_id),
                total_duration = (SELECT COALESCE(SUM(mf.duration), 0) 
                                FROM playlist_items pi 
                                JOIN media_files mf ON pi.media_file_id = mf.id 
                                WHERE pi.playlist_id = NEW.playlist_id)
            WHERE id = NEW.playlist_id;
        END
        )",

        R"(
        CREATE TRIGGER IF NOT EXISTS update_playlist_modified_delete
        AFTER DELETE ON playlist_items
        BEGIN
            UPDATE playlists 
            SET modified_date = CURRENT_TIMESTAMP,
                item_count = (SELECT COUNT(*) FROM playlist_items WHERE playlist_id = OLD.playlist_id),
                total_duration = (SELECT COALESCE(SUM(mf.duration), 0) 
                                FROM playlist_items pi 
                                JOIN media_files mf ON pi.media_file_id = mf.id 
                                WHERE pi.playlist_id = OLD.playlist_id)
            WHERE id = OLD.playlist_id;
        END
        )",

        // Update media file modified date
        R"(
        CREATE TRIGGER IF NOT EXISTS update_media_modified
        AFTER UPDATE ON media_files
        BEGIN
            UPDATE media_files 
            SET date_modified = CURRENT_TIMESTAMP 
            WHERE id = NEW.id;
        END
        )"
    };

    for (const QString& query : triggerQueries) {
        if (!executeQuery(query)) {
            qCWarning(dbManager) << "Failed to create trigger:" << lastSqlError().text();
            // Continue with other triggers even if one fails
        }
    }

    return true;
}

bool DatabaseManager::migrateDatabase()
{
    int currentVersion = getCurrentSchemaVersion();
    
    if (currentVersion == CURRENT_SCHEMA_VERSION) {
        return true; // No migration needed
    }

    if (currentVersion > CURRENT_SCHEMA_VERSION) {
        m_lastError = QString("Database version %1 is newer than supported version %2")
                     .arg(currentVersion).arg(CURRENT_SCHEMA_VERSION);
        qCCritical(dbManager) << m_lastError;
        return false;
    }

    qCInfo(dbManager) << "Migrating database from version" << currentVersion 
                      << "to version" << CURRENT_SCHEMA_VERSION;

    // Run migrations sequentially
    for (int version = currentVersion + 1; version <= CURRENT_SCHEMA_VERSION; ++version) {
        if (!beginTransaction()) {
            return false;
        }

        bool migrationSuccess = false;
        switch (version) {
            case 2:
                migrationSuccess = migrateToVersion2();
                break;
            case 3:
                migrationSuccess = migrateToVersion3();
                break;
            // Add more migration cases as needed
            default:
                m_lastError = QString("Unknown migration version: %1").arg(version);
                qCCritical(dbManager) << m_lastError;
                rollbackTransaction();
                return false;
        }

        if (migrationSuccess && setSchemaVersion(version)) {
            commitTransaction();
            qCInfo(dbManager) << "Successfully migrated to version" << version;
        } else {
            rollbackTransaction();
            m_lastError = QString("Failed to migrate to version %1").arg(version);
            qCCritical(dbManager) << m_lastError;
            return false;
        }
    }

    return true;
}

int DatabaseManager::getCurrentSchemaVersion()
{
    if (!tableExists("schema_version")) {
        return 0; // No schema version table means version 0
    }

    QSqlQuery query = prepareQuery("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool DatabaseManager::setSchemaVersion(int version)
{
    return executeQuery("INSERT OR REPLACE INTO schema_version (version) VALUES (?)", {version});
}

// Media file operations
bool DatabaseManager::addMediaFile(const QString& filePath, const QString& title,
                                  const QString& artist, const QString& album,
                                  qint64 duration, qint64 fileSize)
{
    QMutexLocker locker(&m_mutex);
    
    const QString query = R"(
        INSERT INTO media_files (file_path, title, artist, album, duration, file_size)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    
    QVariantList params = {filePath, title, artist, album, duration, fileSize};
    
    if (executeQuery(query, params)) {
        QSqlQuery lastInsert = prepareQuery("SELECT last_insert_rowid()");
        if (lastInsert.exec() && lastInsert.next()) {
            int id = lastInsert.value(0).toInt();
            emit mediaFileAdded(id, filePath);
            return true;
        }
    }
    
    return false;
}

bool DatabaseManager::updateMediaFile(int id, const QString& title, const QString& artist,
                                     const QString& album, qint64 duration)
{
    QMutexLocker locker(&m_mutex);
    
    const QString query = R"(
        UPDATE media_files 
        SET title = ?, artist = ?, album = ?, duration = ?
        WHERE id = ?
    )";
    
    QVariantList params = {title, artist, album, duration, id};
    
    if (executeQuery(query, params)) {
        emit mediaFileUpdated(id);
        return true;
    }
    
    return false;
}

bool DatabaseManager::removeMediaFile(int id)
{
    QMutexLocker locker(&m_mutex);
    
    // Get file path before deletion for signal
    QSqlQuery pathQuery = prepareQuery("SELECT file_path FROM media_files WHERE id = ?");
    pathQuery.addBindValue(id);
    QString filePath;
    if (pathQuery.exec() && pathQuery.next()) {
        filePath = pathQuery.value(0).toString();
    }
    
    if (executeQuery("DELETE FROM media_files WHERE id = ?", {id})) {
        emit mediaFileRemoved(id, filePath);
        return true;
    }
    
    return false;
}

bool DatabaseManager::removeMediaFileByPath(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    
    // Get ID before deletion for signal
    QSqlQuery idQuery = prepareQuery("SELECT id FROM media_files WHERE file_path = ?");
    idQuery.addBindValue(filePath);
    int id = -1;
    if (idQuery.exec() && idQuery.next()) {
        id = idQuery.value(0).toInt();
    }
    
    if (executeQuery("DELETE FROM media_files WHERE file_path = ?", {filePath})) {
        if (id != -1) {
            emit mediaFileRemoved(id, filePath);
        }
        return true;
    }
    
    return false;
}

QSqlQuery DatabaseManager::getMediaFile(int id)
{
    QSqlQuery query = prepareQuery("SELECT * FROM media_files WHERE id = ?");
    query.addBindValue(id);
    query.exec();
    return query;
}

QSqlQuery DatabaseManager::getMediaFileByPath(const QString& filePath)
{
    QSqlQuery query = prepareQuery("SELECT * FROM media_files WHERE file_path = ?");
    query.addBindValue(filePath);
    query.exec();
    return query;
}

QSqlQuery DatabaseManager::getAllMediaFiles()
{
    QSqlQuery query = prepareQuery("SELECT * FROM media_files ORDER BY title, artist");
    query.exec();
    return query;
}

QSqlQuery DatabaseManager::searchMediaFiles(const QString& searchTerm)
{
    const QString query = R"(
        SELECT * FROM media_files 
        WHERE title LIKE ? OR artist LIKE ? OR album LIKE ? OR file_path LIKE ?
        ORDER BY title, artist
    )";
    
    QString searchPattern = QString("%%1%").arg(searchTerm);
    QSqlQuery sqlQuery = prepareQuery(query);
    sqlQuery.addBindValue(searchPattern);
    sqlQuery.addBindValue(searchPattern);
    sqlQuery.addBindValue(searchPattern);
    sqlQuery.addBindValue(searchPattern);
    sqlQuery.exec();
    return sqlQuery;
}

bool DatabaseManager::updatePlayCount(int id)
{
    QMutexLocker locker(&m_mutex);
    
    const QString query = R"(
        UPDATE media_files 
        SET play_count = play_count + 1
        WHERE id = ?
    )";
    
    return executeQuery(query, {id});
}

bool DatabaseManager::updateLastPlayed(int id, const QDateTime& timestamp)
{
    QMutexLocker locker(&m_mutex);
    
    const QString query = R"(
        UPDATE media_files 
        SET last_played = ?
        WHERE id = ?
    )";
    
    return executeQuery(query, {timestamp, id});
}

// Playlist operations
int DatabaseManager::createPlaylist(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    
    const QString query = "INSERT INTO playlists (name) VALUES (?)";
    
    if (executeQuery(query, {name})) {
        QSqlQuery lastInsert = prepareQuery("SELECT last_insert_rowid()");
        if (lastInsert.exec() && lastInsert.next()) {
            int id = lastInsert.value(0).toInt();
            emit playlistCreated(id, name);
            return id;
        }
    }
    
    return -1;
}

bool DatabaseManager::updatePlaylist(int id, const QString& name)
{
    QMutexLocker locker(&m_mutex);
    
    if (executeQuery("UPDATE playlists SET name = ? WHERE id = ?", {name, id})) {
        emit playlistUpdated(id);
        return true;
    }
    
    return false;
}

bool DatabaseManager::removePlaylist(int id)
{
    QMutexLocker locker(&m_mutex);
    
    if (executeQuery("DELETE FROM playlists WHERE id = ?", {id})) {
        emit playlistRemoved(id);
        return true;
    }
    
    return false;
}

QSqlQuery DatabaseManager::getPlaylist(int id)
{
    QSqlQuery query = prepareQuery("SELECT * FROM playlists WHERE id = ?");
    query.addBindValue(id);
    query.exec();
    return query;
}

QSqlQuery DatabaseManager::getAllPlaylists()
{
    QSqlQuery query = prepareQuery("SELECT * FROM playlists ORDER BY name");
    query.exec();
    return query;
}

bool DatabaseManager::addToPlaylist(int playlistId, int mediaFileId, int position)
{
    QMutexLocker locker(&m_mutex);
    
    if (position == -1) {
        // Add to end of playlist
        QSqlQuery maxPos = prepareQuery("SELECT COALESCE(MAX(position), -1) + 1 FROM playlist_items WHERE playlist_id = ?");
        maxPos.addBindValue(playlistId);
        if (maxPos.exec() && maxPos.next()) {
            position = maxPos.value(0).toInt();
        } else {
            position = 0;
        }
    } else {
        // Shift existing items to make room
        executeQuery("UPDATE playlist_items SET position = position + 1 WHERE playlist_id = ? AND position >= ?",
                    {playlistId, position});
    }
    
    return executeQuery("INSERT INTO playlist_items (playlist_id, media_file_id, position) VALUES (?, ?, ?)",
                       {playlistId, mediaFileId, position});
}

bool DatabaseManager::removeFromPlaylist(int playlistId, int mediaFileId)
{
    QMutexLocker locker(&m_mutex);
    
    // Get position of item to remove
    QSqlQuery posQuery = prepareQuery("SELECT position FROM playlist_items WHERE playlist_id = ? AND media_file_id = ?");
    posQuery.addBindValue(playlistId);
    posQuery.addBindValue(mediaFileId);
    
    int removedPosition = -1;
    if (posQuery.exec() && posQuery.next()) {
        removedPosition = posQuery.value(0).toInt();
    }
    
    // Remove the item
    if (!executeQuery("DELETE FROM playlist_items WHERE playlist_id = ? AND media_file_id = ?",
                     {playlistId, mediaFileId})) {
        return false;
    }
    
    // Shift remaining items down
    if (removedPosition != -1) {
        executeQuery("UPDATE playlist_items SET position = position - 1 WHERE playlist_id = ? AND position > ?",
                    {playlistId, removedPosition});
    }
    
    return true;
}

bool DatabaseManager::movePlaylistItem(int playlistId, int fromPosition, int toPosition)
{
    QMutexLocker locker(&m_mutex);
    
    if (fromPosition == toPosition) {
        return true;
    }
    
    if (!beginTransaction()) {
        return false;
    }
    
    // Get the item to move
    QSqlQuery itemQuery = prepareQuery("SELECT media_file_id FROM playlist_items WHERE playlist_id = ? AND position = ?");
    itemQuery.addBindValue(playlistId);
    itemQuery.addBindValue(fromPosition);
    
    if (!itemQuery.exec() || !itemQuery.next()) {
        rollbackTransaction();
        return false;
    }
    
    int mediaFileId = itemQuery.value(0).toInt();
    
    // Remove item temporarily
    if (!executeQuery("DELETE FROM playlist_items WHERE playlist_id = ? AND position = ?",
                     {playlistId, fromPosition})) {
        rollbackTransaction();
        return false;
    }
    
    // Adjust positions
    if (fromPosition < toPosition) {
        // Moving down: shift items up
        executeQuery("UPDATE playlist_items SET position = position - 1 WHERE playlist_id = ? AND position > ? AND position <= ?",
                    {playlistId, fromPosition, toPosition});
    } else {
        // Moving up: shift items down
        executeQuery("UPDATE playlist_items SET position = position + 1 WHERE playlist_id = ? AND position >= ? AND position < ?",
                    {playlistId, toPosition, fromPosition});
    }
    
    // Insert item at new position
    if (!executeQuery("INSERT INTO playlist_items (playlist_id, media_file_id, position) VALUES (?, ?, ?)",
                     {playlistId, mediaFileId, toPosition})) {
        rollbackTransaction();
        return false;
    }
    
    return commitTransaction();
}

QSqlQuery DatabaseManager::getPlaylistItems(int playlistId)
{
    const QString query = R"(
        SELECT pi.*, mf.title, mf.artist, mf.album, mf.duration, mf.file_path
        FROM playlist_items pi
        JOIN media_files mf ON pi.media_file_id = mf.id
        WHERE pi.playlist_id = ?
        ORDER BY pi.position
    )";
    
    QSqlQuery sqlQuery = prepareQuery(query);
    sqlQuery.addBindValue(playlistId);
    sqlQuery.exec();
    return sqlQuery;
}

bool DatabaseManager::clearPlaylist(int playlistId)
{
    QMutexLocker locker(&m_mutex);
    return executeQuery("DELETE FROM playlist_items WHERE playlist_id = ?", {playlistId});
}

// Statistics and maintenance
int DatabaseManager::getMediaFileCount()
{
    QSqlQuery query = prepareQuery("SELECT COUNT(*) FROM media_files");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int DatabaseManager::getPlaylistCount()
{
    QSqlQuery query = prepareQuery("SELECT COUNT(*) FROM playlists");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

qint64 DatabaseManager::getTotalLibrarySize()
{
    QSqlQuery query = prepareQuery("SELECT COALESCE(SUM(file_size), 0) FROM media_files");
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

bool DatabaseManager::cleanupOrphanedRecords()
{
    QMutexLocker locker(&m_mutex);
    
    // Remove playlist items that reference non-existent media files
    return executeQuery(R"(
        DELETE FROM playlist_items 
        WHERE media_file_id NOT IN (SELECT id FROM media_files)
    )");
}

bool DatabaseManager::optimizeDatabase()
{
    QMutexLocker locker(&m_mutex);
    
    QStringList optimizeQueries = {
        "VACUUM",
        "ANALYZE",
        "PRAGMA optimize"
    };
    
    for (const QString& query : optimizeQueries) {
        if (!executeQuery(query)) {
            qCWarning(dbManager) << "Failed to execute optimization query:" << query;
        }
    }
    
    return true;
}

// Backup and restore
bool DatabaseManager::createBackup(const QString& backupPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    
    // Simple file copy for SQLite backup
    QFile::remove(backupPath); // Remove existing backup
    if (QFile::copy(m_databasePath, backupPath)) {
        qCInfo(dbManager) << "Database backup created:" << backupPath;
        return true;
    } else {
        m_lastError = "Failed to create backup file";
        return false;
    }
}

bool DatabaseManager::restoreFromBackup(const QString& backupPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (!QFile::exists(backupPath)) {
        m_lastError = "Backup file does not exist";
        return false;
    }
    
    // Close current database
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    // Replace current database with backup
    QFile::remove(m_databasePath);
    if (QFile::copy(backupPath, m_databasePath)) {
        // Reopen database
        if (m_database.open()) {
            qCInfo(dbManager) << "Database restored from backup:" << backupPath;
            return true;
        }
    }
    
    m_lastError = "Failed to restore from backup";
    return false;
}

// Transaction support
bool DatabaseManager::beginTransaction()
{
    return m_database.transaction();
}

bool DatabaseManager::commitTransaction()
{
    return m_database.commit();
}

bool DatabaseManager::rollbackTransaction()
{
    return m_database.rollback();
}

QSqlError DatabaseManager::lastSqlError() const
{
    return m_database.lastError();
}

void DatabaseManager::handleDatabaseError()
{
    QString error = m_database.lastError().text();
    m_lastError = error;
    emit databaseError(error);
    qCCritical(dbManager) << "Database error:" << error;
}

// Migration methods (placeholders for future versions)
bool DatabaseManager::migrateToVersion2()
{
    // Example migration: Add new column
    // return executeQuery("ALTER TABLE media_files ADD COLUMN new_column TEXT");
    return true; // No migration needed for version 2 yet
}

bool DatabaseManager::migrateToVersion3()
{
    // Example migration: Create new table
    return true; // No migration needed for version 3 yet
}

// Utility methods
bool DatabaseManager::executeQuery(const QString& query, const QVariantList& params)
{
    QSqlQuery sqlQuery = prepareQuery(query);
    
    for (const QVariant& param : params) {
        sqlQuery.addBindValue(param);
    }
    
    if (!sqlQuery.exec()) {
        logError("executeQuery", sqlQuery.lastError());
        return false;
    }
    
    return true;
}

QSqlQuery DatabaseManager::prepareQuery(const QString& query)
{
    QSqlQuery sqlQuery(m_database);
    if (!sqlQuery.prepare(query)) {
        logError("prepareQuery", sqlQuery.lastError());
    }
    return sqlQuery;
}

bool DatabaseManager::tableExists(const QString& tableName)
{
    QSqlQuery query = prepareQuery("SELECT name FROM sqlite_master WHERE type='table' AND name=?");
    query.addBindValue(tableName);
    return query.exec() && query.next();
}

QStringList DatabaseManager::getTableColumns(const QString& tableName)
{
    QStringList columns;
    QSqlQuery query = prepareQuery(QString("PRAGMA table_info(%1)").arg(tableName));
    if (query.exec()) {
        while (query.next()) {
            columns << query.value("name").toString();
        }
    }
    return columns;
}

void DatabaseManager::logError(const QString& operation, const QSqlError& error)
{
    m_lastError = QString("%1: %2").arg(operation, error.text());
    qCCritical(dbManager) << m_lastError;
}

} // namespace Data
} // namespace EonPlay
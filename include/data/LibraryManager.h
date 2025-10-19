#pragma once

#include "data/DatabaseManager.h"
#include "data/MediaScanner.h"
#include "data/MetadataExtractor.h"
#include "data/MediaFile.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace EonPlay {
namespace Data {

/**
 * @brief Manages the complete media library for EonPlay
 * 
 * Coordinates database operations, file scanning, metadata extraction,
 * and provides a unified interface for library management.
 */
class LibraryManager : public QObject
{
    Q_OBJECT

public:
    struct LibrarySettings {
        QStringList libraryPaths;
        bool autoScanEnabled = true;
        int autoScanInterval = 3600; // 1 hour in seconds
        bool extractMetadata = true;
        bool fetchWebMetadata = false;
        bool autoCleanup = true;
        MediaScanner::ScanOptions scanOptions;
        MetadataExtractor::ExtractionOptions extractionOptions;
    };

    struct LibraryStatistics {
        int totalFiles = 0;
        int audioFiles = 0;
        int videoFiles = 0;
        qint64 totalSize = 0;
        int totalPlaylists = 0;
        QStringList topGenres;
        QStringList topArtists;
        QString lastScanTime;
        QString libraryCreated;
    };

    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() = default;

    // Initialization
    bool initialize(const QString& databasePath);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Configuration
    void setLibrarySettings(const LibrarySettings& settings);
    LibrarySettings librarySettings() const { return m_settings; }

    // Component access
    DatabaseManager* databaseManager() const { return m_dbManager.get(); }
    MediaScanner* mediaScanner() const { return m_scanner.get(); }
    MetadataExtractor* metadataExtractor() const { return m_extractor.get(); }

    // Library management
    void addLibraryPath(const QString& path);
    void removeLibraryPath(const QString& path);
    QStringList libraryPaths() const { return m_settings.libraryPaths; }

    // Scanning operations
    void scanLibrary();
    void scanPath(const QString& path);
    void rescanLibrary();
    void quickScan();

    // File operations
    void addFile(const QString& filePath);
    void removeFile(const QString& filePath);
    void updateFile(const QString& filePath);
    bool containsFile(const QString& filePath);

    // Search and filtering
    QList<MediaFile> searchFiles(const QString& query);
    QList<MediaFile> getFilesByArtist(const QString& artist);
    QList<MediaFile> getFilesByAlbum(const QString& album);
    QList<MediaFile> getFilesByGenre(const QString& genre);
    QList<MediaFile> getRecentFiles(int limit = 50);
    QList<MediaFile> getMostPlayedFiles(int limit = 50);

    // Library maintenance
    void cleanupLibrary();
    void optimizeLibrary();
    void rebuildLibrary();
    QList<QStringList> findDuplicates();
    void removeDuplicates(const QStringList& filesToRemove);

    // Statistics
    LibraryStatistics getStatistics();
    QStringList getArtists();
    QStringList getAlbums();
    QStringList getGenres();

    // Auto-scan management
    void enableAutoScan(bool enabled);
    bool isAutoScanEnabled() const { return m_autoScanEnabled; }
    void setAutoScanInterval(int seconds);

    // Import/Export
    bool exportLibrary(const QString& filePath, const QString& format = "json");
    bool importLibrary(const QString& filePath);

    // Status
    bool isScanning() const;
    bool isBusy() const;
    QString currentOperation() const { return m_currentOperation; }

signals:
    void libraryInitialized();
    void libraryShutdown();
    
    void scanStarted(const QString& path);
    void scanProgress(int percentage, const QString& status);
    void scanCompleted(int addedFiles, int updatedFiles);
    void scanFailed(const QString& error);
    
    void fileAdded(const QString& filePath);
    void fileUpdated(const QString& filePath);
    void fileRemoved(const QString& filePath);
    
    void libraryChanged();
    void statisticsUpdated();
    
    void operationStarted(const QString& operation);
    void operationCompleted(const QString& operation);
    void operationFailed(const QString& operation, const QString& error);

private slots:
    // Scanner slots
    void onScanStarted(const QString& directory);
    void onScanProgress(const MediaScanner::ScanProgress& progress);
    void onScanCompleted(int addedFiles, int updatedFiles, int errorFiles);
    void onScanError(const QString& error);
    void onFileAdded(const QString& filePath);
    void onFileUpdated(const QString& filePath);
    void onFileRemoved(const QString& filePath);

    // Metadata extractor slots
    void onMetadataExtracted(const QString& filePath, const MetadataExtractor::MediaMetadata& metadata);
    void onMetadataExtractionFailed(const QString& filePath, const QString& error);

    // Auto-scan timer
    void performAutoScan();

private:
    // Initialization helpers
    void setupComponents();
    void connectSignals();
    void loadSettings();
    void saveSettings();

    // Library operations
    void updateFileWithMetadata(const QString& filePath, const MetadataExtractor::MediaMetadata& metadata);
    MediaFile createMediaFileFromMetadata(const QString& filePath, const MetadataExtractor::MediaMetadata& metadata);

    // Statistics helpers
    void updateStatistics();
    void cacheStatistics();

    // Member variables
    bool m_initialized;
    LibrarySettings m_settings;
    QString m_currentOperation;
    
    // Components
    std::unique_ptr<DatabaseManager> m_dbManager;
    std::unique_ptr<MediaScanner> m_scanner;
    std::unique_ptr<MetadataExtractor> m_extractor;
    
    // Auto-scan
    bool m_autoScanEnabled;
    QTimer* m_autoScanTimer;
    
    // Statistics cache
    LibraryStatistics m_cachedStatistics;
    QDateTime m_statisticsCacheTime;
    bool m_statisticsValid;
};

} // namespace Data
} // namespace EonPlay
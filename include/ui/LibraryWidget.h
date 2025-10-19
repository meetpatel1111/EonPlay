#ifndef LIBRARYWIDGET_H
#define LIBRARYWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QListView>
#include <QTableView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QTimer>
#include <QMutex>
#include <memory>

// Include required classes
#include "data/LibraryManager.h"
#include "data/MediaFile.h"
#include "data/PlaylistManager.h"

// Bring namespaced classes into scope
using EonPlay::Data::LibraryManager;
using EonPlay::Data::MediaFile;
using EonPlay::Data::PlaylistManager;

/**
 * @brief Comprehensive media library browser widget
 * 
 * Provides a complete media library interface with tree view, search,
 * filtering, album art display, and library management features.
 */
class LibraryWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Library view modes
     */
    enum ViewMode {
        TreeView = 0,
        ListView,
        GridView,
        DetailView
    };

    /**
     * @brief Library sort options
     */
    enum SortBy {
        Title = 0,
        Artist,
        Album,
        Genre,
        Year,
        Duration,
        DateAdded,
        PlayCount,
        Rating
    };

    /**
     * @brief Library filter options
     */
    enum FilterBy {
        All = 0,
        Audio,
        Video,
        Playlist,
        Recent,
        Favorites,
        Unplayed
    };

    /**
     * @brief Library statistics
     */
    struct LibraryStats {
        int totalFiles;
        int audioFiles;
        int videoFiles;
        int playlists;
        qint64 totalSize;        // Total size in bytes
        qint64 totalDuration;    // Total duration in milliseconds
        QString mostPlayedArtist;
        QString mostPlayedGenre;
        int totalPlayCount;
        
        LibraryStats() : totalFiles(0), audioFiles(0), videoFiles(0), playlists(0),
                        totalSize(0), totalDuration(0), totalPlayCount(0) {}
    };

    explicit LibraryWidget(QWidget* parent = nullptr);
    ~LibraryWidget() override;

    /**
     * @brief Initialize the widget with managers
     * @param libraryManager Library manager instance
     * @param playlistManager Playlist manager instance
     */
    void initialize(LibraryManager* libraryManager, PlaylistManager* playlistManager);

    /**
     * @brief Set library manager
     * @param libraryManager Library manager instance
     */
    void setLibraryManager(LibraryManager* libraryManager);

    /**
     * @brief Set playlist manager
     * @param playlistManager Playlist manager instance
     */
    void setPlaylistManager(PlaylistManager* playlistManager);

    /**
     * @brief Get current view mode
     * @return Current view mode
     */
    ViewMode getViewMode() const;

    /**
     * @brief Set view mode
     * @param mode View mode
     */
    void setViewMode(ViewMode mode);

    /**
     * @brief Get current sort option
     * @return Current sort option
     */
    SortBy getSortBy() const;

    /**
     * @brief Set sort option
     * @param sortBy Sort option
     */
    void setSortBy(SortBy sortBy);

    /**
     * @brief Get current filter option
     * @return Current filter option
     */
    FilterBy getFilterBy() const;

    /**
     * @brief Set filter option
     * @param filterBy Filter option
     */
    void setFilterBy(FilterBy filterBy);

    /**
     * @brief Get search text
     * @return Current search text
     */
    QString getSearchText() const;

    /**
     * @brief Set search text
     * @param searchText Search text
     */
    void setSearchText(const QString& searchText);

    /**
     * @brief Get selected media files
     * @return List of selected media files
     */
    QVector<std::shared_ptr<MediaFile>> getSelectedFiles() const;

    /**
     * @brief Get library statistics
     * @return Library statistics
     */
    LibraryStats getLibraryStats() const;

    /**
     * @brief Refresh library display
     */
    void refreshLibrary();

    /**
     * @brief Export library to file
     * @param filePath Export file path
     * @param format Export format (json, csv)
     * @return true if successful
     */
    bool exportLibrary(const QString& filePath, const QString& format);

    /**
     * @brief Show library statistics dialog
     */
    void showLibraryStats();

    /**
     * @brief Enable or disable album art display
     * @param enabled Enable state
     */
    void setAlbumArtEnabled(bool enabled);

    /**
     * @brief Check if album art display is enabled
     * @return true if enabled
     */
    bool isAlbumArtEnabled() const;

signals:
    /**
     * @brief Emitted when play is requested for a file
     * @param filePath Path to the media file
     */
    void playRequested(const QString& filePath);

    /**
     * @brief Emitted when queue is requested for a file
     * @param filePath Path to the media file
     */
    void queueRequested(const QString& filePath);

    /**
     * @brief Emitted when files are selected for playback
     * @param files Selected media files
     */
    void filesSelectedForPlayback(const QVector<std::shared_ptr<MediaFile>>& files);

    /**
     * @brief Emitted when files are added to playlist
     * @param files Media files to add
     * @param playlistName Target playlist name
     */
    void filesAddedToPlaylist(const QVector<std::shared_ptr<MediaFile>>& files, const QString& playlistName);

    /**
     * @brief Emitted when library export completes
     * @param success Export success
     * @param filePath Export file path
     */
    void libraryExported(bool success, const QString& filePath);

    /**
     * @brief Emitted when view mode changes
     * @param mode New view mode
     */
    void viewModeChanged(ViewMode mode);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onSearchTextChanged();
    void onViewModeChanged();
    void onSortByChanged();
    void onFilterByChanged();
    void onItemDoubleClicked(const QModelIndex& index);
    void onItemSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);
    void onLibraryUpdated();
    void updateLibraryStats();
    void onAlbumArtLoaded(const QString& filePath, const QPixmap& albumArt);

private:
    void setupUI();
    void setupModels();
    void setupConnections();
    void updateViewMode();
    void updateSorting();
    void updateFiltering();
    void populateLibraryModel();
    void createContextMenu();
    void playSelectedFiles();
    void addToPlaylist();
    void showFileProperties();
    void removeFromLibrary();
    bool exportToJSON(const QString& filePath);
    bool exportToCSV(const QString& filePath);
    void loadAlbumArt(const QString& filePath);
    QString formatDuration(qint64 milliseconds) const;
    QString formatFileSize(qint64 bytes) const;

    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QHBoxLayout* m_searchLayout;
    QSplitter* m_mainSplitter;
    QSplitter* m_contentSplitter;

    // Toolbar
    QComboBox* m_viewModeCombo;
    QComboBox* m_sortByCombo;
    QComboBox* m_filterByCombo;
    QPushButton* m_refreshButton;
    QPushButton* m_exportButton;
    QPushButton* m_statsButton;

    // Search
    QLineEdit* m_searchEdit;
    QLabel* m_searchLabel;

    // Views
    QTreeView* m_treeView;
    QListView* m_listView;
    QTableView* m_tableView;
    QWidget* m_currentView;

    // Album art display
    QLabel* m_albumArtLabel;
    QWidget* m_albumArtWidget;
    bool m_albumArtEnabled;

    // Status
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    // Models
    QStandardItemModel* m_libraryModel;
    QSortFilterProxyModel* m_proxyModel;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_playAction;
    QAction* m_addToPlaylistAction;
    QAction* m_propertiesAction;
    QAction* m_removeAction;

    // Managers
    LibraryManager* m_libraryManager;
    PlaylistManager* m_playlistManager;

    // State
    ViewMode m_viewMode;
    SortBy m_sortBy;
    FilterBy m_filterBy;
    QString m_searchText;
    LibraryStats m_libraryStats;

    // Album art cache
    QHash<QString, QPixmap> m_albumArtCache;
    QTimer* m_albumArtTimer;

    // Thread safety
    mutable QMutex m_dataMutex;

    // Constants
    static constexpr int ALBUM_ART_SIZE = 200;
    static constexpr int THUMBNAIL_SIZE = 64;
    static constexpr int MAX_ALBUM_ART_CACHE = 100;
};

#endif // LIBRARYWIDGET_H
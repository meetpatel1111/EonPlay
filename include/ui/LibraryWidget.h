#ifndef LIBRARYWIDGET_H
#define LIBRARYWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QListView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QTimer>
#include <memory>

class LibraryManager;
class PlaylistManager;
class MediaFile;
class Playlist;

/**
 * @brief Widget for browsing and managing the media library
 * 
 * Provides a comprehensive interface for viewing, searching, and organizing
 * media files in the library with playlist management capabilities.
 */
class LibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryWidget(QWidget* parent = nullptr);
    ~LibraryWidget() override;

    /**
     * @brief Initialize the library widget with managers
     * @param libraryManager Library manager instance
     * @param playlistManager Playlist manager instance
     */
    void initialize(std::shared_ptr<LibraryManager> libraryManager,
                   std::shared_ptr<PlaylistManager> playlistManager);

    /**
     * @brief Refresh the library display
     */
    void refreshLibrary();

    /**
     * @brief Get currently selected media files
     * @return List of selected media file paths
     */
    QStringList getSelectedFiles() const;

    /**
     * @brief Set the current search filter
     * @param filter Search text to filter by
     */
    void setSearchFilter(const QString& filter);

public slots:
    /**
     * @brief Handle library scan progress updates
     * @param current Current file being scanned
     * @param total Total files to scan
     */
    void onScanProgress(int current, int total);

    /**
     * @brief Handle library scan completion
     */
    void onScanComplete();

    /**
     * @brief Handle new media file added to library
     * @param filePath Path of the added file
     */
    void onMediaFileAdded(const QString& filePath);

    /**
     * @brief Handle media file removed from library
     * @param filePath Path of the removed file
     */
    void onMediaFileRemoved(const QString& filePath);

signals:
    /**
     * @brief Emitted when user wants to play selected files
     * @param filePaths List of file paths to play
     */
    void playRequested(const QStringList& filePaths);

    /**
     * @brief Emitted when user wants to add files to queue
     * @param filePaths List of file paths to queue
     */
    void queueRequested(const QStringList& filePaths);

    /**
     * @brief Emitted when user wants to add files to playlist
     * @param filePaths List of file paths
     * @param playlistName Name of target playlist
     */
    void addToPlaylistRequested(const QStringList& filePaths, const QString& playlistName);

    /**
     * @brief Emitted when user wants to create new playlist
     * @param name Playlist name
     * @param filePaths Initial files for playlist
     */
    void createPlaylistRequested(const QString& name, const QStringList& filePaths);

private slots:
    void onSearchTextChanged();
    void onFilterChanged();
    void onViewModeChanged();
    void onItemDoubleClicked(const QModelIndex& index);
    void onItemSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);
    void onRefreshRequested();
    void onScanLibraryRequested();
    void onExportLibraryRequested();
    void onShowStatisticsRequested();

    // Context menu actions
    void onPlaySelected();
    void onQueueSelected();
    void onAddToPlaylist();
    void onCreatePlaylist();
    void onShowFileInfo();
    void onRemoveFromLibrary();
    void onShowInExplorer();

private:
    void setupUI();
    void setupToolbar();
    void setupLibraryView();
    void setupPlaylistView();
    void setupContextMenus();
    void setupConnections();
    
    void populateLibraryModel();
    void populatePlaylistModel();
    void updateStatistics();
    void showFileInfoDialog(const QString& filePath);
    void exportLibraryToFile();
    void showLibraryStatistics();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QSplitter* m_mainSplitter;
    QSplitter* m_librarySplitter;

    // Toolbar
    QLineEdit* m_searchEdit;
    QComboBox* m_filterCombo;
    QComboBox* m_viewModeCombo;
    QPushButton* m_refreshButton;
    QPushButton* m_scanButton;
    QPushButton* m_exportButton;
    QPushButton* m_statsButton;
    QProgressBar* m_scanProgress;
    QLabel* m_statusLabel;

    // Library view
    QTreeView* m_libraryTreeView;
    QListView* m_libraryListView;
    QStandardItemModel* m_libraryModel;
    QSortFilterProxyModel* m_libraryProxyModel;

    // Playlist view
    QTreeView* m_playlistView;
    QStandardItemModel* m_playlistModel;

    // Context menus
    QMenu* m_libraryContextMenu;
    QMenu* m_playlistContextMenu;

    // Actions
    QAction* m_playAction;
    QAction* m_queueAction;
    QAction* m_addToPlaylistAction;
    QAction* m_createPlaylistAction;
    QAction* m_showInfoAction;
    QAction* m_removeAction;
    QAction* m_showInExplorerAction;

    // Managers
    std::shared_ptr<LibraryManager> m_libraryManager;
    std::shared_ptr<PlaylistManager> m_playlistManager;

    // State
    bool m_isScanning;
    QString m_currentFilter;
    int m_totalFiles;
    QTimer* m_refreshTimer;

    enum ViewMode {
        TreeView,
        ListView
    };
    ViewMode m_currentViewMode;

    enum FilterType {
        AllFiles,
        AudioFiles,
        VideoFiles,
        RecentFiles,
        FavoriteFiles
    };
};

#endif // LIBRARYWIDGET_H
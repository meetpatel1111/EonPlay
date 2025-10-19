#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QMutex>
#include <memory>

// Forward declarations
class PlaylistManager;
class Playlist;
class MediaFile;

/**
 * @brief Comprehensive playlist management widget
 * 
 * Provides a complete playlist interface with drag-and-drop support,
 * playlist creation, editing, and advanced playlist features.
 */
class PlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Playlist display modes
     */
    enum DisplayMode {
        ListMode = 0,
        TreeMode,
        CompactMode
    };

    /**
     * @brief Playlist sort options
     */
    enum SortBy {
        Default = 0,
        Title,
        Artist,
        Album,
        Duration,
        DateAdded
    };

    explicit PlaylistWidget(QWidget* parent = nullptr);
    ~PlaylistWidget() override;

    /**
     * @brief Set playlist manager
     * @param playlistManager Playlist manager instance
     */
    void setPlaylistManager(PlaylistManager* playlistManager);

    /**
     * @brief Get current display mode
     * @return Current display mode
     */
    DisplayMode getDisplayMode() const;

    /**
     * @brief Set display mode
     * @param mode Display mode
     */
    void setDisplayMode(DisplayMode mode);

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
     * @brief Get current playlist
     * @return Current playlist
     */
    std::shared_ptr<Playlist> getCurrentPlaylist() const;

    /**
     * @brief Set current playlist
     * @param playlist Playlist to display
     */
    void setCurrentPlaylist(std::shared_ptr<Playlist> playlist);

    /**
     * @brief Get selected media files
     * @return List of selected media files
     */
    QVector<std::shared_ptr<MediaFile>> getSelectedFiles() const;

    /**
     * @brief Add files to current playlist
     * @param files Media files to add
     */
    void addFiles(const QVector<std::shared_ptr<MediaFile>>& files);

    /**
     * @brief Remove selected files from playlist
     */
    void removeSelectedFiles();

    /**
     * @brief Clear current playlist
     */
    void clearPlaylist();

    /**
     * @brief Shuffle current playlist
     */
    void shufflePlaylist();

    /**
     * @brief Create new playlist
     * @param name Playlist name
     * @return true if successful
     */
    bool createNewPlaylist(const QString& name);

    /**
     * @brief Delete current playlist
     * @return true if successful
     */
    bool deleteCurrentPlaylist();

    /**
     * @brief Rename current playlist
     * @param newName New playlist name
     * @return true if successful
     */
    bool renameCurrentPlaylist(const QString& newName);

    /**
     * @brief Save current playlist to file
     * @param filePath File path to save to
     * @return true if successful
     */
    bool savePlaylistToFile(const QString& filePath);

    /**
     * @brief Load playlist from file
     * @param filePath File path to load from
     * @return true if successful
     */
    bool loadPlaylistFromFile(const QString& filePath);

    /**
     * @brief Get playlist statistics
     * @return Playlist statistics
     */
    struct PlaylistStats {
        int totalTracks;
        qint64 totalDuration;
        qint64 totalSize;
        QString longestTrack;
        QString shortestTrack;
        
        PlaylistStats() : totalTracks(0), totalDuration(0), totalSize(0) {}
    };

    /**
     * @brief Get current playlist statistics
     * @return Playlist statistics
     */
    PlaylistStats getPlaylistStats() const;

signals:
    /**
     * @brief Emitted when files are selected for playback
     * @param files Selected media files
     */
    void filesSelectedForPlayback(const QVector<std::shared_ptr<MediaFile>>& files);

    /**
     * @brief Emitted when playlist changes
     * @param playlist New current playlist
     */
    void playlistChanged(std::shared_ptr<Playlist> playlist);

    /**
     * @brief Emitted when playlist is modified
     * @param playlist Modified playlist
     */
    void playlistModified(std::shared_ptr<Playlist> playlist);

    /**
     * @brief Emitted when new playlist is created
     * @param playlist New playlist
     */
    void playlistCreated(std::shared_ptr<Playlist> playlist);

    /**
     * @brief Emitted when playlist is deleted
     * @param playlistName Deleted playlist name
     */
    void playlistDeleted(const QString& playlistName);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onItemSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);
    void onDisplayModeChanged();
    void onSortByChanged();
    void onNewPlaylistClicked();
    void onDeletePlaylistClicked();
    void onSavePlaylistClicked();
    void onLoadPlaylistClicked();
    void onShuffleClicked();
    void onClearClicked();
    void updatePlaylistDisplay();

private:
    void setupUI();
    void setupConnections();
    void createContextMenu();
    void populatePlaylistView();
    void updatePlaylistStats();
    void moveSelectedItems(int direction);
    QString formatDuration(qint64 milliseconds) const;
    QString formatFileSize(qint64 bytes) const;

    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QHBoxLayout* m_controlsLayout;

    // Toolbar
    QComboBox* m_displayModeCombo;
    QComboBox* m_sortByCombo;
    QPushButton* m_newPlaylistButton;
    QPushButton* m_deletePlaylistButton;
    QPushButton* m_savePlaylistButton;
    QPushButton* m_loadPlaylistButton;

    // Controls
    QPushButton* m_shuffleButton;
    QPushButton* m_clearButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;

    // Views
    QListWidget* m_listWidget;
    QTreeWidget* m_treeWidget;
    QWidget* m_currentView;

    // Status
    QLabel* m_statusLabel;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_playAction;
    QAction* m_removeAction;
    QAction* m_moveUpAction;
    QAction* m_moveDownAction;
    QAction* m_propertiesAction;

    // Manager
    PlaylistManager* m_playlistManager;

    // State
    std::shared_ptr<Playlist> m_currentPlaylist;
    DisplayMode m_displayMode;
    SortBy m_sortBy;
    PlaylistStats m_playlistStats;

    // Thread safety
    mutable QMutex m_dataMutex;
};

#endif // PLAYLISTWIDGET_H
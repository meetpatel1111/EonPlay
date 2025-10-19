#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include <QWidget>
#include <QListView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <memory>

class PlaylistManager;
class Playlist;

/**
 * @brief Widget for displaying and managing playlist contents
 * 
 * Shows the current playlist with drag-and-drop support for reordering
 * and provides controls for playlist management.
 */
class PlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(QWidget* parent = nullptr);
    ~PlaylistWidget() override;

    /**
     * @brief Initialize the playlist widget with manager
     * @param playlistManager Playlist manager instance
     */
    void initialize(std::shared_ptr<PlaylistManager> playlistManager);

    /**
     * @brief Set the current playlist to display
     * @param playlistId ID of playlist to display
     */
    void setCurrentPlaylist(int playlistId);

    /**
     * @brief Get the current playlist ID
     * @return Current playlist ID, -1 if none
     */
    int getCurrentPlaylistId() const;

    /**
     * @brief Add files to the current playlist
     * @param filePaths List of file paths to add
     */
    void addFiles(const QStringList& filePaths);

    /**
     * @brief Get currently selected files
     * @return List of selected file paths
     */
    QStringList getSelectedFiles() const;

    /**
     * @brief Set the currently playing item
     * @param filePath Path of currently playing file
     */
    void setCurrentlyPlaying(const QString& filePath);

public slots:
    /**
     * @brief Handle playlist updated
     * @param playlistId ID of updated playlist
     */
    void onPlaylistUpdated(int playlistId);

    /**
     * @brief Handle playlist item added
     * @param playlistId Playlist ID
     * @param filePath Added file path
     */
    void onPlaylistItemAdded(int playlistId, const QString& filePath);

    /**
     * @brief Handle playlist item removed
     * @param playlistId Playlist ID
     * @param filePath Removed file path
     */
    void onPlaylistItemRemoved(int playlistId, const QString& filePath);

signals:
    /**
     * @brief Emitted when user wants to play selected item
     * @param filePath File path to play
     */
    void playRequested(const QString& filePath);

    /**
     * @brief Emitted when user wants to play from specific position
     * @param filePaths List of files from position onwards
     * @param startIndex Index to start playing from
     */
    void playFromRequested(const QStringList& filePaths, int startIndex);

    /**
     * @brief Emitted when user removes items from playlist
     * @param filePaths List of file paths to remove
     */
    void removeFromPlaylistRequested(const QStringList& filePaths);

    /**
     * @brief Emitted when playlist order changes
     * @param playlistId Playlist ID
     * @param newOrder New order of file paths
     */
    void playlistReordered(int playlistId, const QStringList& newOrder);

private slots:
    void onItemDoubleClicked(const QModelIndex& index);
    void onItemSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);
    void onClearPlaylist();
    void onShufflePlaylist();
    void onSavePlaylist();
    void onLoadPlaylist();

    // Context menu actions
    void onPlaySelected();
    void onPlayFromHere();
    void onRemoveSelected();
    void onMoveUp();
    void onMoveDown();
    void onShowFileInfo();

private:
    void setupUI();
    void setupContextMenu();
    void setupConnections();
    void populatePlaylist();
    void updatePlaylistInfo();
    void moveSelectedItems(int direction);

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_headerLayout;
    QHBoxLayout* m_buttonLayout;

    QLabel* m_playlistLabel;
    QLabel* m_infoLabel;
    QListView* m_playlistView;
    QStandardItemModel* m_playlistModel;

    // Buttons
    QPushButton* m_clearButton;
    QPushButton* m_shuffleButton;
    QPushButton* m_saveButton;
    QPushButton* m_loadButton;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_playAction;
    QAction* m_playFromAction;
    QAction* m_removeAction;
    QAction* m_moveUpAction;
    QAction* m_moveDownAction;
    QAction* m_showInfoAction;

    // Manager
    std::shared_ptr<PlaylistManager> m_playlistManager;

    // State
    int m_currentPlaylistId;
    QString m_currentlyPlayingFile;
    QTimer* m_updateTimer;
};

#endif // PLAYLISTWIDGET_H
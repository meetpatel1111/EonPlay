#include "ui/PlaylistWidget.h"
#include "data/PlaylistManager.h"
#include "data/Playlist.h"
#include "data/MediaFile.h"
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QListWidgetItem>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(playlistWidget)
Q_LOGGING_CATEGORY(playlistWidget, "ui.playlist")

PlaylistWidget::PlaylistWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_toolbarLayout(new QHBoxLayout())
    , m_controlsLayout(new QHBoxLayout())
    , m_displayModeCombo(new QComboBox(this))
    , m_sortByCombo(new QComboBox(this))
    , m_newPlaylistButton(new QPushButton("New", this))
    , m_deletePlaylistButton(new QPushButton("Delete", this))
    , m_savePlaylistButton(new QPushButton("Save", this))
    , m_loadPlaylistButton(new QPushButton("Load", this))
    , m_shuffleButton(new QPushButton("Shuffle", this))
    , m_clearButton(new QPushButton("Clear", this))
    , m_moveUpButton(new QPushButton("↑", this))
    , m_moveDownButton(new QPushButton("↓", this))
    , m_listWidget(new QListWidget(this))
    , m_treeWidget(new QTreeWidget(this))
    , m_currentView(nullptr)
    , m_statusLabel(new QLabel(this))
    , m_contextMenu(new QMenu(this))
    , m_playlistManager(nullptr)
    , m_displayMode(ListMode)
    , m_sortBy(Default)
{
    setupUI();
    setupConnections();
    createContextMenu();
    
    qCDebug(playlistWidget) << "PlaylistWidget created";
}

PlaylistWidget::~PlaylistWidget()
{
    qCDebug(playlistWidget) << "PlaylistWidget destroyed";
}

void PlaylistWidget::initialize(PlaylistManager* playlistManager)
{
    setPlaylistManager(playlistManager);
}

void PlaylistWidget::setPlaylistManager(PlaylistManager* playlistManager)
{
    if (m_playlistManager) {
        disconnect(m_playlistManager, nullptr, this, nullptr);
    }
    
    m_playlistManager = playlistManager;
    
    if (m_playlistManager) {
        // Connect to playlist manager signals
        // connect(m_playlistManager, &PlaylistManager::playlistUpdated,
        //         this, &PlaylistWidget::updatePlaylistDisplay);
    }
}

PlaylistWidget::DisplayMode PlaylistWidget::getDisplayMode() const
{
    return m_displayMode;
}

void PlaylistWidget::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode != mode) {
        m_displayMode = mode;
        m_displayModeCombo->setCurrentIndex(static_cast<int>(mode));
        updatePlaylistDisplay();
    }
}

PlaylistWidget::SortBy PlaylistWidget::getSortBy() const
{
    return m_sortBy;
}

void PlaylistWidget::setSortBy(SortBy sortBy)
{
    if (m_sortBy != sortBy) {
        m_sortBy = sortBy;
        m_sortByCombo->setCurrentIndex(static_cast<int>(sortBy));
        updatePlaylistDisplay();
    }
}

std::shared_ptr<Playlist> PlaylistWidget::getCurrentPlaylist() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_currentPlaylist;
}

void PlaylistWidget::setCurrentPlaylist(std::shared_ptr<Playlist> playlist)
{
    QMutexLocker locker(&m_dataMutex);
    
    if (m_currentPlaylist != playlist) {
        m_currentPlaylist = playlist;
        locker.unlock();
        
        populatePlaylistView();
        updatePlaylistStats();
        emit playlistChanged(m_currentPlaylist);
    }
}

QVector<std::shared_ptr<MediaFile>> PlaylistWidget::getSelectedFiles() const
{
    QVector<std::shared_ptr<MediaFile>> selectedFiles;
    
    if (m_displayMode == ListMode) {
        QList<QListWidgetItem*> selectedItems = m_listWidget->selectedItems();
        for (QListWidgetItem* item : selectedItems) {
            QVariant fileData = item->data(Qt::UserRole);
            if (fileData.isValid()) {
                // Extract MediaFile from item data
                // Implementation depends on how MediaFile is stored
                qCDebug(playlistWidget) << "Selected file:" << item->text();
            }
        }
    } else if (m_displayMode == TreeMode) {
        QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
        for (QTreeWidgetItem* item : selectedItems) {
            QVariant fileData = item->data(0, Qt::UserRole);
            if (fileData.isValid()) {
                // Extract MediaFile from item data
                qCDebug(playlistWidget) << "Selected file:" << item->text(0);
            }
        }
    }
    
    return selectedFiles;
}

void PlaylistWidget::addFiles(const QVector<std::shared_ptr<MediaFile>>& files)
{
    if (!m_currentPlaylist || files.isEmpty()) {
        return;
    }
    
    // Add files to current playlist
    for (auto file : files) {
        // m_currentPlaylist->addMediaFile(file);
    }
    
    populatePlaylistView();
    updatePlaylistStats();
    emit playlistModified(m_currentPlaylist);
    
    qCDebug(playlistWidget) << "Added" << files.size() << "files to playlist";
}

void PlaylistWidget::removeSelectedFiles()
{
    if (!m_currentPlaylist) {
        return;
    }
    
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (selectedFiles.isEmpty()) {
        return;
    }
    
    int ret = QMessageBox::question(this, "Remove Files", 
                                   QString("Remove %1 files from playlist?").arg(selectedFiles.size()),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // Remove files from playlist
        for (auto file : selectedFiles) {
            // m_currentPlaylist->removeMediaFile(file);
        }
        
        populatePlaylistView();
        updatePlaylistStats();
        emit playlistModified(m_currentPlaylist);
        
        qCDebug(playlistWidget) << "Removed" << selectedFiles.size() << "files from playlist";
    }
}

void PlaylistWidget::clearPlaylist()
{
    if (!m_currentPlaylist) {
        return;
    }
    
    int ret = QMessageBox::question(this, "Clear Playlist", 
                                   "Clear all files from playlist?",
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // m_currentPlaylist->clear();
        
        populatePlaylistView();
        updatePlaylistStats();
        emit playlistModified(m_currentPlaylist);
        
        qCDebug(playlistWidget) << "Playlist cleared";
    }
}

void PlaylistWidget::shufflePlaylist()
{
    if (!m_currentPlaylist) {
        return;
    }
    
    // m_currentPlaylist->shuffle();
    
    populatePlaylistView();
    emit playlistModified(m_currentPlaylist);
    
    qCDebug(playlistWidget) << "Playlist shuffled";
}

bool PlaylistWidget::createNewPlaylist(const QString& name)
{
    if (!m_playlistManager) {
        return false;
    }
    
    QString playlistName = name;
    if (playlistName.isEmpty()) {
        bool ok;
        playlistName = QInputDialog::getText(this, "New Playlist", 
                                           "Playlist name:", QLineEdit::Normal, 
                                           "New Playlist", &ok);
        if (!ok || playlistName.isEmpty()) {
            return false;
        }
    }
    
    // Create new playlist
    // auto newPlaylist = m_playlistManager->createPlaylist(playlistName);
    // if (newPlaylist) {
    //     setCurrentPlaylist(newPlaylist);
    //     emit playlistCreated(newPlaylist);
    //     qCDebug(playlistWidget) << "Created new playlist:" << playlistName;
    //     return true;
    // }
    
    return false;
}

bool PlaylistWidget::deleteCurrentPlaylist()
{
    if (!m_currentPlaylist || !m_playlistManager) {
        return false;
    }
    
    QString playlistName = "Current Playlist"; // m_currentPlaylist->getName();
    
    int ret = QMessageBox::question(this, "Delete Playlist", 
                                   QString("Delete playlist '%1'?").arg(playlistName),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // bool success = m_playlistManager->deletePlaylist(m_currentPlaylist->getId());
        // if (success) {
        //     emit playlistDeleted(playlistName);
        //     setCurrentPlaylist(nullptr);
        //     qCDebug(playlistWidget) << "Deleted playlist:" << playlistName;
        //     return true;
        // }
    }
    
    return false;
}

bool PlaylistWidget::renameCurrentPlaylist(const QString& newName)
{
    if (!m_currentPlaylist) {
        return false;
    }
    
    QString name = newName;
    if (name.isEmpty()) {
        bool ok;
        name = QInputDialog::getText(this, "Rename Playlist", 
                                   "New name:", QLineEdit::Normal, 
                                   "Current Playlist", &ok); // m_currentPlaylist->getName()
        if (!ok || name.isEmpty()) {
            return false;
        }
    }
    
    // m_currentPlaylist->setName(name);
    emit playlistModified(m_currentPlaylist);
    
    qCDebug(playlistWidget) << "Renamed playlist to:" << name;
    return true;
}

bool PlaylistWidget::savePlaylistToFile(const QString& filePath)
{
    if (!m_currentPlaylist) {
        return false;
    }
    
    QString path = filePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Playlist", 
                                          QString(), "M3U Playlist (*.m3u);;PLS Playlist (*.pls)");
        if (path.isEmpty()) {
            return false;
        }
    }
    
    // Save playlist to file
    // bool success = m_currentPlaylist->saveToFile(path);
    bool success = false; // Placeholder
    
    if (success) {
        qCDebug(playlistWidget) << "Playlist saved to:" << path;
    } else {
        QMessageBox::warning(this, "Save Error", "Failed to save playlist to file.");
    }
    
    return success;
}

bool PlaylistWidget::loadPlaylistFromFile(const QString& filePath)
{
    QString path = filePath;
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, "Load Playlist", 
                                          QString(), "Playlist Files (*.m3u *.pls *.xspf)");
        if (path.isEmpty()) {
            return false;
        }
    }
    
    if (!m_playlistManager) {
        return false;
    }
    
    // Load playlist from file
    // auto playlist = m_playlistManager->loadPlaylistFromFile(path);
    // if (playlist) {
    //     setCurrentPlaylist(playlist);
    //     emit playlistCreated(playlist);
    //     qCDebug(playlistWidget) << "Playlist loaded from:" << path;
    //     return true;
    // }
    
    QMessageBox::warning(this, "Load Error", "Failed to load playlist from file.");
    return false;
}

PlaylistWidget::PlaylistStats PlaylistWidget::getPlaylistStats() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_playlistStats;
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_currentView && m_currentView->geometry().contains(event->pos())) {
        m_contextMenu->exec(event->globalPos());
    }
}

void PlaylistWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void PlaylistWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void PlaylistWidget::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        QVector<std::shared_ptr<MediaFile>> files;
        
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                // Create MediaFile from path and add to files vector
                // auto mediaFile = std::make_shared<MediaFile>(filePath);
                // files.append(mediaFile);
            }
        }
        
        if (!files.isEmpty()) {
            addFiles(files);
        }
        
        event->acceptProposedAction();
    }
}

void PlaylistWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item)
    
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        emit filesSelectedForPlayback(selectedFiles);
    }
}

void PlaylistWidget::onItemSelectionChanged()
{
    // Update button states based on selection
    bool hasSelection = !getSelectedFiles().isEmpty();
    
    m_moveUpButton->setEnabled(hasSelection);
    m_moveDownButton->setEnabled(hasSelection);
}

void PlaylistWidget::onContextMenuRequested(const QPoint& pos)
{
    Q_UNUSED(pos)
    // Context menu is handled in contextMenuEvent
}

void PlaylistWidget::onDisplayModeChanged()
{
    DisplayMode newMode = static_cast<DisplayMode>(m_displayModeCombo->currentIndex());
    setDisplayMode(newMode);
}

void PlaylistWidget::onSortByChanged()
{
    SortBy newSortBy = static_cast<SortBy>(m_sortByCombo->currentIndex());
    setSortBy(newSortBy);
}

void PlaylistWidget::onNewPlaylistClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Playlist", "Playlist name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        createNewPlaylist(name);
    }
}

void PlaylistWidget::onDeletePlaylistClicked()
{
    deleteCurrentPlaylist();
}

void PlaylistWidget::onSavePlaylistClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save Playlist", "", "Playlist Files (*.m3u *.pls)");
    if (!filePath.isEmpty()) {
        savePlaylistToFile(filePath);
    }
}

void PlaylistWidget::onLoadPlaylistClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Load Playlist", "", "Playlist Files (*.m3u *.pls)");
    if (!filePath.isEmpty()) {
        loadPlaylistFromFile(filePath);
    }
}

void PlaylistWidget::onShuffleClicked()
{
    shufflePlaylist();
}

void PlaylistWidget::onClearClicked()
{
    clearPlaylist();
}

void PlaylistWidget::updatePlaylistDisplay()
{
    populatePlaylistView();
}

void PlaylistWidget::setupUI()
{
    setAcceptDrops(true);
    
    // Setup toolbar
    m_displayModeCombo->addItems({"List", "Tree", "Compact"});
    m_displayModeCombo->setCurrentIndex(static_cast<int>(m_displayMode));
    
    m_sortByCombo->addItems({"Default", "Title", "Artist", "Album", "Duration", "Date Added"});
    m_sortByCombo->setCurrentIndex(static_cast<int>(m_sortBy));
    
    m_toolbarLayout->addWidget(new QLabel("View:"));
    m_toolbarLayout->addWidget(m_displayModeCombo);
    m_toolbarLayout->addWidget(new QLabel("Sort:"));
    m_toolbarLayout->addWidget(m_sortByCombo);
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(m_newPlaylistButton);
    m_toolbarLayout->addWidget(m_deletePlaylistButton);
    m_toolbarLayout->addWidget(m_savePlaylistButton);
    m_toolbarLayout->addWidget(m_loadPlaylistButton);
    
    // Setup controls
    m_moveUpButton->setMaximumWidth(30);
    m_moveDownButton->setMaximumWidth(30);
    
    m_controlsLayout->addWidget(m_shuffleButton);
    m_controlsLayout->addWidget(m_clearButton);
    m_controlsLayout->addStretch();
    m_controlsLayout->addWidget(m_moveUpButton);
    m_controlsLayout->addWidget(m_moveDownButton);
    
    // Setup views
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeWidget->setHeaderLabels({"Title", "Artist", "Album", "Duration"});
    
    // Set initial view
    m_currentView = m_listWidget;
    
    // Status
    m_statusLabel->setText("No playlist loaded");
    
    // Main layout
    m_mainLayout->addLayout(m_toolbarLayout);
    m_mainLayout->addLayout(m_controlsLayout);
    m_mainLayout->addWidget(m_currentView);
    m_mainLayout->addWidget(m_statusLabel);
}

void PlaylistWidget::setupConnections()
{
    // Toolbar connections
    connect(m_displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlaylistWidget::onDisplayModeChanged);
    connect(m_sortByCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlaylistWidget::onSortByChanged);
    connect(m_newPlaylistButton, &QPushButton::clicked,
            this, &PlaylistWidget::onNewPlaylistClicked);
    connect(m_deletePlaylistButton, &QPushButton::clicked,
            this, &PlaylistWidget::onDeletePlaylistClicked);
    connect(m_savePlaylistButton, &QPushButton::clicked,
            this, &PlaylistWidget::onSavePlaylistClicked);
    connect(m_loadPlaylistButton, &QPushButton::clicked,
            this, &PlaylistWidget::onLoadPlaylistClicked);
    
    // Control connections
    connect(m_shuffleButton, &QPushButton::clicked,
            this, &PlaylistWidget::onShuffleClicked);
    connect(m_clearButton, &QPushButton::clicked,
            this, &PlaylistWidget::onClearClicked);
    connect(m_moveUpButton, &QPushButton::clicked,
            this, [this]() { moveSelectedItems(-1); });
    connect(m_moveDownButton, &QPushButton::clicked,
            this, [this]() { moveSelectedItems(1); });
    
    // View connections
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &PlaylistWidget::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::itemSelectionChanged,
            this, &PlaylistWidget::onItemSelectionChanged);
    
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* /*item*/, int column) {
                Q_UNUSED(column)
                onItemDoubleClicked(nullptr); // Convert to list widget item call
            });
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &PlaylistWidget::onItemSelectionChanged);
}

void PlaylistWidget::createContextMenu()
{
    m_playAction = m_contextMenu->addAction("Play");
    m_contextMenu->addSeparator();
    m_moveUpAction = m_contextMenu->addAction("Move Up");
    m_moveDownAction = m_contextMenu->addAction("Move Down");
    m_contextMenu->addSeparator();
    m_removeAction = m_contextMenu->addAction("Remove from Playlist");
    m_contextMenu->addSeparator();
    m_propertiesAction = m_contextMenu->addAction("Properties");
    
    connect(m_playAction, &QAction::triggered, this, [this]() {
        QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            emit filesSelectedForPlayback(selectedFiles);
        }
    });
    
    connect(m_moveUpAction, &QAction::triggered, this, [this]() {
        moveSelectedItems(-1);
    });
    
    connect(m_moveDownAction, &QAction::triggered, this, [this]() {
        moveSelectedItems(1);
    });
    
    connect(m_removeAction, &QAction::triggered, this, &PlaylistWidget::removeSelectedFiles);
    
    connect(m_propertiesAction, &QAction::triggered, this, [this]() {
        QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            // Show file properties dialog
            qCDebug(playlistWidget) << "Show properties for" << selectedFiles.size() << "files";
        }
    });
}

void PlaylistWidget::populatePlaylistView()
{
    if (!m_currentPlaylist) {
        m_listWidget->clear();
        m_treeWidget->clear();
        m_statusLabel->setText("No playlist loaded");
        return;
    }
    
    // Clear current views
    m_listWidget->clear();
    m_treeWidget->clear();
    
    // Get playlist items
    // auto playlistItems = m_currentPlaylist->getMediaFiles();
    
    // Populate list view
    // for (auto mediaFile : playlistItems) {
    //     QListWidgetItem* item = new QListWidgetItem(mediaFile->getTitle());
    //     item->setData(Qt::UserRole, QVariant::fromValue(mediaFile));
    //     m_listWidget->addItem(item);
    // }
    
    // Populate tree view
    // for (auto mediaFile : playlistItems) {
    //     QTreeWidgetItem* item = new QTreeWidgetItem();
    //     item->setText(0, mediaFile->getTitle());
    //     item->setText(1, mediaFile->getArtist());
    //     item->setText(2, mediaFile->getAlbum());
    //     item->setText(3, formatDuration(mediaFile->getDuration()));
    //     item->setData(0, Qt::UserRole, QVariant::fromValue(mediaFile));
    //     m_treeWidget->addTopLevelItem(item);
    // }
    
    // Switch to appropriate view
    if (m_displayMode == ListMode || m_displayMode == CompactMode) {
        m_currentView->setParent(nullptr);
        m_currentView = m_listWidget;
        m_mainLayout->insertWidget(2, m_currentView);
    } else if (m_displayMode == TreeMode) {
        m_currentView->setParent(nullptr);
        m_currentView = m_treeWidget;
        m_mainLayout->insertWidget(2, m_currentView);
    }
    
    updatePlaylistStats();
}

void PlaylistWidget::updatePlaylistStats()
{
    QMutexLocker locker(&m_dataMutex);
    
    m_playlistStats = PlaylistStats();
    
    if (!m_currentPlaylist) {
        m_statusLabel->setText("No playlist loaded");
        return;
    }
    
    // Calculate statistics
    // auto playlistItems = m_currentPlaylist->getMediaFiles();
    // m_playlistStats.totalTracks = playlistItems.size();
    
    // for (auto mediaFile : playlistItems) {
    //     m_playlistStats.totalDuration += mediaFile->getDuration();
    //     m_playlistStats.totalSize += mediaFile->getFileSize();
    // }
    
    // Update status
    m_statusLabel->setText(QString("Tracks: %1, Duration: %2")
                          .arg(m_playlistStats.totalTracks)
                          .arg(formatDuration(m_playlistStats.totalDuration)));
}

void PlaylistWidget::moveSelectedItems(int direction)
{
    if (m_displayMode == ListMode || m_displayMode == CompactMode) {
        QList<QListWidgetItem*> selectedItems = m_listWidget->selectedItems();
        if (selectedItems.isEmpty()) {
            return;
        }
        
        // Move items in the specified direction
        for (QListWidgetItem* item : selectedItems) {
            int currentRow = m_listWidget->row(item);
            int newRow = currentRow + direction;
            
            if (newRow >= 0 && newRow < m_listWidget->count()) {
                QListWidgetItem* takenItem = m_listWidget->takeItem(currentRow);
                m_listWidget->insertItem(newRow, takenItem);
                m_listWidget->setCurrentItem(takenItem);
            }
        }
    } else if (m_displayMode == TreeMode) {
        QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
        if (selectedItems.isEmpty()) {
            return;
        }
        
        // Move items in tree view
        for (QTreeWidgetItem* item : selectedItems) {
            int currentIndex = m_treeWidget->indexOfTopLevelItem(item);
            int newIndex = currentIndex + direction;
            
            if (newIndex >= 0 && newIndex < m_treeWidget->topLevelItemCount()) {
                QTreeWidgetItem* takenItem = m_treeWidget->takeTopLevelItem(currentIndex);
                m_treeWidget->insertTopLevelItem(newIndex, takenItem);
                m_treeWidget->setCurrentItem(takenItem);
            }
        }
    }
    
    emit playlistModified(m_currentPlaylist);
}

QString PlaylistWidget::formatDuration(qint64 milliseconds) const
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    if (hours > 0) {
        return QString("%1:%2:%3")
               .arg(hours)
               .arg(minutes, 2, 10, QChar('0'))
               .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
               .arg(minutes)
               .arg(seconds, 2, 10, QChar('0'));
    }
}

QString PlaylistWidget::formatFileSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString("%1 GB").arg(static_cast<double>(bytes) / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(static_cast<double>(bytes) / MB, 0, 'f', 1);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(static_cast<double>(bytes) / KB, 0, 'f', 0);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}
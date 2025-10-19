#include "ui/PlaylistWidget.h"
#include "data/PlaylistManager.h"
#include "data/Playlist.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStandardPaths>
#include <QFileInfo>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(playlistWidget)
Q_LOGGING_CATEGORY(playlistWidget, "ui.playlist")

PlaylistWidget::PlaylistWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_headerLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_playlistLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_playlistView(nullptr)
    , m_playlistModel(nullptr)
    , m_clearButton(nullptr)
    , m_shuffleButton(nullptr)
    , m_saveButton(nullptr)
    , m_loadButton(nullptr)
    , m_contextMenu(nullptr)
    , m_currentPlaylistId(-1)
    , m_updateTimer(new QTimer(this))
{
    setupUI();
    setupContextMenu();
    setupConnections();
    
    // Set up update timer for batching updates
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    
    qCDebug(playlistWidget) << "PlaylistWidget created";
}

PlaylistWidget::~PlaylistWidget()
{
    qCDebug(playlistWidget) << "PlaylistWidget destroyed";
}

void PlaylistWidget::initialize(std::shared_ptr<PlaylistManager> playlistManager)
{
    m_playlistManager = playlistManager;
    
    if (m_playlistManager) {
        // Connect to playlist manager signals
        connect(m_playlistManager.get(), &PlaylistManager::playlistUpdated,
                this, &PlaylistWidget::onPlaylistUpdated);
        connect(m_playlistManager.get(), &PlaylistManager::playlistItemAdded,
                this, &PlaylistWidget::onPlaylistItemAdded);
        connect(m_playlistManager.get(), &PlaylistManager::playlistItemRemoved,
                this, &PlaylistWidget::onPlaylistItemRemoved);
    }
    
    qCDebug(playlistWidget) << "PlaylistWidget initialized with manager";
}

void PlaylistWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);
    
    // Header
    m_headerLayout = new QHBoxLayout();
    
    m_playlistLabel = new QLabel("Current Playlist", this);
    m_playlistLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12px; }");
    m_headerLayout->addWidget(m_playlistLabel);
    
    m_headerLayout->addStretch();
    
    m_infoLabel = new QLabel("0 items", this);
    m_infoLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_headerLayout->addWidget(m_infoLabel);
    
    m_mainLayout->addLayout(m_headerLayout);
    
    // Playlist view
    m_playlistModel = new QStandardItemModel(this);
    m_playlistModel->setHorizontalHeaderLabels({"Track", "Title", "Duration"});
    
    m_playlistView = new QListView(this);
    m_playlistView->setModel(m_playlistModel);
    m_playlistView->setAlternatingRowColors(true);
    m_playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_playlistView->setDragDropMode(QAbstractItemView::InternalMove);
    m_playlistView->setDefaultDropAction(Qt::MoveAction);
    
    m_mainLayout->addWidget(m_playlistView);
    
    // Button layout
    m_buttonLayout = new QHBoxLayout();
    
    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setMaximumWidth(60);
    m_buttonLayout->addWidget(m_clearButton);
    
    m_shuffleButton = new QPushButton("Shuffle", this);
    m_shuffleButton->setMaximumWidth(70);
    m_buttonLayout->addWidget(m_shuffleButton);
    
    m_buttonLayout->addStretch();
    
    m_saveButton = new QPushButton("Save", this);
    m_saveButton->setMaximumWidth(60);
    m_buttonLayout->addWidget(m_saveButton);
    
    m_loadButton = new QPushButton("Load", this);
    m_loadButton->setMaximumWidth(60);
    m_buttonLayout->addWidget(m_loadButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
}

void PlaylistWidget::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_playAction = new QAction("Play", this);
    m_playAction->setShortcut(QKeySequence::InsertParagraphSeparator);
    m_contextMenu->addAction(m_playAction);
    
    m_playFromAction = new QAction("Play from Here", this);
    m_contextMenu->addAction(m_playFromAction);
    
    m_contextMenu->addSeparator();
    
    m_moveUpAction = new QAction("Move Up", this);
    m_moveUpAction->setShortcut(QKeySequence("Ctrl+Up"));
    m_contextMenu->addAction(m_moveUpAction);
    
    m_moveDownAction = new QAction("Move Down", this);
    m_moveDownAction->setShortcut(QKeySequence("Ctrl+Down"));
    m_contextMenu->addAction(m_moveDownAction);
    
    m_contextMenu->addSeparator();
    
    m_showInfoAction = new QAction("Properties", this);
    m_contextMenu->addAction(m_showInfoAction);
    
    m_contextMenu->addSeparator();
    
    m_removeAction = new QAction("Remove from Playlist", this);
    m_removeAction->setShortcut(QKeySequence::Delete);
    m_contextMenu->addAction(m_removeAction);
}

void PlaylistWidget::setupConnections()
{
    // View connections
    connect(m_playlistView, &QListView::doubleClicked,
            this, &PlaylistWidget::onItemDoubleClicked);
    connect(m_playlistView, &QListView::customContextMenuRequested,
            this, &PlaylistWidget::onContextMenuRequested);
    
    // Button connections
    connect(m_clearButton, &QPushButton::clicked,
            this, &PlaylistWidget::onClearPlaylist);
    connect(m_shuffleButton, &QPushButton::clicked,
            this, &PlaylistWidget::onShufflePlaylist);
    connect(m_saveButton, &QPushButton::clicked,
            this, &PlaylistWidget::onSavePlaylist);
    connect(m_loadButton, &QPushButton::clicked,
            this, &PlaylistWidget::onLoadPlaylist);
    
    // Context menu actions
    connect(m_playAction, &QAction::triggered,
            this, &PlaylistWidget::onPlaySelected);
    connect(m_playFromAction, &QAction::triggered,
            this, &PlaylistWidget::onPlayFromHere);
    connect(m_removeAction, &QAction::triggered,
            this, &PlaylistWidget::onRemoveSelected);
    connect(m_moveUpAction, &QAction::triggered,
            [this]() { moveSelectedItems(-1); });
    connect(m_moveDownAction, &QAction::triggered,
            [this]() { moveSelectedItems(1); });
    connect(m_showInfoAction, &QAction::triggered,
            this, &PlaylistWidget::onShowFileInfo);
    
    // Timer connection
    connect(m_updateTimer, &QTimer::timeout,
            this, &PlaylistWidget::populatePlaylist);
}

void PlaylistWidget::setCurrentPlaylist(int playlistId)
{
    if (m_currentPlaylistId != playlistId) {
        m_currentPlaylistId = playlistId;
        populatePlaylist();
        updatePlaylistInfo();
        
        qCDebug(playlistWidget) << "Current playlist set to" << playlistId;
    }
}

int PlaylistWidget::getCurrentPlaylistId() const
{
    return m_currentPlaylistId;
}

void PlaylistWidget::addFiles(const QStringList& filePaths)
{
    if (m_playlistManager && m_currentPlaylistId >= 0) {
        for (const QString& filePath : filePaths) {
            m_playlistManager->addToPlaylist(m_currentPlaylistId, filePath);
        }
    }
}

QStringList PlaylistWidget::getSelectedFiles() const
{
    QStringList files;
    QModelIndexList selected = m_playlistView->selectionModel()->selectedRows();
    
    for (const QModelIndex& index : selected) {
        QStandardItem* item = m_playlistModel->itemFromIndex(index);
        if (item) {
            QString filePath = item->data(Qt::UserRole).toString();
            if (!filePath.isEmpty()) {
                files.append(filePath);
            }
        }
    }
    
    return files;
}

void PlaylistWidget::setCurrentlyPlaying(const QString& filePath)
{
    if (m_currentlyPlayingFile != filePath) {
        m_currentlyPlayingFile = filePath;
        
        // Update visual indication of currently playing item
        for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
            QStandardItem* item = m_playlistModel->item(i);
            if (item) {
                QString itemPath = item->data(Qt::UserRole).toString();
                QFont font = item->font();
                font.setBold(itemPath == filePath);
                item->setFont(font);
                
                if (itemPath == filePath) {
                    item->setIcon(QIcon(":/icons/play.png")); // TODO: Add play icon
                } else {
                    item->setIcon(QIcon());
                }
            }
        }
    }
}

void PlaylistWidget::populatePlaylist()
{
    m_playlistModel->clear();
    m_playlistModel->setHorizontalHeaderLabels({"Track", "Title", "Duration"});
    
    if (!m_playlistManager || m_currentPlaylistId < 0) {
        updatePlaylistInfo();
        return;
    }
    
    auto playlist = m_playlistManager->getPlaylist(m_currentPlaylistId);
    if (!playlist.has_value()) {
        updatePlaylistInfo();
        return;
    }
    
    auto items = m_playlistManager->getPlaylistItems(m_currentPlaylistId);
    
    for (int i = 0; i < items.size(); ++i) {
        const QString& filePath = items[i];
        QFileInfo fileInfo(filePath);
        
        auto* item = new QStandardItem(QString("%1. %2").arg(i + 1).arg(fileInfo.baseName()));
        item->setData(filePath, Qt::UserRole);
        item->setToolTip(filePath);
        
        // Set currently playing indication
        if (filePath == m_currentlyPlayingFile) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setIcon(QIcon(":/icons/play.png")); // TODO: Add play icon
        }
        
        m_playlistModel->appendRow(item);
    }
    
    updatePlaylistInfo();
    
    qCDebug(playlistWidget) << "Playlist populated with" << items.size() << "items";
}

void PlaylistWidget::updatePlaylistInfo()
{
    if (!m_playlistManager || m_currentPlaylistId < 0) {
        m_playlistLabel->setText("No Playlist");
        m_infoLabel->setText("0 items");
        return;
    }
    
    auto playlist = m_playlistManager->getPlaylist(m_currentPlaylistId);
    if (playlist.has_value()) {
        m_playlistLabel->setText(playlist->name);
        
        int itemCount = m_playlistModel->rowCount();
        m_infoLabel->setText(QString("%1 item%2").arg(itemCount).arg(itemCount != 1 ? "s" : ""));
    } else {
        m_playlistLabel->setText("Invalid Playlist");
        m_infoLabel->setText("0 items");
    }
}

void PlaylistWidget::moveSelectedItems(int direction)
{
    QModelIndexList selected = m_playlistView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    
    // Sort indices for proper movement
    if (direction > 0) {
        std::sort(selected.begin(), selected.end(), [](const QModelIndex& a, const QModelIndex& b) {
            return a.row() > b.row();
        });
    } else {
        std::sort(selected.begin(), selected.end(), [](const QModelIndex& a, const QModelIndex& b) {
            return a.row() < b.row();
        });
    }
    
    // Move items
    for (const QModelIndex& index : selected) {
        int currentRow = index.row();
        int newRow = currentRow + direction;
        
        if (newRow >= 0 && newRow < m_playlistModel->rowCount()) {
            QList<QStandardItem*> items = m_playlistModel->takeRow(currentRow);
            m_playlistModel->insertRow(newRow, items);
        }
    }
    
    // Update playlist order in manager
    if (m_playlistManager && m_currentPlaylistId >= 0) {
        QStringList newOrder;
        for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
            QStandardItem* item = m_playlistModel->item(i);
            if (item) {
                newOrder.append(item->data(Qt::UserRole).toString());
            }
        }
        emit playlistReordered(m_currentPlaylistId, newOrder);
    }
}

// Slot implementations
void PlaylistWidget::onPlaylistUpdated(int playlistId)
{
    if (playlistId == m_currentPlaylistId) {
        m_updateTimer->start();
    }
}

void PlaylistWidget::onPlaylistItemAdded(int playlistId, const QString& filePath)
{
    Q_UNUSED(filePath)
    if (playlistId == m_currentPlaylistId) {
        m_updateTimer->start();
    }
}

void PlaylistWidget::onPlaylistItemRemoved(int playlistId, const QString& filePath)
{
    Q_UNUSED(filePath)
    if (playlistId == m_currentPlaylistId) {
        m_updateTimer->start();
    }
}

void PlaylistWidget::onItemDoubleClicked(const QModelIndex& index)
{
    QStandardItem* item = m_playlistModel->itemFromIndex(index);
    if (item) {
        QString filePath = item->data(Qt::UserRole).toString();
        if (!filePath.isEmpty()) {
            emit playRequested(filePath);
        }
    }
}

void PlaylistWidget::onContextMenuRequested(const QPoint& pos)
{
    QModelIndex index = m_playlistView->indexAt(pos);
    if (index.isValid()) {
        m_contextMenu->exec(m_playlistView->mapToGlobal(pos));
    }
}

void PlaylistWidget::onClearPlaylist()
{
    if (m_playlistManager && m_currentPlaylistId >= 0) {
        int ret = QMessageBox::question(this, "Clear Playlist",
                                       "Clear all items from playlist?",
                                       QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            m_playlistManager->clearPlaylist(m_currentPlaylistId);
        }
    }
}

void PlaylistWidget::onShufflePlaylist()
{
    if (m_playlistManager && m_currentPlaylistId >= 0) {
        // TODO: Implement shuffle functionality
        QMessageBox::information(this, "Shuffle", "Shuffle functionality will be implemented.");
    }
}

void PlaylistWidget::onSavePlaylist()
{
    if (m_currentPlaylistId < 0) {
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
                                                   "Save Playlist", 
                                                   QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                                                   "M3U Playlists (*.m3u);;M3U8 Playlists (*.m3u8)");
    
    if (!fileName.isEmpty()) {
        // TODO: Implement playlist export
        QMessageBox::information(this, "Save Playlist", "Playlist save functionality will be implemented.");
    }
}

void PlaylistWidget::onLoadPlaylist()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
                                                   "Load Playlist", 
                                                   QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                                                   "Playlists (*.m3u *.m3u8 *.pls)");
    
    if (!fileName.isEmpty()) {
        // TODO: Implement playlist import
        QMessageBox::information(this, "Load Playlist", "Playlist load functionality will be implemented.");
    }
}

void PlaylistWidget::onPlaySelected()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        emit playRequested(files.first());
    }
}

void PlaylistWidget::onPlayFromHere()
{
    QModelIndexList selected = m_playlistView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    
    int startIndex = selected.first().row();
    QStringList allFiles;
    
    for (int i = 0; i < m_playlistModel->rowCount(); ++i) {
        QStandardItem* item = m_playlistModel->item(i);
        if (item) {
            allFiles.append(item->data(Qt::UserRole).toString());
        }
    }
    
    if (startIndex < allFiles.size()) {
        emit playFromRequested(allFiles, startIndex);
    }
}

void PlaylistWidget::onRemoveSelected()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        emit removeFromPlaylistRequested(files);
    }
}

void PlaylistWidget::onShowFileInfo()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        // TODO: Show file info dialog
        QMessageBox::information(this, "File Information", 
                               QString("File: %1").arg(files.first()));
    }
}
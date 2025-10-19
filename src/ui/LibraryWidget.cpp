#include "ui/LibraryWidget.h"
#include "data/LibraryManager.h"
#include "data/PlaylistManager.h"
#include "data/MediaFile.h"
#include "data/Playlist.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(libraryWidget)
Q_LOGGING_CATEGORY(libraryWidget, "ui.library")

LibraryWidget::LibraryWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_toolbarLayout(nullptr)
    , m_mainSplitter(nullptr)
    , m_librarySplitter(nullptr)
    , m_searchEdit(nullptr)
    , m_filterCombo(nullptr)
    , m_viewModeCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_scanButton(nullptr)
    , m_exportButton(nullptr)
    , m_statsButton(nullptr)
    , m_scanProgress(nullptr)
    , m_statusLabel(nullptr)
    , m_libraryTreeView(nullptr)
    , m_libraryListView(nullptr)
    , m_libraryModel(nullptr)
    , m_libraryProxyModel(nullptr)
    , m_playlistView(nullptr)
    , m_playlistModel(nullptr)
    , m_libraryContextMenu(nullptr)
    , m_playlistContextMenu(nullptr)
    , m_isScanning(false)
    , m_totalFiles(0)
    , m_currentViewMode(TreeView)
    , m_refreshTimer(new QTimer(this))
{
    setupUI();
    setupContextMenus();
    setupConnections();
    
    // Set up refresh timer for periodic updates
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(500); // 500ms delay for search
    
    qCDebug(libraryWidget) << "LibraryWidget created";
}

LibraryWidget::~LibraryWidget()
{
    qCDebug(libraryWidget) << "LibraryWidget destroyed";
}

void LibraryWidget::initialize(std::shared_ptr<LibraryManager> libraryManager,
                              std::shared_ptr<PlaylistManager> playlistManager)
{
    m_libraryManager = libraryManager;
    m_playlistManager = playlistManager;
    
    if (m_libraryManager) {
        // Connect to library manager signals
        connect(m_libraryManager.get(), &LibraryManager::scanProgress,
                this, &LibraryWidget::onScanProgress);
        connect(m_libraryManager.get(), &LibraryManager::scanComplete,
                this, &LibraryWidget::onScanComplete);
        connect(m_libraryManager.get(), &LibraryManager::mediaFileAdded,
                this, &LibraryWidget::onMediaFileAdded);
        connect(m_libraryManager.get(), &LibraryManager::mediaFileRemoved,
                this, &LibraryWidget::onMediaFileRemoved);
    }
    
    // Initial population
    populateLibraryModel();
    populatePlaylistModel();
    updateStatistics();
    
    qCDebug(libraryWidget) << "LibraryWidget initialized with managers";
}

void LibraryWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);
    
    setupToolbar();
    
    // Main splitter for library and playlist
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    setupLibraryView();
    setupPlaylistView();
    
    m_mainSplitter->addWidget(m_librarySplitter);
    m_mainSplitter->addWidget(m_playlistView);
    m_mainSplitter->setSizes({700, 300}); // 70% library, 30% playlist
    
    m_mainLayout->addWidget(m_mainSplitter);
    
    // Status bar
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_mainLayout->addWidget(m_statusLabel);
}

void LibraryWidget::setupToolbar()
{
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setSpacing(6);
    
    // Search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search library...");
    m_searchEdit->setMaximumWidth(200);
    m_toolbarLayout->addWidget(m_searchEdit);
    
    // Filter combo
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("All Files", static_cast<int>(AllFiles));
    m_filterCombo->addItem("Audio Files", static_cast<int>(AudioFiles));
    m_filterCombo->addItem("Video Files", static_cast<int>(VideoFiles));
    m_filterCombo->addItem("Recent Files", static_cast<int>(RecentFiles));
    m_filterCombo->addItem("Favorite Files", static_cast<int>(FavoriteFiles));
    m_filterCombo->setMaximumWidth(120);
    m_toolbarLayout->addWidget(m_filterCombo);
    
    // View mode combo
    m_viewModeCombo = new QComboBox(this);
    m_viewModeCombo->addItem("Tree View", static_cast<int>(TreeView));
    m_viewModeCombo->addItem("List View", static_cast<int>(ListView));
    m_viewModeCombo->setMaximumWidth(100);
    m_toolbarLayout->addWidget(m_viewModeCombo);
    
    m_toolbarLayout->addStretch();
    
    // Action buttons
    m_refreshButton = new QPushButton("Refresh", this);
    m_refreshButton->setMaximumWidth(80);
    m_toolbarLayout->addWidget(m_refreshButton);
    
    m_scanButton = new QPushButton("Scan", this);
    m_scanButton->setMaximumWidth(60);
    m_toolbarLayout->addWidget(m_scanButton);
    
    m_exportButton = new QPushButton("Export", this);
    m_exportButton->setMaximumWidth(70);
    m_toolbarLayout->addWidget(m_exportButton);
    
    m_statsButton = new QPushButton("Stats", this);
    m_statsButton->setMaximumWidth(60);
    m_toolbarLayout->addWidget(m_statsButton);
    
    // Progress bar (initially hidden)
    m_scanProgress = new QProgressBar(this);
    m_scanProgress->setVisible(false);
    m_scanProgress->setMaximumWidth(150);
    m_toolbarLayout->addWidget(m_scanProgress);
    
    m_mainLayout->addLayout(m_toolbarLayout);
}

void LibraryWidget::setupLibraryView()
{
    m_librarySplitter = new QSplitter(Qt::Vertical, this);
    
    // Create models
    m_libraryModel = new QStandardItemModel(this);
    m_libraryModel->setHorizontalHeaderLabels({"Title", "Artist", "Album", "Duration", "Format", "Path"});
    
    m_libraryProxyModel = new QSortFilterProxyModel(this);
    m_libraryProxyModel->setSourceModel(m_libraryModel);
    m_libraryProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_libraryProxyModel->setFilterKeyColumn(-1); // Search all columns
    
    // Tree view
    m_libraryTreeView = new QTreeView(this);
    m_libraryTreeView->setModel(m_libraryProxyModel);
    m_libraryTreeView->setAlternatingRowColors(true);
    m_libraryTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_libraryTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_libraryTreeView->setSortingEnabled(true);
    m_libraryTreeView->setDragEnabled(true);
    m_libraryTreeView->setAcceptDrops(true);
    m_libraryTreeView->setDropIndicatorShown(true);
    
    // Configure column widths
    m_libraryTreeView->header()->resizeSection(0, 200); // Title
    m_libraryTreeView->header()->resizeSection(1, 150); // Artist
    m_libraryTreeView->header()->resizeSection(2, 150); // Album
    m_libraryTreeView->header()->resizeSection(3, 80);  // Duration
    m_libraryTreeView->header()->resizeSection(4, 60);  // Format
    m_libraryTreeView->header()->setStretchLastSection(true); // Path
    
    // List view
    m_libraryListView = new QListView(this);
    m_libraryListView->setModel(m_libraryProxyModel);
    m_libraryListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_libraryListView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_libraryListView->setDragEnabled(true);
    m_libraryListView->setAcceptDrops(true);
    m_libraryListView->setDropIndicatorShown(true);
    m_libraryListView->setVisible(false); // Initially hidden
    
    m_librarySplitter->addWidget(m_libraryTreeView);
    m_librarySplitter->addWidget(m_libraryListView);
}

void LibraryWidget::setupPlaylistView()
{
    m_playlistModel = new QStandardItemModel(this);
    m_playlistModel->setHorizontalHeaderLabels({"Playlists"});
    
    m_playlistView = new QTreeView(this);
    m_playlistView->setModel(m_playlistModel);
    m_playlistView->setHeaderHidden(false);
    m_playlistView->setAlternatingRowColors(true);
    m_playlistView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_playlistView->setDragEnabled(true);
    m_playlistView->setAcceptDrops(true);
    m_playlistView->setDropIndicatorShown(true);
    m_playlistView->setMaximumWidth(250);
}

void LibraryWidget::setupContextMenus()
{
    // Library context menu
    m_libraryContextMenu = new QMenu(this);
    
    m_playAction = new QAction("Play", this);
    m_playAction->setShortcut(QKeySequence::InsertParagraphSeparator);
    m_libraryContextMenu->addAction(m_playAction);
    
    m_queueAction = new QAction("Add to Queue", this);
    m_queueAction->setShortcut(QKeySequence("Ctrl+Q"));
    m_libraryContextMenu->addAction(m_queueAction);
    
    m_libraryContextMenu->addSeparator();
    
    m_addToPlaylistAction = new QAction("Add to Playlist...", this);
    m_libraryContextMenu->addAction(m_addToPlaylistAction);
    
    m_createPlaylistAction = new QAction("Create Playlist...", this);
    m_libraryContextMenu->addAction(m_createPlaylistAction);
    
    m_libraryContextMenu->addSeparator();
    
    m_showInfoAction = new QAction("Properties", this);
    m_libraryContextMenu->addAction(m_showInfoAction);
    
    m_showInExplorerAction = new QAction("Show in Explorer", this);
    m_libraryContextMenu->addAction(m_showInExplorerAction);
    
    m_libraryContextMenu->addSeparator();
    
    m_removeAction = new QAction("Remove from Library", this);
    m_libraryContextMenu->addAction(m_removeAction);
    
    // Playlist context menu
    m_playlistContextMenu = new QMenu(this);
    // TODO: Add playlist-specific actions
}

void LibraryWidget::setupConnections()
{
    // Toolbar connections
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &LibraryWidget::onSearchTextChanged);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LibraryWidget::onFilterChanged);
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LibraryWidget::onViewModeChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &LibraryWidget::onRefreshRequested);
    connect(m_scanButton, &QPushButton::clicked,
            this, &LibraryWidget::onScanLibraryRequested);
    connect(m_exportButton, &QPushButton::clicked,
            this, &LibraryWidget::onExportLibraryRequested);
    connect(m_statsButton, &QPushButton::clicked,
            this, &LibraryWidget::onShowStatisticsRequested);
    
    // View connections
    connect(m_libraryTreeView, &QTreeView::doubleClicked,
            this, &LibraryWidget::onItemDoubleClicked);
    connect(m_libraryListView, &QListView::doubleClicked,
            this, &LibraryWidget::onItemDoubleClicked);
    
    connect(m_libraryTreeView, &QTreeView::customContextMenuRequested,
            this, &LibraryWidget::onContextMenuRequested);
    connect(m_libraryListView, &QListView::customContextMenuRequested,
            this, &LibraryWidget::onContextMenuRequested);
    
    // Context menu actions
    connect(m_playAction, &QAction::triggered,
            this, &LibraryWidget::onPlaySelected);
    connect(m_queueAction, &QAction::triggered,
            this, &LibraryWidget::onQueueSelected);
    connect(m_addToPlaylistAction, &QAction::triggered,
            this, &LibraryWidget::onAddToPlaylist);
    connect(m_createPlaylistAction, &QAction::triggered,
            this, &LibraryWidget::onCreatePlaylist);
    connect(m_showInfoAction, &QAction::triggered,
            this, &LibraryWidget::onShowFileInfo);
    connect(m_showInExplorerAction, &QAction::triggered,
            this, &LibraryWidget::onShowInExplorer);
    connect(m_removeAction, &QAction::triggered,
            this, &LibraryWidget::onRemoveFromLibrary);
    
    // Timer connection
    connect(m_refreshTimer, &QTimer::timeout,
            this, &LibraryWidget::refreshLibrary);
}

void LibraryWidget::refreshLibrary()
{
    populateLibraryModel();
    updateStatistics();
}

QStringList LibraryWidget::getSelectedFiles() const
{
    QStringList files;
    
    QAbstractItemView* currentView = (m_currentViewMode == TreeView) ? 
        static_cast<QAbstractItemView*>(m_libraryTreeView) : 
        static_cast<QAbstractItemView*>(m_libraryListView);
    
    QModelIndexList selected = currentView->selectionModel()->selectedRows();
    for (const QModelIndex& index : selected) {
        QModelIndex sourceIndex = m_libraryProxyModel->mapToSource(index);
        QStandardItem* item = m_libraryModel->itemFromIndex(sourceIndex);
        if (item) {
            // Path is in the last column
            QStandardItem* pathItem = m_libraryModel->item(sourceIndex.row(), 5);
            if (pathItem) {
                files.append(pathItem->text());
            }
        }
    }
    
    return files;
}

void LibraryWidget::setSearchFilter(const QString& filter)
{
    m_currentFilter = filter;
    m_libraryProxyModel->setFilterFixedString(filter);
    m_searchEdit->setText(filter);
}

void LibraryWidget::populateLibraryModel()
{
    if (!m_libraryManager) {
        return;
    }
    
    m_libraryModel->clear();
    m_libraryModel->setHorizontalHeaderLabels({"Title", "Artist", "Album", "Duration", "Format", "Path"});
    
    // Get all media files from library
    auto mediaFiles = m_libraryManager->getAllMediaFiles();
    
    for (const auto& mediaFile : mediaFiles) {
        QList<QStandardItem*> row;
        
        row.append(new QStandardItem(mediaFile.title.isEmpty() ? 
                                   QFileInfo(mediaFile.filePath).baseName() : mediaFile.title));
        row.append(new QStandardItem(mediaFile.artist));
        row.append(new QStandardItem(mediaFile.album));
        row.append(new QStandardItem(formatDuration(mediaFile.duration)));
        row.append(new QStandardItem(QFileInfo(mediaFile.filePath).suffix().toUpper()));
        row.append(new QStandardItem(mediaFile.filePath));
        
        // Set tooltips
        for (int i = 0; i < row.size(); ++i) {
            row[i]->setToolTip(mediaFile.filePath);
        }
        
        m_libraryModel->appendRow(row);
    }
    
    m_totalFiles = mediaFiles.size();
    updateStatistics();
    
    qCDebug(libraryWidget) << "Library model populated with" << m_totalFiles << "files";
}

void LibraryWidget::populatePlaylistModel()
{
    if (!m_playlistManager) {
        return;
    }
    
    m_playlistModel->clear();
    m_playlistModel->setHorizontalHeaderLabels({"Playlists"});
    
    // Add root items
    auto* libraryRoot = new QStandardItem("Library");
    libraryRoot->setEditable(false);
    m_playlistModel->appendRow(libraryRoot);
    
    auto* playlistsRoot = new QStandardItem("Playlists");
    playlistsRoot->setEditable(false);
    m_playlistModel->appendRow(playlistsRoot);
    
    // Get all playlists
    auto playlists = m_playlistManager->getAllPlaylists();
    for (const auto& playlist : playlists) {
        auto* item = new QStandardItem(playlist.name);
        item->setData(playlist.id, Qt::UserRole);
        playlistsRoot->appendRow(item);
    }
    
    m_playlistView->expandAll();
}

void LibraryWidget::updateStatistics()
{
    if (m_isScanning) {
        return;
    }
    
    QString status = QString("Library: %1 files").arg(m_totalFiles);
    
    if (!m_currentFilter.isEmpty()) {
        int filteredCount = m_libraryProxyModel->rowCount();
        status += QString(" (%1 filtered)").arg(filteredCount);
    }
    
    m_statusLabel->setText(status);
}

QString LibraryWidget::formatDuration(qint64 duration) const
{
    if (duration <= 0) {
        return "--:--";
    }
    
    int seconds = duration / 1000;
    int minutes = seconds / 60;
    int hours = minutes / 60;
    
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

// Slot implementations
void LibraryWidget::onScanProgress(int current, int total)
{
    m_isScanning = true;
    m_scanProgress->setVisible(true);
    m_scanProgress->setMaximum(total);
    m_scanProgress->setValue(current);
    
    m_statusLabel->setText(QString("Scanning... %1/%2").arg(current).arg(total));
    m_scanButton->setEnabled(false);
}

void LibraryWidget::onScanComplete()
{
    m_isScanning = false;
    m_scanProgress->setVisible(false);
    m_scanButton->setEnabled(true);
    
    refreshLibrary();
    
    qCDebug(libraryWidget) << "Library scan completed";
}

void LibraryWidget::onMediaFileAdded(const QString& filePath)
{
    Q_UNUSED(filePath)
    // Refresh with a small delay to batch updates
    m_refreshTimer->start();
}

void LibraryWidget::onMediaFileRemoved(const QString& filePath)
{
    Q_UNUSED(filePath)
    // Refresh with a small delay to batch updates
    m_refreshTimer->start();
}

void LibraryWidget::onSearchTextChanged()
{
    m_currentFilter = m_searchEdit->text();
    m_libraryProxyModel->setFilterFixedString(m_currentFilter);
    updateStatistics();
}

void LibraryWidget::onFilterChanged()
{
    FilterType filterType = static_cast<FilterType>(m_filterCombo->currentData().toInt());
    
    // TODO: Implement different filter types
    // For now, just update the display
    updateStatistics();
}

void LibraryWidget::onViewModeChanged()
{
    ViewMode newMode = static_cast<ViewMode>(m_viewModeCombo->currentData().toInt());
    
    if (newMode != m_currentViewMode) {
        m_currentViewMode = newMode;
        
        if (m_currentViewMode == TreeView) {
            m_libraryTreeView->setVisible(true);
            m_libraryListView->setVisible(false);
        } else {
            m_libraryTreeView->setVisible(false);
            m_libraryListView->setVisible(true);
        }
    }
}

void LibraryWidget::onItemDoubleClicked(const QModelIndex& index)
{
    QModelIndex sourceIndex = m_libraryProxyModel->mapToSource(index);
    QStandardItem* pathItem = m_libraryModel->item(sourceIndex.row(), 5);
    
    if (pathItem) {
        QStringList files;
        files.append(pathItem->text());
        emit playRequested(files);
    }
}

void LibraryWidget::onContextMenuRequested(const QPoint& pos)
{
    QAbstractItemView* currentView = (m_currentViewMode == TreeView) ? 
        static_cast<QAbstractItemView*>(m_libraryTreeView) : 
        static_cast<QAbstractItemView*>(m_libraryListView);
    
    QModelIndex index = currentView->indexAt(pos);
    if (index.isValid()) {
        m_libraryContextMenu->exec(currentView->mapToGlobal(pos));
    }
}

void LibraryWidget::onRefreshRequested()
{
    refreshLibrary();
}

void LibraryWidget::onScanLibraryRequested()
{
    if (m_libraryManager && !m_isScanning) {
        m_libraryManager->scanLibrary();
    }
}

void LibraryWidget::onExportLibraryRequested()
{
    exportLibraryToFile();
}

void LibraryWidget::onShowStatisticsRequested()
{
    showLibraryStatistics();
}

void LibraryWidget::onPlaySelected()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        emit playRequested(files);
    }
}

void LibraryWidget::onQueueSelected()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        emit queueRequested(files);
    }
}

void LibraryWidget::onAddToPlaylist()
{
    // TODO: Show playlist selection dialog
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        // For now, just emit with empty playlist name
        emit addToPlaylistRequested(files, QString());
    }
}

void LibraryWidget::onCreatePlaylist()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Create Playlist", 
                                        "Playlist name:", QLineEdit::Normal, 
                                        QString(), &ok);
    
    if (ok && !name.isEmpty()) {
        QStringList files = getSelectedFiles();
        emit createPlaylistRequested(name, files);
    }
}

void LibraryWidget::onShowFileInfo()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        showFileInfoDialog(files.first());
    }
}

void LibraryWidget::onRemoveFromLibrary()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty() && m_libraryManager) {
        int ret = QMessageBox::question(this, "Remove from Library",
                                       QString("Remove %1 file(s) from library?").arg(files.size()),
                                       QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            for (const QString& file : files) {
                m_libraryManager->removeMediaFile(file);
            }
        }
    }
}

void LibraryWidget::onShowInExplorer()
{
    QStringList files = getSelectedFiles();
    if (!files.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(files.first()).absolutePath()));
    }
}

void LibraryWidget::showFileInfoDialog(const QString& filePath)
{
    // TODO: Create a proper file info dialog
    QMessageBox::information(this, "File Information", 
                           QString("File: %1").arg(filePath));
}

void LibraryWidget::exportLibraryToFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
                                                   "Export Library", 
                                                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/library.json",
                                                   "JSON Files (*.json);;CSV Files (*.csv)");
    
    if (!fileName.isEmpty()) {
        // TODO: Implement actual export functionality
        QMessageBox::information(this, "Export", "Library export functionality will be implemented.");
    }
}

void LibraryWidget::showLibraryStatistics()
{
    // TODO: Create a proper statistics dialog
    QString stats = QString("Total files: %1\n").arg(m_totalFiles);
    QMessageBox::information(this, "Library Statistics", stats);
}
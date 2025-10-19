#include "ui/LibraryWidget.h"
#include "data/LibraryManager.h"
#include "data/MediaFile.h"
#include "data/PlaylistManager.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QPixmap>
#include <QImageReader>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>

Q_DECLARE_LOGGING_CATEGORY(libraryWidget)
Q_LOGGING_CATEGORY(libraryWidget, "ui.library")

LibraryWidget::LibraryWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_toolbarLayout(new QHBoxLayout())
    , m_searchLayout(new QHBoxLayout())
    , m_mainSplitter(new QSplitter(Qt::Horizontal, this))
    , m_contentSplitter(new QSplitter(Qt::Vertical, this))
    , m_viewModeCombo(new QComboBox(this))
    , m_sortByCombo(new QComboBox(this))
    , m_filterByCombo(new QComboBox(this))
    , m_refreshButton(new QPushButton("Refresh", this))
    , m_exportButton(new QPushButton("Export", this))
    , m_statsButton(new QPushButton("Statistics", this))
    , m_searchEdit(new QLineEdit(this))
    , m_searchLabel(new QLabel("Search:", this))
    , m_treeView(new QTreeView(this))
    , m_listView(new QListView(this))
    , m_tableView(new QTableView(this))
    , m_currentView(nullptr)
    , m_albumArtLabel(new QLabel(this))
    , m_albumArtWidget(new QWidget(this))
    , m_albumArtEnabled(true)
    , m_statusLabel(new QLabel(this))
    , m_progressBar(new QProgressBar(this))
    , m_libraryModel(new QStandardItemModel(this))
    , m_proxyModel(new QSortFilterProxyModel(this))
    , m_contextMenu(new QMenu(this))
    , m_libraryManager(nullptr)
    , m_playlistManager(nullptr)
    , m_viewMode(DetailView)
    , m_sortBy(Title)
    , m_filterBy(All)
    , m_albumArtTimer(new QTimer(this))
{
    setupUI();
    setupModels();
    setupConnections();
    createContextMenu();
    
    // Setup album art timer
    m_albumArtTimer->setSingleShot(true);
    m_albumArtTimer->setInterval(500); // 500ms delay for album art loading
    
    qCDebug(libraryWidget) << "LibraryWidget created";
}

LibraryWidget::~LibraryWidget()
{
    qCDebug(libraryWidget) << "LibraryWidget destroyed";
}

void LibraryWidget::setLibraryManager(LibraryManager* libraryManager)
{
    if (m_libraryManager) {
        disconnect(m_libraryManager, nullptr, this, nullptr);
    }
    
    m_libraryManager = libraryManager;
    
    if (m_libraryManager) {
        connect(m_libraryManager, &LibraryManager::libraryUpdated,
                this, &LibraryWidget::onLibraryUpdated);
        
        refreshLibrary();
    }
}

void LibraryWidget::setPlaylistManager(PlaylistManager* playlistManager)
{
    m_playlistManager = playlistManager;
}

LibraryWidget::ViewMode LibraryWidget::getViewMode() const
{
    return m_viewMode;
}

void LibraryWidget::setViewMode(ViewMode mode)
{
    if (m_viewMode != mode) {
        m_viewMode = mode;
        m_viewModeCombo->setCurrentIndex(static_cast<int>(mode));
        updateViewMode();
        emit viewModeChanged(m_viewMode);
    }
}

LibraryWidget::SortBy LibraryWidget::getSortBy() const
{
    return m_sortBy;
}

void LibraryWidget::setSortBy(SortBy sortBy)
{
    if (m_sortBy != sortBy) {
        m_sortBy = sortBy;
        m_sortByCombo->setCurrentIndex(static_cast<int>(sortBy));
        updateSorting();
    }
}

LibraryWidget::FilterBy LibraryWidget::getFilterBy() const
{
    return m_filterBy;
}

void LibraryWidget::setFilterBy(FilterBy filterBy)
{
    if (m_filterBy != filterBy) {
        m_filterBy = filterBy;
        m_filterByCombo->setCurrentIndex(static_cast<int>(filterBy));
        updateFiltering();
    }
}

QString LibraryWidget::getSearchText() const
{
    return m_searchText;
}

void LibraryWidget::setSearchText(const QString& searchText)
{
    if (m_searchText != searchText) {
        m_searchText = searchText;
        m_searchEdit->setText(searchText);
        m_proxyModel->setFilterFixedString(searchText);
    }
}

QVector<std::shared_ptr<MediaFile>> LibraryWidget::getSelectedFiles() const
{
    QVector<std::shared_ptr<MediaFile>> selectedFiles;
    
    if (!m_currentView) {
        return selectedFiles;
    }
    
    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(m_currentView);
    if (!view) {
        return selectedFiles;
    }
    
    QModelIndexList selectedIndexes = view->selectionModel()->selectedRows();
    
    for (const QModelIndex& index : selectedIndexes) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem* item = m_libraryModel->itemFromIndex(sourceIndex);
        
        if (item) {
            QVariant fileData = item->data(Qt::UserRole);
            if (fileData.isValid()) {
                // This would contain the MediaFile pointer
                // Implementation depends on how MediaFile is stored
                qCDebug(libraryWidget) << "Selected file:" << item->text();
            }
        }
    }
    
    return selectedFiles;
}

LibraryWidget::LibraryStats LibraryWidget::getLibraryStats() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_libraryStats;
}

void LibraryWidget::refreshLibrary()
{
    if (!m_libraryManager) {
        return;
    }
    
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate progress
    
    populateLibraryModel();
    updateLibraryStats();
    
    m_progressBar->setVisible(false);
    
    qCDebug(libraryWidget) << "Library refreshed";
}

bool LibraryWidget::exportLibrary(const QString& filePath, const QString& format)
{
    if (format.toLower() == "json") {
        return exportToJSON(filePath);
    } else if (format.toLower() == "csv") {
        return exportToCSV(filePath);
    } else {
        qCWarning(libraryWidget) << "Unsupported export format:" << format;
        return false;
    }
}

void LibraryWidget::showLibraryStats()
{
    LibraryStats stats = getLibraryStats();
    
    QString statsText = QString(
        "Library Statistics\n\n"
        "Total Files: %1\n"
        "Audio Files: %2\n"
        "Video Files: %3\n"
        "Playlists: %4\n"
        "Total Size: %5\n"
        "Total Duration: %6\n"
        "Most Played Artist: %7\n"
        "Most Played Genre: %8\n"
        "Total Play Count: %9"
    ).arg(stats.totalFiles)
     .arg(stats.audioFiles)
     .arg(stats.videoFiles)
     .arg(stats.playlists)
     .arg(formatFileSize(stats.totalSize))
     .arg(formatDuration(stats.totalDuration))
     .arg(stats.mostPlayedArtist.isEmpty() ? "N/A" : stats.mostPlayedArtist)
     .arg(stats.mostPlayedGenre.isEmpty() ? "N/A" : stats.mostPlayedGenre)
     .arg(stats.totalPlayCount);
    
    QMessageBox::information(this, "Library Statistics", statsText);
}

void LibraryWidget::setAlbumArtEnabled(bool enabled)
{
    if (m_albumArtEnabled != enabled) {
        m_albumArtEnabled = enabled;
        m_albumArtWidget->setVisible(enabled);
        
        if (enabled) {
            // Load album art for current selection
            QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
            if (!selectedFiles.isEmpty()) {
                // Load album art for first selected file
                // loadAlbumArt(selectedFiles.first()->getFilePath());
            }
        }
    }
}

bool LibraryWidget::isAlbumArtEnabled() const
{
    return m_albumArtEnabled;
}

void LibraryWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_currentView && m_currentView->geometry().contains(event->pos())) {
        m_contextMenu->exec(event->globalPos());
    }
}

void LibraryWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void LibraryWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void LibraryWidget::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        QStringList filePaths;
        
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                filePaths.append(url.toLocalFile());
            }
        }
        
        if (!filePaths.isEmpty() && m_libraryManager) {
            // Add files to library
            for (const QString& filePath : filePaths) {
                m_libraryManager->addMediaFile(filePath);
            }
            
            refreshLibrary();
        }
        
        event->acceptProposedAction();
    }
}

void LibraryWidget::onSearchTextChanged()
{
    m_searchText = m_searchEdit->text();
    m_proxyModel->setFilterFixedString(m_searchText);
}

void LibraryWidget::onViewModeChanged()
{
    ViewMode newMode = static_cast<ViewMode>(m_viewModeCombo->currentIndex());
    setViewMode(newMode);
}

void LibraryWidget::onSortByChanged()
{
    SortBy newSortBy = static_cast<SortBy>(m_sortByCombo->currentIndex());
    setSortBy(newSortBy);
}

void LibraryWidget::onFilterByChanged()
{
    FilterBy newFilterBy = static_cast<FilterBy>(m_filterByCombo->currentIndex());
    setFilterBy(newFilterBy);
}

void LibraryWidget::onItemDoubleClicked(const QModelIndex& index)
{
    Q_UNUSED(index)
    playSelectedFiles();
}

void LibraryWidget::onItemSelectionChanged()
{
    if (m_albumArtEnabled) {
        QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            // Load album art for first selected file
            // loadAlbumArt(selectedFiles.first()->getFilePath());
        }
    }
}

void LibraryWidget::onContextMenuRequested(const QPoint& pos)
{
    Q_UNUSED(pos)
    // Context menu is handled in contextMenuEvent
}

void LibraryWidget::onLibraryUpdated()
{
    refreshLibrary();
}

void LibraryWidget::updateLibraryStats()
{
    if (!m_libraryManager) {
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    
    // Reset stats
    m_libraryStats = LibraryStats();
    
    // Get all media files from library manager
    // This would be implemented based on LibraryManager interface
    // auto allFiles = m_libraryManager->getAllMediaFiles();
    
    // Calculate statistics
    // Implementation would iterate through files and calculate stats
    
    // Update status label
    m_statusLabel->setText(QString("Total: %1 files").arg(m_libraryStats.totalFiles));
    
    qCDebug(libraryWidget) << "Library stats updated - Total files:" << m_libraryStats.totalFiles;
}

void LibraryWidget::onAlbumArtLoaded(const QString& filePath, const QPixmap& albumArt)
{
    m_albumArtCache[filePath] = albumArt;
    
    // Update display if this is the currently selected file
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        // Check if this matches current selection
        // if (selectedFiles.first()->getFilePath() == filePath) {
        //     m_albumArtLabel->setPixmap(albumArt.scaled(ALBUM_ART_SIZE, ALBUM_ART_SIZE, 
        //                                                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        // }
    }
}

void LibraryWidget::setupUI()
{
    setAcceptDrops(true);
    
    // Setup toolbar
    m_viewModeCombo->addItems({"Tree", "List", "Grid", "Details"});
    m_viewModeCombo->setCurrentIndex(static_cast<int>(m_viewMode));
    
    m_sortByCombo->addItems({"Title", "Artist", "Album", "Genre", "Year", 
                            "Duration", "Date Added", "Play Count", "Rating"});
    m_sortByCombo->setCurrentIndex(static_cast<int>(m_sortBy));
    
    m_filterByCombo->addItems({"All", "Audio", "Video", "Playlists", 
                              "Recent", "Favorites", "Unplayed"});
    m_filterByCombo->setCurrentIndex(static_cast<int>(m_filterBy));
    
    m_toolbarLayout->addWidget(new QLabel("View:"));
    m_toolbarLayout->addWidget(m_viewModeCombo);
    m_toolbarLayout->addWidget(new QLabel("Sort:"));
    m_toolbarLayout->addWidget(m_sortByCombo);
    m_toolbarLayout->addWidget(new QLabel("Filter:"));
    m_toolbarLayout->addWidget(m_filterByCombo);
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(m_refreshButton);
    m_toolbarLayout->addWidget(m_exportButton);
    m_toolbarLayout->addWidget(m_statsButton);
    
    // Setup search
    m_searchEdit->setPlaceholderText("Search library...");
    m_searchLayout->addWidget(m_searchLabel);
    m_searchLayout->addWidget(m_searchEdit);
    m_searchLayout->addStretch();
    
    // Setup views
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setDragDropMode(QAbstractItemView::DragOnly);
    
    m_listView->setAlternatingRowColors(true);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setDragDropMode(QAbstractItemView::DragOnly);
    
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setDragDropMode(QAbstractItemView::DragOnly);
    m_tableView->setSortingEnabled(true);
    
    // Setup album art widget
    m_albumArtLabel->setFixedSize(ALBUM_ART_SIZE, ALBUM_ART_SIZE);
    m_albumArtLabel->setAlignment(Qt::AlignCenter);
    m_albumArtLabel->setStyleSheet("border: 1px solid gray; background-color: #f0f0f0;");
    m_albumArtLabel->setText("No Album Art");
    
    QVBoxLayout* albumArtLayout = new QVBoxLayout(m_albumArtWidget);
    albumArtLayout->addWidget(new QLabel("Album Art:"));
    albumArtLayout->addWidget(m_albumArtLabel);
    albumArtLayout->addStretch();
    
    // Setup splitters
    m_contentSplitter->addWidget(m_tableView); // Default view
    m_currentView = m_tableView;
    
    if (m_albumArtEnabled) {
        m_mainSplitter->addWidget(m_contentSplitter);
        m_mainSplitter->addWidget(m_albumArtWidget);
        m_mainSplitter->setSizes({800, 200});
    } else {
        m_mainSplitter->addWidget(m_contentSplitter);
    }
    
    // Setup status bar
    m_progressBar->setVisible(false);
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_progressBar);
    
    // Main layout
    m_mainLayout->addLayout(m_toolbarLayout);
    m_mainLayout->addLayout(m_searchLayout);
    m_mainLayout->addWidget(m_mainSplitter);
    m_mainLayout->addLayout(statusLayout);
    
    updateViewMode();
}

void LibraryWidget::setupModels()
{
    // Setup library model
    m_libraryModel->setHorizontalHeaderLabels({
        "Title", "Artist", "Album", "Genre", "Year", 
        "Duration", "Size", "Date Added", "Play Count"
    });
    
    // Setup proxy model for filtering and sorting
    m_proxyModel->setSourceModel(m_libraryModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // Search all columns
    
    // Set models to views
    m_treeView->setModel(m_proxyModel);
    m_listView->setModel(m_proxyModel);
    m_tableView->setModel(m_proxyModel);
}

void LibraryWidget::setupConnections()
{
    // Toolbar connections
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LibraryWidget::onViewModeChanged);
    connect(m_sortByCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LibraryWidget::onSortByChanged);
    connect(m_filterByCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LibraryWidget::onFilterByChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &LibraryWidget::refreshLibrary);
    connect(m_exportButton, &QPushButton::clicked,
            this, [this]() {
                QString filePath = QFileDialog::getSaveFileName(this, "Export Library", 
                                                               QString(), "JSON (*.json);;CSV (*.csv)");
                if (!filePath.isEmpty()) {
                    QString format = filePath.endsWith(".csv") ? "csv" : "json";
                    bool success = exportLibrary(filePath, format);
                    emit libraryExported(success, filePath);
                }
            });
    connect(m_statsButton, &QPushButton::clicked,
            this, &LibraryWidget::showLibraryStats);
    
    // Search connection
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &LibraryWidget::onSearchTextChanged);
    
    // View connections
    connect(m_tableView, &QTableView::doubleClicked,
            this, &LibraryWidget::onItemDoubleClicked);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LibraryWidget::onItemSelectionChanged);
    
    // Album art timer
    connect(m_albumArtTimer, &QTimer::timeout,
            this, [this]() {
                // Load album art for current selection
                onItemSelectionChanged();
            });
}

void LibraryWidget::updateViewMode()
{
    // Hide all views first
    m_treeView->setVisible(false);
    m_listView->setVisible(false);
    m_tableView->setVisible(false);
    
    // Remove current view from splitter
    if (m_currentView) {
        m_contentSplitter->widget(0)->setParent(nullptr);
    }
    
    // Show selected view
    switch (m_viewMode) {
        case TreeView:
            m_currentView = m_treeView;
            break;
        case ListView:
            m_currentView = m_listView;
            break;
        case GridView:
            // Grid view would need a custom widget
            m_currentView = m_listView; // Fallback to list view
            break;
        case DetailView:
        default:
            m_currentView = m_tableView;
            break;
    }
    
    m_contentSplitter->insertWidget(0, m_currentView);
    m_currentView->setVisible(true);
}

void LibraryWidget::updateSorting()
{
    int column = static_cast<int>(m_sortBy);
    m_proxyModel->sort(column, Qt::AscendingOrder);
}

void LibraryWidget::updateFiltering()
{
    // Implement filtering based on m_filterBy
    // This would set appropriate filters on the proxy model
    QString filterPattern;
    
    switch (m_filterBy) {
        case All:
            filterPattern = "";
            break;
        case Audio:
            filterPattern = "\\.(mp3|flac|wav|ogg|m4a)$";
            break;
        case Video:
            filterPattern = "\\.(mp4|avi|mkv|mov|wmv)$";
            break;
        case Playlist:
            filterPattern = "\\.(m3u|pls|xspf)$";
            break;
        case Recent:
            // Would filter by date added
            break;
        case Favorites:
            // Would filter by rating or favorite flag
            break;
        case Unplayed:
            // Would filter by play count = 0
            break;
    }
    
    if (!filterPattern.isEmpty()) {
        m_proxyModel->setFilterRegularExpression(QRegularExpression(filterPattern, QRegularExpression::CaseInsensitiveOption));
    } else {
        m_proxyModel->setFilterRegularExpression(QRegularExpression());
    }
}

void LibraryWidget::populateLibraryModel()
{
    if (!m_libraryManager) {
        return;
    }
    
    m_libraryModel->clear();
    m_libraryModel->setHorizontalHeaderLabels({
        "Title", "Artist", "Album", "Genre", "Year", 
        "Duration", "Size", "Date Added", "Play Count"
    });
    
    // Get all media files from library manager
    // This would be implemented based on LibraryManager interface
    // auto allFiles = m_libraryManager->getAllMediaFiles();
    
    // Populate model with media files
    // Implementation would iterate through files and add to model
    
    qCDebug(libraryWidget) << "Library model populated";
}

void LibraryWidget::createContextMenu()
{
    m_playAction = m_contextMenu->addAction("Play");
    m_addToPlaylistAction = m_contextMenu->addAction("Add to Playlist");
    m_contextMenu->addSeparator();
    m_propertiesAction = m_contextMenu->addAction("Properties");
    m_contextMenu->addSeparator();
    m_removeAction = m_contextMenu->addAction("Remove from Library");
    
    connect(m_playAction, &QAction::triggered, this, &LibraryWidget::playSelectedFiles);
    connect(m_addToPlaylistAction, &QAction::triggered, this, &LibraryWidget::addToPlaylist);
    connect(m_propertiesAction, &QAction::triggered, this, &LibraryWidget::showFileProperties);
    connect(m_removeAction, &QAction::triggered, this, &LibraryWidget::removeFromLibrary);
}

void LibraryWidget::playSelectedFiles()
{
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        emit filesSelectedForPlayback(selectedFiles);
    }
}

void LibraryWidget::addToPlaylist()
{
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        // Show playlist selection dialog
        // For now, emit with default playlist name
        emit filesAddedToPlaylist(selectedFiles, "Default");
    }
}

void LibraryWidget::showFileProperties()
{
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        // Show file properties dialog
        // Implementation would show detailed file information
        qCDebug(libraryWidget) << "Show properties for" << selectedFiles.size() << "files";
    }
}

void LibraryWidget::removeFromLibrary()
{
    QVector<std::shared_ptr<MediaFile>> selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        int ret = QMessageBox::question(this, "Remove Files", 
                                       QString("Remove %1 files from library?").arg(selectedFiles.size()),
                                       QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes && m_libraryManager) {
            // Remove files from library
            // Implementation would call library manager to remove files
            refreshLibrary();
        }
    }
}

bool LibraryWidget::exportToJSON(const QString& filePath)
{
    QJsonObject root;
    root["version"] = "1.0";
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray filesArray;
    
    // Export all files in the model
    for (int row = 0; row < m_libraryModel->rowCount(); ++row) {
        QJsonObject fileObj;
        
        for (int col = 0; col < m_libraryModel->columnCount(); ++col) {
            QStandardItem* item = m_libraryModel->item(row, col);
            if (item) {
                QString headerName = m_libraryModel->headerData(col, Qt::Horizontal).toString();
                fileObj[headerName.toLower().replace(" ", "_")] = item->text();
            }
        }
        
        filesArray.append(fileObj);
    }
    
    root["files"] = filesArray;
    root["statistics"] = QJsonObject{
        {"totalFiles", m_libraryStats.totalFiles},
        {"audioFiles", m_libraryStats.audioFiles},
        {"videoFiles", m_libraryStats.videoFiles},
        {"totalSize", static_cast<qint64>(m_libraryStats.totalSize)},
        {"totalDuration", static_cast<qint64>(m_libraryStats.totalDuration)}
    };
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        qCDebug(libraryWidget) << "Library exported to JSON:" << filePath;
        return true;
    }
    
    qCWarning(libraryWidget) << "Failed to export library to JSON:" << filePath;
    return false;
}

bool LibraryWidget::exportToCSV(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(libraryWidget) << "Failed to export library to CSV:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    QStringList headers;
    for (int col = 0; col < m_libraryModel->columnCount(); ++col) {
        headers << m_libraryModel->headerData(col, Qt::Horizontal).toString();
    }
    out << headers.join(",") << "\n";
    
    // Write data
    for (int row = 0; row < m_libraryModel->rowCount(); ++row) {
        QStringList rowData;
        
        for (int col = 0; col < m_libraryModel->columnCount(); ++col) {
            QStandardItem* item = m_libraryModel->item(row, col);
            QString cellData = item ? item->text() : "";
            
            // Escape commas and quotes in CSV
            if (cellData.contains(",") || cellData.contains("\"")) {
                cellData = "\"" + cellData.replace("\"", "\"\"") + "\"";
            }
            
            rowData << cellData;
        }
        
        out << rowData.join(",") << "\n";
    }
    
    qCDebug(libraryWidget) << "Library exported to CSV:" << filePath;
    return true;
}

void LibraryWidget::loadAlbumArt(const QString& filePath)
{
    // Check cache first
    if (m_albumArtCache.contains(filePath)) {
        QPixmap albumArt = m_albumArtCache[filePath];
        m_albumArtLabel->setPixmap(albumArt.scaled(ALBUM_ART_SIZE, ALBUM_ART_SIZE, 
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }
    
    // Load album art from file metadata or folder
    // This would be implemented using TagLib or similar library
    // For now, show placeholder
    m_albumArtLabel->setText("Loading...");
    
    // Simulate async loading
    QTimer::singleShot(100, this, [this, filePath]() {
        // Create placeholder pixmap
        QPixmap placeholder(ALBUM_ART_SIZE, ALBUM_ART_SIZE);
        placeholder.fill(Qt::lightGray);
        
        m_albumArtCache[filePath] = placeholder;
        m_albumArtLabel->setPixmap(placeholder);
        
        emit onAlbumArtLoaded(filePath, placeholder);
    });
}

QString LibraryWidget::formatDuration(qint64 milliseconds) const
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

QString LibraryWidget::formatFileSize(qint64 bytes) const
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
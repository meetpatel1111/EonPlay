#include "ui/MainWindow.h"
#include "ui/DragDropWidget.h"
#include "ui/MediaInfoWidget.h"
#include "ui/PlaybackControls.h"
#include "ui/PlaylistWidget.h"
#include "ui/LibraryWidget.h"
#include "ui/SystemTrayManager.h"
#include "ui/MediaKeysManager.h"
#include "ui/NotificationManager.h"
#include "ui/HotkeyManager.h"
#include "media/FileUrlSupport.h"
#include "data/LibraryManager.h"
#include "data/PlaylistManager.h"
#include "ComponentManager.h"
#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QKeyEvent>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QSizePolicy>
#include <QDesktopServices>
#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_componentManager(nullptr)
    , m_centralWidget(nullptr)
    , m_mainSplitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_videoWidget(nullptr)
    , m_playbackControls(nullptr)
    , m_playlistWidget(nullptr)
    , m_libraryWidget(nullptr)
    , m_mediaInfoWidget(nullptr)
    , m_dragDropWidget(nullptr)
    , m_menuBar(nullptr)
    , m_fileMenu(nullptr)
    , m_recentFilesMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_toolsMenu(nullptr)
    , m_helpMenu(nullptr)
    , m_statusBar(nullptr)
    , m_statusLabel(nullptr)
    , m_mediaInfoLabel(nullptr)
    , m_isDarkTheme(true)  // Default to dark theme for futuristic look
    , m_isFullscreen(false)
    , m_saveTimer(nullptr)
    , m_fileUrlSupport(nullptr)
    , m_currentSkin("dark")
{
    // Initialize settings
    m_settings = std::make_unique<QSettings>(
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/EonPlay.ini",
        QSettings::IniFormat
    );
    
    // Set window properties
    setWindowTitle("EonPlay - Timeless Media Player");
    setWindowIcon(QIcon(":/icons/app_icon.png"));
    setMinimumSize(800, 600);
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Initialize UI
    initializeUI();
    
    // Initialize system integration
    initializeSystemIntegration();
    
    // Create auto-save timer
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(false);
    m_saveTimer->setInterval(30000); // Save every 30 seconds
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveWindowState);
    m_saveTimer->start();
    
    // Apply initial theme
    applyTheme();
    
    // Restore window state
    restoreState();
}

MainWindow::~MainWindow()
{
    saveState();
}

bool MainWindow::initialize(ComponentManager* componentManager)
{
    if (!componentManager) {
        return false;
    }
    
    m_componentManager = componentManager;
    
    // Initialize FileUrlSupport component
    m_fileUrlSupport = std::make_shared<FileUrlSupport>(this);
    if (m_fileUrlSupport->initialize()) {
        // Connect FileUrlSupport signals
        connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaFileLoaded, 
                this, [this](const QString& filePath, const MediaInfo& mediaInfo) {
            updateWindowTitle(mediaInfo.title.isEmpty() ? QFileInfo(filePath).baseName() : mediaInfo.title);
            updateStatusBar(tr("Loaded: %1").arg(QFileInfo(filePath).fileName()));
            addToRecentFiles(filePath);
            updateRecentFilesMenu();
        });
        
        connect(m_fileUrlSupport.get(), &FileUrlSupport::fileValidationFailed,
                this, [this](const QString& filePath, auto result, const QString& errorMessage) {
            Q_UNUSED(result)
            onFileValidationFailed(filePath, errorMessage);
        });
        
        // Set FileUrlSupport for drag-drop widget
        if (m_dragDropWidget) {
            m_dragDropWidget->setFileUrlSupport(m_fileUrlSupport);
        }
    }
    
    // Initialize media info widget with component manager
    if (m_mediaInfoWidget && m_fileUrlSupport) {
        // Connect media info widget to file support
        // m_mediaInfoWidget->setFileUrlSupport(m_fileUrlSupport);
    }
    
    // Initialize playback controls with component manager
    if (m_playbackControls) {
        m_playbackControls->initialize(componentManager);
        
        // Connect playback control signals (TODO: connect to media engine when available)
        connect(m_playbackControls, &PlaybackControls::playRequested, [this]() {
            updateStatusBar(tr("Play requested"));
        });
        connect(m_playbackControls, &PlaybackControls::pauseRequested, [this]() {
            updateStatusBar(tr("Pause requested"));
        });
        connect(m_playbackControls, &PlaybackControls::stopRequested, [this]() {
            updateStatusBar(tr("Stop requested"));
        });
        connect(m_playbackControls, &PlaybackControls::seekRequested, [this](qint64 position) {
            int seconds = static_cast<int>(position / 1000);
            int minutes = seconds / 60;
            seconds %= 60;
            QString timeStr = QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
            updateStatusBar(tr("Seek to %1").arg(timeStr));
        });
        connect(m_playbackControls, &PlaybackControls::volumeChanged, [this](int volume) {
            updateStatusBar(tr("Volume: %1%").arg(volume));
        });
    }
    
    // Initialize library and playlist widgets
    if (m_libraryWidget && m_playlistWidget) {
        // Get managers from component manager
        auto libraryManager = componentManager->getComponent<LibraryManager>();
        auto playlistManager = componentManager->getComponent<PlaylistManager>();
        
        if (libraryManager && playlistManager) {
            m_libraryWidget->initialize(libraryManager, playlistManager);
            m_playlistWidget->initialize(playlistManager);
            
            // Connect library widget signals
            connect(m_libraryWidget, &LibraryWidget::playRequested,
                    this, [this](const QStringList& filePaths) {
                        if (!filePaths.isEmpty()) {
                            // TODO: Connect to media engine
                            updateStatusBar(tr("Playing: %1").arg(QFileInfo(filePaths.first()).fileName()));
                        }
                    });
            
            connect(m_libraryWidget, &LibraryWidget::queueRequested,
                    this, [this](const QStringList& filePaths) {
                        updateStatusBar(tr("Added %1 files to queue").arg(filePaths.size()));
                    });
            
            // Connect playlist widget signals
            connect(m_playlistWidget, &PlaylistWidget::playRequested,
                    this, [this](const QString& filePath) {
                        // TODO: Connect to media engine
                        updateStatusBar(tr("Playing: %1").arg(QFileInfo(filePath).fileName()));
                    });
        }
    }
    
    return true;
}

void MainWindow::setTheme(bool isDark)
{
    if (m_isDarkTheme != isDark) {
        m_isDarkTheme = isDark;
        applyTheme();
        emit themeChanged(m_isDarkTheme);
    }
}

void MainWindow::toggleTheme()
{
    setTheme(!m_isDarkTheme);
}

void MainWindow::setFullscreen(bool fullscreen)
{
    if (m_isFullscreen != fullscreen) {
        m_isFullscreen = fullscreen;
        
        if (fullscreen) {
            m_normalGeometry = geometry();
            showFullScreen();
            // Hide menu bar and status bar in fullscreen
            menuBar()->hide();
            statusBar()->hide();
        } else {
            showNormal();
            setGeometry(m_normalGeometry);
            // Restore menu bar and status bar
            menuBar()->show();
            statusBar()->show();
        }
        
        emit fullscreenChanged(m_isFullscreen);
    }
}

void MainWindow::setMenuBarVisible(bool visible)
{
    menuBar()->setVisible(visible);
    m_toggleMenuBarAction->setChecked(visible);
}

void MainWindow::setStatusBarVisible(bool visible)
{
    statusBar()->setVisible(visible);
    m_toggleStatusBarAction->setChecked(visible);
}

void MainWindow::showWindow()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::updateWindowTitle(const QString& mediaTitle)
{
    QString title = "EonPlay - Timeless Media Player";
    if (!mediaTitle.isEmpty()) {
        title = QString("%1 - %2").arg(mediaTitle, title);
    }
    setWindowTitle(title);
}

void MainWindow::updateStatusBar(const QString& message)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosing();
    saveState();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    
    // Save geometry periodically during resize
    if (m_saveTimer && !m_saveTimer->isActive()) {
        m_saveTimer->start();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // Handle global shortcuts
    switch (event->key()) {
        case Qt::Key_F11:
            onToggleFullscreen();
            event->accept();
            return;
        case Qt::Key_Escape:
            if (m_isFullscreen) {
                setFullscreen(false);
                event->accept();
                return;
            }
            break;
        case Qt::Key_F1:
            onAbout();
            event->accept();
            return;
    }
    
    QMainWindow::keyPressEvent(event);
}

void MainWindow::onOpenFile()
{
    QString fileFilter = tr("All Media Files (*.mp4 *.avi *.mkv *.mov *.webm *.mp3 *.flac *.wav *.aac *.ogg)");
    if (m_fileUrlSupport) {
        fileFilter = m_fileUrlSupport->getFileFilter();
    }
    
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Media File"),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        fileFilter
    );
    
    if (!fileName.isEmpty()) {
        if (m_fileUrlSupport) {
            // Use FileUrlSupport for validation and loading
            if (m_fileUrlSupport->loadMediaFile(fileName)) {
                updateStatusBar(tr("Loading: %1").arg(QFileInfo(fileName).fileName()));
            } else {
                updateStatusBar(tr("Failed to load: %1").arg(QFileInfo(fileName).fileName()));
            }
        } else {
            // Fallback for when FileUrlSupport is not available
            addToRecentFiles(fileName);
            updateRecentFilesMenu();
            updateStatusBar(tr("Loaded: %1").arg(QFileInfo(fileName).fileName()));
        }
    }
}

void MainWindow::onOpenUrl()
{
    bool ok;
    QString urlString = QInputDialog::getText(
        this,
        tr("Open URL"),
        tr("Enter media URL (HTTP, HTTPS, RTSP, etc.):"),
        QLineEdit::Normal,
        QString(),
        &ok
    );
    
    if (ok && !urlString.isEmpty()) {
        QUrl url(urlString);
        if (m_fileUrlSupport) {
            if (m_fileUrlSupport->loadMediaUrl(url)) {
                updateStatusBar(tr("Loading URL: %1").arg(urlString));
            } else {
                updateStatusBar(tr("Failed to load URL: %1").arg(urlString));
            }
        } else {
            updateStatusBar(tr("Loading URL: %1").arg(urlString));
        }
    }
}

void MainWindow::onRecentFile()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        QString fileName = action->data().toString();
        
        // Check if file still exists
        if (!QFileInfo::exists(fileName)) {
            QMessageBox::warning(this, tr("File Not Found"), 
                tr("The file '%1' no longer exists and will be removed from recent files.")
                .arg(QFileInfo(fileName).fileName()));
            
            // Remove from recent files
            m_recentFiles.removeAll(fileName);
            updateRecentFilesMenu();
            return;
        }
        
        if (m_fileUrlSupport) {
            if (m_fileUrlSupport->loadMediaFile(fileName)) {
                updateStatusBar(tr("Loading: %1").arg(QFileInfo(fileName).fileName()));
            } else {
                updateStatusBar(tr("Failed to load: %1").arg(QFileInfo(fileName).fileName()));
            }
        } else {
            updateStatusBar(tr("Loaded: %1").arg(QFileInfo(fileName).fileName()));
        }
    }
}

void MainWindow::onExit()
{
    close();
}

void MainWindow::onToggleFullscreen()
{
    setFullscreen(!m_isFullscreen);
}

void MainWindow::onToggleMenuBar()
{
    bool visible = !menuBar()->isVisible();
    setMenuBarVisible(visible);
}

void MainWindow::onToggleStatusBar()
{
    bool visible = !statusBar()->isVisible();
    setStatusBarVisible(visible);
}

void MainWindow::onTogglePlaylist()
{
    if (m_playlistWidget) {
        bool visible = !m_playlistWidget->isVisible();
        m_playlistWidget->setVisible(visible);
        m_togglePlaylistAction->setChecked(visible);
    }
}

void MainWindow::onToggleLibrary()
{
    if (m_libraryWidget) {
        bool visible = !m_libraryWidget->isVisible();
        m_libraryWidget->setVisible(visible);
        m_toggleLibraryAction->setChecked(visible);
    }
}

void MainWindow::onToggleMediaInfo()
{
    if (m_mediaInfoWidget) {
        bool visible = !m_mediaInfoWidget->isVisible();
        m_mediaInfoWidget->setVisible(visible);
        m_toggleMediaInfoAction->setChecked(visible);
    }
}

void MainWindow::onPreferences()
{
    // TODO: Show preferences dialog when implemented
    QMessageBox::information(this, tr("Preferences"), tr("Preferences dialog will be implemented in a future task."));
}

void MainWindow::onToggleTheme()
{
    toggleTheme();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About EonPlay"),
        tr("<h2>EonPlay - Timeless Media Player</h2>"
           "<p>Version 1.0.0</p>"
           "<p>A futuristic, cross-platform media player designed to play for eons.</p>"
           "<p>Built with Qt %1 and libVLC</p>"
           "<p>Copyright © 2024 EonPlay Team</p>").arg(QT_VERSION_STR));
}

void MainWindow::onAboutQt()
{
    QMessageBox::aboutQt(this, tr("About Qt"));
}

void MainWindow::saveWindowState()
{
    if (!m_isFullscreen) {
        saveState();
    }
}

void MainWindow::initializeUI()
{
    createMenuBar();
    createStatusBar();
    createCentralWidget();
    createShortcuts();
}

void MainWindow::createMenuBar()
{
    m_menuBar = menuBar();
    
    // File Menu
    m_fileMenu = m_menuBar->addMenu(tr("&File"));
    
    m_openFileAction = m_fileMenu->addAction(tr("&Open File..."));
    m_openFileAction->setShortcut(QKeySequence::Open);
    m_openFileAction->setIcon(QIcon(":/icons/open.png"));
    connect(m_openFileAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    m_openUrlAction = m_fileMenu->addAction(tr("Open &URL..."));
    m_openUrlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    connect(m_openUrlAction, &QAction::triggered, this, &MainWindow::onOpenUrl);
    
    m_fileMenu->addSeparator();
    
    // Recent Files submenu
    m_recentFilesMenu = m_fileMenu->addMenu(tr("Recent Files"));
    updateRecentFilesMenu();
    
    m_fileMenu->addSeparator();
    
    m_exitAction = m_fileMenu->addAction(tr("E&xit"));
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
    // View Menu
    m_viewMenu = m_menuBar->addMenu(tr("&View"));
    
    m_fullscreenAction = m_viewMenu->addAction(tr("&Fullscreen"));
    m_fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    m_fullscreenAction->setCheckable(true);
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::onToggleFullscreen);
    
    m_viewMenu->addSeparator();
    
    m_toggleMenuBarAction = m_viewMenu->addAction(tr("Show &Menu Bar"));
    m_toggleMenuBarAction->setCheckable(true);
    m_toggleMenuBarAction->setChecked(true);
    connect(m_toggleMenuBarAction, &QAction::triggered, this, &MainWindow::onToggleMenuBar);
    
    m_toggleStatusBarAction = m_viewMenu->addAction(tr("Show &Status Bar"));
    m_toggleStatusBarAction->setCheckable(true);
    m_toggleStatusBarAction->setChecked(true);
    connect(m_toggleStatusBarAction, &QAction::triggered, this, &MainWindow::onToggleStatusBar);
    
    m_viewMenu->addSeparator();
    
    m_togglePlaylistAction = m_viewMenu->addAction(tr("Show &Playlist"));
    m_togglePlaylistAction->setCheckable(true);
    m_togglePlaylistAction->setChecked(false);
    connect(m_togglePlaylistAction, &QAction::triggered, this, &MainWindow::onTogglePlaylist);
    
    m_toggleLibraryAction = m_viewMenu->addAction(tr("Show &Library"));
    m_toggleLibraryAction->setCheckable(true);
    m_toggleLibraryAction->setChecked(false);
    connect(m_toggleLibraryAction, &QAction::triggered, this, &MainWindow::onToggleLibrary);
    
    m_toggleMediaInfoAction = m_viewMenu->addAction(tr("Show Media &Info"));
    m_toggleMediaInfoAction->setCheckable(true);
    m_toggleMediaInfoAction->setChecked(false);
    connect(m_toggleMediaInfoAction, &QAction::triggered, this, &MainWindow::onToggleMediaInfo);
    
    // Tools Menu
    m_toolsMenu = m_menuBar->addMenu(tr("&Tools"));
    
    m_preferencesAction = m_toolsMenu->addAction(tr("&Preferences..."));
    m_preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::onPreferences);
    
    m_toolsMenu->addSeparator();
    
    m_toggleThemeAction = m_toolsMenu->addAction(tr("Toggle &Theme"));
    m_toggleThemeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_toggleThemeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);
    
    m_toolsMenu->addSeparator();
    
    QAction* fileAssociationsAction = m_toolsMenu->addAction(tr("Register &File Associations"));
    connect(fileAssociationsAction, &QAction::triggered, this, &MainWindow::registerFileAssociations);
    
    QAction* skinSelectorAction = m_toolsMenu->addAction(tr("Select &Skin..."));
    connect(skinSelectorAction, &QAction::triggered, this, &MainWindow::showSkinSelector);
    
    m_toolsMenu->addSeparator();
    
    // System Integration submenu
    QMenu* systemIntegrationMenu = m_toolsMenu->addMenu(tr("System &Integration"));
    
    QAction* toggleSystemTrayAction = systemIntegrationMenu->addAction(tr("Enable System &Tray"));
    toggleSystemTrayAction->setCheckable(true);
    toggleSystemTrayAction->setChecked(isSystemTrayEnabled());
    connect(toggleSystemTrayAction, &QAction::toggled, this, &MainWindow::setSystemTrayEnabled);
    
    QAction* toggleMediaKeysAction = systemIntegrationMenu->addAction(tr("Enable &Media Keys"));
    toggleMediaKeysAction->setCheckable(true);
    toggleMediaKeysAction->setChecked(isMediaKeysEnabled());
    connect(toggleMediaKeysAction, &QAction::toggled, this, &MainWindow::setMediaKeysEnabled);
    
    QAction* toggleNotificationsAction = systemIntegrationMenu->addAction(tr("Enable &Notifications"));
    toggleNotificationsAction->setCheckable(true);
    toggleNotificationsAction->setChecked(isNotificationsEnabled());
    connect(toggleNotificationsAction, &QAction::toggled, this, &MainWindow::setNotificationsEnabled);
    
    systemIntegrationMenu->addSeparator();
    
    QAction* customizeHotkeysAction = systemIntegrationMenu->addAction(tr("Customize &Hotkeys..."));
    connect(customizeHotkeysAction, &QAction::triggered, this, [this]() {
        if (m_hotkeyManager) {
            m_hotkeyManager->showCustomizationDialog();
        }
    });
    
    // Help Menu
    m_helpMenu = m_menuBar->addMenu(tr("&Help"));
    
    m_aboutAction = m_helpMenu->addAction(tr("&About EonPlay"));
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    
    m_aboutQtAction = m_helpMenu->addAction(tr("About &Qt"));
    connect(m_aboutQtAction, &QAction::triggered, this, &MainWindow::onAboutQt);
}

void MainWindow::createStatusBar()
{
    m_statusBar = statusBar();
    
    m_statusLabel = new QLabel(tr("Ready"));
    m_statusBar->addWidget(m_statusLabel, 1);
    
    m_mediaInfoLabel = new QLabel();
    m_statusBar->addPermanentWidget(m_mediaInfoLabel);
    
    updateStatusBar(tr("EonPlay ready - Drop media files to play"));
}

void MainWindow::createCentralWidget()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // Create main splitter (horizontal)
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(m_mainSplitter);
    
    // Create video display area with drag-and-drop support
    m_dragDropWidget = new DragDropWidget;
    m_dragDropWidget->setStyleSheet("background-color: #1a1a1a; border: 1px solid #333;");
    m_dragDropWidget->setMinimumSize(400, 300);
    
    // Connect drag-and-drop signals
    connect(m_dragDropWidget, &DragDropWidget::filesDropped, this, &MainWindow::onFilesDropped);
    connect(m_dragDropWidget, &DragDropWidget::urlDropped, this, &MainWindow::onUrlDropped);
    connect(m_dragDropWidget, &DragDropWidget::dragEntered, this, &MainWindow::onDragEntered);
    connect(m_dragDropWidget, &DragDropWidget::dragLeft, this, &MainWindow::onDragLeft);
    
    QWidget* videoArea = m_dragDropWidget;
    
    // Add futuristic placeholder content
    QVBoxLayout* videoLayout = new QVBoxLayout(videoArea);
    videoLayout->setAlignment(Qt::AlignCenter);
    
    QLabel* logoLabel = new QLabel("EonPlay");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet(
        "font-size: 48px; "
        "font-weight: bold; "
        "color: #00d4ff; "
        "background: transparent; "
        "border: none;"
    );
    
    QLabel* taglineLabel = new QLabel("Timeless Media Player");
    taglineLabel->setAlignment(Qt::AlignCenter);
    taglineLabel->setStyleSheet(
        "font-size: 16px; "
        "color: #888; "
        "background: transparent; "
        "border: none; "
        "margin-top: 10px;"
    );
    
    QLabel* instructionLabel = new QLabel("Drop media files here or use File → Open");
    instructionLabel->setAlignment(Qt::AlignCenter);
    instructionLabel->setStyleSheet(
        "font-size: 14px; "
        "color: #666; "
        "background: transparent; "
        "border: none; "
        "margin-top: 20px;"
    );
    
    videoLayout->addWidget(logoLabel);
    videoLayout->addWidget(taglineLabel);
    videoLayout->addWidget(instructionLabel);
    
    m_mainSplitter->addWidget(videoArea);
    
    // Create right panel splitter (vertical)
    m_rightSplitter = new QSplitter(Qt::Vertical);
    m_rightSplitter->setVisible(false); // Hidden by default
    
    // Create media info widget
    m_mediaInfoWidget = new MediaInfoWidget;
    m_mediaInfoWidget->setVisible(false); // Hidden by default
    m_rightSplitter->addWidget(m_mediaInfoWidget);
    
    // TODO: Add playlist and library widgets in future tasks
    // Create playlist widget
    m_playlistWidget = new PlaylistWidget(this);
    m_playlistWidget->setVisible(false);
    m_rightSplitter->addWidget(m_playlistWidget);
    
    // Create library widget
    m_libraryWidget = new LibraryWidget(this);
    m_libraryWidget->setVisible(false);
    m_rightSplitter->addWidget(m_libraryWidget);
    
    m_mainSplitter->addWidget(m_rightSplitter);
    
    // Set splitter proportions
    m_mainSplitter->setSizes({800, 300});
    m_rightSplitter->setSizes({200, 200, 200});
    
    // Create playback controls
    m_playbackControls = new PlaybackControls;
    mainLayout->addWidget(m_playbackControls);
}

void MainWindow::createShortcuts()
{
    // Additional shortcuts not covered by menu actions
    
    // Media control shortcuts (will be connected to media engine later)
    QShortcut* playPauseShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    // TODO: Connect to media engine when available
    
    QShortcut* stopShortcut = new QShortcut(QKeySequence(Qt::Key_S), this);
    // TODO: Connect to media engine when available
    
    QShortcut* volumeUpShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    // TODO: Connect to media engine when available
    
    QShortcut* volumeDownShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    // TODO: Connect to media engine when available
}

void MainWindow::applyTheme()
{
    QString stylesheet = loadThemeStylesheet(m_isDarkTheme ? "dark" : "light");
    setStyleSheet(stylesheet);
    
    // Update theme action text
    if (m_toggleThemeAction) {
        m_toggleThemeAction->setText(m_isDarkTheme ? tr("Switch to Light Theme") : tr("Switch to Dark Theme"));
    }
}

QString MainWindow::loadThemeStylesheet(const QString& themeName)
{
    // Load theme from resources
    QFile themeFile(QString(":/themes/%1.qss").arg(themeName));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(themeFile.readAll());
    }
    
    // Fallback to built-in theme
    if (themeName == "dark") {
        return QString(
            "QMainWindow {"
            "    background-color: #1e1e1e;"
            "    color: #ffffff;"
            "}"
            "QMenuBar {"
            "    background-color: #2d2d2d;"
            "    color: #ffffff;"
            "    border-bottom: 1px solid #444444;"
            "}"
            "QMenuBar::item {"
            "    background-color: transparent;"
            "    padding: 4px 8px;"
            "}"
            "QMenuBar::item:selected {"
            "    background-color: #404040;"
            "}"
            "QMenu {"
            "    background-color: #2d2d2d;"
            "    color: #ffffff;"
            "    border: 1px solid #444444;"
            "}"
            "QMenu::item {"
            "    padding: 4px 20px;"
            "}"
            "QMenu::item:selected {"
            "    background-color: #404040;"
            "}"
            "QStatusBar {"
            "    background-color: #2d2d2d;"
            "    color: #ffffff;"
            "    border-top: 1px solid #444444;"
            "}"
            "QLabel {"
            "    color: #ffffff;"
            "}"
            "QSplitter::handle {"
            "    background-color: #444444;"
            "}"
            "QSplitter::handle:horizontal {"
            "    width: 2px;"
            "}"
            "QSplitter::handle:vertical {"
            "    height: 2px;"
            "}"
        );
    } else {
        return QString(
            "QMainWindow {"
            "    background-color: #f0f0f0;"
            "    color: #000000;"
            "}"
            "QMenuBar {"
            "    background-color: #e0e0e0;"
            "    color: #000000;"
            "    border-bottom: 1px solid #c0c0c0;"
            "}"
            "QMenuBar::item {"
            "    background-color: transparent;"
            "    padding: 4px 8px;"
            "}"
            "QMenuBar::item:selected {"
            "    background-color: #d0d0d0;"
            "}"
            "QMenu {"
            "    background-color: #f0f0f0;"
            "    color: #000000;"
            "    border: 1px solid #c0c0c0;"
            "}"
            "QMenu::item {"
            "    padding: 4px 20px;"
            "}"
            "QMenu::item:selected {"
            "    background-color: #e0e0e0;"
            "}"
            "QStatusBar {"
            "    background-color: #e0e0e0;"
            "    color: #000000;"
            "    border-top: 1px solid #c0c0c0;"
            "}"
            "QLabel {"
            "    color: #000000;"
            "}"
            "QSplitter::handle {"
            "    background-color: #c0c0c0;"
            "}"
            "QSplitter::handle:horizontal {"
            "    width: 2px;"
            "}"
            "QSplitter::handle:vertical {"
            "    height: 2px;"
            "}"
        );
    }
}

void MainWindow::saveState()
{
    m_settings->beginGroup("MainWindow");
    m_settings->setValue("geometry", saveGeometry());
    m_settings->setValue("windowState", QMainWindow::saveState());
    m_settings->setValue("isDarkTheme", m_isDarkTheme);
    m_settings->setValue("currentSkin", m_currentSkin);
    m_settings->setValue("menuBarVisible", menuBar()->isVisible());
    m_settings->setValue("statusBarVisible", statusBar()->isVisible());
    m_settings->setValue("splitterState", m_mainSplitter->saveState());
    m_settings->setValue("rightSplitterState", m_rightSplitter->saveState());
    m_settings->endGroup();
    
    // Save system integration settings
    m_settings->beginGroup("SystemIntegration");
    m_settings->setValue("systemTrayEnabled", isSystemTrayEnabled());
    m_settings->setValue("mediaKeysEnabled", isMediaKeysEnabled());
    m_settings->setValue("notificationsEnabled", isNotificationsEnabled());
    m_settings->endGroup();
    
    // Save recent files
    m_settings->beginGroup("RecentFiles");
    m_settings->setValue("files", m_recentFiles);
    m_settings->endGroup();
    
    m_settings->sync();
}

void MainWindow::restoreState()
{
    m_settings->beginGroup("MainWindow");
    restoreGeometry(m_settings->value("geometry").toByteArray());
    QMainWindow::restoreState(m_settings->value("windowState").toByteArray());
    
    bool isDark = m_settings->value("isDarkTheme", true).toBool();
    QString savedSkin = m_settings->value("currentSkin", "dark").toString();
    
    if (savedSkin == "dark" || savedSkin == "light") {
        setTheme(savedSkin == "dark");
    } else {
        // Try to load custom skin
        loadCustomSkin(savedSkin);
        // Fallback to theme if skin loading fails
        if (m_currentSkin != savedSkin) {
            setTheme(isDark);
        }
    }
    
    bool menuVisible = m_settings->value("menuBarVisible", true).toBool();
    setMenuBarVisible(menuVisible);
    
    bool statusVisible = m_settings->value("statusBarVisible", true).toBool();
    setStatusBarVisible(statusVisible);
    
    m_mainSplitter->restoreState(m_settings->value("splitterState").toByteArray());
    m_rightSplitter->restoreState(m_settings->value("rightSplitterState").toByteArray());
    m_settings->endGroup();
    
    // Restore recent files
    m_settings->beginGroup("RecentFiles");
    m_recentFiles = m_settings->value("files").toStringList();
    m_settings->endGroup();
    
    // Restore system integration settings
    m_settings->beginGroup("SystemIntegration");
    bool systemTrayEnabled = m_settings->value("systemTrayEnabled", true).toBool();
    bool mediaKeysEnabled = m_settings->value("mediaKeysEnabled", true).toBool();
    bool notificationsEnabled = m_settings->value("notificationsEnabled", true).toBool();
    m_settings->endGroup();
    
    // Apply system integration settings
    setSystemTrayEnabled(systemTrayEnabled);
    setMediaKeysEnabled(mediaKeysEnabled);
    setNotificationsEnabled(notificationsEnabled);
    
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    
    for (int i = 0; i < m_recentFiles.size() && i < MaxRecentFiles; ++i) {
        const QString& fileName = m_recentFiles.at(i);
        QAction* action = m_recentFilesMenu->addAction(
            QString("&%1 %2").arg(i + 1).arg(QFileInfo(fileName).fileName())
        );
        action->setData(fileName);
        action->setToolTip(fileName);
        connect(action, &QAction::triggered, this, &MainWindow::onRecentFile);
    }
    
    if (m_recentFiles.isEmpty()) {
        QAction* noFilesAction = m_recentFilesMenu->addAction(tr("No recent files"));
        noFilesAction->setEnabled(false);
    }
}

void MainWindow::addToRecentFiles(const QString& filePath)
{
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    
    while (m_recentFiles.size() > MaxRecentFiles) {
        m_recentFiles.removeLast();
    }
}



void MainWindow::onFilesDropped(const QStringList& processedFiles, const QStringList& failedFiles)
{
    if (!processedFiles.isEmpty()) {
        QString firstFile = processedFiles.first();
        updateStatusBar(tr("Loaded %1 file(s): %2")
            .arg(processedFiles.size())
            .arg(QFileInfo(firstFile).fileName()));
        
        // Add processed files to recent files
        for (const QString& file : processedFiles) {
            addToRecentFiles(file);
        }
        updateRecentFilesMenu();
    }
    
    if (!failedFiles.isEmpty()) {
        QString message = tr("Failed to load %1 file(s)").arg(failedFiles.size());
        if (failedFiles.size() == 1) {
            message += tr(": %1").arg(QFileInfo(failedFiles.first()).fileName());
        }
        
        QMessageBox::warning(this, tr("File Load Error"), message);
    }
}

void MainWindow::onUrlDropped(const QUrl& url, bool success)
{
    if (success) {
        updateStatusBar(tr("Loaded URL: %1").arg(url.toString()));
    } else {
        updateStatusBar(tr("Failed to load URL: %1").arg(url.toString()));
        QMessageBox::warning(this, tr("URL Load Error"), 
            tr("Failed to load URL: %1").arg(url.toString()));
    }
}

void MainWindow::onDragEntered(bool hasValidData)
{
    if (hasValidData) {
        updateStatusBar(tr("Drop media files to play"));
    } else {
        updateStatusBar(tr("Unsupported file type"));
    }
}

void MainWindow::onDragLeft()
{
    updateStatusBar(tr("EonPlay ready - Drop media files to play"));
}

void MainWindow::onFileValidationFailed(const QString& filePath, const QString& errorMessage)
{
    QString fileName = QFileInfo(filePath).fileName();
    updateStatusBar(tr("Validation failed: %1").arg(fileName));
    
    QMessageBox::warning(this, tr("File Validation Error"), 
        tr("Cannot load file '%1':\n%2").arg(fileName, errorMessage));
}

void MainWindow::registerFileAssociations()
{
    // This is a placeholder implementation
    // In a real application, this would register file associations with the OS
    
#ifdef Q_OS_WIN
    // Windows implementation would use registry entries
    // HKEY_CLASSES_ROOT\.mp4 = "EonPlay.MediaFile"
    // HKEY_CLASSES_ROOT\EonPlay.MediaFile\shell\open\command = "path\to\eonplay.exe" "%1"
    qDebug() << "File associations registration not implemented for Windows";
#elif defined(Q_OS_LINUX)
    // Linux implementation would use XDG MIME type registration
    // Create .desktop file and register MIME types
    qDebug() << "File associations registration not implemented for Linux";
#endif
    
    updateStatusBar(tr("File associations feature will be implemented in system integration phase"));
}

bool MainWindow::areFileAssociationsRegistered() const
{
    // This is a placeholder implementation
    // In a real application, this would check if associations are registered
    
#ifdef Q_OS_WIN
    // Check Windows registry entries
    return false;
#elif defined(Q_OS_LINUX)
    // Check XDG MIME type registrations
    return false;
#else
    return false;
#endif
}

void MainWindow::loadCustomSkin(const QString& skinName)
{
    if (skinName.isEmpty()) {
        return;
    }
    
    // Look for skin file in resources or skins directory
    QString skinPath = QString(":/skins/%1.qss").arg(skinName);
    QFile skinFile(skinPath);
    
    // If not in resources, try skins directory
    if (!skinFile.exists()) {
        QString skinsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/skins";
        skinPath = QString("%1/%2.qss").arg(skinsDir, skinName);
        skinFile.setFileName(skinPath);
    }
    
    if (skinFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString stylesheet = QString::fromUtf8(skinFile.readAll());
        setStyleSheet(stylesheet);
        m_currentSkin = skinName;
        
        updateStatusBar(tr("Loaded skin: %1").arg(skinName));
        
        // Save skin preference
        m_settings->setValue("appearance/customSkin", skinName);
    } else {
        QMessageBox::warning(this, tr("Skin Load Error"), 
            tr("Could not load skin '%1'").arg(skinName));
    }
}

QStringList MainWindow::getAvailableSkins() const
{
    QStringList skins;
    
    // Add built-in themes
    skins << "dark" << "light";
    
    // Look for custom skins in resources
    QDir resourceSkins(":/skins");
    if (resourceSkins.exists()) {
        QStringList skinFiles = resourceSkins.entryList(QStringList() << "*.qss", QDir::Files);
        for (const QString& file : skinFiles) {
            QString skinName = QFileInfo(file).baseName();
            if (!skins.contains(skinName)) {
                skins << skinName;
            }
        }
    }
    
    // Look for custom skins in user directory
    QString skinsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/skins";
    QDir userSkins(skinsDir);
    if (userSkins.exists()) {
        QStringList skinFiles = userSkins.entryList(QStringList() << "*.qss", QDir::Files);
        for (const QString& file : skinFiles) {
            QString skinName = QFileInfo(file).baseName();
            if (!skins.contains(skinName)) {
                skins << skinName;
            }
        }
    }
    
    return skins;
}

void MainWindow::showSkinSelector()
{
    QStringList availableSkins = getAvailableSkins();
    
    if (availableSkins.isEmpty()) {
        QMessageBox::information(this, tr("No Skins Available"), 
            tr("No custom skins are available. Only built-in themes can be used."));
        return;
    }
    
    bool ok;
    QString selectedSkin = QInputDialog::getItem(
        this,
        tr("Select Skin"),
        tr("Choose a skin:"),
        availableSkins,
        availableSkins.indexOf(m_currentSkin),
        false,
        &ok
    );
    
    if (ok && !selectedSkin.isEmpty()) {
        if (selectedSkin == "dark" || selectedSkin == "light") {
            // Use built-in theme
            setTheme(selectedSkin == "dark");
        } else {
            // Load custom skin
            loadCustomSkin(selectedSkin);
        }
    }
}

void MainWindow::initializeSystemIntegration()
{
    // Initialize System Tray Manager
    if (SystemTrayManager::isSystemTrayAvailable()) {
        m_systemTrayManager = std::make_unique<SystemTrayManager>(this);
        if (m_systemTrayManager->initialize(this)) {
            // Connect system tray signals
            connect(m_systemTrayManager.get(), &SystemTrayManager::showMainWindowRequested,
                    this, &MainWindow::showWindow);
            connect(m_systemTrayManager.get(), &SystemTrayManager::quitRequested,
                    this, &MainWindow::close);
            connect(m_systemTrayManager.get(), &SystemTrayManager::playRequested,
                    this, [this]() { updateStatusBar(tr("Play requested from system tray")); });
            connect(m_systemTrayManager.get(), &SystemTrayManager::pauseRequested,
                    this, [this]() { updateStatusBar(tr("Pause requested from system tray")); });
            
            // Show system tray by default
            m_systemTrayManager->setVisible(true);
            
            qDebug() << "System tray initialized successfully";
        } else {
            qWarning() << "Failed to initialize system tray";
            m_systemTrayManager.reset();
        }
    } else {
        qDebug() << "System tray not available on this platform";
    }
    
    // Initialize Media Keys Manager
    if (MediaKeysManager::isSupported()) {
        m_mediaKeysManager = std::make_unique<MediaKeysManager>(this);
        if (m_mediaKeysManager->initialize()) {
            // Connect media key signals
            connect(m_mediaKeysManager.get(), &MediaKeysManager::playPausePressed,
                    this, [this]() { updateStatusBar(tr("Play/Pause key pressed")); });
            connect(m_mediaKeysManager.get(), &MediaKeysManager::stopPressed,
                    this, [this]() { updateStatusBar(tr("Stop key pressed")); });
            connect(m_mediaKeysManager.get(), &MediaKeysManager::nextPressed,
                    this, [this]() { updateStatusBar(tr("Next key pressed")); });
            connect(m_mediaKeysManager.get(), &MediaKeysManager::previousPressed,
                    this, [this]() { updateStatusBar(tr("Previous key pressed")); });
            connect(m_mediaKeysManager.get(), &MediaKeysManager::volumeUpPressed,
                    this, [this]() { updateStatusBar(tr("Volume up")); });
            connect(m_mediaKeysManager.get(), &MediaKeysManager::volumeDownPressed,
                    this, [this]() { updateStatusBar(tr("Volume down")); });
            
            qDebug() << "Media keys initialized successfully";
        } else {
            qWarning() << "Failed to initialize media keys";
            m_mediaKeysManager.reset();
        }
    } else {
        qDebug() << "Media keys not supported on this platform";
    }
    
    // Initialize Notification Manager
    m_notificationManager = std::make_unique<NotificationManager>(this);
    if (m_notificationManager->initialize(this)) {
        // Connect notification signals
        connect(m_notificationManager.get(), &NotificationManager::notificationClicked,
                this, [this](const NotificationData& data) {
            Q_UNUSED(data)
            updateStatusBar(tr("Notification clicked"));
        });
        
        // Connect to file support for media notifications
        if (m_fileUrlSupport) {
            connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaFileLoaded,
                    this, [this](const QString& filePath, const MediaInfo& info) {
                        if (m_notificationManager) {
                            QString title = info.title.isEmpty() ? QFileInfo(filePath).baseName() : info.title;
                            m_notificationManager->onMediaLoaded(filePath, title);
                        }
                    });
        }
        
        // Show welcome notification
        m_notificationManager->showNotification(tr("EonPlay Ready"), 
                                               tr("Timeless media player initialized"));
        
        qDebug() << "Notifications initialized successfully";
    } else {
        qWarning() << "Failed to initialize notifications";
        m_notificationManager.reset();
    }
    
    // Initialize Hotkey Manager
    m_hotkeyManager = std::make_unique<HotkeyManager>(this);
    if (m_hotkeyManager->initialize(this)) {
        // Connect hotkey signals
        connect(m_hotkeyManager.get(), &HotkeyManager::actionTriggered,
                this, [this](const QString& actionId) {
            handleHotkeyAction(actionId);
        });
        
        qDebug() << "Hotkey manager initialized successfully";
    } else {
        qWarning() << "Failed to initialize hotkey manager";
        m_hotkeyManager.reset();
    }
}

void MainWindow::setSystemTrayEnabled(bool enabled)
{
    if (m_systemTrayManager) {
        m_systemTrayManager->setVisible(enabled);
        
        // Save preference
        m_settings->setValue("systemIntegration/systemTrayEnabled", enabled);
        
        updateStatusBar(enabled ? tr("System tray enabled") : tr("System tray disabled"));
    }
}

bool MainWindow::isSystemTrayEnabled() const
{
    return m_systemTrayManager && m_systemTrayManager->isVisible();
}

void MainWindow::setMediaKeysEnabled(bool enabled)
{
    if (m_mediaKeysManager) {
        m_mediaKeysManager->setEnabled(enabled);
        
        // Save preference
        m_settings->setValue("systemIntegration/mediaKeysEnabled", enabled);
        
        updateStatusBar(enabled ? tr("Media keys enabled") : tr("Media keys disabled"));
    }
}

bool MainWindow::isMediaKeysEnabled() const
{
    return m_mediaKeysManager && m_mediaKeysManager->isEnabled();
}

void MainWindow::setNotificationsEnabled(bool enabled)
{
    if (m_notificationManager) {
        m_notificationManager->setEnabled(enabled);
        
        // Save preference
        m_settings->setValue("systemIntegration/notificationsEnabled", enabled);
        
        updateStatusBar(enabled ? tr("Notifications enabled") : tr("Notifications disabled"));
    }
}

bool MainWindow::isNotificationsEnabled() const
{
    return m_notificationManager && m_notificationManager->isEnabled();
}

void MainWindow::onSystemTrayActivated()
{
    updateStatusBar(tr("System tray activated"));
}

void MainWindow::onMediaKeyPressed(const QString& key)
{
    updateStatusBar(tr("Media key pressed: %1").arg(key));
}

void MainWindow::onNotificationClicked()
{
    updateStatusBar(tr("Notification clicked"));
}

void MainWindow::handleHotkeyAction(const QString& actionId)
{
    // Handle various hotkey actions
    if (actionId == "play_pause") {
        updateStatusBar(tr("Play/Pause hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "stop") {
        updateStatusBar(tr("Stop hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "next") {
        updateStatusBar(tr("Next hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "previous") {
        updateStatusBar(tr("Previous hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "volume_up") {
        updateStatusBar(tr("Volume up hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "volume_down") {
        updateStatusBar(tr("Volume down hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "mute") {
        updateStatusBar(tr("Mute hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "open_file") {
        onOpenFile();
    } else if (actionId == "open_url") {
        onOpenUrl();
    } else if (actionId == "fullscreen") {
        onToggleFullscreen();
    } else if (actionId == "minimize") {
        showMinimized();
    } else if (actionId == "quit") {
        close();
    } else if (actionId == "toggle_playlist") {
        onTogglePlaylist();
    } else if (actionId == "toggle_library") {
        onToggleLibrary();
    } else if (actionId == "seek_forward") {
        updateStatusBar(tr("Seek forward hotkey pressed"));
        // TODO: Connect to media engine when available
    } else if (actionId == "seek_backward") {
        updateStatusBar(tr("Seek backward hotkey pressed"));
        // TODO: Connect to media engine when available
    } else {
        updateStatusBar(tr("Unknown hotkey action: %1").arg(actionId));
    }
}
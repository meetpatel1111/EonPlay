#include "ui/SystemTrayManager.h"
#include "ui/MainWindow.h"
#include "media/IMediaEngine.h"
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(systemTray, "eonplay.systemtray")

SystemTrayManager::SystemTrayManager(QObject* parent)
    : QObject(parent)
    , m_mainWindow(nullptr)
    , m_trayIcon(nullptr)
    , m_contextMenu(nullptr)
    , m_showAction(nullptr)
    , m_hideAction(nullptr)
    , m_playAction(nullptr)
    , m_pauseAction(nullptr)
    , m_stopAction(nullptr)
    , m_nextAction(nullptr)
    , m_previousAction(nullptr)
    , m_quitAction(nullptr)
    , m_isPlaying(false)
    , m_hasMedia(false)
{
    // Load icons
    m_defaultIcon = QIcon(":/icons/app_icon.png");
    m_playingIcon = QIcon(":/icons/tray_playing.png");
    m_pausedIcon = QIcon(":/icons/tray_paused.png");
    
    // Fallback to application icon if custom icons not available
    if (m_defaultIcon.isNull()) {
        m_defaultIcon = QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
    }
    if (m_playingIcon.isNull()) {
        m_playingIcon = QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
    }
    if (m_pausedIcon.isNull()) {
        m_pausedIcon = QApplication::style()->standardIcon(QStyle::SP_MediaPause);
    }
    
    qCDebug(systemTray) << "SystemTrayManager created";
}

SystemTrayManager::~SystemTrayManager()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    qCDebug(systemTray) << "SystemTrayManager destroyed";
}

bool SystemTrayManager::initialize(MainWindow* mainWindow)
{
    if (!mainWindow) {
        qCWarning(systemTray) << "Cannot initialize with null main window";
        return false;
    }
    
    if (!isSystemTrayAvailable()) {
        qCWarning(systemTray) << "System tray is not available on this system";
        return false;
    }
    
    m_mainWindow = mainWindow;
    
    createTrayIcon();
    createContextMenu();
    
    // Connect to main window signals
    connect(this, &SystemTrayManager::showMainWindowRequested, 
            m_mainWindow, &MainWindow::showWindow);
    connect(this, &SystemTrayManager::quitRequested, 
            m_mainWindow, &MainWindow::close);
    
    qCDebug(systemTray) << "SystemTrayManager initialized successfully";
    return true;
}

void SystemTrayManager::setMediaEngine(std::shared_ptr<IMediaEngine> mediaEngine)
{
    // Disconnect from previous media engine
    if (m_mediaEngine) {
        disconnect(m_mediaEngine.get(), nullptr, this, nullptr);
    }
    
    m_mediaEngine = mediaEngine;
    
    // Connect to new media engine
    if (m_mediaEngine) {
        // TODO: Connect to media engine signals when available
        // connect(m_mediaEngine.get(), &IMediaEngine::stateChanged, 
        //         this, &SystemTrayManager::onPlaybackStateChanged);
        
        qCDebug(systemTray) << "Media engine connected";
    }
}

void SystemTrayManager::setVisible(bool visible)
{
    if (m_trayIcon) {
        m_trayIcon->setVisible(visible);
        qCDebug(systemTray) << "System tray icon" << (visible ? "shown" : "hidden");
    }
}

bool SystemTrayManager::isVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

bool SystemTrayManager::isSystemTrayAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void SystemTrayManager::showNotification(const QString& title, const QString& message, int duration)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
        
        if (duration <= 0) {
            duration = 3000; // Default 3 seconds
        }
        
        m_trayIcon->showMessage(title, message, icon, duration);
        qCDebug(systemTray) << "Notification shown:" << title << "-" << message;
    }
}

void SystemTrayManager::updateTooltip(const QString& mediaTitle, bool isPlaying)
{
    if (!m_trayIcon) {
        return;
    }
    
    QString tooltip = "EonPlay - Timeless Media Player";
    
    if (!mediaTitle.isEmpty()) {
        QString status = isPlaying ? tr("Playing") : tr("Paused");
        tooltip = QString("EonPlay - %1: %2").arg(status, mediaTitle);
    }
    
    m_trayIcon->setToolTip(tooltip);
}

void SystemTrayManager::updatePlaybackState(bool isPlaying, bool hasMedia)
{
    m_isPlaying = isPlaying;
    m_hasMedia = hasMedia;
    
    // Update context menu actions
    if (m_playAction) m_playAction->setEnabled(hasMedia && !isPlaying);
    if (m_pauseAction) m_pauseAction->setEnabled(hasMedia && isPlaying);
    if (m_stopAction) m_stopAction->setEnabled(hasMedia);
    if (m_nextAction) m_nextAction->setEnabled(hasMedia);
    if (m_previousAction) m_previousAction->setEnabled(hasMedia);
    
    // Update tray icon
    updateTrayIcon(isPlaying);
    
    // Update tooltip
    updateTooltip(m_currentMediaTitle, isPlaying);
}

void SystemTrayManager::onMediaLoaded(const QString& filePath, const QString& title)
{
    m_currentMediaTitle = title.isEmpty() ? QFileInfo(filePath).baseName() : title;
    
    // Show notification
    showNotification(tr("Media Loaded"), 
                    tr("Now playing: %1").arg(m_currentMediaTitle));
    
    // Update state
    updatePlaybackState(false, true);
    
    qCDebug(systemTray) << "Media loaded:" << m_currentMediaTitle;
}

void SystemTrayManager::onPlaybackStateChanged(bool isPlaying)
{
    updatePlaybackState(isPlaying, m_hasMedia);
    
    if (isPlaying && !m_currentMediaTitle.isEmpty()) {
        showNotification(tr("Now Playing"), m_currentMediaTitle);
    }
    
    qCDebug(systemTray) << "Playback state changed:" << (isPlaying ? "playing" : "paused");
}

void SystemTrayManager::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        // Show/hide main window on click
        if (m_mainWindow) {
            if (m_mainWindow->isVisible() && m_mainWindow->isActiveWindow()) {
                m_mainWindow->hide();
                emit hideMainWindowRequested();
            } else {
                m_mainWindow->showWindow();
                emit showMainWindowRequested();
            }
        }
        break;
    case QSystemTrayIcon::MiddleClick:
        // Toggle play/pause on middle click
        if (m_hasMedia) {
            if (m_isPlaying) {
                emit pauseRequested();
            } else {
                emit playRequested();
            }
        }
        break;
    default:
        break;
    }
}

void SystemTrayManager::onShowMainWindow()
{
    emit showMainWindowRequested();
}

void SystemTrayManager::onHideMainWindow()
{
    emit hideMainWindowRequested();
}

void SystemTrayManager::onPlay()
{
    emit playRequested();
}

void SystemTrayManager::onPause()
{
    emit pauseRequested();
}

void SystemTrayManager::onStop()
{
    emit stopRequested();
}

void SystemTrayManager::onNext()
{
    emit nextRequested();
}

void SystemTrayManager::onPrevious()
{
    emit previousRequested();
}

void SystemTrayManager::onQuit()
{
    emit quitRequested();
}

void SystemTrayManager::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(m_defaultIcon);
    m_trayIcon->setToolTip("EonPlay - Timeless Media Player");
    
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTrayManager::onTrayIconActivated);
    
    qCDebug(systemTray) << "System tray icon created";
}

void SystemTrayManager::createContextMenu()
{
    m_contextMenu = new QMenu();
    
    // Window actions
    m_showAction = m_contextMenu->addAction(tr("Show EonPlay"));
    m_showAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(m_showAction, &QAction::triggered, this, &SystemTrayManager::onShowMainWindow);
    
    m_hideAction = m_contextMenu->addAction(tr("Hide EonPlay"));
    m_hideAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarMinButton));
    connect(m_hideAction, &QAction::triggered, this, &SystemTrayManager::onHideMainWindow);
    
    m_contextMenu->addSeparator();
    
    // Playback controls
    m_playAction = m_contextMenu->addAction(tr("Play"));
    m_playAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    m_playAction->setEnabled(false);
    connect(m_playAction, &QAction::triggered, this, &SystemTrayManager::onPlay);
    
    m_pauseAction = m_contextMenu->addAction(tr("Pause"));
    m_pauseAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseAction->setEnabled(false);
    connect(m_pauseAction, &QAction::triggered, this, &SystemTrayManager::onPause);
    
    m_stopAction = m_contextMenu->addAction(tr("Stop"));
    m_stopAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this, &SystemTrayManager::onStop);
    
    m_contextMenu->addSeparator();
    
    m_previousAction = m_contextMenu->addAction(tr("Previous"));
    m_previousAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSkipBackward));
    m_previousAction->setEnabled(false);
    connect(m_previousAction, &QAction::triggered, this, &SystemTrayManager::onPrevious);
    
    m_nextAction = m_contextMenu->addAction(tr("Next"));
    m_nextAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSkipForward));
    m_nextAction->setEnabled(false);
    connect(m_nextAction, &QAction::triggered, this, &SystemTrayManager::onNext);
    
    m_contextMenu->addSeparator();
    
    // Quit action
    m_quitAction = m_contextMenu->addAction(tr("Quit EonPlay"));
    m_quitAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));
    connect(m_quitAction, &QAction::triggered, this, &SystemTrayManager::onQuit);
    
    m_trayIcon->setContextMenu(m_contextMenu);
    
    qCDebug(systemTray) << "Context menu created";
}

void SystemTrayManager::updateTrayIcon(bool isPlaying)
{
    if (!m_trayIcon) {
        return;
    }
    
    if (isPlaying) {
        m_trayIcon->setIcon(m_playingIcon);
    } else if (m_hasMedia) {
        m_trayIcon->setIcon(m_pausedIcon);
    } else {
        m_trayIcon->setIcon(m_defaultIcon);
    }
}
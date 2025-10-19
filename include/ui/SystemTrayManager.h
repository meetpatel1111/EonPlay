#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <memory>

class MainWindow;
class IMediaEngine;

/**
 * @brief System tray integration manager for EonPlay
 * 
 * Provides system tray icon with basic playback controls, notifications,
 * and quick access to main window functionality.
 */
class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(QObject* parent = nullptr);
    ~SystemTrayManager() override;
    
    /**
     * @brief Initialize system tray with main window reference
     * @param mainWindow Pointer to main window
     * @return true if initialization successful
     */
    bool initialize(MainWindow* mainWindow);
    
    /**
     * @brief Set media engine for playback control
     * @param mediaEngine Shared pointer to media engine
     */
    void setMediaEngine(std::shared_ptr<IMediaEngine> mediaEngine);
    
    /**
     * @brief Show or hide system tray icon
     * @param visible true to show tray icon
     */
    void setVisible(bool visible);
    
    /**
     * @brief Check if system tray icon is visible
     * @return true if tray icon is visible
     */
    bool isVisible() const;
    
    /**
     * @brief Check if system tray is available on this system
     * @return true if system tray is supported
     */
    static bool isSystemTrayAvailable();
    
    /**
     * @brief Show notification message
     * @param title Notification title
     * @param message Notification message
     * @param duration Duration in milliseconds (0 for system default)
     */
    void showNotification(const QString& title, const QString& message, int duration = 0);
    
    /**
     * @brief Update tray icon tooltip with current media info
     * @param mediaTitle Current media title
     * @param isPlaying Whether media is currently playing
     */
    void updateTooltip(const QString& mediaTitle = QString(), bool isPlaying = false);
    
    /**
     * @brief Update playback controls state
     * @param isPlaying Whether media is currently playing
     * @param hasMedia Whether media is loaded
     */
    void updatePlaybackState(bool isPlaying, bool hasMedia);

public slots:
    /**
     * @brief Handle media file loaded
     * @param filePath Path to loaded media file
     * @param title Media title
     */
    void onMediaLoaded(const QString& filePath, const QString& title = QString());
    
    /**
     * @brief Handle playback state changes
     * @param isPlaying Whether playback started or stopped
     */
    void onPlaybackStateChanged(bool isPlaying);

signals:
    /**
     * @brief Emitted when user requests to show main window
     */
    void showMainWindowRequested();
    
    /**
     * @brief Emitted when user requests to hide main window
     */
    void hideMainWindowRequested();
    
    /**
     * @brief Emitted when user requests playback control
     */
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();
    
    /**
     * @brief Emitted when user requests to quit application
     */
    void quitRequested();

private slots:
    /**
     * @brief Handle tray icon activation
     * @param reason Activation reason
     */
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    
    /**
     * @brief Handle context menu actions
     */
    void onShowMainWindow();
    void onHideMainWindow();
    void onPlay();
    void onPause();
    void onStop();
    void onNext();
    void onPrevious();
    void onQuit();

private:
    /**
     * @brief Create system tray icon
     */
    void createTrayIcon();
    
    /**
     * @brief Create context menu
     */
    void createContextMenu();
    
    /**
     * @brief Update tray icon based on playback state
     * @param isPlaying Whether media is playing
     */
    void updateTrayIcon(bool isPlaying);
    
    MainWindow* m_mainWindow;
    std::shared_ptr<IMediaEngine> m_mediaEngine;
    
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_contextMenu;
    
    // Context menu actions
    QAction* m_showAction;
    QAction* m_hideAction;
    QAction* m_playAction;
    QAction* m_pauseAction;
    QAction* m_stopAction;
    QAction* m_nextAction;
    QAction* m_previousAction;
    QAction* m_quitAction;
    
    // State tracking
    bool m_isPlaying;
    bool m_hasMedia;
    QString m_currentMediaTitle;
    
    // Icons
    QIcon m_defaultIcon;
    QIcon m_playingIcon;
    QIcon m_pausedIcon;
};
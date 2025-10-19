#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QWidget>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <memory>

class PlaybackControls;
class VideoWidget;
class PlaylistWidget;
class LibraryWidget;
class DragDropWidget;
class MediaInfoWidget;
class ComponentManager;
class FileUrlSupport;
class SystemTrayManager;
class MediaKeysManager;
class NotificationManager;
class HotkeyManager;

/**
 * @brief Main window for EonPlay - Timeless, futuristic media player
 * 
 * The central UI component that provides the main application window with
 * modern futuristic design, responsive layout, and comprehensive media controls.
 * Implements window state persistence and theme management for EonPlay.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    
    /**
     * @brief Initialize the main window with component manager
     * @param componentManager Pointer to the component manager
     * @return true if initialization was successful
     */
    bool initialize(ComponentManager* componentManager);
    
    /**
     * @brief Set the current theme (dark/light mode)
     * @param isDark true for dark theme, false for light theme
     */
    void setTheme(bool isDark);
    
    /**
     * @brief Get current theme state
     * @return true if dark theme is active
     */
    bool isDarkTheme() const { return m_isDarkTheme; }
    
    /**
     * @brief Toggle between dark and light themes
     */
    void toggleTheme();
    
    /**
     * @brief Set window to fullscreen mode
     * @param fullscreen true to enable fullscreen
     */
    void setFullscreen(bool fullscreen);
    
    /**
     * @brief Check if window is in fullscreen mode
     * @return true if in fullscreen mode
     */
    bool isFullscreen() const { return m_isFullscreen; }
    
    /**
     * @brief Show or hide the menu bar
     * @param visible true to show menu bar
     */
    void setMenuBarVisible(bool visible);
    
    /**
     * @brief Show or hide the status bar
     * @param visible true to show status bar
     */
    void setStatusBarVisible(bool visible);

public slots:
    /**
     * @brief Show the main window and restore state
     */
    void showWindow();
    
    /**
     * @brief Update window title with media information
     * @param mediaTitle Title of currently playing media
     */
    void updateWindowTitle(const QString& mediaTitle = QString());
    
    /**
     * @brief Update status bar with playback information
     * @param message Status message to display
     */
    void updateStatusBar(const QString& message);

signals:
    /**
     * @brief Emitted when window is closing
     */
    void windowClosing();
    
    /**
     * @brief Emitted when theme changes
     * @param isDark true if switched to dark theme
     */
    void themeChanged(bool isDark);
    
    /**
     * @brief Emitted when fullscreen mode changes
     * @param isFullscreen true if entered fullscreen
     */
    void fullscreenChanged(bool isFullscreen);

protected:
    /**
     * @brief Handle window close event
     * @param event Close event
     */
    void closeEvent(QCloseEvent* event) override;
    
    /**
     * @brief Handle window resize event
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent* event) override;
    
    /**
     * @brief Handle key press events for shortcuts
     * @param event Key press event
     */
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    /**
     * @brief Handle File menu actions
     */
    void onOpenFile();
    void onOpenUrl();
    void onRecentFile();
    void onExit();
    
    /**
     * @brief Handle View menu actions
     */
    void onToggleFullscreen();
    void onToggleMenuBar();
    void onToggleStatusBar();
    void onTogglePlaylist();
    void onToggleLibrary();
    void onToggleMediaInfo();
    
    /**
     * @brief Handle Tools menu actions
     */
    void onPreferences();
    void onToggleTheme();
    
    /**
     * @brief Handle Help menu actions
     */
    void onAbout();
    void onAboutQt();
    
    /**
     * @brief Handle drag and drop events
     */
    void onFilesDropped(const QStringList& processedFiles, const QStringList& failedFiles);
    void onUrlDropped(const QUrl& url, bool success);
    void onDragEntered(bool hasValidData);
    void onDragLeft();
    
    /**
     * @brief Handle file format validation
     */
    void onFileValidationFailed(const QString& filePath, const QString& errorMessage);
    
    /**
     * @brief Handle system integration events
     */
    void onSystemTrayActivated();
    void onMediaKeyPressed(const QString& key);
    void onNotificationClicked();
    
    /**
     * @brief Save window state periodically
     */
    void saveWindowState();

private:
    /**
     * @brief Initialize the user interface
     */
    void initializeUI();
    
    /**
     * @brief Create the menu bar
     */
    void createMenuBar();
    
    /**
     * @brief Create the status bar
     */
    void createStatusBar();
    
    /**
     * @brief Create the central widget layout
     */
    void createCentralWidget();
    
    /**
     * @brief Create keyboard shortcuts
     */
    void createShortcuts();
    
    /**
     * @brief Apply the current theme
     */
    void applyTheme();
    
    /**
     * @brief Load theme stylesheet
     * @param themeName Name of the theme (dark/light)
     * @return Stylesheet content
     */
    QString loadThemeStylesheet(const QString& themeName);
    
    /**
     * @brief Save current window state to settings
     */
    void saveState();
    
    /**
     * @brief Restore window state from settings
     */
    void restoreState();
    
    /**
     * @brief Update recent files menu
     */
    void updateRecentFilesMenu();
    
    /**
     * @brief Add file to recent files list
     * @param filePath Path to the file
     */
    void addToRecentFiles(const QString& filePath);
    
    /**
     * @brief Register system file associations
     */
    void registerFileAssociations();
    
    /**
     * @brief Check if file associations are registered
     * @return true if associations are registered
     */
    bool areFileAssociationsRegistered() const;
    
    /**
     * @brief Load custom skin
     * @param skinName Name of the skin to load
     */
    void loadCustomSkin(const QString& skinName);
    
    /**
     * @brief Get available custom skins
     * @return List of available skin names
     */
    QStringList getAvailableSkins() const;
    
    /**
     * @brief Show skin selection dialog
     */
    void showSkinSelector();
    
    /**
     * @brief Initialize system integration components
     */
    void initializeSystemIntegration();
    
    /**
     * @brief Enable or disable system tray
     * @param enabled true to enable system tray
     */
    void setSystemTrayEnabled(bool enabled);
    
    /**
     * @brief Check if system tray is enabled
     * @return true if system tray is enabled
     */
    bool isSystemTrayEnabled() const;
    
    /**
     * @brief Enable or disable media keys
     * @param enabled true to enable media keys
     */
    void setMediaKeysEnabled(bool enabled);
    
    /**
     * @brief Check if media keys are enabled
     * @return true if media keys are enabled
     */
    bool isMediaKeysEnabled() const;
    
    /**
     * @brief Enable or disable notifications
     * @param enabled true to enable notifications
     */
    void setNotificationsEnabled(bool enabled);
    
    /**
     * @brief Check if notifications are enabled
     * @return true if notifications are enabled
     */
    bool isNotificationsEnabled() const;
    
    /**
     * @brief Handle hotkey action
     * @param actionId Action identifier
     */
    void handleHotkeyAction(const QString& actionId);
    
    // Component manager and file support
    ComponentManager* m_componentManager;
    std::shared_ptr<FileUrlSupport> m_fileUrlSupport;
    
    // System integration components
    std::unique_ptr<SystemTrayManager> m_systemTrayManager;
    std::unique_ptr<MediaKeysManager> m_mediaKeysManager;
    std::unique_ptr<NotificationManager> m_notificationManager;
    std::unique_ptr<HotkeyManager> m_hotkeyManager;
    
    // UI Components
    QWidget* m_centralWidget;
    QSplitter* m_mainSplitter;
    QSplitter* m_rightSplitter;
    
    // Media display area (will be implemented in task 3.3)
    VideoWidget* m_videoWidget;
    
    // Control widgets (will be implemented in task 3.2)
    PlaybackControls* m_playbackControls;
    
    // Side panels (will be implemented in later tasks)
    PlaylistWidget* m_playlistWidget;
    LibraryWidget* m_libraryWidget;
    MediaInfoWidget* m_mediaInfoWidget;
    
    // Drag and drop support
    DragDropWidget* m_dragDropWidget;
    
    // Menu system
    QMenuBar* m_menuBar;
    QMenu* m_fileMenu;
    QMenu* m_recentFilesMenu;
    QMenu* m_viewMenu;
    QMenu* m_toolsMenu;
    QMenu* m_helpMenu;
    
    // File menu actions
    QAction* m_openFileAction;
    QAction* m_openUrlAction;
    QAction* m_exitAction;
    
    // View menu actions
    QAction* m_fullscreenAction;
    QAction* m_toggleMenuBarAction;
    QAction* m_toggleStatusBarAction;
    QAction* m_togglePlaylistAction;
    QAction* m_toggleLibraryAction;
    QAction* m_toggleMediaInfoAction;
    
    // Tools menu actions
    QAction* m_preferencesAction;
    QAction* m_toggleThemeAction;
    
    // Help menu actions
    QAction* m_aboutAction;
    QAction* m_aboutQtAction;
    
    // Status bar
    QStatusBar* m_statusBar;
    QLabel* m_statusLabel;
    QLabel* m_mediaInfoLabel;
    
    // Window state
    bool m_isDarkTheme;
    bool m_isFullscreen;
    QRect m_normalGeometry;
    QString m_currentSkin;
    
    // Settings
    std::unique_ptr<QSettings> m_settings;
    
    // Auto-save timer
    QTimer* m_saveTimer;
    
    // Recent files
    QStringList m_recentFiles;
    static const int MaxRecentFiles = 10;
};
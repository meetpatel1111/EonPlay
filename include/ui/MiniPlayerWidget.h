#ifndef MINIPLAYERWIDGET_H
#define MINIPLAYERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <memory>

// Forward declarations
class PlaybackController;
class VideoWidget;

/**
 * @brief Mini player with always-on-top floating window
 * 
 * Provides a compact, always-on-top mini player window with essential
 * playback controls and video display for convenient media control.
 */
class MiniPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Mini player modes
     */
    enum PlayerMode {
        VideoMode = 0,      // Video display with controls
        AudioMode,          // Audio-only with album art
        CompactMode         // Minimal controls only
    };

    explicit MiniPlayerWidget(QWidget* parent = nullptr);
    ~MiniPlayerWidget() override;

    /**
     * @brief Set playback controller
     * @param controller Playback controller instance
     */
    void setPlaybackController(PlaybackController* controller);

    /**
     * @brief Set video widget
     * @param videoWidget Video widget instance
     */
    void setVideoWidget(VideoWidget* videoWidget);

    /**
     * @brief Get current player mode
     * @return Current player mode
     */
    PlayerMode getPlayerMode() const;

    /**
     * @brief Set player mode
     * @param mode Player mode
     */
    void setPlayerMode(PlayerMode mode);

    /**
     * @brief Check if always on top is enabled
     * @return true if always on top
     */
    bool isAlwaysOnTop() const;

    /**
     * @brief Set always on top
     * @param alwaysOnTop Always on top state
     */
    void setAlwaysOnTop(bool alwaysOnTop);

    /**
     * @brief Check if controls are visible
     * @return true if controls are visible
     */
    bool areControlsVisible() const;

    /**
     * @brief Set controls visibility
     * @param visible Controls visibility
     */
    void setControlsVisible(bool visible);

    /**
     * @brief Get auto-hide timeout
     * @return Auto-hide timeout in milliseconds
     */
    int getAutoHideTimeout() const;

    /**
     * @brief Set auto-hide timeout
     * @param timeoutMs Auto-hide timeout in milliseconds
     */
    void setAutoHideTimeout(int timeoutMs);

    /**
     * @brief Check if window is resizable
     * @return true if resizable
     */
    bool isResizable() const;

    /**
     * @brief Set window resizable
     * @param resizable Resizable state
     */
    void setResizable(bool resizable);

    /**
     * @brief Get opacity level
     * @return Opacity (0.0 to 1.0)
     */
    float getOpacity() const;

    /**
     * @brief Set opacity level
     * @param opacity Opacity (0.0 to 1.0)
     */
    void setOpacity(float opacity);

    /**
     * @brief Show mini player
     */
    void showMiniPlayer();

    /**
     * @brief Hide mini player
     */
    void hideMiniPlayer();

    /**
     * @brief Toggle mini player visibility
     */
    void toggleMiniPlayer();

signals:
    /**
     * @brief Emitted when mini player is closed
     */
    void miniPlayerClosed();

    /**
     * @brief Emitted when player mode changes
     * @param mode New player mode
     */
    void playerModeChanged(PlayerMode mode);

    /**
     * @brief Emitted when always on top changes
     * @param alwaysOnTop New always on top state
     */
    void alwaysOnTopChanged(bool alwaysOnTop);

    /**
     * @brief Emitted when restore to main window is requested
     */
    void restoreToMainWindow();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onPreviousClicked();
    void onNextClicked();
    void onVolumeChanged(int volume);
    void onPositionChanged(int position);
    void onSeekRequested(int position);
    void onAutoHideTimeout();
    void updatePlaybackInfo();

private:
    void setupUI();
    void setupConnections();
    void createContextMenu();
    void updateControlsLayout();
    void updateWindowFlags();
    void startAutoHideTimer();
    void stopAutoHideTimer();
    void updatePlayPauseButton();
    void updatePositionSlider();
    void updateVolumeSlider();
    QString formatTime(qint64 milliseconds) const;

    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;
    QHBoxLayout* m_seekLayout;
    QHBoxLayout* m_volumeLayout;

    // Video display
    QWidget* m_videoContainer;
    VideoWidget* m_videoWidget;
    QLabel* m_albumArtLabel;

    // Controls
    QPushButton* m_playPauseButton;
    QPushButton* m_stopButton;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QPushButton* m_volumeButton;
    QPushButton* m_closeButton;

    // Sliders
    QSlider* m_positionSlider;
    QSlider* m_volumeSlider;

    // Info display
    QLabel* m_titleLabel;
    QLabel* m_timeLabel;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_videoModeAction;
    QAction* m_audioModeAction;
    QAction* m_compactModeAction;
    QAction* m_alwaysOnTopAction;
    QAction* m_resizableAction;
    QAction* m_restoreAction;

    // Controllers
    PlaybackController* m_playbackController;

    // State
    PlayerMode m_playerMode;
    bool m_alwaysOnTop;
    bool m_controlsVisible;
    bool m_resizable;
    float m_opacity;
    int m_autoHideTimeout;

    // Mouse tracking for dragging
    bool m_dragging;
    QPoint m_dragStartPosition;

    // Auto-hide timer
    QTimer* m_autoHideTimer;
    QTimer* m_updateTimer;

    // Constants
    static constexpr int DEFAULT_WIDTH = 320;
    static constexpr int DEFAULT_HEIGHT = 240;
    static constexpr int COMPACT_HEIGHT = 80;
    static constexpr int DEFAULT_AUTO_HIDE_TIMEOUT = 3000; // 3 seconds
    static constexpr float DEFAULT_OPACITY = 0.9f;
};

#endif // MINIPLAYERWIDGET_H
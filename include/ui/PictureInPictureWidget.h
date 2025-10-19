#ifndef PICTUREINPICTUREWIDGET_H
#define PICTUREINPICTUREWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QGraphicsOpacityEffect>
#include <memory>

// Forward declarations
class VideoWidget;
class PlaybackController;

/**
 * @brief Picture-in-Picture floating video window
 * 
 * Provides a floating, resizable video window that stays on top
 * with minimal controls for picture-in-picture viewing experience.
 */
class PictureInPictureWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief PiP window positions
     */
    enum WindowPosition {
        TopLeft = 0,
        TopRight,
        BottomLeft,
        BottomRight,
        Center,
        Custom
    };

    /**
     * @brief PiP control modes
     */
    enum ControlMode {
        NoControls = 0,     // Video only
        MinimalControls,    // Play/pause only
        BasicControls,      // Play/pause, volume
        FullControls        // All controls
    };

    explicit PictureInPictureWidget(QWidget* parent = nullptr);
    ~PictureInPictureWidget() override;

    /**
     * @brief Set video widget
     * @param videoWidget Video widget instance
     */
    void setVideoWidget(VideoWidget* videoWidget);

    /**
     * @brief Set playback controller
     * @param controller Playback controller instance
     */
    void setPlaybackController(PlaybackController* controller);

    /**
     * @brief Get window position
     * @return Current window position
     */
    WindowPosition getWindowPosition() const;

    /**
     * @brief Set window position
     * @param position Window position
     */
    void setWindowPosition(WindowPosition position);

    /**
     * @brief Get control mode
     * @return Current control mode
     */
    ControlMode getControlMode() const;

    /**
     * @brief Set control mode
     * @param mode Control mode
     */
    void setControlMode(ControlMode mode);

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
     * @brief Check if controls auto-hide
     * @return true if auto-hide enabled
     */
    bool isAutoHideEnabled() const;

    /**
     * @brief Set controls auto-hide
     * @param enabled Auto-hide enabled
     */
    void setAutoHideEnabled(bool enabled);

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
     * @brief Check if aspect ratio is locked
     * @return true if aspect ratio is locked
     */
    bool isAspectRatioLocked() const;

    /**
     * @brief Set aspect ratio lock
     * @param locked Aspect ratio lock state
     */
    void setAspectRatioLocked(bool locked);

    /**
     * @brief Get current aspect ratio
     * @return Aspect ratio (width/height)
     */
    float getAspectRatio() const;

    /**
     * @brief Set aspect ratio
     * @param ratio Aspect ratio (width/height)
     */
    void setAspectRatio(float ratio);

    /**
     * @brief Show picture-in-picture window
     */
    void showPiP();

    /**
     * @brief Hide picture-in-picture window
     */
    void hidePiP();

    /**
     * @brief Toggle picture-in-picture visibility
     */
    void togglePiP();

    /**
     * @brief Snap to screen edge
     */
    void snapToEdge();

signals:
    /**
     * @brief Emitted when PiP window is closed
     */
    void pipClosed();

    /**
     * @brief Emitted when window position changes
     * @param position New window position
     */
    void windowPositionChanged(WindowPosition position);

    /**
     * @brief Emitted when control mode changes
     * @param mode New control mode
     */
    void controlModeChanged(ControlMode mode);

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
    void onVolumeChanged(int volume);
    void onAutoHideTimeout();
    void updateControls();

private:
    void setupUI();
    void setupConnections();
    void createContextMenu();
    void updateControlsVisibility();
    void updateWindowPosition();
    void startAutoHideTimer();
    void stopAutoHideTimer();
    void calculateOptimalSize();
    QPoint getPositionForMode(WindowPosition position) const;
    QRect getAvailableScreenGeometry() const;

    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;

    // Video display
    QWidget* m_videoContainer;
    VideoWidget* m_videoWidget;

    // Controls
    QPushButton* m_playPauseButton;
    QPushButton* m_volumeButton;
    QPushButton* m_closeButton;
    QSlider* m_volumeSlider;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_noControlsAction;
    QAction* m_minimalControlsAction;
    QAction* m_basicControlsAction;
    QAction* m_fullControlsAction;
    QAction* m_topLeftAction;
    QAction* m_topRightAction;
    QAction* m_bottomLeftAction;
    QAction* m_bottomRightAction;
    QAction* m_centerAction;
    QAction* m_resizableAction;
    QAction* m_aspectRatioAction;
    QAction* m_autoHideAction;
    QAction* m_restoreAction;

    // Controllers
    PlaybackController* m_playbackController;

    // State
    WindowPosition m_windowPosition;
    ControlMode m_controlMode;
    bool m_resizable;
    float m_opacity;
    bool m_autoHideEnabled;
    int m_autoHideTimeout;
    bool m_aspectRatioLocked;
    float m_aspectRatio;

    // Mouse tracking for dragging and resizing
    bool m_dragging;
    bool m_resizing;
    QPoint m_dragStartPosition;
    QSize m_resizeStartSize;
    QPoint m_resizeStartPosition;

    // Auto-hide timer
    QTimer* m_autoHideTimer;
    QTimer* m_updateTimer;

    // Opacity effect
    QGraphicsOpacityEffect* m_opacityEffect;

    // Constants
    static constexpr int DEFAULT_WIDTH = 320;
    static constexpr int DEFAULT_HEIGHT = 180;
    static constexpr int MIN_WIDTH = 160;
    static constexpr int MIN_HEIGHT = 90;
    static constexpr int RESIZE_MARGIN = 10;
    static constexpr int DEFAULT_AUTO_HIDE_TIMEOUT = 2000; // 2 seconds
    static constexpr float DEFAULT_OPACITY = 1.0f;
    static constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f;
    static constexpr int SNAP_DISTANCE = 20; // Pixels from edge to snap
};

#endif // PICTUREINPICTUREWIDGET_H
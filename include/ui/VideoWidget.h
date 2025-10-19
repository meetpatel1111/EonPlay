#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QFrame>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <memory>

class VLCBackend;
class ComponentManager;
class SubtitleRenderer;
class SubtitleManager;

/**
 * @brief Video display widget for EonPlay media player
 * 
 * Provides video display area with libVLC integration, aspect ratio handling,
 * fullscreen support, and video adjustment controls (brightness, contrast, gamma).
 * Features futuristic overlay controls and video rotation/mirroring capabilities.
 */
class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget* parent = nullptr);
    ~VideoWidget() override;
    
    /**
     * @brief Initialize with component manager
     * @param componentManager Pointer to component manager
     * @return true if initialization successful
     */
    bool initialize(ComponentManager* componentManager);
    
    /**
     * @brief Set the VLC backend for video output
     * @param vlcBackend Pointer to VLC backend
     */
    void setVLCBackend(VLCBackend* vlcBackend);
    
    /**
     * @brief Set video aspect ratio mode
     * @param aspectRatio Aspect ratio string (e.g., "16:9", "4:3", "auto")
     */
    void setAspectRatio(const QString& aspectRatio);
    
    /**
     * @brief Get current aspect ratio
     * @return Current aspect ratio string
     */
    QString aspectRatio() const { return m_aspectRatio; }
    
    /**
     * @brief Set brightness adjustment (-100 to 100)
     * @param brightness Brightness value
     */
    void setBrightness(int brightness);
    
    /**
     * @brief Get current brightness
     * @return Brightness value (-100 to 100)
     */
    int brightness() const { return m_brightness; }
    
    /**
     * @brief Set contrast adjustment (-100 to 100)
     * @param contrast Contrast value
     */
    void setContrast(int contrast);
    
    /**
     * @brief Get current contrast
     * @return Contrast value (-100 to 100)
     */
    int contrast() const { return m_contrast; }
    
    /**
     * @brief Set gamma adjustment (0.1 to 10.0)
     * @param gamma Gamma value
     */
    void setGamma(double gamma);
    
    /**
     * @brief Get current gamma
     * @return Gamma value (0.1 to 10.0)
     */
    double gamma() const { return m_gamma; }
    
    /**
     * @brief Set video rotation (0, 90, 180, 270 degrees)
     * @param rotation Rotation angle in degrees
     */
    void setRotation(int rotation);
    
    /**
     * @brief Get current rotation
     * @return Rotation angle in degrees
     */
    int rotation() const { return m_rotation; }
    
    /**
     * @brief Set horizontal mirroring
     * @param mirrored true to mirror horizontally
     */
    void setHorizontalMirror(bool mirrored);
    
    /**
     * @brief Check if horizontally mirrored
     * @return true if horizontally mirrored
     */
    bool isHorizontalMirrored() const { return m_horizontalMirror; }
    
    /**
     * @brief Set vertical mirroring
     * @param mirrored true to mirror vertically
     */
    void setVerticalMirror(bool mirrored);
    
    /**
     * @brief Check if vertically mirrored
     * @return true if vertically mirrored
     */
    bool isVerticalMirrored() const { return m_verticalMirror; }
    
    /**
     * @brief Enter or exit fullscreen mode
     * @param fullscreen true for fullscreen
     */
    void setFullscreen(bool fullscreen);
    
    /**
     * @brief Check if in fullscreen mode
     * @return true if in fullscreen
     */
    bool isFullscreen() const { return m_isFullscreen; }
    
    /**
     * @brief Show or hide video controls overlay
     * @param visible true to show controls
     */
    void setControlsVisible(bool visible);
    
    /**
     * @brief Check if controls are visible
     * @return true if controls are visible
     */
    bool areControlsVisible() const { return m_controlsVisible; }
    
    /**
     * @brief Reset all video adjustments to default
     */
    void resetVideoAdjustments();
    
    /**
     * @brief Get subtitle renderer
     * @return Subtitle renderer instance
     */
    SubtitleRenderer* subtitleRenderer() const { return m_subtitleRenderer; }
    
    /**
     * @brief Get subtitle manager
     * @return Subtitle manager instance
     */
    SubtitleManager* subtitleManager() const { return m_subtitleManager; }

public slots:
    /**
     * @brief Toggle fullscreen mode
     */
    void toggleFullscreen();
    
    /**
     * @brief Toggle controls visibility
     */
    void toggleControls();
    
    /**
     * @brief Take a screenshot of current video frame
     */
    void takeScreenshot();
    
    /**
     * @brief Show video adjustment controls
     */
    void showVideoAdjustments();
    
    /**
     * @brief Hide video adjustment controls
     */
    void hideVideoAdjustments();

signals:
    /**
     * @brief Emitted when fullscreen mode changes
     * @param isFullscreen true if entered fullscreen
     */
    void fullscreenChanged(bool isFullscreen);
    
    /**
     * @brief Emitted when video adjustments change
     * @param brightness Brightness value
     * @param contrast Contrast value
     * @param gamma Gamma value
     */
    void videoAdjustmentsChanged(int brightness, int contrast, double gamma);
    
    /**
     * @brief Emitted when video transformation changes
     * @param rotation Rotation angle
     * @param horizontalMirror Horizontal mirror state
     * @param verticalMirror Vertical mirror state
     */
    void videoTransformChanged(int rotation, bool horizontalMirror, bool verticalMirror);
    
    /**
     * @brief Emitted when screenshot is taken
     * @param filePath Path to saved screenshot
     */
    void screenshotTaken(const QString& filePath);
    
    /**
     * @brief Emitted when aspect ratio changes
     * @param aspectRatio New aspect ratio
     */
    void aspectRatioChanged(const QString& aspectRatio);

protected:
    /**
     * @brief Handle mouse press events
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent* event) override;
    
    /**
     * @brief Handle mouse double-click events
     * @param event Mouse double-click event
     */
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    
    /**
     * @brief Handle mouse move events
     * @param event Mouse move event
     */
    void mouseMoveEvent(QMouseEvent* event) override;
    
    /**
     * @brief Handle key press events
     * @param event Key press event
     */
    void keyPressEvent(QKeyEvent* event) override;
    
    /**
     * @brief Handle resize events
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent* event) override;
    
    /**
     * @brief Handle context menu events
     * @param event Context menu event
     */
    void contextMenuEvent(QContextMenuEvent* event) override;
    
    /**
     * @brief Handle paint events
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;

private slots:
    /**
     * @brief Handle brightness slider change
     * @param value New brightness value
     */
    void onBrightnessChanged(int value);
    
    /**
     * @brief Handle contrast slider change
     * @param value New contrast value
     */
    void onContrastChanged(int value);
    
    /**
     * @brief Handle gamma slider change
     * @param value New gamma value
     */
    void onGammaChanged(int value);
    
    /**
     * @brief Handle rotation button clicks
     */
    void onRotateClockwise();
    void onRotateCounterClockwise();
    
    /**
     * @brief Handle mirror button clicks
     */
    void onToggleHorizontalMirror();
    void onToggleVerticalMirror();
    
    /**
     * @brief Handle aspect ratio selection
     */
    void onAspectRatioChanged();
    
    /**
     * @brief Reset video adjustments to defaults
     */
    void onResetAdjustments();
    
    /**
     * @brief Hide controls after timeout
     */
    void onHideControlsTimeout();
    
    /**
     * @brief Update controls opacity animation
     */
    void updateControlsOpacity();

private:
    /**
     * @brief Initialize the user interface
     */
    void initializeUI();
    
    /**
     * @brief Initialize subtitle components
     */
    void initializeSubtitleComponents();
    
    /**
     * @brief Create video display area
     */
    void createVideoDisplay();
    
    /**
     * @brief Create overlay controls
     */
    void createOverlayControls();
    
    /**
     * @brief Create context menu
     */
    void createContextMenu();
    
    /**
     * @brief Create video adjustment controls
     */
    void createVideoAdjustmentControls();
    
    /**
     * @brief Apply futuristic styling
     */
    void applyFuturisticStyling();
    
    /**
     * @brief Update video output window
     */
    void updateVideoOutput();
    
    /**
     * @brief Apply video adjustments to VLC
     */
    void applyVideoAdjustments();
    
    /**
     * @brief Apply video transformations to VLC
     */
    void applyVideoTransformations();
    
    /**
     * @brief Calculate aspect ratio from string
     * @param aspectRatio Aspect ratio string
     * @return Calculated ratio value
     */
    double calculateAspectRatio(const QString& aspectRatio) const;
    
    /**
     * @brief Show controls with animation
     */
    void showControlsAnimated();
    
    /**
     * @brief Hide controls with animation
     */
    void hideControlsAnimated();
    
    /**
     * @brief Start controls hide timer
     */
    void startControlsHideTimer();
    
    /**
     * @brief Stop controls hide timer
     */
    void stopControlsHideTimer();
    
    // Component manager and VLC backend
    ComponentManager* m_componentManager;
    VLCBackend* m_vlcBackend;
    
    // Subtitle components
    SubtitleRenderer* m_subtitleRenderer;
    SubtitleManager* m_subtitleManager;
    
    // Main layout and video display
    QVBoxLayout* m_mainLayout;
    QWidget* m_videoDisplayWidget;
    QLabel* m_placeholderLabel;
    
    // Overlay controls
    QWidget* m_overlayWidget;
    QVBoxLayout* m_overlayLayout;
    QHBoxLayout* m_overlayButtonsLayout;
    QPushButton* m_fullscreenButton;
    QPushButton* m_screenshotButton;
    QPushButton* m_adjustmentsButton;
    
    // Video adjustment controls
    QWidget* m_adjustmentWidget;
    QVBoxLayout* m_adjustmentLayout;
    QSlider* m_brightnessSlider;
    QSlider* m_contrastSlider;
    QSlider* m_gammaSlider;
    QLabel* m_brightnessLabel;
    QLabel* m_contrastLabel;
    QLabel* m_gammaLabel;
    QPushButton* m_resetAdjustmentsButton;
    
    // Transformation controls
    QPushButton* m_rotateClockwiseButton;
    QPushButton* m_rotateCounterClockwiseButton;
    QPushButton* m_horizontalMirrorButton;
    QPushButton* m_verticalMirrorButton;
    
    // Context menu
    QMenu* m_contextMenu;
    QAction* m_fullscreenAction;
    QAction* m_screenshotAction;
    QAction* m_adjustmentsAction;
    QMenu* m_aspectRatioMenu;
    QAction* m_aspectRatioAuto;
    QAction* m_aspectRatio16_9;
    QAction* m_aspectRatio4_3;
    QAction* m_aspectRatio1_1;
    
    // Video properties
    QString m_aspectRatio;
    int m_brightness;
    int m_contrast;
    double m_gamma;
    int m_rotation;
    bool m_horizontalMirror;
    bool m_verticalMirror;
    
    // State variables
    bool m_isFullscreen;
    bool m_controlsVisible;
    bool m_adjustmentsVisible;
    QRect m_normalGeometry;
    
    // Animation and timers
    QTimer* m_controlsHideTimer;
    QPropertyAnimation* m_controlsOpacityAnimation;
    QGraphicsOpacityEffect* m_overlayOpacityEffect;
    
    // Mouse tracking
    QPoint m_lastMousePosition;
    QTimer* m_mouseMoveTimer;
    
    // Predefined aspect ratios
    static const QStringList AspectRatios;
};
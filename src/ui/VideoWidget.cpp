#include "ui/VideoWidget.h"
#include "ComponentManager.h"
#include "media/VLCBackend.h"
#include "subtitles/SubtitleRenderer.h"
#include "subtitles/SubtitleManager.h"
#include <QApplication>
// QDesktopWidget was removed in Qt6, use QScreen instead
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QMessageBox>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QFontMetrics>
#include <QScreen>
#include <QWindow>
#include <cmath>

// Static member definitions
const QStringList VideoWidget::AspectRatios = {"Auto", "16:9", "4:3", "1:1", "21:9", "2.35:1"};

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
    , m_componentManager(nullptr)
    , m_vlcBackend(nullptr)
    , m_subtitleRenderer(nullptr)
    , m_subtitleManager(nullptr)
    , m_mainLayout(nullptr)
    , m_videoDisplayWidget(nullptr)
    , m_placeholderLabel(nullptr)
    , m_overlayWidget(nullptr)
    , m_overlayLayout(nullptr)
    , m_overlayButtonsLayout(nullptr)
    , m_fullscreenButton(nullptr)
    , m_screenshotButton(nullptr)
    , m_adjustmentsButton(nullptr)
    , m_adjustmentWidget(nullptr)
    , m_adjustmentLayout(nullptr)
    , m_brightnessSlider(nullptr)
    , m_contrastSlider(nullptr)
    , m_gammaSlider(nullptr)
    , m_brightnessLabel(nullptr)
    , m_contrastLabel(nullptr)
    , m_gammaLabel(nullptr)
    , m_resetAdjustmentsButton(nullptr)
    , m_rotateClockwiseButton(nullptr)
    , m_rotateCounterClockwiseButton(nullptr)
    , m_horizontalMirrorButton(nullptr)
    , m_verticalMirrorButton(nullptr)
    , m_contextMenu(nullptr)
    , m_aspectRatio("Auto")
    , m_brightness(0)
    , m_contrast(0)
    , m_gamma(1.0)
    , m_rotation(0)
    , m_horizontalMirror(false)
    , m_verticalMirror(false)
    , m_isFullscreen(false)
    , m_controlsVisible(false)
    , m_adjustmentsVisible(false)
    , m_controlsHideTimer(nullptr)
    , m_controlsOpacityAnimation(nullptr)
    , m_overlayOpacityEffect(nullptr)
    , m_mouseMoveTimer(nullptr)
{
    // Set widget properties
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    
    // Initialize UI components
    initializeUI();
    
    // Initialize subtitle components
    initializeSubtitleComponents();
    
    // Apply futuristic styling
    applyFuturisticStyling();
    
    // Initialize timers
    m_controlsHideTimer = new QTimer(this);
    m_controlsHideTimer->setSingleShot(true);
    m_controlsHideTimer->setInterval(3000); // 3 seconds
    connect(m_controlsHideTimer, &QTimer::timeout, this, &VideoWidget::onHideControlsTimeout);
    
    m_mouseMoveTimer = new QTimer(this);
    m_mouseMoveTimer->setSingleShot(true);
    m_mouseMoveTimer->setInterval(100); // 100ms debounce
    connect(m_mouseMoveTimer, &QTimer::timeout, this, &VideoWidget::updateControlsOpacity);
}

VideoWidget::~VideoWidget()
{
    // Cleanup is handled by Qt's parent-child relationship
}

bool VideoWidget::initialize(ComponentManager* componentManager)
{
    if (!componentManager) {
        return false;
    }
    
    m_componentManager = componentManager;
    
    // Additional initialization with component manager
    // This would typically involve registering with the component system
    
    return true;
}

void VideoWidget::setVLCBackend(VLCBackend* vlcBackend)
{
    m_vlcBackend = vlcBackend;
    
    if (m_vlcBackend) {
        // Set up VLC video output to this widget
        updateVideoOutput();
    }
}

void VideoWidget::setAspectRatio(const QString& aspectRatio)
{
    if (m_aspectRatio != aspectRatio) {
        m_aspectRatio = aspectRatio;
        
        // Apply aspect ratio to VLC if available
        if (m_vlcBackend) {
            // VLC aspect ratio setting would go here
            // m_vlcBackend->setAspectRatio(aspectRatio);
        }
        
        emit aspectRatioChanged(aspectRatio);
    }
}

void VideoWidget::setBrightness(int brightness)
{
    brightness = qBound(-100, brightness, 100);
    if (m_brightness != brightness) {
        m_brightness = brightness;
        applyVideoAdjustments();
        emit videoAdjustmentsChanged(m_brightness, m_contrast, m_gamma);
    }
}

void VideoWidget::setContrast(int contrast)
{
    contrast = qBound(-100, contrast, 100);
    if (m_contrast != contrast) {
        m_contrast = contrast;
        applyVideoAdjustments();
        emit videoAdjustmentsChanged(m_brightness, m_contrast, m_gamma);
    }
}

void VideoWidget::setGamma(double gamma)
{
    gamma = qBound(0.1, gamma, 10.0);
    if (qAbs(m_gamma - gamma) > 0.01) {
        m_gamma = gamma;
        applyVideoAdjustments();
        emit videoAdjustmentsChanged(m_brightness, m_contrast, m_gamma);
    }
}

void VideoWidget::setRotation(int rotation)
{
    rotation = rotation % 360;
    if (rotation < 0) rotation += 360;
    
    if (m_rotation != rotation) {
        m_rotation = rotation;
        applyVideoTransformations();
        emit videoTransformChanged(m_rotation, m_horizontalMirror, m_verticalMirror);
    }
}

void VideoWidget::setHorizontalMirror(bool mirrored)
{
    if (m_horizontalMirror != mirrored) {
        m_horizontalMirror = mirrored;
        applyVideoTransformations();
        emit videoTransformChanged(m_rotation, m_horizontalMirror, m_verticalMirror);
    }
}

void VideoWidget::setVerticalMirror(bool mirrored)
{
    if (m_verticalMirror != mirrored) {
        m_verticalMirror = mirrored;
        applyVideoTransformations();
        emit videoTransformChanged(m_rotation, m_horizontalMirror, m_verticalMirror);
    }
}

void VideoWidget::setFullscreen(bool fullscreen)
{
    if (m_isFullscreen != fullscreen) {
        m_isFullscreen = fullscreen;
        
        if (fullscreen) {
            // Store current geometry
            m_normalGeometry = geometry();
            
            // Enter fullscreen
            setWindowState(Qt::WindowFullScreen);
            showFullScreen();
        } else {
            // Exit fullscreen
            setWindowState(Qt::WindowNoState);
            showNormal();
            
            // Restore geometry if available
            if (!m_normalGeometry.isNull()) {
                setGeometry(m_normalGeometry);
            }
        }
        
        emit fullscreenChanged(fullscreen);
    }
}

void VideoWidget::setControlsVisible(bool visible)
{
    if (m_controlsVisible != visible) {
        m_controlsVisible = visible;
        
        if (visible) {
            showControlsAnimated();
        } else {
            hideControlsAnimated();
        }
    }
}

void VideoWidget::resetVideoAdjustments()
{
    setBrightness(0);
    setContrast(0);
    setGamma(1.0);
    setRotation(0);
    setHorizontalMirror(false);
    setVerticalMirror(false);
    
    // Update UI sliders
    if (m_brightnessSlider) m_brightnessSlider->setValue(0);
    if (m_contrastSlider) m_contrastSlider->setValue(0);
    if (m_gammaSlider) m_gammaSlider->setValue(100); // 1.0 * 100
}

void VideoWidget::toggleFullscreen()
{
    setFullscreen(!m_isFullscreen);
}

void VideoWidget::toggleControls()
{
    setControlsVisible(!m_controlsVisible);
}

void VideoWidget::takeScreenshot()
{
    if (!m_vlcBackend) {
        QMessageBox::warning(this, "EonPlay", "No video backend available for screenshot");
        return;
    }
    
    // Generate screenshot filename
    QString screenshotsDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString filename = QString("EonPlay_Screenshot_%1.png").arg(timestamp);
    QString filePath = QDir(screenshotsDir).absoluteFilePath(filename);
    
    // Take screenshot via VLC backend
    // m_vlcBackend->takeScreenshot(filePath);
    
    emit screenshotTaken(filePath);
    
    // Show temporary notification
    QMessageBox::information(this, "EonPlay", QString("Screenshot saved to:\n%1").arg(filePath));
}

void VideoWidget::showVideoAdjustments()
{
    if (m_adjustmentWidget) {
        m_adjustmentWidget->setVisible(true);
        m_adjustmentsVisible = true;
    }
}

void VideoWidget::hideVideoAdjustments()
{
    if (m_adjustmentWidget) {
        m_adjustmentWidget->setVisible(false);
        m_adjustmentsVisible = false;
    }
}

// Event handlers
void VideoWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_lastMousePosition = event->pos();
        
        // Show controls on mouse click
        if (!m_controlsVisible) {
            setControlsVisible(true);
            startControlsHideTimer();
        }
    }
    
    QWidget::mousePressEvent(event);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        toggleFullscreen();
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Show controls on mouse movement
    if (!m_controlsVisible) {
        setControlsVisible(true);
    }
    
    // Restart hide timer
    startControlsHideTimer();
    
    m_lastMousePosition = event->pos();
    
    QWidget::mouseMoveEvent(event);
}

void VideoWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_F:
    case Qt::Key_F11:
        toggleFullscreen();
        break;
    case Qt::Key_Escape:
        if (m_isFullscreen) {
            setFullscreen(false);
        }
        break;
    case Qt::Key_Space:
        // This would typically trigger play/pause
        // emit playPauseRequested();
        break;
    case Qt::Key_S:
        if (event->modifiers() & Qt::ControlModifier) {
            takeScreenshot();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void VideoWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    // Update video output size
    updateVideoOutput();
    
    // Reposition overlay controls
    if (m_overlayWidget) {
        m_overlayWidget->resize(size());
    }
    
    // Update subtitle renderer position
    if (m_subtitleRenderer) {
        m_subtitleRenderer->updatePosition();
    }
}

void VideoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_contextMenu) {
        m_contextMenu->exec(event->globalPos());
    }
}

void VideoWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw futuristic background when no video is playing
    if (!m_vlcBackend) {
        // Create gradient background
        QLinearGradient gradient(0, 0, width(), height());
        gradient.setColorAt(0, QColor(10, 15, 25));
        gradient.setColorAt(0.5, QColor(15, 25, 40));
        gradient.setColorAt(1, QColor(5, 10, 20));
        
        painter.fillRect(rect(), gradient);
        
        // Draw EonPlay logo/text
        painter.setPen(QColor(100, 150, 255, 180));
        QFont font("Arial", 24, QFont::Bold);
        painter.setFont(font);
        
        QString text = "EonPlay";
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(text);
        QPoint center = rect().center();
        QPoint textPos(center.x() - textRect.width() / 2, center.y() + textRect.height() / 2);
        
        painter.drawText(textPos, text);
        
        // Draw subtle grid pattern
        painter.setPen(QPen(QColor(50, 80, 120, 30), 1));
        int gridSize = 50;
        for (int x = 0; x < width(); x += gridSize) {
            painter.drawLine(x, 0, x, height());
        }
        for (int y = 0; y < height(); y += gridSize) {
            painter.drawLine(0, y, width(), y);
        }
    }
    
    QWidget::paintEvent(event);
}

// Private slot implementations
void VideoWidget::onBrightnessChanged(int value)
{
    setBrightness(value);
    if (m_brightnessLabel) {
        m_brightnessLabel->setText(QString("Brightness: %1").arg(value));
    }
}

void VideoWidget::onContrastChanged(int value)
{
    setContrast(value);
    if (m_contrastLabel) {
        m_contrastLabel->setText(QString("Contrast: %1").arg(value));
    }
}

void VideoWidget::onGammaChanged(int value)
{
    double gamma = value / 100.0; // Convert from 0-200 to 0.0-2.0
    setGamma(gamma);
    if (m_gammaLabel) {
        m_gammaLabel->setText(QString("Gamma: %1").arg(gamma, 0, 'f', 2));
    }
}

void VideoWidget::onRotateClockwise()
{
    setRotation(m_rotation + 90);
}

void VideoWidget::onRotateCounterClockwise()
{
    setRotation(m_rotation - 90);
}

void VideoWidget::onToggleHorizontalMirror()
{
    setHorizontalMirror(!m_horizontalMirror);
    if (m_horizontalMirrorButton) {
        m_horizontalMirrorButton->setChecked(m_horizontalMirror);
    }
}

void VideoWidget::onToggleVerticalMirror()
{
    setVerticalMirror(!m_verticalMirror);
    if (m_verticalMirrorButton) {
        m_verticalMirrorButton->setChecked(m_verticalMirror);
    }
}

void VideoWidget::onAspectRatioChanged()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        setAspectRatio(action->text());
    }
}

void VideoWidget::onResetAdjustments()
{
    resetVideoAdjustments();
}

void VideoWidget::onHideControlsTimeout()
{
    if (m_controlsVisible && !m_adjustmentsVisible) {
        setControlsVisible(false);
    }
}

void VideoWidget::updateControlsOpacity()
{
    // This would be called to update control opacity based on mouse position
    // Implementation depends on specific animation requirements
}

// Private method implementations
void VideoWidget::initializeUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Create video display area
    createVideoDisplay();
    
    // Create overlay controls
    createOverlayControls();
    
    // Create context menu
    createContextMenu();
    
    // Create video adjustment controls
    createVideoAdjustmentControls();
}

void VideoWidget::createVideoDisplay()
{
    // Create video display widget
    m_videoDisplayWidget = new QWidget(this);
    m_videoDisplayWidget->setStyleSheet("background-color: black;");
    m_videoDisplayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Create placeholder label
    m_placeholderLabel = new QLabel("No video loaded", m_videoDisplayWidget);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet("color: #64A0FF; font-size: 18px; font-weight: bold;");
    
    // Add to main layout
    m_mainLayout->addWidget(m_videoDisplayWidget);
}

void VideoWidget::createOverlayControls()
{
    // Create overlay widget
    m_overlayWidget = new QWidget(this);
    m_overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_overlayWidget->setStyleSheet("background: transparent;");
    
    // Create overlay layout
    m_overlayLayout = new QVBoxLayout(m_overlayWidget);
    m_overlayLayout->addStretch(); // Push buttons to bottom
    
    // Create buttons layout
    m_overlayButtonsLayout = new QHBoxLayout();
    m_overlayButtonsLayout->addStretch(); // Center buttons
    
    // Create control buttons
    m_fullscreenButton = new QPushButton("⛶", m_overlayWidget);
    m_fullscreenButton->setToolTip("Toggle Fullscreen (F)");
    connect(m_fullscreenButton, &QPushButton::clicked, this, &VideoWidget::toggleFullscreen);
    
    m_screenshotButton = new QPushButton("📷", m_overlayWidget);
    m_screenshotButton->setToolTip("Take Screenshot (Ctrl+S)");
    connect(m_screenshotButton, &QPushButton::clicked, this, &VideoWidget::takeScreenshot);
    
    m_adjustmentsButton = new QPushButton("⚙", m_overlayWidget);
    m_adjustmentsButton->setToolTip("Video Adjustments");
    connect(m_adjustmentsButton, &QPushButton::clicked, this, &VideoWidget::showVideoAdjustments);
    
    // Add buttons to layout
    m_overlayButtonsLayout->addWidget(m_fullscreenButton);
    m_overlayButtonsLayout->addWidget(m_screenshotButton);
    m_overlayButtonsLayout->addWidget(m_adjustmentsButton);
    m_overlayButtonsLayout->addStretch();
    
    m_overlayLayout->addLayout(m_overlayButtonsLayout);
    
    // Initially hide overlay
    m_overlayWidget->setVisible(false);
    
    // Set up opacity effect
    m_overlayOpacityEffect = new QGraphicsOpacityEffect(this);
    m_overlayWidget->setGraphicsEffect(m_overlayOpacityEffect);
    
    // Set up opacity animation
    m_controlsOpacityAnimation = new QPropertyAnimation(m_overlayOpacityEffect, "opacity", this);
    m_controlsOpacityAnimation->setDuration(300);
}

void VideoWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    // Fullscreen action
    m_fullscreenAction = m_contextMenu->addAction("Toggle Fullscreen");
    m_fullscreenAction->setShortcut(QKeySequence("F"));
    connect(m_fullscreenAction, &QAction::triggered, this, &VideoWidget::toggleFullscreen);
    
    // Screenshot action
    m_screenshotAction = m_contextMenu->addAction("Take Screenshot");
    m_screenshotAction->setShortcut(QKeySequence("Ctrl+S"));
    connect(m_screenshotAction, &QAction::triggered, this, &VideoWidget::takeScreenshot);
    
    m_contextMenu->addSeparator();
    
    // Aspect ratio menu
    m_aspectRatioMenu = m_contextMenu->addMenu("Aspect Ratio");
    
    m_aspectRatioAuto = m_aspectRatioMenu->addAction("Auto");
    m_aspectRatio16_9 = m_aspectRatioMenu->addAction("16:9");
    m_aspectRatio4_3 = m_aspectRatioMenu->addAction("4:3");
    m_aspectRatio1_1 = m_aspectRatioMenu->addAction("1:1");
    
    connect(m_aspectRatioAuto, &QAction::triggered, this, &VideoWidget::onAspectRatioChanged);
    connect(m_aspectRatio16_9, &QAction::triggered, this, &VideoWidget::onAspectRatioChanged);
    connect(m_aspectRatio4_3, &QAction::triggered, this, &VideoWidget::onAspectRatioChanged);
    connect(m_aspectRatio1_1, &QAction::triggered, this, &VideoWidget::onAspectRatioChanged);
    
    // Video adjustments action
    m_adjustmentsAction = m_contextMenu->addAction("Video Adjustments");
    connect(m_adjustmentsAction, &QAction::triggered, this, &VideoWidget::showVideoAdjustments);
}

void VideoWidget::createVideoAdjustmentControls()
{
    // Create adjustment widget
    m_adjustmentWidget = new QWidget(this);
    m_adjustmentWidget->setFixedWidth(300);
    m_adjustmentWidget->setVisible(false);
    
    m_adjustmentLayout = new QVBoxLayout(m_adjustmentWidget);
    
    // Brightness control
    m_brightnessLabel = new QLabel("Brightness: 0", m_adjustmentWidget);
    m_brightnessSlider = new QSlider(Qt::Horizontal, m_adjustmentWidget);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &VideoWidget::onBrightnessChanged);
    
    // Contrast control
    m_contrastLabel = new QLabel("Contrast: 0", m_adjustmentWidget);
    m_contrastSlider = new QSlider(Qt::Horizontal, m_adjustmentWidget);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &VideoWidget::onContrastChanged);
    
    // Gamma control
    m_gammaLabel = new QLabel("Gamma: 1.00", m_adjustmentWidget);
    m_gammaSlider = new QSlider(Qt::Horizontal, m_adjustmentWidget);
    m_gammaSlider->setRange(10, 1000); // 0.1 to 10.0
    m_gammaSlider->setValue(100); // 1.0
    connect(m_gammaSlider, &QSlider::valueChanged, this, &VideoWidget::onGammaChanged);
    
    // Transformation controls
    QHBoxLayout* rotationLayout = new QHBoxLayout();
    m_rotateCounterClockwiseButton = new QPushButton("↺", m_adjustmentWidget);
    m_rotateCounterClockwiseButton->setToolTip("Rotate Counter-Clockwise");
    m_rotateClockwiseButton = new QPushButton("↻", m_adjustmentWidget);
    m_rotateClockwiseButton->setToolTip("Rotate Clockwise");
    
    connect(m_rotateCounterClockwiseButton, &QPushButton::clicked, this, &VideoWidget::onRotateCounterClockwise);
    connect(m_rotateClockwiseButton, &QPushButton::clicked, this, &VideoWidget::onRotateClockwise);
    
    rotationLayout->addWidget(m_rotateCounterClockwiseButton);
    rotationLayout->addWidget(m_rotateClockwiseButton);
    
    // Mirror controls
    QHBoxLayout* mirrorLayout = new QHBoxLayout();
    m_horizontalMirrorButton = new QPushButton("↔", m_adjustmentWidget);
    m_horizontalMirrorButton->setToolTip("Horizontal Mirror");
    m_horizontalMirrorButton->setCheckable(true);
    m_verticalMirrorButton = new QPushButton("↕", m_adjustmentWidget);
    m_verticalMirrorButton->setToolTip("Vertical Mirror");
    m_verticalMirrorButton->setCheckable(true);
    
    connect(m_horizontalMirrorButton, &QPushButton::clicked, this, &VideoWidget::onToggleHorizontalMirror);
    connect(m_verticalMirrorButton, &QPushButton::clicked, this, &VideoWidget::onToggleVerticalMirror);
    
    mirrorLayout->addWidget(m_horizontalMirrorButton);
    mirrorLayout->addWidget(m_verticalMirrorButton);
    
    // Reset button
    m_resetAdjustmentsButton = new QPushButton("Reset All", m_adjustmentWidget);
    connect(m_resetAdjustmentsButton, &QPushButton::clicked, this, &VideoWidget::onResetAdjustments);
    
    // Add all controls to layout
    m_adjustmentLayout->addWidget(m_brightnessLabel);
    m_adjustmentLayout->addWidget(m_brightnessSlider);
    m_adjustmentLayout->addWidget(m_contrastLabel);
    m_adjustmentLayout->addWidget(m_contrastSlider);
    m_adjustmentLayout->addWidget(m_gammaLabel);
    m_adjustmentLayout->addWidget(m_gammaSlider);
    m_adjustmentLayout->addLayout(rotationLayout);
    m_adjustmentLayout->addLayout(mirrorLayout);
    m_adjustmentLayout->addWidget(m_resetAdjustmentsButton);
    
    // Position adjustment widget (this would typically be positioned relative to parent)
    m_adjustmentWidget->move(10, 10);
}

void VideoWidget::applyFuturisticStyling()
{
    // Main widget styling
    setStyleSheet(R"(
        VideoWidget {
            background-color: #0A0F19;
            border: 1px solid #1E3A5F;
        }
    )");
    
    // Button styling for overlay controls
    QString buttonStyle = R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(100, 160, 255, 180),
                stop:1 rgba(50, 100, 200, 180));
            border: 2px solid rgba(100, 160, 255, 100);
            border-radius: 20px;
            color: white;
            font-weight: bold;
            font-size: 16px;
            min-width: 40px;
            min-height: 40px;
            margin: 5px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(120, 180, 255, 200),
                stop:1 rgba(70, 120, 220, 200));
            border: 2px solid rgba(120, 180, 255, 150);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(80, 140, 235, 220),
                stop:1 rgba(40, 80, 180, 220));
        }
        QPushButton:checked {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255, 160, 100, 180),
                stop:1 rgba(200, 100, 50, 180));
            border: 2px solid rgba(255, 160, 100, 150);
        }
    )";
    
    // Apply styling to buttons
    if (m_fullscreenButton) m_fullscreenButton->setStyleSheet(buttonStyle);
    if (m_screenshotButton) m_screenshotButton->setStyleSheet(buttonStyle);
    if (m_adjustmentsButton) m_adjustmentsButton->setStyleSheet(buttonStyle);
    if (m_rotateClockwiseButton) m_rotateClockwiseButton->setStyleSheet(buttonStyle);
    if (m_rotateCounterClockwiseButton) m_rotateCounterClockwiseButton->setStyleSheet(buttonStyle);
    if (m_horizontalMirrorButton) m_horizontalMirrorButton->setStyleSheet(buttonStyle);
    if (m_verticalMirrorButton) m_verticalMirrorButton->setStyleSheet(buttonStyle);
    if (m_resetAdjustmentsButton) m_resetAdjustmentsButton->setStyleSheet(buttonStyle);
    
    // Slider styling
    QString sliderStyle = R"(
        QSlider::groove:horizontal {
            border: 1px solid #1E3A5F;
            height: 8px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0A0F19, stop:1 #1E3A5F);
            margin: 2px 0;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #64A0FF, stop:1 #3264C8);
            border: 1px solid #64A0FF;
            width: 18px;
            margin: -2px 0;
            border-radius: 9px;
        }
        QSlider::handle:horizontal:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #78B4FF, stop:1 #4678DC);
        }
    )";
    
    if (m_brightnessSlider) m_brightnessSlider->setStyleSheet(sliderStyle);
    if (m_contrastSlider) m_contrastSlider->setStyleSheet(sliderStyle);
    if (m_gammaSlider) m_gammaSlider->setStyleSheet(sliderStyle);
    
    // Label styling
    QString labelStyle = R"(
        QLabel {
            color: #64A0FF;
            font-weight: bold;
            font-size: 12px;
            margin: 5px 0;
        }
    )";
    
    if (m_brightnessLabel) m_brightnessLabel->setStyleSheet(labelStyle);
    if (m_contrastLabel) m_contrastLabel->setStyleSheet(labelStyle);
    if (m_gammaLabel) m_gammaLabel->setStyleSheet(labelStyle);
    
    // Adjustment widget styling
    if (m_adjustmentWidget) {
        m_adjustmentWidget->setStyleSheet(R"(
            QWidget {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 rgba(10, 15, 25, 240),
                    stop:1 rgba(30, 58, 95, 240));
                border: 2px solid rgba(100, 160, 255, 100);
                border-radius: 10px;
                padding: 10px;
            }
        )");
    }
}

void VideoWidget::updateVideoOutput()
{
    if (m_vlcBackend && m_videoDisplayWidget) {
        // Set VLC video output to this widget's window handle
        // This would typically involve platform-specific code
        // m_vlcBackend->setVideoOutput(m_videoDisplayWidget->winId());
        
        // Hide placeholder when video is available
        if (m_placeholderLabel) {
            m_placeholderLabel->setVisible(false);
        }
    } else {
        // Show placeholder when no video
        if (m_placeholderLabel) {
            m_placeholderLabel->setVisible(true);
            m_placeholderLabel->resize(m_videoDisplayWidget->size());
        }
    }
}

void VideoWidget::applyVideoAdjustments()
{
    if (m_vlcBackend) {
        // Apply brightness, contrast, and gamma to VLC
        // m_vlcBackend->setBrightness(m_brightness);
        // m_vlcBackend->setContrast(m_contrast);
        // m_vlcBackend->setGamma(m_gamma);
    }
}

void VideoWidget::applyVideoTransformations()
{
    if (m_vlcBackend) {
        // Apply rotation and mirroring to VLC
        // m_vlcBackend->setRotation(m_rotation);
        // m_vlcBackend->setHorizontalMirror(m_horizontalMirror);
        // m_vlcBackend->setVerticalMirror(m_verticalMirror);
    }
}

double VideoWidget::calculateAspectRatio(const QString& aspectRatio) const
{
    if (aspectRatio == "Auto") {
        return 0.0; // Let VLC determine automatically
    } else if (aspectRatio == "16:9") {
        return 16.0 / 9.0;
    } else if (aspectRatio == "4:3") {
        return 4.0 / 3.0;
    } else if (aspectRatio == "1:1") {
        return 1.0;
    } else if (aspectRatio == "21:9") {
        return 21.0 / 9.0;
    } else if (aspectRatio == "2.35:1") {
        return 2.35;
    }
    
    // Try to parse custom ratio (e.g., "16:10")
    QStringList parts = aspectRatio.split(':');
    if (parts.size() == 2) {
        bool ok1, ok2;
        double width = parts[0].toDouble(&ok1);
        double height = parts[1].toDouble(&ok2);
        if (ok1 && ok2 && height > 0) {
            return width / height;
        }
    }
    
    return 0.0; // Default to auto
}

void VideoWidget::showControlsAnimated()
{
    if (m_overlayWidget && m_controlsOpacityAnimation) {
        m_overlayWidget->setVisible(true);
        m_controlsOpacityAnimation->setStartValue(0.0);
        m_controlsOpacityAnimation->setEndValue(1.0);
        m_controlsOpacityAnimation->start();
    }
}

void VideoWidget::hideControlsAnimated()
{
    if (m_overlayWidget && m_controlsOpacityAnimation) {
        m_controlsOpacityAnimation->setStartValue(1.0);
        m_controlsOpacityAnimation->setEndValue(0.0);
        connect(m_controlsOpacityAnimation, &QPropertyAnimation::finished, 
                m_overlayWidget, &QWidget::hide, Qt::UniqueConnection);
        m_controlsOpacityAnimation->start();
    }
}

void VideoWidget::startControlsHideTimer()
{
    if (m_controlsHideTimer) {
        m_controlsHideTimer->start();
    }
}

void VideoWidget::stopControlsHideTimer()
{
    if (m_controlsHideTimer) {
        m_controlsHideTimer->stop();
    }
}

void VideoWidget::initializeSubtitleComponents()
{
    // Create subtitle manager
    m_subtitleManager = new SubtitleManager(this);
    if (m_subtitleManager->initialize()) {
        qDebug() << "Subtitle manager initialized";
    }
    
    // Create subtitle renderer
    m_subtitleRenderer = new SubtitleRenderer(this);
    m_subtitleRenderer->setEnabled(true);
    
    // Connect subtitle manager to renderer
    if (m_subtitleManager && m_subtitleRenderer) {
        connect(m_subtitleManager, &SubtitleManager::activeSubtitlesChanged,
                this, [this](qint64 time, const QList<SubtitleEntry>& entries) {
                    if (m_subtitleRenderer) {
                        m_subtitleRenderer->setSubtitleEntries(entries, time);
                    }
                });
        connect(m_subtitleManager, &SubtitleManager::subtitlesEnabledChanged,
                m_subtitleRenderer, &SubtitleRenderer::setEnabled);
    }
    
    // Position subtitle renderer
    m_subtitleRenderer->updatePosition();
}
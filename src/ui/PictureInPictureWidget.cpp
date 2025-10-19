#include "ui/PictureInPictureWidget.h"
#include "ui/VideoWidget.h"
#include "media/PlaybackController.h"
#include <QApplication>
#include <QScreen>
#include <QLoggingCategory>
#include <QActionGroup>
#include <QCursor>

Q_DECLARE_LOGGING_CATEGORY(pictureInPicture)
Q_LOGGING_CATEGORY(pictureInPicture, "ui.pip")

PictureInPictureWidget::PictureInPictureWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_controlsLayout(new QHBoxLayout())
    , m_videoContainer(new QWidget(this))
    , m_videoWidget(nullptr)
    , m_playPauseButton(new QPushButton(this))
    , m_volumeButton(new QPushButton(this))
    , m_closeButton(new QPushButton(this))
    , m_volumeSlider(new QSlider(Qt::Horizontal, this))
    , m_contextMenu(new QMenu(this))
    , m_playbackController(nullptr)
    , m_windowPosition(BottomRight)
    , m_controlMode(MinimalControls)
    , m_resizable(true)
    , m_opacity(DEFAULT_OPACITY)
    , m_autoHideEnabled(true)
    , m_autoHideTimeout(DEFAULT_AUTO_HIDE_TIMEOUT)
    , m_aspectRatioLocked(true)
    , m_aspectRatio(DEFAULT_ASPECT_RATIO)
    , m_dragging(false)
    , m_resizing(false)
    , m_autoHideTimer(new QTimer(this))
    , m_updateTimer(new QTimer(this))
    , m_opacityEffect(new QGraphicsOpacityEffect(this))
{
    setupUI();
    setupConnections();
    createContextMenu();
    
    // Set window properties
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    // Set opacity effect
    setGraphicsEffect(m_opacityEffect);
    m_opacityEffect->setOpacity(m_opacity);
    
    // Setup timers
    m_autoHideTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    
    qCDebug(pictureInPicture) << "PictureInPictureWidget created";
}

PictureInPictureWidget::~PictureInPictureWidget()
{
    qCDebug(pictureInPicture) << "PictureInPictureWidget destroyed";
}

void PictureInPictureWidget::setVideoWidget(VideoWidget* videoWidget)
{
    if (m_videoWidget) {
        m_videoWidget->setParent(nullptr);
    }
    
    m_videoWidget = videoWidget;
    
    if (m_videoWidget) {
        m_videoWidget->setParent(m_videoContainer);
        m_videoWidget->resize(m_videoContainer->size());
        m_videoWidget->show();
    }
}

void PictureInPictureWidget::setPlaybackController(PlaybackController* controller)
{
    if (m_playbackController) {
        disconnect(m_playbackController, nullptr, this, nullptr);
    }
    
    m_playbackController = controller;
    
    if (m_playbackController) {
        // Connect to playback controller signals
        // Implementation would connect to actual signals
    }
}

PictureInPictureWidget::WindowPosition PictureInPictureWidget::getWindowPosition() const
{
    return m_windowPosition;
}

void PictureInPictureWidget::setWindowPosition(WindowPosition position)
{
    if (m_windowPosition != position) {
        m_windowPosition = position;
        updateWindowPosition();
        emit windowPositionChanged(m_windowPosition);
        
        qCDebug(pictureInPicture) << "Window position changed to:" << static_cast<int>(position);
    }
}

PictureInPictureWidget::ControlMode PictureInPictureWidget::getControlMode() const
{
    return m_controlMode;
}

void PictureInPictureWidget::setControlMode(ControlMode mode)
{
    if (m_controlMode != mode) {
        m_controlMode = mode;
        updateControlsVisibility();
        emit controlModeChanged(m_controlMode);
        
        qCDebug(pictureInPicture) << "Control mode changed to:" << static_cast<int>(mode);
    }
}
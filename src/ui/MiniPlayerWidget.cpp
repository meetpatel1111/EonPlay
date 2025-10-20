#include "ui/MiniPlayerWidget.h"
#include "media/PlaybackController.h"
#include "ui/VideoWidget.h"
#include <QApplication>
#include <QScreen>
#include <QLoggingCategory>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QGraphicsOpacityEffect>

Q_DECLARE_LOGGING_CATEGORY(miniPlayer)
Q_LOGGING_CATEGORY(miniPlayer, "ui.miniplayer")

MiniPlayerWidget::MiniPlayerWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_controlsLayout(new QHBoxLayout())
    , m_seekLayout(new QHBoxLayout())
    , m_volumeLayout(new QHBoxLayout())
    , m_videoContainer(new QWidget(this))
    , m_videoWidget(nullptr)
    , m_albumArtLabel(new QLabel(this))
    , m_playPauseButton(new QPushButton(this))
    , m_stopButton(new QPushButton(this))
    , m_previousButton(new QPushButton(this))
    , m_nextButton(new QPushButton(this))
    , m_volumeButton(new QPushButton(this))
    , m_closeButton(new QPushButton(this))
    , m_positionSlider(new QSlider(Qt::Horizontal, this))
    , m_volumeSlider(new QSlider(Qt::Horizontal, this))
    , m_titleLabel(new QLabel(this))
    , m_timeLabel(new QLabel(this))
    , m_contextMenu(new QMenu(this))
    , m_playbackController(nullptr)
    , m_playerMode(VideoMode)
    , m_alwaysOnTop(true)
    , m_controlsVisible(true)
    , m_resizable(true)
    , m_opacity(DEFAULT_OPACITY)
    , m_autoHideTimeout(DEFAULT_AUTO_HIDE_TIMEOUT)
    , m_dragging(false)
    , m_autoHideTimer(new QTimer(this))
    , m_updateTimer(new QTimer(this))
{
    setupUI();
    setupConnections();
    createContextMenu();
    
    // Set initial window properties
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    // Set initial opacity
    setOpacity(m_opacity);
    
    // Setup timers
    m_autoHideTimer->setSingleShot(true);
    m_updateTimer->setInterval(100); // Update every 100ms
    
    qCDebug(miniPlayer) << "MiniPlayerWidget created";
}

MiniPlayerWidget::~MiniPlayerWidget()
{
    qCDebug(miniPlayer) << "MiniPlayerWidget destroyed";
}

void MiniPlayerWidget::setPlaybackController(PlaybackController* controller)
{
    if (m_playbackController) {
        disconnect(m_playbackController, nullptr, this, nullptr);
    }
    
    m_playbackController = controller;
    
    if (m_playbackController) {
        // Connect to playback controller signals
        // connect(m_playbackController, &PlaybackController::stateChanged,
        //         this, &MiniPlayerWidget::updatePlayPauseButton);
        // connect(m_playbackController, &PlaybackController::positionChanged,
        //         this, &MiniPlayerWidget::updatePositionSlider);
        // connect(m_playbackController, &PlaybackController::volumeChanged,
        //         this, &MiniPlayerWidget::updateVolumeSlider);
    }
}

void MiniPlayerWidget::setVideoWidget(VideoWidget* videoWidget)
{
    if (m_videoWidget) {
        m_videoWidget->setParent(nullptr);
    }
    
    m_videoWidget = videoWidget;
    
    if (m_videoWidget && m_playerMode == VideoMode) {
        m_videoWidget->setParent(m_videoContainer);
        m_videoWidget->resize(m_videoContainer->size());
        m_videoWidget->show();
    }
}

MiniPlayerWidget::PlayerMode MiniPlayerWidget::getPlayerMode() const
{
    return m_playerMode;
}

void MiniPlayerWidget::setPlayerMode(PlayerMode mode)
{
    if (m_playerMode != mode) {
        m_playerMode = mode;
        updateControlsLayout();
        emit playerModeChanged(m_playerMode);
        
        qCDebug(miniPlayer) << "Player mode changed to:" << static_cast<int>(mode);
    }
}

bool MiniPlayerWidget::isAlwaysOnTop() const
{
    return m_alwaysOnTop;
}

void MiniPlayerWidget::setAlwaysOnTop(bool alwaysOnTop)
{
    if (m_alwaysOnTop != alwaysOnTop) {
        m_alwaysOnTop = alwaysOnTop;
        updateWindowFlags();
        emit alwaysOnTopChanged(m_alwaysOnTop);
        
        qCDebug(miniPlayer) << "Always on top:" << alwaysOnTop;
    }
}

bool MiniPlayerWidget::areControlsVisible() const
{
    return m_controlsVisible;
}

void MiniPlayerWidget::setControlsVisible(bool visible)
{
    if (m_controlsVisible != visible) {
        m_controlsVisible = visible;
        
        // Show/hide control widgets
        m_playPauseButton->setVisible(visible);
        m_stopButton->setVisible(visible);
        m_previousButton->setVisible(visible);
        m_nextButton->setVisible(visible);
        m_volumeButton->setVisible(visible);
        m_positionSlider->setVisible(visible);
        m_volumeSlider->setVisible(visible);
        m_titleLabel->setVisible(visible);
        m_timeLabel->setVisible(visible);
        
        qCDebug(miniPlayer) << "Controls visible:" << visible;
    }
}

int MiniPlayerWidget::getAutoHideTimeout() const
{
    return m_autoHideTimeout;
}

void MiniPlayerWidget::setAutoHideTimeout(int timeoutMs)
{
    m_autoHideTimeout = qMax(1000, timeoutMs); // Minimum 1 second
    qCDebug(miniPlayer) << "Auto-hide timeout set to:" << m_autoHideTimeout << "ms";
}

bool MiniPlayerWidget::isResizable() const
{
    return m_resizable;
}

void MiniPlayerWidget::setResizable(bool resizable)
{
    if (m_resizable != resizable) {
        m_resizable = resizable;
        
        if (resizable) {
            setMinimumSize(200, 150);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        } else {
            setFixedSize(size());
        }
        
        qCDebug(miniPlayer) << "Resizable:" << resizable;
    }
}

float MiniPlayerWidget::getOpacity() const
{
    return m_opacity;
}

void MiniPlayerWidget::setOpacity(float opacity)
{
    opacity = qBound(0.1f, opacity, 1.0f);
    
    if (m_opacity != opacity) {
        m_opacity = opacity;
        setWindowOpacity(opacity);
        
        qCDebug(miniPlayer) << "Opacity set to:" << opacity;
    }
}

void MiniPlayerWidget::showMiniPlayer()
{
    show();
    raise();
    activateWindow();
    startAutoHideTimer();
    
    qCDebug(miniPlayer) << "Mini player shown";
}

void MiniPlayerWidget::hideMiniPlayer()
{
    hide();
    stopAutoHideTimer();
    
    qCDebug(miniPlayer) << "Mini player hidden";
}

void MiniPlayerWidget::toggleMiniPlayer()
{
    if (isVisible()) {
        hideMiniPlayer();
    } else {
        showMiniPlayer();
    }
}

void MiniPlayerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    
    QWidget::mousePressEvent(event);
}

void MiniPlayerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStartPosition);
        event->accept();
    }
    
    QWidget::mouseMoveEvent(event);
}

void MiniPlayerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
    
    QWidget::mouseReleaseEvent(event);
}

void MiniPlayerWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit restoreToMainWindow();
        event->accept();
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void MiniPlayerWidget::enterEvent(QEnterEvent* event)
{
    stopAutoHideTimer();
    setControlsVisible(true);
    
    QWidget::enterEvent(event);
}

void MiniPlayerWidget::leaveEvent(QEvent* event)
{
    startAutoHideTimer();
    
    QWidget::leaveEvent(event);
}

void MiniPlayerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    // Resize video widget if present
    if (m_videoWidget && m_playerMode == VideoMode) {
        m_videoWidget->resize(m_videoContainer->size());
    }
}

void MiniPlayerWidget::closeEvent(QCloseEvent* event)
{
    emit miniPlayerClosed();
    event->accept();
}

void MiniPlayerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    m_contextMenu->exec(event->globalPos());
}

void MiniPlayerWidget::onPlayPauseClicked()
{
    if (m_playbackController) {
        // m_playbackController->togglePlayPause();
    }
}

void MiniPlayerWidget::onStopClicked()
{
    if (m_playbackController) {
        // m_playbackController->stop();
    }
}

void MiniPlayerWidget::onPreviousClicked()
{
    if (m_playbackController) {
        // m_playbackController->previous();
    }
}

void MiniPlayerWidget::onNextClicked()
{
    if (m_playbackController) {
        // m_playbackController->next();
    }
}

void MiniPlayerWidget::onVolumeChanged(int /*volume*/)
{
    if (m_playbackController) {
        // m_playbackController->setVolume(volume);
    }
}

void MiniPlayerWidget::onPositionChanged(int position)
{
    Q_UNUSED(position)
    updatePositionSlider();
}

void MiniPlayerWidget::onSeekRequested(int /*position*/)
{
    if (m_playbackController) {
        // m_playbackController->setPosition(position);
    }
}

void MiniPlayerWidget::onAutoHideTimeout()
{
    if (!underMouse()) {
        setControlsVisible(false);
    }
}

void MiniPlayerWidget::updatePlaybackInfo()
{
    if (!m_playbackController) {
        return;
    }
    
    // Update title and time labels
    // m_titleLabel->setText(m_playbackController->getCurrentTitle());
    // m_timeLabel->setText(formatTime(m_playbackController->getPosition()) + 
    //                     " / " + formatTime(m_playbackController->getDuration()));
}

void MiniPlayerWidget::setupUI()
{
    setStyleSheet(R"(
        MiniPlayerWidget {
            background-color: rgba(30, 30, 30, 200);
            border: 1px solid rgba(100, 100, 100, 150);
            border-radius: 8px;
        }
        QPushButton {
            background-color: rgba(50, 50, 50, 150);
            border: 1px solid rgba(100, 100, 100, 100);
            border-radius: 4px;
            color: white;
            padding: 4px;
            min-width: 24px;
            min-height: 24px;
        }
        QPushButton:hover {
            background-color: rgba(70, 70, 70, 200);
        }
        QPushButton:pressed {
            background-color: rgba(40, 40, 40, 200);
        }
        QSlider::groove:horizontal {
            border: 1px solid rgba(100, 100, 100, 100);
            height: 4px;
            background: rgba(50, 50, 50, 150);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: rgba(0, 150, 255, 200);
            border: 1px solid rgba(0, 100, 200, 150);
            width: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QLabel {
            color: white;
            font-size: 11px;
        }
    )");
    
    // Setup video container
    m_videoContainer->setStyleSheet("background-color: black;");
    
    // Setup album art label
    m_albumArtLabel->setAlignment(Qt::AlignCenter);
    m_albumArtLabel->setStyleSheet("background-color: rgba(50, 50, 50, 150); border-radius: 4px;");
    m_albumArtLabel->setText("♪");
    m_albumArtLabel->setFont(QFont("Arial", 24));
    
    // Setup buttons
    m_playPauseButton->setText("▶");
    m_stopButton->setText("⏹");
    m_previousButton->setText("⏮");
    m_nextButton->setText("⏭");
    m_volumeButton->setText("🔊");
    m_closeButton->setText("✕");
    
    // Setup sliders
    m_positionSlider->setRange(0, 1000);
    m_positionSlider->setValue(0);
    
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(50);
    m_volumeSlider->setMaximumWidth(80);
    
    // Setup labels
    m_titleLabel->setText("No media");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("font-weight: bold;");
    
    m_timeLabel->setText("00:00 / 00:00");
    m_timeLabel->setAlignment(Qt::AlignCenter);
    
    updateControlsLayout();
}

void MiniPlayerWidget::setupConnections()
{
    // Button connections
    connect(m_playPauseButton, &QPushButton::clicked, this, &MiniPlayerWidget::onPlayPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MiniPlayerWidget::onStopClicked);
    connect(m_previousButton, &QPushButton::clicked, this, &MiniPlayerWidget::onPreviousClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &MiniPlayerWidget::onNextClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &MiniPlayerWidget::close);
    
    // Slider connections
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MiniPlayerWidget::onVolumeChanged);
    connect(m_positionSlider, &QSlider::sliderPressed, this, [this]() {
        // Pause position updates while dragging
    });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this]() {
        onSeekRequested(m_positionSlider->value());
    });
    
    // Timer connections
    connect(m_autoHideTimer, &QTimer::timeout, this, &MiniPlayerWidget::onAutoHideTimeout);
    connect(m_updateTimer, &QTimer::timeout, this, &MiniPlayerWidget::updatePlaybackInfo);
    
    // Start update timer
    m_updateTimer->start();
}

void MiniPlayerWidget::createContextMenu()
{
    // Player mode actions
    QActionGroup* modeGroup = new QActionGroup(this);
    m_videoModeAction = m_contextMenu->addAction("Video Mode");
    m_videoModeAction->setCheckable(true);
    m_videoModeAction->setActionGroup(modeGroup);
    
    m_audioModeAction = m_contextMenu->addAction("Audio Mode");
    m_audioModeAction->setCheckable(true);
    m_audioModeAction->setActionGroup(modeGroup);
    
    m_compactModeAction = m_contextMenu->addAction("Compact Mode");
    m_compactModeAction->setCheckable(true);
    m_compactModeAction->setActionGroup(modeGroup);
    
    m_contextMenu->addSeparator();
    
    // Window options
    m_alwaysOnTopAction = m_contextMenu->addAction("Always on Top");
    m_alwaysOnTopAction->setCheckable(true);
    m_alwaysOnTopAction->setChecked(m_alwaysOnTop);
    
    m_resizableAction = m_contextMenu->addAction("Resizable");
    m_resizableAction->setCheckable(true);
    m_resizableAction->setChecked(m_resizable);
    
    m_contextMenu->addSeparator();
    
    // Actions
    m_restoreAction = m_contextMenu->addAction("Restore to Main Window");
    
    // Connect actions
    connect(m_videoModeAction, &QAction::triggered, this, [this]() {
        setPlayerMode(VideoMode);
    });
    connect(m_audioModeAction, &QAction::triggered, this, [this]() {
        setPlayerMode(AudioMode);
    });
    connect(m_compactModeAction, &QAction::triggered, this, [this]() {
        setPlayerMode(CompactMode);
    });
    connect(m_alwaysOnTopAction, &QAction::toggled, this, &MiniPlayerWidget::setAlwaysOnTop);
    connect(m_resizableAction, &QAction::toggled, this, &MiniPlayerWidget::setResizable);
    connect(m_restoreAction, &QAction::triggered, this, &MiniPlayerWidget::restoreToMainWindow);
    
    // Set initial mode
    m_videoModeAction->setChecked(true);
}

void MiniPlayerWidget::updateControlsLayout()
{
    // Clear existing layout
    QLayoutItem* item;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) {
        delete item;
    }
    
    switch (m_playerMode) {
        case VideoMode:
            m_mainLayout->addWidget(m_videoContainer, 1);
            m_mainLayout->addWidget(m_titleLabel);
            m_mainLayout->addLayout(m_seekLayout);
            m_mainLayout->addLayout(m_controlsLayout);
            
            m_videoContainer->setVisible(true);
            m_albumArtLabel->setVisible(false);
            
            if (m_videoWidget) {
                m_videoWidget->setParent(m_videoContainer);
                m_videoWidget->resize(m_videoContainer->size());
                m_videoWidget->show();
            }
            
            setFixedSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
            break;
            
        case AudioMode:
            m_mainLayout->addWidget(m_albumArtLabel, 1);
            m_mainLayout->addWidget(m_titleLabel);
            m_mainLayout->addLayout(m_seekLayout);
            m_mainLayout->addLayout(m_controlsLayout);
            
            m_videoContainer->setVisible(false);
            m_albumArtLabel->setVisible(true);
            
            setFixedSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
            break;
            
        case CompactMode:
            m_mainLayout->addLayout(m_controlsLayout);
            
            m_videoContainer->setVisible(false);
            m_albumArtLabel->setVisible(false);
            m_titleLabel->setVisible(false);
            m_positionSlider->setVisible(false);
            
            setFixedSize(DEFAULT_WIDTH, COMPACT_HEIGHT);
            break;
    }
    
    // Setup control layouts
    m_controlsLayout->addWidget(m_previousButton);
    m_controlsLayout->addWidget(m_playPauseButton);
    m_controlsLayout->addWidget(m_stopButton);
    m_controlsLayout->addWidget(m_nextButton);
    m_controlsLayout->addStretch();
    m_controlsLayout->addWidget(m_volumeButton);
    m_controlsLayout->addWidget(m_volumeSlider);
    m_controlsLayout->addWidget(m_closeButton);
    
    if (m_playerMode != CompactMode) {
        m_seekLayout->addWidget(m_positionSlider);
        m_seekLayout->addWidget(m_timeLabel);
    }
}

void MiniPlayerWidget::updateWindowFlags()
{
    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint;
    
    if (m_alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    
    setWindowFlags(flags);
    show(); // Need to show again after changing flags
}

void MiniPlayerWidget::startAutoHideTimer()
{
    if (m_autoHideTimeout > 0) {
        m_autoHideTimer->start(m_autoHideTimeout);
    }
}

void MiniPlayerWidget::stopAutoHideTimer()
{
    m_autoHideTimer->stop();
}

void MiniPlayerWidget::updatePlayPauseButton()
{
    if (!m_playbackController) {
        return;
    }
    
    // Update button text based on playback state
    // if (m_playbackController->isPlaying()) {
    //     m_playPauseButton->setText("⏸");
    // } else {
    //     m_playPauseButton->setText("▶");
    // }
}

void MiniPlayerWidget::updatePositionSlider()
{
    if (!m_playbackController || m_positionSlider->isSliderDown()) {
        return;
    }
    
    // Update slider position
    // qint64 position = m_playbackController->getPosition();
    // qint64 duration = m_playbackController->getDuration();
    
    // if (duration > 0) {
    //     int sliderValue = static_cast<int>((position * 1000) / duration);
    //     m_positionSlider->setValue(sliderValue);
    // }
}

void MiniPlayerWidget::updateVolumeSlider()
{
    if (!m_playbackController) {
        return;
    }
    
    // Update volume slider
    // int volume = m_playbackController->getVolume();
    // m_volumeSlider->setValue(volume);
}

QString MiniPlayerWidget::formatTime(qint64 milliseconds) const
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
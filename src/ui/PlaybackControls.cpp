#include "ui/PlaybackControls.h"
#include "ComponentManager.h"
#include <QApplication>
#include <QStyle>
#include <QMouseEvent>
#include <QToolTip>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <cmath>

// Static member definitions
const QList<double> PlaybackControls::PlaybackSpeeds = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0};
const QStringList PlaybackControls::SpeedLabels = {"0.25x", "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "1.75x", "2.0x", "2.5x", "3.0x"};

PlaybackControls::PlaybackControls(QWidget* parent)
    : QWidget(parent)
    , m_componentManager(nullptr)
    , m_playbackController(nullptr)
    , m_mainLayout(nullptr)
    , m_compactLayout(nullptr)
    , m_skipPreviousButton(nullptr)
    , m_playPauseButton(nullptr)
    , m_stopButton(nullptr)
    , m_skipNextButton(nullptr)
    , m_seekSlider(nullptr)
    , m_seekPreviewLabel(nullptr)
    , m_volumeSlider(nullptr)
    , m_muteButton(nullptr)
    , m_volumeLabel(nullptr)
    , m_currentTimeLabel(nullptr)
    , m_durationLabel(nullptr)
    , m_timeSeparatorLabel(nullptr)
    , m_speedComboBox(nullptr)
    , m_speedLabel(nullptr)
    , m_currentPosition(0)
    , m_duration(0)
    , m_volume(75)
    , m_previousVolume(75)
    , m_playbackSpeed(1.0)
    , m_isPlaying(false)
    , m_isMuted(false)
    , m_isSeekSliderPressed(false)
    , m_compactMode(false)
    , m_controlsEnabled(true)
    , m_seekPreviewVisible(false)
{
    setFixedHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    initializeUI();
    applyFuturisticStyling();
    
    // Initialize controls state
    resetControls();
}

PlaybackControls::~PlaybackControls() = default;

bool PlaybackControls::initialize(ComponentManager* componentManager)
{
    if (!componentManager) {
        return false;
    }
    
    m_componentManager = componentManager;
    
    // TODO: Get PlaybackController from component manager when available
    // m_playbackController = componentManager->getComponent<PlaybackController>();
    
    return true;
}

void PlaybackControls::setPosition(qint64 position)
{
    if (m_currentPosition != position) {
        m_currentPosition = position;
        
        if (!m_isSeekSliderPressed && m_duration > 0) {
            int sliderValue = static_cast<int>((position * 1000) / m_duration);
            m_seekSlider->blockSignals(true);
            m_seekSlider->setValue(sliderValue);
            m_seekSlider->blockSignals(false);
        }
        
        updateTimeDisplay();
    }
}

void PlaybackControls::setDuration(qint64 duration)
{
    if (m_duration != duration) {
        m_duration = duration;
        updateTimeDisplay();
    }
}

void PlaybackControls::setVolume(int volume)
{
    volume = qBound(0, volume, 100);
    if (m_volume != volume) {
        m_volume = volume;
        
        m_volumeSlider->blockSignals(true);
        m_volumeSlider->setValue(volume);
        m_volumeSlider->blockSignals(false);
        
        m_volumeLabel->setText(QString("%1%").arg(volume));
        
        // Update mute state
        m_isMuted = (volume == 0);
        updateMuteButton();
    }
}

void PlaybackControls::setPlaybackSpeed(double speed)
{
    if (qAbs(m_playbackSpeed - speed) > 0.01) {
        m_playbackSpeed = speed;
        
        // Find closest speed in combo box
        int index = 3; // Default to 1.0x
        double minDiff = std::abs(speed - 1.0);
        
        for (int i = 0; i < PlaybackSpeeds.size(); ++i) {
            double diff = std::abs(speed - PlaybackSpeeds[i]);
            if (diff < minDiff) {
                minDiff = diff;
                index = i;
            }
        }
        
        m_speedComboBox->blockSignals(true);
        m_speedComboBox->setCurrentIndex(index);
        m_speedComboBox->blockSignals(false);
    }
}

void PlaybackControls::setPlaybackState(bool isPlaying)
{
    if (m_isPlaying != isPlaying) {
        m_isPlaying = isPlaying;
        updatePlayPauseButton();
    }
}

void PlaybackControls::setControlsEnabled(bool enabled)
{
    if (m_controlsEnabled != enabled) {
        m_controlsEnabled = enabled;
        
        // Enable/disable all controls
        m_skipPreviousButton->setEnabled(enabled);
        m_playPauseButton->setEnabled(enabled);
        m_stopButton->setEnabled(enabled);
        m_skipNextButton->setEnabled(enabled);
        m_seekSlider->setEnabled(enabled);
        m_volumeSlider->setEnabled(enabled);
        m_muteButton->setEnabled(enabled);
        m_speedComboBox->setEnabled(enabled);
    }
}

void PlaybackControls::setCompactMode(bool compact)
{
    if (m_compactMode != compact) {
        m_compactMode = compact;
        
        if (compact) {
            setFixedHeight(60);
            // Hide some controls in compact mode
            m_speedLabel->setVisible(false);
            m_speedComboBox->setVisible(false);
            m_volumeLabel->setVisible(false);
        } else {
            setFixedHeight(80);
            // Show all controls in normal mode
            m_speedLabel->setVisible(true);
            m_speedComboBox->setVisible(true);
            m_volumeLabel->setVisible(true);
        }
    }
}

void PlaybackControls::updatePosition(qint64 position)
{
    setPosition(position);
}

void PlaybackControls::updateDuration(qint64 duration)
{
    setDuration(duration);
}

void PlaybackControls::resetControls()
{
    setPosition(0);
    setDuration(0);
    setVolume(75);
    setPlaybackSpeed(1.0);
    setPlaybackState(false);
    setControlsEnabled(false);
}

void PlaybackControls::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_seekSlider->geometry().contains(event->pos())) {
        showSeekPreview(event->pos());
    }
    QWidget::mousePressEvent(event);
}

void PlaybackControls::mouseMoveEvent(QMouseEvent* event)
{
    if (m_seekSlider->geometry().contains(event->pos())) {
        showSeekPreview(event->pos());
        m_lastMousePosition = event->pos();
    } else {
        hideSeekPreview();
    }
    QWidget::mouseMoveEvent(event);
}

void PlaybackControls::mouseReleaseEvent(QMouseEvent* event)
{
    hideSeekPreview();
    QWidget::mouseReleaseEvent(event);
}

void PlaybackControls::onPlayPauseClicked()
{
    if (m_isPlaying) {
        emit pauseRequested();
    } else {
        emit playRequested();
    }
}

void PlaybackControls::onStopClicked()
{
    emit stopRequested();
}

void PlaybackControls::onSkipPreviousClicked()
{
    emit skipPreviousRequested();
}

void PlaybackControls::onSkipNextClicked()
{
    emit skipNextRequested();
}

void PlaybackControls::onSeekSliderChanged(int value)
{
    if (m_duration > 0) {
        qint64 position = (static_cast<qint64>(value) * m_duration) / 1000;
        m_currentPosition = position;
        updateTimeDisplay();
        
        if (!m_isSeekSliderPressed) {
            emit seekRequested(position);
        }
    }
}

void PlaybackControls::onSeekSliderPressed()
{
    m_isSeekSliderPressed = true;
}

void PlaybackControls::onSeekSliderReleased()
{
    m_isSeekSliderPressed = false;
    
    if (m_duration > 0) {
        qint64 position = (static_cast<qint64>(m_seekSlider->value()) * m_duration) / 1000;
        emit seekRequested(position);
    }
}

void PlaybackControls::onVolumeSliderChanged(int value)
{
    setVolume(value);
    emit volumeChanged(value);
}

void PlaybackControls::onMuteClicked()
{
    if (m_isMuted) {
        // Unmute - restore previous volume
        setVolume(m_previousVolume);
        emit volumeChanged(m_previousVolume);
    } else {
        // Mute - save current volume and set to 0
        m_previousVolume = m_volume;
        setVolume(0);
        emit volumeChanged(0);
    }
    
    emit muteToggleRequested();
}

void PlaybackControls::onPlaybackSpeedChanged(int index)
{
    if (index >= 0 && index < PlaybackSpeeds.size()) {
        double speed = PlaybackSpeeds[index];
        setPlaybackSpeed(speed);
        emit playbackSpeedChanged(speed);
    }
}

void PlaybackControls::updateTimeDisplay()
{
    m_currentTimeLabel->setText(formatTime(m_currentPosition));
    m_durationLabel->setText(formatTime(m_duration));
}

void PlaybackControls::showSeekPreview(const QPoint& position)
{
    if (m_duration > 0 && m_seekSlider->geometry().contains(position)) {
        QRect sliderRect = m_seekSlider->geometry();
        int relativeX = position.x() - sliderRect.x();
        qint64 previewPosition = calculateSeekPosition(relativeX);
        
        QString previewText = formatTime(previewPosition);
        QPoint tooltipPos = mapToGlobal(QPoint(position.x(), sliderRect.y() - 30));
        
        QToolTip::showText(tooltipPos, previewText, this);
        m_seekPreviewVisible = true;
    }
}

void PlaybackControls::hideSeekPreview()
{
    if (m_seekPreviewVisible) {
        QToolTip::hideText();
        m_seekPreviewVisible = false;
    }
}

void PlaybackControls::initializeUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(8);
    
    createMainControls();
    createSeekControls();
    createTimeDisplay();
    createVolumeControls();
    createSpeedControls();
}

void PlaybackControls::createMainControls()
{
    // Skip Previous Button
    m_skipPreviousButton = new QPushButton();
    m_skipPreviousButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    m_skipPreviousButton->setToolTip(tr("Skip Previous"));
    m_skipPreviousButton->setFixedSize(40, 40);
    connect(m_skipPreviousButton, &QPushButton::clicked, this, &PlaybackControls::onSkipPreviousClicked);
    
    // Play/Pause Button
    m_playPauseButton = new QPushButton();
    m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_playPauseButton->setToolTip(tr("Play"));
    m_playPauseButton->setFixedSize(50, 50);
    connect(m_playPauseButton, &QPushButton::clicked, this, &PlaybackControls::onPlayPauseClicked);
    
    // Stop Button
    m_stopButton = new QPushButton();
    m_stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopButton->setToolTip(tr("Stop"));
    m_stopButton->setFixedSize(40, 40);
    connect(m_stopButton, &QPushButton::clicked, this, &PlaybackControls::onStopClicked);
    
    // Skip Next Button
    m_skipNextButton = new QPushButton();
    m_skipNextButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    m_skipNextButton->setToolTip(tr("Skip Next"));
    m_skipNextButton->setFixedSize(40, 40);
    connect(m_skipNextButton, &QPushButton::clicked, this, &PlaybackControls::onSkipNextClicked);
    
    // Add buttons to layout
    m_mainLayout->addWidget(m_skipPreviousButton);
    m_mainLayout->addWidget(m_playPauseButton);
    m_mainLayout->addWidget(m_stopButton);
    m_mainLayout->addWidget(m_skipNextButton);
    
    // Add spacer
    m_mainLayout->addSpacerItem(new QSpacerItem(10, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
}

void PlaybackControls::createSeekControls()
{
    // Current time label
    m_currentTimeLabel = new QLabel("00:00");
    m_currentTimeLabel->setMinimumWidth(50);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);
    
    // Seek slider
    m_seekSlider = new QSlider(Qt::Horizontal);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setValue(0);
    m_seekSlider->setToolTip(tr("Seek"));
    m_seekSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_seekSlider, &QSlider::valueChanged, this, &PlaybackControls::onSeekSliderChanged);
    connect(m_seekSlider, &QSlider::sliderPressed, this, &PlaybackControls::onSeekSliderPressed);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &PlaybackControls::onSeekSliderReleased);
    
    // Duration label
    m_durationLabel = new QLabel("00:00");
    m_durationLabel->setMinimumWidth(50);
    m_durationLabel->setAlignment(Qt::AlignCenter);
    
    // Time separator
    m_timeSeparatorLabel = new QLabel("/");
    m_timeSeparatorLabel->setAlignment(Qt::AlignCenter);
    
    // Add to layout
    m_mainLayout->addWidget(m_currentTimeLabel);
    m_mainLayout->addWidget(m_seekSlider);
    m_mainLayout->addWidget(m_timeSeparatorLabel);
    m_mainLayout->addWidget(m_durationLabel);
    
    // Add spacer
    m_mainLayout->addSpacerItem(new QSpacerItem(10, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
}

void PlaybackControls::createTimeDisplay()
{
    // Time display is already created in createSeekControls()
    updateTimeDisplay();
}

void PlaybackControls::createVolumeControls()
{
    // Mute button
    m_muteButton = new QPushButton();
    m_muteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
    m_muteButton->setToolTip(tr("Mute"));
    m_muteButton->setFixedSize(30, 30);
    connect(m_muteButton, &QPushButton::clicked, this, &PlaybackControls::onMuteClicked);
    
    // Volume slider
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(75);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setToolTip(tr("Volume"));
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PlaybackControls::onVolumeSliderChanged);
    
    // Volume label
    m_volumeLabel = new QLabel("75%");
    m_volumeLabel->setMinimumWidth(35);
    m_volumeLabel->setAlignment(Qt::AlignCenter);
    
    // Add to layout
    m_mainLayout->addWidget(m_muteButton);
    m_mainLayout->addWidget(m_volumeSlider);
    m_mainLayout->addWidget(m_volumeLabel);
    
    // Add spacer
    m_mainLayout->addSpacerItem(new QSpacerItem(10, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
}

void PlaybackControls::createSpeedControls()
{
    // Speed label
    m_speedLabel = new QLabel(tr("Speed:"));
    m_speedLabel->setAlignment(Qt::AlignCenter);
    
    // Speed combo box
    m_speedComboBox = new QComboBox();
    m_speedComboBox->addItems(SpeedLabels);
    m_speedComboBox->setCurrentIndex(3); // 1.0x
    m_speedComboBox->setFixedWidth(70);
    m_speedComboBox->setToolTip(tr("Playback Speed"));
    connect(m_speedComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlaybackControls::onPlaybackSpeedChanged);
    
    // Add to layout
    m_mainLayout->addWidget(m_speedLabel);
    m_mainLayout->addWidget(m_speedComboBox);
}

void PlaybackControls::applyFuturisticStyling()
{
    // Apply futuristic styling to the controls
    setStyleSheet(
        "PlaybackControls {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #2a2a2a, stop:1 #1a1a1a);"
        "    border-top: 2px solid #00d4ff;"
        "    border-radius: 8px;"
        "}"
        
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #3a3a3a, stop:1 #2a2a2a);"
        "    border: 2px solid #00d4ff;"
        "    border-radius: 20px;"
        "    color: #e0e0e0;"
        "    font-weight: bold;"
        "}"
        
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #00d4ff, stop:1 #0099cc);"
        "    color: #000000;"
        "    border-color: #ffffff;"
        "}"
        
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #0099cc, stop:1 #007799);"
        "    color: #000000;"
        "}"
        
        "QPushButton:disabled {"
        "    background-color: #333333;"
        "    border-color: #555555;"
        "    color: #666666;"
        "}"
        
        "QSlider::groove:horizontal {"
        "    border: 1px solid #333333;"
        "    height: 6px;"
        "    background: #1a1a1a;"
        "    border-radius: 3px;"
        "}"
        
        "QSlider::handle:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #00d4ff, stop:1 #0099cc);"
        "    border: 2px solid #ffffff;"
        "    width: 16px;"
        "    margin: -6px 0;"
        "    border-radius: 10px;"
        "}"
        
        "QSlider::handle:horizontal:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #ffffff, stop:1 #00d4ff);"
        "}"
        
        "QSlider::sub-page:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #00d4ff, stop:1 #0099cc);"
        "    border-radius: 3px;"
        "}"
        
        "QLabel {"
        "    color: #e0e0e0;"
        "    background: transparent;"
        "    border: none;"
        "    font-weight: bold;"
        "}"
        
        "QComboBox {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                                stop:0 #2a2a2a, stop:1 #1a1a1a);"
        "    border: 2px solid #00d4ff;"
        "    border-radius: 4px;"
        "    padding: 4px 8px;"
        "    color: #e0e0e0;"
        "    font-weight: bold;"
        "}"
        
        "QComboBox:hover {"
        "    border-color: #ffffff;"
        "}"
        
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"
        
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 5px solid transparent;"
        "    border-right: 5px solid transparent;"
        "    border-top: 5px solid #00d4ff;"
        "}"
        
        "QComboBox QAbstractItemView {"
        "    background-color: #1a1a1a;"
        "    color: #e0e0e0;"
        "    border: 2px solid #00d4ff;"
        "    border-radius: 4px;"
        "    selection-background-color: #00d4ff;"
        "    selection-color: #000000;"
        "}"
    );
}

QString PlaybackControls::formatTime(qint64 milliseconds) const
{
    if (milliseconds < 0) {
        return "00:00";
    }
    
    int seconds = static_cast<int>(milliseconds / 1000);
    int minutes = seconds / 60;
    int hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

void PlaybackControls::updatePlayPauseButton()
{
    if (m_isPlaying) {
        m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        m_playPauseButton->setToolTip(tr("Pause"));
    } else {
        m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_playPauseButton->setToolTip(tr("Play"));
    }
}

void PlaybackControls::updateMuteButton()
{
    if (m_isMuted || m_volume == 0) {
        m_muteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
        m_muteButton->setToolTip(tr("Unmute"));
    } else {
        m_muteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
        m_muteButton->setToolTip(tr("Mute"));
    }
}

qint64 PlaybackControls::calculateSeekPosition(int mouseX) const
{
    if (m_duration <= 0) {
        return 0;
    }
    
    QRect sliderRect = m_seekSlider->geometry();
    int sliderWidth = sliderRect.width();
    
    if (sliderWidth <= 0) {
        return 0;
    }
    
    double ratio = static_cast<double>(mouseX) / sliderWidth;
    ratio = qBound(0.0, ratio, 1.0);
    
    return static_cast<qint64>(ratio * m_duration);
}


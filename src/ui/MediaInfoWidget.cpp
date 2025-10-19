#include "ui/MediaInfoWidget.h"
#include "media/FileUrlSupport.h"
#include <QLoggingCategory>
#include <QPushButton>
#include <QSplitter>
#include <QApplication>
#include <QStyle>
#include <QFont>
#include <QFontMetrics>
#include <QUrl>

Q_LOGGING_CATEGORY(mediaInfoWidget, "eonplay.mediainfowidget")

MediaInfoWidget::MediaInfoWidget(QWidget* parent)
    : QWidget(parent)
    , m_showTechnicalDetails(false)
    , m_autoUpdate(true)
    , m_compactMode(false)
    , m_updateTimer(new QTimer(this))
{
    initializeUI();
    
    // Set up update timer
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(1000); // Update every second
    connect(m_updateTimer, &QTimer::timeout, this, &MediaInfoWidget::refreshMediaInfo);
    
    qCDebug(mediaInfoWidget) << "MediaInfoWidget created";
}

MediaInfoWidget::~MediaInfoWidget()
{
    qCDebug(mediaInfoWidget) << "MediaInfoWidget destroyed";
}

void MediaInfoWidget::setFileUrlSupport(std::shared_ptr<FileUrlSupport> fileUrlSupport)
{
    // Disconnect from previous instance
    if (m_fileUrlSupport) {
        disconnect(m_fileUrlSupport.get(), nullptr, this, nullptr);
    }
    
    m_fileUrlSupport = fileUrlSupport;
    
    // Connect to new instance
    if (m_fileUrlSupport) {
        connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaInfoExtracted,
                this, &MediaInfoWidget::onMediaInfoExtracted);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaFileLoaded,
                this, &MediaInfoWidget::onMediaFileLoaded);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaUrlLoaded,
                this, &MediaInfoWidget::onMediaUrlLoaded);
        
        qCDebug(mediaInfoWidget) << "FileUrlSupport connected";
        
        // Update with current media info if available
        if (m_autoUpdate) {
            refreshMediaInfo();
        }
    } else {
        qCDebug(mediaInfoWidget) << "FileUrlSupport disconnected";
        clearMediaInfo();
    }
}

void MediaInfoWidget::updateMediaInfo(const MediaInfo& mediaInfo)
{
    m_currentMediaInfo = mediaInfo;
    
    qCDebug(mediaInfoWidget) << "Updating media info for:" << mediaInfo.filePath;
    
    // Update all sections
    updateBasicInfo(mediaInfo);
    updateVideoInfo(mediaInfo);
    updateAudioInfo(mediaInfo);
    updateTechnicalInfo(mediaInfo);
    
    // Show/hide sections based on content
    m_videoInfoGroup->setVisible(mediaInfo.hasVideo);
    m_audioInfoGroup->setVisible(mediaInfo.hasAudio);
    
    emit mediaInfoUpdated(mediaInfo);
}

void MediaInfoWidget::clearMediaInfo()
{
    qCDebug(mediaInfoWidget) << "Clearing media info";
    
    m_currentMediaInfo.clear();
    
    // Clear all labels
    m_titleLabel->clear();
    m_formatLabel->clear();
    m_durationLabel->clear();
    m_fileSizeLabel->clear();
    
    m_resolutionLabel->clear();
    m_frameRateLabel->clear();
    m_videoCodecLabel->clear();
    m_videoBitrateLabel->clear();
    
    m_audioCodecLabel->clear();
    m_channelsLabel->clear();
    m_sampleRateLabel->clear();
    m_audioBitrateLabel->clear();
    
    m_filePathLabel->clear();
    m_hasVideoLabel->clear();
    m_hasAudioLabel->clear();
    
    // Hide sections
    m_videoInfoGroup->setVisible(false);
    m_audioInfoGroup->setVisible(false);
    
    // Show placeholder text
    m_titleLabel->setText("No media loaded");
}

void MediaInfoWidget::setShowTechnicalDetails(bool show)
{
    if (m_showTechnicalDetails != show) {
        m_showTechnicalDetails = show;
        m_technicalInfoGroup->setVisible(show);
        
        qCDebug(mediaInfoWidget) << "Technical details" << (show ? "shown" : "hidden");
    }
}

void MediaInfoWidget::setAutoUpdate(bool autoUpdate)
{
    if (m_autoUpdate != autoUpdate) {
        m_autoUpdate = autoUpdate;
        
        if (autoUpdate) {
            m_updateTimer->start();
        } else {
            m_updateTimer->stop();
        }
        
        qCDebug(mediaInfoWidget) << "Auto-update" << (autoUpdate ? "enabled" : "disabled");
    }
}

void MediaInfoWidget::setCompactMode(bool compact)
{
    if (m_compactMode != compact) {
        m_compactMode = compact;
        applyLayoutMode();
        
        qCDebug(mediaInfoWidget) << "Compact mode" << (compact ? "enabled" : "disabled");
    }
}

void MediaInfoWidget::refreshMediaInfo()
{
    if (!m_fileUrlSupport) {
        return;
    }
    
    MediaInfo currentInfo = m_fileUrlSupport->getCurrentMediaInfo();
    if (currentInfo.isValid && currentInfo.filePath != m_currentMediaInfo.filePath) {
        updateMediaInfo(currentInfo);
    }
}

void MediaInfoWidget::toggleTechnicalDetails()
{
    setShowTechnicalDetails(!m_showTechnicalDetails);
}

void MediaInfoWidget::onMediaInfoExtracted(const QString& filePath, const MediaInfo& mediaInfo)
{
    Q_UNUSED(filePath)
    
    if (m_autoUpdate) {
        updateMediaInfo(mediaInfo);
    }
}

void MediaInfoWidget::onMediaFileLoaded(const QString& filePath, const MediaInfo& mediaInfo)
{
    Q_UNUSED(filePath)
    
    if (m_autoUpdate) {
        updateMediaInfo(mediaInfo);
    }
}

void MediaInfoWidget::onMediaUrlLoaded(const QUrl& url, const MediaInfo& mediaInfo)
{
    Q_UNUSED(url)
    
    if (m_autoUpdate) {
        updateMediaInfo(mediaInfo);
    }
}

void MediaInfoWidget::initializeUI()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);
    
    // Create scroll area for content
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    
    // Create content widget
    m_contentWidget = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    
    // Create sections
    contentLayout->addWidget(createBasicInfoSection());
    contentLayout->addWidget(createVideoInfoSection());
    contentLayout->addWidget(createAudioInfoSection());
    contentLayout->addWidget(createTechnicalInfoSection());
    
    // Add stretch to push content to top
    contentLayout->addStretch();
    
    // Set up scroll area
    m_scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(m_scrollArea);
    
    // Apply initial layout mode
    applyLayoutMode();
    
    // Initialize with empty state
    clearMediaInfo();
}

QWidget* MediaInfoWidget::createBasicInfoSection()
{
    m_basicInfoGroup = new QGroupBox("Media Information");
    QGridLayout* layout = new QGridLayout(m_basicInfoGroup);
    
    // Title
    layout->addWidget(new QLabel("Title:"), 0, 0);
    m_titleLabel = new QLabel();
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel, 0, 1);
    
    // Format
    layout->addWidget(new QLabel("Format:"), 1, 0);
    m_formatLabel = new QLabel();
    layout->addWidget(m_formatLabel, 1, 1);
    
    // Duration
    layout->addWidget(new QLabel("Duration:"), 2, 0);
    m_durationLabel = new QLabel();
    layout->addWidget(m_durationLabel, 2, 1);
    
    // File size
    layout->addWidget(new QLabel("File Size:"), 3, 0);
    m_fileSizeLabel = new QLabel();
    layout->addWidget(m_fileSizeLabel, 3, 1);
    
    // Set column stretch
    layout->setColumnStretch(1, 1);
    
    return m_basicInfoGroup;
}

QWidget* MediaInfoWidget::createVideoInfoSection()
{
    m_videoInfoGroup = new QGroupBox("Video");
    QGridLayout* layout = new QGridLayout(m_videoInfoGroup);
    
    // Resolution
    layout->addWidget(new QLabel("Resolution:"), 0, 0);
    m_resolutionLabel = new QLabel();
    layout->addWidget(m_resolutionLabel, 0, 1);
    
    // Frame rate
    layout->addWidget(new QLabel("Frame Rate:"), 1, 0);
    m_frameRateLabel = new QLabel();
    layout->addWidget(m_frameRateLabel, 1, 1);
    
    // Video codec
    layout->addWidget(new QLabel("Codec:"), 2, 0);
    m_videoCodecLabel = new QLabel();
    layout->addWidget(m_videoCodecLabel, 2, 1);
    
    // Video bitrate
    layout->addWidget(new QLabel("Bitrate:"), 3, 0);
    m_videoBitrateLabel = new QLabel();
    layout->addWidget(m_videoBitrateLabel, 3, 1);
    
    // Set column stretch
    layout->setColumnStretch(1, 1);
    
    return m_videoInfoGroup;
}

QWidget* MediaInfoWidget::createAudioInfoSection()
{
    m_audioInfoGroup = new QGroupBox("Audio");
    QGridLayout* layout = new QGridLayout(m_audioInfoGroup);
    
    // Audio codec
    layout->addWidget(new QLabel("Codec:"), 0, 0);
    m_audioCodecLabel = new QLabel();
    layout->addWidget(m_audioCodecLabel, 0, 1);
    
    // Channels
    layout->addWidget(new QLabel("Channels:"), 1, 0);
    m_channelsLabel = new QLabel();
    layout->addWidget(m_channelsLabel, 1, 1);
    
    // Sample rate
    layout->addWidget(new QLabel("Sample Rate:"), 2, 0);
    m_sampleRateLabel = new QLabel();
    layout->addWidget(m_sampleRateLabel, 2, 1);
    
    // Audio bitrate
    layout->addWidget(new QLabel("Bitrate:"), 3, 0);
    m_audioBitrateLabel = new QLabel();
    layout->addWidget(m_audioBitrateLabel, 3, 1);
    
    // Set column stretch
    layout->setColumnStretch(1, 1);
    
    return m_audioInfoGroup;
}

QWidget* MediaInfoWidget::createTechnicalInfoSection()
{
    m_technicalInfoGroup = new QGroupBox("Technical Details");
    QGridLayout* layout = new QGridLayout(m_technicalInfoGroup);
    
    // File path
    layout->addWidget(new QLabel("File Path:"), 0, 0);
    m_filePathLabel = new QLabel();
    m_filePathLabel->setWordWrap(true);
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_filePathLabel, 0, 1);
    
    // Has video
    layout->addWidget(new QLabel("Has Video:"), 1, 0);
    m_hasVideoLabel = new QLabel();
    layout->addWidget(m_hasVideoLabel, 1, 1);
    
    // Has audio
    layout->addWidget(new QLabel("Has Audio:"), 2, 0);
    m_hasAudioLabel = new QLabel();
    layout->addWidget(m_hasAudioLabel, 2, 1);
    
    // Set column stretch
    layout->setColumnStretch(1, 1);
    
    // Initially hidden
    m_technicalInfoGroup->setVisible(false);
    
    return m_technicalInfoGroup;
}

void MediaInfoWidget::updateBasicInfo(const MediaInfo& mediaInfo)
{
    m_titleLabel->setText(mediaInfo.title.isEmpty() ? "Unknown" : mediaInfo.title);
    m_formatLabel->setText(mediaInfo.format.isEmpty() ? "Unknown" : mediaInfo.format);
    m_durationLabel->setText(formatDuration(mediaInfo.duration));
    m_fileSizeLabel->setText(formatFileSize(mediaInfo.fileSize));
}

void MediaInfoWidget::updateVideoInfo(const MediaInfo& mediaInfo)
{
    if (mediaInfo.hasVideo) {
        m_resolutionLabel->setText(QString("%1 x %2").arg(mediaInfo.width).arg(mediaInfo.height));
        m_frameRateLabel->setText(mediaInfo.frameRate > 0 ? 
                                 QString("%1 fps").arg(mediaInfo.frameRate, 0, 'f', 2) : 
                                 "Unknown");
        m_videoCodecLabel->setText(mediaInfo.videoCodec.isEmpty() ? "Unknown" : mediaInfo.videoCodec);
        m_videoBitrateLabel->setText(formatBitrate(mediaInfo.videoBitrate));
    } else {
        m_resolutionLabel->setText("N/A");
        m_frameRateLabel->setText("N/A");
        m_videoCodecLabel->setText("N/A");
        m_videoBitrateLabel->setText("N/A");
    }
}

void MediaInfoWidget::updateAudioInfo(const MediaInfo& mediaInfo)
{
    if (mediaInfo.hasAudio) {
        m_audioCodecLabel->setText(mediaInfo.audioCodec.isEmpty() ? "Unknown" : mediaInfo.audioCodec);
        m_channelsLabel->setText(mediaInfo.audioChannels > 0 ? 
                                QString::number(mediaInfo.audioChannels) : 
                                "Unknown");
        m_sampleRateLabel->setText(mediaInfo.audioSampleRate > 0 ? 
                                  QString("%1 Hz").arg(mediaInfo.audioSampleRate) : 
                                  "Unknown");
        m_audioBitrateLabel->setText(formatBitrate(mediaInfo.audioBitrate));
    } else {
        m_audioCodecLabel->setText("N/A");
        m_channelsLabel->setText("N/A");
        m_sampleRateLabel->setText("N/A");
        m_audioBitrateLabel->setText("N/A");
    }
}

void MediaInfoWidget::updateTechnicalInfo(const MediaInfo& mediaInfo)
{
    m_filePathLabel->setText(mediaInfo.filePath.isEmpty() ? "Unknown" : mediaInfo.filePath);
    m_hasVideoLabel->setText(mediaInfo.hasVideo ? "Yes" : "No");
    m_hasAudioLabel->setText(mediaInfo.hasAudio ? "Yes" : "No");
}

QString MediaInfoWidget::formatDuration(qint64 milliseconds) const
{
    if (milliseconds <= 0) {
        return "Unknown";
    }
    
    int totalSeconds = milliseconds / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
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

QString MediaInfoWidget::formatFileSize(qint64 bytes) const
{
    if (bytes <= 0) {
        return "Unknown";
    }
    
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString("%1 GB").arg(static_cast<double>(bytes) / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(static_cast<double>(bytes) / MB, 0, 'f', 1);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(static_cast<double>(bytes) / KB, 0, 'f', 0);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

QString MediaInfoWidget::formatBitrate(int kbps) const
{
    if (kbps <= 0) {
        return "Unknown";
    }
    
    if (kbps >= 1000) {
        return QString("%1 Mbps").arg(static_cast<double>(kbps) / 1000.0, 0, 'f', 1);
    } else {
        return QString("%1 kbps").arg(kbps);
    }
}

void MediaInfoWidget::applyLayoutMode()
{
    if (m_compactMode) {
        // In compact mode, use smaller fonts and tighter spacing
        QFont compactFont = font();
        compactFont.setPointSize(compactFont.pointSize() - 1);
        setFont(compactFont);
        
        m_mainLayout->setContentsMargins(4, 4, 4, 4);
        m_mainLayout->setSpacing(4);
    } else {
        // In normal mode, use default fonts and spacing
        QFont normalFont = font();
        setFont(normalFont);
        
        m_mainLayout->setContentsMargins(6, 6, 6, 6);
        m_mainLayout->setSpacing(6);
    }
}
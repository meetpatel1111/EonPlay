#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QTimer>
#include <memory>

class FileUrlSupport;
struct MediaInfo;

/**
 * @brief Widget for displaying detailed media information
 * 
 * Shows comprehensive information about the currently loaded media including
 * format details, codecs, bitrates, resolution, frame rate, and file size.
 * Updates automatically when new media is loaded.
 */
class MediaInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MediaInfoWidget(QWidget* parent = nullptr);
    ~MediaInfoWidget() override;
    
    /**
     * @brief Set the file URL support component
     * @param fileUrlSupport Shared pointer to FileUrlSupport component
     */
    void setFileUrlSupport(std::shared_ptr<FileUrlSupport> fileUrlSupport);
    
    /**
     * @brief Update display with new media information
     * @param mediaInfo Media information to display
     */
    void updateMediaInfo(const MediaInfo& mediaInfo);
    
    /**
     * @brief Clear all displayed information
     */
    void clearMediaInfo();
    
    /**
     * @brief Set whether to show technical details
     * @param show true to show technical details
     */
    void setShowTechnicalDetails(bool show);
    
    /**
     * @brief Check if technical details are shown
     * @return true if technical details are shown
     */
    bool showTechnicalDetails() const { return m_showTechnicalDetails; }
    
    /**
     * @brief Set whether to auto-update when media changes
     * @param autoUpdate true to enable auto-update
     */
    void setAutoUpdate(bool autoUpdate);
    
    /**
     * @brief Check if auto-update is enabled
     * @return true if auto-update is enabled
     */
    bool autoUpdate() const { return m_autoUpdate; }
    
    /**
     * @brief Set compact display mode
     * @param compact true for compact display
     */
    void setCompactMode(bool compact);
    
    /**
     * @brief Check if in compact mode
     * @return true if in compact mode
     */
    bool isCompactMode() const { return m_compactMode; }

public slots:
    /**
     * @brief Refresh media information display
     */
    void refreshMediaInfo();
    
    /**
     * @brief Toggle technical details visibility
     */
    void toggleTechnicalDetails();

signals:
    /**
     * @brief Emitted when media info is updated
     * @param mediaInfo Updated media information
     */
    void mediaInfoUpdated(const MediaInfo& mediaInfo);
    
    /**
     * @brief Emitted when user requests to take screenshot
     */
    void screenshotRequested();

private slots:
    /**
     * @brief Handle media info extraction completion
     * @param filePath Path to media file
     * @param mediaInfo Extracted media information
     */
    void onMediaInfoExtracted(const QString& filePath, const MediaInfo& mediaInfo);
    
    /**
     * @brief Handle media file loading
     * @param filePath Path to loaded media file
     * @param mediaInfo Media information
     */
    void onMediaFileLoaded(const QString& filePath, const MediaInfo& mediaInfo);
    
    /**
     * @brief Handle media URL loading
     * @param url Loaded media URL
     * @param mediaInfo Media information
     */
    void onMediaUrlLoaded(const QUrl& url, const MediaInfo& mediaInfo);

private:
    /**
     * @brief Initialize the user interface
     */
    void initializeUI();
    
    /**
     * @brief Create the basic info section
     * @return Widget containing basic info
     */
    QWidget* createBasicInfoSection();
    
    /**
     * @brief Create the video info section
     * @return Widget containing video info
     */
    QWidget* createVideoInfoSection();
    
    /**
     * @brief Create the audio info section
     * @return Widget containing audio info
     */
    QWidget* createAudioInfoSection();
    
    /**
     * @brief Create the technical info section
     * @return Widget containing technical info
     */
    QWidget* createTechnicalInfoSection();
    
    /**
     * @brief Update basic information labels
     * @param mediaInfo Media information to display
     */
    void updateBasicInfo(const MediaInfo& mediaInfo);
    
    /**
     * @brief Update video information labels
     * @param mediaInfo Media information to display
     */
    void updateVideoInfo(const MediaInfo& mediaInfo);
    
    /**
     * @brief Update audio information labels
     * @param mediaInfo Media information to display
     */
    void updateAudioInfo(const MediaInfo& mediaInfo);
    
    /**
     * @brief Update technical information labels
     * @param mediaInfo Media information to display
     */
    void updateTechnicalInfo(const MediaInfo& mediaInfo);
    
    /**
     * @brief Format duration for display
     * @param milliseconds Duration in milliseconds
     * @return Formatted duration string
     */
    QString formatDuration(qint64 milliseconds) const;
    
    /**
     * @brief Format file size for display
     * @param bytes File size in bytes
     * @return Formatted file size string
     */
    QString formatFileSize(qint64 bytes) const;
    
    /**
     * @brief Format bitrate for display
     * @param kbps Bitrate in kbps
     * @return Formatted bitrate string
     */
    QString formatBitrate(int kbps) const;
    
    /**
     * @brief Apply compact or full layout
     */
    void applyLayoutMode();
    
    std::shared_ptr<FileUrlSupport> m_fileUrlSupport;
    
    // Current media info
    MediaInfo m_currentMediaInfo;
    
    // Settings
    bool m_showTechnicalDetails;
    bool m_autoUpdate;
    bool m_compactMode;
    
    // UI components
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_mainLayout;
    
    // Basic info section
    QGroupBox* m_basicInfoGroup;
    QLabel* m_titleLabel;
    QLabel* m_formatLabel;
    QLabel* m_durationLabel;
    QLabel* m_fileSizeLabel;
    
    // Video info section
    QGroupBox* m_videoInfoGroup;
    QLabel* m_resolutionLabel;
    QLabel* m_frameRateLabel;
    QLabel* m_videoCodecLabel;
    QLabel* m_videoBitrateLabel;
    
    // Audio info section
    QGroupBox* m_audioInfoGroup;
    QLabel* m_audioCodecLabel;
    QLabel* m_channelsLabel;
    QLabel* m_sampleRateLabel;
    QLabel* m_audioBitrateLabel;
    
    // Technical info section
    QGroupBox* m_technicalInfoGroup;
    QLabel* m_filePathLabel;
    QLabel* m_hasVideoLabel;
    QLabel* m_hasAudioLabel;
    
    // Update timer for periodic refresh
    QTimer* m_updateTimer;
};
#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QColor>
#include <memory>

/**
 * @brief Advanced video processing system with filters and enhancements
 * 
 * Provides comprehensive video processing including color filters,
 * deinterlacing, frame rate display, and video information overlay.
 */
class VideoProcessor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Video filter types
     */
    enum FilterType {
        None = 0,
        Brightness,
        Contrast,
        Saturation,
        Hue,
        Gamma,
        Sepia,
        Grayscale,
        Invert,
        Blur,
        Sharpen,
        EdgeDetection,
        Emboss,
        Vintage,
        WarmTone,
        CoolTone,
        HighContrast,
        Deinterlace
    };

    /**
     * @brief Video processing parameters
     */
    struct ProcessingParameters {
        // Color adjustments
        float brightness = 0.0f;      // -100 to +100
        float contrast = 0.0f;        // -100 to +100
        float saturation = 0.0f;      // -100 to +100
        float hue = 0.0f;            // -180 to +180
        float gamma = 1.0f;          // 0.1 to 3.0
        
        // Filter effects
        float sepia = 0.0f;          // 0.0 to 1.0
        float blur = 0.0f;           // 0.0 to 10.0
        float sharpen = 0.0f;        // 0.0 to 2.0
        
        // Deinterlacing
        bool deinterlaceEnabled = false;
        QString deinterlaceMethod = "linear"; // linear, bob, yadif
        
        ProcessingParameters() = default;
    };

    /**
     * @brief Video information structure
     */
    struct VideoInfo {
        QString codec;               // Video codec
        QString profile;             // Codec profile
        int width = 0;              // Video width
        int height = 0;             // Video height
        float frameRate = 0.0f;     // Frame rate
        float aspectRatio = 0.0f;   // Aspect ratio
        qint64 bitrate = 0;         // Bitrate in bps
        QString colorSpace;         // Color space
        QString pixelFormat;        // Pixel format
        bool isHDR = false;         // HDR content
        int currentFrame = 0;       // Current frame number
        qint64 totalFrames = 0;     // Total frame count
        
        VideoInfo() = default;
    };

    /**
     * @brief Chapter information
     */
    struct ChapterInfo {
        int id;                     // Chapter ID
        QString title;              // Chapter title
        qint64 startTime;          // Start time in milliseconds
        qint64 endTime;            // End time in milliseconds
        QString thumbnail;          // Thumbnail path
        
        ChapterInfo() : id(-1), startTime(0), endTime(0) {}
        ChapterInfo(int chapterId, const QString& chapterTitle, qint64 start, qint64 end)
            : id(chapterId), title(chapterTitle), startTime(start), endTime(end) {}
    };

    explicit VideoProcessor(QObject* parent = nullptr);
    ~VideoProcessor() override;

    /**
     * @brief Initialize the video processor
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the processor
     */
    void shutdown();

    /**
     * @brief Check if processor is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Enable or disable the processor
     * @param enabled Enable state
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get active filters
     * @return List of active filter types
     */
    QVector<FilterType> getActiveFilters() const;

    /**
     * @brief Enable or disable a filter
     * @param filter Filter type
     * @param enabled Enable state
     */
    void setFilterEnabled(FilterType filter, bool enabled);

    /**
     * @brief Check if a filter is enabled
     * @param filter Filter type
     * @return true if enabled
     */
    bool isFilterEnabled(FilterType filter) const;

    /**
     * @brief Get filter name
     * @param filter Filter type
     * @return Filter name
     */
    static QString getFilterName(FilterType filter);

    /**
     * @brief Get all available filters
     * @return List of filter types
     */
    static QVector<FilterType> getAvailableFilters();

    /**
     * @brief Get processing parameters
     * @return Current parameters
     */
    ProcessingParameters getParameters() const;

    /**
     * @brief Set processing parameters
     * @param params New parameters
     */
    void setParameters(const ProcessingParameters& params);

    /**
     * @brief Get specific parameter value
     * @param filter Filter type
     * @param paramName Parameter name
     * @return Parameter value
     */
    float getParameter(FilterType filter, const QString& paramName) const;

    /**
     * @brief Set specific parameter value
     * @param filter Filter type
     * @param paramName Parameter name
     * @param value Parameter value
     */
    void setParameter(FilterType filter, const QString& paramName, float value);

    /**
     * @brief Get current video information
     * @return Video information
     */
    VideoInfo getCurrentVideoInfo() const;

    /**
     * @brief Update video information
     * @param info New video information
     */
    void updateVideoInfo(const VideoInfo& info);

    /**
     * @brief Get video chapters
     * @return List of chapters
     */
    QVector<ChapterInfo> getChapters() const;

    /**
     * @brief Set video chapters
     * @param chapters Chapter list
     */
    void setChapters(const QVector<ChapterInfo>& chapters);

    /**
     * @brief Get current chapter
     * @param currentTime Current playback time in milliseconds
     * @return Current chapter info
     */
    ChapterInfo getCurrentChapter(qint64 currentTime) const;

    /**
     * @brief Get next chapter
     * @param currentTime Current playback time in milliseconds
     * @return Next chapter info
     */
    ChapterInfo getNextChapter(qint64 currentTime) const;

    /**
     * @brief Get previous chapter
     * @param currentTime Current playback time in milliseconds
     * @return Previous chapter info
     */
    ChapterInfo getPreviousChapter(qint64 currentTime) const;

    /**
     * @brief Check if video information overlay is enabled
     * @return true if enabled
     */
    bool isInfoOverlayEnabled() const;

    /**
     * @brief Enable or disable video information overlay
     * @param enabled Enable state
     */
    void setInfoOverlayEnabled(bool enabled);

    /**
     * @brief Get overlay opacity
     * @return Opacity (0.0 to 1.0)
     */
    float getOverlayOpacity() const;

    /**
     * @brief Set overlay opacity
     * @param opacity Opacity (0.0 to 1.0)
     */
    void setOverlayOpacity(float opacity);

    /**
     * @brief Reset all filters to default
     */
    void resetFilters();

    /**
     * @brief Load filter preset
     * @param presetName Preset name
     * @return true if loaded successfully
     */
    bool loadFilterPreset(const QString& presetName);

    /**
     * @brief Save current settings as preset
     * @param presetName Preset name
     * @return true if saved successfully
     */
    bool saveFilterPreset(const QString& presetName);

    /**
     * @brief Get available presets
     * @return List of preset names
     */
    QStringList getAvailablePresets() const;

signals:
    /**
     * @brief Emitted when processor is enabled/disabled
     * @param enabled New enabled state
     */
    void enabledChanged(bool enabled);

    /**
     * @brief Emitted when a filter is enabled/disabled
     * @param filter Filter type
     * @param enabled New enabled state
     */
    void filterEnabledChanged(FilterType filter, bool enabled);

    /**
     * @brief Emitted when parameters change
     * @param params New parameters
     */
    void parametersChanged(const ProcessingParameters& params);

    /**
     * @brief Emitted when video info is updated
     * @param info New video information
     */
    void videoInfoUpdated(const VideoInfo& info);

    /**
     * @brief Emitted when chapters are updated
     * @param chapters New chapter list
     */
    void chaptersUpdated(const QVector<ChapterInfo>& chapters);

    /**
     * @brief Emitted when current chapter changes
     * @param chapter New current chapter
     */
    void currentChapterChanged(const ChapterInfo& chapter);

private:
    void applyColorAdjustments(ProcessingParameters& params);
    void applyFilterEffects(ProcessingParameters& params);
    void applyDeinterlacing(ProcessingParameters& params);
    float clampParameter(float value, float min, float max) const;

    // Processor state
    bool m_enabled;
    QVector<FilterType> m_activeFilters;
    ProcessingParameters m_parameters;
    VideoInfo m_currentVideoInfo;
    QVector<ChapterInfo> m_chapters;
    
    // Overlay settings
    bool m_infoOverlayEnabled;
    float m_overlayOpacity;

    // Thread safety
    mutable QMutex m_processingMutex;

    // Constants
    static constexpr float MIN_BRIGHTNESS = -100.0f;
    static constexpr float MAX_BRIGHTNESS = 100.0f;
    static constexpr float MIN_CONTRAST = -100.0f;
    static constexpr float MAX_CONTRAST = 100.0f;
    static constexpr float MIN_SATURATION = -100.0f;
    static constexpr float MAX_SATURATION = 100.0f;
    static constexpr float MIN_HUE = -180.0f;
    static constexpr float MAX_HUE = 180.0f;
    static constexpr float MIN_GAMMA = 0.1f;
    static constexpr float MAX_GAMMA = 3.0f;
};

#endif // VIDEOPROCESSOR_H
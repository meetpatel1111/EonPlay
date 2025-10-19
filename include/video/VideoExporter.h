#ifndef VIDEOEXPORTER_H
#define VIDEOEXPORTER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QPixmap>
#include <QImage>
#include <QSize>
#include <memory>

/**
 * @brief Advanced video export system with screenshots, GIF, and processing
 * 
 * Provides comprehensive video export capabilities including screenshot capture,
 * GIF creation, HDR support, AI upscaling, scene detection, and color correction.
 */
class VideoExporter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Export formats
     */
    enum ExportFormat {
        PNG = 0,
        JPEG,
        BMP,
        TIFF,
        GIF,
        MP4,
        AVI,
        MOV,
        WEBM
    };

    /**
     * @brief Export quality levels
     */
    enum QualityLevel {
        Low = 0,
        Medium,
        High,
        Lossless
    };

    /**
     * @brief HDR formats
     */
    enum HDRFormat {
        SDR = 0,        // Standard Dynamic Range
        HDR10,          // HDR10
        HDR10Plus,      // HDR10+
        DolbyVision,    // Dolby Vision
        HLG             // Hybrid Log-Gamma
    };

    /**
     * @brief AI upscaling methods
     */
    enum UpscalingMethod {
        None = 0,
        Bilinear,
        Bicubic,
        Lanczos,
        ESRGAN,         // Enhanced Super-Resolution GAN
        RealESRGAN,     // Real-ESRGAN
        EDSR,           // Enhanced Deep Super-Resolution
        SRCNN           // Super-Resolution CNN
    };

    /**
     * @brief Screenshot options
     */
    struct ScreenshotOptions {
        ExportFormat format = PNG;
        QualityLevel quality = High;
        QSize resolution;           // Custom resolution (empty = original)
        bool includeSubtitles = true;
        bool includeOverlay = false;
        QString outputPath;
        
        ScreenshotOptions() = default;
    };

    /**
     * @brief GIF export options
     */
    struct GIFOptions {
        qint64 startTime = 0;       // Start time in milliseconds
        qint64 duration = 5000;     // Duration in milliseconds
        int frameRate = 15;         // Frames per second
        QSize resolution;           // Custom resolution (empty = original)
        QualityLevel quality = Medium;
        bool loop = true;           // Loop animation
        int colorCount = 256;       // Number of colors (2-256)
        QString outputPath;
        
        GIFOptions() = default;
    };

    /**
     * @brief Video export options
     */
    struct VideoExportOptions {
        ExportFormat format = MP4;
        QualityLevel quality = High;
        QSize resolution;           // Custom resolution (empty = original)
        int bitrate = 0;           // Bitrate in kbps (0 = auto)
        int frameRate = 0;         // Frame rate (0 = original)
        HDRFormat hdrFormat = SDR;
        UpscalingMethod upscaling = None;
        float upscaleRatio = 2.0f; // Upscaling ratio (1.0-4.0)
        bool enableColorCorrection = false;
        qint64 startTime = 0;      // Start time in milliseconds
        qint64 duration = 0;       // Duration in milliseconds (0 = full)
        QString outputPath;
        
        VideoExportOptions() = default;
    };

    /**
     * @brief Scene detection result
     */
    struct SceneInfo {
        qint64 startTime;          // Scene start time in milliseconds
        qint64 endTime;            // Scene end time in milliseconds
        QString description;        // Scene description
        float confidence;          // Detection confidence (0.0-1.0)
        QPixmap thumbnail;         // Scene thumbnail
        
        SceneInfo() : startTime(0), endTime(0), confidence(0.0f) {}
        SceneInfo(qint64 start, qint64 end, const QString& desc, float conf)
            : startTime(start), endTime(end), description(desc), confidence(conf) {}
    };

    explicit VideoExporter(QObject* parent = nullptr);
    ~VideoExporter() override;

    /**
     * @brief Initialize the video exporter
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the exporter
     */
    void shutdown();

    // Screenshot functionality
    /**
     * @brief Capture screenshot at current position
     * @param options Screenshot options
     * @return true if capture started successfully
     */
    bool captureScreenshot(const ScreenshotOptions& options);

    /**
     * @brief Capture screenshot at specific time
     * @param timeMs Time position in milliseconds
     * @param options Screenshot options
     * @return true if capture started successfully
     */
    bool captureScreenshotAtTime(qint64 timeMs, const ScreenshotOptions& options);

    // GIF export functionality
    /**
     * @brief Export GIF animation
     * @param options GIF export options
     * @return true if export started successfully
     */
    bool exportGIF(const GIFOptions& options);

    // Video export functionality
    /**
     * @brief Export video with processing
     * @param options Video export options
     * @return true if export started successfully
     */
    bool exportVideo(const VideoExportOptions& options);

    // HDR support
    /**
     * @brief Check if HDR is supported
     * @param format HDR format to check
     * @return true if supported
     */
    bool isHDRSupported(HDRFormat format) const;

    /**
     * @brief Get supported HDR formats
     * @return List of supported HDR formats
     */
    QVector<HDRFormat> getSupportedHDRFormats() const;

    // AI upscaling
    /**
     * @brief Check if AI upscaling is available
     * @param method Upscaling method to check
     * @return true if available
     */
    bool isUpscalingAvailable(UpscalingMethod method) const;

    /**
     * @brief Get available upscaling methods
     * @return List of available methods
     */
    QVector<UpscalingMethod> getAvailableUpscalingMethods() const;

    // Scene detection
    /**
     * @brief Detect scenes in video
     * @param threshold Detection threshold (0.0-1.0)
     * @return true if detection started successfully
     */
    bool detectScenes(float threshold = 0.3f);

    /**
     * @brief Get detected scenes
     * @return List of detected scenes
     */
    QVector<SceneInfo> getDetectedScenes() const;

    // Color correction
    /**
     * @brief Apply smart color correction
     * @param autoAdjust Enable automatic adjustment
     * @return true if correction applied successfully
     */
    bool applyColorCorrection(bool autoAdjust = true);

    /**
     * @brief Get export progress
     * @return Progress percentage (0-100)
     */
    int getExportProgress() const;

    /**
     * @brief Cancel ongoing export
     */
    void cancelExport();

    /**
     * @brief Check if export is in progress
     * @return true if exporting
     */
    bool isExporting() const;

    /**
     * @brief Get supported export formats
     * @return List of supported formats
     */
    static QVector<ExportFormat> getSupportedFormats();

    /**
     * @brief Get format name
     * @param format Export format
     * @return Format name
     */
    static QString getFormatName(ExportFormat format);

    /**
     * @brief Get format extension
     * @param format Export format
     * @return File extension
     */
    static QString getFormatExtension(ExportFormat format);

signals:
    /**
     * @brief Emitted when screenshot capture completes
     * @param success Capture success
     * @param filePath Output file path
     */
    void screenshotCaptured(bool success, const QString& filePath);

    /**
     * @brief Emitted when GIF export completes
     * @param success Export success
     * @param filePath Output file path
     */
    void gifExported(bool success, const QString& filePath);

    /**
     * @brief Emitted when video export completes
     * @param success Export success
     * @param filePath Output file path
     */
    void videoExported(bool success, const QString& filePath);

    /**
     * @brief Emitted when export progress updates
     * @param progress Progress percentage (0-100)
     */
    void exportProgressUpdated(int progress);

    /**
     * @brief Emitted when scene detection completes
     * @param scenes Detected scenes
     */
    void scenesDetected(const QVector<SceneInfo>& scenes);

    /**
     * @brief Emitted when color correction completes
     * @param success Correction success
     */
    void colorCorrectionApplied(bool success);

private slots:
    void updateExportProgress();

private:
    // Screenshot methods
    bool captureCurrentFrame(const ScreenshotOptions& options);
    bool captureFrameAtTime(qint64 timeMs, const ScreenshotOptions& options);
    QImage processScreenshot(const QImage& image, const ScreenshotOptions& options);

    // GIF export methods
    bool createGIFAnimation(const GIFOptions& options);
    QVector<QImage> extractFramesForGIF(const GIFOptions& options);
    bool writeGIFFile(const QVector<QImage>& frames, const GIFOptions& options);

    // Video export methods
    bool processVideoExport(const VideoExportOptions& options);
    bool applyVideoFilters(const VideoExportOptions& options);
    bool encodeVideo(const VideoExportOptions& options);

    // HDR processing
    bool processHDRContent(const VideoExportOptions& options);
    QImage convertToHDR(const QImage& image, HDRFormat format);

    // AI upscaling methods
    bool initializeAIModels();
    QImage upscaleImage(const QImage& image, UpscalingMethod method, float ratio);
    QImage applyESRGAN(const QImage& image, float ratio);
    QImage applyRealESRGAN(const QImage& image, float ratio);
    QImage applyEDSR(const QImage& image, float ratio);
    QImage applySRCNN(const QImage& image, float ratio);

    // Scene detection methods
    bool analyzeVideoForScenes(float threshold);
    float calculateSceneChange(const QImage& frame1, const QImage& frame2);
    QString generateSceneDescription(const QImage& frame);
    QPixmap createSceneThumbnail(const QImage& frame);

    // Color correction methods
    bool analyzeColorBalance();
    QImage correctColors(const QImage& image);
    QImage adjustWhiteBalance(const QImage& image);
    QImage enhanceContrast(const QImage& image);
    QImage adjustSaturation(const QImage& image);

    // Helper methods
    QSize calculateOptimalSize(const QSize& original, const QSize& target);
    QString generateUniqueFilename(const QString& basePath, const QString& extension);
    bool validateExportOptions(const VideoExportOptions& options);

    // Export state
    bool m_exporting;
    int m_exportProgress;
    QTimer* m_progressTimer;

    // Scene detection results
    QVector<SceneInfo> m_detectedScenes;

    // AI model availability
    bool m_aiModelsLoaded;
    QVector<UpscalingMethod> m_availableUpscalingMethods;

    // HDR support
    QVector<HDRFormat> m_supportedHDRFormats;

    // Thread safety
    mutable QMutex m_exportMutex;

    // Constants
    static constexpr int DEFAULT_GIF_FRAMERATE = 15;
    static constexpr int MAX_GIF_DURATION = 30000; // 30 seconds
    static constexpr int DEFAULT_SCREENSHOT_QUALITY = 95;
    static constexpr float DEFAULT_SCENE_THRESHOLD = 0.3f;
    static constexpr float MAX_UPSCALE_RATIO = 4.0f;
};

#endif // VIDEOEXPORTER_H
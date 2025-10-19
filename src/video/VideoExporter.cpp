#include "video/VideoExporter.h"
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QImageWriter>
#include <QImageReader>
#include <QtMath>
#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(videoExporter)
Q_LOGGING_CATEGORY(videoExporter, "video.exporter")

VideoExporter::VideoExporter(QObject* parent)
    : QObject(parent)
    , m_exporting(false)
    , m_exportProgress(0)
    , m_progressTimer(new QTimer(this))
    , m_aiModelsLoaded(false)
{
    // Initialize supported formats
    m_supportedHDRFormats = {SDR, HDR10, HLG}; // Basic HDR support
    m_availableUpscalingMethods = {None, Bilinear, Bicubic, Lanczos}; // Basic upscaling
    
    // Setup progress timer
    m_progressTimer->setInterval(100); // Update every 100ms
    connect(m_progressTimer, &QTimer::timeout, this, &VideoExporter::updateExportProgress);
    
    qCDebug(videoExporter) << "VideoExporter created";
}

VideoExporter::~VideoExporter()
{
    shutdown();
    qCDebug(videoExporter) << "VideoExporter destroyed";
}

bool VideoExporter::initialize()
{
    // Initialize AI models if available
    initializeAIModels();
    
    qCDebug(videoExporter) << "VideoExporter initialized";
    return true;
}

void VideoExporter::shutdown()
{
    cancelExport();
    qCDebug(videoExporter) << "VideoExporter shutdown";
}

bool VideoExporter::captureScreenshot(const ScreenshotOptions& options)
{
    if (m_exporting) {
        qCWarning(videoExporter) << "Export already in progress";
        return false;
    }
    
    return captureCurrentFrame(options);
}

bool VideoExporter::captureScreenshotAtTime(qint64 timeMs, const ScreenshotOptions& options)
{
    if (m_exporting) {
        qCWarning(videoExporter) << "Export already in progress";
        return false;
    }
    
    return captureFrameAtTime(timeMs, options);
}b
ool VideoExporter::exportGIF(const GIFOptions& options)
{
    if (m_exporting) {
        qCWarning(videoExporter) << "Export already in progress";
        return false;
    }
    
    QMutexLocker locker(&m_exportMutex);
    m_exporting = true;
    m_exportProgress = 0;
    m_progressTimer->start();
    
    // Start GIF creation in background
    QTimer::singleShot(0, this, [this, options]() {
        bool success = createGIFAnimation(options);
        
        QMutexLocker locker(&m_exportMutex);
        m_exporting = false;
        m_exportProgress = success ? 100 : 0;
        m_progressTimer->stop();
        
        emit gifExported(success, options.outputPath);
        qCDebug(videoExporter) << "GIF export completed:" << success;
    });
    
    return true;
}

bool VideoExporter::exportVideo(const VideoExportOptions& options)
{
    if (m_exporting) {
        qCWarning(videoExporter) << "Export already in progress";
        return false;
    }
    
    if (!validateExportOptions(options)) {
        qCWarning(videoExporter) << "Invalid export options";
        return false;
    }
    
    QMutexLocker locker(&m_exportMutex);
    m_exporting = true;
    m_exportProgress = 0;
    m_progressTimer->start();
    
    // Start video export in background
    QTimer::singleShot(0, this, [this, options]() {
        bool success = processVideoExport(options);
        
        QMutexLocker locker(&m_exportMutex);
        m_exporting = false;
        m_exportProgress = success ? 100 : 0;
        m_progressTimer->stop();
        
        emit videoExported(success, options.outputPath);
        qCDebug(videoExporter) << "Video export completed:" << success;
    });
    
    return true;
}

bool VideoExporter::isHDRSupported(HDRFormat format) const
{
    return m_supportedHDRFormats.contains(format);
}

QVector<VideoExporter::HDRFormat> VideoExporter::getSupportedHDRFormats() const
{
    return m_supportedHDRFormats;
}

bool VideoExporter::isUpscalingAvailable(UpscalingMethod method) const
{
    return m_availableUpscalingMethods.contains(method);
}

QVector<VideoExporter::UpscalingMethod> VideoExporter::getAvailableUpscalingMethods() const
{
    return m_availableUpscalingMethods;
}

bool VideoExporter::detectScenes(float threshold)
{
    if (m_exporting) {
        qCWarning(videoExporter) << "Export in progress, cannot detect scenes";
        return false;
    }
    
    // Start scene detection in background
    QTimer::singleShot(0, this, [this, threshold]() {
        bool success = analyzeVideoForScenes(threshold);
        if (success) {
            emit scenesDetected(m_detectedScenes);
        }
        qCDebug(videoExporter) << "Scene detection completed:" << success;
    });
    
    return true;
}

QVector<VideoExporter::SceneInfo> VideoExporter::getDetectedScenes() const
{
    QMutexLocker locker(&m_exportMutex);
    return m_detectedScenes;
}

bool VideoExporter::applyColorCorrection(bool autoAdjust)
{
    Q_UNUSED(autoAdjust)
    
    // Start color correction in background
    QTimer::singleShot(0, this, [this]() {
        bool success = analyzeColorBalance();
        emit colorCorrectionApplied(success);
        qCDebug(videoExporter) << "Color correction completed:" << success;
    });
    
    return true;
}

int VideoExporter::getExportProgress() const
{
    QMutexLocker locker(&m_exportMutex);
    return m_exportProgress;
}

void VideoExporter::cancelExport()
{
    QMutexLocker locker(&m_exportMutex);
    if (m_exporting) {
        m_exporting = false;
        m_progressTimer->stop();
        qCDebug(videoExporter) << "Export cancelled";
    }
}

bool VideoExporter::isExporting() const
{
    QMutexLocker locker(&m_exportMutex);
    return m_exporting;
}

QVector<VideoExporter::ExportFormat> VideoExporter::getSupportedFormats()
{
    return {PNG, JPEG, BMP, TIFF, GIF, MP4, AVI, MOV, WEBM};
}

QString VideoExporter::getFormatName(ExportFormat format)
{
    switch (format) {
        case PNG: return "PNG Image";
        case JPEG: return "JPEG Image";
        case BMP: return "Bitmap Image";
        case TIFF: return "TIFF Image";
        case GIF: return "GIF Animation";
        case MP4: return "MP4 Video";
        case AVI: return "AVI Video";
        case MOV: return "QuickTime Movie";
        case WEBM: return "WebM Video";
        default: return "Unknown";
    }
}

QString VideoExporter::getFormatExtension(ExportFormat format)
{
    switch (format) {
        case PNG: return "png";
        case JPEG: return "jpg";
        case BMP: return "bmp";
        case TIFF: return "tiff";
        case GIF: return "gif";
        case MP4: return "mp4";
        case AVI: return "avi";
        case MOV: return "mov";
        case WEBM: return "webm";
        default: return "bin";
    }
}
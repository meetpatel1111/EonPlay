#include "video/VideoProcessor.h"
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>
#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(videoProcessor)
Q_LOGGING_CATEGORY(videoProcessor, "video.processor")

VideoProcessor::VideoProcessor(QObject* parent)
    : QObject(parent)
    , m_enabled(false)
    , m_infoOverlayEnabled(false)
    , m_overlayOpacity(0.8f)
{
    qCDebug(videoProcessor) << "VideoProcessor created";
}

VideoProcessor::~VideoProcessor()
{
    shutdown();
    qCDebug(videoProcessor) << "VideoProcessor destroyed";
}

bool VideoProcessor::initialize()
{
    qCDebug(videoProcessor) << "VideoProcessor initialized";
    return true;
}

void VideoProcessor::shutdown()
{
    setEnabled(false);
    qCDebug(videoProcessor) << "VideoProcessor shutdown";
}

bool VideoProcessor::isEnabled() const
{
    return m_enabled;
}

void VideoProcessor::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!enabled) {
            resetFilters();
        }
        emit enabledChanged(m_enabled);
        qCDebug(videoProcessor) << "Video processor" << (enabled ? "enabled" : "disabled");
    }
}

QVector<VideoProcessor::FilterType> VideoProcessor::getActiveFilters() const
{
    QMutexLocker locker(&m_processingMutex);
    return m_activeFilters;
}

void VideoProcessor::setFilterEnabled(FilterType filter, bool enabled)
{
    QMutexLocker locker(&m_processingMutex);
    
    if (enabled && !m_activeFilters.contains(filter)) {
        m_activeFilters.append(filter);
        emit filterEnabledChanged(filter, true);
        qCDebug(videoProcessor) << "Filter enabled:" << getFilterName(filter);
    } else if (!enabled && m_activeFilters.contains(filter)) {
        m_activeFilters.removeAll(filter);
        emit filterEnabledChanged(filter, false);
        qCDebug(videoProcessor) << "Filter disabled:" << getFilterName(filter);
    }
}

bool VideoProcessor::isFilterEnabled(FilterType filter) const
{
    QMutexLocker locker(&m_processingMutex);
    return m_activeFilters.contains(filter);
}

QString VideoProcessor::getFilterName(FilterType filter)
{
    switch (filter) {
        case None: return "None";
        case Brightness: return "Brightness";
        case Contrast: return "Contrast";
        case Saturation: return "Saturation";
        case Hue: return "Hue";
        case Gamma: return "Gamma";
        case Sepia: return "Sepia";
        case Grayscale: return "Grayscale";
        case Invert: return "Invert";
        case Blur: return "Blur";
        case Sharpen: return "Sharpen";
        case EdgeDetection: return "Edge Detection";
        case Emboss: return "Emboss";
        case Vintage: return "Vintage";
        case WarmTone: return "Warm Tone";
        case CoolTone: return "Cool Tone";
        case HighContrast: return "High Contrast";
        case Deinterlace: return "Deinterlace";
        default: return "Unknown";
    }
}

QVector<VideoProcessor::FilterType> VideoProcessor::getAvailableFilters()
{
    return {
        Brightness, Contrast, Saturation, Hue, Gamma,
        Sepia, Grayscale, Invert, Blur, Sharpen,
        EdgeDetection, Emboss, Vintage, WarmTone, CoolTone,
        HighContrast, Deinterlace
    };
}

VideoProcessor::ProcessingParameters VideoProcessor::getParameters() const
{
    QMutexLocker locker(&m_processingMutex);
    return m_parameters;
}

void VideoProcessor::setParameters(const ProcessingParameters& params)
{
    QMutexLocker locker(&m_processingMutex);
    
    // Clamp all parameters to valid ranges
    ProcessingParameters clampedParams = params;
    clampedParams.brightness = clampParameter(params.brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    clampedParams.contrast = clampParameter(params.contrast, MIN_CONTRAST, MAX_CONTRAST);
    clampedParams.saturation = clampParameter(params.saturation, MIN_SATURATION, MAX_SATURATION);
    clampedParams.hue = clampParameter(params.hue, MIN_HUE, MAX_HUE);
    clampedParams.gamma = clampParameter(params.gamma, MIN_GAMMA, MAX_GAMMA);
    clampedParams.sepia = clampParameter(params.sepia, 0.0f, 1.0f);
    clampedParams.blur = clampParameter(params.blur, 0.0f, 10.0f);
    clampedParams.sharpen = clampParameter(params.sharpen, 0.0f, 2.0f);
    
    m_parameters = clampedParams;
    emit parametersChanged(m_parameters);
    qCDebug(videoProcessor) << "Video processing parameters updated";
}

float VideoProcessor::getParameter(FilterType filter, const QString& paramName) const
{
    QMutexLocker locker(&m_processingMutex);
    
    switch (filter) {
        case Brightness:
            if (paramName == "brightness") return m_parameters.brightness;
            break;
        case Contrast:
            if (paramName == "contrast") return m_parameters.contrast;
            break;
        case Saturation:
            if (paramName == "saturation") return m_parameters.saturation;
            break;
        case Hue:
            if (paramName == "hue") return m_parameters.hue;
            break;
        case Gamma:
            if (paramName == "gamma") return m_parameters.gamma;
            break;
        case Sepia:
            if (paramName == "sepia") return m_parameters.sepia;
            break;
        case Blur:
            if (paramName == "blur") return m_parameters.blur;
            break;
        case Sharpen:
            if (paramName == "sharpen") return m_parameters.sharpen;
            break;
        default:
            break;
    }
    
    return 0.0f;
}

void VideoProcessor::setParameter(FilterType filter, const QString& paramName, float value)
{
    QMutexLocker locker(&m_processingMutex);
    
    switch (filter) {
        case Brightness:
            if (paramName == "brightness") {
                m_parameters.brightness = clampParameter(value, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
            }
            break;
        case Contrast:
            if (paramName == "contrast") {
                m_parameters.contrast = clampParameter(value, MIN_CONTRAST, MAX_CONTRAST);
            }
            break;
        case Saturation:
            if (paramName == "saturation") {
                m_parameters.saturation = clampParameter(value, MIN_SATURATION, MAX_SATURATION);
            }
            break;
        case Hue:
            if (paramName == "hue") {
                m_parameters.hue = clampParameter(value, MIN_HUE, MAX_HUE);
            }
            break;
        case Gamma:
            if (paramName == "gamma") {
                m_parameters.gamma = clampParameter(value, MIN_GAMMA, MAX_GAMMA);
            }
            break;
        case Sepia:
            if (paramName == "sepia") {
                m_parameters.sepia = clampParameter(value, 0.0f, 1.0f);
            }
            break;
        case Blur:
            if (paramName == "blur") {
                m_parameters.blur = clampParameter(value, 0.0f, 10.0f);
            }
            break;
        case Sharpen:
            if (paramName == "sharpen") {
                m_parameters.sharpen = clampParameter(value, 0.0f, 2.0f);
            }
            break;
        default:
            break;
    }
    
    emit parametersChanged(m_parameters);
}

VideoProcessor::VideoInfo VideoProcessor::getCurrentVideoInfo() const
{
    QMutexLocker locker(&m_processingMutex);
    return m_currentVideoInfo;
}

void VideoProcessor::updateVideoInfo(const VideoInfo& info)
{
    QMutexLocker locker(&m_processingMutex);
    
    if (m_currentVideoInfo.codec != info.codec ||
        m_currentVideoInfo.width != info.width ||
        m_currentVideoInfo.height != info.height ||
        m_currentVideoInfo.frameRate != info.frameRate) {
        
        m_currentVideoInfo = info;
        locker.unlock();
        
        emit videoInfoUpdated(m_currentVideoInfo);
        qCDebug(videoProcessor) << "Video info updated:" << info.codec 
                               << info.width << "x" << info.height 
                               << "@" << info.frameRate << "fps";
    } else {
        // Update frame counters without emitting signal
        m_currentVideoInfo.currentFrame = info.currentFrame;
        m_currentVideoInfo.totalFrames = info.totalFrames;
    }
}

QVector<VideoProcessor::ChapterInfo> VideoProcessor::getChapters() const
{
    QMutexLocker locker(&m_processingMutex);
    return m_chapters;
}

void VideoProcessor::setChapters(const QVector<ChapterInfo>& chapters)
{
    QMutexLocker locker(&m_processingMutex);
    
    m_chapters = chapters;
    locker.unlock();
    
    emit chaptersUpdated(m_chapters);
    qCDebug(videoProcessor) << "Chapters updated:" << chapters.size() << "chapters";
}

VideoProcessor::ChapterInfo VideoProcessor::getCurrentChapter(qint64 currentTime) const
{
    QMutexLocker locker(&m_processingMutex);
    
    for (const auto& chapter : m_chapters) {
        if (currentTime >= chapter.startTime && currentTime <= chapter.endTime) {
            return chapter;
        }
    }
    
    return ChapterInfo();
}

VideoProcessor::ChapterInfo VideoProcessor::getNextChapter(qint64 currentTime) const
{
    QMutexLocker locker(&m_processingMutex);
    
    for (const auto& chapter : m_chapters) {
        if (chapter.startTime > currentTime) {
            return chapter;
        }
    }
    
    return ChapterInfo();
}

VideoProcessor::ChapterInfo VideoProcessor::getPreviousChapter(qint64 currentTime) const
{
    QMutexLocker locker(&m_processingMutex);
    
    ChapterInfo previousChapter;
    
    for (const auto& chapter : m_chapters) {
        if (chapter.startTime < currentTime) {
            previousChapter = chapter;
        } else {
            break;
        }
    }
    
    return previousChapter;
}

bool VideoProcessor::isInfoOverlayEnabled() const
{
    return m_infoOverlayEnabled;
}

void VideoProcessor::setInfoOverlayEnabled(bool enabled)
{
    if (m_infoOverlayEnabled != enabled) {
        m_infoOverlayEnabled = enabled;
        qCDebug(videoProcessor) << "Info overlay" << (enabled ? "enabled" : "disabled");
    }
}

float VideoProcessor::getOverlayOpacity() const
{
    return m_overlayOpacity;
}

void VideoProcessor::setOverlayOpacity(float opacity)
{
    opacity = clampParameter(opacity, 0.0f, 1.0f);
    
    if (m_overlayOpacity != opacity) {
        m_overlayOpacity = opacity;
        qCDebug(videoProcessor) << "Overlay opacity set to:" << opacity;
    }
}

void VideoProcessor::resetFilters()
{
    QMutexLocker locker(&m_processingMutex);
    
    m_activeFilters.clear();
    m_parameters = ProcessingParameters();
    
    locker.unlock();
    
    emit parametersChanged(m_parameters);
    qCDebug(videoProcessor) << "Video filters reset";
}

bool VideoProcessor::loadFilterPreset(const QString& presetName)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString filePath = configPath + "/video_presets/" + presetName + ".json";
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(videoProcessor) << "Failed to load preset:" << presetName;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(videoProcessor) << "JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Load parameters
    QJsonObject params = root["parameters"].toObject();
    ProcessingParameters newParams;
    newParams.brightness = static_cast<float>(params["brightness"].toDouble());
    newParams.contrast = static_cast<float>(params["contrast"].toDouble());
    newParams.saturation = static_cast<float>(params["saturation"].toDouble());
    newParams.hue = static_cast<float>(params["hue"].toDouble());
    newParams.gamma = static_cast<float>(params["gamma"].toDouble());
    newParams.sepia = static_cast<float>(params["sepia"].toDouble());
    newParams.blur = static_cast<float>(params["blur"].toDouble());
    newParams.sharpen = static_cast<float>(params["sharpen"].toDouble());
    newParams.deinterlaceEnabled = params["deinterlaceEnabled"].toBool();
    newParams.deinterlaceMethod = params["deinterlaceMethod"].toString();
    
    setParameters(newParams);
    
    // Load active filters
    QJsonArray filtersArray = root["activeFilters"].toArray();
    QMutexLocker locker(&m_processingMutex);
    m_activeFilters.clear();
    
    for (const QJsonValue& value : filtersArray) {
        m_activeFilters.append(static_cast<FilterType>(value.toInt()));
    }
    
    qCDebug(videoProcessor) << "Loaded preset:" << presetName;
    return true;
}

bool VideoProcessor::saveFilterPreset(const QString& presetName)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString presetsDir = configPath + "/video_presets";
    QDir().mkpath(presetsDir);
    
    QString filePath = presetsDir + "/" + presetName + ".json";
    
    QJsonObject root;
    root["version"] = "1.0";
    root["name"] = presetName;
    
    // Save parameters
    QJsonObject params;
    params["brightness"] = static_cast<double>(m_parameters.brightness);
    params["contrast"] = static_cast<double>(m_parameters.contrast);
    params["saturation"] = static_cast<double>(m_parameters.saturation);
    params["hue"] = static_cast<double>(m_parameters.hue);
    params["gamma"] = static_cast<double>(m_parameters.gamma);
    params["sepia"] = static_cast<double>(m_parameters.sepia);
    params["blur"] = static_cast<double>(m_parameters.blur);
    params["sharpen"] = static_cast<double>(m_parameters.sharpen);
    params["deinterlaceEnabled"] = m_parameters.deinterlaceEnabled;
    params["deinterlaceMethod"] = m_parameters.deinterlaceMethod;
    root["parameters"] = params;
    
    // Save active filters
    QJsonArray filtersArray;
    QMutexLocker locker(&m_processingMutex);
    for (FilterType filter : m_activeFilters) {
        filtersArray.append(static_cast<int>(filter));
    }
    root["activeFilters"] = filtersArray;
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        qCDebug(videoProcessor) << "Saved preset:" << presetName;
        return true;
    }
    
    qCWarning(videoProcessor) << "Failed to save preset:" << presetName;
    return false;
}

QStringList VideoProcessor::getAvailablePresets() const
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString presetsDir = configPath + "/video_presets";
    
    QDir dir(presetsDir);
    QStringList presets;
    QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString& file : files) {
        presets.append(QFileInfo(file).baseName());
    }
    
    return presets;
}

void VideoProcessor::applyColorAdjustments(ProcessingParameters& params)
{
    // Color adjustments would be applied to video frames
    // This is a placeholder for the actual video processing implementation
    Q_UNUSED(params)
    
    qCDebug(videoProcessor) << "Applying color adjustments";
}

void VideoProcessor::applyFilterEffects(ProcessingParameters& params)
{
    // Filter effects would be applied to video frames
    // This is a placeholder for the actual video processing implementation
    Q_UNUSED(params)
    
    qCDebug(videoProcessor) << "Applying filter effects";
}

void VideoProcessor::applyDeinterlacing(ProcessingParameters& params)
{
    // Deinterlacing would be applied to video frames
    // This is a placeholder for the actual video processing implementation
    Q_UNUSED(params)
    
    qCDebug(videoProcessor) << "Applying deinterlacing";
}

float VideoProcessor::clampParameter(float value, float min, float max) const
{
    return qBound(min, value, max);
}
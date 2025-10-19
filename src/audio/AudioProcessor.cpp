#include "audio/AudioProcessor.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QLoggingCategory>
#include <QtMath>
#include <QDateTime>

Q_DECLARE_LOGGING_CATEGORY(audioProcessor)
Q_LOGGING_CATEGORY(audioProcessor, "audio.processor")

AudioProcessor::AudioProcessor(QObject* parent)
    : QObject(parent)
    , m_enabled(false)
    , m_currentTrackId(-1)
    , m_karaokeMode(Off)
    , m_vocalRemovalStrength(DEFAULT_VOCAL_REMOVAL_STRENGTH)
    , m_karaokeLowFreq(DEFAULT_KARAOKE_LOW_FREQ)
    , m_karaokeHighFreq(DEFAULT_KARAOKE_HIGH_FREQ)
    , m_currentLyricsIndex(-1)
    , m_lyricsTimer(new QTimer(this))
    , m_audioDelay(0)
    , m_noiseReductionEnabled(false)
    , m_noiseReductionStrength(DEFAULT_NOISE_REDUCTION_STRENGTH)
    , m_conversionInProgress(false)
    , m_conversionProgress(0)
    , m_conversionTimer(new QTimer(this))
{
    // Setup lyrics timer
    m_lyricsTimer->setSingleShot(false);
    m_lyricsTimer->setInterval(100); // Check every 100ms
    connect(m_lyricsTimer, &QTimer::timeout, this, &AudioProcessor::updateLyricsPosition);
    
    // Setup conversion timer
    m_conversionTimer->setSingleShot(false);
    m_conversionTimer->setInterval(500); // Update every 500ms
    connect(m_conversionTimer, &QTimer::timeout, this, &AudioProcessor::updateConversionProgress);
    
    qCDebug(audioProcessor) << "AudioProcessor created";
}

AudioProcessor::~AudioProcessor()
{
    shutdown();
    qCDebug(audioProcessor) << "AudioProcessor destroyed";
}

bool AudioProcessor::initialize()
{
    // Initialize with default audio track
    AudioTrack defaultTrack;
    defaultTrack.id = 0;
    defaultTrack.language = "Default";
    defaultTrack.codec = "Unknown";
    defaultTrack.channels = 2;
    defaultTrack.sampleRate = 44100;
    defaultTrack.bitrate = 0;
    defaultTrack.description = "Default Audio Track";
    defaultTrack.isDefault = true;
    
    m_audioTracks.clear();
    m_audioTracks.append(defaultTrack);
    m_currentTrackId = 0;
    
    qCDebug(audioProcessor) << "AudioProcessor initialized";
    return true;
}

void AudioProcessor::shutdown()
{
    setEnabled(false);
    clearLyrics();
    cancelConversion();
    qCDebug(audioProcessor) << "AudioProcessor shutdown";
}

bool AudioProcessor::isEnabled() const
{
    return m_enabled;
}

void AudioProcessor::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        if (m_enabled) {
            if (hasLyrics()) {
                m_lyricsTimer->start();
            }
        } else {
            m_lyricsTimer->stop();
        }
        
        emit enabledChanged(m_enabled);
        qCDebug(audioProcessor) << "Audio processor" << (enabled ? "enabled" : "disabled");
    }
}

// Audio Track Management
QVector<AudioProcessor::AudioTrack> AudioProcessor::getAvailableAudioTracks() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_audioTracks;
}

int AudioProcessor::getCurrentAudioTrack() const
{
    return m_currentTrackId;
}

bool AudioProcessor::switchAudioTrack(int trackId)
{
    QMutexLocker locker(&m_dataMutex);
    
    // Find track by ID
    for (const auto& track : m_audioTracks) {
        if (track.id == trackId) {
            m_currentTrackId = trackId;
            emit audioTrackChanged(trackId, track);
            qCDebug(audioProcessor) << "Switched to audio track" << trackId << track.description;
            return true;
        }
    }
    
    qCWarning(audioProcessor) << "Audio track not found:" << trackId;
    return false;
}

AudioProcessor::AudioTrack AudioProcessor::getAudioTrack(int trackId) const
{
    QMutexLocker locker(&m_dataMutex);
    
    for (const auto& track : m_audioTracks) {
        if (track.id == trackId) {
            return track;
        }
    }
    
    return AudioTrack(); // Return empty track if not found
}

// Karaoke Mode
AudioProcessor::KaraokeMode AudioProcessor::getKaraokeMode() const
{
    return m_karaokeMode;
}

void AudioProcessor::setKaraokeMode(KaraokeMode mode)
{
    if (m_karaokeMode != mode) {
        m_karaokeMode = mode;
        emit karaokeModeChanged(m_karaokeMode);
        qCDebug(audioProcessor) << "Karaoke mode set to" << getKaraokeModeName(mode);
    }
}

QString AudioProcessor::getKaraokeModeName(KaraokeMode mode)
{
    switch (mode) {
        case Off: return "Off";
        case VocalRemoval: return "Vocal Removal";
        case VocalIsolation: return "Vocal Isolation";
        case CenterChannelExtraction: return "Center Channel Extraction";
        default: return "Unknown";
    }
}

float AudioProcessor::getVocalRemovalStrength() const
{
    return m_vocalRemovalStrength;
}

void AudioProcessor::setVocalRemovalStrength(float strength)
{
    strength = qBound(0.0f, strength, 1.0f);
    
    if (m_vocalRemovalStrength != strength) {
        m_vocalRemovalStrength = strength;
        qCDebug(audioProcessor) << "Vocal removal strength set to" << strength;
    }
}

QPair<float, float> AudioProcessor::getKaraokeFrequencyRange() const
{
    return qMakePair(m_karaokeLowFreq, m_karaokeHighFreq);
}

void AudioProcessor::setKaraokeFrequencyRange(float lowFreq, float highFreq)
{
    lowFreq = qBound(20.0f, lowFreq, 20000.0f);
    highFreq = qBound(lowFreq, highFreq, 20000.0f);
    
    if (m_karaokeLowFreq != lowFreq || m_karaokeHighFreq != highFreq) {
        m_karaokeLowFreq = lowFreq;
        m_karaokeHighFreq = highFreq;
        qCDebug(audioProcessor) << "Karaoke frequency range set to" << lowFreq << "-" << highFreq << "Hz";
    }
}

// Lyrics Support
bool AudioProcessor::hasLyrics() const
{
    QMutexLocker locker(&m_dataMutex);
    return !m_lyricsLines.isEmpty();
}

bool AudioProcessor::loadLyricsFromFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    bool success = false;
    if (extension == "lrc") {
        success = parseLRCFile(filePath);
    } else if (extension == "srt") {
        success = parseSRTFile(filePath);
    } else {
        qCWarning(audioProcessor) << "Unsupported lyrics format:" << extension;
        return false;
    }
    
    if (success) {
        emit lyricsLoaded(m_lyricsLines.size());
        
        if (m_enabled) {
            m_lyricsTimer->start();
        }
        
        qCDebug(audioProcessor) << "Loaded" << m_lyricsLines.size() << "lyrics lines from" << filePath;
    }
    
    return success;
}

bool AudioProcessor::loadEmbeddedLyrics(const QString& mediaPath)
{
    // TODO: Implement embedded lyrics extraction using libVLC or TagLib
    Q_UNUSED(mediaPath)
    
    qCDebug(audioProcessor) << "Embedded lyrics loading not yet implemented";
    return false;
}

QVector<AudioProcessor::LyricsLine> AudioProcessor::getAllLyrics() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_lyricsLines;
}

AudioProcessor::LyricsLine AudioProcessor::getCurrentLyricsLine(qint64 timeMs) const
{
    QMutexLocker locker(&m_dataMutex);
    
    for (const auto& line : m_lyricsLines) {
        if (timeMs >= line.startTime && timeMs <= line.endTime) {
            return line;
        }
    }
    
    return LyricsLine(); // Return empty line if not found
}

AudioProcessor::LyricsLine AudioProcessor::getNextLyricsLine(qint64 timeMs) const
{
    QMutexLocker locker(&m_dataMutex);
    
    for (const auto& line : m_lyricsLines) {
        if (line.startTime > timeMs) {
            return line;
        }
    }
    
    return LyricsLine(); // Return empty line if not found
}

void AudioProcessor::clearLyrics()
{
    QMutexLocker locker(&m_dataMutex);
    
    m_lyricsLines.clear();
    m_currentLyricsIndex = -1;
    m_lyricsTimer->stop();
    
    qCDebug(audioProcessor) << "Lyrics cleared";
}

bool AudioProcessor::exportLyrics(const QString& filePath, const QString& format) const
{
    QMutexLocker locker(&m_dataMutex);
    
    if (m_lyricsLines.isEmpty()) {
        qCWarning(audioProcessor) << "No lyrics to export";
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(audioProcessor) << "Failed to open file for writing:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    
    if (format.toLower() == "lrc") {
        // Export as LRC format
        for (const auto& line : m_lyricsLines) {
            int minutes = static_cast<int>(line.startTime / 60000);
            int seconds = static_cast<int>((line.startTime % 60000) / 1000);
            int centiseconds = static_cast<int>((line.startTime % 1000) / 10);
            
            out << QString("[%1:%2.%3]%4\n")
                   .arg(minutes, 2, 10, QChar('0'))
                   .arg(seconds, 2, 10, QChar('0'))
                   .arg(centiseconds, 2, 10, QChar('0'))
                   .arg(line.text);
        }
    } else if (format.toLower() == "srt") {
        // Export as SRT format
        for (int i = 0; i < m_lyricsLines.size(); ++i) {
            const auto& line = m_lyricsLines[i];
            
            out << (i + 1) << "\n";
            out << QString("%1 --> %2\n")
                   .arg(QTime::fromMSecsSinceStartOfDay(line.startTime).toString("hh:mm:ss,zzz"))
                   .arg(QTime::fromMSecsSinceStartOfDay(line.endTime).toString("hh:mm:ss,zzz"));
            out << line.text << "\n\n";
        }
    } else if (format.toLower() == "txt") {
        // Export as plain text
        for (const auto& line : m_lyricsLines) {
            out << line.text << "\n";
        }
    } else {
        qCWarning(audioProcessor) << "Unsupported export format:" << format;
        return false;
    }
    
    qCDebug(audioProcessor) << "Exported lyrics to" << filePath << "in" << format << "format";
    return true;
}

// Audio Tag Editor
QMap<QString, QString> AudioProcessor::getAudioTags(const QString& filePath) const
{
    // TODO: Implement using TagLib or similar library
    Q_UNUSED(filePath)
    
    QMap<QString, QString> tags;
    tags["title"] = "Unknown Title";
    tags["artist"] = "Unknown Artist";
    tags["album"] = "Unknown Album";
    tags["year"] = "0000";
    tags["genre"] = "Unknown";
    tags["track"] = "0";
    
    qCDebug(audioProcessor) << "Audio tag reading not yet implemented";
    return tags;
}

bool AudioProcessor::setAudioTag(const QString& filePath, const QString& tagName, const QString& tagValue)
{
    // TODO: Implement using TagLib or similar library
    Q_UNUSED(filePath)
    Q_UNUSED(tagName)
    Q_UNUSED(tagValue)
    
    qCDebug(audioProcessor) << "Audio tag writing not yet implemented";
    return false;
}

QStringList AudioProcessor::getSupportedTagNames()
{
    return {"title", "artist", "album", "albumartist", "year", "genre", "track", "disc", "comment"};
}

QByteArray AudioProcessor::getAlbumArt(const QString& filePath) const
{
    // TODO: Implement using TagLib or similar library
    Q_UNUSED(filePath)
    
    qCDebug(audioProcessor) << "Album art reading not yet implemented";
    return QByteArray();
}

bool AudioProcessor::setAlbumArt(const QString& filePath, const QByteArray& imageData)
{
    // TODO: Implement using TagLib or similar library
    Q_UNUSED(filePath)
    Q_UNUSED(imageData)
    
    qCDebug(audioProcessor) << "Album art writing not yet implemented";
    return false;
}

// Audio Format Conversion
bool AudioProcessor::isConversionSupported(AudioFormat fromFormat, AudioFormat toFormat)
{
    // For demonstration, assume all conversions are supported
    Q_UNUSED(fromFormat)
    Q_UNUSED(toFormat)
    return true;
}

QString AudioProcessor::getFormatName(AudioFormat format)
{
    switch (format) {
        case MP3: return "MP3";
        case FLAC: return "FLAC";
        case WAV: return "WAV";
        case OGG: return "OGG Vorbis";
        case AAC: return "AAC";
        case WMA: return "WMA";
        default: return "Unknown";
    }
}

QString AudioProcessor::getFormatExtension(AudioFormat format)
{
    switch (format) {
        case MP3: return "mp3";
        case FLAC: return "flac";
        case WAV: return "wav";
        case OGG: return "ogg";
        case AAC: return "aac";
        case WMA: return "wma";
        default: return "unknown";
    }
}

bool AudioProcessor::convertAudioFormat(const QString& inputPath, const QString& outputPath, 
                                       AudioFormat targetFormat, int quality)
{
    if (m_conversionInProgress) {
        qCWarning(audioProcessor) << "Conversion already in progress";
        return false;
    }
    
    // TODO: Implement actual audio conversion using FFmpeg or similar
    Q_UNUSED(inputPath)
    Q_UNUSED(outputPath)
    Q_UNUSED(targetFormat)
    Q_UNUSED(quality)
    
    m_conversionInProgress = true;
    m_conversionProgress = 0;
    m_conversionTimer->start();
    
    qCDebug(audioProcessor) << "Started audio conversion (simulated)";
    return true;
}

int AudioProcessor::getConversionProgress() const
{
    return m_conversionProgress;
}

void AudioProcessor::cancelConversion()
{
    if (m_conversionInProgress) {
        m_conversionInProgress = false;
        m_conversionProgress = 0;
        m_conversionTimer->stop();
        
        qCDebug(audioProcessor) << "Audio conversion cancelled";
    }
}

// Audio Delay Adjustment
int AudioProcessor::getAudioDelay() const
{
    return m_audioDelay;
}

void AudioProcessor::setAudioDelay(int delayMs)
{
    delayMs = qBound(-1000, delayMs, 1000);
    
    if (m_audioDelay != delayMs) {
        m_audioDelay = delayMs;
        emit audioDelayChanged(m_audioDelay);
        qCDebug(audioProcessor) << "Audio delay set to" << delayMs << "ms";
    }
}

// Noise Reduction
bool AudioProcessor::isNoiseReductionEnabled() const
{
    return m_noiseReductionEnabled;
}

void AudioProcessor::setNoiseReductionEnabled(bool enabled)
{
    if (m_noiseReductionEnabled != enabled) {
        m_noiseReductionEnabled = enabled;
        emit noiseReductionChanged(m_noiseReductionEnabled, m_noiseReductionStrength);
        qCDebug(audioProcessor) << "Noise reduction" << (enabled ? "enabled" : "disabled");
    }
}

float AudioProcessor::getNoiseReductionStrength() const
{
    return m_noiseReductionStrength;
}

void AudioProcessor::setNoiseReductionStrength(float strength)
{
    strength = qBound(0.0f, strength, 1.0f);
    
    if (m_noiseReductionStrength != strength) {
        m_noiseReductionStrength = strength;
        emit noiseReductionChanged(m_noiseReductionEnabled, m_noiseReductionStrength);
        qCDebug(audioProcessor) << "Noise reduction strength set to" << strength;
    }
}

// Private slots
void AudioProcessor::updateLyricsPosition()
{
    // TODO: Get current playback position from media engine
    // For now, simulate with current time
    static qint64 simulatedTime = 0;
    simulatedTime += 100; // Increment by 100ms each update
    
    LyricsLine currentLine = getCurrentLyricsLine(simulatedTime);
    if (!currentLine.text.isEmpty()) {
        emit currentLyricsChanged(currentLine);
    }
}

void AudioProcessor::updateConversionProgress()
{
    if (!m_conversionInProgress) {
        return;
    }
    
    // Simulate conversion progress
    m_conversionProgress += 5;
    emit conversionProgress(m_conversionProgress);
    
    if (m_conversionProgress >= 100) {
        m_conversionInProgress = false;
        m_conversionTimer->stop();
        emit conversionCompleted(true, "output.mp3"); // Simulated output path
        
        qCDebug(audioProcessor) << "Audio conversion completed (simulated)";
    }
}

// Private methods
bool AudioProcessor::parseLRCFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(audioProcessor) << "Failed to open LRC file:" << filePath;
        return false;
    }
    
    QMutexLocker locker(&m_dataMutex);
    m_lyricsLines.clear();
    
    QTextStream in(&file);
    QRegularExpression timeRegex(R"(\[(\d{2}):(\d{2})\.(\d{2})\](.*)$)");
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QRegularExpressionMatch match = timeRegex.match(line);
        if (match.hasMatch()) {
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            int centiseconds = match.captured(3).toInt();
            QString text = match.captured(4).trimmed();
            
            qint64 timeMs = (minutes * 60 + seconds) * 1000 + centiseconds * 10;
            
            LyricsLine lyricsLine;
            lyricsLine.startTime = timeMs;
            lyricsLine.endTime = timeMs + 3000; // Default 3 second duration
            lyricsLine.text = text;
            
            m_lyricsLines.append(lyricsLine);
        }
    }
    
    // Adjust end times based on next line start times
    for (int i = 0; i < m_lyricsLines.size() - 1; ++i) {
        m_lyricsLines[i].endTime = m_lyricsLines[i + 1].startTime;
    }
    
    return !m_lyricsLines.isEmpty();
}

bool AudioProcessor::parseSRTFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(audioProcessor) << "Failed to open SRT file:" << filePath;
        return false;
    }
    
    QMutexLocker locker(&m_dataMutex);
    m_lyricsLines.clear();
    
    QTextStream in(&file);
    QRegularExpression timeRegex(R"((\d{2}):(\d{2}):(\d{2}),(\d{3}) --> (\d{2}):(\d{2}):(\d{2}),(\d{3}))");
    
    QString currentText;
    qint64 startTime = 0, endTime = 0;
    bool expectingTime = false;
    bool expectingText = false;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        if (line.isEmpty()) {
            if (!currentText.isEmpty()) {
                LyricsLine lyricsLine;
                lyricsLine.startTime = startTime;
                lyricsLine.endTime = endTime;
                lyricsLine.text = currentText;
                m_lyricsLines.append(lyricsLine);
                
                currentText.clear();
                expectingTime = false;
                expectingText = false;
            }
            continue;
        }
        
        // Check if it's a subtitle number
        bool isNumber;
        line.toInt(&isNumber);
        if (isNumber) {
            expectingTime = true;
            continue;
        }
        
        // Check if it's a time line
        if (expectingTime) {
            QRegularExpressionMatch match = timeRegex.match(line);
            if (match.hasMatch()) {
                // Parse start time
                int h1 = match.captured(1).toInt();
                int m1 = match.captured(2).toInt();
                int s1 = match.captured(3).toInt();
                int ms1 = match.captured(4).toInt();
                startTime = ((h1 * 3600 + m1 * 60 + s1) * 1000) + ms1;
                
                // Parse end time
                int h2 = match.captured(5).toInt();
                int m2 = match.captured(6).toInt();
                int s2 = match.captured(7).toInt();
                int ms2 = match.captured(8).toInt();
                endTime = ((h2 * 3600 + m2 * 60 + s2) * 1000) + ms2;
                
                expectingTime = false;
                expectingText = true;
                continue;
            }
        }
        
        // Collect text lines
        if (expectingText) {
            if (!currentText.isEmpty()) {
                currentText += " ";
            }
            currentText += line;
        }
    }
    
    // Handle last subtitle if file doesn't end with empty line
    if (!currentText.isEmpty()) {
        LyricsLine lyricsLine;
        lyricsLine.startTime = startTime;
        lyricsLine.endTime = endTime;
        lyricsLine.text = currentText;
        m_lyricsLines.append(lyricsLine);
    }
    
    return !m_lyricsLines.isEmpty();
}

void AudioProcessor::processKaraokeAudio(QVector<float>& leftChannel, QVector<float>& rightChannel)
{
    if (m_karaokeMode == Off || leftChannel.size() != rightChannel.size()) {
        return;
    }
    
    switch (m_karaokeMode) {
        case VocalRemoval:
            // Subtract right channel from left channel to remove center vocals
            for (int i = 0; i < leftChannel.size(); ++i) {
                float diff = (leftChannel[i] - rightChannel[i]) * m_vocalRemovalStrength;
                leftChannel[i] = diff;
                rightChannel[i] = diff;
            }
            break;
            
        case VocalIsolation:
            // Add channels to isolate center vocals
            for (int i = 0; i < leftChannel.size(); ++i) {
                float sum = (leftChannel[i] + rightChannel[i]) * 0.5f * m_vocalRemovalStrength;
                leftChannel[i] = sum;
                rightChannel[i] = sum;
            }
            break;
            
        case CenterChannelExtraction:
            // Extract center channel (mono mix)
            for (int i = 0; i < leftChannel.size(); ++i) {
                float center = (leftChannel[i] + rightChannel[i]) * 0.5f;
                leftChannel[i] = center;
                rightChannel[i] = center;
            }
            break;
            
        default:
            break;
    }
}

void AudioProcessor::applyNoiseReduction(QVector<float>& audioData)
{
    if (!m_noiseReductionEnabled || audioData.isEmpty()) {
        return;
    }
    
    // Simple noise gate implementation
    float threshold = 0.01f * (1.0f - m_noiseReductionStrength);
    
    for (float& sample : audioData) {
        if (qAbs(sample) < threshold) {
            sample *= (1.0f - m_noiseReductionStrength);
        }
    }
}

AudioProcessor::AudioFormat AudioProcessor::detectAudioFormat(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "mp3") return MP3;
    if (extension == "flac") return FLAC;
    if (extension == "wav") return WAV;
    if (extension == "ogg") return OGG;
    if (extension == "aac" || extension == "m4a") return AAC;
    if (extension == "wma") return WMA;
    
    return MP3; // Default fallback
}
#include "ai/WhisperSubtitleGenerator.h"
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QDebug>

Q_LOGGING_CATEGORY(whisperGenerator, "eonplay.ai.whisper")

namespace EonPlay {
namespace AI {

QString SubtitleGenerationResult::summary() const
{
    if (success) {
        return QString("Generated %1 segments in %2ms (Language: %3, Confidence: %4%)")
               .arg(segmentCount)
               .arg(processingTimeMs)
               .arg(detectedLanguage)
               .arg(qRound(confidence * 100));
    } else {
        return QString("Generation failed: %1").arg(errorMessage);
    }
}

WhisperSubtitleGenerator::WhisperSubtitleGenerator(QObject* parent)
    : IAISubtitleGenerator(parent)
    , m_processingMode(AutoDetect)
    , m_modelSize("base")
    , m_useGPU(true)
    , m_maxSegmentLength(30)
    , m_confidenceThreshold(0.5)
    , m_process(nullptr)
    , m_progressTimer(new QTimer(this))
    , m_tempDir(nullptr)
    , m_currentProgress(0.0)
    , m_isProcessing(false)
    , m_isCancelled(false)
{
    // Initialize supported formats
    m_supportedFormats = {
        "mp3", "wav", "flac", "aac", "ogg", "m4a", "wma",
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm"
    };

    // Initialize supported languages (ISO 639-1 codes)
    m_supportedLanguages = {
        "en", "es", "fr", "de", "it", "pt", "ru", "ja", "ko", "zh",
        "ar", "hi", "tr", "pl", "nl", "sv", "da", "no", "fi", "cs",
        "hu", "ro", "bg", "hr", "sk", "sl", "et", "lv", "lt", "mt",
        "ga", "cy", "eu", "ca", "gl", "is", "mk", "sq", "sr", "bs",
        "me", "hr", "sl", "sk", "cs", "hu", "ro", "bg", "el", "he",
        "fa", "ur", "bn", "ta", "te", "ml", "kn", "gu", "pa", "or",
        "as", "ne", "si", "my", "th", "lo", "km", "vi", "id", "ms",
        "tl", "jv", "su", "mg", "ny", "sn", "yo", "zu", "xh", "af",
        "sw", "rw", "lg", "ln", "wo", "ff", "ha", "ig", "am", "ti",
        "om", "so", "mg", "zu", "xh", "af", "sw"
    };

    // Setup progress timer
    m_progressTimer->setInterval(1000); // Update every second
    connect(m_progressTimer, &QTimer::timeout, this, &WhisperSubtitleGenerator::onProgressTimer);
}

WhisperSubtitleGenerator::~WhisperSubtitleGenerator()
{
    cancelGeneration();
    cleanupTemporaryFiles();
}

bool WhisperSubtitleGenerator::initialize()
{
    qCInfo(whisperGenerator) << "Initializing Whisper subtitle generator";

    switch (m_processingMode) {
        case LocalWhisper:
            return initializeLocalWhisper();
        case CloudAPI:
            return initializeCloudAPI();
        case AutoDetect:
            if (initializeLocalWhisper()) {
                m_processingMode = LocalWhisper;
                return true;
            } else if (initializeCloudAPI()) {
                m_processingMode = CloudAPI;
                return true;
            }
            return false;
    }

    return false;
}

bool WhisperSubtitleGenerator::isAvailable() const
{
    switch (m_processingMode) {
        case LocalWhisper:
            return !m_whisperPath.isEmpty() && QFileInfo::exists(m_whisperPath);
        case CloudAPI:
            return !m_apiKey.isEmpty();
        case AutoDetect:
            return false; // Should be resolved during initialization
    }
    return false;
}

QStringList WhisperSubtitleGenerator::getSupportedFormats() const
{
    return m_supportedFormats;
}

QStringList WhisperSubtitleGenerator::getSupportedLanguages() const
{
    return m_supportedLanguages;
}

bool WhisperSubtitleGenerator::generateSubtitles(const QString& mediaFilePath, 
                                                const QString& targetLanguage,
                                                SubtitleGenerationResult& result)
{
    QMutexLocker locker(&m_mutex);

    if (m_isProcessing) {
        result.success = false;
        result.errorMessage = "Generation already in progress";
        return false;
    }

    if (!isAvailable()) {
        result.success = false;
        result.errorMessage = "Whisper generator not available";
        return false;
    }

    QFileInfo mediaInfo(mediaFilePath);
    if (!mediaInfo.exists()) {
        result.success = false;
        result.errorMessage = "Media file does not exist";
        return false;
    }

    qCInfo(whisperGenerator) << "Starting subtitle generation for:" << mediaFilePath;

    m_isProcessing = true;
    m_isCancelled = false;
    m_currentProgress = 0.0;
    m_currentResult = SubtitleGenerationResult();

    QElapsedTimer timer;
    timer.start();

    // Extract audio from media file
    QString audioPath;
    if (!extractAudioFromMedia(mediaFilePath, audioPath)) {
        result.success = false;
        result.errorMessage = "Failed to extract audio from media file";
        m_isProcessing = false;
        return false;
    }

    bool success = false;
    switch (m_processingMode) {
        case LocalWhisper:
            success = processWithLocalWhisper(audioPath, targetLanguage);
            break;
        case CloudAPI:
            success = processWithCloudAPI(audioPath, targetLanguage);
            break;
        case AutoDetect:
            result.success = false;
            result.errorMessage = "Processing mode not resolved";
            m_isProcessing = false;
            return false;
    }

    m_currentResult.processingTimeMs = timer.elapsed();
    result = m_currentResult;
    m_isProcessing = false;

    if (success) {
        qCInfo(whisperGenerator) << "Subtitle generation completed successfully";
        emit generationCompleted(result);
    } else {
        qCWarning(whisperGenerator) << "Subtitle generation failed:" << result.errorMessage;
        emit generationFailed(result.errorMessage);
    }

    cleanupTemporaryFiles();
    return success;
}

bool WhisperSubtitleGenerator::generateSubtitlesFromAudio(const QByteArray& audioData,
                                                         int sampleRate,
                                                         int channels,
                                                         const QString& targetLanguage,
                                                         SubtitleGenerationResult& result)
{
    // Create temporary audio file
    if (!m_tempDir) {
        m_tempDir = new QTemporaryDir(this);
        if (!m_tempDir->isValid()) {
            result.success = false;
            result.errorMessage = "Failed to create temporary directory";
            return false;
        }
    }

    QString audioPath = m_tempDir->filePath("audio_input.wav");
    
    // Write audio data to temporary file (simplified - would need proper WAV header)
    QFile audioFile(audioPath);
    if (!audioFile.open(QIODevice::WriteOnly)) {
        result.success = false;
        result.errorMessage = "Failed to write temporary audio file";
        return false;
    }

    // Write simple WAV header (44 bytes)
    QByteArray wavHeader;
    wavHeader.append("RIFF");
    wavHeader.append(QByteArray(4, 0)); // File size (will be updated)
    wavHeader.append("WAVE");
    wavHeader.append("fmt ");
    wavHeader.append(QByteArray::fromRawData("\x10\x00\x00\x00", 4)); // Format chunk size
    wavHeader.append(QByteArray::fromRawData("\x01\x00", 2)); // PCM format
    wavHeader.append(reinterpret_cast<const char*>(&channels), 2);
    wavHeader.append(reinterpret_cast<const char*>(&sampleRate), 4);
    int byteRate = sampleRate * channels * 2; // 16-bit samples
    wavHeader.append(reinterpret_cast<const char*>(&byteRate), 4);
    int blockAlign = channels * 2;
    wavHeader.append(reinterpret_cast<const char*>(&blockAlign), 2);
    wavHeader.append(QByteArray::fromRawData("\x10\x00", 2)); // 16 bits per sample
    wavHeader.append("data");
    int dataSize = audioData.size();
    wavHeader.append(reinterpret_cast<const char*>(&dataSize), 4);

    audioFile.write(wavHeader);
    audioFile.write(audioData);
    audioFile.close();

    // Update file size in header
    int fileSize = wavHeader.size() + audioData.size() - 8;
    audioFile.open(QIODevice::ReadWrite);
    audioFile.seek(4);
    audioFile.write(reinterpret_cast<const char*>(&fileSize), 4);
    audioFile.close();

    // Process the temporary audio file
    return generateSubtitles(audioPath, targetLanguage, result);
}

void WhisperSubtitleGenerator::cancelGeneration()
{
    QMutexLocker locker(&m_mutex);

    if (m_isProcessing) {
        m_isCancelled = true;
        
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        
        m_progressTimer->stop();
        m_isProcessing = false;
        
        qCInfo(whisperGenerator) << "Subtitle generation cancelled";
    }
}

double WhisperSubtitleGenerator::getProgress() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentProgress;
}

QString WhisperSubtitleGenerator::getGeneratorName() const
{
    return "Whisper AI Subtitle Generator";
}

QString WhisperSubtitleGenerator::getVersion() const
{
    return "1.0.0";
}

void WhisperSubtitleGenerator::setProcessingMode(ProcessingMode mode)
{
    if (m_processingMode != mode) {
        m_processingMode = mode;
        qCInfo(whisperGenerator) << "Processing mode changed to:" << mode;
    }
}

void WhisperSubtitleGenerator::setWhisperExecutablePath(const QString& path)
{
    m_whisperPath = path;
}

void WhisperSubtitleGenerator::setModelSize(const QString& size)
{
    QStringList validSizes = {"tiny", "base", "small", "medium", "large"};
    if (validSizes.contains(size)) {
        m_modelSize = size;
    }
}

void WhisperSubtitleGenerator::setUseGPU(bool enabled)
{
    m_useGPU = enabled;
}

void WhisperSubtitleGenerator::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void WhisperSubtitleGenerator::setMaxSegmentLength(int seconds)
{
    if (seconds > 0 && seconds <= 300) { // Max 5 minutes per segment
        m_maxSegmentLength = seconds;
    }
}

void WhisperSubtitleGenerator::setConfidenceThreshold(double threshold)
{
    m_confidenceThreshold = qBound(0.0, threshold, 1.0);
}

bool WhisperSubtitleGenerator::initializeLocalWhisper()
{
    qCInfo(whisperGenerator) << "Initializing local Whisper";

    if (m_whisperPath.isEmpty()) {
        m_whisperPath = findWhisperExecutable();
    }

    if (m_whisperPath.isEmpty() || !checkWhisperInstallation()) {
        qCWarning(whisperGenerator) << "Local Whisper not found or not working";
        return false;
    }

    qCInfo(whisperGenerator) << "Local Whisper initialized at:" << m_whisperPath;
    return true;
}

bool WhisperSubtitleGenerator::initializeCloudAPI()
{
    qCInfo(whisperGenerator) << "Initializing cloud API";

    if (m_apiKey.isEmpty()) {
        qCWarning(whisperGenerator) << "No API key provided for cloud processing";
        return false;
    }

    // TODO: Test API connectivity
    qCInfo(whisperGenerator) << "Cloud API initialized";
    return true;
}

bool WhisperSubtitleGenerator::checkWhisperInstallation()
{
    if (m_whisperPath.isEmpty()) {
        return false;
    }

    QProcess testProcess;
    testProcess.start(m_whisperPath, QStringList() << "--help");
    testProcess.waitForFinished(5000);

    return testProcess.exitCode() == 0;
}

QString WhisperSubtitleGenerator::findWhisperExecutable()
{
    QStringList possiblePaths = {
        "whisper",
        "whisper.exe",
        QStandardPaths::findExecutable("whisper"),
        QStandardPaths::findExecutable("whisper.exe")
    };

    // Add common installation paths
    QStringList commonPaths = {
        "/usr/local/bin/whisper",
        "/usr/bin/whisper",
        QDir::homePath() + "/.local/bin/whisper",
        "C:/Python*/Scripts/whisper.exe",
        "C:/Program Files/Python*/Scripts/whisper.exe"
    };

    possiblePaths.append(commonPaths);

    for (const QString& path : possiblePaths) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return path;
        }
    }

    return QString();
}

bool WhisperSubtitleGenerator::extractAudioFromMedia(const QString& mediaPath, QString& audioPath)
{
    if (!m_tempDir) {
        m_tempDir = new QTemporaryDir(this);
        if (!m_tempDir->isValid()) {
            return false;
        }
    }

    audioPath = m_tempDir->filePath("extracted_audio.wav");

    // Use ffmpeg to extract audio (simplified - would need proper ffmpeg integration)
    QProcess ffmpegProcess;
    QStringList args;
    args << "-i" << mediaPath
         << "-vn" // No video
         << "-acodec" << "pcm_s16le" // PCM 16-bit
         << "-ar" << "16000" // 16kHz sample rate (good for speech recognition)
         << "-ac" << "1" // Mono
         << "-y" // Overwrite output file
         << audioPath;

    ffmpegProcess.start("ffmpeg", args);
    ffmpegProcess.waitForFinished(30000); // 30 second timeout

    if (ffmpegProcess.exitCode() != 0) {
        qCWarning(whisperGenerator) << "Failed to extract audio:" << ffmpegProcess.readAllStandardError();
        return false;
    }

    return QFileInfo::exists(audioPath);
}

bool WhisperSubtitleGenerator::processWithLocalWhisper(const QString& audioPath, const QString& language)
{
    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &WhisperSubtitleGenerator::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred,
                this, &WhisperSubtitleGenerator::onProcessError);
        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &WhisperSubtitleGenerator::onProcessOutput);
    }

    m_currentOutputFile = m_tempDir->filePath("whisper_output.srt");

    QStringList args;
    args << audioPath
         << "--model" << m_modelSize
         << "--output_format" << "srt"
         << "--output_dir" << m_tempDir->path()
         << "--verbose" << "True";

    if (!language.isEmpty() && m_supportedLanguages.contains(language)) {
        args << "--language" << language;
    }

    if (m_useGPU) {
        args << "--device" << "cuda";
    }

    qCInfo(whisperGenerator) << "Starting Whisper process with args:" << args;

    m_process->start(m_whisperPath, args);
    m_progressTimer->start();

    if (!m_process->waitForStarted(5000)) {
        m_currentResult.success = false;
        m_currentResult.errorMessage = "Failed to start Whisper process";
        return false;
    }

    // Wait for process to complete (with timeout)
    if (!m_process->waitForFinished(300000)) { // 5 minute timeout
        m_process->kill();
        m_currentResult.success = false;
        m_currentResult.errorMessage = "Whisper process timed out";
        return false;
    }

    return m_process->exitCode() == 0;
}

bool WhisperSubtitleGenerator::processWithCloudAPI(const QString& audioPath, const QString& language)
{
    // TODO: Implement cloud API processing
    // This would involve uploading the audio file to OpenAI's Whisper API
    // and processing the response
    
    m_currentResult.success = false;
    m_currentResult.errorMessage = "Cloud API processing not yet implemented";
    return false;
}

void WhisperSubtitleGenerator::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_progressTimer->stop();

    if (m_isCancelled) {
        return;
    }

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        // Read the generated SRT file
        QString srtPath = m_tempDir->filePath("extracted_audio.srt");
        QFile srtFile(srtPath);
        
        if (srtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(srtFile.readAll());
            m_currentResult = parseWhisperOutput(content);
            m_currentResult.success = true;
        } else {
            m_currentResult.success = false;
            m_currentResult.errorMessage = "Failed to read generated subtitle file";
        }
    } else {
        QString errorOutput = m_process->readAllStandardError();
        m_currentResult.success = false;
        m_currentResult.errorMessage = QString("Whisper process failed: %1").arg(errorOutput);
    }

    m_currentProgress = 1.0;
    emit progressChanged(m_currentProgress, m_currentResult.success ? "Completed" : "Failed");
}

void WhisperSubtitleGenerator::onProcessError(QProcess::ProcessError error)
{
    m_progressTimer->stop();
    
    QString errorString;
    switch (error) {
        case QProcess::FailedToStart:
            errorString = "Failed to start Whisper process";
            break;
        case QProcess::Crashed:
            errorString = "Whisper process crashed";
            break;
        case QProcess::Timedout:
            errorString = "Whisper process timed out";
            break;
        default:
            errorString = "Unknown process error";
            break;
    }

    m_currentResult.success = false;
    m_currentResult.errorMessage = errorString;
    
    emit generationFailed(errorString);
}

void WhisperSubtitleGenerator::onProcessOutput()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardOutput();
        QString output = QString::fromUtf8(data);
        
        // Parse progress from Whisper output (simplified)
        QRegularExpression progressRegex(R"(\[(\d+)%\])");
        QRegularExpressionMatch match = progressRegex.match(output);
        if (match.hasMatch()) {
            double progress = match.captured(1).toDouble() / 100.0;
            m_currentProgress = progress;
            emit progressChanged(progress, "Processing audio...");
        }
    }
}

void WhisperSubtitleGenerator::onProgressTimer()
{
    // Estimate progress based on time elapsed (fallback if no progress info available)
    if (m_currentProgress < 0.9) {
        m_currentProgress += 0.1;
        emit progressChanged(m_currentProgress, "Processing...");
    }
}

SubtitleGenerationResult WhisperSubtitleGenerator::parseWhisperOutput(const QString& output)
{
    SubtitleGenerationResult result;
    result.generatedContent = output;
    
    // Parse SRT content to extract segments
    QList<SubtitleSegment> segments = parseSegments(output);
    result.segmentCount = segments.size();
    
    if (!segments.isEmpty()) {
        result.confidence = calculateOverallConfidence(segments);
        result.detectedLanguage = detectLanguageFromOutput(output);
        m_currentSegments = segments;
        
        // Emit segment processed signals
        for (const SubtitleSegment& segment : segments) {
            emit segmentProcessed(segment);
        }
    }
    
    return result;
}

QList<SubtitleSegment> WhisperSubtitleGenerator::parseSegments(const QString& output)
{
    QList<SubtitleSegment> segments;
    
    // Parse SRT format
    QRegularExpression srtRegex(
        R"((\d+)\s*\n(\d{2}):(\d{2}):(\d{2}),(\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2}),(\d{3})\s*\n(.*?)(?=\n\d+\s*\n|\n*$))",
        QRegularExpression::DotMatchesEverythingOption
    );
    
    QRegularExpressionMatchIterator iterator = srtRegex.globalMatch(output);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        
        SubtitleSegment segment;
        
        // Parse start time
        int startHours = match.captured(2).toInt();
        int startMinutes = match.captured(3).toInt();
        int startSeconds = match.captured(4).toInt();
        int startMs = match.captured(5).toInt();
        segment.startTime = (startHours * 3600 + startMinutes * 60 + startSeconds) * 1000 + startMs;
        
        // Parse end time
        int endHours = match.captured(6).toInt();
        int endMinutes = match.captured(7).toInt();
        int endSeconds = match.captured(8).toInt();
        int endMs = match.captured(9).toInt();
        segment.endTime = (endHours * 3600 + endMinutes * 60 + endSeconds) * 1000 + endMs;
        
        // Parse text
        segment.text = match.captured(10).trimmed();
        segment.confidence = 0.8; // Default confidence for Whisper
        
        if (segment.isValid()) {
            segments.append(segment);
        }
    }
    
    return segments;
}

QString WhisperSubtitleGenerator::detectLanguageFromOutput(const QString& output)
{
    // Simple language detection based on common patterns
    // In a real implementation, this would be more sophisticated
    
    if (output.contains(QRegularExpression(R"([a-zA-Z\s]+)"))) {
        return "en"; // Default to English
    }
    
    return "unknown";
}

double WhisperSubtitleGenerator::calculateOverallConfidence(const QList<SubtitleSegment>& segments)
{
    if (segments.isEmpty()) {
        return 0.0;
    }
    
    double totalConfidence = 0.0;
    for (const SubtitleSegment& segment : segments) {
        totalConfidence += segment.confidence;
    }
    
    return totalConfidence / segments.size();
}

void WhisperSubtitleGenerator::cleanupTemporaryFiles()
{
    if (m_tempDir) {
        delete m_tempDir;
        m_tempDir = nullptr;
    }
}

} // namespace AI
} // namespace EonPlay
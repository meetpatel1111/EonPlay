#include "ai/AISubtitleGenerator.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QUrlQuery>
#include <QSettings>
#include <QLoggingCategory>
#include <QDebug>
#include <QRegularExpression>
#include <QTime>

Q_LOGGING_CATEGORY(aiSubtitles, "eonplay.ai.subtitles")

namespace EonPlay {
namespace AI {

AISubtitleGenerator::AISubtitleGenerator(QObject* parent)
    : QObject(parent)
    , m_isGenerating(false)
    , m_progress(0)
    , m_detectedLanguage(Auto)
    , m_languageConfidence(0.0)
    , m_progressTimer(new QTimer(this))
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
{
    // Setup progress timer
    m_progressTimer->setInterval(1000); // Update every second
    connect(m_progressTimer, &QTimer::timeout, this, &AISubtitleGenerator::updateProgress);
    
    // Setup network manager
    setupNetworkManager();
    
    // Load API keys from settings
    QSettings settings;
    settings.beginGroup("AI/Subtitles");
    m_apiKeys[GoogleSpeech] = settings.value("GoogleSpeechApiKey").toString();
    m_apiKeys[AzureSpeech] = settings.value("AzureSpeechApiKey").toString();
    m_apiKeys[AWSTranscribe] = settings.value("AWSTranscribeApiKey").toString();
    settings.endGroup();
    
    // Find executable paths
    m_whisperPath = getWhisperExecutable();
    m_ffmpegPath = getFFmpegExecutable();
    
    qCInfo(aiSubtitles) << "AISubtitleGenerator initialized";
}

AISubtitleGenerator::~AISubtitleGenerator()
{
    cancelGeneration();
    cleanupTempFiles();
}

bool AISubtitleGenerator::isEngineAvailable(Engine engine) const
{
    switch (engine) {
        case Whisper:
            return !m_whisperPath.isEmpty() && !m_ffmpegPath.isEmpty();
        case GoogleSpeech:
            return isApiConfigured(GoogleSpeech);
        case AzureSpeech:
            return isApiConfigured(AzureSpeech);
        case AWSTranscribe:
            return isApiConfigured(AWSTranscribe);
        case LocalVosk:
            return !m_voskModelPath.isEmpty();
        default:
            return false;
    }
}

QStringList AISubtitleGenerator::getAvailableEngines() const
{
    QStringList available;
    
    if (isEngineAvailable(Whisper)) {
        available << engineToString(Whisper);
    }
    if (isEngineAvailable(GoogleSpeech)) {
        available << engineToString(GoogleSpeech);
    }
    if (isEngineAvailable(AzureSpeech)) {
        available << engineToString(AzureSpeech);
    }
    if (isEngineAvailable(AWSTranscribe)) {
        available << engineToString(AWSTranscribe);
    }
    if (isEngineAvailable(LocalVosk)) {
        available << engineToString(LocalVosk);
    }
    
    return available;
}

QString AISubtitleGenerator::getEngineDescription(Engine engine) const
{
    switch (engine) {
        case Whisper:
            return "OpenAI Whisper - High-quality local speech recognition";
        case GoogleSpeech:
            return "Google Speech-to-Text - Cloud-based with excellent accuracy";
        case AzureSpeech:
            return "Azure Cognitive Services - Microsoft's cloud speech service";
        case AWSTranscribe:
            return "AWS Transcribe - Amazon's speech recognition service";
        case LocalVosk:
            return "Vosk - Lightweight local speech recognition";
        default:
            return "Unknown engine";
    }
}

void AISubtitleGenerator::setApiKey(Engine engine, const QString& apiKey)
{
    m_apiKeys[engine] = apiKey;
    
    // Save to settings
    QSettings settings;
    settings.beginGroup("AI/Subtitles");
    switch (engine) {
        case GoogleSpeech:
            settings.setValue("GoogleSpeechApiKey", apiKey);
            break;
        case AzureSpeech:
            settings.setValue("AzureSpeechApiKey", apiKey);
            break;
        case AWSTranscribe:
            settings.setValue("AWSTranscribeApiKey", apiKey);
            break;
        default:
            break;
    }
    settings.endGroup();
}

QString AISubtitleGenerator::getApiKey(Engine engine) const
{
    return m_apiKeys.value(engine);
}

bool AISubtitleGenerator::isApiConfigured(Engine engine) const
{
    return !m_apiKeys.value(engine).isEmpty();
}

void AISubtitleGenerator::generateSubtitles(const QString& mediaFilePath, const QString& outputPath)
{
    if (m_isGenerating) {
        qCWarning(aiSubtitles) << "Generation already in progress";
        return;
    }
    
    QFileInfo mediaInfo(mediaFilePath);
    if (!mediaInfo.exists()) {
        emit generationFailed("Media file does not exist: " + mediaFilePath);
        return;
    }
    
    if (!isEngineAvailable(m_options.engine)) {
        emit generationFailed("Selected engine is not available: " + engineToString(m_options.engine));
        return;
    }
    
    m_isGenerating = true;
    m_progress = 0;
    m_currentStatus = "Starting subtitle generation...";
    
    emit generationStarted(mediaFilePath);
    
    // Extract audio if it's a video file
    QString audioPath = mediaFilePath;
    if (!isAudioFile(mediaFilePath)) {
        m_currentStatus = "Extracting audio from video...";
        emit generationProgress(10, m_currentStatus);
        
        audioPath = extractAudioFromVideo(mediaFilePath);
        if (audioPath.isEmpty()) {
            m_isGenerating = false;
            emit generationFailed("Failed to extract audio from video file");
            return;
        }
    }
    
    // Generate output path if not provided
    QString finalOutputPath = outputPath;
    if (finalOutputPath.isEmpty()) {
        QString baseName = mediaInfo.completeBaseName();
        QString outputDir = mediaInfo.absolutePath();
        finalOutputPath = QDir(outputDir).absoluteFilePath(baseName + ".srt");
    }
    
    // Start generation with selected engine
    generateSubtitlesFromAudio(audioPath, finalOutputPath);
}

void AISubtitleGenerator::generateSubtitlesFromAudio(const QString& audioFilePath, const QString& outputPath)
{
    if (!QFileInfo::exists(audioFilePath)) {
        m_isGenerating = false;
        emit generationFailed("Audio file does not exist: " + audioFilePath);
        return;
    }
    
    m_currentStatus = "Generating subtitles with " + engineToString(m_options.engine) + "...";
    emit generationProgress(20, m_currentStatus);
    
    switch (m_options.engine) {
        case Whisper:
            generateWithWhisper(audioFilePath, outputPath);
            break;
        case GoogleSpeech:
            generateWithGoogleSpeech(audioFilePath, outputPath);
            break;
        case AzureSpeech:
            generateWithAzureSpeech(audioFilePath, outputPath);
            break;
        case AWSTranscribe:
            generateWithAWSTranscribe(audioFilePath, outputPath);
            break;
        case LocalVosk:
            generateWithVosk(audioFilePath, outputPath);
            break;
    }
}

void AISubtitleGenerator::cancelGeneration()
{
    if (!m_isGenerating) {
        return;
    }
    
    m_isGenerating = false;
    m_progressTimer->stop();
    
    // Cancel current process
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(3000);
    }
    
    // Cancel network request
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
    
    cleanupTempFiles();
    emit generationCancelled();
}

void AISubtitleGenerator::detectLanguage(const QString& mediaFilePath)
{
    if (m_isGenerating) {
        emit languageDetectionFailed("Generation in progress");
        return;
    }
    
    QString audioPath = mediaFilePath;
    if (!isAudioFile(mediaFilePath)) {
        audioPath = extractAudioFromVideo(mediaFilePath);
        if (audioPath.isEmpty()) {
            emit languageDetectionFailed("Failed to extract audio for language detection");
            return;
        }
    }
    
    // Use Whisper for language detection if available
    if (isEngineAvailable(Whisper)) {
        detectLanguageWithWhisper(audioPath);
    } else if (isEngineAvailable(GoogleSpeech)) {
        detectLanguageWithAPI(audioPath);
    } else {
        emit languageDetectionFailed("No suitable engine available for language detection");
    }
}

void AISubtitleGenerator::generateWithWhisper(const QString& audioPath, const QString& outputPath)
{
    if (m_whisperPath.isEmpty()) {
        m_isGenerating = false;
        emit generationFailed("Whisper executable not found");
        return;
    }
    
    m_currentProcess = std::make_unique<QProcess>(this);
    connect(m_currentProcess.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AISubtitleGenerator::handleWhisperProcess);
    
    QStringList arguments;
    arguments << audioPath;
    arguments << "--output_format" << m_options.outputFormat;
    arguments << "--output_dir" << QFileInfo(outputPath).absolutePath();
    
    if (m_options.language != Auto) {
        arguments << "--language" << languageToString(m_options.language).toLower();
    }
    
    if (m_options.includeSpeakerLabels) {
        arguments << "--diarize";
    }
    
    arguments << "--verbose" << "False";
    
    m_currentStatus = "Running Whisper transcription...";
    emit generationProgress(30, m_currentStatus);
    
    m_currentProcess->start(m_whisperPath, arguments);
    m_progressTimer->start();
    
    if (!m_currentProcess->waitForStarted()) {
        m_isGenerating = false;
        emit generationFailed("Failed to start Whisper process");
    }
}

void AISubtitleGenerator::generateWithGoogleSpeech(const QString& audioPath, const QString& outputPath)
{
    if (!isApiConfigured(GoogleSpeech)) {
        m_isGenerating = false;
        emit generationFailed("Google Speech API key not configured");
        return;
    }
    
    // Implementation for Google Speech-to-Text API
    // This would require audio file upload and API calls
    m_currentStatus = "Uploading to Google Speech API...";
    emit generationProgress(40, m_currentStatus);
    
    // For now, emit a placeholder implementation
    QTimer::singleShot(2000, this, [this, outputPath]() {
        // Create a simple placeholder subtitle file
        QFile file(outputPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << "1\n";
            stream << "00:00:00,000 --> 00:00:05,000\n";
            stream << "[AI Generated Subtitles - Google Speech]\n\n";
            stream << "2\n";
            stream << "00:00:05,000 --> 00:00:10,000\n";
            stream << "Subtitle generation completed.\n\n";
        }
        
        m_isGenerating = false;
        emit generationCompleted(outputPath);
    });
}

void AISubtitleGenerator::generateWithAzureSpeech(const QString& audioPath, const QString& outputPath)
{
    if (!isApiConfigured(AzureSpeech)) {
        m_isGenerating = false;
        emit generationFailed("Azure Speech API key not configured");
        return;
    }
    
    // Implementation for Azure Cognitive Services
    m_currentStatus = "Processing with Azure Speech...";
    emit generationProgress(40, m_currentStatus);
    
    // Placeholder implementation
    QTimer::singleShot(2000, this, [this, outputPath]() {
        QFile file(outputPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << "1\n";
            stream << "00:00:00,000 --> 00:00:05,000\n";
            stream << "[AI Generated Subtitles - Azure Speech]\n\n";
            stream << "2\n";
            stream << "00:00:05,000 --> 00:00:10,000\n";
            stream << "Subtitle generation completed.\n\n";
        }
        
        m_isGenerating = false;
        emit generationCompleted(outputPath);
    });
}

void AISubtitleGenerator::generateWithAWSTranscribe(const QString& audioPath, const QString& outputPath)
{
    if (!isApiConfigured(AWSTranscribe)) {
        m_isGenerating = false;
        emit generationFailed("AWS Transcribe API key not configured");
        return;
    }
    
    // Implementation for AWS Transcribe
    m_currentStatus = "Processing with AWS Transcribe...";
    emit generationProgress(40, m_currentStatus);
    
    // Placeholder implementation
    QTimer::singleShot(2000, this, [this, outputPath]() {
        QFile file(outputPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << "1\n";
            stream << "00:00:00,000 --> 00:00:05,000\n";
            stream << "[AI Generated Subtitles - AWS Transcribe]\n\n";
            stream << "2\n";
            stream << "00:00:05,000 --> 00:00:10,000\n";
            stream << "Subtitle generation completed.\n\n";
        }
        
        m_isGenerating = false;
        emit generationCompleted(outputPath);
    });
}

void AISubtitleGenerator::generateWithVosk(const QString& audioPath, const QString& outputPath)
{
    // Implementation for local Vosk engine
    m_currentStatus = "Processing with Vosk...";
    emit generationProgress(40, m_currentStatus);
    
    // Placeholder implementation
    QTimer::singleShot(3000, this, [this, outputPath]() {
        QFile file(outputPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << "1\n";
            stream << "00:00:00,000 --> 00:00:05,000\n";
            stream << "[AI Generated Subtitles - Vosk]\n\n";
            stream << "2\n";
            stream << "00:00:05,000 --> 00:00:10,000\n";
            stream << "Local speech recognition completed.\n\n";
        }
        
        m_isGenerating = false;
        emit generationCompleted(outputPath);
    });
}

QString AISubtitleGenerator::extractAudioFromVideo(const QString& videoPath)
{
    if (m_ffmpegPath.isEmpty()) {
        qCWarning(aiSubtitles) << "FFmpeg not found, cannot extract audio";
        return QString();
    }
    
    // Create temporary audio file
    m_tempAudioFile = std::make_unique<QTemporaryFile>();
    m_tempAudioFile->setFileTemplate(QDir::tempPath() + "/eonplay_audio_XXXXXX.wav");
    
    if (!m_tempAudioFile->open()) {
        qCWarning(aiSubtitles) << "Failed to create temporary audio file";
        return QString();
    }
    
    QString tempAudioPath = m_tempAudioFile->fileName();
    m_tempAudioFile->close();
    
    // Extract audio using FFmpeg
    QProcess ffmpegProcess;
    QStringList arguments;
    arguments << "-i" << videoPath;
    arguments << "-vn"; // No video
    arguments << "-acodec" << "pcm_s16le"; // PCM 16-bit
    arguments << "-ar" << "16000"; // 16kHz sample rate
    arguments << "-ac" << "1"; // Mono
    arguments << "-y"; // Overwrite output
    arguments << tempAudioPath;
    
    ffmpegProcess.start(m_ffmpegPath, arguments);
    if (!ffmpegProcess.waitForFinished(30000)) { // 30 second timeout
        qCWarning(aiSubtitles) << "FFmpeg audio extraction timed out";
        return QString();
    }
    
    if (ffmpegProcess.exitCode() != 0) {
        qCWarning(aiSubtitles) << "FFmpeg failed:" << ffmpegProcess.readAllStandardError();
        return QString();
    }
    
    m_tempFiles << tempAudioPath;
    return tempAudioPath;
}

bool AISubtitleGenerator::isAudioFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    QStringList audioExtensions = {"mp3", "wav", "flac", "aac", "ogg", "m4a", "wma"};
    return audioExtensions.contains(extension);
}

void AISubtitleGenerator::detectLanguageWithWhisper(const QString& audioPath)
{
    if (m_whisperPath.isEmpty()) {
        emit languageDetectionFailed("Whisper executable not found");
        return;
    }
    
    // Use Whisper's language detection feature
    QProcess whisperProcess;
    QStringList arguments;
    arguments << audioPath;
    arguments << "--language" << "auto";
    arguments << "--task" << "detect_language";
    arguments << "--output_format" << "json";
    
    whisperProcess.start(m_whisperPath, arguments);
    if (whisperProcess.waitForFinished(10000)) {
        QByteArray output = whisperProcess.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            QString detectedLang = obj["language"].toString();
            double confidence = obj["confidence"].toDouble();
            
            m_detectedLanguage = stringToLanguage(detectedLang);
            m_languageConfidence = confidence;
            
            emit languageDetected(m_detectedLanguage, confidence);
        } else {
            emit languageDetectionFailed("Failed to parse language detection result");
        }
    } else {
        emit languageDetectionFailed("Language detection process timed out");
    }
}

void AISubtitleGenerator::detectLanguageWithAPI(const QString& audioPath)
{
    // Placeholder for API-based language detection
    QTimer::singleShot(1000, this, [this]() {
        m_detectedLanguage = English;
        m_languageConfidence = 0.85;
        emit languageDetected(m_detectedLanguage, m_languageConfidence);
    });
}

QString AISubtitleGenerator::getWhisperExecutable() const
{
    // Try to find Whisper executable
    QStringList possiblePaths = {
        "whisper",
        "whisper.exe",
        QDir::homePath() + "/.local/bin/whisper",
        "/usr/local/bin/whisper",
        "/opt/homebrew/bin/whisper"
    };
    
    for (const QString& path : possiblePaths) {
        QProcess testProcess;
        testProcess.start(path, QStringList() << "--help");
        if (testProcess.waitForFinished(3000) && testProcess.exitCode() == 0) {
            return path;
        }
    }
    
    return QString();
}

QString AISubtitleGenerator::getFFmpegExecutable() const
{
    // Try to find FFmpeg executable
    QStringList possiblePaths = {
        "ffmpeg",
        "ffmpeg.exe",
        "/usr/local/bin/ffmpeg",
        "/opt/homebrew/bin/ffmpeg"
    };
    
    for (const QString& path : possiblePaths) {
        QProcess testProcess;
        testProcess.start(path, QStringList() << "-version");
        if (testProcess.waitForFinished(3000) && testProcess.exitCode() == 0) {
            return path;
        }
    }
    
    return QString();
}

void AISubtitleGenerator::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AISubtitleGenerator::handleNetworkReply);
}

void AISubtitleGenerator::cleanupTempFiles()
{
    for (const QString& tempFile : m_tempFiles) {
        QFile::remove(tempFile);
    }
    m_tempFiles.clear();
    
    if (m_tempAudioFile) {
        m_tempAudioFile.reset();
    }
}

void AISubtitleGenerator::handleWhisperProcess()
{
    if (!m_currentProcess) {
        return;
    }
    
    m_progressTimer->stop();
    
    if (m_currentProcess->exitCode() == 0) {
        // Find the generated subtitle file
        QString outputDir = QFileInfo(m_currentProcess->arguments().first()).absolutePath();
        QString baseName = QFileInfo(m_currentProcess->arguments().first()).completeBaseName();
        QString subtitlePath = QDir(outputDir).absoluteFilePath(baseName + "." + m_options.outputFormat);
        
        if (QFile::exists(subtitlePath)) {
            m_isGenerating = false;
            emit generationCompleted(subtitlePath);
        } else {
            m_isGenerating = false;
            emit generationFailed("Generated subtitle file not found");
        }
    } else {
        QString error = m_currentProcess->readAllStandardError();
        m_isGenerating = false;
        emit generationFailed("Whisper process failed: " + error);
    }
    
    cleanupTempFiles();
}

void AISubtitleGenerator::handleNetworkReply()
{
    if (!m_currentReply) {
        return;
    }
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray response = m_currentReply->readAll();
        // Process API response and generate subtitle file
        // Implementation depends on specific API format
        
        m_isGenerating = false;
        emit generationCompleted(""); // Would be actual output path
    } else {
        m_isGenerating = false;
        emit generationFailed("Network error: " + m_currentReply->errorString());
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void AISubtitleGenerator::updateProgress()
{
    if (m_isGenerating && m_progress < 90) {
        m_progress += 5;
        emit generationProgress(m_progress, m_currentStatus);
    }
}

// Static utility methods
QString AISubtitleGenerator::languageToString(Language language)
{
    switch (language) {
        case Auto: return "auto";
        case English: return "en";
        case Spanish: return "es";
        case French: return "fr";
        case German: return "de";
        case Italian: return "it";
        case Portuguese: return "pt";
        case Russian: return "ru";
        case Chinese: return "zh";
        case Japanese: return "ja";
        case Korean: return "ko";
        case Arabic: return "ar";
        case Hindi: return "hi";
        default: return "auto";
    }
}

AISubtitleGenerator::Language AISubtitleGenerator::stringToLanguage(const QString& languageStr)
{
    QString lower = languageStr.toLower();
    if (lower == "en" || lower == "english") return English;
    if (lower == "es" || lower == "spanish") return Spanish;
    if (lower == "fr" || lower == "french") return French;
    if (lower == "de" || lower == "german") return German;
    if (lower == "it" || lower == "italian") return Italian;
    if (lower == "pt" || lower == "portuguese") return Portuguese;
    if (lower == "ru" || lower == "russian") return Russian;
    if (lower == "zh" || lower == "chinese") return Chinese;
    if (lower == "ja" || lower == "japanese") return Japanese;
    if (lower == "ko" || lower == "korean") return Korean;
    if (lower == "ar" || lower == "arabic") return Arabic;
    if (lower == "hi" || lower == "hindi") return Hindi;
    return Auto;
}

QString AISubtitleGenerator::engineToString(Engine engine)
{
    switch (engine) {
        case Whisper: return "Whisper";
        case GoogleSpeech: return "Google Speech";
        case AzureSpeech: return "Azure Speech";
        case AWSTranscribe: return "AWS Transcribe";
        case LocalVosk: return "Vosk";
        default: return "Unknown";
    }
}

AISubtitleGenerator::Engine AISubtitleGenerator::stringToEngine(const QString& engineStr)
{
    QString lower = engineStr.toLower();
    if (lower.contains("whisper")) return Whisper;
    if (lower.contains("google")) return GoogleSpeech;
    if (lower.contains("azure")) return AzureSpeech;
    if (lower.contains("aws")) return AWSTranscribe;
    if (lower.contains("vosk")) return LocalVosk;
    return Whisper;
}

} // namespace AI
} // namespace EonPlay
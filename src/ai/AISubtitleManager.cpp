#include "ai/AISubtitleManager.h"
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QLoggingCategory>
#include <QDebug>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(aiSubtitleManager, "eonplay.ai.subtitlemanager")

namespace EonPlay {
namespace AI {

AISubtitleManager::AISubtitleManager(QObject* parent)
    : QObject(parent)
    , m_totalOperations(0)
    , m_completedOperations(0)
    , m_batchTimer(new QTimer(this))
    , m_processingBatch(false)
    , m_autoDetectionEnabled(true)
{
    setupComponents();
    connectSignals();
    
    // Setup batch processing timer
    m_batchTimer->setSingleShot(true);
    m_batchTimer->setInterval(100); // Small delay between batch operations
    connect(m_batchTimer, &QTimer::timeout, this, &AISubtitleManager::processBatchQueue);
    
    // Set default subtitle search paths
    m_settings.subtitleSearchPaths << "."
                                   << "./subtitles"
                                   << "./subs"
                                   << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/subtitles";
    
    // Set default preferred languages
    m_settings.preferredLanguages << "en" << "eng" << "english";
    
    qCInfo(aiSubtitleManager) << "AISubtitleManager initialized";
}

void AISubtitleManager::setSettings(const AISubtitleSettings& settings)
{
    m_settings = settings;
    
    // Update component settings
    if (m_generator) {
        AISubtitleGenerator::GenerationOptions genOptions = m_generator->generationOptions();
        genOptions.engine = m_settings.preferredEngine;
        genOptions.language = m_settings.defaultLanguage;
        m_generator->setGenerationOptions(genOptions);
    }
    
    if (m_detector) {
        m_detector->setDetectionMethod(m_settings.detectionMethod);
    }
    
    if (m_translator) {
        SubtitleTranslator::TranslationOptions transOptions = m_translator->translationOptions();
        transOptions.service = m_settings.translationService;
        transOptions.targetLanguage = m_settings.preferredTargetLanguage;
        m_translator->setTranslationOptions(transOptions);
    }
    
    qCInfo(aiSubtitleManager) << "Settings updated";
}

void AISubtitleManager::processMediaFile(const QString& mediaFilePath)
{
    if (!QFile::exists(mediaFilePath)) {
        emit operationFailed("processMediaFile", "Media file does not exist: " + mediaFilePath);
        return;
    }
    
    if (!isMediaFile(mediaFilePath)) {
        emit operationFailed("processMediaFile", "File is not a supported media format: " + mediaFilePath);
        return;
    }
    
    m_currentOperation = "Processing media file";
    emit operationStarted(m_currentOperation, mediaFilePath);
    
    // Find existing subtitle files
    QStringList subtitleFiles = findSubtitleFiles(mediaFilePath);
    
    if (!subtitleFiles.isEmpty()) {
        emit subtitlesDetected(mediaFilePath, subtitleFiles);
        
        // Auto-load best subtitle if enabled
        if (m_settings.autoLoadSubtitles) {
            QString bestSubtitle = findBestSubtitleFile(mediaFilePath);
            if (!bestSubtitle.isEmpty()) {
                emit subtitleAutoLoaded(mediaFilePath, bestSubtitle);
                
                // Process the subtitle file for language detection
                if (m_settings.autoDetectionMode == OnMediaLoad || m_settings.autoDetectionMode == Always) {
                    processSubtitleFile(bestSubtitle);
                }
            }
        }
    } else if (m_settings.autoGenerateSubtitles) {
        // No subtitles found, generate them
        generateSubtitlesForMedia(mediaFilePath);
        return; // Will emit completion when generation is done
    }
    
    emit operationCompleted(m_currentOperation, mediaFilePath);
}

void AISubtitleManager::processSubtitleFile(const QString& subtitleFilePath)
{
    if (!QFile::exists(subtitleFilePath)) {
        emit operationFailed("processSubtitleFile", "Subtitle file does not exist: " + subtitleFilePath);
        return;
    }
    
    if (!isSubtitleFile(subtitleFilePath)) {
        emit operationFailed("processSubtitleFile", "File is not a supported subtitle format: " + subtitleFilePath);
        return;
    }
    
    m_currentOperation = "Processing subtitle file";
    emit operationStarted(m_currentOperation, subtitleFilePath);
    
    // Perform language detection if enabled
    if (m_settings.autoDetectionMode == OnSubtitleLoad || m_settings.autoDetectionMode == Always) {
        performAutoDetection(subtitleFilePath);
    } else {
        emit operationCompleted(m_currentOperation, subtitleFilePath);
    }
}

void AISubtitleManager::generateSubtitlesForMedia(const QString& mediaFilePath, const QString& outputPath)
{
    if (!m_generator) {
        emit operationFailed("generateSubtitles", "Subtitle generator not available");
        return;
    }
    
    if (m_generator->isGenerating()) {
        emit operationFailed("generateSubtitles", "Subtitle generation already in progress");
        return;
    }
    
    m_currentOperation = "Generating subtitles";
    m_generator->generateSubtitles(mediaFilePath, outputPath);
}

void AISubtitleManager::translateSubtitleFile(const QString& inputPath, const QString& outputPath,
                                             SubtitleTranslator::Language targetLanguage)
{
    if (!m_translator) {
        emit operationFailed("translateSubtitles", "Subtitle translator not available");
        return;
    }
    
    if (m_translator->isTranslating()) {
        emit operationFailed("translateSubtitles", "Translation already in progress");
        return;
    }
    
    m_currentOperation = "Translating subtitles";
    m_translator->translateSubtitleFile(inputPath, outputPath, SubtitleTranslator::Auto, targetLanguage);
}

void AISubtitleManager::enableAutoDetection(bool enabled)
{
    m_autoDetectionEnabled = enabled;
    if (enabled) {
        m_settings.autoDetectionMode = OnSubtitleLoad;
    } else {
        m_settings.autoDetectionMode = Disabled;
    }
}

bool AISubtitleManager::isAutoDetectionEnabled() const
{
    return m_autoDetectionEnabled && m_settings.autoDetectionMode != Disabled;
}

QStringList AISubtitleManager::findSubtitleFiles(const QString& mediaFilePath)
{
    QStringList subtitleFiles;
    QFileInfo mediaInfo(mediaFilePath);
    QString baseName = mediaInfo.completeBaseName();
    QString mediaDir = mediaInfo.absolutePath();
    
    QStringList searchPaths = m_settings.subtitleSearchPaths;
    // Add media file directory if not already included
    if (!searchPaths.contains(".") && !searchPaths.contains(mediaDir)) {
        searchPaths.prepend(mediaDir);
    }
    
    QStringList subtitleExtensions = getSubtitleExtensions();
    
    for (const QString& searchPath : searchPaths) {
        QString absoluteSearchPath = searchPath;
        if (QFileInfo(searchPath).isRelative()) {
            absoluteSearchPath = QDir(mediaDir).absoluteFilePath(searchPath);
        }
        
        QDir searchDir(absoluteSearchPath);
        if (!searchDir.exists()) {
            continue;
        }
        
        // Look for subtitle files with matching base name
        for (const QString& ext : subtitleExtensions) {
            QString subtitlePattern = baseName + "*." + ext;
            QStringList matches = searchDir.entryList(QStringList() << subtitlePattern, QDir::Files);
            
            for (const QString& match : matches) {
                QString fullPath = searchDir.absoluteFilePath(match);
                if (!subtitleFiles.contains(fullPath)) {
                    subtitleFiles << fullPath;
                }
            }
        }
    }
    
    return subtitleFiles;
}

QString AISubtitleManager::findBestSubtitleFile(const QString& mediaFilePath)
{
    QStringList subtitleFiles = findSubtitleFiles(mediaFilePath);
    if (subtitleFiles.isEmpty()) {
        return QString();
    }
    
    // Prioritize by preferred languages
    for (const QString& preferredLang : m_settings.preferredLanguages) {
        for (const QString& subtitleFile : subtitleFiles) {
            QString fileName = QFileInfo(subtitleFile).fileName().toLower();
            if (fileName.contains(preferredLang.toLower())) {
                return subtitleFile;
            }
        }
    }
    
    // If no preferred language found, return the first one
    return subtitleFiles.first();
}

void AISubtitleManager::processMultipleMediaFiles(const QStringList& mediaFilePaths)
{
    m_totalOperations = mediaFilePaths.size();
    m_completedOperations = 0;
    
    emit batchOperationStarted("Processing media files", m_totalOperations);
    
    for (const QString& mediaFilePath : mediaFilePaths) {
        BatchOperation operation;
        operation.type = BatchOperation::DetectLanguage; // Just process, don't generate
        operation.inputPath = mediaFilePath;
        queueBatchOperation(operation);
    }
}

void AISubtitleManager::generateSubtitlesForMultipleFiles(const QStringList& mediaFilePaths, const QString& outputDirectory)
{
    m_totalOperations = mediaFilePaths.size();
    m_completedOperations = 0;
    
    emit batchOperationStarted("Generating subtitles", m_totalOperations);
    
    for (const QString& mediaFilePath : mediaFilePaths) {
        QFileInfo mediaInfo(mediaFilePath);
        QString outputPath = QDir(outputDirectory).absoluteFilePath(mediaInfo.completeBaseName() + ".srt");
        
        BatchOperation operation;
        operation.type = BatchOperation::GenerateSubtitles;
        operation.inputPath = mediaFilePath;
        operation.outputPath = outputPath;
        queueBatchOperation(operation);
    }
}

void AISubtitleManager::translateMultipleSubtitleFiles(const QStringList& subtitlePaths, const QString& outputDirectory,
                                                      SubtitleTranslator::Language targetLanguage)
{
    m_totalOperations = subtitlePaths.size();
    m_completedOperations = 0;
    
    emit batchOperationStarted("Translating subtitles", m_totalOperations);
    
    for (const QString& subtitlePath : subtitlePaths) {
        QFileInfo subtitleInfo(subtitlePath);
        QString outputPath = QDir(outputDirectory).absoluteFilePath(
            subtitleInfo.completeBaseName() + "_translated." + subtitleInfo.suffix());
        
        BatchOperation operation;
        operation.type = BatchOperation::TranslateSubtitles;
        operation.inputPath = subtitlePath;
        operation.outputPath = outputPath;
        operation.targetLanguage = targetLanguage;
        queueBatchOperation(operation);
    }
}

bool AISubtitleManager::isBusy() const
{
    bool generatorBusy = m_generator && m_generator->isGenerating();
    bool translatorBusy = m_translator && m_translator->isTranslating();
    bool batchBusy = m_processingBatch || !m_batchQueue.isEmpty();
    
    return generatorBusy || translatorBusy || batchBusy;
}

void AISubtitleManager::setupComponents()
{
    // Create AI components
    m_generator = std::make_unique<AISubtitleGenerator>(this);
    m_detector = std::make_unique<SubtitleLanguageDetector>(this);
    m_translator = std::make_unique<SubtitleTranslator>(this);
    
    qCInfo(aiSubtitleManager) << "AI components created";
}

void AISubtitleManager::connectSignals()
{
    // Connect generator signals
    if (m_generator) {
        connect(m_generator.get(), &AISubtitleGenerator::generationStarted,
                this, &AISubtitleManager::onGenerationStarted);
        connect(m_generator.get(), &AISubtitleGenerator::generationCompleted,
                this, &AISubtitleManager::onGenerationCompleted);
        connect(m_generator.get(), &AISubtitleGenerator::generationFailed,
                this, &AISubtitleManager::onGenerationFailed);
    }
    
    // Connect detector signals
    if (m_detector) {
        connect(m_detector.get(), &SubtitleLanguageDetector::languageDetected,
                this, &AISubtitleManager::onLanguageDetected);
        connect(m_detector.get(), &SubtitleLanguageDetector::detectionFailed,
                this, &AISubtitleManager::onDetectionFailed);
    }
    
    // Connect translator signals
    if (m_translator) {
        connect(m_translator.get(), &SubtitleTranslator::translationStarted,
                this, &AISubtitleManager::onTranslationStarted);
        connect(m_translator.get(), &SubtitleTranslator::translationCompleted,
                this, &AISubtitleManager::onTranslationCompleted);
        connect(m_translator.get(), &SubtitleTranslator::translationFailed,
                this, &AISubtitleManager::onTranslationFailed);
    }
    
    qCInfo(aiSubtitleManager) << "Component signals connected";
}

void AISubtitleManager::performAutoDetection(const QString& filePath)
{
    if (!m_detector || !isAutoDetectionEnabled()) {
        return;
    }
    
    m_detector->detectLanguageFromFile(filePath);
}

void AISubtitleManager::loadBestSubtitleFile(const QString& mediaFilePath)
{
    QString bestSubtitle = findBestSubtitleFile(mediaFilePath);
    if (!bestSubtitle.isEmpty()) {
        emit subtitleAutoLoaded(mediaFilePath, bestSubtitle);
    }
}

bool AISubtitleManager::isMediaFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    QStringList mediaExtensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "3gp", "ogv",
        "mp3", "flac", "wav", "aac", "ogg", "wma", "m4a"
    };
    
    return mediaExtensions.contains(extension);
}

bool AISubtitleManager::isSubtitleFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    return getSubtitleExtensions().contains(extension);
}

QString AISubtitleManager::getMediaFileForSubtitle(const QString& subtitlePath) const
{
    QFileInfo subtitleInfo(subtitlePath);
    QString baseName = subtitleInfo.completeBaseName();
    QString dir = subtitleInfo.absolutePath();
    
    // Remove common subtitle suffixes
    QStringList suffixesToRemove = {"_en", "_eng", "_english", "_sub", "_subtitle"};
    for (const QString& suffix : suffixesToRemove) {
        if (baseName.endsWith(suffix, Qt::CaseInsensitive)) {
            baseName = baseName.left(baseName.length() - suffix.length());
            break;
        }
    }
    
    // Look for media files with matching base name
    QStringList mediaExtensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "3gp", "ogv"
    };
    
    QDir searchDir(dir);
    for (const QString& ext : mediaExtensions) {
        QString mediaFile = searchDir.absoluteFilePath(baseName + "." + ext);
        if (QFile::exists(mediaFile)) {
            return mediaFile;
        }
    }
    
    return QString();
}

QStringList AISubtitleManager::getSubtitleExtensions() const
{
    return QStringList() << "srt" << "ass" << "ssa" << "vtt" << "sub" << "sbv";
}

void AISubtitleManager::queueBatchOperation(const BatchOperation& operation)
{
    m_batchQueue.append(operation);
    
    if (!m_processingBatch) {
        m_batchTimer->start();
    }
}

void AISubtitleManager::processBatchOperation(const BatchOperation& operation)
{
    switch (operation.type) {
        case BatchOperation::GenerateSubtitles:
            generateSubtitlesForMedia(operation.inputPath, operation.outputPath);
            break;
        case BatchOperation::TranslateSubtitles:
            translateSubtitleFile(operation.inputPath, operation.outputPath, operation.targetLanguage);
            break;
        case BatchOperation::DetectLanguage:
            if (isMediaFile(operation.inputPath)) {
                processMediaFile(operation.inputPath);
            } else if (isSubtitleFile(operation.inputPath)) {
                processSubtitleFile(operation.inputPath);
            }
            break;
    }
}

void AISubtitleManager::processBatchQueue()
{
    if (m_batchQueue.isEmpty() || m_processingBatch) {
        return;
    }
    
    if (isBusy()) {
        // Wait for current operations to complete
        m_batchTimer->start();
        return;
    }
    
    m_processingBatch = true;
    BatchOperation operation = m_batchQueue.takeFirst();
    
    processBatchOperation(operation);
    
    m_processingBatch = false;
    
    // Process next operation if available
    if (!m_batchQueue.isEmpty()) {
        m_batchTimer->start();
    }
}

// Slot implementations
void AISubtitleManager::onGenerationStarted(const QString& filePath)
{
    emit operationStarted("Generating subtitles", filePath);
}

void AISubtitleManager::onGenerationCompleted(const QString& subtitlePath)
{
    m_completedOperations++;
    emit subtitlesGenerated("", subtitlePath); // Media file path not available here
    emit operationCompleted("Generating subtitles", subtitlePath);
    
    if (m_totalOperations > 0) {
        emit batchOperationProgress(m_completedOperations, m_totalOperations);
        
        if (m_completedOperations >= m_totalOperations) {
            emit batchOperationCompleted("Generating subtitles", m_completedOperations, 0);
        }
    }
}

void AISubtitleManager::onGenerationFailed(const QString& error)
{
    m_completedOperations++;
    emit operationFailed("Generating subtitles", error);
    
    if (m_totalOperations > 0) {
        emit batchOperationProgress(m_completedOperations, m_totalOperations);
        
        if (m_completedOperations >= m_totalOperations) {
            int failedFiles = m_totalOperations - (m_completedOperations - 1);
            emit batchOperationCompleted("Generating subtitles", m_completedOperations - 1, failedFiles);
        }
    }
}

void AISubtitleManager::onLanguageDetected(const QString& filePath, const SubtitleLanguageDetector::DetectionResult& result)
{
    QString languageName = SubtitleLanguageDetector::languageToDisplayName(result.language);
    emit subtitleLanguageDetected(filePath, languageName, result.confidence);
    
    // Auto-translate if enabled and confidence is high enough
    if (m_settings.autoTranslateSubtitles && 
        result.confidence >= m_settings.confidenceThreshold &&
        result.language != static_cast<SubtitleLanguageDetector::Language>(m_settings.preferredTargetLanguage)) {
        
        QFileInfo fileInfo(filePath);
        QString outputPath = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + 
                           "_" + SubtitleTranslator::languageToCode(m_settings.preferredTargetLanguage) + 
                           "." + fileInfo.suffix();
        
        translateSubtitleFile(filePath, outputPath, m_settings.preferredTargetLanguage);
    }
    
    emit operationCompleted("Language detection", filePath);
}

void AISubtitleManager::onDetectionFailed(const QString& filePath, const QString& error)
{
    emit operationFailed("Language detection", error);
}

void AISubtitleManager::onTranslationStarted(const QString& filePath)
{
    emit operationStarted("Translating subtitles", filePath);
}

void AISubtitleManager::onTranslationCompleted(const QString& outputPath)
{
    m_completedOperations++;
    emit subtitlesTranslated("", outputPath, ""); // Input path and language not available here
    emit operationCompleted("Translating subtitles", outputPath);
    
    if (m_totalOperations > 0) {
        emit batchOperationProgress(m_completedOperations, m_totalOperations);
        
        if (m_completedOperations >= m_totalOperations) {
            emit batchOperationCompleted("Translating subtitles", m_completedOperations, 0);
        }
    }
}

void AISubtitleManager::onTranslationFailed(const QString& error)
{
    m_completedOperations++;
    emit operationFailed("Translating subtitles", error);
    
    if (m_totalOperations > 0) {
        emit batchOperationProgress(m_completedOperations, m_totalOperations);
        
        if (m_completedOperations >= m_totalOperations) {
            int failedFiles = m_totalOperations - (m_completedOperations - 1);
            emit batchOperationCompleted("Translating subtitles", m_completedOperations - 1, failedFiles);
        }
    }
}

} // namespace AI
} // namespace EonPlay
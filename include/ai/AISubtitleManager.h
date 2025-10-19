#pragma once

#include "ai/AISubtitleGenerator.h"
#include "ai/SubtitleLanguageDetector.h"
#include "ai/SubtitleTranslator.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace EonPlay {
namespace AI {

/**
 * @brief Manages all AI-powered subtitle features for EonPlay
 * 
 * Coordinates subtitle generation, language detection, and translation
 * to provide a unified AI subtitle experience.
 */
class AISubtitleManager : public QObject
{
    Q_OBJECT

public:
    enum AutoDetectionMode {
        Disabled,
        OnMediaLoad,
        OnSubtitleLoad,
        Always
    };

    struct AISubtitleSettings {
        // Generation settings
        AISubtitleGenerator::Engine preferredEngine = AISubtitleGenerator::Whisper;
        AISubtitleGenerator::Language defaultLanguage = AISubtitleGenerator::Auto;
        bool autoGenerateSubtitles = false;
        
        // Detection settings
        SubtitleLanguageDetector::DetectionMethod detectionMethod = SubtitleLanguageDetector::LocalNGram;
        AutoDetectionMode autoDetectionMode = OnSubtitleLoad;
        double confidenceThreshold = 0.7;
        
        // Translation settings
        SubtitleTranslator::TranslationService translationService = SubtitleTranslator::GoogleTranslate;
        SubtitleTranslator::Language preferredTargetLanguage = SubtitleTranslator::English;
        bool autoTranslateSubtitles = false;
        
        // Auto-loading settings
        bool autoLoadSubtitles = true;
        QStringList subtitleSearchPaths;
        QStringList preferredLanguages;
    };

    explicit AISubtitleManager(QObject* parent = nullptr);
    ~AISubtitleManager() = default;

    // Configuration
    void setSettings(const AISubtitleSettings& settings);
    AISubtitleSettings settings() const { return m_settings; }

    // Component access
    AISubtitleGenerator* subtitleGenerator() const { return m_generator.get(); }
    SubtitleLanguageDetector* languageDetector() const { return m_detector.get(); }
    SubtitleTranslator* subtitleTranslator() const { return m_translator.get(); }

    // High-level operations
    void processMediaFile(const QString& mediaFilePath);
    void processSubtitleFile(const QString& subtitleFilePath);
    void generateSubtitlesForMedia(const QString& mediaFilePath, const QString& outputPath = QString());
    void translateSubtitleFile(const QString& inputPath, const QString& outputPath,
                              SubtitleTranslator::Language targetLanguage = SubtitleTranslator::English);

    // Auto-detection and loading
    void enableAutoDetection(bool enabled);
    bool isAutoDetectionEnabled() const;
    QStringList findSubtitleFiles(const QString& mediaFilePath);
    QString findBestSubtitleFile(const QString& mediaFilePath);

    // Batch operations
    void processMultipleMediaFiles(const QStringList& mediaFilePaths);
    void generateSubtitlesForMultipleFiles(const QStringList& mediaFilePaths, const QString& outputDirectory);
    void translateMultipleSubtitleFiles(const QStringList& subtitlePaths, const QString& outputDirectory,
                                       SubtitleTranslator::Language targetLanguage);

    // Status and progress
    bool isBusy() const;
    QString currentOperation() const { return m_currentOperation; }
    int totalOperations() const { return m_totalOperations; }
    int completedOperations() const { return m_completedOperations; }

signals:
    // High-level operation signals
    void operationStarted(const QString& operation, const QString& filePath);
    void operationProgress(int completed, int total, const QString& status);
    void operationCompleted(const QString& operation, const QString& filePath);
    void operationFailed(const QString& operation, const QString& error);

    // Subtitle detection and loading
    void subtitlesDetected(const QString& mediaFilePath, const QStringList& subtitlePaths);
    void subtitleLanguageDetected(const QString& subtitlePath, const QString& language, double confidence);
    void subtitleAutoLoaded(const QString& mediaFilePath, const QString& subtitlePath);

    // Generation and translation results
    void subtitlesGenerated(const QString& mediaFilePath, const QString& subtitlePath);
    void subtitlesTranslated(const QString& inputPath, const QString& outputPath, const QString& targetLanguage);

    // Batch operation signals
    void batchOperationStarted(const QString& operation, int totalFiles);
    void batchOperationProgress(int completedFiles, int totalFiles);
    void batchOperationCompleted(const QString& operation, int successfulFiles, int failedFiles);

private slots:
    // Generator slots
    void onGenerationStarted(const QString& filePath);
    void onGenerationCompleted(const QString& subtitlePath);
    void onGenerationFailed(const QString& error);

    // Detector slots
    void onLanguageDetected(const QString& filePath, const SubtitleLanguageDetector::DetectionResult& result);
    void onDetectionFailed(const QString& filePath, const QString& error);

    // Translator slots
    void onTranslationStarted(const QString& filePath);
    void onTranslationCompleted(const QString& outputPath);
    void onTranslationFailed(const QString& error);

    // Batch processing
    void processBatchQueue();

private:
    // Initialization
    void setupComponents();
    void connectSignals();

    // Auto-detection helpers
    void performAutoDetection(const QString& filePath);
    void loadBestSubtitleFile(const QString& mediaFilePath);

    // File utilities
    bool isMediaFile(const QString& filePath) const;
    bool isSubtitleFile(const QString& filePath) const;
    QString getMediaFileForSubtitle(const QString& subtitlePath) const;
    QStringList getSubtitleExtensions() const;

    // Batch processing
    struct BatchOperation {
        enum Type {
            GenerateSubtitles,
            TranslateSubtitles,
            DetectLanguage
        };
        
        Type type;
        QString inputPath;
        QString outputPath;
        SubtitleTranslator::Language targetLanguage = SubtitleTranslator::English;
    };

    void queueBatchOperation(const BatchOperation& operation);
    void processBatchOperation(const BatchOperation& operation);

    // Member variables
    AISubtitleSettings m_settings;
    
    // Components
    std::unique_ptr<AISubtitleGenerator> m_generator;
    std::unique_ptr<SubtitleLanguageDetector> m_detector;
    std::unique_ptr<SubtitleTranslator> m_translator;
    
    // Operation tracking
    QString m_currentOperation;
    int m_totalOperations;
    int m_completedOperations;
    
    // Batch processing
    QList<BatchOperation> m_batchQueue;
    QTimer* m_batchTimer;
    bool m_processingBatch;
    
    // Auto-detection state
    bool m_autoDetectionEnabled;
    QStringList m_processedFiles;
};

} // namespace AI
} // namespace EonPlay
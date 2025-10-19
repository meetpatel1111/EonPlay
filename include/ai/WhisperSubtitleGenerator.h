#pragma once

#include "IAISubtitleGenerator.h"
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QMutex>

namespace EonPlay {
namespace AI {

/**
 * @brief Whisper-based AI subtitle generator
 * 
 * Uses OpenAI Whisper for speech-to-text subtitle generation.
 * Supports both local Whisper installation and cloud-based processing.
 */
class WhisperSubtitleGenerator : public IAISubtitleGenerator
{
    Q_OBJECT

public:
    enum ProcessingMode {
        LocalWhisper,      // Use local Whisper installation
        CloudAPI,          // Use cloud-based Whisper API
        AutoDetect         // Auto-detect best available option
    };

    explicit WhisperSubtitleGenerator(QObject* parent = nullptr);
    ~WhisperSubtitleGenerator() override;

    // IAISubtitleGenerator interface
    bool initialize() override;
    bool isAvailable() const override;
    QStringList getSupportedFormats() const override;
    QStringList getSupportedLanguages() const override;
    bool generateSubtitles(const QString& mediaFilePath, 
                          const QString& targetLanguage,
                          SubtitleGenerationResult& result) override;
    bool generateSubtitlesFromAudio(const QByteArray& audioData,
                                   int sampleRate,
                                   int channels,
                                   const QString& targetLanguage,
                                   SubtitleGenerationResult& result) override;
    void cancelGeneration() override;
    double getProgress() const override;
    QString getGeneratorName() const override;
    QString getVersion() const override;

    // Whisper-specific configuration
    ProcessingMode processingMode() const { return m_processingMode; }
    void setProcessingMode(ProcessingMode mode);

    QString whisperExecutablePath() const { return m_whisperPath; }
    void setWhisperExecutablePath(const QString& path);

    QString modelSize() const { return m_modelSize; }
    void setModelSize(const QString& size); // tiny, base, small, medium, large

    bool useGPU() const { return m_useGPU; }
    void setUseGPU(bool enabled);

    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString& key);

    int maxSegmentLength() const { return m_maxSegmentLength; }
    void setMaxSegmentLength(int seconds);

    double confidenceThreshold() const { return m_confidenceThreshold; }
    void setConfidenceThreshold(double threshold);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();
    void onProgressTimer();

private:
    bool initializeLocalWhisper();
    bool initializeCloudAPI();
    bool checkWhisperInstallation();
    QString findWhisperExecutable();
    bool extractAudioFromMedia(const QString& mediaPath, QString& audioPath);
    bool processWithLocalWhisper(const QString& audioPath, const QString& language);
    bool processWithCloudAPI(const QString& audioPath, const QString& language);
    SubtitleGenerationResult parseWhisperOutput(const QString& output);
    QList<SubtitleSegment> parseSegments(const QString& output);
    QString formatSubtitleContent(const QList<SubtitleSegment>& segments);
    QString detectLanguageFromOutput(const QString& output);
    double calculateOverallConfidence(const QList<SubtitleSegment>& segments);
    void cleanupTemporaryFiles();

    // Configuration
    ProcessingMode m_processingMode;
    QString m_whisperPath;
    QString m_modelSize;
    bool m_useGPU;
    QString m_apiKey;
    int m_maxSegmentLength;
    double m_confidenceThreshold;

    // Processing state
    QProcess* m_process;
    QTimer* m_progressTimer;
    QTemporaryDir* m_tempDir;
    QString m_currentAudioFile;
    QString m_currentOutputFile;
    double m_currentProgress;
    bool m_isProcessing;
    bool m_isCancelled;
    mutable QMutex m_mutex;

    // Supported formats and languages
    QStringList m_supportedFormats;
    QStringList m_supportedLanguages;

    // Results
    SubtitleGenerationResult m_currentResult;
    QList<SubtitleSegment> m_currentSegments;
};

} // namespace AI
} // namespace EonPlay
#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QVariant>

namespace EonPlay {
namespace AI {

/**
 * @brief Subtitle generation result
 */
struct SubtitleGenerationResult
{
    bool success = false;
    QString errorMessage;
    QString generatedContent;
    QString detectedLanguage;
    double confidence = 0.0;
    qint64 processingTimeMs = 0;
    int segmentCount = 0;
    
    QString summary() const;
};

/**
 * @brief Subtitle generation segment
 */
struct SubtitleSegment
{
    qint64 startTime = 0;      // Start time in milliseconds
    qint64 endTime = 0;        // End time in milliseconds
    QString text;              // Transcribed text
    double confidence = 0.0;   // Confidence score (0.0 - 1.0)
    QString language;          // Detected language code
    
    bool isValid() const { return startTime < endTime && !text.isEmpty(); }
};

/**
 * @brief Abstract interface for AI subtitle generation
 * 
 * Provides speech-to-text functionality for generating subtitles from audio tracks.
 */
class IAISubtitleGenerator : public QObject
{
    Q_OBJECT

public:
    explicit IAISubtitleGenerator(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IAISubtitleGenerator() = default;

    /**
     * @brief Initialize the AI subtitle generator
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Check if the generator is available and ready
     * @return true if ready to generate subtitles
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Get supported audio formats for subtitle generation
     * @return List of supported audio format extensions
     */
    virtual QStringList getSupportedFormats() const = 0;

    /**
     * @brief Get supported languages for subtitle generation
     * @return List of supported language codes (ISO 639-1)
     */
    virtual QStringList getSupportedLanguages() const = 0;

    /**
     * @brief Generate subtitles from media file
     * @param mediaFilePath Path to media file
     * @param targetLanguage Target language code (empty for auto-detect)
     * @param result Generation result (output)
     * @return true if generation successful
     */
    virtual bool generateSubtitles(const QString& mediaFilePath, 
                                  const QString& targetLanguage,
                                  SubtitleGenerationResult& result) = 0;

    /**
     * @brief Generate subtitles from audio data
     * @param audioData Raw audio data
     * @param sampleRate Audio sample rate
     * @param channels Number of audio channels
     * @param targetLanguage Target language code (empty for auto-detect)
     * @param result Generation result (output)
     * @return true if generation successful
     */
    virtual bool generateSubtitlesFromAudio(const QByteArray& audioData,
                                           int sampleRate,
                                           int channels,
                                           const QString& targetLanguage,
                                           SubtitleGenerationResult& result) = 0;

    /**
     * @brief Cancel ongoing subtitle generation
     */
    virtual void cancelGeneration() = 0;

    /**
     * @brief Get generation progress (0.0 - 1.0)
     * @return Progress percentage
     */
    virtual double getProgress() const = 0;

    /**
     * @brief Get generator name/identifier
     * @return Generator name
     */
    virtual QString getGeneratorName() const = 0;

    /**
     * @brief Get generator version
     * @return Version string
     */
    virtual QString getVersion() const = 0;

signals:
    /**
     * @brief Emitted when generation progress changes
     * @param progress Progress percentage (0.0 - 1.0)
     * @param status Current status message
     */
    void progressChanged(double progress, const QString& status);

    /**
     * @brief Emitted when generation is completed
     * @param result Generation result
     */
    void generationCompleted(const SubtitleGenerationResult& result);

    /**
     * @brief Emitted when generation fails
     * @param errorMessage Error description
     */
    void generationFailed(const QString& errorMessage);

    /**
     * @brief Emitted when a subtitle segment is processed
     * @param segment Processed subtitle segment
     */
    void segmentProcessed(const SubtitleSegment& segment);
};

} // namespace AI
} // namespace EonPlay
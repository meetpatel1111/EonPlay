#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QProcess>
#include <QTemporaryFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <memory>

namespace EonPlay {
namespace AI {

/**
 * @brief AI-powered subtitle generator using speech-to-text
 * 
 * Provides automatic subtitle generation from audio tracks using
 * various speech recognition engines and AI services.
 */
class AISubtitleGenerator : public QObject
{
    Q_OBJECT

public:
    enum Engine {
        Whisper,        // OpenAI Whisper (local)
        GoogleSpeech,   // Google Speech-to-Text API
        AzureSpeech,    // Azure Cognitive Services
        AWSTranscribe,  // AWS Transcribe
        LocalVosk       // Local Vosk engine
    };

    enum Language {
        Auto,
        English,
        Spanish,
        French,
        German,
        Italian,
        Portuguese,
        Russian,
        Chinese,
        Japanese,
        Korean,
        Arabic,
        Hindi
    };

    struct GenerationOptions {
        Engine engine = Whisper;
        Language language = Auto;
        bool includeTimestamps = true;
        bool includeSpeakerLabels = false;
        int maxLineLength = 42;
        double confidenceThreshold = 0.7;
        QString outputFormat = "srt";
        bool enablePunctuation = true;
        bool enableCapitalization = true;
    };

    explicit AISubtitleGenerator(QObject* parent = nullptr);
    ~AISubtitleGenerator();

    // Configuration
    void setEngine(Engine engine) { m_options.engine = engine; }
    Engine engine() const { return m_options.engine; }

    void setLanguage(Language language) { m_options.language = language; }
    Language language() const { return m_options.language; }

    void setGenerationOptions(const GenerationOptions& options) { m_options = options; }
    GenerationOptions generationOptions() const { return m_options; }

    // Engine availability
    bool isEngineAvailable(Engine engine) const;
    QStringList getAvailableEngines() const;
    QString getEngineDescription(Engine engine) const;

    // API configuration
    void setApiKey(Engine engine, const QString& apiKey);
    QString getApiKey(Engine engine) const;
    bool isApiConfigured(Engine engine) const;

    // Generation operations
    void generateSubtitles(const QString& mediaFilePath, const QString& outputPath = QString());
    void generateSubtitlesFromAudio(const QString& audioFilePath, const QString& outputPath = QString());
    void cancelGeneration();

    // Progress and status
    bool isGenerating() const { return m_isGenerating; }
    int progress() const { return m_progress; }
    QString currentStatus() const { return m_currentStatus; }

    // Language detection
    void detectLanguage(const QString& mediaFilePath);
    Language getDetectedLanguage() const { return m_detectedLanguage; }
    double getLanguageConfidence() const { return m_languageConfidence; }

    // Utility methods
    static QString languageToString(Language language);
    static Language stringToLanguage(const QString& languageStr);
    static QString engineToString(Engine engine);
    static Engine stringToEngine(const QString& engineStr);

signals:
    void generationStarted(const QString& filePath);
    void generationProgress(int percentage, const QString& status);
    void generationCompleted(const QString& subtitlePath);
    void generationFailed(const QString& error);
    void generationCancelled();
    
    void languageDetected(Language language, double confidence);
    void languageDetectionFailed(const QString& error);

private slots:
    void handleWhisperProcess();
    void handleNetworkReply();
    void updateProgress();

private:
    // Engine implementations
    void generateWithWhisper(const QString& audioPath, const QString& outputPath);
    void generateWithGoogleSpeech(const QString& audioPath, const QString& outputPath);
    void generateWithAzureSpeech(const QString& audioPath, const QString& outputPath);
    void generateWithAWSTranscribe(const QString& audioPath, const QString& outputPath);
    void generateWithVosk(const QString& audioPath, const QString& outputPath);

    // Audio extraction
    QString extractAudioFromVideo(const QString& videoPath);
    bool isAudioFile(const QString& filePath) const;

    // Subtitle formatting
    QString formatSubtitles(const QJsonObject& transcription, const QString& format);
    QString formatSRT(const QJsonObject& transcription);
    QString formatVTT(const QJsonObject& transcription);
    QString formatASS(const QJsonObject& transcription);

    // Language detection helpers
    void detectLanguageWithWhisper(const QString& audioPath);
    void detectLanguageWithAPI(const QString& audioPath);

    // Utility methods
    QString getWhisperExecutable() const;
    QString getFFmpegExecutable() const;
    bool checkDependencies(Engine engine) const;
    void setupNetworkManager();
    void cleanupTempFiles();

    // Member variables
    GenerationOptions m_options;
    bool m_isGenerating;
    int m_progress;
    QString m_currentStatus;
    Language m_detectedLanguage;
    double m_languageConfidence;

    // Process management
    std::unique_ptr<QProcess> m_currentProcess;
    QTimer* m_progressTimer;
    
    // Network management
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    
    // Temporary files
    QStringList m_tempFiles;
    std::unique_ptr<QTemporaryFile> m_tempAudioFile;
    
    // API keys
    QHash<Engine, QString> m_apiKeys;
    
    // Engine paths
    QString m_whisperPath;
    QString m_ffmpegPath;
    QString m_voskModelPath;
};

} // namespace AI
} // namespace EonPlay
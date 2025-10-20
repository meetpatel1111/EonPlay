#ifndef ADVANCEDAUDIOPROCESSOR_H
#define ADVANCEDAUDIOPROCESSOR_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <complex>

// Forward declarations
class AudioProcessor;

// Type aliases for consistent complex number usage
using ComplexF = std::complex<float>;

/**
 * @brief Advanced audio processing system with karaoke, lyrics, and format conversion
 * 
 * Provides comprehensive advanced audio processing including vocal removal,
 * multi-track support, lyrics display, metadata editing, format conversion,
 * and AI-powered noise reduction.
 */
class AdvancedAudioProcessor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Karaoke processing modes
     */
    enum KaraokeMode {
        Off = 0,
        VocalRemoval,
        VocalIsolation,
        CenterChannelExtraction,
        AdvancedVocalRemoval
    };

    /**
     * @brief Audio track information
     */
    struct AudioTrackInfo {
        int trackId;              // Track ID
        QString name;             // Track name
        QString language;         // Language code
        QString codec;            // Audio codec
        int channels;             // Channel count
        int sampleRate;           // Sample rate
        int bitrate;              // Bitrate
        bool isDefault;           // Is default track
        
        AudioTrackInfo() : trackId(-1), channels(2), sampleRate(44100), bitrate(0), isDefault(false) {}
    };

    /**
     * @brief Lyrics line information
     */
    struct LyricsLine {
        qint64 startTime;         // Start time in milliseconds
        qint64 endTime;           // End time in milliseconds
        QString text;             // Lyrics text
        QString translation;      // Translation (optional)
        
        LyricsLine() : startTime(0), endTime(0) {}
        LyricsLine(qint64 start, qint64 end, const QString& txt) 
            : startTime(start), endTime(end), text(txt) {}
    };

    /**
     * @brief Audio metadata information
     */
    struct AudioMetadata {
        QString title;
        QString artist;
        QString album;
        QString albumArtist;
        QString genre;
        QString year;
        QString track;
        QString totalTracks;
        QString disc;
        QString totalDiscs;
        QString comment;
        QString composer;
        QString copyright;
        QByteArray albumArt;      // Album art image data
        
        AudioMetadata() = default;
    };

    /**
     * @brief Audio format conversion options
     */
    struct ConversionOptions {
        QString outputFormat;     // Target format (mp3, flac, wav, ogg)
        int quality;              // Quality level (0-10)
        int bitrate;              // Target bitrate (for lossy formats)
        int sampleRate;           // Target sample rate
        int channels;             // Target channel count
        bool preserveMetadata;    // Preserve metadata
        
        ConversionOptions() : quality(8), bitrate(320), sampleRate(44100), 
                             channels(2), preserveMetadata(true) {}
    };

    /**
     * @brief Noise reduction settings
     */
    struct NoiseReductionSettings {
        bool enabled;             // Enable noise reduction
        float strength;           // Reduction strength (0.0 to 1.0)
        float threshold;          // Noise threshold (-60 to 0 dB)
        bool adaptiveMode;        // Adaptive noise reduction
        bool preserveVoice;       // Preserve voice frequencies
        
        NoiseReductionSettings() : enabled(false), strength(0.5f), threshold(-40.0f),
                                  adaptiveMode(true), preserveVoice(true) {}
    };

    explicit AdvancedAudioProcessor(QObject* parent = nullptr);
    ~AdvancedAudioProcessor() override;

    /**
     * @brief Initialize the advanced audio processor
     * @param audioProcessor Base audio processor
     * @return true if initialization was successful
     */
    bool initialize(AudioProcessor* audioProcessor = nullptr);

    /**
     * @brief Shutdown the processor
     */
    void shutdown();

    // Karaoke functionality
    /**
     * @brief Get current karaoke mode
     * @return Current karaoke mode
     */
    KaraokeMode getKaraokeMode() const;

    /**
     * @brief Set karaoke mode
     * @param mode Karaoke mode
     */
    void setKaraokeMode(KaraokeMode mode);

    /**
     * @brief Get karaoke mode name
     * @param mode Karaoke mode
     * @return Mode name
     */
    static QString getKaraokeModeName(KaraokeMode mode);

    /**
     * @brief Get vocal removal strength
     * @return Strength level (0.0 to 1.0)
     */
    float getVocalRemovalStrength() const;

    /**
     * @brief Set vocal removal strength
     * @param strength Strength level (0.0 to 1.0)
     */
    void setVocalRemovalStrength(float strength);

    // Audio track management
    /**
     * @brief Get available audio tracks
     * @return List of audio tracks
     */
    QVector<AudioTrackInfo> getAvailableAudioTracks() const;

    /**
     * @brief Get current audio track
     * @return Current track info
     */
    AudioTrackInfo getCurrentAudioTrack() const;

    /**
     * @brief Set current audio track
     * @param trackId Track ID to switch to
     * @return true if successful
     */
    bool setCurrentAudioTrack(int trackId);

    // Lyrics functionality
    /**
     * @brief Load lyrics from file
     * @param filePath Lyrics file path
     * @return true if successful
     */
    bool loadLyricsFromFile(const QString& filePath);

    /**
     * @brief Load embedded lyrics
     * @param mediaPath Media file path
     * @return true if successful
     */
    bool loadEmbeddedLyrics(const QString& mediaPath);

    /**
     * @brief Get all lyrics lines
     * @return List of lyrics lines
     */
    QVector<LyricsLine> getAllLyrics() const;

    /**
     * @brief Get current lyrics line
     * @param currentTime Current playback time in milliseconds
     * @return Current lyrics line
     */
    LyricsLine getCurrentLyricsLine(qint64 currentTime) const;

    /**
     * @brief Get next lyrics line
     * @param currentTime Current playback time in milliseconds
     * @return Next lyrics line
     */
    LyricsLine getNextLyricsLine(qint64 currentTime) const;

    /**
     * @brief Check if lyrics are available
     * @return true if lyrics are loaded
     */
    bool hasLyrics() const;

    /**
     * @brief Clear loaded lyrics
     */
    void clearLyrics();

    // Metadata editing
    /**
     * @brief Get audio metadata
     * @param filePath Audio file path
     * @return Audio metadata
     */
    AudioMetadata getAudioMetadata(const QString& filePath) const;

    /**
     * @brief Set audio metadata
     * @param filePath Audio file path
     * @param metadata Metadata to set
     * @return true if successful
     */
    bool setAudioMetadata(const QString& filePath, const AudioMetadata& metadata);

    /**
     * @brief Get supported metadata formats
     * @return List of supported formats
     */
    static QStringList getSupportedMetadataFormats();

    // Format conversion
    /**
     * @brief Convert audio file format
     * @param inputPath Input file path
     * @param outputPath Output file path
     * @param options Conversion options
     * @return true if successful
     */
    bool convertAudioFormat(const QString& inputPath, const QString& outputPath, 
                           const ConversionOptions& options);

    /**
     * @brief Get supported input formats
     * @return List of supported input formats
     */
    static QStringList getSupportedInputFormats();

    /**
     * @brief Get supported output formats
     * @return List of supported output formats
     */
    static QStringList getSupportedOutputFormats();

    /**
     * @brief Get conversion progress
     * @return Progress percentage (0-100)
     */
    int getConversionProgress() const;

    /**
     * @brief Cancel ongoing conversion
     */
    void cancelConversion();

    // Noise reduction
    /**
     * @brief Get noise reduction settings
     * @return Current settings
     */
    NoiseReductionSettings getNoiseReductionSettings() const;

    /**
     * @brief Set noise reduction settings
     * @param settings Noise reduction settings
     */
    void setNoiseReductionSettings(const NoiseReductionSettings& settings);

    /**
     * @brief Apply noise reduction to audio buffer
     * @param leftChannel Left audio channel
     * @param rightChannel Right audio channel
     * @param sampleRate Sample rate
     */
    void applyNoiseReduction(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate);

    /**
     * @brief Analyze noise profile
     * @param leftChannel Left audio channel
     * @param rightChannel Right audio channel
     * @param sampleRate Sample rate
     */
    void analyzeNoiseProfile(const QVector<float>& leftChannel, const QVector<float>& rightChannel, int sampleRate);

    /**
     * @brief Process audio with all advanced features
     * @param leftChannel Left audio channel
     * @param rightChannel Right audio channel
     * @param sampleRate Sample rate
     */
    void processAdvancedAudio(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate);

signals:
    /**
     * @brief Emitted when karaoke mode changes
     * @param mode New karaoke mode
     */
    void karaokeModeChanged(KaraokeMode mode);

    /**
     * @brief Emitted when audio track changes
     * @param track New audio track
     */
    void audioTrackChanged(const AudioTrackInfo& track);

    /**
     * @brief Emitted when lyrics are loaded
     * @param lyricsCount Number of lyrics lines loaded
     */
    void lyricsLoaded(int lyricsCount);

    /**
     * @brief Emitted when current lyrics line changes
     * @param line Current lyrics line
     */
    void currentLyricsChanged(const LyricsLine& line);

    /**
     * @brief Emitted when conversion progress updates
     * @param progress Progress percentage (0-100)
     */
    void conversionProgressUpdated(int progress);

    /**
     * @brief Emitted when conversion completes
     * @param success true if successful
     * @param outputPath Output file path
     */
    void conversionCompleted(bool success, const QString& outputPath);

    /**
     * @brief Emitted when noise profile is analyzed
     * @param noiseLevel Detected noise level in dB
     */
    void noiseProfileAnalyzed(float noiseLevel);

private slots:
    void updateConversionProgress();

private:
    // Karaoke processing methods
    void applyVocalRemoval(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void applyVocalIsolation(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void applyCenterChannelExtraction(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void applyAdvancedVocalRemoval(QVector<float>& leftChannel, QVector<float>& rightChannel);

    // Lyrics parsing methods
    bool parseLRCFile(const QString& filePath);
    bool parseSRTFile(const QString& filePath);
    qint64 parseTimeString(const QString& timeStr) const;

    // Metadata methods
    bool readID3Metadata(const QString& filePath, AudioMetadata& metadata) const;
    bool writeID3Metadata(const QString& filePath, const AudioMetadata& metadata);
    bool readFLACMetadata(const QString& filePath, AudioMetadata& metadata) const;
    bool writeFLACMetadata(const QString& filePath, const AudioMetadata& metadata);

    // Format conversion methods
    bool convertToMP3(const QString& inputPath, const QString& outputPath, const ConversionOptions& options);
    bool convertToFLAC(const QString& inputPath, const QString& outputPath, const ConversionOptions& options);
    bool convertToWAV(const QString& inputPath, const QString& outputPath, const ConversionOptions& options);
    bool convertToOGG(const QString& inputPath, const QString& outputPath, const ConversionOptions& options);

    // Noise reduction methods
    void updateNoiseProfile(const QVector<float>& leftChannel, const QVector<float>& rightChannel);
    void applySpectralSubtraction(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void applyWienerFilter(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void performFFT(const QVector<float>& input, QVector<ComplexF>& output);
    void performIFFT(const QVector<ComplexF>& input, QVector<float>& output);

    // Audio processor integration
    AudioProcessor* m_audioProcessor;

    // Karaoke settings
    KaraokeMode m_karaokeMode;
    float m_vocalRemovalStrength;

    // Audio tracks
    QVector<AudioTrackInfo> m_availableAudioTracks;
    AudioTrackInfo m_currentAudioTrack;

    // Lyrics
    QVector<LyricsLine> m_lyricsLines;
    int m_currentLyricsIndex;

    // Format conversion
    int m_conversionProgress;
    bool m_conversionCancelled;
    QTimer* m_conversionTimer;

    // Noise reduction
    NoiseReductionSettings m_noiseReductionSettings;
    QVector<float> m_noiseProfile;
    QVector<float> m_noiseSpectrum;
    bool m_noiseProfileReady;

    // Processing state
    struct VocalRemovalState {
        QVector<float> delayLineL;
        QVector<float> delayLineR;
        int delayIndex;
        float highpassState1L, highpassState2L;
        float highpassState1R, highpassState2R;
        
        VocalRemovalState() : delayIndex(0), highpassState1L(0), highpassState2L(0),
                             highpassState1R(0), highpassState2R(0) {
            delayLineL.resize(1024);
            delayLineR.resize(1024);
            delayLineL.fill(0.0f);
            delayLineR.fill(0.0f);
        }
    } m_vocalRemovalState;

    // Thread safety
    mutable QMutex m_processingMutex;

    // Constants
    static constexpr int FFT_SIZE = 2048;
    static constexpr float VOCAL_FREQUENCY_MIN = 80.0f;   // Hz
    static constexpr float VOCAL_FREQUENCY_MAX = 8000.0f; // Hz
    static constexpr float NOISE_GATE_THRESHOLD = -60.0f; // dB
};

#endif // ADVANCEDAUDIOPROCESSOR_H
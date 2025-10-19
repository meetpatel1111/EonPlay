#ifndef AUDIOPROCESSOR_H
#define AUDIOPROCESSOR_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include <memory>

/**
 * @brief Advanced audio processing system
 * 
 * Provides advanced audio processing features including karaoke mode,
 * audio track switching, lyrics support, and audio format conversion.
 */
class AudioProcessor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Audio track information
     */
    struct AudioTrack {
        int id;
        QString language;
        QString codec;
        int channels;
        int sampleRate;
        int bitrate;
        QString description;
        bool isDefault;
        
        AudioTrack() : id(-1), channels(0), sampleRate(0), bitrate(0), isDefault(false) {}
    };

    /**
     * @brief Lyrics line structure
     */
    struct LyricsLine {
        qint64 startTime;    // Start time in milliseconds
        qint64 endTime;      // End time in milliseconds
        QString text;        // Lyrics text
        QString translation; // Optional translation
        
        LyricsLine() : startTime(0), endTime(0) {}
        LyricsLine(qint64 start, qint64 end, const QString& txt)
            : startTime(start), endTime(end), text(txt) {}
    };

    /**
     * @brief Audio format conversion options
     */
    enum AudioFormat {
        MP3,
        FLAC,
        WAV,
        OGG,
        AAC,
        WMA
    };

    /**
     * @brief Karaoke processing modes
     */
    enum KaraokeMode {
        Off,
        VocalRemoval,
        VocalIsolation,
        CenterChannelExtraction
    };

    explicit AudioProcessor(QObject* parent = nullptr);
    ~AudioProcessor() override;

    /**
     * @brief Initialize the audio processor
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the processor
     */
    void shutdown();

    /**
     * @brief Check if processor is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Enable or disable the processor
     * @param enabled Enable state
     */
    void setEnabled(bool enabled);

    // Audio Track Management
    /**
     * @brief Get available audio tracks
     * @return List of available audio tracks
     */
    QVector<AudioTrack> getAvailableAudioTracks() const;

    /**
     * @brief Get current audio track ID
     * @return Current track ID, -1 if none
     */
    int getCurrentAudioTrack() const;

    /**
     * @brief Switch to specified audio track
     * @param trackId Track ID to switch to
     * @return true if successful
     */
    bool switchAudioTrack(int trackId);

    /**
     * @brief Get audio track by ID
     * @param trackId Track ID
     * @return Audio track information
     */
    AudioTrack getAudioTrack(int trackId) const;

    // Karaoke Mode
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

    /**
     * @brief Get karaoke frequency range
     * @return Frequency range for vocal processing (Hz)
     */
    QPair<float, float> getKaraokeFrequencyRange() const;

    /**
     * @brief Set karaoke frequency range
     * @param lowFreq Low frequency cutoff (Hz)
     * @param highFreq High frequency cutoff (Hz)
     */
    void setKaraokeFrequencyRange(float lowFreq, float highFreq);

    // Lyrics Support
    /**
     * @brief Check if lyrics are available
     * @return true if lyrics are loaded
     */
    bool hasLyrics() const;

    /**
     * @brief Load lyrics from file
     * @param filePath Path to lyrics file (LRC, SRT, etc.)
     * @return true if successful
     */
    bool loadLyricsFromFile(const QString& filePath);

    /**
     * @brief Load embedded lyrics from media file
     * @param mediaPath Path to media file
     * @return true if successful
     */
    bool loadEmbeddedLyrics(const QString& mediaPath);

    /**
     * @brief Get all lyrics lines
     * @return Vector of lyrics lines
     */
    QVector<LyricsLine> getAllLyrics() const;

    /**
     * @brief Get current lyrics line for given time
     * @param timeMs Current playback time in milliseconds
     * @return Current lyrics line, empty if none
     */
    LyricsLine getCurrentLyricsLine(qint64 timeMs) const;

    /**
     * @brief Get next lyrics line
     * @param timeMs Current playback time in milliseconds
     * @return Next lyrics line, empty if none
     */
    LyricsLine getNextLyricsLine(qint64 timeMs) const;

    /**
     * @brief Clear loaded lyrics
     */
    void clearLyrics();

    /**
     * @brief Export lyrics to file
     * @param filePath Output file path
     * @param format Export format ("lrc", "srt", "txt")
     * @return true if successful
     */
    bool exportLyrics(const QString& filePath, const QString& format) const;

    // Audio Tag Editor
    /**
     * @brief Get audio metadata tags
     * @param filePath Path to audio file
     * @return Map of tag names to values
     */
    QMap<QString, QString> getAudioTags(const QString& filePath) const;

    /**
     * @brief Set audio metadata tag
     * @param filePath Path to audio file
     * @param tagName Tag name (title, artist, album, etc.)
     * @param tagValue Tag value
     * @return true if successful
     */
    bool setAudioTag(const QString& filePath, const QString& tagName, const QString& tagValue);

    /**
     * @brief Get supported tag names
     * @return List of supported tag names
     */
    static QStringList getSupportedTagNames();

    /**
     * @brief Get album art from audio file
     * @param filePath Path to audio file
     * @return Album art data, empty if none
     */
    QByteArray getAlbumArt(const QString& filePath) const;

    /**
     * @brief Set album art for audio file
     * @param filePath Path to audio file
     * @param imageData Album art image data
     * @return true if successful
     */
    bool setAlbumArt(const QString& filePath, const QByteArray& imageData);

    // Audio Format Conversion
    /**
     * @brief Check if format conversion is supported
     * @param fromFormat Source format
     * @param toFormat Target format
     * @return true if conversion is supported
     */
    static bool isConversionSupported(AudioFormat fromFormat, AudioFormat toFormat);

    /**
     * @brief Get format name
     * @param format Audio format
     * @return Format name
     */
    static QString getFormatName(AudioFormat format);

    /**
     * @brief Get format extension
     * @param format Audio format
     * @return File extension (without dot)
     */
    static QString getFormatExtension(AudioFormat format);

    /**
     * @brief Convert audio file format
     * @param inputPath Input file path
     * @param outputPath Output file path
     * @param targetFormat Target format
     * @param quality Quality level (0-100)
     * @return true if successful
     */
    bool convertAudioFormat(const QString& inputPath, const QString& outputPath, 
                           AudioFormat targetFormat, int quality = 80);

    /**
     * @brief Get conversion progress
     * @return Progress percentage (0-100)
     */
    int getConversionProgress() const;

    /**
     * @brief Cancel ongoing conversion
     */
    void cancelConversion();

    // Audio Delay Adjustment
    /**
     * @brief Get audio delay in milliseconds
     * @return Audio delay
     */
    int getAudioDelay() const;

    /**
     * @brief Set audio delay for A/V sync
     * @param delayMs Delay in milliseconds (-1000 to +1000)
     */
    void setAudioDelay(int delayMs);

    // Noise Reduction
    /**
     * @brief Check if noise reduction is enabled
     * @return true if enabled
     */
    bool isNoiseReductionEnabled() const;

    /**
     * @brief Enable or disable noise reduction
     * @param enabled Enable state
     */
    void setNoiseReductionEnabled(bool enabled);

    /**
     * @brief Get noise reduction strength
     * @return Strength level (0.0 to 1.0)
     */
    float getNoiseReductionStrength() const;

    /**
     * @brief Set noise reduction strength
     * @param strength Strength level (0.0 to 1.0)
     */
    void setNoiseReductionStrength(float strength);

signals:
    /**
     * @brief Emitted when processor is enabled/disabled
     * @param enabled New enabled state
     */
    void enabledChanged(bool enabled);

    /**
     * @brief Emitted when audio track changes
     * @param trackId New track ID
     * @param trackInfo Track information
     */
    void audioTrackChanged(int trackId, const AudioTrack& trackInfo);

    /**
     * @brief Emitted when karaoke mode changes
     * @param mode New karaoke mode
     */
    void karaokeModeChanged(KaraokeMode mode);

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
     * @brief Emitted during audio conversion
     * @param progress Progress percentage (0-100)
     */
    void conversionProgress(int progress);

    /**
     * @brief Emitted when conversion completes
     * @param success true if successful
     * @param outputPath Output file path
     */
    void conversionCompleted(bool success, const QString& outputPath);

    /**
     * @brief Emitted when audio delay changes
     * @param delayMs New delay in milliseconds
     */
    void audioDelayChanged(int delayMs);

    /**
     * @brief Emitted when noise reduction settings change
     * @param enabled Enable state
     * @param strength Strength level
     */
    void noiseReductionChanged(bool enabled, float strength);

private slots:
    void updateLyricsPosition();
    void updateConversionProgress();

private:
    bool parseLRCFile(const QString& filePath);
    bool parseSRTFile(const QString& filePath);
    void processKaraokeAudio(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void applyNoiseReduction(QVector<float>& audioData);
    AudioFormat detectAudioFormat(const QString& filePath) const;

    // Processor state
    bool m_enabled;
    QVector<AudioTrack> m_audioTracks;
    int m_currentTrackId;

    // Karaoke settings
    KaraokeMode m_karaokeMode;
    float m_vocalRemovalStrength;
    float m_karaokeLowFreq;
    float m_karaokeHighFreq;

    // Lyrics data
    QVector<LyricsLine> m_lyricsLines;
    int m_currentLyricsIndex;
    QTimer* m_lyricsTimer;

    // Audio processing
    int m_audioDelay;
    bool m_noiseReductionEnabled;
    float m_noiseReductionStrength;

    // Conversion state
    bool m_conversionInProgress;
    int m_conversionProgress;
    QTimer* m_conversionTimer;

    // Thread safety
    mutable QMutex m_dataMutex;

    // Constants
    static constexpr float DEFAULT_VOCAL_REMOVAL_STRENGTH = 0.8f;
    static constexpr float DEFAULT_KARAOKE_LOW_FREQ = 200.0f;
    static constexpr float DEFAULT_KARAOKE_HIGH_FREQ = 4000.0f;
    static constexpr float DEFAULT_NOISE_REDUCTION_STRENGTH = 0.3f;
};

#endif // AUDIOPROCESSOR_H
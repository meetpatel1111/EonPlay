#ifndef AUDIOEQUALIZER_H
#define AUDIOEQUALIZER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QSettings>
#include <QMutex>
#include <memory>

// Forward declarations
class AudioProcessor;

/**
 * @brief Audio equalizer with multi-band frequency adjustment
 * 
 * Provides a comprehensive audio equalizer system with preset profiles,
 * custom band adjustment, bass boost, treble enhancement, and ReplayGain support.
 */
class AudioEqualizer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Equalizer preset profiles
     */
    enum PresetProfile {
        Flat = 0,
        Rock,
        Jazz,
        Pop,
        Classical,
        Electronic,
        Vocal,
        Bass,
        Treble,
        Custom
    };

    /**
     * @brief Equalizer band information
     */
    struct EqualizerBand {
        double frequency;    // Center frequency in Hz
        double gain;        // Gain in dB (-20 to +20)
        double bandwidth;   // Bandwidth (Q factor)
        
        EqualizerBand(double freq = 1000.0, double g = 0.0, double bw = 1.0)
            : frequency(freq), gain(g), bandwidth(bw) {}
    };

    explicit AudioEqualizer(QObject* parent = nullptr);
    ~AudioEqualizer() override;

    /**
     * @brief Initialize the equalizer
     * @param audioProcessor Optional audio processor for integration
     * @return true if initialization was successful
     */
    bool initialize(AudioProcessor* audioProcessor = nullptr);

    /**
     * @brief Shutdown the equalizer
     */
    void shutdown();

    /**
     * @brief Check if equalizer is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Enable or disable the equalizer
     * @param enabled Enable state
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get the number of equalizer bands
     * @return Number of bands
     */
    int getBandCount() const;

    /**
     * @brief Get equalizer band information
     * @param bandIndex Band index (0-based)
     * @return Band information
     */
    EqualizerBand getBand(int bandIndex) const;

    /**
     * @brief Set equalizer band gain
     * @param bandIndex Band index (0-based)
     * @param gain Gain in dB (-20 to +20)
     */
    void setBandGain(int bandIndex, double gain);

    /**
     * @brief Get all band gains
     * @return Vector of gain values in dB
     */
    QVector<double> getAllBandGains() const;

    /**
     * @brief Set all band gains
     * @param gains Vector of gain values in dB
     */
    void setAllBandGains(const QVector<double>& gains);

    /**
     * @brief Get current preset profile
     * @return Current preset
     */
    PresetProfile getCurrentPreset() const;

    /**
     * @brief Apply a preset profile
     * @param preset Preset to apply
     */
    void applyPreset(PresetProfile preset);

    /**
     * @brief Get preset name
     * @param preset Preset profile
     * @return Preset name
     */
    static QString getPresetName(PresetProfile preset);

    /**
     * @brief Get all available presets
     * @return List of preset profiles
     */
    static QVector<PresetProfile> getAvailablePresets();

    /**
     * @brief Get bass boost level
     * @return Bass boost in dB (0 to +12)
     */
    double getBassBoost() const;

    /**
     * @brief Set bass boost level
     * @param boost Bass boost in dB (0 to +12)
     */
    void setBassBoost(double boost);

    /**
     * @brief Get treble enhancement level
     * @return Treble enhancement in dB (0 to +12)
     */
    double getTrebleEnhancement() const;

    /**
     * @brief Set treble enhancement level
     * @param enhancement Treble enhancement in dB (0 to +12)
     */
    void setTrebleEnhancement(double enhancement);

    /**
     * @brief Check if 3D surround sound is enabled
     * @return true if enabled
     */
    bool is3DSurroundEnabled() const;

    /**
     * @brief Enable or disable 3D surround sound simulation
     * @param enabled Enable state
     */
    void set3DSurroundEnabled(bool enabled);

    /**
     * @brief Get 3D surround sound strength
     * @return Strength level (0.0 to 1.0)
     */
    double get3DSurroundStrength() const;

    /**
     * @brief Set 3D surround sound strength
     * @param strength Strength level (0.0 to 1.0)
     */
    void set3DSurroundStrength(double strength);

    /**
     * @brief Check if ReplayGain is enabled
     * @return true if enabled
     */
    bool isReplayGainEnabled() const;

    /**
     * @brief Enable or disable ReplayGain
     * @param enabled Enable state
     */
    void setReplayGainEnabled(bool enabled);

    /**
     * @brief Get ReplayGain mode
     * @return ReplayGain mode (track or album)
     */
    QString getReplayGainMode() const;

    /**
     * @brief Set ReplayGain mode
     * @param mode ReplayGain mode ("track" or "album")
     */
    void setReplayGainMode(const QString& mode);

    /**
     * @brief Get ReplayGain preamp
     * @return Preamp value in dB
     */
    double getReplayGainPreamp() const;

    /**
     * @brief Set ReplayGain preamp
     * @param preamp Preamp value in dB
     */
    void setReplayGainPreamp(double preamp);

    /**
     * @brief Save current equalizer settings
     */
    void saveSettings();

    /**
     * @brief Load equalizer settings
     */
    void loadSettings();

    /**
     * @brief Reset equalizer to default settings
     */
    void resetToDefaults();

    /**
     * @brief Export equalizer settings to file
     * @param filePath File path to export to
     * @return true if successful
     */
    bool exportSettings(const QString& filePath) const;

    /**
     * @brief Import equalizer settings from file
     * @param filePath File path to import from
     * @return true if successful
     */
    bool importSettings(const QString& filePath);

    /**
     * @brief Process audio buffer through equalizer
     * @param leftChannel Left audio channel data
     * @param rightChannel Right audio channel data
     * @param sampleRate Sample rate in Hz
     */
    void processAudio(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate);

    /**
     * @brief Get frequency response at given frequency
     * @param frequency Frequency in Hz
     * @return Gain response in dB
     */
    double getFrequencyResponse(double frequency) const;

    /**
     * @brief Analyze audio spectrum for auto-EQ suggestions
     * @param leftChannel Left audio channel data
     * @param rightChannel Right audio channel data
     * @param sampleRate Sample rate in Hz
     * @return Suggested EQ adjustments
     */
    QVector<double> analyzeAndSuggestEQ(const QVector<float>& leftChannel, 
                                       const QVector<float>& rightChannel, 
                                       int sampleRate) const;

signals:
    /**
     * @brief Emitted when equalizer is enabled/disabled
     * @param enabled New enabled state
     */
    void enabledChanged(bool enabled);

    /**
     * @brief Emitted when a band gain changes
     * @param bandIndex Band index
     * @param gain New gain value
     */
    void bandGainChanged(int bandIndex, double gain);

    /**
     * @brief Emitted when preset changes
     * @param preset New preset
     */
    void presetChanged(PresetProfile preset);

    /**
     * @brief Emitted when bass boost changes
     * @param boost New bass boost value
     */
    void bassBoostChanged(double boost);

    /**
     * @brief Emitted when treble enhancement changes
     * @param enhancement New treble enhancement value
     */
    void trebleEnhancementChanged(double enhancement);

    /**
     * @brief Emitted when 3D surround settings change
     * @param enabled Enable state
     * @param strength Strength level
     */
    void surroundChanged(bool enabled, double strength);

    /**
     * @brief Emitted when ReplayGain settings change
     * @param enabled Enable state
     * @param mode ReplayGain mode
     * @param preamp Preamp value
     */
    void replayGainChanged(bool enabled, const QString& mode, double preamp);

private:
    void initializeBands();
    void applyEqualizerSettings();
    QVector<double> getPresetGains(PresetProfile preset) const;
    void clampGainValue(double& gain) const;
    
    // Audio processing methods
    void initializeFilters(int sampleRate);
    float processSample(float sample, int channel, int bandIndex);
    void updateFilterCoefficients(int bandIndex, double frequency, double gain, double q);
    void apply3DSurround(QVector<float>& leftChannel, QVector<float>& rightChannel);
    void applyReplayGain(QVector<float>& leftChannel, QVector<float>& rightChannel, double gain);
    
    // Analysis methods
    void performFFT(const QVector<float>& input, QVector<float>& magnitude, QVector<float>& phase) const;
    double calculateRMS(const QVector<float>& samples) const;

    // Equalizer state
    bool m_enabled;
    QVector<EqualizerBand> m_bands;
    PresetProfile m_currentPreset;

    // Audio enhancements
    double m_bassBoost;
    double m_trebleEnhancement;
    bool m_3dSurroundEnabled;
    double m_3dSurroundStrength;

    // ReplayGain
    bool m_replayGainEnabled;
    QString m_replayGainMode;
    double m_replayGainPreamp;

    // Settings
    std::unique_ptr<QSettings> m_settings;
    
    // Audio processing
    AudioProcessor* m_audioProcessor;
    int m_sampleRate;
    
    // Filter states for each band and channel
    struct FilterState {
        double b0, b1, b2, a1, a2;  // Biquad coefficients
        double x1, x2, y1, y2;      // Delay line states
        FilterState() : b0(1), b1(0), b2(0), a1(0), a2(0), x1(0), x2(0), y1(0), y2(0) {}
    };
    
    QVector<QVector<FilterState>> m_filterStates; // [band][channel]
    
    // 3D Surround processing state
    struct SurroundState {
        float delayLineL[1024];
        float delayLineR[1024];
        int delayIndex;
        float crossfeedGain;
        SurroundState() : delayIndex(0), crossfeedGain(0.3f) {
            std::fill(std::begin(delayLineL), std::end(delayLineL), 0.0f);
            std::fill(std::begin(delayLineR), std::end(delayLineR), 0.0f);
        }
    } m_surroundState;
    
    // Thread safety
    mutable QMutex m_processingMutex;
    
    // Constants
    static constexpr int DEFAULT_BAND_COUNT = 10;
    static constexpr double MIN_GAIN = -20.0;
    static constexpr double MAX_GAIN = 20.0;
    static constexpr double MIN_BASS_BOOST = 0.0;
    static constexpr double MAX_BASS_BOOST = 12.0;
    static constexpr double MIN_TREBLE_ENHANCEMENT = 0.0;
    static constexpr double MAX_TREBLE_ENHANCEMENT = 12.0;
    static constexpr double PI = 3.14159265359;
    static constexpr int MAX_CHANNELS = 2;
};

#endif // AUDIOEQUALIZER_H
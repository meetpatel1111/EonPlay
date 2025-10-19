#ifndef AUDIOVISUALIZER_H
#define AUDIOVISUALIZER_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include <memory>

/**
 * @brief Audio visualization system with spectrum analyzer and waveform display
 * 
 * Provides real-time audio visualization including spectrum analysis,
 * waveform display, and music-driven animations for the media player.
 */
class AudioVisualizer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Visualization modes
     */
    enum VisualizationMode {
        None = 0,
        SpectrumAnalyzer,
        Waveform,
        Oscilloscope,
        VUMeter,
        FrequencyBars,
        CircularSpectrum,
        AIVisualizer
    };

    /**
     * @brief Audio sample data structure
     */
    struct AudioSample {
        QVector<float> leftChannel;
        QVector<float> rightChannel;
        int sampleRate;
        qint64 timestamp;
        
        AudioSample() : sampleRate(44100), timestamp(0) {}
    };

    /**
     * @brief Spectrum analysis data
     */
    struct SpectrumData {
        QVector<float> frequencies;     // Frequency values in Hz
        QVector<float> magnitudes;      // Magnitude values (0.0 to 1.0)
        QVector<float> phases;          // Phase values
        float peakFrequency;            // Peak frequency
        float peakMagnitude;            // Peak magnitude
        
        SpectrumData() : peakFrequency(0.0f), peakMagnitude(0.0f) {}
    };

    explicit AudioVisualizer(QObject* parent = nullptr);
    ~AudioVisualizer() override;

    /**
     * @brief Initialize the audio visualizer
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the visualizer
     */
    void shutdown();

    /**
     * @brief Check if visualizer is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Enable or disable the visualizer
     * @param enabled Enable state
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get current visualization mode
     * @return Current mode
     */
    VisualizationMode getVisualizationMode() const;

    /**
     * @brief Set visualization mode
     * @param mode Visualization mode
     */
    void setVisualizationMode(VisualizationMode mode);

    /**
     * @brief Get visualization mode name
     * @param mode Visualization mode
     * @return Mode name
     */
    static QString getModeName(VisualizationMode mode);

    /**
     * @brief Get all available visualization modes
     * @return List of modes
     */
    static QVector<VisualizationMode> getAvailableModes();

    /**
     * @brief Get spectrum analyzer band count
     * @return Number of frequency bands
     */
    int getSpectrumBandCount() const;

    /**
     * @brief Set spectrum analyzer band count
     * @param bandCount Number of bands (8, 16, 32, 64, 128, 256)
     */
    void setSpectrumBandCount(int bandCount);

    /**
     * @brief Get update rate in Hz
     * @return Update rate
     */
    int getUpdateRate() const;

    /**
     * @brief Set update rate
     * @param rate Update rate in Hz (10-60)
     */
    void setUpdateRate(int rate);

    /**
     * @brief Get sensitivity level
     * @return Sensitivity (0.0 to 1.0)
     */
    float getSensitivity() const;

    /**
     * @brief Set sensitivity level
     * @param sensitivity Sensitivity (0.0 to 1.0)
     */
    void setSensitivity(float sensitivity);

    /**
     * @brief Check if smoothing is enabled
     * @return true if enabled
     */
    bool isSmoothingEnabled() const;

    /**
     * @brief Enable or disable smoothing
     * @param enabled Enable state
     */
    void setSmoothingEnabled(bool enabled);

    /**
     * @brief Get smoothing factor
     * @return Smoothing factor (0.0 to 1.0)
     */
    float getSmoothingFactor() const;

    /**
     * @brief Set smoothing factor
     * @param factor Smoothing factor (0.0 to 1.0)
     */
    void setSmoothingFactor(float factor);

    /**
     * @brief Check if peak hold is enabled
     * @return true if enabled
     */
    bool isPeakHoldEnabled() const;

    /**
     * @brief Enable or disable peak hold
     * @param enabled Enable state
     */
    void setPeakHoldEnabled(bool enabled);

    /**
     * @brief Get peak hold time in milliseconds
     * @return Peak hold time
     */
    int getPeakHoldTime() const;

    /**
     * @brief Set peak hold time
     * @param timeMs Peak hold time in milliseconds
     */
    void setPeakHoldTime(int timeMs);

    /**
     * @brief Process audio sample data
     * @param sample Audio sample data
     */
    void processAudioSample(const AudioSample& sample);

    /**
     * @brief Get current spectrum data
     * @return Spectrum analysis data
     */
    SpectrumData getCurrentSpectrum() const;

    /**
     * @brief Get current waveform data
     * @return Waveform data for left and right channels
     */
    QPair<QVector<float>, QVector<float>> getCurrentWaveform() const;

    /**
     * @brief Get current VU meter levels
     * @return VU levels for left and right channels (0.0 to 1.0)
     */
    QPair<float, float> getVULevels() const;

    /**
     * @brief Get current peak levels
     * @return Peak levels for left and right channels (0.0 to 1.0)
     */
    QPair<float, float> getPeakLevels() const;

    /**
     * @brief Check if AI visualizer is available
     * @return true if AI features are available
     */
    bool isAIVisualizerAvailable() const;

    /**
     * @brief Get AI visualizer mood detection
     * @return Detected mood string
     */
    QString getDetectedMood() const;

    /**
     * @brief Get AI visualizer color palette
     * @return Color palette based on audio analysis
     */
    QVector<QColor> getAIColorPalette() const;

    /**
     * @brief Get beat detection information
     * @return Beat detection data
     */
    struct BeatInfo {
        bool isBeat;           // Beat detected
        float strength;        // Beat strength (0.0 to 1.0)
        float bpm;            // Estimated BPM
        qint64 lastBeatTime;  // Last beat timestamp
        
        BeatInfo() : isBeat(false), strength(0.0f), bpm(0.0f), lastBeatTime(0) {}
    };

    /**
     * @brief Get current beat information
     * @return Beat detection data
     */
    BeatInfo getCurrentBeatInfo() const;

    /**
     * @brief Get music-driven animation parameters
     * @return Animation parameters based on audio analysis
     */
    struct AnimationParams {
        float energy;          // Overall energy level (0.0 to 1.0)
        float bassEnergy;      // Bass energy level (0.0 to 1.0)
        float midEnergy;       // Mid-range energy level (0.0 to 1.0)
        float trebleEnergy;    // Treble energy level (0.0 to 1.0)
        float dynamicRange;    // Dynamic range (0.0 to 1.0)
        QVector<float> bandEnergies; // Per-band energy levels
        
        AnimationParams() : energy(0.0f), bassEnergy(0.0f), midEnergy(0.0f), 
                           trebleEnergy(0.0f), dynamicRange(0.0f) {}
    };

    /**
     * @brief Get current animation parameters
     * @return Animation parameters
     */
    AnimationParams getCurrentAnimationParams() const;

signals:
    /**
     * @brief Emitted when visualizer is enabled/disabled
     * @param enabled New enabled state
     */
    void enabledChanged(bool enabled);

    /**
     * @brief Emitted when visualization mode changes
     * @param mode New visualization mode
     */
    void visualizationModeChanged(VisualizationMode mode);

    /**
     * @brief Emitted when spectrum data is updated
     * @param spectrum New spectrum data
     */
    void spectrumDataUpdated(const SpectrumData& spectrum);

    /**
     * @brief Emitted when waveform data is updated
     * @param leftChannel Left channel waveform
     * @param rightChannel Right channel waveform
     */
    void waveformDataUpdated(const QVector<float>& leftChannel, const QVector<float>& rightChannel);

    /**
     * @brief Emitted when VU levels are updated
     * @param leftLevel Left channel VU level
     * @param rightLevel Right channel VU level
     */
    void vuLevelsUpdated(float leftLevel, float rightLevel);

    /**
     * @brief Emitted when peak levels are updated
     * @param leftPeak Left channel peak level
     * @param rightPeak Right channel peak level
     */
    void peakLevelsUpdated(float leftPeak, float rightPeak);

    /**
     * @brief Emitted when AI mood detection updates
     * @param mood Detected mood
     * @param confidence Confidence level (0.0 to 1.0)
     */
    void moodDetected(const QString& mood, float confidence);

    /**
     * @brief Emitted when AI color palette updates
     * @param colors New color palette
     */
    void colorPaletteUpdated(const QVector<QColor>& colors);

    /**
     * @brief Emitted when beat is detected
     * @param beatInfo Beat information
     */
    void beatDetected(const BeatInfo& beatInfo);

    /**
     * @brief Emitted when animation parameters update
     * @param params Animation parameters
     */
    void animationParamsUpdated(const AnimationParams& params);

private slots:
    void updateVisualization();
    void updatePeakHold();

private:
    void performFFT(const QVector<float>& input, QVector<float>& magnitudes, QVector<float>& phases);
    void calculateVULevels(const QVector<float>& leftChannel, const QVector<float>& rightChannel);
    void updatePeakLevels(float leftLevel, float rightLevel);
    void applySmoothingToSpectrum(QVector<float>& magnitudes);
    void detectMoodFromSpectrum(const SpectrumData& spectrum);
    void generateAIColorPalette(const SpectrumData& spectrum);
    void detectBeat(const SpectrumData& spectrum);
    void calculateAnimationParams(const SpectrumData& spectrum);
    void updateBPMEstimation();
    float calculateRMS(const QVector<float>& samples);
    float calculatePeak(const QVector<float>& samples);
    float calculateSpectralCentroid(const SpectrumData& spectrum) const;
    float calculateSpectralRolloff(const SpectrumData& spectrum) const;

    // Visualization state
    bool m_enabled;
    VisualizationMode m_visualizationMode;
    int m_spectrumBandCount;
    int m_updateRate;
    float m_sensitivity;
    bool m_smoothingEnabled;
    float m_smoothingFactor;
    bool m_peakHoldEnabled;
    int m_peakHoldTime;

    // Audio data
    AudioSample m_currentSample;
    SpectrumData m_currentSpectrum;
    QVector<float> m_leftWaveform;
    QVector<float> m_rightWaveform;
    
    // VU and peak levels
    float m_leftVULevel;
    float m_rightVULevel;
    float m_leftPeakLevel;
    float m_rightPeakLevel;
    qint64 m_leftPeakTime;
    qint64 m_rightPeakTime;

    // Smoothing buffers
    QVector<float> m_smoothedMagnitudes;
    QVector<float> m_previousMagnitudes;

    // AI features
    QString m_detectedMood;
    float m_moodConfidence;
    QVector<QColor> m_aiColorPalette;

    // Beat detection
    BeatInfo m_currentBeatInfo;
    QVector<float> m_beatHistory;
    QVector<qint64> m_beatTimes;
    float m_lastEnergy;
    float m_averageEnergy;

    // Animation parameters
    AnimationParams m_animationParams;
    QVector<float> m_energyHistory;

    // Timers
    QTimer* m_updateTimer;
    QTimer* m_peakHoldTimer;

    // Thread safety
    mutable QMutex m_dataMutex;

    // Constants
    static constexpr int DEFAULT_BAND_COUNT = 32;
    static constexpr int DEFAULT_UPDATE_RATE = 30;
    static constexpr float DEFAULT_SENSITIVITY = 0.5f;
    static constexpr float DEFAULT_SMOOTHING_FACTOR = 0.3f;
    static constexpr int DEFAULT_PEAK_HOLD_TIME = 1000;
    static constexpr int FFT_SIZE = 1024;
    static constexpr int WAVEFORM_SIZE = 256;
};

#endif // AUDIOVISUALIZER_H
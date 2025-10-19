#ifndef AUDIOOUTPUTMANAGER_H
#define AUDIOOUTPUTMANAGER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QMutex>
#include <memory>

/**
 * @brief Audio output device management system
 * 
 * Provides comprehensive audio output device selection, spatial sound simulation,
 * and A/V synchronization features for the media player.
 */
class AudioOutputManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Audio output device information
     */
    struct AudioDeviceInfo {
        QString id;                    // Device ID
        QString name;                  // Display name
        QString description;           // Device description
        bool isDefault;               // Is default device
        int maxChannels;              // Maximum supported channels
        QVector<int> supportedRates;  // Supported sample rates
        bool isAvailable;             // Device availability
        
        AudioDeviceInfo() : isDefault(false), maxChannels(2), isAvailable(false) {}
    };

    /**
     * @brief Spatial sound modes
     */
    enum SpatialSoundMode {
        None = 0,
        Headphones,
        Speakers_2_1,
        Speakers_5_1,
        Speakers_7_1,
        Custom
    };

    /**
     * @brief Audio delay compensation
     */
    struct DelayCompensation {
        int audioDelayMs;      // Audio delay in milliseconds (-500 to +500)
        int videoDelayMs;      // Video delay in milliseconds (-500 to +500)
        bool autoSync;         // Automatic A/V sync detection
        float syncTolerance;   // Sync tolerance in milliseconds
        
        DelayCompensation() : audioDelayMs(0), videoDelayMs(0), autoSync(false), syncTolerance(40.0f) {}
    };

    explicit AudioOutputManager(QObject* parent = nullptr);
    ~AudioOutputManager() override;

    /**
     * @brief Initialize the audio output manager
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the manager
     */
    void shutdown();

    /**
     * @brief Get all available audio output devices
     * @return List of audio devices
     */
    QVector<AudioDeviceInfo> getAvailableDevices() const;

    /**
     * @brief Get current audio output device
     * @return Current device info
     */
    AudioDeviceInfo getCurrentDevice() const;

    /**
     * @brief Set audio output device
     * @param deviceId Device ID to set
     * @return true if successful
     */
    bool setAudioDevice(const QString& deviceId);

    /**
     * @brief Get default audio device
     * @return Default device info
     */
    AudioDeviceInfo getDefaultDevice() const;

    /**
     * @brief Refresh device list
     */
    void refreshDevices();

    /**
     * @brief Get current spatial sound mode
     * @return Current spatial sound mode
     */
    SpatialSoundMode getSpatialSoundMode() const;

    /**
     * @brief Set spatial sound mode
     * @param mode Spatial sound mode
     */
    void setSpatialSoundMode(SpatialSoundMode mode);

    /**
     * @brief Get spatial sound mode name
     * @param mode Spatial sound mode
     * @return Mode name
     */
    static QString getSpatialSoundModeName(SpatialSoundMode mode);

    /**
     * @brief Check if headphone spatial sound is enabled
     * @return true if enabled
     */
    bool isHeadphoneSpatialSoundEnabled() const;

    /**
     * @brief Enable or disable headphone spatial sound
     * @param enabled Enable state
     */
    void setHeadphoneSpatialSoundEnabled(bool enabled);

    /**
     * @brief Get headphone spatial sound strength
     * @return Strength level (0.0 to 1.0)
     */
    float getHeadphoneSpatialStrength() const;

    /**
     * @brief Set headphone spatial sound strength
     * @param strength Strength level (0.0 to 1.0)
     */
    void setHeadphoneSpatialStrength(float strength);

    /**
     * @brief Get room size for spatial simulation
     * @return Room size (0.0 to 1.0)
     */
    float getRoomSize() const;

    /**
     * @brief Set room size for spatial simulation
     * @param size Room size (0.0 to 1.0)
     */
    void setRoomSize(float size);

    /**
     * @brief Get delay compensation settings
     * @return Current delay compensation
     */
    DelayCompensation getDelayCompensation() const;

    /**
     * @brief Set delay compensation settings
     * @param compensation Delay compensation settings
     */
    void setDelayCompensation(const DelayCompensation& compensation);

    /**
     * @brief Get audio delay in milliseconds
     * @return Audio delay
     */
    int getAudioDelay() const;

    /**
     * @brief Set audio delay in milliseconds
     * @param delayMs Audio delay (-500 to +500)
     */
    void setAudioDelay(int delayMs);

    /**
     * @brief Get video delay in milliseconds
     * @return Video delay
     */
    int getVideoDelay() const;

    /**
     * @brief Set video delay in milliseconds
     * @param delayMs Video delay (-500 to +500)
     */
    void setVideoDelay(int delayMs);

    /**
     * @brief Check if auto A/V sync is enabled
     * @return true if enabled
     */
    bool isAutoSyncEnabled() const;

    /**
     * @brief Enable or disable auto A/V sync
     * @param enabled Enable state
     */
    void setAutoSyncEnabled(bool enabled);

    /**
     * @brief Detect A/V sync offset
     * @return Detected offset in milliseconds
     */
    float detectSyncOffset();

    /**
     * @brief Apply spatial sound processing to audio buffer
     * @param leftChannel Left audio channel
     * @param rightChannel Right audio channel
     * @param sampleRate Sample rate
     */
    void processSpatialSound(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate);

    /**
     * @brief Apply delay compensation to audio buffer
     * @param leftChannel Left audio channel
     * @param rightChannel Right audio channel
     * @param sampleRate Sample rate
     */
    void applyDelayCompensation(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate);

    /**
     * @brief Get current audio latency
     * @return Latency in milliseconds
     */
    float getCurrentLatency() const;

    /**
     * @brief Test audio device
     * @param deviceId Device ID to test
     * @return true if device is working
     */
    bool testAudioDevice(const QString& deviceId);

signals:
    /**
     * @brief Emitted when device list changes
     */
    void devicesChanged();

    /**
     * @brief Emitted when current device changes
     * @param device New current device
     */
    void currentDeviceChanged(const AudioDeviceInfo& device);

    /**
     * @brief Emitted when spatial sound mode changes
     * @param mode New spatial sound mode
     */
    void spatialSoundModeChanged(SpatialSoundMode mode);

    /**
     * @brief Emitted when delay compensation changes
     * @param compensation New delay compensation
     */
    void delayCompensationChanged(const DelayCompensation& compensation);

    /**
     * @brief Emitted when A/V sync is detected
     * @param offsetMs Detected offset in milliseconds
     */
    void syncOffsetDetected(float offsetMs);

private slots:
    void onDevicesChanged();

private:
    void updateDeviceList();
    AudioDeviceInfo createDeviceInfo(const QAudioDevice& device) const;
    void initializeSpatialProcessing();
    void applyCrossfeed(QVector<float>& leftChannel, QVector<float>& rightChannel, float strength);
    void applyRoomSimulation(QVector<float>& leftChannel, QVector<float>& rightChannel, float roomSize);
    void applyHRTF(QVector<float>& leftChannel, QVector<float>& rightChannel);

    // Device management
    QVector<AudioDeviceInfo> m_availableDevices;
    AudioDeviceInfo m_currentDevice;
    QMediaDevices* m_mediaDevices;

    // Spatial sound settings
    SpatialSoundMode m_spatialSoundMode;
    bool m_headphoneSpatialEnabled;
    float m_spatialStrength;
    float m_roomSize;

    // Delay compensation
    DelayCompensation m_delayCompensation;

    // Spatial processing state
    struct SpatialState {
        QVector<float> delayLineL;
        QVector<float> delayLineR;
        int delayIndex;
        float crossfeedGain;
        QVector<float> roomReflections;
        
        SpatialState() : delayIndex(0), crossfeedGain(0.3f) {
            delayLineL.resize(4410); // 100ms at 44.1kHz
            delayLineR.resize(4410);
            roomReflections.resize(8820); // 200ms reflections
            delayLineL.fill(0.0f);
            delayLineR.fill(0.0f);
            roomReflections.fill(0.0f);
        }
    } m_spatialState;

    // Thread safety
    mutable QMutex m_deviceMutex;

    // Constants
    static constexpr int MIN_DELAY_MS = -500;
    static constexpr int MAX_DELAY_MS = 500;
    static constexpr float DEFAULT_SYNC_TOLERANCE = 40.0f;
    static constexpr int MAX_DELAY_SAMPLES = 22050; // 500ms at 44.1kHz
};

#endif // AUDIOOUTPUTMANAGER_H
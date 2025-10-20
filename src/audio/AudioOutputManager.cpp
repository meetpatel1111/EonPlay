#include "audio/AudioOutputManager.h"
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QtMath>
#include <QTimer>
#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(audioOutputManager)
Q_LOGGING_CATEGORY(audioOutputManager, "audio.output")

AudioOutputManager::AudioOutputManager(QObject* parent)
    : QObject(parent)
    , m_mediaDevices(new QMediaDevices(this))
    , m_spatialSoundMode(None)
    , m_headphoneSpatialEnabled(false)
    , m_spatialStrength(0.5f)
    , m_roomSize(0.5f)
{
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, &AudioOutputManager::onDevicesChanged);
    
    qCDebug(audioOutputManager) << "AudioOutputManager created";
}

AudioOutputManager::~AudioOutputManager()
{
    shutdown();
    qCDebug(audioOutputManager) << "AudioOutputManager destroyed";
}

bool AudioOutputManager::initialize()
{
    updateDeviceList();
    initializeSpatialProcessing();
    
    qCDebug(audioOutputManager) << "AudioOutputManager initialized with" 
                               << m_availableDevices.size() << "devices";
    return true;
}

void AudioOutputManager::shutdown()
{
    qCDebug(audioOutputManager) << "AudioOutputManager shutdown";
}

QVector<AudioOutputManager::AudioDeviceInfo> AudioOutputManager::getAvailableDevices() const
{
    QMutexLocker locker(&m_deviceMutex);
    return m_availableDevices;
}

AudioOutputManager::AudioDeviceInfo AudioOutputManager::getCurrentDevice() const
{
    QMutexLocker locker(&m_deviceMutex);
    return m_currentDevice;
}

bool AudioOutputManager::setAudioDevice(const QString& deviceId)
{
    QMutexLocker locker(&m_deviceMutex);
    
    for (const auto& device : m_availableDevices) {
        if (device.id == deviceId) {
            m_currentDevice = device;
            emit currentDeviceChanged(m_currentDevice);
            qCDebug(audioOutputManager) << "Audio device set to:" << device.name;
            return true;
        }
    }
    
    qCWarning(audioOutputManager) << "Audio device not found:" << deviceId;
    return false;
}

AudioOutputManager::AudioDeviceInfo AudioOutputManager::getDefaultDevice() const
{
    QMutexLocker locker(&m_deviceMutex);
    
    for (const auto& device : m_availableDevices) {
        if (device.isDefault) {
            return device;
        }
    }
    
    // Return first available device if no default found
    if (!m_availableDevices.isEmpty()) {
        return m_availableDevices.first();
    }
    
    return AudioDeviceInfo();
}

void AudioOutputManager::refreshDevices()
{
    updateDeviceList();
    emit devicesChanged();
    qCDebug(audioOutputManager) << "Device list refreshed";
}

AudioOutputManager::SpatialSoundMode AudioOutputManager::getSpatialSoundMode() const
{
    return m_spatialSoundMode;
}

void AudioOutputManager::setSpatialSoundMode(SpatialSoundMode mode)
{
    if (m_spatialSoundMode != mode) {
        m_spatialSoundMode = mode;
        
        // Auto-enable headphone spatial sound for headphone mode
        if (mode == Headphones) {
            m_headphoneSpatialEnabled = true;
        }
        
        emit spatialSoundModeChanged(m_spatialSoundMode);
        qCDebug(audioOutputManager) << "Spatial sound mode set to:" << getSpatialSoundModeName(mode);
    }
}

QString AudioOutputManager::getSpatialSoundModeName(SpatialSoundMode mode)
{
    switch (mode) {
        case None: return "None";
        case Headphones: return "Headphones";
        case Speakers_2_1: return "2.1 Speakers";
        case Speakers_5_1: return "5.1 Speakers";
        case Speakers_7_1: return "7.1 Speakers";
        case Custom: return "Custom";
        default: return "Unknown";
    }
}

bool AudioOutputManager::isHeadphoneSpatialSoundEnabled() const
{
    return m_headphoneSpatialEnabled;
}

void AudioOutputManager::setHeadphoneSpatialSoundEnabled(bool enabled)
{
    if (m_headphoneSpatialEnabled != enabled) {
        m_headphoneSpatialEnabled = enabled;
        qCDebug(audioOutputManager) << "Headphone spatial sound" << (enabled ? "enabled" : "disabled");
    }
}

float AudioOutputManager::getHeadphoneSpatialStrength() const
{
    return m_spatialStrength;
}

void AudioOutputManager::setHeadphoneSpatialStrength(float strength)
{
    strength = qBound(0.0f, strength, 1.0f);
    
    if (m_spatialStrength != strength) {
        m_spatialStrength = strength;
        qCDebug(audioOutputManager) << "Spatial strength set to:" << strength;
    }
}

float AudioOutputManager::getRoomSize() const
{
    return m_roomSize;
}

void AudioOutputManager::setRoomSize(float size)
{
    size = qBound(0.0f, size, 1.0f);
    
    if (m_roomSize != size) {
        m_roomSize = size;
        qCDebug(audioOutputManager) << "Room size set to:" << size;
    }
}

AudioOutputManager::DelayCompensation AudioOutputManager::getDelayCompensation() const
{
    return m_delayCompensation;
}

void AudioOutputManager::setDelayCompensation(const DelayCompensation& compensation)
{
    m_delayCompensation = compensation;
    
    // Clamp delay values
    m_delayCompensation.audioDelayMs = qBound(MIN_DELAY_MS, compensation.audioDelayMs, MAX_DELAY_MS);
    m_delayCompensation.videoDelayMs = qBound(MIN_DELAY_MS, compensation.videoDelayMs, MAX_DELAY_MS);
    
    emit delayCompensationChanged(m_delayCompensation);
    qCDebug(audioOutputManager) << "Delay compensation updated - Audio:" << m_delayCompensation.audioDelayMs
                               << "ms, Video:" << m_delayCompensation.videoDelayMs << "ms";
}

int AudioOutputManager::getAudioDelay() const
{
    return m_delayCompensation.audioDelayMs;
}

void AudioOutputManager::setAudioDelay(int delayMs)
{
    delayMs = qBound(MIN_DELAY_MS, delayMs, MAX_DELAY_MS);
    
    if (m_delayCompensation.audioDelayMs != delayMs) {
        m_delayCompensation.audioDelayMs = delayMs;
        emit delayCompensationChanged(m_delayCompensation);
        qCDebug(audioOutputManager) << "Audio delay set to:" << delayMs << "ms";
    }
}

int AudioOutputManager::getVideoDelay() const
{
    return m_delayCompensation.videoDelayMs;
}

void AudioOutputManager::setVideoDelay(int delayMs)
{
    delayMs = qBound(MIN_DELAY_MS, delayMs, MAX_DELAY_MS);
    
    if (m_delayCompensation.videoDelayMs != delayMs) {
        m_delayCompensation.videoDelayMs = delayMs;
        emit delayCompensationChanged(m_delayCompensation);
        qCDebug(audioOutputManager) << "Video delay set to:" << delayMs << "ms";
    }
}

bool AudioOutputManager::isAutoSyncEnabled() const
{
    return m_delayCompensation.autoSync;
}

void AudioOutputManager::setAutoSyncEnabled(bool enabled)
{
    if (m_delayCompensation.autoSync != enabled) {
        m_delayCompensation.autoSync = enabled;
        emit delayCompensationChanged(m_delayCompensation);
        qCDebug(audioOutputManager) << "Auto sync" << (enabled ? "enabled" : "disabled");
    }
}

float AudioOutputManager::detectSyncOffset()
{
    // Simplified sync detection - in a real implementation this would
    // analyze audio and video streams to detect timing differences
    float detectedOffset = 0.0f;
    
    // Simulate detection process
    // This would involve cross-correlation analysis of audio/video signals
    
    if (qAbs(detectedOffset) > m_delayCompensation.syncTolerance) {
        emit syncOffsetDetected(detectedOffset);
        qCDebug(audioOutputManager) << "Sync offset detected:" << detectedOffset << "ms";
    }
    
    return detectedOffset;
}

void AudioOutputManager::processSpatialSound(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate)
{
    if (m_spatialSoundMode == None || (!m_headphoneSpatialEnabled && m_spatialSoundMode == Headphones)) {
        return;
    }
    
    switch (m_spatialSoundMode) {
        case Headphones:
            applyCrossfeed(leftChannel, rightChannel, m_spatialStrength);
            applyHRTF(leftChannel, rightChannel);
            break;
            
        case Speakers_2_1:
        case Speakers_5_1:
        case Speakers_7_1:
            applyRoomSimulation(leftChannel, rightChannel, m_roomSize);
            break;
            
        case Custom:
            applyCrossfeed(leftChannel, rightChannel, m_spatialStrength * 0.5f);
            applyRoomSimulation(leftChannel, rightChannel, m_roomSize);
            break;
            
        case None:
        default:
            break;
    }
}

void AudioOutputManager::applyDelayCompensation(QVector<float>& leftChannel, QVector<float>& rightChannel, int sampleRate)
{
    if (m_delayCompensation.audioDelayMs == 0) {
        return;
    }
    
    int delaySamples = (m_delayCompensation.audioDelayMs * sampleRate) / 1000;
    delaySamples = qBound(-MAX_DELAY_SAMPLES, delaySamples, MAX_DELAY_SAMPLES);
    
    if (delaySamples > 0) {
        // Positive delay - add silence at the beginning
        QVector<float> silenceBuffer(delaySamples, 0.0f);
        leftChannel = silenceBuffer + leftChannel;
        rightChannel = silenceBuffer + rightChannel;
    } else if (delaySamples < 0) {
        // Negative delay - remove samples from the beginning
        int samplesToRemove = -delaySamples;
        if (samplesToRemove < leftChannel.size()) {
            leftChannel.remove(0, samplesToRemove);
            rightChannel.remove(0, samplesToRemove);
        }
    }
}

float AudioOutputManager::getCurrentLatency() const
{
    // Return estimated latency based on current device and buffer settings
    // This would be calculated based on actual audio system latency
    return 20.0f; // Default 20ms latency
}

bool AudioOutputManager::testAudioDevice(const QString& deviceId)
{
    QMutexLocker locker(&m_deviceMutex);
    
    for (const auto& device : m_availableDevices) {
        if (device.id == deviceId) {
            // Simple availability check
            return device.isAvailable;
        }
    }
    
    return false;
}

void AudioOutputManager::onDevicesChanged()
{
    updateDeviceList();
    emit devicesChanged();
    qCDebug(audioOutputManager) << "Audio devices changed";
}

void AudioOutputManager::updateDeviceList()
{
    QMutexLocker locker(&m_deviceMutex);
    
    m_availableDevices.clear();
    
    // Get default device
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    
    // Get all available devices
    QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    
    for (const QAudioDevice& device : devices) {
        AudioDeviceInfo deviceInfo = createDeviceInfo(device);
        deviceInfo.isDefault = (device == defaultDevice);
        m_availableDevices.append(deviceInfo);
    }
    
    // Set current device to default if not set
    if (m_currentDevice.id.isEmpty() && !m_availableDevices.isEmpty()) {
        m_currentDevice = getDefaultDevice();
    }
    
    qCDebug(audioOutputManager) << "Updated device list with" << m_availableDevices.size() << "devices";
}

AudioOutputManager::AudioDeviceInfo AudioOutputManager::createDeviceInfo(const QAudioDevice& device) const
{
    AudioDeviceInfo info;
    info.id = device.id();
    info.name = device.description();
    info.description = device.description();
    info.maxChannels = device.maximumChannelCount();
    info.isAvailable = true; // Assume available if listed
    
    // Get supported sample rates
    // Note: QAudioDevice doesn't provide this directly in Qt6
    // This would need to be queried through the actual audio system
    info.supportedRates = {8000, 11025, 16000, 22050, 44100, 48000, 88200, 96000, 176400, 192000};
    
    return info;
}

void AudioOutputManager::initializeSpatialProcessing()
{
    // Initialize spatial processing state
    m_spatialState = SpatialState();
    qCDebug(audioOutputManager) << "Spatial processing initialized";
}

void AudioOutputManager::applyCrossfeed(QVector<float>& leftChannel, QVector<float>& rightChannel, float strength)
{
    if (strength <= 0.0f || leftChannel.size() != rightChannel.size()) {
        return;
    }
    
    float crossfeedGain = strength * 0.3f; // Max 30% crossfeed
    
    for (int i = 0; i < leftChannel.size(); ++i) {
        float left = leftChannel[i];
        float right = rightChannel[i];
        
        // Apply crossfeed with delay for more natural sound
        int delayIndex = (m_spatialState.delayIndex + i) % m_spatialState.delayLineL.size();
        
        float delayedLeft = m_spatialState.delayLineL[delayIndex];
        float delayedRight = m_spatialState.delayLineR[delayIndex];
        
        // Store current samples
        m_spatialState.delayLineL[delayIndex] = left;
        m_spatialState.delayLineR[delayIndex] = right;
        
        // Apply crossfeed
        leftChannel[i] = left + delayedRight * crossfeedGain;
        rightChannel[i] = right + delayedLeft * crossfeedGain;
    }
    
    // Update delay index
    m_spatialState.delayIndex = (m_spatialState.delayIndex + leftChannel.size()) % m_spatialState.delayLineL.size();
}

void AudioOutputManager::applyRoomSimulation(QVector<float>& leftChannel, QVector<float>& rightChannel, float roomSize)
{
    if (roomSize <= 0.0f) {
        return;
    }
    
    float reverbGain = roomSize * 0.2f; // Max 20% reverb
    int reverbDelay = static_cast<int>(roomSize * 2000); // Up to 2000 samples delay
    
    for (int i = 0; i < leftChannel.size(); ++i) {
        float left = leftChannel[i];
        float right = rightChannel[i];
        
        // Simple room simulation with reflections
        int reflectionIndex = (i + reverbDelay) % m_spatialState.roomReflections.size();
        
        if (reflectionIndex < m_spatialState.roomReflections.size()) {
            float reflection = m_spatialState.roomReflections[reflectionIndex];
            
            leftChannel[i] = left + reflection * reverbGain;
            rightChannel[i] = right + reflection * reverbGain;
            
            // Store mixed signal for future reflections
            m_spatialState.roomReflections[i % m_spatialState.roomReflections.size()] = 
                (left + right) * 0.5f * 0.7f; // Decay factor
        }
    }
}

void AudioOutputManager::applyHRTF(QVector<float>& leftChannel, QVector<float>& rightChannel)
{
    // Simplified HRTF (Head-Related Transfer Function) simulation
    // In a real implementation, this would use measured HRTF data
    
    for (int i = 1; i < leftChannel.size(); ++i) {
        // Simple high-frequency emphasis for spatial effect
        float leftDiff = leftChannel[i] - leftChannel[i-1];
        float rightDiff = rightChannel[i] - rightChannel[i-1];
        
        leftChannel[i] += leftDiff * 0.1f;
        rightChannel[i] += rightDiff * 0.1f;
    }
}
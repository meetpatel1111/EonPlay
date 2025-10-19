#include "audio/AudioVisualizer.h"
#include <QLoggingCategory>
#include <QtMath>
#include <QRandomGenerator>
#include <QColor>
#include <algorithm>
#include <complex>

Q_DECLARE_LOGGING_CATEGORY(audioVisualizer)
Q_LOGGING_CATEGORY(audioVisualizer, "audio.visualizer")

AudioVisualizer::AudioVisualizer(QObject* parent)
    : QObject(parent)
    , m_enabled(false)
    , m_visualizationMode(SpectrumAnalyzer)
    , m_spectrumBandCount(DEFAULT_BAND_COUNT)
    , m_updateRate(DEFAULT_UPDATE_RATE)
    , m_sensitivity(DEFAULT_SENSITIVITY)
    , m_smoothingEnabled(true)
    , m_smoothingFactor(DEFAULT_SMOOTHING_FACTOR)
    , m_peakHoldEnabled(true)
    , m_peakHoldTime(DEFAULT_PEAK_HOLD_TIME)
    , m_leftVULevel(0.0f)
    , m_rightVULevel(0.0f)
    , m_leftPeakLevel(0.0f)
    , m_rightPeakLevel(0.0f)
    , m_leftPeakTime(0)
    , m_rightPeakTime(0)
    , m_moodConfidence(0.0f)
    , m_updateTimer(new QTimer(this))
    , m_peakHoldTimer(new QTimer(this))
{
    // Initialize spectrum data
    m_currentSpectrum.frequencies.resize(m_spectrumBandCount);
    m_currentSpectrum.magnitudes.resize(m_spectrumBandCount);
    m_currentSpectrum.phases.resize(m_spectrumBandCount);
    m_smoothedMagnitudes.resize(m_spectrumBandCount);
    m_previousMagnitudes.resize(m_spectrumBandCount);
    
    // Initialize waveform data
    m_leftWaveform.resize(WAVEFORM_SIZE);
    m_rightWaveform.resize(WAVEFORM_SIZE);
    
    // Initialize frequency bands (logarithmic distribution)
    for (int i = 0; i < m_spectrumBandCount; ++i) {
        float ratio = static_cast<float>(i) / (m_spectrumBandCount - 1);
        m_currentSpectrum.frequencies[i] = 20.0f * qPow(1000.0f, ratio); // 20Hz to 20kHz
    }
    
    // Setup timers
    m_updateTimer->setSingleShot(false);
    m_peakHoldTimer->setSingleShot(false);
    m_peakHoldTimer->setInterval(50); // Check peak hold every 50ms
    
    connect(m_updateTimer, &QTimer::timeout, this, &AudioVisualizer::updateVisualization);
    connect(m_peakHoldTimer, &QTimer::timeout, this, &AudioVisualizer::updatePeakHold);
    
    qCDebug(audioVisualizer) << "AudioVisualizer created with" << m_spectrumBandCount << "bands";
}

AudioVisualizer::~AudioVisualizer()
{
    shutdown();
    qCDebug(audioVisualizer) << "AudioVisualizer destroyed";
}

bool AudioVisualizer::initialize()
{
    // Initialize AI color palette with default colors
    m_aiColorPalette = {
        QColor(255, 0, 0),    // Red
        QColor(255, 127, 0),  // Orange
        QColor(255, 255, 0),  // Yellow
        QColor(0, 255, 0),    // Green
        QColor(0, 0, 255),    // Blue
        QColor(75, 0, 130),   // Indigo
        QColor(148, 0, 211)   // Violet
    };
    
    qCDebug(audioVisualizer) << "AudioVisualizer initialized";
    return true;
}

void AudioVisualizer::shutdown()
{
    setEnabled(false);
    qCDebug(audioVisualizer) << "AudioVisualizer shutdown";
}

bool AudioVisualizer::isEnabled() const
{
    return m_enabled;
}

void AudioVisualizer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        if (m_enabled) {
            m_updateTimer->start(1000 / m_updateRate);
            if (m_peakHoldEnabled) {
                m_peakHoldTimer->start();
            }
        } else {
            m_updateTimer->stop();
            m_peakHoldTimer->stop();
        }
        
        emit enabledChanged(m_enabled);
        qCDebug(audioVisualizer) << "Visualizer" << (enabled ? "enabled" : "disabled");
    }
}

AudioVisualizer::VisualizationMode AudioVisualizer::getVisualizationMode() const
{
    return m_visualizationMode;
}

void AudioVisualizer::setVisualizationMode(VisualizationMode mode)
{
    if (m_visualizationMode != mode) {
        m_visualizationMode = mode;
        emit visualizationModeChanged(m_visualizationMode);
        qCDebug(audioVisualizer) << "Visualization mode set to" << getModeName(mode);
    }
}

QString AudioVisualizer::getModeName(VisualizationMode mode)
{
    switch (mode) {
        case None: return "None";
        case SpectrumAnalyzer: return "Spectrum Analyzer";
        case Waveform: return "Waveform";
        case Oscilloscope: return "Oscilloscope";
        case VUMeter: return "VU Meter";
        case FrequencyBars: return "Frequency Bars";
        case CircularSpectrum: return "Circular Spectrum";
        case AIVisualizer: return "AI Visualizer";
        default: return "Unknown";
    }
}

QVector<AudioVisualizer::VisualizationMode> AudioVisualizer::getAvailableModes()
{
    return {None, SpectrumAnalyzer, Waveform, Oscilloscope, VUMeter, FrequencyBars, CircularSpectrum, AIVisualizer};
}

int AudioVisualizer::getSpectrumBandCount() const
{
    return m_spectrumBandCount;
}

void AudioVisualizer::setSpectrumBandCount(int bandCount)
{
    // Validate band count (must be power of 2 for FFT efficiency)
    QVector<int> validCounts = {8, 16, 32, 64, 128, 256};
    if (!validCounts.contains(bandCount)) {
        qCWarning(audioVisualizer) << "Invalid band count:" << bandCount << "using default";
        bandCount = DEFAULT_BAND_COUNT;
    }
    
    if (m_spectrumBandCount != bandCount) {
        m_spectrumBandCount = bandCount;
        
        // Resize arrays
        m_currentSpectrum.frequencies.resize(m_spectrumBandCount);
        m_currentSpectrum.magnitudes.resize(m_spectrumBandCount);
        m_currentSpectrum.phases.resize(m_spectrumBandCount);
        m_smoothedMagnitudes.resize(m_spectrumBandCount);
        m_previousMagnitudes.resize(m_spectrumBandCount);
        
        // Recalculate frequency bands
        for (int i = 0; i < m_spectrumBandCount; ++i) {
            float ratio = static_cast<float>(i) / (m_spectrumBandCount - 1);
            m_currentSpectrum.frequencies[i] = 20.0f * qPow(1000.0f, ratio);
        }
        
        qCDebug(audioVisualizer) << "Spectrum band count set to" << bandCount;
    }
}

int AudioVisualizer::getUpdateRate() const
{
    return m_updateRate;
}

void AudioVisualizer::setUpdateRate(int rate)
{
    rate = qBound(10, rate, 60);
    
    if (m_updateRate != rate) {
        m_updateRate = rate;
        
        if (m_enabled) {
            m_updateTimer->setInterval(1000 / m_updateRate);
        }
        
        qCDebug(audioVisualizer) << "Update rate set to" << rate << "Hz";
    }
}

float AudioVisualizer::getSensitivity() const
{
    return m_sensitivity;
}

void AudioVisualizer::setSensitivity(float sensitivity)
{
    sensitivity = qBound(0.0f, sensitivity, 1.0f);
    
    if (m_sensitivity != sensitivity) {
        m_sensitivity = sensitivity;
        qCDebug(audioVisualizer) << "Sensitivity set to" << sensitivity;
    }
}

bool AudioVisualizer::isSmoothingEnabled() const
{
    return m_smoothingEnabled;
}

void AudioVisualizer::setSmoothingEnabled(bool enabled)
{
    if (m_smoothingEnabled != enabled) {
        m_smoothingEnabled = enabled;
        qCDebug(audioVisualizer) << "Smoothing" << (enabled ? "enabled" : "disabled");
    }
}

float AudioVisualizer::getSmoothingFactor() const
{
    return m_smoothingFactor;
}

void AudioVisualizer::setSmoothingFactor(float factor)
{
    factor = qBound(0.0f, factor, 1.0f);
    
    if (m_smoothingFactor != factor) {
        m_smoothingFactor = factor;
        qCDebug(audioVisualizer) << "Smoothing factor set to" << factor;
    }
}

bool AudioVisualizer::isPeakHoldEnabled() const
{
    return m_peakHoldEnabled;
}

void AudioVisualizer::setPeakHoldEnabled(bool enabled)
{
    if (m_peakHoldEnabled != enabled) {
        m_peakHoldEnabled = enabled;
        
        if (m_enabled) {
            if (enabled) {
                m_peakHoldTimer->start();
            } else {
                m_peakHoldTimer->stop();
            }
        }
        
        qCDebug(audioVisualizer) << "Peak hold" << (enabled ? "enabled" : "disabled");
    }
}

int AudioVisualizer::getPeakHoldTime() const
{
    return m_peakHoldTime;
}

void AudioVisualizer::setPeakHoldTime(int timeMs)
{
    timeMs = qBound(100, timeMs, 5000);
    
    if (m_peakHoldTime != timeMs) {
        m_peakHoldTime = timeMs;
        qCDebug(audioVisualizer) << "Peak hold time set to" << timeMs << "ms";
    }
}

void AudioVisualizer::processAudioSample(const AudioSample& sample)
{
    if (!m_enabled || sample.leftChannel.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    
    m_currentSample = sample;
    
    // Calculate VU levels
    calculateVULevels(sample.leftChannel, sample.rightChannel);
    
    // Update waveform data (downsample if necessary)
    int sampleStep = qMax(1, sample.leftChannel.size() / WAVEFORM_SIZE);
    
    for (int i = 0; i < WAVEFORM_SIZE && i * sampleStep < sample.leftChannel.size(); ++i) {
        m_leftWaveform[i] = sample.leftChannel[i * sampleStep] * m_sensitivity;
        if (i * sampleStep < sample.rightChannel.size()) {
            m_rightWaveform[i] = sample.rightChannel[i * sampleStep] * m_sensitivity;
        } else {
            m_rightWaveform[i] = m_leftWaveform[i]; // Mono fallback
        }
    }
    
    // Perform FFT for spectrum analysis
    if (m_visualizationMode == SpectrumAnalyzer || m_visualizationMode == FrequencyBars || 
        m_visualizationMode == CircularSpectrum || m_visualizationMode == AIVisualizer) {
        
        performFFT(sample.leftChannel, m_currentSpectrum.magnitudes, m_currentSpectrum.phases);
        
        // Apply sensitivity
        for (float& magnitude : m_currentSpectrum.magnitudes) {
            magnitude *= m_sensitivity;
        }
        
        // Apply smoothing
        if (m_smoothingEnabled) {
            applySmoothingToSpectrum(m_currentSpectrum.magnitudes);
        }
        
        // Find peak frequency and magnitude
        auto maxIt = std::max_element(m_currentSpectrum.magnitudes.begin(), m_currentSpectrum.magnitudes.end());
        if (maxIt != m_currentSpectrum.magnitudes.end()) {
            int peakIndex = std::distance(m_currentSpectrum.magnitudes.begin(), maxIt);
            m_currentSpectrum.peakFrequency = m_currentSpectrum.frequencies[peakIndex];
            m_currentSpectrum.peakMagnitude = *maxIt;
        }
        
        // AI mood detection and color palette generation
        if (m_visualizationMode == AIVisualizer) {
            detectMoodFromSpectrum(m_currentSpectrum);
            generateAIColorPalette(m_currentSpectrum);
        }
    }
}

AudioVisualizer::SpectrumData AudioVisualizer::getCurrentSpectrum() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_currentSpectrum;
}

QPair<QVector<float>, QVector<float>> AudioVisualizer::getCurrentWaveform() const
{
    QMutexLocker locker(&m_dataMutex);
    return qMakePair(m_leftWaveform, m_rightWaveform);
}

QPair<float, float> AudioVisualizer::getVULevels() const
{
    QMutexLocker locker(&m_dataMutex);
    return qMakePair(m_leftVULevel, m_rightVULevel);
}

QPair<float, float> AudioVisualizer::getPeakLevels() const
{
    QMutexLocker locker(&m_dataMutex);
    return qMakePair(m_leftPeakLevel, m_rightPeakLevel);
}

bool AudioVisualizer::isAIVisualizerAvailable() const
{
    return true; // AI features are always available in this implementation
}

QString AudioVisualizer::getDetectedMood() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_detectedMood;
}

QVector<QColor> AudioVisualizer::getAIColorPalette() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_aiColorPalette;
}

void AudioVisualizer::updateVisualization()
{
    if (!m_enabled) {
        return;
    }
    
    // Emit appropriate signals based on visualization mode
    switch (m_visualizationMode) {
        case SpectrumAnalyzer:
        case FrequencyBars:
        case CircularSpectrum:
            emit spectrumDataUpdated(getCurrentSpectrum());
            break;
            
        case Waveform:
        case Oscilloscope: {
            auto waveform = getCurrentWaveform();
            emit waveformDataUpdated(waveform.first, waveform.second);
            break;
        }
        
        case VUMeter: {
            auto vuLevels = getVULevels();
            emit vuLevelsUpdated(vuLevels.first, vuLevels.second);
            
            auto peakLevels = getPeakLevels();
            emit peakLevelsUpdated(peakLevels.first, peakLevels.second);
            break;
        }
        
        case AIVisualizer:
            emit spectrumDataUpdated(getCurrentSpectrum());
            emit colorPaletteUpdated(getAIColorPalette());
            if (!m_detectedMood.isEmpty()) {
                emit moodDetected(m_detectedMood, m_moodConfidence);
            }
            break;
            
        case None:
        default:
            break;
    }
}

void AudioVisualizer::updatePeakHold()
{
    if (!m_peakHoldEnabled) {
        return;
    }
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // Decay peak levels over time
    if (currentTime - m_leftPeakTime > m_peakHoldTime) {
        m_leftPeakLevel *= 0.95f; // Gradual decay
    }
    
    if (currentTime - m_rightPeakTime > m_peakHoldTime) {
        m_rightPeakLevel *= 0.95f; // Gradual decay
    }
}

void AudioVisualizer::performFFT(const QVector<float>& input, QVector<float>& magnitudes, QVector<float>& phases)
{
    // Simple FFT implementation for demonstration
    // In a real implementation, you would use a proper FFT library like FFTW
    
    int inputSize = qMin(input.size(), FFT_SIZE);
    QVector<std::complex<float>> fftInput(FFT_SIZE, std::complex<float>(0, 0));
    
    // Copy input data and apply window function (Hanning window)
    for (int i = 0; i < inputSize; ++i) {
        float window = 0.5f * (1.0f - qCos(2.0f * M_PI * i / (inputSize - 1)));
        fftInput[i] = std::complex<float>(input[i] * window, 0);
    }
    
    // Simplified DFT (not optimized, for demonstration only)
    QVector<std::complex<float>> fftOutput(m_spectrumBandCount);
    
    for (int k = 0; k < m_spectrumBandCount; ++k) {
        std::complex<float> sum(0, 0);
        
        for (int n = 0; n < FFT_SIZE; ++n) {
            float angle = -2.0f * M_PI * k * n / FFT_SIZE;
            std::complex<float> w(qCos(angle), qSin(angle));
            sum += fftInput[n] * w;
        }
        
        fftOutput[k] = sum;
        
        // Calculate magnitude and phase
        magnitudes[k] = std::abs(fftOutput[k]) / FFT_SIZE;
        phases[k] = std::arg(fftOutput[k]);
    }
}

void AudioVisualizer::calculateVULevels(const QVector<float>& leftChannel, const QVector<float>& rightChannel)
{
    // Calculate RMS levels for VU meters
    float leftRMS = calculateRMS(leftChannel);
    float rightRMS = rightChannel.isEmpty() ? leftRMS : calculateRMS(rightChannel);
    
    m_leftVULevel = leftRMS;
    m_rightVULevel = rightRMS;
    
    // Update peak levels
    updatePeakLevels(leftRMS, rightRMS);
}

void AudioVisualizer::updatePeakLevels(float leftLevel, float rightLevel)
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    if (leftLevel > m_leftPeakLevel) {
        m_leftPeakLevel = leftLevel;
        m_leftPeakTime = currentTime;
    }
    
    if (rightLevel > m_rightPeakLevel) {
        m_rightPeakLevel = rightLevel;
        m_rightPeakTime = currentTime;
    }
}

void AudioVisualizer::applySmoothingToSpectrum(QVector<float>& magnitudes)
{
    for (int i = 0; i < magnitudes.size(); ++i) {
        m_smoothedMagnitudes[i] = m_smoothingFactor * m_previousMagnitudes[i] + 
                                  (1.0f - m_smoothingFactor) * magnitudes[i];
        magnitudes[i] = m_smoothedMagnitudes[i];
    }
    
    m_previousMagnitudes = m_smoothedMagnitudes;
}

void AudioVisualizer::detectMoodFromSpectrum(const SpectrumData& spectrum)
{
    // Simple mood detection based on frequency distribution
    float bassEnergy = 0.0f;
    float midEnergy = 0.0f;
    float trebleEnergy = 0.0f;
    
    for (int i = 0; i < spectrum.magnitudes.size(); ++i) {
        float freq = spectrum.frequencies[i];
        float magnitude = spectrum.magnitudes[i];
        
        if (freq < 250.0f) {
            bassEnergy += magnitude;
        } else if (freq < 4000.0f) {
            midEnergy += magnitude;
        } else {
            trebleEnergy += magnitude;
        }
    }
    
    // Normalize energies
    float totalEnergy = bassEnergy + midEnergy + trebleEnergy;
    if (totalEnergy > 0.0f) {
        bassEnergy /= totalEnergy;
        midEnergy /= totalEnergy;
        trebleEnergy /= totalEnergy;
    }
    
    // Determine mood based on frequency distribution
    QString newMood;
    float confidence = 0.0f;
    
    if (bassEnergy > 0.4f) {
        newMood = "Energetic";
        confidence = bassEnergy;
    } else if (midEnergy > 0.5f) {
        newMood = "Balanced";
        confidence = midEnergy;
    } else if (trebleEnergy > 0.3f) {
        newMood = "Bright";
        confidence = trebleEnergy;
    } else {
        newMood = "Calm";
        confidence = 1.0f - totalEnergy;
    }
    
    if (newMood != m_detectedMood) {
        m_detectedMood = newMood;
        m_moodConfidence = confidence;
    }
}

void AudioVisualizer::generateAIColorPalette(const SpectrumData& spectrum)
{
    // Generate color palette based on spectrum analysis
    QVector<QColor> newPalette;
    
    // Base colors on dominant frequencies
    for (int i = 0; i < qMin(7, spectrum.magnitudes.size()); ++i) {
        float magnitude = spectrum.magnitudes[i];
        float frequency = spectrum.frequencies[i];
        
        // Map frequency to hue (20Hz = red, 20kHz = violet)
        float hue = (qLn(frequency / 20.0f) / qLn(1000.0f)) * 300.0f; // 0-300 degrees
        hue = qBound(0.0f, hue, 300.0f);
        
        // Map magnitude to saturation and value
        float saturation = qBound(0.3f, magnitude * 2.0f, 1.0f);
        float value = qBound(0.5f, magnitude * 1.5f, 1.0f);
        
        QColor color = QColor::fromHsvF(hue / 360.0f, saturation, value);
        newPalette.append(color);
    }
    
    // Ensure we have at least 3 colors
    while (newPalette.size() < 3) {
        newPalette.append(QColor(QRandomGenerator::global()->bounded(256),
                                QRandomGenerator::global()->bounded(256),
                                QRandomGenerator::global()->bounded(256)));
    }
    
    m_aiColorPalette = newPalette;
}

float AudioVisualizer::calculateRMS(const QVector<float>& samples)
{
    if (samples.isEmpty()) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (float sample : samples) {
        sum += sample * sample;
    }
    
    return qSqrt(sum / samples.size());
}

float AudioVisualizer::calculatePeak(const QVector<float>& samples)
{
    if (samples.isEmpty()) {
        return 0.0f;
    }
    
    float peak = 0.0f;
    for (float sample : samples) {
        peak = qMax(peak, qAbs(sample));
    }
    
    return peak;
}
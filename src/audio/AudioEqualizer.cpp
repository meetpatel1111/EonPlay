#include "audio/AudioEqualizer.h"
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QLoggingCategory>
#include <QtMath>

Q_DECLARE_LOGGING_CATEGORY(audioEqualizer)
Q_LOGGING_CATEGORY(audioEqualizer, "audio.equalizer")

AudioEqualizer::AudioEqualizer(QObject* parent)
    : QObject(parent)
    , m_enabled(false)
    , m_currentPreset(Flat)
    , m_bassBoost(0.0)
    , m_trebleEnhancement(0.0)
    , m_3dSurroundEnabled(false)
    , m_3dSurroundStrength(0.5)
    , m_replayGainEnabled(false)
    , m_replayGainMode("track")
    , m_replayGainPreamp(0.0)
{
    // Initialize settings
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);
    m_settings = std::make_unique<QSettings>(configPath + "/equalizer.ini", QSettings::IniFormat);
    
    initializeBands();
    
    qCDebug(audioEqualizer) << "AudioEqualizer created";
}

AudioEqualizer::~AudioEqualizer()
{
    shutdown();
    qCDebug(audioEqualizer) << "AudioEqualizer destroyed";
}

bool AudioEqualizer::initialize()
{
    loadSettings();
    applyEqualizerSettings();
    
    qCDebug(audioEqualizer) << "AudioEqualizer initialized with" << m_bands.size() << "bands";
    return true;
}

void AudioEqualizer::shutdown()
{
    saveSettings();
    qCDebug(audioEqualizer) << "AudioEqualizer shutdown";
}

void AudioEqualizer::initializeBands()
{
    m_bands.clear();
    
    // Standard 10-band equalizer frequencies (Hz)
    QVector<double> frequencies = {
        31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
    };
    
    for (double freq : frequencies) {
        m_bands.append(EqualizerBand(freq, 0.0, 1.0));
    }
    
    qCDebug(audioEqualizer) << "Initialized" << m_bands.size() << "equalizer bands";
}

bool AudioEqualizer::isEnabled() const
{
    return m_enabled;
}

void AudioEqualizer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        applyEqualizerSettings();
        emit enabledChanged(m_enabled);
        
        qCDebug(audioEqualizer) << "Equalizer" << (enabled ? "enabled" : "disabled");
    }
}

int AudioEqualizer::getBandCount() const
{
    return m_bands.size();
}

AudioEqualizer::EqualizerBand AudioEqualizer::getBand(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < m_bands.size()) {
        return m_bands[bandIndex];
    }
    return EqualizerBand();
}

void AudioEqualizer::setBandGain(int bandIndex, double gain)
{
    if (bandIndex >= 0 && bandIndex < m_bands.size()) {
        clampGainValue(gain);
        
        if (m_bands[bandIndex].gain != gain) {
            m_bands[bandIndex].gain = gain;
            m_currentPreset = Custom;
            
            applyEqualizerSettings();
            emit bandGainChanged(bandIndex, gain);
            emit presetChanged(m_currentPreset);
            
            qCDebug(audioEqualizer) << "Band" << bandIndex << "gain set to" << gain << "dB";
        }
    }
}

QVector<double> AudioEqualizer::getAllBandGains() const
{
    QVector<double> gains;
    for (const auto& band : m_bands) {
        gains.append(band.gain);
    }
    return gains;
}

void AudioEqualizer::setAllBandGains(const QVector<double>& gains)
{
    if (gains.size() != m_bands.size()) {
        qCWarning(audioEqualizer) << "Invalid gains vector size:" << gains.size() << "expected:" << m_bands.size();
        return;
    }
    
    bool changed = false;
    for (int i = 0; i < gains.size(); ++i) {
        double gain = gains[i];
        clampGainValue(gain);
        
        if (m_bands[i].gain != gain) {
            m_bands[i].gain = gain;
            changed = true;
        }
    }
    
    if (changed) {
        m_currentPreset = Custom;
        applyEqualizerSettings();
        
        for (int i = 0; i < gains.size(); ++i) {
            emit bandGainChanged(i, m_bands[i].gain);
        }
        emit presetChanged(m_currentPreset);
        
        qCDebug(audioEqualizer) << "All band gains updated";
    }
}

AudioEqualizer::PresetProfile AudioEqualizer::getCurrentPreset() const
{
    return m_currentPreset;
}

void AudioEqualizer::applyPreset(PresetProfile preset)
{
    if (m_currentPreset != preset) {
        m_currentPreset = preset;
        
        QVector<double> gains = getPresetGains(preset);
        setAllBandGains(gains);
        
        emit presetChanged(m_currentPreset);
        
        qCDebug(audioEqualizer) << "Applied preset:" << getPresetName(preset);
    }
}

QString AudioEqualizer::getPresetName(PresetProfile preset)
{
    switch (preset) {
        case Flat: return "Flat";
        case Rock: return "Rock";
        case Jazz: return "Jazz";
        case Pop: return "Pop";
        case Classical: return "Classical";
        case Electronic: return "Electronic";
        case Vocal: return "Vocal";
        case Bass: return "Bass";
        case Treble: return "Treble";
        case Custom: return "Custom";
        default: return "Unknown";
    }
}

QVector<AudioEqualizer::PresetProfile> AudioEqualizer::getAvailablePresets()
{
    return {Flat, Rock, Jazz, Pop, Classical, Electronic, Vocal, Bass, Treble};
}

QVector<double> AudioEqualizer::getPresetGains(PresetProfile preset) const
{
    // Preset gain values for 10-band equalizer (31Hz to 16kHz)
    switch (preset) {
        case Flat:
            return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            
        case Rock:
            return {5, 3, -1, -2, -1, 2, 4, 6, 7, 7};
            
        case Jazz:
            return {4, 3, 1, 2, -1, -1, 0, 1, 3, 4};
            
        case Pop:
            return {-1, 2, 4, 4, 1, -1, -2, -2, -1, -1};
            
        case Classical:
            return {5, 4, 3, 2, -1, -1, 0, 2, 3, 4};
            
        case Electronic:
            return {6, 5, 1, 0, -2, 2, 1, 2, 6, 7};
            
        case Vocal:
            return {-2, -1, 1, 3, 4, 4, 3, 1, 0, -1};
            
        case Bass:
            return {8, 6, 4, 2, 0, -1, -2, -3, -3, -3};
            
        case Treble:
            return {-3, -3, -2, -1, 0, 2, 4, 6, 8, 9};
            
        case Custom:
        default:
            return getAllBandGains();
    }
}

double AudioEqualizer::getBassBoost() const
{
    return m_bassBoost;
}

void AudioEqualizer::setBassBoost(double boost)
{
    boost = qBound(MIN_BASS_BOOST, boost, MAX_BASS_BOOST);
    
    if (m_bassBoost != boost) {
        m_bassBoost = boost;
        applyEqualizerSettings();
        emit bassBoostChanged(m_bassBoost);
        
        qCDebug(audioEqualizer) << "Bass boost set to" << boost << "dB";
    }
}

double AudioEqualizer::getTrebleEnhancement() const
{
    return m_trebleEnhancement;
}

void AudioEqualizer::setTrebleEnhancement(double enhancement)
{
    enhancement = qBound(MIN_TREBLE_ENHANCEMENT, enhancement, MAX_TREBLE_ENHANCEMENT);
    
    if (m_trebleEnhancement != enhancement) {
        m_trebleEnhancement = enhancement;
        applyEqualizerSettings();
        emit trebleEnhancementChanged(m_trebleEnhancement);
        
        qCDebug(audioEqualizer) << "Treble enhancement set to" << enhancement << "dB";
    }
}

bool AudioEqualizer::is3DSurroundEnabled() const
{
    return m_3dSurroundEnabled;
}

void AudioEqualizer::set3DSurroundEnabled(bool enabled)
{
    if (m_3dSurroundEnabled != enabled) {
        m_3dSurroundEnabled = enabled;
        applyEqualizerSettings();
        emit surroundChanged(m_3dSurroundEnabled, m_3dSurroundStrength);
        
        qCDebug(audioEqualizer) << "3D surround" << (enabled ? "enabled" : "disabled");
    }
}

double AudioEqualizer::get3DSurroundStrength() const
{
    return m_3dSurroundStrength;
}

void AudioEqualizer::set3DSurroundStrength(double strength)
{
    strength = qBound(0.0, strength, 1.0);
    
    if (m_3dSurroundStrength != strength) {
        m_3dSurroundStrength = strength;
        applyEqualizerSettings();
        emit surroundChanged(m_3dSurroundEnabled, m_3dSurroundStrength);
        
        qCDebug(audioEqualizer) << "3D surround strength set to" << strength;
    }
}

bool AudioEqualizer::isReplayGainEnabled() const
{
    return m_replayGainEnabled;
}

void AudioEqualizer::setReplayGainEnabled(bool enabled)
{
    if (m_replayGainEnabled != enabled) {
        m_replayGainEnabled = enabled;
        applyEqualizerSettings();
        emit replayGainChanged(m_replayGainEnabled, m_replayGainMode, m_replayGainPreamp);
        
        qCDebug(audioEqualizer) << "ReplayGain" << (enabled ? "enabled" : "disabled");
    }
}

QString AudioEqualizer::getReplayGainMode() const
{
    return m_replayGainMode;
}

void AudioEqualizer::setReplayGainMode(const QString& mode)
{
    if (mode == "track" || mode == "album") {
        if (m_replayGainMode != mode) {
            m_replayGainMode = mode;
            applyEqualizerSettings();
            emit replayGainChanged(m_replayGainEnabled, m_replayGainMode, m_replayGainPreamp);
            
            qCDebug(audioEqualizer) << "ReplayGain mode set to" << mode;
        }
    } else {
        qCWarning(audioEqualizer) << "Invalid ReplayGain mode:" << mode;
    }
}

double AudioEqualizer::getReplayGainPreamp() const
{
    return m_replayGainPreamp;
}

void AudioEqualizer::setReplayGainPreamp(double preamp)
{
    preamp = qBound(-15.0, preamp, 15.0);
    
    if (m_replayGainPreamp != preamp) {
        m_replayGainPreamp = preamp;
        applyEqualizerSettings();
        emit replayGainChanged(m_replayGainEnabled, m_replayGainMode, m_replayGainPreamp);
        
        qCDebug(audioEqualizer) << "ReplayGain preamp set to" << preamp << "dB";
    }
}

void AudioEqualizer::saveSettings()
{
    if (!m_settings) {
        return;
    }
    
    m_settings->beginGroup("Equalizer");
    m_settings->setValue("enabled", m_enabled);
    m_settings->setValue("preset", static_cast<int>(m_currentPreset));
    
    // Save band gains
    m_settings->beginWriteArray("bands");
    for (int i = 0; i < m_bands.size(); ++i) {
        m_settings->setArrayIndex(i);
        m_settings->setValue("frequency", m_bands[i].frequency);
        m_settings->setValue("gain", m_bands[i].gain);
        m_settings->setValue("bandwidth", m_bands[i].bandwidth);
    }
    m_settings->endArray();
    
    // Save enhancements
    m_settings->setValue("bassBoost", m_bassBoost);
    m_settings->setValue("trebleEnhancement", m_trebleEnhancement);
    m_settings->setValue("3dSurroundEnabled", m_3dSurroundEnabled);
    m_settings->setValue("3dSurroundStrength", m_3dSurroundStrength);
    
    // Save ReplayGain
    m_settings->setValue("replayGainEnabled", m_replayGainEnabled);
    m_settings->setValue("replayGainMode", m_replayGainMode);
    m_settings->setValue("replayGainPreamp", m_replayGainPreamp);
    
    m_settings->endGroup();
    m_settings->sync();
    
    qCDebug(audioEqualizer) << "Settings saved";
}

void AudioEqualizer::loadSettings()
{
    if (!m_settings) {
        return;
    }
    
    m_settings->beginGroup("Equalizer");
    
    m_enabled = m_settings->value("enabled", false).toBool();
    m_currentPreset = static_cast<PresetProfile>(m_settings->value("preset", static_cast<int>(Flat)).toInt());
    
    // Load band gains
    int bandCount = m_settings->beginReadArray("bands");
    if (bandCount == m_bands.size()) {
        for (int i = 0; i < bandCount; ++i) {
            m_settings->setArrayIndex(i);
            m_bands[i].frequency = m_settings->value("frequency", m_bands[i].frequency).toDouble();
            m_bands[i].gain = m_settings->value("gain", 0.0).toDouble();
            m_bands[i].bandwidth = m_settings->value("bandwidth", 1.0).toDouble();
        }
    }
    m_settings->endArray();
    
    // Load enhancements
    m_bassBoost = m_settings->value("bassBoost", 0.0).toDouble();
    m_trebleEnhancement = m_settings->value("trebleEnhancement", 0.0).toDouble();
    m_3dSurroundEnabled = m_settings->value("3dSurroundEnabled", false).toBool();
    m_3dSurroundStrength = m_settings->value("3dSurroundStrength", 0.5).toDouble();
    
    // Load ReplayGain
    m_replayGainEnabled = m_settings->value("replayGainEnabled", false).toBool();
    m_replayGainMode = m_settings->value("replayGainMode", "track").toString();
    m_replayGainPreamp = m_settings->value("replayGainPreamp", 0.0).toDouble();
    
    m_settings->endGroup();
    
    qCDebug(audioEqualizer) << "Settings loaded";
}

void AudioEqualizer::resetToDefaults()
{
    m_enabled = false;
    m_currentPreset = Flat;
    
    // Reset all band gains to 0
    for (auto& band : m_bands) {
        band.gain = 0.0;
    }
    
    m_bassBoost = 0.0;
    m_trebleEnhancement = 0.0;
    m_3dSurroundEnabled = false;
    m_3dSurroundStrength = 0.5;
    m_replayGainEnabled = false;
    m_replayGainMode = "track";
    m_replayGainPreamp = 0.0;
    
    applyEqualizerSettings();
    
    // Emit all change signals
    emit enabledChanged(m_enabled);
    emit presetChanged(m_currentPreset);
    for (int i = 0; i < m_bands.size(); ++i) {
        emit bandGainChanged(i, m_bands[i].gain);
    }
    emit bassBoostChanged(m_bassBoost);
    emit trebleEnhancementChanged(m_trebleEnhancement);
    emit surroundChanged(m_3dSurroundEnabled, m_3dSurroundStrength);
    emit replayGainChanged(m_replayGainEnabled, m_replayGainMode, m_replayGainPreamp);
    
    qCDebug(audioEqualizer) << "Reset to defaults";
}

bool AudioEqualizer::exportSettings(const QString& filePath) const
{
    QJsonObject root;
    root["version"] = "1.0";
    root["enabled"] = m_enabled;
    root["preset"] = static_cast<int>(m_currentPreset);
    
    // Export bands
    QJsonArray bandsArray;
    for (const auto& band : m_bands) {
        QJsonObject bandObj;
        bandObj["frequency"] = band.frequency;
        bandObj["gain"] = band.gain;
        bandObj["bandwidth"] = band.bandwidth;
        bandsArray.append(bandObj);
    }
    root["bands"] = bandsArray;
    
    // Export enhancements
    QJsonObject enhancements;
    enhancements["bassBoost"] = m_bassBoost;
    enhancements["trebleEnhancement"] = m_trebleEnhancement;
    enhancements["3dSurroundEnabled"] = m_3dSurroundEnabled;
    enhancements["3dSurroundStrength"] = m_3dSurroundStrength;
    root["enhancements"] = enhancements;
    
    // Export ReplayGain
    QJsonObject replayGain;
    replayGain["enabled"] = m_replayGainEnabled;
    replayGain["mode"] = m_replayGainMode;
    replayGain["preamp"] = m_replayGainPreamp;
    root["replayGain"] = replayGain;
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        qCDebug(audioEqualizer) << "Settings exported to" << filePath;
        return true;
    }
    
    qCWarning(audioEqualizer) << "Failed to export settings to" << filePath;
    return false;
}

bool AudioEqualizer::importSettings(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(audioEqualizer) << "Failed to open settings file" << filePath;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(audioEqualizer) << "JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Import basic settings
    m_enabled = root["enabled"].toBool();
    m_currentPreset = static_cast<PresetProfile>(root["preset"].toInt());
    
    // Import bands
    QJsonArray bandsArray = root["bands"].toArray();
    if (bandsArray.size() == m_bands.size()) {
        for (int i = 0; i < bandsArray.size(); ++i) {
            QJsonObject bandObj = bandsArray[i].toObject();
            m_bands[i].frequency = bandObj["frequency"].toDouble();
            m_bands[i].gain = bandObj["gain"].toDouble();
            m_bands[i].bandwidth = bandObj["bandwidth"].toDouble();
        }
    }
    
    // Import enhancements
    QJsonObject enhancements = root["enhancements"].toObject();
    m_bassBoost = enhancements["bassBoost"].toDouble();
    m_trebleEnhancement = enhancements["trebleEnhancement"].toDouble();
    m_3dSurroundEnabled = enhancements["3dSurroundEnabled"].toBool();
    m_3dSurroundStrength = enhancements["3dSurroundStrength"].toDouble();
    
    // Import ReplayGain
    QJsonObject replayGain = root["replayGain"].toObject();
    m_replayGainEnabled = replayGain["enabled"].toBool();
    m_replayGainMode = replayGain["mode"].toString();
    m_replayGainPreamp = replayGain["preamp"].toDouble();
    
    applyEqualizerSettings();
    
    // Emit all change signals
    emit enabledChanged(m_enabled);
    emit presetChanged(m_currentPreset);
    for (int i = 0; i < m_bands.size(); ++i) {
        emit bandGainChanged(i, m_bands[i].gain);
    }
    emit bassBoostChanged(m_bassBoost);
    emit trebleEnhancementChanged(m_trebleEnhancement);
    emit surroundChanged(m_3dSurroundEnabled, m_3dSurroundStrength);
    emit replayGainChanged(m_replayGainEnabled, m_replayGainMode, m_replayGainPreamp);
    
    qCDebug(audioEqualizer) << "Settings imported from" << filePath;
    return true;
}

void AudioEqualizer::applyEqualizerSettings()
{
    // TODO: Apply equalizer settings to audio engine
    // This would integrate with the VLC backend or audio processing pipeline
    
    qCDebug(audioEqualizer) << "Equalizer settings applied - enabled:" << m_enabled
                           << "preset:" << getPresetName(m_currentPreset)
                           << "bass boost:" << m_bassBoost
                           << "treble:" << m_trebleEnhancement;
}

void AudioEqualizer::clampGainValue(double& gain) const
{
    gain = qBound(MIN_GAIN, gain, MAX_GAIN);
}
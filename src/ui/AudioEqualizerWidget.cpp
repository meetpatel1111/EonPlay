#include "ui/AudioEqualizerWidget.h"
#include "audio/AudioEqualizer.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(audioEqualizerWidget)
Q_LOGGING_CATEGORY(audioEqualizerWidget, "ui.audioEqualizer")

AudioEqualizerWidget::AudioEqualizerWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_enableCheckBox(nullptr)
    , m_presetLayout(nullptr)
    , m_presetLabel(nullptr)
    , m_presetCombo(nullptr)
    , m_bandsGroupBox(nullptr)
    , m_bandsLayout(nullptr)
    , m_enhancementsGroupBox(nullptr)
    , m_enhancementsLayout(nullptr)
    , m_bassBoostLabel(nullptr)
    , m_bassBoostSlider(nullptr)
    , m_bassBoostValueLabel(nullptr)
    , m_trebleLabel(nullptr)
    , m_trebleSlider(nullptr)
    , m_trebleValueLabel(nullptr)
    , m_3dSurroundCheckBox(nullptr)
    , m_3dSurroundStrengthLabel(nullptr)
    , m_3dSurroundStrengthSlider(nullptr)
    , m_3dSurroundValueLabel(nullptr)
    , m_replayGainGroupBox(nullptr)
    , m_replayGainLayout(nullptr)
    , m_replayGainCheckBox(nullptr)
    , m_replayGainModeLabel(nullptr)
    , m_replayGainModeCombo(nullptr)
    , m_replayGainPreampLabel(nullptr)
    , m_replayGainPreampSpinBox(nullptr)
    , m_buttonLayout(nullptr)
    , m_resetButton(nullptr)
    , m_exportButton(nullptr)
    , m_importButton(nullptr)
    , m_updatingFromEqualizer(false)
{
    setupUI();
    setupConnections();
    
    qCDebug(audioEqualizerWidget) << "AudioEqualizerWidget created";
}

AudioEqualizerWidget::~AudioEqualizerWidget()
{
    qCDebug(audioEqualizerWidget) << "AudioEqualizerWidget destroyed";
}

void AudioEqualizerWidget::initialize(std::shared_ptr<AudioEqualizer> equalizer)
{
    m_equalizer = equalizer;
    
    if (m_equalizer) {
        // Connect to equalizer signals
        connect(m_equalizer.get(), &AudioEqualizer::enabledChanged,
                this, &AudioEqualizerWidget::onEqualizerEnabledChanged);
        connect(m_equalizer.get(), &AudioEqualizer::bandGainChanged,
                this, &AudioEqualizerWidget::onBandGainChanged);
        connect(m_equalizer.get(), &AudioEqualizer::presetChanged,
                this, &AudioEqualizerWidget::onPresetChanged);
        connect(m_equalizer.get(), &AudioEqualizer::bassBoostChanged,
                this, &AudioEqualizerWidget::onBassBoostChanged);
        connect(m_equalizer.get(), &AudioEqualizer::trebleEnhancementChanged,
                this, &AudioEqualizerWidget::onTrebleEnhancementChanged);
        connect(m_equalizer.get(), &AudioEqualizer::surroundChanged,
                this, &AudioEqualizerWidget::onSurroundChanged);
        connect(m_equalizer.get(), &AudioEqualizer::replayGainChanged,
                this, &AudioEqualizerWidget::onReplayGainChanged);
        
        updateFromEqualizer();
    }
    
    qCDebug(audioEqualizerWidget) << "AudioEqualizerWidget initialized";
}

void AudioEqualizerWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);
    
    // Enable checkbox
    m_enableCheckBox = new QCheckBox("Enable Equalizer", this);
    m_enableCheckBox->setStyleSheet("QCheckBox { font-weight: bold; }");
    m_mainLayout->addWidget(m_enableCheckBox);
    
    setupEqualizerBands();
    setupEnhancements();
    setupReplayGain();
    setupButtons();
    
    // Initially disabled
    setWidgetsEnabled(false);
}

void AudioEqualizerWidget::setupEqualizerBands()
{
    // Preset selection
    m_presetLayout = new QHBoxLayout();
    m_presetLabel = new QLabel("Preset:", this);
    m_presetCombo = new QComboBox(this);
    
    // Add presets
    auto presets = AudioEqualizer::getAvailablePresets();
    for (auto preset : presets) {
        m_presetCombo->addItem(AudioEqualizer::getPresetName(preset), static_cast<int>(preset));
    }
    m_presetCombo->addItem(AudioEqualizer::getPresetName(AudioEqualizer::Custom), static_cast<int>(AudioEqualizer::Custom));
    
    m_presetLayout->addWidget(m_presetLabel);
    m_presetLayout->addWidget(m_presetCombo);
    m_presetLayout->addStretch();
    
    m_mainLayout->addLayout(m_presetLayout);
    
    // Equalizer bands
    m_bandsGroupBox = new QGroupBox("Equalizer Bands", this);
    m_bandsLayout = new QHBoxLayout(m_bandsGroupBox);
    
    // Standard 10-band frequencies
    QStringList frequencies = {"31", "62", "125", "250", "500", "1K", "2K", "4K", "8K", "16K"};
    
    for (int i = 0; i < frequencies.size(); ++i) {
        QVBoxLayout* bandLayout = new QVBoxLayout();
        
        // Frequency label
        QLabel* freqLabel = new QLabel(frequencies[i] + " Hz", this);
        freqLabel->setAlignment(Qt::AlignCenter);
        freqLabel->setStyleSheet("QLabel { font-size: 10px; }");
        
        // Gain slider
        QSlider* slider = new QSlider(Qt::Vertical, this);
        slider->setRange(-200, 200); // -20dB to +20dB (scaled by 10)
        slider->setValue(0);
        slider->setTickPosition(QSlider::TicksBothSides);
        slider->setTickInterval(50); // 5dB intervals
        slider->setMinimumHeight(150);
        slider->setProperty("bandIndex", i);
        
        // Value label
        QLabel* valueLabel = new QLabel("0 dB", this);
        valueLabel->setAlignment(Qt::AlignCenter);
        valueLabel->setStyleSheet("QLabel { font-size: 10px; }");
        
        bandLayout->addWidget(valueLabel);
        bandLayout->addWidget(slider);
        bandLayout->addWidget(freqLabel);
        
        m_bandsLayout->addLayout(bandLayout);
        
        m_bandSliders.append(slider);
        m_bandLabels.append(freqLabel);
        m_bandValueLabels.append(valueLabel);
    }
    
    m_mainLayout->addWidget(m_bandsGroupBox);
}

void AudioEqualizerWidget::setupEnhancements()
{
    m_enhancementsGroupBox = new QGroupBox("Audio Enhancements", this);
    m_enhancementsLayout = new QGridLayout(m_enhancementsGroupBox);
    
    int row = 0;
    
    // Bass boost
    m_bassBoostLabel = new QLabel("Bass Boost:", this);
    m_bassBoostSlider = new QSlider(Qt::Horizontal, this);
    m_bassBoostSlider->setRange(0, 120); // 0 to 12dB (scaled by 10)
    m_bassBoostSlider->setValue(0);
    m_bassBoostValueLabel = new QLabel("0 dB", this);
    m_bassBoostValueLabel->setMinimumWidth(50);
    
    m_enhancementsLayout->addWidget(m_bassBoostLabel, row, 0);
    m_enhancementsLayout->addWidget(m_bassBoostSlider, row, 1);
    m_enhancementsLayout->addWidget(m_bassBoostValueLabel, row, 2);
    row++;
    
    // Treble enhancement
    m_trebleLabel = new QLabel("Treble Enhancement:", this);
    m_trebleSlider = new QSlider(Qt::Horizontal, this);
    m_trebleSlider->setRange(0, 120); // 0 to 12dB (scaled by 10)
    m_trebleSlider->setValue(0);
    m_trebleValueLabel = new QLabel("0 dB", this);
    m_trebleValueLabel->setMinimumWidth(50);
    
    m_enhancementsLayout->addWidget(m_trebleLabel, row, 0);
    m_enhancementsLayout->addWidget(m_trebleSlider, row, 1);
    m_enhancementsLayout->addWidget(m_trebleValueLabel, row, 2);
    row++;
    
    // 3D Surround
    m_3dSurroundCheckBox = new QCheckBox("3D Surround Sound", this);
    m_enhancementsLayout->addWidget(m_3dSurroundCheckBox, row, 0);
    row++;
    
    m_3dSurroundStrengthLabel = new QLabel("Surround Strength:", this);
    m_3dSurroundStrengthSlider = new QSlider(Qt::Horizontal, this);
    m_3dSurroundStrengthSlider->setRange(0, 100); // 0.0 to 1.0 (scaled by 100)
    m_3dSurroundStrengthSlider->setValue(50);
    m_3dSurroundStrengthSlider->setEnabled(false);
    m_3dSurroundValueLabel = new QLabel("50%", this);
    m_3dSurroundValueLabel->setMinimumWidth(50);
    
    m_enhancementsLayout->addWidget(m_3dSurroundStrengthLabel, row, 0);
    m_enhancementsLayout->addWidget(m_3dSurroundStrengthSlider, row, 1);
    m_enhancementsLayout->addWidget(m_3dSurroundValueLabel, row, 2);
    
    m_mainLayout->addWidget(m_enhancementsGroupBox);
}

void AudioEqualizerWidget::setupReplayGain()
{
    m_replayGainGroupBox = new QGroupBox("ReplayGain", this);
    m_replayGainLayout = new QGridLayout(m_replayGainGroupBox);
    
    int row = 0;
    
    // Enable ReplayGain
    m_replayGainCheckBox = new QCheckBox("Enable ReplayGain", this);
    m_replayGainLayout->addWidget(m_replayGainCheckBox, row, 0, 1, 3);
    row++;
    
    // ReplayGain mode
    m_replayGainModeLabel = new QLabel("Mode:", this);
    m_replayGainModeCombo = new QComboBox(this);
    m_replayGainModeCombo->addItem("Track", "track");
    m_replayGainModeCombo->addItem("Album", "album");
    m_replayGainModeCombo->setEnabled(false);
    
    m_replayGainLayout->addWidget(m_replayGainModeLabel, row, 0);
    m_replayGainLayout->addWidget(m_replayGainModeCombo, row, 1, 1, 2);
    row++;
    
    // ReplayGain preamp
    m_replayGainPreampLabel = new QLabel("Preamp:", this);
    m_replayGainPreampSpinBox = new QDoubleSpinBox(this);
    m_replayGainPreampSpinBox->setRange(-15.0, 15.0);
    m_replayGainPreampSpinBox->setSuffix(" dB");
    m_replayGainPreampSpinBox->setValue(0.0);
    m_replayGainPreampSpinBox->setEnabled(false);
    
    m_replayGainLayout->addWidget(m_replayGainPreampLabel, row, 0);
    m_replayGainLayout->addWidget(m_replayGainPreampSpinBox, row, 1, 1, 2);
    
    m_mainLayout->addWidget(m_replayGainGroupBox);
}

void AudioEqualizerWidget::setupButtons()
{
    m_buttonLayout = new QHBoxLayout();
    
    m_resetButton = new QPushButton("Reset", this);
    m_exportButton = new QPushButton("Export", this);
    m_importButton = new QPushButton("Import", this);
    
    m_buttonLayout->addWidget(m_resetButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_exportButton);
    m_buttonLayout->addWidget(m_importButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
}

void AudioEqualizerWidget::setupConnections()
{
    // Enable checkbox
    connect(m_enableCheckBox, &QCheckBox::toggled,
            this, &AudioEqualizerWidget::onEnableCheckBoxToggled);
    
    // Preset combo
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioEqualizerWidget::onPresetComboChanged);
    
    // Band sliders
    for (int i = 0; i < m_bandSliders.size(); ++i) {
        connect(m_bandSliders[i], &QSlider::valueChanged,
                this, &AudioEqualizerWidget::onBandSliderChanged);
    }
    
    // Enhancement sliders
    connect(m_bassBoostSlider, &QSlider::valueChanged,
            this, &AudioEqualizerWidget::onBassBoostSliderChanged);
    connect(m_trebleSlider, &QSlider::valueChanged,
            this, &AudioEqualizerWidget::onTrebleSliderChanged);
    
    // 3D Surround
    connect(m_3dSurroundCheckBox, &QCheckBox::toggled,
            this, &AudioEqualizerWidget::on3DSurroundToggled);
    connect(m_3dSurroundStrengthSlider, &QSlider::valueChanged,
            this, &AudioEqualizerWidget::on3DSurroundStrengthChanged);
    
    // ReplayGain
    connect(m_replayGainCheckBox, &QCheckBox::toggled,
            this, &AudioEqualizerWidget::onReplayGainToggled);
    connect(m_replayGainModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioEqualizerWidget::onReplayGainModeChanged);
    connect(m_replayGainPreampSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioEqualizerWidget::onReplayGainPreampChanged);
    
    // Buttons
    connect(m_resetButton, &QPushButton::clicked,
            this, &AudioEqualizerWidget::onResetClicked);
    connect(m_exportButton, &QPushButton::clicked,
            this, &AudioEqualizerWidget::onExportClicked);
    connect(m_importButton, &QPushButton::clicked,
            this, &AudioEqualizerWidget::onImportClicked);
}

void AudioEqualizerWidget::updateFromEqualizer()
{
    if (!m_equalizer) {
        return;
    }
    
    m_updatingFromEqualizer = true;
    
    // Update enable state
    m_enableCheckBox->setChecked(m_equalizer->isEnabled());
    setWidgetsEnabled(m_equalizer->isEnabled());
    
    // Update preset
    int presetIndex = m_presetCombo->findData(static_cast<int>(m_equalizer->getCurrentPreset()));
    if (presetIndex >= 0) {
        m_presetCombo->setCurrentIndex(presetIndex);
    }
    
    // Update band gains
    QVector<double> gains = m_equalizer->getAllBandGains();
    for (int i = 0; i < qMin(gains.size(), m_bandSliders.size()); ++i) {
        m_bandSliders[i]->setValue(static_cast<int>(gains[i] * 10));
    }
    updateBandLabels();
    
    // Update enhancements
    m_bassBoostSlider->setValue(static_cast<int>(m_equalizer->getBassBoost() * 10));
    m_trebleSlider->setValue(static_cast<int>(m_equalizer->getTrebleEnhancement() * 10));
    
    // Update 3D surround
    m_3dSurroundCheckBox->setChecked(m_equalizer->is3DSurroundEnabled());
    m_3dSurroundStrengthSlider->setValue(static_cast<int>(m_equalizer->get3DSurroundStrength() * 100));
    m_3dSurroundStrengthSlider->setEnabled(m_equalizer->is3DSurroundEnabled());
    
    // Update ReplayGain
    m_replayGainCheckBox->setChecked(m_equalizer->isReplayGainEnabled());
    int modeIndex = m_replayGainModeCombo->findData(m_equalizer->getReplayGainMode());
    if (modeIndex >= 0) {
        m_replayGainModeCombo->setCurrentIndex(modeIndex);
    }
    m_replayGainPreampSpinBox->setValue(m_equalizer->getReplayGainPreamp());
    
    bool replayGainEnabled = m_equalizer->isReplayGainEnabled();
    m_replayGainModeCombo->setEnabled(replayGainEnabled);
    m_replayGainPreampSpinBox->setEnabled(replayGainEnabled);
    
    updateEnhancementLabels();
    
    m_updatingFromEqualizer = false;
}

void AudioEqualizerWidget::updateBandLabels()
{
    for (int i = 0; i < m_bandValueLabels.size(); ++i) {
        double gain = m_bandSliders[i]->value() / 10.0;
        m_bandValueLabels[i]->setText(QString("%1 dB").arg(gain, 0, 'f', 1));
    }
}

void AudioEqualizerWidget::updateEnhancementLabels()
{
    m_bassBoostValueLabel->setText(QString("%1 dB").arg(m_bassBoostSlider->value() / 10.0, 0, 'f', 1));
    m_trebleValueLabel->setText(QString("%1 dB").arg(m_trebleSlider->value() / 10.0, 0, 'f', 1));
    m_3dSurroundValueLabel->setText(QString("%1%").arg(m_3dSurroundStrengthSlider->value()));
}

void AudioEqualizerWidget::setWidgetsEnabled(bool enabled)
{
    m_presetCombo->setEnabled(enabled);
    m_bandsGroupBox->setEnabled(enabled);
    m_enhancementsGroupBox->setEnabled(enabled);
    m_replayGainGroupBox->setEnabled(enabled);
    
    if (enabled) {
        // Re-enable based on individual settings
        m_3dSurroundStrengthSlider->setEnabled(m_3dSurroundCheckBox->isChecked());
        m_replayGainModeCombo->setEnabled(m_replayGainCheckBox->isChecked());
        m_replayGainPreampSpinBox->setEnabled(m_replayGainCheckBox->isChecked());
    }
}

// Slot implementations
void AudioEqualizerWidget::onEqualizerEnabledChanged(bool enabled)
{
    if (!m_updatingFromEqualizer) {
        m_enableCheckBox->setChecked(enabled);
        setWidgetsEnabled(enabled);
    }
}

void AudioEqualizerWidget::onBandGainChanged(int bandIndex, double gain)
{
    if (!m_updatingFromEqualizer && bandIndex < m_bandSliders.size()) {
        m_bandSliders[bandIndex]->setValue(static_cast<int>(gain * 10));
        updateBandLabels();
    }
}

void AudioEqualizerWidget::onPresetChanged(int preset)
{
    if (!m_updatingFromEqualizer) {
        int index = m_presetCombo->findData(preset);
        if (index >= 0) {
            m_presetCombo->setCurrentIndex(index);
        }
    }
}

void AudioEqualizerWidget::onBassBoostChanged(double boost)
{
    if (!m_updatingFromEqualizer) {
        m_bassBoostSlider->setValue(static_cast<int>(boost * 10));
        updateEnhancementLabels();
    }
}

void AudioEqualizerWidget::onTrebleEnhancementChanged(double enhancement)
{
    if (!m_updatingFromEqualizer) {
        m_trebleSlider->setValue(static_cast<int>(enhancement * 10));
        updateEnhancementLabels();
    }
}

void AudioEqualizerWidget::onSurroundChanged(bool enabled, double strength)
{
    if (!m_updatingFromEqualizer) {
        m_3dSurroundCheckBox->setChecked(enabled);
        m_3dSurroundStrengthSlider->setValue(static_cast<int>(strength * 100));
        m_3dSurroundStrengthSlider->setEnabled(enabled);
        updateEnhancementLabels();
    }
}

void AudioEqualizerWidget::onReplayGainChanged(bool enabled, const QString& mode, double preamp)
{
    if (!m_updatingFromEqualizer) {
        m_replayGainCheckBox->setChecked(enabled);
        
        int modeIndex = m_replayGainModeCombo->findData(mode);
        if (modeIndex >= 0) {
            m_replayGainModeCombo->setCurrentIndex(modeIndex);
        }
        
        m_replayGainPreampSpinBox->setValue(preamp);
        
        m_replayGainModeCombo->setEnabled(enabled);
        m_replayGainPreampSpinBox->setEnabled(enabled);
    }
}

void AudioEqualizerWidget::onEnableCheckBoxToggled(bool enabled)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        m_equalizer->setEnabled(enabled);
    }
}

void AudioEqualizerWidget::onPresetComboChanged(int index)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        int preset = m_presetCombo->itemData(index).toInt();
        m_equalizer->applyPreset(static_cast<AudioEqualizer::PresetProfile>(preset));
    }
}

void AudioEqualizerWidget::onBandSliderChanged(int value)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        QSlider* slider = qobject_cast<QSlider*>(sender());
        if (slider) {
            int bandIndex = slider->property("bandIndex").toInt();
            double gain = value / 10.0;
            m_equalizer->setBandGain(bandIndex, gain);
        }
    }
    updateBandLabels();
}

void AudioEqualizerWidget::onBassBoostSliderChanged(int value)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        double boost = value / 10.0;
        m_equalizer->setBassBoost(boost);
    }
    updateEnhancementLabels();
}

void AudioEqualizerWidget::onTrebleSliderChanged(int value)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        double enhancement = value / 10.0;
        m_equalizer->setTrebleEnhancement(enhancement);
    }
    updateEnhancementLabels();
}

void AudioEqualizerWidget::on3DSurroundToggled(bool enabled)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        m_equalizer->set3DSurroundEnabled(enabled);
    }
    m_3dSurroundStrengthSlider->setEnabled(enabled);
}

void AudioEqualizerWidget::on3DSurroundStrengthChanged(int value)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        double strength = value / 100.0;
        m_equalizer->set3DSurroundStrength(strength);
    }
    updateEnhancementLabels();
}

void AudioEqualizerWidget::onReplayGainToggled(bool enabled)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        m_equalizer->setReplayGainEnabled(enabled);
    }
    m_replayGainModeCombo->setEnabled(enabled);
    m_replayGainPreampSpinBox->setEnabled(enabled);
}

void AudioEqualizerWidget::onReplayGainModeChanged(int index)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        QString mode = m_replayGainModeCombo->itemData(index).toString();
        m_equalizer->setReplayGainMode(mode);
    }
}

void AudioEqualizerWidget::onReplayGainPreampChanged(double value)
{
    if (m_equalizer && !m_updatingFromEqualizer) {
        m_equalizer->setReplayGainPreamp(value);
    }
}

void AudioEqualizerWidget::onResetClicked()
{
    if (m_equalizer) {
        int ret = QMessageBox::question(this, "Reset Equalizer",
                                       "Reset all equalizer settings to defaults?",
                                       QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            m_equalizer->resetToDefaults();
        }
    }
}

void AudioEqualizerWidget::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                   "Export Equalizer Settings",
                                                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/equalizer.json",
                                                   "JSON Files (*.json)");
    
    if (!fileName.isEmpty() && m_equalizer) {
        if (m_equalizer->exportSettings(fileName)) {
            QMessageBox::information(this, "Export Successful",
                                   "Equalizer settings exported successfully.");
        } else {
            QMessageBox::warning(this, "Export Failed",
                                "Failed to export equalizer settings.");
        }
    }
}

void AudioEqualizerWidget::onImportClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                   "Import Equalizer Settings",
                                                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                   "JSON Files (*.json)");
    
    if (!fileName.isEmpty() && m_equalizer) {
        if (m_equalizer->importSettings(fileName)) {
            QMessageBox::information(this, "Import Successful",
                                   "Equalizer settings imported successfully.");
        } else {
            QMessageBox::warning(this, "Import Failed",
                                "Failed to import equalizer settings.");
        }
    }
}
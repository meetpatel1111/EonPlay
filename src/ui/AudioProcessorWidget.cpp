#include "ui/AudioProcessorWidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(audioProcessorWidget)
Q_LOGGING_CATEGORY(audioProcessorWidget, "ui.audioProcessor")

AudioProcessorWidget::AudioProcessorWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_enableCheckBox(nullptr)
    , m_tabWidget(nullptr)
    , m_updatingFromProcessor(false)
{
    setupUI();
    setupConnections();
    
    qCDebug(audioProcessorWidget) << "AudioProcessorWidget created";
}

AudioProcessorWidget::~AudioProcessorWidget()
{
    qCDebug(audioProcessorWidget) << "AudioProcessorWidget destroyed";
}

void AudioProcessorWidget::initialize(std::shared_ptr<AudioProcessor> processor)
{
    m_processor = processor;
    
    if (m_processor) {
        // Connect to processor signals
        connect(m_processor.get(), &AudioProcessor::enabledChanged,
                this, &AudioProcessorWidget::onProcessorEnabledChanged);
        connect(m_processor.get(), &AudioProcessor::audioTrackChanged,
                this, &AudioProcessorWidget::onAudioTrackChanged);
        connect(m_processor.get(), &AudioProcessor::karaokeModeChanged,
                this, &AudioProcessorWidget::onKaraokeModeChanged);
        connect(m_processor.get(), &AudioProcessor::lyricsLoaded,
                this, &AudioProcessorWidget::onLyricsLoaded);
        connect(m_processor.get(), &AudioProcessor::currentLyricsChanged,
                this, &AudioProcessorWidget::onCurrentLyricsChanged);
        connect(m_processor.get(), &AudioProcessor::conversionProgress,
                this, &AudioProcessorWidget::onConversionProgress);
        connect(m_processor.get(), &AudioProcessor::conversionCompleted,
                this, &AudioProcessorWidget::onConversionCompleted);
        connect(m_processor.get(), &AudioProcessor::audioDelayChanged,
                this, &AudioProcessorWidget::onAudioDelayChanged);
        connect(m_processor.get(), &AudioProcessor::noiseReductionChanged,
                this, &AudioProcessorWidget::onNoiseReductionChanged);
        
        updateFromProcessor();
    }
    
    qCDebug(audioProcessorWidget) << "AudioProcessorWidget initialized";
}

void AudioProcessorWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);
    
    // Enable checkbox
    m_enableCheckBox = new QCheckBox("Enable Advanced Audio Processing", this);
    m_enableCheckBox->setStyleSheet("QCheckBox { font-weight: bold; }");
    m_mainLayout->addWidget(m_enableCheckBox);
    
    // Tab widget for different features
    m_tabWidget = new QTabWidget(this);
    
    setupAudioTrackTab();
    setupKaraokeTab();
    setupLyricsTab();
    setupConversionTab();
    setupEffectsTab();
    
    m_mainLayout->addWidget(m_tabWidget);
    
    // Initially disabled
    setWidgetsEnabled(false);
}

void AudioProcessorWidget::setupAudioTrackTab()
{
    m_audioTrackTab = new QWidget();
    m_audioTrackLayout = new QVBoxLayout(m_audioTrackTab);
    
    // Audio track selection
    QLabel* trackLabel = new QLabel("Audio Track:", m_audioTrackTab);
    m_audioTrackCombo = new QComboBox(m_audioTrackTab);
    m_audioTrackLayout->addWidget(trackLabel);
    m_audioTrackLayout->addWidget(m_audioTrackCombo);
    
    // Track information
    m_trackInfoLabel = new QLabel("No track information available", m_audioTrackTab);
    m_trackInfoLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; padding: 10px; border: 1px solid #ccc; }");
    m_trackInfoLabel->setWordWrap(true);
    m_audioTrackLayout->addWidget(m_trackInfoLabel);
    
    m_audioTrackLayout->addStretch();
    
    m_tabWidget->addTab(m_audioTrackTab, "Audio Tracks");
}

void AudioProcessorWidget::setupKaraokeTab()
{
    m_karaokeTab = new QWidget();
    m_karaokeLayout = new QGridLayout(m_karaokeTab);
    
    int row = 0;
    
    // Karaoke mode
    m_karaokeLayout->addWidget(new QLabel("Karaoke Mode:", m_karaokeTab), row, 0);
    m_karaokeModeCombo = new QComboBox(m_karaokeTab);
    m_karaokeModeCombo->addItem(AudioProcessor::getKaraokeModeName(AudioProcessor::Off), static_cast<int>(AudioProcessor::Off));
    m_karaokeModeCombo->addItem(AudioProcessor::getKaraokeModeName(AudioProcessor::VocalRemoval), static_cast<int>(AudioProcessor::VocalRemoval));
    m_karaokeModeCombo->addItem(AudioProcessor::getKaraokeModeName(AudioProcessor::VocalIsolation), static_cast<int>(AudioProcessor::VocalIsolation));
    m_karaokeModeCombo->addItem(AudioProcessor::getKaraokeModeName(AudioProcessor::CenterChannelExtraction), static_cast<int>(AudioProcessor::CenterChannelExtraction));
    m_karaokeLayout->addWidget(m_karaokeModeCombo, row, 1, 1, 2);
    row++;
    
    // Vocal removal strength
    m_karaokeLayout->addWidget(new QLabel("Vocal Strength:", m_karaokeTab), row, 0);
    m_vocalStrengthSlider = new QSlider(Qt::Horizontal, m_karaokeTab);
    m_vocalStrengthSlider->setRange(0, 100);
    m_vocalStrengthSlider->setValue(80);
    m_vocalStrengthLabel = new QLabel("80%", m_karaokeTab);
    m_vocalStrengthLabel->setMinimumWidth(40);
    m_karaokeLayout->addWidget(m_vocalStrengthSlider, row, 1);
    m_karaokeLayout->addWidget(m_vocalStrengthLabel, row, 2);
    row++;
    
    // Frequency range
    m_karaokeLayout->addWidget(new QLabel("Low Freq (Hz):", m_karaokeTab), row, 0);
    m_lowFreqSpinBox = new QSpinBox(m_karaokeTab);
    m_lowFreqSpinBox->setRange(20, 2000);
    m_lowFreqSpinBox->setValue(200);
    m_karaokeLayout->addWidget(m_lowFreqSpinBox, row, 1, 1, 2);
    row++;
    
    m_karaokeLayout->addWidget(new QLabel("High Freq (Hz):", m_karaokeTab), row, 0);
    m_highFreqSpinBox = new QSpinBox(m_karaokeTab);
    m_highFreqSpinBox->setRange(1000, 20000);
    m_highFreqSpinBox->setValue(4000);
    m_karaokeLayout->addWidget(m_highFreqSpinBox, row, 1, 1, 2);
    
    m_tabWidget->addTab(m_karaokeTab, "Karaoke");
}

void AudioProcessorWidget::setupLyricsTab()
{
    m_lyricsTab = new QWidget();
    m_lyricsLayout = new QVBoxLayout(m_lyricsTab);
    
    // Lyrics buttons
    m_lyricsButtonLayout = new QHBoxLayout();
    m_loadLyricsButton = new QPushButton("Load Lyrics", m_lyricsTab);
    m_clearLyricsButton = new QPushButton("Clear", m_lyricsTab);
    m_exportLyricsButton = new QPushButton("Export", m_lyricsTab);
    
    m_lyricsButtonLayout->addWidget(m_loadLyricsButton);
    m_lyricsButtonLayout->addWidget(m_clearLyricsButton);
    m_lyricsButtonLayout->addWidget(m_exportLyricsButton);
    m_lyricsButtonLayout->addStretch();
    
    m_lyricsLayout->addLayout(m_lyricsButtonLayout);
    
    // Lyrics display
    m_lyricsDisplay = new QTextEdit(m_lyricsTab);
    m_lyricsDisplay->setReadOnly(true);
    m_lyricsDisplay->setPlaceholderText("No lyrics loaded. Click 'Load Lyrics' to load from file.");
    m_lyricsLayout->addWidget(m_lyricsDisplay);
    
    // Status label
    m_lyricsStatusLabel = new QLabel("No lyrics loaded", m_lyricsTab);
    m_lyricsStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_lyricsLayout->addWidget(m_lyricsStatusLabel);
    
    m_tabWidget->addTab(m_lyricsTab, "Lyrics");
}

void AudioProcessorWidget::setupConversionTab()
{
    m_conversionTab = new QWidget();
    m_conversionLayout = new QGridLayout(m_conversionTab);
    
    int row = 0;
    
    // Input format
    m_conversionLayout->addWidget(new QLabel("Input Format:", m_conversionTab), row, 0);
    m_inputFormatCombo = new QComboBox(m_conversionTab);
    m_inputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::MP3), static_cast<int>(AudioProcessor::MP3));
    m_inputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::FLAC), static_cast<int>(AudioProcessor::FLAC));
    m_inputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::WAV), static_cast<int>(AudioProcessor::WAV));
    m_inputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::OGG), static_cast<int>(AudioProcessor::OGG));
    m_conversionLayout->addWidget(m_inputFormatCombo, row, 1);
    row++;
    
    // Output format
    m_conversionLayout->addWidget(new QLabel("Output Format:", m_conversionTab), row, 0);
    m_outputFormatCombo = new QComboBox(m_conversionTab);
    m_outputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::MP3), static_cast<int>(AudioProcessor::MP3));
    m_outputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::FLAC), static_cast<int>(AudioProcessor::FLAC));
    m_outputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::WAV), static_cast<int>(AudioProcessor::WAV));
    m_outputFormatCombo->addItem(AudioProcessor::getFormatName(AudioProcessor::OGG), static_cast<int>(AudioProcessor::OGG));
    m_conversionLayout->addWidget(m_outputFormatCombo, row, 1);
    row++;
    
    // Quality
    m_conversionLayout->addWidget(new QLabel("Quality:", m_conversionTab), row, 0);
    m_qualitySlider = new QSlider(Qt::Horizontal, m_conversionTab);
    m_qualitySlider->setRange(0, 100);
    m_qualitySlider->setValue(80);
    m_qualityLabel = new QLabel("80%", m_conversionTab);
    m_conversionLayout->addWidget(m_qualitySlider, row, 1);
    m_conversionLayout->addWidget(m_qualityLabel, row, 2);
    row++;
    
    // Convert button
    m_convertButton = new QPushButton("Convert Audio", m_conversionTab);
    m_cancelConversionButton = new QPushButton("Cancel", m_conversionTab);
    m_cancelConversionButton->setEnabled(false);
    m_conversionLayout->addWidget(m_convertButton, row, 0);
    m_conversionLayout->addWidget(m_cancelConversionButton, row, 1);
    row++;
    
    // Progress bar
    m_conversionProgress = new QProgressBar(m_conversionTab);
    m_conversionProgress->setVisible(false);
    m_conversionLayout->addWidget(m_conversionProgress, row, 0, 1, 3);
    row++;
    
    // Status label
    m_conversionStatusLabel = new QLabel("Ready for conversion", m_conversionTab);
    m_conversionStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_conversionLayout->addWidget(m_conversionStatusLabel, row, 0, 1, 3);
    
    m_tabWidget->addTab(m_conversionTab, "Conversion");
}

void AudioProcessorWidget::setupEffectsTab()
{
    m_effectsTab = new QWidget();
    m_effectsLayout = new QGridLayout(m_effectsTab);
    
    int row = 0;
    
    // Audio delay
    m_effectsLayout->addWidget(new QLabel("Audio Delay (ms):", m_effectsTab), row, 0);
    m_audioDelaySpinBox = new QSpinBox(m_effectsTab);
    m_audioDelaySpinBox->setRange(-1000, 1000);
    m_audioDelaySpinBox->setValue(0);
    m_audioDelaySpinBox->setSuffix(" ms");
    m_effectsLayout->addWidget(m_audioDelaySpinBox, row, 1);
    row++;
    
    // Noise reduction
    m_noiseReductionCheckBox = new QCheckBox("Enable Noise Reduction", m_effectsTab);
    m_effectsLayout->addWidget(m_noiseReductionCheckBox, row, 0, 1, 2);
    row++;
    
    m_effectsLayout->addWidget(new QLabel("Noise Reduction Strength:", m_effectsTab), row, 0);
    m_noiseStrengthSlider = new QSlider(Qt::Horizontal, m_effectsTab);
    m_noiseStrengthSlider->setRange(0, 100);
    m_noiseStrengthSlider->setValue(30);
    m_noiseStrengthSlider->setEnabled(false);
    m_noiseStrengthLabel = new QLabel("30%", m_effectsTab);
    m_effectsLayout->addWidget(m_noiseStrengthSlider, row, 1);
    m_effectsLayout->addWidget(m_noiseStrengthLabel, row, 2);
    
    m_tabWidget->addTab(m_effectsTab, "Effects");
}

void AudioProcessorWidget::setupConnections()
{
    // Enable checkbox
    connect(m_enableCheckBox, &QCheckBox::toggled,
            this, &AudioProcessorWidget::onEnableCheckBoxToggled);
    
    // Audio track tab
    connect(m_audioTrackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioProcessorWidget::onAudioTrackComboChanged);
    
    // Karaoke tab
    connect(m_karaokeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioProcessorWidget::onKaraokeModeComboChanged);
    connect(m_vocalStrengthSlider, &QSlider::valueChanged,
            this, &AudioProcessorWidget::onVocalStrengthSliderChanged);
    connect(m_lowFreqSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AudioProcessorWidget::onFrequencyRangeChanged);
    connect(m_highFreqSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AudioProcessorWidget::onFrequencyRangeChanged);
    
    // Lyrics tab
    connect(m_loadLyricsButton, &QPushButton::clicked,
            this, &AudioProcessorWidget::onLoadLyricsClicked);
    connect(m_clearLyricsButton, &QPushButton::clicked,
            this, &AudioProcessorWidget::onClearLyricsClicked);
    connect(m_exportLyricsButton, &QPushButton::clicked,
            this, &AudioProcessorWidget::onExportLyricsClicked);
    
    // Conversion tab
    connect(m_convertButton, &QPushButton::clicked,
            this, &AudioProcessorWidget::onConvertAudioClicked);
    connect(m_cancelConversionButton, &QPushButton::clicked,
            this, &AudioProcessorWidget::onCancelConversionClicked);
    connect(m_qualitySlider, &QSlider::valueChanged,
            [this](int value) { m_qualityLabel->setText(QString("%1%").arg(value)); });
    
    // Effects tab
    connect(m_audioDelaySpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AudioProcessorWidget::onAudioDelaySpinBoxChanged);
    connect(m_noiseReductionCheckBox, &QCheckBox::toggled,
            this, &AudioProcessorWidget::onNoiseReductionToggled);
    connect(m_noiseStrengthSlider, &QSlider::valueChanged,
            this, &AudioProcessorWidget::onNoiseStrengthSliderChanged);
}
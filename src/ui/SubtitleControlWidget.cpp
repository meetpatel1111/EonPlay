#include "ui/SubtitleControlWidget.h"
#include "subtitles/SubtitleManager.h"
#include <QColorDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(subtitleControls, "eonplay.subtitles.controls")

SubtitleControlWidget::SubtitleControlWidget(QWidget* parent)
    : QWidget(parent)
    , m_subtitleManager(nullptr)
    , m_subtitleRenderer(nullptr)
    , m_mainLayout(nullptr)
    , m_advancedVisible(false)
    , m_updatingControls(false)
{
    m_currentFont = QFont("Arial", 16, QFont::Bold);
    
    initializeUI();
    
    qCDebug(subtitleControls) << "SubtitleControlWidget created";
}

SubtitleControlWidget::~SubtitleControlWidget()
{
    qCDebug(subtitleControls) << "SubtitleControlWidget destroyed";
}

void SubtitleControlWidget::setSubtitleManager(SubtitleManager* manager)
{
    // Disconnect from previous manager
    if (m_subtitleManager) {
        disconnect(m_subtitleManager, nullptr, this, nullptr);
    }
    
    m_subtitleManager = manager;
    
    // Connect to new manager
    if (m_subtitleManager) {
        connect(m_subtitleManager, &SubtitleManager::activeTrackChanged,
                this, &SubtitleControlWidget::onActiveTrackChanged);
        connect(m_subtitleManager, &SubtitleManager::subtitlesEnabledChanged,
                this, &SubtitleControlWidget::onSubtitlesEnabledChanged);
        connect(m_subtitleManager, &SubtitleManager::timingOffsetChanged,
                this, &SubtitleControlWidget::onTimingOffsetChanged);
        
        // Connect control signals to manager
        connect(this, &SubtitleControlWidget::trackSelectionChanged,
                m_subtitleManager, &SubtitleManager::setActiveTrack);
        connect(this, &SubtitleControlWidget::subtitlesEnabledChanged,
                m_subtitleManager, &SubtitleManager::setEnabled);
        connect(this, &SubtitleControlWidget::timingOffsetChanged,
                m_subtitleManager, &SubtitleManager::setTimingOffset);
        
        updateTrackComboBox();
        updateControls();
    }
}

void SubtitleControlWidget::setSubtitleRenderer(SubtitleRenderer* renderer)
{
    // Disconnect from previous renderer
    if (m_subtitleRenderer) {
        disconnect(m_subtitleRenderer, nullptr, this, nullptr);
        disconnect(this, nullptr, m_subtitleRenderer, nullptr);
    }
    
    m_subtitleRenderer = renderer;
    
    // Connect to new renderer
    if (m_subtitleRenderer) {
        connect(this, &SubtitleControlWidget::renderSettingsChanged,
                m_subtitleRenderer, &SubtitleRenderer::setRenderSettings);
        
        updateControls();
    }
}

void SubtitleControlWidget::updateControls()
{
    if (m_updatingControls) {
        return;
    }
    
    m_updatingControls = true;
    
    // Update from subtitle manager
    if (m_subtitleManager) {
        m_enableCheckBox->setChecked(m_subtitleManager->isEnabled());
        m_trackComboBox->setCurrentIndex(m_subtitleManager->activeTrack() + 1); // +1 for "Disabled" option
        
        qint64 offset = m_subtitleManager->timingOffset();
        m_timingOffsetSlider->setValue(static_cast<int>(offset / 100)); // Convert to deciseconds
        m_timingOffsetLabel->setText(QString("%1 ms").arg(offset));
    }
    
    // Update from subtitle renderer
    if (m_subtitleRenderer) {
        SubtitleRenderSettings settings = m_subtitleRenderer->renderSettings();
        applySettingsToControls(settings);
    }
    
    m_updatingControls = false;
}

void SubtitleControlWidget::setAdvancedControlsVisible(bool visible)
{
    if (m_advancedVisible != visible) {
        m_advancedVisible = visible;
        m_advancedGroup->setVisible(visible);
        m_advancedToggleButton->setText(visible ? tr("Hide Advanced") : tr("Show Advanced"));
        
        qCDebug(subtitleControls) << "Advanced controls" << (visible ? "shown" : "hidden");
    }
}

void SubtitleControlWidget::onActiveTrackChanged(int trackIndex)
{
    if (!m_updatingControls) {
        m_trackComboBox->setCurrentIndex(trackIndex + 1); // +1 for "Disabled" option
    }
}

void SubtitleControlWidget::onSubtitlesEnabledChanged(bool enabled)
{
    if (!m_updatingControls) {
        m_enableCheckBox->setChecked(enabled);
    }
}

void SubtitleControlWidget::onTimingOffsetChanged(qint64 offsetMs)
{
    if (!m_updatingControls) {
        m_timingOffsetSlider->setValue(static_cast<int>(offsetMs / 100));
        m_timingOffsetLabel->setText(QString("%1 ms").arg(offsetMs));
    }
}

void SubtitleControlWidget::onEnableToggled(bool enabled)
{
    if (!m_updatingControls) {
        emit subtitlesEnabledChanged(enabled);
    }
}

void SubtitleControlWidget::onTrackChanged(int index)
{
    if (!m_updatingControls) {
        emit trackSelectionChanged(index - 1); // -1 for "Disabled" option
    }
}

void SubtitleControlWidget::onTimingOffsetSliderChanged(int value)
{
    if (!m_updatingControls) {
        qint64 offsetMs = value * 100; // Convert from deciseconds
        m_timingOffsetLabel->setText(QString("%1 ms").arg(offsetMs));
        emit timingOffsetChanged(offsetMs);
    }
}

void SubtitleControlWidget::onOpacityChanged(int value)
{
    if (!m_updatingControls) {
        double opacity = value / 100.0;
        m_opacityLabel->setText(QString("%1%").arg(value));
        
        if (m_subtitleRenderer) {
            m_subtitleRenderer->setOpacity(opacity);
        }
    }
}

void SubtitleControlWidget::onScaleChanged(int value)
{
    if (!m_updatingControls) {
        double scale = value / 100.0;
        m_scaleLabel->setText(QString("%1%").arg(value));
        
        if (m_subtitleRenderer) {
            m_subtitleRenderer->setScale(scale);
        }
    }
}

void SubtitleControlWidget::onFontButtonClicked()
{
    bool ok;
    QFont newFont = QFontDialog::getFont(&ok, m_currentFont, this, tr("Select Subtitle Font"));
    
    if (ok) {
        m_currentFont = newFont;
        m_fontButton->setText(QString("%1, %2pt").arg(newFont.family()).arg(newFont.pointSize()));
        
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.font = newFont;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onTextColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QColor currentColor = button->property("color").value<QColor>();
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Text Color"));
    
    if (newColor.isValid()) {
        updateColorButton(button, newColor);
        
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.textColor = newColor;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onOutlineColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QColor currentColor = button->property("color").value<QColor>();
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Outline Color"));
    
    if (newColor.isValid()) {
        updateColorButton(button, newColor);
        
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.outlineColor = newColor;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onBackgroundColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QColor currentColor = button->property("color").value<QColor>();
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Background Color"));
    
    if (newColor.isValid()) {
        updateColorButton(button, newColor);
        
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.backgroundColor = newColor;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onShadowColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QColor currentColor = button->property("color").value<QColor>();
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Shadow Color"));
    
    if (newColor.isValid()) {
        updateColorButton(button, newColor);
        
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.shadowColor = newColor;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onOutlineWidthChanged(int value)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.outlineWidth = value;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onShadowOffsetChanged(int value)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.shadowOffset = value;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onEnableOutlineToggled(bool enabled)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.enableOutline = enabled;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onEnableShadowToggled(bool enabled)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.enableShadow = enabled;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onEnableBackgroundToggled(bool enabled)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.enableBackground = enabled;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onAlignmentChanged()
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        
        int index = m_alignmentComboBox->currentIndex();
        switch (index) {
        case 0: settings.alignment = Qt::AlignTop | Qt::AlignLeft; break;
        case 1: settings.alignment = Qt::AlignTop | Qt::AlignHCenter; break;
        case 2: settings.alignment = Qt::AlignTop | Qt::AlignRight; break;
        case 3: settings.alignment = Qt::AlignVCenter | Qt::AlignLeft; break;
        case 4: settings.alignment = Qt::AlignVCenter | Qt::AlignHCenter; break;
        case 5: settings.alignment = Qt::AlignVCenter | Qt::AlignRight; break;
        case 6: settings.alignment = Qt::AlignBottom | Qt::AlignLeft; break;
        case 7: settings.alignment = Qt::AlignBottom | Qt::AlignHCenter; break;
        case 8: settings.alignment = Qt::AlignBottom | Qt::AlignRight; break;
        default: settings.alignment = Qt::AlignBottom | Qt::AlignHCenter; break;
        }
        
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onMarginChanged()
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.marginLeft = m_marginLeftSpinBox->value();
        settings.marginRight = m_marginRightSpinBox->value();
        settings.marginTop = m_marginTopSpinBox->value();
        settings.marginBottom = m_marginBottomSpinBox->value();
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onFadeInToggled(bool enabled)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.enableFadeIn = enabled;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onFadeOutToggled(bool enabled)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.enableFadeOut = enabled;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onFadeInDurationChanged(int value)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.fadeInDuration = value;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onFadeOutDurationChanged(int value)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.fadeOutDuration = value;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onMaxLinesChanged(int value)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.maxLines = value;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onLineSpacingChanged(int value)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.lineSpacing = value;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onWordWrapToggled(bool enabled)
{
    if (!m_updatingControls) {
        SubtitleRenderSettings settings = getCurrentSettings();
        settings.wordWrap = enabled;
        emit renderSettingsChanged(settings);
    }
}

void SubtitleControlWidget::onLoadPreset()
{
    // TODO: Implement preset loading
    QMessageBox::information(this, tr("Load Preset"), tr("Preset loading will be implemented in a future version."));
}

void SubtitleControlWidget::onSavePreset()
{
    // TODO: Implement preset saving
    QMessageBox::information(this, tr("Save Preset"), tr("Preset saving will be implemented in a future version."));
}

void SubtitleControlWidget::onResetToDefaults()
{
    SubtitleRenderSettings defaultSettings;
    applySettingsToControls(defaultSettings);
    emit renderSettingsChanged(defaultSettings);
    
    qCDebug(subtitleControls) << "Reset to default settings";
}

void SubtitleControlWidget::onToggleAdvanced()
{
    setAdvancedControlsVisible(!m_advancedVisible);
}vo
id SubtitleControlWidget::initializeUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // Create control groups
    m_basicGroup = createBasicControlsGroup();
    m_fontColorGroup = createFontColorGroup();
    m_outlineShadowGroup = createOutlineShadowGroup();
    m_positioningGroup = createPositioningGroup();
    m_animationGroup = createAnimationGroup();
    m_advancedGroup = createAdvancedGroup();
    
    // Add groups to layout
    m_mainLayout->addWidget(m_basicGroup);
    m_mainLayout->addWidget(m_fontColorGroup);
    m_mainLayout->addWidget(m_outlineShadowGroup);
    m_mainLayout->addWidget(m_positioningGroup);
    m_mainLayout->addWidget(m_animationGroup);
    m_mainLayout->addWidget(m_advancedGroup);
    
    // Create control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_loadPresetButton = new QPushButton(tr("Load Preset"));
    m_savePresetButton = new QPushButton(tr("Save Preset"));
    m_resetButton = new QPushButton(tr("Reset to Defaults"));
    m_advancedToggleButton = new QPushButton(tr("Show Advanced"));
    
    connect(m_loadPresetButton, &QPushButton::clicked, this, &SubtitleControlWidget::onLoadPreset);
    connect(m_savePresetButton, &QPushButton::clicked, this, &SubtitleControlWidget::onSavePreset);
    connect(m_resetButton, &QPushButton::clicked, this, &SubtitleControlWidget::onResetToDefaults);
    connect(m_advancedToggleButton, &QPushButton::clicked, this, &SubtitleControlWidget::onToggleAdvanced);
    
    buttonLayout->addWidget(m_loadPresetButton);
    buttonLayout->addWidget(m_savePresetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addWidget(m_advancedToggleButton);
    
    m_mainLayout->addLayout(buttonLayout);
    m_mainLayout->addStretch();
    
    // Initially hide advanced controls
    setAdvancedControlsVisible(false);
}

QGroupBox* SubtitleControlWidget::createBasicControlsGroup()
{
    QGroupBox* group = new QGroupBox(tr("Basic Controls"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    
    // Enable checkbox
    m_enableCheckBox = new QCheckBox(tr("Enable Subtitles"));
    m_enableCheckBox->setChecked(true);
    connect(m_enableCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onEnableToggled);
    layout->addWidget(m_enableCheckBox);
    
    // Track selection
    QHBoxLayout* trackLayout = new QHBoxLayout();
    trackLayout->addWidget(new QLabel(tr("Track:")));
    m_trackComboBox = new QComboBox();
    connect(m_trackComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SubtitleControlWidget::onTrackChanged);
    trackLayout->addWidget(m_trackComboBox);
    layout->addLayout(trackLayout);
    
    // Timing offset
    QHBoxLayout* timingLayout = new QHBoxLayout();
    timingLayout->addWidget(new QLabel(tr("Timing Offset:")));
    m_timingOffsetSlider = new QSlider(Qt::Horizontal);
    m_timingOffsetSlider->setRange(-50000, 50000); // -5000ms to +5000ms in deciseconds
    m_timingOffsetSlider->setValue(0);
    connect(m_timingOffsetSlider, &QSlider::valueChanged, this, &SubtitleControlWidget::onTimingOffsetSliderChanged);
    m_timingOffsetLabel = new QLabel("0 ms");
    m_timingOffsetLabel->setMinimumWidth(60);
    timingLayout->addWidget(m_timingOffsetSlider);
    timingLayout->addWidget(m_timingOffsetLabel);
    layout->addLayout(timingLayout);
    
    // Opacity
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    opacityLayout->addWidget(new QLabel(tr("Opacity:")));
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &SubtitleControlWidget::onOpacityChanged);
    m_opacityLabel = new QLabel("100%");
    m_opacityLabel->setMinimumWidth(40);
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityLabel);
    layout->addLayout(opacityLayout);
    
    // Scale
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    scaleLayout->addWidget(new QLabel(tr("Scale:")));
    m_scaleSlider = new QSlider(Qt::Horizontal);
    m_scaleSlider->setRange(50, 300);
    m_scaleSlider->setValue(100);
    connect(m_scaleSlider, &QSlider::valueChanged, this, &SubtitleControlWidget::onScaleChanged);
    m_scaleLabel = new QLabel("100%");
    m_scaleLabel->setMinimumWidth(40);
    scaleLayout->addWidget(m_scaleSlider);
    scaleLayout->addWidget(m_scaleLabel);
    layout->addLayout(scaleLayout);
    
    return group;
}

QGroupBox* SubtitleControlWidget::createFontColorGroup()
{
    QGroupBox* group = new QGroupBox(tr("Font & Colors"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    
    // Font selection
    QHBoxLayout* fontLayout = new QHBoxLayout();
    fontLayout->addWidget(new QLabel(tr("Font:")));
    m_fontButton = new QPushButton(QString("%1, %2pt").arg(m_currentFont.family()).arg(m_currentFont.pointSize()));
    connect(m_fontButton, &QPushButton::clicked, this, &SubtitleControlWidget::onFontButtonClicked);
    fontLayout->addWidget(m_fontButton);
    layout->addLayout(fontLayout);
    
    // Color buttons
    QGridLayout* colorLayout = new QGridLayout();
    
    colorLayout->addWidget(new QLabel(tr("Text:")), 0, 0);
    m_textColorButton = createColorButton(Qt::white);
    connect(m_textColorButton, &QPushButton::clicked, this, &SubtitleControlWidget::onTextColorButtonClicked);
    colorLayout->addWidget(m_textColorButton, 0, 1);
    
    colorLayout->addWidget(new QLabel(tr("Outline:")), 0, 2);
    m_outlineColorButton = createColorButton(Qt::black);
    connect(m_outlineColorButton, &QPushButton::clicked, this, &SubtitleControlWidget::onOutlineColorButtonClicked);
    colorLayout->addWidget(m_outlineColorButton, 0, 3);
    
    colorLayout->addWidget(new QLabel(tr("Background:")), 1, 0);
    m_backgroundColorButton = createColorButton(Qt::transparent);
    connect(m_backgroundColorButton, &QPushButton::clicked, this, &SubtitleControlWidget::onBackgroundColorButtonClicked);
    colorLayout->addWidget(m_backgroundColorButton, 1, 1);
    
    colorLayout->addWidget(new QLabel(tr("Shadow:")), 1, 2);
    m_shadowColorButton = createColorButton(QColor(0, 0, 0, 128));
    connect(m_shadowColorButton, &QPushButton::clicked, this, &SubtitleControlWidget::onShadowColorButtonClicked);
    colorLayout->addWidget(m_shadowColorButton, 1, 3);
    
    layout->addLayout(colorLayout);
    
    return group;
}

QGroupBox* SubtitleControlWidget::createOutlineShadowGroup()
{
    QGroupBox* group = new QGroupBox(tr("Outline & Shadow"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    
    // Outline controls
    QHBoxLayout* outlineLayout = new QHBoxLayout();
    m_enableOutlineCheckBox = new QCheckBox(tr("Enable Outline"));
    m_enableOutlineCheckBox->setChecked(true);
    connect(m_enableOutlineCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onEnableOutlineToggled);
    outlineLayout->addWidget(m_enableOutlineCheckBox);
    
    outlineLayout->addWidget(new QLabel(tr("Width:")));
    m_outlineWidthSpinBox = new QSpinBox();
    m_outlineWidthSpinBox->setRange(0, 10);
    m_outlineWidthSpinBox->setValue(2);
    connect(m_outlineWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onOutlineWidthChanged);
    outlineLayout->addWidget(m_outlineWidthSpinBox);
    
    layout->addLayout(outlineLayout);
    
    // Shadow controls
    QHBoxLayout* shadowLayout = new QHBoxLayout();
    m_enableShadowCheckBox = new QCheckBox(tr("Enable Shadow"));
    m_enableShadowCheckBox->setChecked(true);
    connect(m_enableShadowCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onEnableShadowToggled);
    shadowLayout->addWidget(m_enableShadowCheckBox);
    
    shadowLayout->addWidget(new QLabel(tr("Offset:")));
    m_shadowOffsetSpinBox = new QSpinBox();
    m_shadowOffsetSpinBox->setRange(0, 20);
    m_shadowOffsetSpinBox->setValue(2);
    connect(m_shadowOffsetSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onShadowOffsetChanged);
    shadowLayout->addWidget(m_shadowOffsetSpinBox);
    
    layout->addLayout(shadowLayout);
    
    // Background
    m_enableBackgroundCheckBox = new QCheckBox(tr("Enable Background"));
    m_enableBackgroundCheckBox->setChecked(false);
    connect(m_enableBackgroundCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onEnableBackgroundToggled);
    layout->addWidget(m_enableBackgroundCheckBox);
    
    return group;
}

QGroupBox* SubtitleControlWidget::createPositioningGroup()
{
    QGroupBox* group = new QGroupBox(tr("Positioning"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    
    // Alignment
    QHBoxLayout* alignLayout = new QHBoxLayout();
    alignLayout->addWidget(new QLabel(tr("Alignment:")));
    m_alignmentComboBox = new QComboBox();
    m_alignmentComboBox->addItems({
        tr("Top Left"), tr("Top Center"), tr("Top Right"),
        tr("Middle Left"), tr("Middle Center"), tr("Middle Right"),
        tr("Bottom Left"), tr("Bottom Center"), tr("Bottom Right")
    });
    m_alignmentComboBox->setCurrentIndex(7); // Bottom Center
    connect(m_alignmentComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SubtitleControlWidget::onAlignmentChanged);
    alignLayout->addWidget(m_alignmentComboBox);
    layout->addLayout(alignLayout);
    
    // Margins
    QGridLayout* marginLayout = new QGridLayout();
    
    marginLayout->addWidget(new QLabel(tr("Left:")), 0, 0);
    m_marginLeftSpinBox = new QSpinBox();
    m_marginLeftSpinBox->setRange(0, 200);
    m_marginLeftSpinBox->setValue(20);
    connect(m_marginLeftSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onMarginChanged);
    marginLayout->addWidget(m_marginLeftSpinBox, 0, 1);
    
    marginLayout->addWidget(new QLabel(tr("Right:")), 0, 2);
    m_marginRightSpinBox = new QSpinBox();
    m_marginRightSpinBox->setRange(0, 200);
    m_marginRightSpinBox->setValue(20);
    connect(m_marginRightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onMarginChanged);
    marginLayout->addWidget(m_marginRightSpinBox, 0, 3);
    
    marginLayout->addWidget(new QLabel(tr("Top:")), 1, 0);
    m_marginTopSpinBox = new QSpinBox();
    m_marginTopSpinBox->setRange(0, 200);
    m_marginTopSpinBox->setValue(20);
    connect(m_marginTopSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onMarginChanged);
    marginLayout->addWidget(m_marginTopSpinBox, 1, 1);
    
    marginLayout->addWidget(new QLabel(tr("Bottom:")), 1, 2);
    m_marginBottomSpinBox = new QSpinBox();
    m_marginBottomSpinBox->setRange(0, 200);
    m_marginBottomSpinBox->setValue(50);
    connect(m_marginBottomSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onMarginChanged);
    marginLayout->addWidget(m_marginBottomSpinBox, 1, 3);
    
    layout->addLayout(marginLayout);
    
    return group;
}

QGroupBox* SubtitleControlWidget::createAnimationGroup()
{
    QGroupBox* group = new QGroupBox(tr("Animation"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    
    // Fade in
    QHBoxLayout* fadeInLayout = new QHBoxLayout();
    m_enableFadeInCheckBox = new QCheckBox(tr("Fade In"));
    m_enableFadeInCheckBox->setChecked(true);
    connect(m_enableFadeInCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onFadeInToggled);
    fadeInLayout->addWidget(m_enableFadeInCheckBox);
    
    fadeInLayout->addWidget(new QLabel(tr("Duration:")));
    m_fadeInDurationSpinBox = new QSpinBox();
    m_fadeInDurationSpinBox->setRange(0, 2000);
    m_fadeInDurationSpinBox->setValue(200);
    m_fadeInDurationSpinBox->setSuffix(" ms");
    connect(m_fadeInDurationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onFadeInDurationChanged);
    fadeInLayout->addWidget(m_fadeInDurationSpinBox);
    
    layout->addLayout(fadeInLayout);
    
    // Fade out
    QHBoxLayout* fadeOutLayout = new QHBoxLayout();
    m_enableFadeOutCheckBox = new QCheckBox(tr("Fade Out"));
    m_enableFadeOutCheckBox->setChecked(true);
    connect(m_enableFadeOutCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onFadeOutToggled);
    fadeOutLayout->addWidget(m_enableFadeOutCheckBox);
    
    fadeOutLayout->addWidget(new QLabel(tr("Duration:")));
    m_fadeOutDurationSpinBox = new QSpinBox();
    m_fadeOutDurationSpinBox->setRange(0, 2000);
    m_fadeOutDurationSpinBox->setValue(200);
    m_fadeOutDurationSpinBox->setSuffix(" ms");
    connect(m_fadeOutDurationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onFadeOutDurationChanged);
    fadeOutLayout->addWidget(m_fadeOutDurationSpinBox);
    
    layout->addLayout(fadeOutLayout);
    
    return group;
}

QGroupBox* SubtitleControlWidget::createAdvancedGroup()
{
    QGroupBox* group = new QGroupBox(tr("Advanced"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    
    // Max lines
    QHBoxLayout* maxLinesLayout = new QHBoxLayout();
    maxLinesLayout->addWidget(new QLabel(tr("Max Lines:")));
    m_maxLinesSpinBox = new QSpinBox();
    m_maxLinesSpinBox->setRange(1, 10);
    m_maxLinesSpinBox->setValue(3);
    connect(m_maxLinesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onMaxLinesChanged);
    maxLinesLayout->addWidget(m_maxLinesSpinBox);
    maxLinesLayout->addStretch();
    layout->addLayout(maxLinesLayout);
    
    // Line spacing
    QHBoxLayout* spacingLayout = new QHBoxLayout();
    spacingLayout->addWidget(new QLabel(tr("Line Spacing:")));
    m_lineSpacingSpinBox = new QSpinBox();
    m_lineSpacingSpinBox->setRange(0, 20);
    m_lineSpacingSpinBox->setValue(5);
    connect(m_lineSpacingSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SubtitleControlWidget::onLineSpacingChanged);
    spacingLayout->addWidget(m_lineSpacingSpinBox);
    spacingLayout->addStretch();
    layout->addLayout(spacingLayout);
    
    // Word wrap
    m_wordWrapCheckBox = new QCheckBox(tr("Word Wrap"));
    m_wordWrapCheckBox->setChecked(true);
    connect(m_wordWrapCheckBox, &QCheckBox::toggled, this, &SubtitleControlWidget::onWordWrapToggled);
    layout->addWidget(m_wordWrapCheckBox);
    
    return group;
}

QPushButton* SubtitleControlWidget::createColorButton(const QColor& color)
{
    QPushButton* button = new QPushButton();
    button->setFixedSize(40, 25);
    button->setProperty("color", color);
    updateColorButton(button, color);
    return button;
}

void SubtitleControlWidget::updateColorButton(QPushButton* button, const QColor& color)
{
    button->setProperty("color", color);
    
    QString colorName = color.name();
    if (color.alpha() < 255) {
        colorName = QString("rgba(%1,%2,%3,%4)")
            .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
    }
    
    button->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #666; }").arg(colorName));
    button->setToolTip(colorName);
}

SubtitleRenderSettings SubtitleControlWidget::getCurrentSettings() const
{
    SubtitleRenderSettings settings;
    
    settings.font = m_currentFont;
    settings.textColor = m_textColorButton->property("color").value<QColor>();
    settings.backgroundColor = m_backgroundColorButton->property("color").value<QColor>();
    settings.outlineColor = m_outlineColorButton->property("color").value<QColor>();
    settings.shadowColor = m_shadowColorButton->property("color").value<QColor>();
    
    settings.outlineWidth = m_outlineWidthSpinBox->value();
    settings.shadowOffset = m_shadowOffsetSpinBox->value();
    settings.enableOutline = m_enableOutlineCheckBox->isChecked();
    settings.enableShadow = m_enableShadowCheckBox->isChecked();
    settings.enableBackground = m_enableBackgroundCheckBox->isChecked();
    
    // Alignment from combo box
    int alignIndex = m_alignmentComboBox->currentIndex();
    switch (alignIndex) {
    case 0: settings.alignment = Qt::AlignTop | Qt::AlignLeft; break;
    case 1: settings.alignment = Qt::AlignTop | Qt::AlignHCenter; break;
    case 2: settings.alignment = Qt::AlignTop | Qt::AlignRight; break;
    case 3: settings.alignment = Qt::AlignVCenter | Qt::AlignLeft; break;
    case 4: settings.alignment = Qt::AlignVCenter | Qt::AlignHCenter; break;
    case 5: settings.alignment = Qt::AlignVCenter | Qt::AlignRight; break;
    case 6: settings.alignment = Qt::AlignBottom | Qt::AlignLeft; break;
    case 7: settings.alignment = Qt::AlignBottom | Qt::AlignHCenter; break;
    case 8: settings.alignment = Qt::AlignBottom | Qt::AlignRight; break;
    default: settings.alignment = Qt::AlignBottom | Qt::AlignHCenter; break;
    }
    
    settings.marginLeft = m_marginLeftSpinBox->value();
    settings.marginRight = m_marginRightSpinBox->value();
    settings.marginTop = m_marginTopSpinBox->value();
    settings.marginBottom = m_marginBottomSpinBox->value();
    
    settings.enableFadeIn = m_enableFadeInCheckBox->isChecked();
    settings.enableFadeOut = m_enableFadeOutCheckBox->isChecked();
    settings.fadeInDuration = m_fadeInDurationSpinBox->value();
    settings.fadeOutDuration = m_fadeOutDurationSpinBox->value();
    
    settings.opacity = m_opacitySlider->value() / 100.0;
    settings.scale = m_scaleSlider->value() / 100.0;
    settings.lineSpacing = m_lineSpacingSpinBox->value();
    settings.maxLines = m_maxLinesSpinBox->value();
    settings.wordWrap = m_wordWrapCheckBox->isChecked();
    
    return settings;
}

void SubtitleControlWidget::applySettingsToControls(const SubtitleRenderSettings& settings)
{
    m_updatingControls = true;
    
    // Font and colors
    m_currentFont = settings.font;
    m_fontButton->setText(QString("%1, %2pt").arg(settings.font.family()).arg(settings.font.pointSize()));
    updateColorButton(m_textColorButton, settings.textColor);
    updateColorButton(m_backgroundColorButton, settings.backgroundColor);
    updateColorButton(m_outlineColorButton, settings.outlineColor);
    updateColorButton(m_shadowColorButton, settings.shadowColor);
    
    // Outline and shadow
    m_outlineWidthSpinBox->setValue(settings.outlineWidth);
    m_shadowOffsetSpinBox->setValue(settings.shadowOffset);
    m_enableOutlineCheckBox->setChecked(settings.enableOutline);
    m_enableShadowCheckBox->setChecked(settings.enableShadow);
    m_enableBackgroundCheckBox->setChecked(settings.enableBackground);
    
    // Alignment
    int alignIndex = 7; // Default to bottom center
    if (settings.alignment & Qt::AlignTop) {
        if (settings.alignment & Qt::AlignLeft) alignIndex = 0;
        else if (settings.alignment & Qt::AlignRight) alignIndex = 2;
        else alignIndex = 1;
    } else if (settings.alignment & Qt::AlignVCenter) {
        if (settings.alignment & Qt::AlignLeft) alignIndex = 3;
        else if (settings.alignment & Qt::AlignRight) alignIndex = 5;
        else alignIndex = 4;
    } else {
        if (settings.alignment & Qt::AlignLeft) alignIndex = 6;
        else if (settings.alignment & Qt::AlignRight) alignIndex = 8;
        else alignIndex = 7;
    }
    m_alignmentComboBox->setCurrentIndex(alignIndex);
    
    // Margins
    m_marginLeftSpinBox->setValue(settings.marginLeft);
    m_marginRightSpinBox->setValue(settings.marginRight);
    m_marginTopSpinBox->setValue(settings.marginTop);
    m_marginBottomSpinBox->setValue(settings.marginBottom);
    
    // Animation
    m_enableFadeInCheckBox->setChecked(settings.enableFadeIn);
    m_enableFadeOutCheckBox->setChecked(settings.enableFadeOut);
    m_fadeInDurationSpinBox->setValue(settings.fadeInDuration);
    m_fadeOutDurationSpinBox->setValue(settings.fadeOutDuration);
    
    // Basic controls
    m_opacitySlider->setValue(static_cast<int>(settings.opacity * 100));
    m_opacityLabel->setText(QString("%1%").arg(static_cast<int>(settings.opacity * 100)));
    m_scaleSlider->setValue(static_cast<int>(settings.scale * 100));
    m_scaleLabel->setText(QString("%1%").arg(static_cast<int>(settings.scale * 100)));
    
    // Advanced
    m_lineSpacingSpinBox->setValue(settings.lineSpacing);
    m_maxLinesSpinBox->setValue(settings.maxLines);
    m_wordWrapCheckBox->setChecked(settings.wordWrap);
    
    m_updatingControls = false;
}

void SubtitleControlWidget::updateTrackComboBox()
{
    if (!m_subtitleManager) {
        return;
    }
    
    m_trackComboBox->clear();
    m_trackComboBox->addItem(tr("Disabled"));
    
    QList<SubtitleTrack> tracks = m_subtitleManager->getSubtitleTracks();
    for (int i = 0; i < tracks.size(); ++i) {
        const SubtitleTrack& track = tracks[i];
        QString trackName = track.title();
        if (trackName.isEmpty()) {
            trackName = tr("Track %1").arg(i + 1);
        }
        if (!track.language().isEmpty()) {
            trackName += QString(" (%1)").arg(track.language());
        }
        m_trackComboBox->addItem(trackName);
    }
}
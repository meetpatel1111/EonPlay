#pragma once

#include "subtitles/SubtitleEntry.h"
#include "subtitles/SubtitleRenderer.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QGroupBox>
#include <QCheckBox>

class SubtitleManager;

/**
 * @brief Subtitle control widget for EonPlay
 * 
 * Provides comprehensive subtitle controls including track selection,
 * timing adjustment, styling customization, and display options.
 */
class SubtitleControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SubtitleControlWidget(QWidget* parent = nullptr);
    ~SubtitleControlWidget() override;
    
    /**
     * @brief Set subtitle manager
     * @param manager Subtitle manager instance
     */
    void setSubtitleManager(SubtitleManager* manager);
    
    /**
     * @brief Set subtitle renderer
     * @param renderer Subtitle renderer instance
     */
    void setSubtitleRenderer(SubtitleRenderer* renderer);
    
    /**
     * @brief Update control states from current settings
     */
    void updateControls();
    
    /**
     * @brief Show or hide advanced controls
     * @param visible true to show advanced controls
     */
    void setAdvancedControlsVisible(bool visible);
    
    /**
     * @brief Check if advanced controls are visible
     * @return true if advanced controls are visible
     */
    bool areAdvancedControlsVisible() const { return m_advancedVisible; }

public slots:
    /**
     * @brief Handle subtitle track changes
     * @param trackIndex New active track index
     */
    void onActiveTrackChanged(int trackIndex);
    
    /**
     * @brief Handle subtitle enable/disable
     * @param enabled true if subtitles are enabled
     */
    void onSubtitlesEnabledChanged(bool enabled);
    
    /**
     * @brief Handle timing offset changes
     * @param offsetMs New timing offset in milliseconds
     */
    void onTimingOffsetChanged(qint64 offsetMs);

signals:
    /**
     * @brief Emitted when subtitle track selection changes
     * @param trackIndex Selected track index (-1 for disabled)
     */
    void trackSelectionChanged(int trackIndex);
    
    /**
     * @brief Emitted when subtitle enable state changes
     * @param enabled true if subtitles should be enabled
     */
    void subtitlesEnabledChanged(bool enabled);
    
    /**
     * @brief Emitted when timing offset changes
     * @param offsetMs New timing offset in milliseconds
     */
    void timingOffsetChanged(qint64 offsetMs);
    
    /**
     * @brief Emitted when subtitle settings change
     * @param settings New subtitle render settings
     */
    void renderSettingsChanged(const SubtitleRenderSettings& settings);

private slots:
    /**
     * @brief Handle basic control changes
     */
    void onEnableToggled(bool enabled);
    void onTrackChanged(int index);
    void onTimingOffsetSliderChanged(int value);
    void onOpacityChanged(int value);
    void onScaleChanged(int value);
    
    /**
     * @brief Handle font and color controls
     */
    void onFontButtonClicked();
    void onTextColorButtonClicked();
    void onOutlineColorButtonClicked();
    void onBackgroundColorButtonClicked();
    void onShadowColorButtonClicked();
    
    /**
     * @brief Handle outline and shadow controls
     */
    void onOutlineWidthChanged(int value);
    void onShadowOffsetChanged(int value);
    void onEnableOutlineToggled(bool enabled);
    void onEnableShadowToggled(bool enabled);
    void onEnableBackgroundToggled(bool enabled);
    
    /**
     * @brief Handle positioning controls
     */
    void onAlignmentChanged();
    void onMarginChanged();
    
    /**
     * @brief Handle animation controls
     */
    void onFadeInToggled(bool enabled);
    void onFadeOutToggled(bool enabled);
    void onFadeInDurationChanged(int value);
    void onFadeOutDurationChanged(int value);
    
    /**
     * @brief Handle advanced controls
     */
    void onMaxLinesChanged(int value);
    void onLineSpacingChanged(int value);
    void onWordWrapToggled(bool enabled);
    
    /**
     * @brief Handle preset and reset
     */
    void onLoadPreset();
    void onSavePreset();
    void onResetToDefaults();
    void onToggleAdvanced();

private:
    /**
     * @brief Initialize the user interface
     */
    void initializeUI();
    
    /**
     * @brief Create basic controls group
     * @return Basic controls group box
     */
    QGroupBox* createBasicControlsGroup();
    
    /**
     * @brief Create font and color controls group
     * @return Font and color controls group box
     */
    QGroupBox* createFontColorGroup();
    
    /**
     * @brief Create outline and shadow controls group
     * @return Outline and shadow controls group box
     */
    QGroupBox* createOutlineShadowGroup();
    
    /**
     * @brief Create positioning controls group
     * @return Positioning controls group box
     */
    QGroupBox* createPositioningGroup();
    
    /**
     * @brief Create animation controls group
     * @return Animation controls group box
     */
    QGroupBox* createAnimationGroup();
    
    /**
     * @brief Create advanced controls group
     * @return Advanced controls group box
     */
    QGroupBox* createAdvancedGroup();
    
    /**
     * @brief Create color button with current color
     * @param color Current color
     * @return Color button
     */
    QPushButton* createColorButton(const QColor& color);
    
    /**
     * @brief Update color button appearance
     * @param button Color button to update
     * @param color New color
     */
    void updateColorButton(QPushButton* button, const QColor& color);
    
    /**
     * @brief Get current render settings from controls
     * @return Current render settings
     */
    SubtitleRenderSettings getCurrentSettings() const;
    
    /**
     * @brief Apply settings to controls
     * @param settings Settings to apply
     */
    void applySettingsToControls(const SubtitleRenderSettings& settings);
    
    /**
     * @brief Update track combo box
     */
    void updateTrackComboBox();
    
    SubtitleManager* m_subtitleManager;
    SubtitleRenderer* m_subtitleRenderer;
    
    // Main layout
    QVBoxLayout* m_mainLayout;
    
    // Basic controls
    QGroupBox* m_basicGroup;
    QCheckBox* m_enableCheckBox;
    QComboBox* m_trackComboBox;
    QSlider* m_timingOffsetSlider;
    QLabel* m_timingOffsetLabel;
    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;
    QSlider* m_scaleSlider;
    QLabel* m_scaleLabel;
    
    // Font and color controls
    QGroupBox* m_fontColorGroup;
    QPushButton* m_fontButton;
    QPushButton* m_textColorButton;
    QPushButton* m_outlineColorButton;
    QPushButton* m_backgroundColorButton;
    QPushButton* m_shadowColorButton;
    QFont m_currentFont;
    
    // Outline and shadow controls
    QGroupBox* m_outlineShadowGroup;
    QCheckBox* m_enableOutlineCheckBox;
    QSpinBox* m_outlineWidthSpinBox;
    QCheckBox* m_enableShadowCheckBox;
    QSpinBox* m_shadowOffsetSpinBox;
    QCheckBox* m_enableBackgroundCheckBox;
    
    // Positioning controls
    QGroupBox* m_positioningGroup;
    QComboBox* m_alignmentComboBox;
    QSpinBox* m_marginLeftSpinBox;
    QSpinBox* m_marginRightSpinBox;
    QSpinBox* m_marginTopSpinBox;
    QSpinBox* m_marginBottomSpinBox;
    
    // Animation controls
    QGroupBox* m_animationGroup;
    QCheckBox* m_enableFadeInCheckBox;
    QCheckBox* m_enableFadeOutCheckBox;
    QSpinBox* m_fadeInDurationSpinBox;
    QSpinBox* m_fadeOutDurationSpinBox;
    
    // Advanced controls
    QGroupBox* m_advancedGroup;
    QSpinBox* m_maxLinesSpinBox;
    QSpinBox* m_lineSpacingSpinBox;
    QCheckBox* m_wordWrapCheckBox;
    
    // Control buttons
    QPushButton* m_loadPresetButton;
    QPushButton* m_savePresetButton;
    QPushButton* m_resetButton;
    QPushButton* m_advancedToggleButton;
    
    bool m_advancedVisible;
    bool m_updatingControls;
};
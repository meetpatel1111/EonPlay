#ifndef AUDIOEQUALIZERWIDGET_H
#define AUDIOEQUALIZERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QVector>
#include <memory>

class AudioEqualizer;

/**
 * @brief Widget for audio equalizer controls
 * 
 * Provides a comprehensive UI for the audio equalizer with band controls,
 * presets, bass boost, treble enhancement, and advanced audio features.
 */
class AudioEqualizerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioEqualizerWidget(QWidget* parent = nullptr);
    ~AudioEqualizerWidget() override;

    /**
     * @brief Initialize the widget with audio equalizer
     * @param equalizer Audio equalizer instance
     */
    void initialize(std::shared_ptr<AudioEqualizer> equalizer);

    /**
     * @brief Update the widget from equalizer settings
     */
    void updateFromEqualizer();

public slots:
    /**
     * @brief Handle equalizer enabled state change
     * @param enabled New enabled state
     */
    void onEqualizerEnabledChanged(bool enabled);

    /**
     * @brief Handle band gain change
     * @param bandIndex Band index
     * @param gain New gain value
     */
    void onBandGainChanged(int bandIndex, double gain);

    /**
     * @brief Handle preset change
     * @param preset New preset
     */
    void onPresetChanged(int preset);

    /**
     * @brief Handle bass boost change
     * @param boost New bass boost value
     */
    void onBassBoostChanged(double boost);

    /**
     * @brief Handle treble enhancement change
     * @param enhancement New treble enhancement value
     */
    void onTrebleEnhancementChanged(double enhancement);

    /**
     * @brief Handle 3D surround change
     * @param enabled Enable state
     * @param strength Strength level
     */
    void onSurroundChanged(bool enabled, double strength);

    /**
     * @brief Handle ReplayGain change
     * @param enabled Enable state
     * @param mode ReplayGain mode
     * @param preamp Preamp value
     */
    void onReplayGainChanged(bool enabled, const QString& mode, double preamp);

signals:
    /**
     * @brief Emitted when user wants to reset equalizer
     */
    void resetRequested();

    /**
     * @brief Emitted when user wants to export settings
     */
    void exportRequested();

    /**
     * @brief Emitted when user wants to import settings
     */
    void importRequested();

private slots:
    void onEnableCheckBoxToggled(bool enabled);
    void onPresetComboChanged(int index);
    void onBandSliderChanged(int value);
    void onBassBoostSliderChanged(int value);
    void onTrebleSliderChanged(int value);
    void on3DSurroundToggled(bool enabled);
    void on3DSurroundStrengthChanged(int value);
    void onReplayGainToggled(bool enabled);
    void onReplayGainModeChanged(int index);
    void onReplayGainPreampChanged(double value);
    void onResetClicked();
    void onExportClicked();
    void onImportClicked();

private:
    void setupUI();
    void setupEqualizerBands();
    void setupEnhancements();
    void setupReplayGain();
    void setupButtons();
    void setupConnections();
    void updateBandLabels();
    void updateEnhancementLabels();
    void setWidgetsEnabled(bool enabled);

    // Main layout
    QVBoxLayout* m_mainLayout;

    // Enable/disable
    QCheckBox* m_enableCheckBox;

    // Preset selection
    QHBoxLayout* m_presetLayout;
    QLabel* m_presetLabel;
    QComboBox* m_presetCombo;

    // Equalizer bands
    QGroupBox* m_bandsGroupBox;
    QHBoxLayout* m_bandsLayout;
    QVector<QSlider*> m_bandSliders;
    QVector<QLabel*> m_bandLabels;
    QVector<QLabel*> m_bandValueLabels;

    // Audio enhancements
    QGroupBox* m_enhancementsGroupBox;
    QGridLayout* m_enhancementsLayout;

    // Bass boost
    QLabel* m_bassBoostLabel;
    QSlider* m_bassBoostSlider;
    QLabel* m_bassBoostValueLabel;

    // Treble enhancement
    QLabel* m_trebleLabel;
    QSlider* m_trebleSlider;
    QLabel* m_trebleValueLabel;

    // 3D Surround
    QCheckBox* m_3dSurroundCheckBox;
    QLabel* m_3dSurroundStrengthLabel;
    QSlider* m_3dSurroundStrengthSlider;
    QLabel* m_3dSurroundValueLabel;

    // ReplayGain
    QGroupBox* m_replayGainGroupBox;
    QGridLayout* m_replayGainLayout;
    QCheckBox* m_replayGainCheckBox;
    QLabel* m_replayGainModeLabel;
    QComboBox* m_replayGainModeCombo;
    QLabel* m_replayGainPreampLabel;
    QDoubleSpinBox* m_replayGainPreampSpinBox;

    // Control buttons
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_resetButton;
    QPushButton* m_exportButton;
    QPushButton* m_importButton;

    // Equalizer instance
    std::shared_ptr<AudioEqualizer> m_equalizer;

    // Update flags
    bool m_updatingFromEqualizer;
};

#endif // AUDIOEQUALIZERWIDGET_H
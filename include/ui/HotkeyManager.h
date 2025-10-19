#pragma once

#include <QObject>
#include <QShortcut>
#include <QKeySequence>
#include <QHash>
#include <QSettings>
#include <QWidget>
#include <memory>

/**
 * @brief Hotkey action data structure
 */
struct HotkeyAction
{
    QString id;
    QString name;
    QString description;
    QKeySequence defaultSequence;
    QKeySequence currentSequence;
    bool enabled = true;
    
    HotkeyAction() = default;
    HotkeyAction(const QString& actionId, const QString& actionName, 
                const QString& desc, const QKeySequence& sequence)
        : id(actionId), name(actionName), description(desc)
        , defaultSequence(sequence), currentSequence(sequence) {}
};

/**
 * @brief Hotkey customization manager for EonPlay
 * 
 * Manages global and local keyboard shortcuts, provides customization
 * interface, and handles hotkey conflicts and validation.
 */
class HotkeyManager : public QObject
{
    Q_OBJECT

public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;
    
    /**
     * @brief Initialize hotkey manager with parent widget
     * @param parentWidget Parent widget for shortcuts
     * @return true if initialization successful
     */
    bool initialize(QWidget* parentWidget);
    
    /**
     * @brief Register a hotkey action
     * @param id Unique action identifier
     * @param name Human-readable action name
     * @param description Action description
     * @param defaultSequence Default key sequence
     * @return true if registration successful
     */
    bool registerAction(const QString& id, const QString& name, 
                       const QString& description, const QKeySequence& defaultSequence);
    
    /**
     * @brief Set hotkey for an action
     * @param actionId Action identifier
     * @param sequence New key sequence
     * @return true if hotkey was set successfully
     */
    bool setHotkey(const QString& actionId, const QKeySequence& sequence);
    
    /**
     * @brief Get current hotkey for an action
     * @param actionId Action identifier
     * @return Current key sequence
     */
    QKeySequence getHotkey(const QString& actionId) const;
    
    /**
     * @brief Enable or disable an action
     * @param actionId Action identifier
     * @param enabled true to enable action
     */
    void setActionEnabled(const QString& actionId, bool enabled);
    
    /**
     * @brief Check if an action is enabled
     * @param actionId Action identifier
     * @return true if action is enabled
     */
    bool isActionEnabled(const QString& actionId) const;
    
    /**
     * @brief Reset hotkey to default
     * @param actionId Action identifier
     */
    void resetToDefault(const QString& actionId);
    
    /**
     * @brief Reset all hotkeys to defaults
     */
    void resetAllToDefaults();
    
    /**
     * @brief Get all registered actions
     * @return List of all hotkey actions
     */
    QList<HotkeyAction> getAllActions() const;
    
    /**
     * @brief Check if key sequence conflicts with existing hotkeys
     * @param sequence Key sequence to check
     * @param excludeActionId Action ID to exclude from conflict check
     * @return Conflicting action ID or empty string if no conflict
     */
    QString checkConflict(const QKeySequence& sequence, const QString& excludeActionId = QString()) const;
    
    /**
     * @brief Save hotkey settings
     */
    void saveSettings();
    
    /**
     * @brief Load hotkey settings
     */
    void loadSettings();
    
    /**
     * @brief Show hotkey customization dialog
     */
    void showCustomizationDialog();

signals:
    /**
     * @brief Emitted when a hotkey action is triggered
     * @param actionId Action identifier
     */
    void actionTriggered(const QString& actionId);
    
    /**
     * @brief Emitted when hotkey settings change
     * @param actionId Action identifier
     * @param newSequence New key sequence
     */
    void hotkeyChanged(const QString& actionId, const QKeySequence& newSequence);

private slots:
    /**
     * @brief Handle shortcut activation
     */
    void onShortcutActivated();

private:
    /**
     * @brief Create QShortcut for an action
     * @param action Hotkey action
     * @return Created shortcut
     */
    QShortcut* createShortcut(const HotkeyAction& action);
    
    /**
     * @brief Update shortcut for an action
     * @param actionId Action identifier
     */
    void updateShortcut(const QString& actionId);
    
    /**
     * @brief Register default EonPlay hotkeys
     */
    void registerDefaultHotkeys();
    
    QWidget* m_parentWidget;
    
    // Hotkey actions and shortcuts
    QHash<QString, HotkeyAction> m_actions;
    QHash<QString, QShortcut*> m_shortcuts;
    
    // Settings
    std::unique_ptr<QSettings> m_settings;
    
    bool m_initialized;
};
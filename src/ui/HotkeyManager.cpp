#include "ui/HotkeyManager.h"
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(hotkeys, "eonplay.hotkeys")

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent)
    , m_parentWidget(nullptr)
    , m_initialized(false)
{
    // Initialize settings
    m_settings = std::make_unique<QSettings>(
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/EonPlay_Hotkeys.ini",
        QSettings::IniFormat
    );
    
    qCDebug(hotkeys) << "HotkeyManager created";
}

HotkeyManager::~HotkeyManager()
{
    saveSettings();
    
    // Clean up shortcuts
    for (auto* shortcut : m_shortcuts.values()) {
        delete shortcut;
    }
    
    qCDebug(hotkeys) << "HotkeyManager destroyed";
}

bool HotkeyManager::initialize(QWidget* parentWidget)
{
    if (!parentWidget) {
        qCWarning(hotkeys) << "Cannot initialize with null parent widget";
        return false;
    }
    
    m_parentWidget = parentWidget;
    
    // Register default hotkeys
    registerDefaultHotkeys();
    
    // Load saved settings
    loadSettings();
    
    m_initialized = true;
    
    qCDebug(hotkeys) << "HotkeyManager initialized with" << m_actions.size() << "actions";
    return true;
}

bool HotkeyManager::registerAction(const QString& id, const QString& name, 
                                  const QString& description, const QKeySequence& defaultSequence)
{
    if (m_actions.contains(id)) {
        qCWarning(hotkeys) << "Action already registered:" << id;
        return false;
    }
    
    HotkeyAction action(id, name, description, defaultSequence);
    m_actions[id] = action;
    
    // Create shortcut if initialized
    if (m_initialized) {
        QShortcut* shortcut = createShortcut(action);
        if (shortcut) {
            m_shortcuts[id] = shortcut;
        }
    }
    
    qCDebug(hotkeys) << "Registered action:" << id << "with sequence:" << defaultSequence.toString();
    return true;
}

bool HotkeyManager::setHotkey(const QString& actionId, const QKeySequence& sequence)
{
    if (!m_actions.contains(actionId)) {
        qCWarning(hotkeys) << "Action not found:" << actionId;
        return false;
    }
    
    // Check for conflicts
    QString conflictId = checkConflict(sequence, actionId);
    if (!conflictId.isEmpty()) {
        qCWarning(hotkeys) << "Hotkey conflict with action:" << conflictId;
        return false;
    }
    
    // Update action
    m_actions[actionId].currentSequence = sequence;
    
    // Update shortcut
    updateShortcut(actionId);
    
    emit hotkeyChanged(actionId, sequence);
    
    qCDebug(hotkeys) << "Updated hotkey for" << actionId << "to" << sequence.toString();
    return true;
}

QKeySequence HotkeyManager::getHotkey(const QString& actionId) const
{
    if (m_actions.contains(actionId)) {
        return m_actions[actionId].currentSequence;
    }
    return QKeySequence();
}

void HotkeyManager::setActionEnabled(const QString& actionId, bool enabled)
{
    if (!m_actions.contains(actionId)) {
        return;
    }
    
    m_actions[actionId].enabled = enabled;
    
    // Update shortcut
    if (m_shortcuts.contains(actionId)) {
        m_shortcuts[actionId]->setEnabled(enabled);
    }
    
    qCDebug(hotkeys) << "Action" << actionId << (enabled ? "enabled" : "disabled");
}

bool HotkeyManager::isActionEnabled(const QString& actionId) const
{
    if (m_actions.contains(actionId)) {
        return m_actions[actionId].enabled;
    }
    return false;
}

void HotkeyManager::resetToDefault(const QString& actionId)
{
    if (!m_actions.contains(actionId)) {
        return;
    }
    
    HotkeyAction& action = m_actions[actionId];
    action.currentSequence = action.defaultSequence;
    
    updateShortcut(actionId);
    
    emit hotkeyChanged(actionId, action.currentSequence);
    
    qCDebug(hotkeys) << "Reset" << actionId << "to default:" << action.defaultSequence.toString();
}

void HotkeyManager::resetAllToDefaults()
{
    for (auto& action : m_actions) {
        action.currentSequence = action.defaultSequence;
        updateShortcut(action.id);
        emit hotkeyChanged(action.id, action.currentSequence);
    }
    
    qCDebug(hotkeys) << "Reset all hotkeys to defaults";
}

QList<HotkeyAction> HotkeyManager::getAllActions() const
{
    return m_actions.values();
}

QString HotkeyManager::checkConflict(const QKeySequence& sequence, const QString& excludeActionId) const
{
    if (sequence.isEmpty()) {
        return QString();
    }
    
    for (const auto& action : m_actions) {
        if (action.id != excludeActionId && action.currentSequence == sequence) {
            return action.id;
        }
    }
    
    return QString();
}

void HotkeyManager::saveSettings()
{
    m_settings->beginGroup("Hotkeys");
    
    for (const auto& action : m_actions) {
        m_settings->setValue(action.id + "/sequence", action.currentSequence.toString());
        m_settings->setValue(action.id + "/enabled", action.enabled);
    }
    
    m_settings->endGroup();
    m_settings->sync();
    
    qCDebug(hotkeys) << "Hotkey settings saved";
}

void HotkeyManager::loadSettings()
{
    m_settings->beginGroup("Hotkeys");
    
    for (auto& action : m_actions) {
        QString sequenceStr = m_settings->value(action.id + "/sequence", action.defaultSequence.toString()).toString();
        bool enabled = m_settings->value(action.id + "/enabled", true).toBool();
        
        action.currentSequence = QKeySequence(sequenceStr);
        action.enabled = enabled;
        
        // Update shortcut if it exists
        updateShortcut(action.id);
    }
    
    m_settings->endGroup();
    
    qCDebug(hotkeys) << "Hotkey settings loaded";
}

void HotkeyManager::showCustomizationDialog()
{
    // This is a simplified implementation
    // In a full implementation, this would show a proper dialog with a table of all hotkeys
    
    QStringList actionNames;
    QStringList actionIds;
    
    for (const auto& action : m_actions) {
        actionNames << QString("%1 (%2)").arg(action.name, action.currentSequence.toString());
        actionIds << action.id;
    }
    
    bool ok;
    QString selectedAction = QInputDialog::getItem(
        m_parentWidget,
        tr("Customize Hotkeys"),
        tr("Select action to modify:"),
        actionNames,
        0,
        false,
        &ok
    );
    
    if (!ok || selectedAction.isEmpty()) {
        return;
    }
    
    int index = actionNames.indexOf(selectedAction);
    if (index < 0 || index >= actionIds.size()) {
        return;
    }
    
    QString actionId = actionIds[index];
    const HotkeyAction& action = m_actions[actionId];
    
    // Get new key sequence
    QString newSequenceStr = QInputDialog::getText(
        m_parentWidget,
        tr("Set Hotkey"),
        tr("Enter new hotkey for '%1':").arg(action.name),
        QLineEdit::Normal,
        action.currentSequence.toString(),
        &ok
    );
    
    if (!ok) {
        return;
    }
    
    QKeySequence newSequence(newSequenceStr);
    
    if (newSequence.isEmpty() && !newSequenceStr.isEmpty()) {
        QMessageBox::warning(m_parentWidget, tr("Invalid Hotkey"), 
                           tr("The entered hotkey is invalid."));
        return;
    }
    
    // Check for conflicts
    QString conflictId = checkConflict(newSequence, actionId);
    if (!conflictId.isEmpty()) {
        QMessageBox::warning(m_parentWidget, tr("Hotkey Conflict"), 
                           tr("This hotkey is already used by '%1'.").arg(m_actions[conflictId].name));
        return;
    }
    
    // Set new hotkey
    if (setHotkey(actionId, newSequence)) {
        QMessageBox::information(m_parentWidget, tr("Hotkey Updated"), 
                                tr("Hotkey for '%1' updated to '%2'.").arg(action.name, newSequence.toString()));
    }
}

void HotkeyManager::onShortcutActivated()
{
    QShortcut* shortcut = qobject_cast<QShortcut*>(sender());
    if (!shortcut) {
        return;
    }
    
    // Find action ID for this shortcut
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it.value() == shortcut) {
            emit actionTriggered(it.key());
            qCDebug(hotkeys) << "Action triggered:" << it.key();
            break;
        }
    }
}

QShortcut* HotkeyManager::createShortcut(const HotkeyAction& action)
{
    if (!m_parentWidget || action.currentSequence.isEmpty()) {
        return nullptr;
    }
    
    QShortcut* shortcut = new QShortcut(action.currentSequence, m_parentWidget);
    shortcut->setEnabled(action.enabled);
    shortcut->setContext(Qt::ApplicationShortcut);
    
    connect(shortcut, &QShortcut::activated, this, &HotkeyManager::onShortcutActivated);
    
    return shortcut;
}

void HotkeyManager::updateShortcut(const QString& actionId)
{
    if (!m_actions.contains(actionId)) {
        return;
    }
    
    const HotkeyAction& action = m_actions[actionId];
    
    // Remove existing shortcut
    if (m_shortcuts.contains(actionId)) {
        delete m_shortcuts[actionId];
        m_shortcuts.remove(actionId);
    }
    
    // Create new shortcut
    QShortcut* shortcut = createShortcut(action);
    if (shortcut) {
        m_shortcuts[actionId] = shortcut;
    }
}

void HotkeyManager::registerDefaultHotkeys()
{
    // Media control hotkeys
    registerAction("play_pause", tr("Play/Pause"), tr("Toggle playback"), QKeySequence(Qt::Key_Space));
    registerAction("stop", tr("Stop"), tr("Stop playback"), QKeySequence(Qt::Key_S));
    registerAction("next", tr("Next"), tr("Next track"), QKeySequence(Qt::Key_Right));
    registerAction("previous", tr("Previous"), tr("Previous track"), QKeySequence(Qt::Key_Left));
    
    // Volume control
    registerAction("volume_up", tr("Volume Up"), tr("Increase volume"), QKeySequence(Qt::Key_Up));
    registerAction("volume_down", tr("Volume Down"), tr("Decrease volume"), QKeySequence(Qt::Key_Down));
    registerAction("mute", tr("Mute"), tr("Toggle mute"), QKeySequence(Qt::Key_M));
    
    // File operations
    registerAction("open_file", tr("Open File"), tr("Open media file"), QKeySequence::Open);
    registerAction("open_url", tr("Open URL"), tr("Open media URL"), QKeySequence(Qt::CTRL | Qt::Key_U));
    
    // Window operations
    registerAction("fullscreen", tr("Fullscreen"), tr("Toggle fullscreen"), QKeySequence(Qt::Key_F11));
    registerAction("minimize", tr("Minimize"), tr("Minimize window"), QKeySequence(Qt::CTRL | Qt::Key_M));
    registerAction("quit", tr("Quit"), tr("Quit application"), QKeySequence::Quit);
    
    // View operations
    registerAction("toggle_playlist", tr("Toggle Playlist"), tr("Show/hide playlist"), QKeySequence(Qt::CTRL | Qt::Key_P));
    registerAction("toggle_library", tr("Toggle Library"), tr("Show/hide library"), QKeySequence(Qt::CTRL | Qt::Key_L));
    
    // Seek operations
    registerAction("seek_forward", tr("Seek Forward"), tr("Seek forward 10 seconds"), QKeySequence(Qt::CTRL | Qt::Key_Right));
    registerAction("seek_backward", tr("Seek Backward"), tr("Seek backward 10 seconds"), QKeySequence(Qt::CTRL | Qt::Key_Left));
    
    qCDebug(hotkeys) << "Registered" << m_actions.size() << "default hotkeys";
}
#include "ComponentManager.h"
#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(componentManager, "mediaplayer.componentmanager")

ComponentManager::ComponentManager(QObject* parent)
    : QObject(parent), m_allInitialized(false)
{
    qCDebug(componentManager) << "ComponentManager created";
}

ComponentManager::~ComponentManager()
{
    shutdownAll();
    qCDebug(componentManager) << "ComponentManager destroyed";
}

void ComponentManager::registerComponent(std::shared_ptr<IComponent> component, int priority)
{
    if (!component) {
        qCWarning(componentManager) << "Attempted to register null component";
        return;
    }
    
    const QString componentName = component->componentName();
    
    if (m_componentsByName.contains(componentName)) {
        qCWarning(componentManager) << "Component already registered:" << componentName;
        return;
    }
    
    m_components.append(ComponentEntry(component, priority));
    m_componentsByName[componentName] = component;
    
    qCDebug(componentManager) << "Registered component:" << componentName << "with priority:" << priority;
}

bool ComponentManager::initializeAll()
{
    if (m_components.isEmpty()) {
        qCDebug(componentManager) << "No components to initialize";
        m_allInitialized = true;
        emit allComponentsInitialized();
        return true;
    }
    
    // Sort components by priority (lower numbers first)
    std::sort(m_components.begin(), m_components.end(),
              [](const ComponentEntry& a, const ComponentEntry& b) {
                  return a.priority < b.priority;
              });
    
    qCDebug(componentManager) << "Initializing" << m_components.size() << "components";
    
    bool allSuccessful = true;
    
    for (auto& entry : m_components) {
        const QString componentName = entry.component->componentName();
        
        qCDebug(componentManager) << "Initializing component:" << componentName;
        
        try {
            if (entry.component->initialize()) {
                entry.initialized = true;
                qCDebug(componentManager) << "Successfully initialized:" << componentName;
                emit componentInitialized(componentName);
            } else {
                const QString error = QString("Component %1 failed to initialize").arg(componentName);
                qCCritical(componentManager) << error;
                emit componentInitializationFailed(componentName, error);
                allSuccessful = false;
            }
        } catch (const std::exception& e) {
            const QString error = QString("Exception during initialization of %1: %2")
                                    .arg(componentName, e.what());
            qCCritical(componentManager) << error;
            emit componentInitializationFailed(componentName, error);
            allSuccessful = false;
        } catch (...) {
            const QString error = QString("Unknown exception during initialization of %1").arg(componentName);
            qCCritical(componentManager) << error;
            emit componentInitializationFailed(componentName, error);
            allSuccessful = false;
        }
    }
    
    m_allInitialized = allSuccessful;
    
    if (allSuccessful) {
        qCDebug(componentManager) << "All components initialized successfully";
        emit allComponentsInitialized();
    } else {
        qCWarning(componentManager) << "Some components failed to initialize";
    }
    
    return allSuccessful;
}

void ComponentManager::shutdownAll()
{
    if (m_components.isEmpty()) {
        return;
    }
    
    qCDebug(componentManager) << "Shutting down" << m_components.size() << "components";
    
    // Shutdown in reverse order
    for (auto it = m_components.rbegin(); it != m_components.rend(); ++it) {
        if (it->initialized) {
            const QString componentName = it->component->componentName();
            qCDebug(componentManager) << "Shutting down component:" << componentName;
            
            try {
                it->component->shutdown();
                it->initialized = false;
                qCDebug(componentManager) << "Successfully shut down:" << componentName;
            } catch (const std::exception& e) {
                qCWarning(componentManager) << "Exception during shutdown of" << componentName << ":" << e.what();
            } catch (...) {
                qCWarning(componentManager) << "Unknown exception during shutdown of" << componentName;
            }
        }
    }
    
    m_allInitialized = false;
    qCDebug(componentManager) << "All components shut down";
}

std::shared_ptr<IComponent> ComponentManager::getComponent(const QString& name) const
{
    return m_componentsByName.value(name);
}

bool ComponentManager::areAllInitialized() const
{
    return m_allInitialized;
}

QStringList ComponentManager::getComponentNames() const
{
    QStringList names;
    for (const auto& entry : m_components) {
        names.append(entry.component->componentName());
    }
    return names;
}
#pragma once

#include "IComponent.h"
#include <QObject>
#include <QList>
#include <QHash>
#include <memory>

/**
 * @brief Manages the lifecycle of application components
 * 
 * Provides centralized management for all application components,
 * ensuring proper initialization order and cleanup.
 */
class ComponentManager : public QObject
{
    Q_OBJECT

public:
    explicit ComponentManager(QObject* parent = nullptr);
    ~ComponentManager();
    
    /**
     * @brief Register a component for management
     * @param component Shared pointer to the component
     * @param priority Initialization priority (lower numbers initialize first)
     */
    void registerComponent(std::shared_ptr<IComponent> component, int priority = 100);
    
    /**
     * @brief Initialize all registered components
     * @return true if all components initialized successfully
     */
    bool initializeAll();
    
    /**
     * @brief Shutdown all components in reverse order
     */
    void shutdownAll();
    
    /**
     * @brief Get a component by name
     * @param name Component name
     * @return Shared pointer to component or nullptr if not found
     */
    std::shared_ptr<IComponent> getComponent(const QString& name) const;
    
    /**
     * @brief Get a component by type
     * @tparam T Component type
     * @return Shared pointer to component or nullptr if not found
     */
    template<typename T>
    std::shared_ptr<T> getComponent() const
    {
        for (const auto& entry : m_components) {
            auto component = std::dynamic_pointer_cast<T>(entry.component);
            if (component) {
                return component;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Check if all components are initialized
     * @return true if all components are initialized
     */
    bool areAllInitialized() const;
    
    /**
     * @brief Get list of component names
     * @return List of registered component names
     */
    QStringList getComponentNames() const;

signals:
    /**
     * @brief Emitted when a component is successfully initialized
     * @param componentName Name of the initialized component
     */
    void componentInitialized(const QString& componentName);
    
    /**
     * @brief Emitted when a component fails to initialize
     * @param componentName Name of the failed component
     * @param error Error description
     */
    void componentInitializationFailed(const QString& componentName, const QString& error);
    
    /**
     * @brief Emitted when all components are initialized
     */
    void allComponentsInitialized();

private:
    struct ComponentEntry
    {
        std::shared_ptr<IComponent> component;
        int priority;
        bool initialized;
        
        ComponentEntry(std::shared_ptr<IComponent> comp, int prio)
            : component(comp), priority(prio), initialized(false) {}
    };
    
    QList<ComponentEntry> m_components;
    QHash<QString, std::shared_ptr<IComponent>> m_componentsByName;
    bool m_allInitialized;
};
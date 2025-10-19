#pragma once

#include <QString>

/**
 * @brief Interface for modular application components
 * 
 * All major application components should implement this interface
 * to enable proper lifecycle management through the ComponentManager.
 */
class IComponent
{
public:
    virtual ~IComponent() = default;
    
    /**
     * @brief Initialize the component
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Shutdown the component and cleanup resources
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Get the component name for identification
     * @return Component name as QString
     */
    virtual QString componentName() const = 0;
    
    /**
     * @brief Check if the component is currently initialized
     * @return true if initialized, false otherwise
     */
    virtual bool isInitialized() const = 0;
};
#pragma once

#include <QApplication>
#include <QLoggingCategory>
#include <memory>

class ComponentManager;
class EventBus;

Q_DECLARE_LOGGING_CATEGORY(eonPlayApp)

/**
 * @brief Main application class for EonPlay - Timeless, futuristic media player
 * 
 * Inherits from QApplication and manages the overall application lifecycle,
 * component initialization, and global application state for EonPlay.
 * 
 * EonPlay: Play for eons - A timeless, futuristic media experience.
 */
class EonPlayApplication : public QApplication
{
    Q_OBJECT

public:
    EonPlayApplication(int& argc, char** argv);
    ~EonPlayApplication();
    
    /**
     * @brief Initialize EonPlay and all components
     * @return true if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Get the component manager instance
     * @return Pointer to the component manager
     */
    ComponentManager* componentManager() const { return m_componentManager.get(); }
    
    /**
     * @brief Get the EonPlay version
     * @return Version string
     */
    static QString version();
    
    /**
     * @brief Get the EonPlay application name
     * @return Application name
     */
    static QString applicationName();
    
    /**
     * @brief Get the EonPlay description
     * @return Application description with tagline
     */
    static QString description();

public slots:
    /**
     * @brief Gracefully shutdown EonPlay
     */
    void shutdown();

signals:
    /**
     * @brief Emitted when EonPlay is ready to show UI
     */
    void applicationReady();
    
    /**
     * @brief Emitted when EonPlay shutdown begins
     */
    void applicationShuttingDown();

protected:
    /**
     * @brief Handle EonPlay application events
     * @param receiver Event receiver
     * @param event Event to handle
     * @return true if event was handled
     */
    bool notify(QObject* receiver, QEvent* event) override;

private slots:
    /**
     * @brief Handle successful EonPlay component initialization
     */
    void onAllComponentsInitialized();
    
    /**
     * @brief Handle EonPlay component initialization failure
     * @param componentName Name of failed component
     * @param error Error description
     */
    void onComponentInitializationFailed(const QString& componentName, const QString& error);

private:
    /**
     * @brief Set up EonPlay logging categories and configuration
     */
    void setupLogging();
    
    /**
     * @brief Register all EonPlay components
     */
    void registerComponents();
    
    /**
     * @brief Set up EonPlay application properties and metadata
     */
    void setupApplicationProperties();
    
    std::unique_ptr<ComponentManager> m_componentManager;
    bool m_initialized;
};
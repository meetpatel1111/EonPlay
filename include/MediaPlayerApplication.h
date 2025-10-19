#pragma once

#include <QApplication>
#include <QLoggingCategory>
#include <memory>

class ComponentManager;
class EventBus;

Q_DECLARE_LOGGING_CATEGORY(mediaPlayerApp)

/**
 * @brief Main application class for the Cross-Platform Media Player
 * 
 * Inherits from QApplication and manages the overall application lifecycle,
 * component initialization, and global application state.
 */
class MediaPlayerApplication : public QApplication
{
    Q_OBJECT

public:
    MediaPlayerApplication(int& argc, char** argv);
    ~MediaPlayerApplication();
    
    /**
     * @brief Initialize the application and all components
     * @return true if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Get the component manager instance
     * @return Pointer to the component manager
     */
    ComponentManager* componentManager() const { return m_componentManager.get(); }
    
    /**
     * @brief Get the application version
     * @return Version string
     */
    static QString version() { return "1.0.0"; }
    
    /**
     * @brief Get the application name
     * @return Application name
     */
    static QString applicationName() { return "Cross-Platform Media Player"; }

public slots:
    /**
     * @brief Gracefully shutdown the application
     */
    void shutdown();

signals:
    /**
     * @brief Emitted when the application is ready to show UI
     */
    void applicationReady();
    
    /**
     * @brief Emitted when application shutdown begins
     */
    void applicationShuttingDown();

protected:
    /**
     * @brief Handle application events
     * @param receiver Event receiver
     * @param event Event to handle
     * @return true if event was handled
     */
    bool notify(QObject* receiver, QEvent* event) override;

private slots:
    /**
     * @brief Handle successful component initialization
     */
    void onAllComponentsInitialized();
    
    /**
     * @brief Handle component initialization failure
     * @param componentName Name of failed component
     * @param error Error description
     */
    void onComponentInitializationFailed(const QString& componentName, const QString& error);

private:
    /**
     * @brief Set up logging categories and configuration
     */
    void setupLogging();
    
    /**
     * @brief Register all application components
     */
    void registerComponents();
    
    /**
     * @brief Set up application properties and metadata
     */
    void setupApplicationProperties();
    
    std::unique_ptr<ComponentManager> m_componentManager;
    bool m_initialized;
};
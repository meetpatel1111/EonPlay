#pragma once

#include <QObject>
#include <QVariant>
#include <QHash>
#include <QList>
#include <functional>

/**
 * @brief Event structure for inter-component communication
 */
struct Event
{
    QString type;
    QVariantMap data;
    QObject* sender;
    
    Event(const QString& eventType, const QVariantMap& eventData = {}, QObject* eventSender = nullptr)
        : type(eventType), data(eventData), sender(eventSender) {}
};

/**
 * @brief Event handler function type
 */
using EventHandler = std::function<void(const Event&)>;

/**
 * @brief Centralized event bus for application-wide communication
 * 
 * Provides a decoupled way for components to communicate through events.
 * Components can subscribe to specific event types and publish events.
 */
class EventBus : public QObject
{
    Q_OBJECT

public:
    static EventBus& instance();
    
    /**
     * @brief Subscribe to an event type
     * @param eventType The type of event to listen for
     * @param handler Function to call when event occurs
     * @return Subscription ID for later unsubscription
     */
    int subscribe(const QString& eventType, EventHandler handler);
    
    /**
     * @brief Unsubscribe from an event type
     * @param subscriptionId The ID returned from subscribe()
     */
    void unsubscribe(int subscriptionId);
    
    /**
     * @brief Publish an event to all subscribers
     * @param event The event to publish
     */
    void publish(const Event& event);
    
    /**
     * @brief Publish an event with simplified parameters
     * @param eventType Type of the event
     * @param data Event data
     * @param sender Event sender (optional)
     */
    void publish(const QString& eventType, const QVariantMap& data = {}, QObject* sender = nullptr);
    
    /**
     * @brief Clear all subscriptions
     */
    void clear();

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    struct Subscription
    {
        int id;
        QString eventType;
        EventHandler handler;
    };
    
    QHash<QString, QList<Subscription>> m_subscriptions;
    int m_nextSubscriptionId = 1;
};
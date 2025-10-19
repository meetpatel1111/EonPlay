#include "EventBus.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(eventBus, "mediaplayer.eventbus")

EventBus& EventBus::instance()
{
    static EventBus instance;
    return instance;
}

int EventBus::subscribe(const QString& eventType, EventHandler handler)
{
    int subscriptionId = m_nextSubscriptionId++;
    
    Subscription subscription;
    subscription.id = subscriptionId;
    subscription.eventType = eventType;
    subscription.handler = handler;
    
    m_subscriptions[eventType].append(subscription);
    
    qCDebug(eventBus) << "Subscribed to event type:" << eventType << "with ID:" << subscriptionId;
    
    return subscriptionId;
}

void EventBus::unsubscribe(int subscriptionId)
{
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it) {
        auto& subscriptions = it.value();
        for (int i = 0; i < subscriptions.size(); ++i) {
            if (subscriptions[i].id == subscriptionId) {
                qCDebug(eventBus) << "Unsubscribed from event type:" << it.key() << "ID:" << subscriptionId;
                subscriptions.removeAt(i);
                if (subscriptions.isEmpty()) {
                    m_subscriptions.erase(it);
                }
                return;
            }
        }
    }
    
    qCWarning(eventBus) << "Attempted to unsubscribe non-existent subscription ID:" << subscriptionId;
}

void EventBus::publish(const Event& event)
{
    const auto& subscriptions = m_subscriptions.value(event.type);
    
    qCDebug(eventBus) << "Publishing event:" << event.type << "to" << subscriptions.size() << "subscribers";
    
    for (const auto& subscription : subscriptions) {
        try {
            subscription.handler(event);
        } catch (const std::exception& e) {
            qCCritical(eventBus) << "Exception in event handler for" << event.type << ":" << e.what();
        } catch (...) {
            qCCritical(eventBus) << "Unknown exception in event handler for" << event.type;
        }
    }
}

void EventBus::publish(const QString& eventType, const QVariantMap& data, QObject* sender)
{
    Event event(eventType, data, sender);
    publish(event);
}

void EventBus::clear()
{
    qCDebug(eventBus) << "Clearing all event subscriptions";
    m_subscriptions.clear();
    m_nextSubscriptionId = 1;
}
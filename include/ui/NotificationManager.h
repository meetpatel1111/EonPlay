#pragma once

#include <QObject>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QQueue>
#include <memory>

/**
 * @brief Notification data structure
 */
struct NotificationData
{
    QString title;
    QString message;
    QString iconPath;
    int duration = 3000; // Duration in milliseconds
    bool isUrgent = false;
    
    NotificationData() = default;
    NotificationData(const QString& t, const QString& m, int d = 3000)
        : title(t), message(m), duration(d) {}
};

/**
 * @brief Custom notification widget for EonPlay
 * 
 * Provides futuristic-styled notification pop-ups for media events,
 * system messages, and user feedback with smooth animations.
 */
class NotificationWidget : public QFrame
{
    Q_OBJECT

public:
    explicit NotificationWidget(QWidget* parent = nullptr);
    ~NotificationWidget() override;
    
    /**
     * @brief Set notification content
     * @param data Notification data
     */
    void setNotification(const NotificationData& data);
    
    /**
     * @brief Show notification with animation
     */
    void showNotification();
    
    /**
     * @brief Hide notification with animation
     */
    void hideNotification();
    
    /**
     * @brief Check if notification is currently visible
     * @return true if visible
     */
    bool isNotificationVisible() const { return m_isVisible; }

signals:
    /**
     * @brief Emitted when notification is clicked
     */
    void clicked();
    
    /**
     * @brief Emitted when notification is closed
     */
    void closed();
    
    /**
     * @brief Emitted when show animation is finished
     */
    void showAnimationFinished();
    
    /**
     * @brief Emitted when hide animation is finished
     */
    void hideAnimationFinished();

protected:
    /**
     * @brief Handle mouse press events for click detection
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent* event) override;
    
    /**
     * @brief Handle paint events for custom styling
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;

private slots:
    /**
     * @brief Handle auto-hide timer timeout
     */
    void onAutoHideTimeout();
    
    /**
     * @brief Handle show animation finished
     */
    void onShowAnimationFinished();
    
    /**
     * @brief Handle hide animation finished
     */
    void onHideAnimationFinished();

private:
    /**
     * @brief Initialize the UI components
     */
    void initializeUI();
    
    /**
     * @brief Apply futuristic styling
     */
    void applyFuturisticStyling();
    
    /**
     * @brief Update notification content
     */
    void updateContent();
    
    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_contentLayout;
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    
    // Animation components
    QPropertyAnimation* m_showAnimation;
    QPropertyAnimation* m_hideAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    
    // Auto-hide timer
    QTimer* m_autoHideTimer;
    
    // Notification data
    NotificationData m_currentNotification;
    
    // State
    bool m_isVisible;
    bool m_isAnimating;
};

/**
 * @brief Notification manager for EonPlay
 * 
 * Manages notification display queue, positioning, and system integration
 * for showing media events and user feedback messages.
 */
class NotificationManager : public QObject
{
    Q_OBJECT

public:
    explicit NotificationManager(QObject* parent = nullptr);
    ~NotificationManager() override;
    
    /**
     * @brief Initialize notification manager with parent widget
     * @param parentWidget Parent widget for positioning notifications
     * @return true if initialization successful
     */
    bool initialize(QWidget* parentWidget);
    
    /**
     * @brief Show notification message
     * @param title Notification title
     * @param message Notification message
     * @param duration Duration in milliseconds (0 for default)
     * @param isUrgent Whether notification is urgent
     */
    void showNotification(const QString& title, const QString& message, 
                         int duration = 0, bool isUrgent = false);
    
    /**
     * @brief Show notification with icon
     * @param title Notification title
     * @param message Notification message
     * @param iconPath Path to notification icon
     * @param duration Duration in milliseconds (0 for default)
     * @param isUrgent Whether notification is urgent
     */
    void showNotification(const QString& title, const QString& message, 
                         const QString& iconPath, int duration = 0, bool isUrgent = false);
    
    /**
     * @brief Show notification from data structure
     * @param data Notification data
     */
    void showNotification(const NotificationData& data);
    
    /**
     * @brief Clear all pending notifications
     */
    void clearAllNotifications();
    
    /**
     * @brief Set default notification duration
     * @param duration Default duration in milliseconds
     */
    void setDefaultDuration(int duration) { m_defaultDuration = duration; }
    
    /**
     * @brief Get default notification duration
     * @return Default duration in milliseconds
     */
    int defaultDuration() const { return m_defaultDuration; }
    
    /**
     * @brief Enable or disable notifications
     * @param enabled true to enable notifications
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief Check if notifications are enabled
     * @return true if notifications are enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Set notification position on screen
     * @param position Position (TopRight, TopLeft, BottomRight, BottomLeft)
     */
    void setNotificationPosition(Qt::Corner position) { m_position = position; }
    
    /**
     * @brief Get current notification position
     * @return Current notification position
     */
    Qt::Corner notificationPosition() const { return m_position; }

public slots:
    /**
     * @brief Handle media loaded notifications
     * @param filePath Path to loaded media
     * @param title Media title
     */
    void onMediaLoaded(const QString& filePath, const QString& title = QString());
    
    /**
     * @brief Handle playback state change notifications
     * @param isPlaying Whether playback started or stopped
     * @param title Current media title
     */
    void onPlaybackStateChanged(bool isPlaying, const QString& title = QString());
    
    /**
     * @brief Handle volume change notifications
     * @param volume New volume level (0-100)
     */
    void onVolumeChanged(int volume);
    
    /**
     * @brief Handle error notifications
     * @param title Error title
     * @param message Error message
     */
    void onError(const QString& title, const QString& message);

signals:
    /**
     * @brief Emitted when notification is clicked
     * @param data Notification data that was clicked
     */
    void notificationClicked(const NotificationData& data);

private slots:
    /**
     * @brief Handle notification widget closed
     */
    void onNotificationClosed();
    
    /**
     * @brief Handle notification widget clicked
     */
    void onNotificationClicked();
    
    /**
     * @brief Process next notification in queue
     */
    void processNextNotification();

private:
    /**
     * @brief Position notification widget on screen
     * @param widget Notification widget to position
     */
    void positionNotificationWidget(NotificationWidget* widget);
    
    /**
     * @brief Create notification widget
     * @return New notification widget
     */
    NotificationWidget* createNotificationWidget();
    
    QWidget* m_parentWidget;
    
    // Notification queue
    QQueue<NotificationData> m_notificationQueue;
    
    // Active notification widget
    NotificationWidget* m_activeNotification;
    
    // Settings
    int m_defaultDuration;
    bool m_enabled;
    Qt::Corner m_position;
    
    // Spacing and margins
    static constexpr int NOTIFICATION_MARGIN = 20;
    static constexpr int NOTIFICATION_SPACING = 10;
};
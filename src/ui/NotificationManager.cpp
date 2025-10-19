#include "ui/NotificationManager.h"
#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QFontMetrics>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(notifications, "eonplay.notifications")

// NotificationWidget Implementation
NotificationWidget::NotificationWidget(QWidget* parent)
    : QFrame(parent)
    , m_mainLayout(nullptr)
    , m_contentLayout(nullptr)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_messageLabel(nullptr)
    , m_showAnimation(nullptr)
    , m_hideAnimation(nullptr)
    , m_opacityEffect(nullptr)
    , m_autoHideTimer(nullptr)
    , m_isVisible(false)
    , m_isAnimating(false)
{
    // Set widget properties
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(350, 80);
    
    // Initialize UI
    initializeUI();
    applyFuturisticStyling();
    
    // Create auto-hide timer
    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, &NotificationWidget::onAutoHideTimeout);
    
    // Create opacity effect
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);
    
    // Create animations
    m_showAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_showAnimation->setDuration(300);
    m_showAnimation->setStartValue(0.0);
    m_showAnimation->setEndValue(1.0);
    connect(m_showAnimation, &QPropertyAnimation::finished, this, &NotificationWidget::onShowAnimationFinished);
    
    m_hideAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_hideAnimation->setDuration(300);
    m_hideAnimation->setStartValue(1.0);
    m_hideAnimation->setEndValue(0.0);
    connect(m_hideAnimation, &QPropertyAnimation::finished, this, &NotificationWidget::onHideAnimationFinished);
    
    qCDebug(notifications) << "NotificationWidget created";
}

NotificationWidget::~NotificationWidget()
{
    qCDebug(notifications) << "NotificationWidget destroyed";
}

void NotificationWidget::setNotification(const NotificationData& data)
{
    m_currentNotification = data;
    updateContent();
    
    // Set auto-hide timer
    if (data.duration > 0) {
        m_autoHideTimer->setInterval(data.duration);
    }
}

void NotificationWidget::showNotification()
{
    if (m_isVisible || m_isAnimating) {
        return;
    }
    
    m_isAnimating = true;
    show();
    m_showAnimation->start();
    
    qCDebug(notifications) << "Showing notification:" << m_currentNotification.title;
}

void NotificationWidget::hideNotification()
{
    if (!m_isVisible || m_isAnimating) {
        return;
    }
    
    m_isAnimating = true;
    m_autoHideTimer->stop();
    m_hideAnimation->start();
    
    qCDebug(notifications) << "Hiding notification:" << m_currentNotification.title;
}

void NotificationWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        hideNotification();
    }
    QFrame::mousePressEvent(event);
}

void NotificationWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw futuristic background
    QRect widgetRect = rect();
    
    // Create gradient background
    QLinearGradient gradient(0, 0, 0, height());
    if (m_currentNotification.isUrgent) {
        gradient.setColorAt(0, QColor(60, 20, 20, 240));
        gradient.setColorAt(1, QColor(40, 10, 10, 240));
    } else {
        gradient.setColorAt(0, QColor(20, 30, 50, 240));
        gradient.setColorAt(1, QColor(10, 15, 25, 240));
    }
    
    painter.fillRect(widgetRect, gradient);
    
    // Draw border
    QPen borderPen(m_currentNotification.isUrgent ? QColor(255, 100, 100, 200) : QColor(100, 150, 255, 200), 2);
    painter.setPen(borderPen);
    painter.drawRoundedRect(widgetRect.adjusted(1, 1, -2, -2), 8, 8);
    
    // Draw subtle glow effect
    QRadialGradient glowGradient(widgetRect.center(), qMin(width(), height()) / 2);
    glowGradient.setColorAt(0, QColor(100, 150, 255, 30));
    glowGradient.setColorAt(1, QColor(100, 150, 255, 0));
    painter.fillRect(widgetRect, glowGradient);
    
    QFrame::paintEvent(event);
}

void NotificationWidget::onAutoHideTimeout()
{
    hideNotification();
}

void NotificationWidget::onShowAnimationFinished()
{
    m_isAnimating = false;
    m_isVisible = true;
    
    // Start auto-hide timer
    if (m_currentNotification.duration > 0) {
        m_autoHideTimer->start();
    }
    
    emit showAnimationFinished();
}

void NotificationWidget::onHideAnimationFinished()
{
    m_isAnimating = false;
    m_isVisible = false;
    hide();
    
    emit hideAnimationFinished();
    emit closed();
}

void NotificationWidget::initializeUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 10, 15, 10);
    m_mainLayout->setSpacing(5);
    
    // Create content layout
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setSpacing(10);
    
    // Create icon label
    m_iconLabel = new QLabel();
    m_iconLabel->setFixedSize(32, 32);
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(m_iconLabel);
    
    // Create text layout
    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    
    // Create title label
    m_titleLabel = new QLabel();
    m_titleLabel->setWordWrap(false);
    textLayout->addWidget(m_titleLabel);
    
    // Create message label
    m_messageLabel = new QLabel();
    m_messageLabel->setWordWrap(true);
    textLayout->addWidget(m_messageLabel);
    
    m_contentLayout->addLayout(textLayout, 1);
    m_mainLayout->addLayout(m_contentLayout);
}

void NotificationWidget::applyFuturisticStyling()
{
    // Title label styling
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "    color: #00d4ff;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    background: transparent;"
        "    border: none;"
        "}"
    );
    
    // Message label styling
    m_messageLabel->setStyleSheet(
        "QLabel {"
        "    color: #ffffff;"
        "    font-size: 12px;"
        "    background: transparent;"
        "    border: none;"
        "}"
    );
    
    // Icon label styling
    m_iconLabel->setStyleSheet(
        "QLabel {"
        "    background: transparent;"
        "    border: 1px solid rgba(100, 150, 255, 100);"
        "    border-radius: 16px;"
        "}"
    );
}

void NotificationWidget::updateContent()
{
    // Set title
    m_titleLabel->setText(m_currentNotification.title);
    
    // Set message
    m_messageLabel->setText(m_currentNotification.message);
    
    // Set icon
    if (!m_currentNotification.iconPath.isEmpty()) {
        QPixmap iconPixmap(m_currentNotification.iconPath);
        if (!iconPixmap.isNull()) {
            m_iconLabel->setPixmap(iconPixmap);
            m_iconLabel->setVisible(true);
        } else {
            m_iconLabel->setVisible(false);
        }
    } else {
        // Use default icon based on notification type
        QPixmap defaultIcon;
        if (m_currentNotification.isUrgent) {
            defaultIcon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(32, 32);
        } else {
            defaultIcon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(32, 32);
        }
        m_iconLabel->setPixmap(defaultIcon);
        m_iconLabel->setVisible(true);
    }
}

// NotificationManager Implementation
NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent)
    , m_parentWidget(nullptr)
    , m_activeNotification(nullptr)
    , m_defaultDuration(3000)
    , m_enabled(true)
    , m_position(Qt::TopRightCorner)
{
    qCDebug(notifications) << "NotificationManager created";
}

NotificationManager::~NotificationManager()
{
    clearAllNotifications();
    
    if (m_activeNotification) {
        m_activeNotification->deleteLater();
    }
    
    qCDebug(notifications) << "NotificationManager destroyed";
}

bool NotificationManager::initialize(QWidget* parentWidget)
{
    if (!parentWidget) {
        qCWarning(notifications) << "Cannot initialize with null parent widget";
        return false;
    }
    
    m_parentWidget = parentWidget;
    
    qCDebug(notifications) << "NotificationManager initialized";
    return true;
}

void NotificationManager::showNotification(const QString& title, const QString& message, 
                                          int duration, bool isUrgent)
{
    NotificationData data(title, message, duration > 0 ? duration : m_defaultDuration);
    data.isUrgent = isUrgent;
    showNotification(data);
}

void NotificationManager::showNotification(const QString& title, const QString& message, 
                                          const QString& iconPath, int duration, bool isUrgent)
{
    NotificationData data(title, message, duration > 0 ? duration : m_defaultDuration);
    data.iconPath = iconPath;
    data.isUrgent = isUrgent;
    showNotification(data);
}

void NotificationManager::showNotification(const NotificationData& data)
{
    if (!m_enabled || !m_parentWidget) {
        return;
    }
    
    // Add to queue
    m_notificationQueue.enqueue(data);
    
    // Process if no active notification
    if (!m_activeNotification || !m_activeNotification->isNotificationVisible()) {
        processNextNotification();
    }
    
    qCDebug(notifications) << "Notification queued:" << data.title;
}

void NotificationManager::clearAllNotifications()
{
    m_notificationQueue.clear();
    
    if (m_activeNotification && m_activeNotification->isNotificationVisible()) {
        m_activeNotification->hideNotification();
    }
    
    qCDebug(notifications) << "All notifications cleared";
}

void NotificationManager::onMediaLoaded(const QString& filePath, const QString& title)
{
    QString mediaTitle = title.isEmpty() ? QFileInfo(filePath).baseName() : title;
    showNotification(tr("Media Loaded"), tr("Now playing: %1").arg(mediaTitle));
}

void NotificationManager::onPlaybackStateChanged(bool isPlaying, const QString& title)
{
    if (!title.isEmpty()) {
        QString status = isPlaying ? tr("Playing") : tr("Paused");
        showNotification(status, title, 2000); // Shorter duration for state changes
    }
}

void NotificationManager::onVolumeChanged(int volume)
{
    showNotification(tr("Volume"), tr("Volume: %1%").arg(volume), 1500); // Short duration
}

void NotificationManager::onError(const QString& title, const QString& message)
{
    showNotification(title, message, 5000, true); // Longer duration for errors, marked as urgent
}

void NotificationManager::onNotificationClosed()
{
    // Process next notification in queue
    QTimer::singleShot(100, this, &NotificationManager::processNextNotification);
}

void NotificationManager::onNotificationClicked()
{
    NotificationWidget* widget = qobject_cast<NotificationWidget*>(sender());
    if (widget && widget == m_activeNotification) {
        emit notificationClicked(m_activeNotification->m_currentNotification);
    }
}

void NotificationManager::processNextNotification()
{
    if (m_notificationQueue.isEmpty()) {
        return;
    }
    
    // Clean up previous notification
    if (m_activeNotification) {
        m_activeNotification->deleteLater();
        m_activeNotification = nullptr;
    }
    
    // Get next notification
    NotificationData data = m_notificationQueue.dequeue();
    
    // Create new notification widget
    m_activeNotification = createNotificationWidget();
    m_activeNotification->setNotification(data);
    
    // Position and show
    positionNotificationWidget(m_activeNotification);
    m_activeNotification->showNotification();
    
    qCDebug(notifications) << "Processing notification:" << data.title;
}

void NotificationManager::positionNotificationWidget(NotificationWidget* widget)
{
    if (!widget || !m_parentWidget) {
        return;
    }
    
    QRect parentGeometry = m_parentWidget->geometry();
    QSize widgetSize = widget->size();
    
    int x, y;
    
    switch (m_position) {
    case Qt::TopRightCorner:
        x = parentGeometry.right() - widgetSize.width() - NOTIFICATION_MARGIN;
        y = parentGeometry.top() + NOTIFICATION_MARGIN;
        break;
    case Qt::TopLeftCorner:
        x = parentGeometry.left() + NOTIFICATION_MARGIN;
        y = parentGeometry.top() + NOTIFICATION_MARGIN;
        break;
    case Qt::BottomRightCorner:
        x = parentGeometry.right() - widgetSize.width() - NOTIFICATION_MARGIN;
        y = parentGeometry.bottom() - widgetSize.height() - NOTIFICATION_MARGIN;
        break;
    case Qt::BottomLeftCorner:
        x = parentGeometry.left() + NOTIFICATION_MARGIN;
        y = parentGeometry.bottom() - widgetSize.height() - NOTIFICATION_MARGIN;
        break;
    }
    
    widget->move(x, y);
}

NotificationWidget* NotificationManager::createNotificationWidget()
{
    NotificationWidget* widget = new NotificationWidget(m_parentWidget);
    
    // Connect signals
    connect(widget, &NotificationWidget::closed, this, &NotificationManager::onNotificationClosed);
    connect(widget, &NotificationWidget::clicked, this, &NotificationManager::onNotificationClicked);
    
    return widget;
}
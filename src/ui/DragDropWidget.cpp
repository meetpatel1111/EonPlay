#include "ui/DragDropWidget.h"
#include "media/FileUrlSupport.h"
#include <QLoggingCategory>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRect>
#include <QApplication>
#include <QStyle>

Q_LOGGING_CATEGORY(dragDropWidget, "eonplay.dragdropwidget")

DragDropWidget::DragDropWidget(QWidget* parent)
    : QWidget(parent)
    , m_dragDropEnabled(true)
    , m_visualFeedbackEnabled(true)
    , m_dragActive(false)
    , m_hasValidDragData(false)
    , m_validDropColor(0, 150, 255, 50)      // Light blue with transparency
    , m_invalidDropColor(255, 100, 100, 50)  // Light red with transparency
    , m_borderColor(0, 150, 255, 200)        // Solid blue border
{
    setAcceptDrops(true);
    
    qCDebug(dragDropWidget) << "DragDropWidget created";
}

DragDropWidget::~DragDropWidget()
{
    qCDebug(dragDropWidget) << "DragDropWidget destroyed";
}

void DragDropWidget::setFileUrlSupport(std::shared_ptr<FileUrlSupport> fileUrlSupport)
{
    // Disconnect from previous instance
    if (m_fileUrlSupport) {
        disconnect(m_fileUrlSupport.get(), nullptr, this, nullptr);
    }
    
    m_fileUrlSupport = fileUrlSupport;
    
    // Connect to new instance
    if (m_fileUrlSupport) {
        connect(m_fileUrlSupport.get(), &FileUrlSupport::dragDropProcessed,
                this, &DragDropWidget::onDragDropProcessed);
        
        qCDebug(dragDropWidget) << "FileUrlSupport connected";
    } else {
        qCDebug(dragDropWidget) << "FileUrlSupport disconnected";
    }
}

void DragDropWidget::setDragDropEnabled(bool enabled)
{
    if (m_dragDropEnabled != enabled) {
        m_dragDropEnabled = enabled;
        setAcceptDrops(enabled);
        
        qCDebug(dragDropWidget) << "Drag and drop" << (enabled ? "enabled" : "disabled");
        
        // Clear any active drag state when disabling
        if (!enabled && m_dragActive) {
            m_dragActive = false;
            m_hasValidDragData = false;
            update();
        }
    }
}

void DragDropWidget::setVisualFeedbackEnabled(bool enabled)
{
    if (m_visualFeedbackEnabled != enabled) {
        m_visualFeedbackEnabled = enabled;
        
        qCDebug(dragDropWidget) << "Visual feedback" << (enabled ? "enabled" : "disabled");
        
        // Trigger repaint if drag is active
        if (m_dragActive) {
            update();
        }
    }
}

void DragDropWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (!m_dragDropEnabled) {
        event->ignore();
        return;
    }
    
    qCDebug(dragDropWidget) << "Drag enter event";
    
    bool hasValidData = hasValidMediaData(event->mimeData());
    
    if (hasValidData) {
        event->acceptProposedAction();
        qCDebug(dragDropWidget) << "Drag accepted - valid media data found";
    } else {
        event->ignore();
        qCDebug(dragDropWidget) << "Drag rejected - no valid media data";
    }
    
    // Update visual state
    m_dragActive = true;
    m_hasValidDragData = hasValidData;
    
    if (m_visualFeedbackEnabled) {
        update();
    }
    
    emit dragEntered(hasValidData);
}

void DragDropWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (!m_dragDropEnabled) {
        event->ignore();
        return;
    }
    
    // Accept the drag if we have valid data
    if (m_hasValidDragData) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DragDropWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event)
    
    qCDebug(dragDropWidget) << "Drag leave event";
    
    // Clear drag state
    m_dragActive = false;
    m_hasValidDragData = false;
    
    if (m_visualFeedbackEnabled) {
        update();
    }
    
    emit dragLeft();
}

void DragDropWidget::dropEvent(QDropEvent* event)
{
    if (!m_dragDropEnabled || !m_fileUrlSupport) {
        event->ignore();
        return;
    }
    
    qCDebug(dragDropWidget) << "Drop event";
    
    const QMimeData* mimeData = event->mimeData();
    
    if (!hasValidMediaData(mimeData)) {
        qCWarning(dragDropWidget) << "Dropped data contains no valid media";
        event->ignore();
        return;
    }
    
    // Process the dropped media
    QStringList processedFiles = m_fileUrlSupport->processDroppedMedia(mimeData);
    
    if (!processedFiles.isEmpty()) {
        event->acceptProposedAction();
        qCDebug(dragDropWidget) << "Drop processed successfully:" << processedFiles.size() << "files";
    } else {
        event->ignore();
        qCWarning(dragDropWidget) << "Failed to process any dropped files";
    }
    
    // Clear drag state
    m_dragActive = false;
    m_hasValidDragData = false;
    
    if (m_visualFeedbackEnabled) {
        update();
    }
}

void DragDropWidget::paintEvent(QPaintEvent* event)
{
    // Call parent paint event first
    QWidget::paintEvent(event);
    
    // Draw drag and drop visual feedback
    if (m_dragActive && m_visualFeedbackEnabled) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QRect widgetRect = rect();
        
        // Choose colors based on drag data validity
        QColor fillColor = m_hasValidDragData ? m_validDropColor : m_invalidDropColor;
        QColor borderColor = m_hasValidDragData ? m_borderColor : QColor(255, 100, 100, 200);
        
        // Draw background overlay
        painter.fillRect(widgetRect, fillColor);
        
        // Draw border
        QPen borderPen(borderColor, 3, Qt::DashLine);
        painter.setPen(borderPen);
        painter.drawRect(widgetRect.adjusted(1, 1, -2, -2));
        
        // Draw drop indicator text
        if (m_hasValidDragData) {
            painter.setPen(QPen(QColor(0, 100, 200), 2));
            QFont font = painter.font();
            font.setPointSize(font.pointSize() + 4);
            font.setBold(true);
            painter.setFont(font);
            
            QString text = "Drop media files here";
            painter.drawText(widgetRect, Qt::AlignCenter, text);
        } else {
            painter.setPen(QPen(QColor(200, 50, 50), 2));
            QFont font = painter.font();
            font.setPointSize(font.pointSize() + 2);
            painter.setFont(font);
            
            QString text = "Unsupported file type";
            painter.drawText(widgetRect, Qt::AlignCenter, text);
        }
    }
}

void DragDropWidget::onDragDropProcessed(const QStringList& processedFiles, const QStringList& failedFiles)
{
    qCDebug(dragDropWidget) << "Drag drop processing completed";
    qCDebug(dragDropWidget) << "Processed:" << processedFiles.size() << "Failed:" << failedFiles.size();
    
    emit filesDropped(processedFiles, failedFiles);
    
    // Emit URL signals for processed URLs
    for (const QString& file : processedFiles) {
        QUrl url(file);
        if (url.isValid() && !url.isLocalFile()) {
            emit urlDropped(url, true);
        }
    }
    
    // Emit URL signals for failed URLs
    for (const QString& file : failedFiles) {
        QUrl url(file);
        if (url.isValid() && !url.isLocalFile()) {
            emit urlDropped(url, false);
        }
    }
}

bool DragDropWidget::hasValidMediaData(const QMimeData* mimeData) const
{
    if (!mimeData || !m_fileUrlSupport) {
        return false;
    }
    
    return m_fileUrlSupport->canHandleMimeData(mimeData);
}

QStringList DragDropWidget::extractFilePaths(const QMimeData* mimeData) const
{
    QStringList filePaths;
    
    if (!mimeData) {
        return filePaths;
    }
    
    // Extract file paths from URLs
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                filePaths << url.toLocalFile();
            }
        }
    }
    
    return filePaths;
}

QList<QUrl> DragDropWidget::extractUrls(const QMimeData* mimeData) const
{
    QList<QUrl> urls;
    
    if (!mimeData) {
        return urls;
    }
    
    // Extract URLs
    if (mimeData->hasUrls()) {
        urls = mimeData->urls();
    }
    
    // Also check for text URLs
    if (mimeData->hasText() && urls.isEmpty()) {
        QString text = mimeData->text().trimmed();
        QUrl url(text);
        if (url.isValid()) {
            urls << url;
        }
    }
    
    return urls;
}
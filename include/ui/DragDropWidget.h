#pragma once

#include <QWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QStringList>
#include <memory>

class FileUrlSupport;

/**
 * @brief Widget that handles drag and drop operations for media files and URLs
 * 
 * This widget can be used as a base class or overlay for any widget that needs
 * to accept dropped media files and URLs for playback in EonPlay.
 */
class DragDropWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DragDropWidget(QWidget* parent = nullptr);
    ~DragDropWidget() override;
    
    /**
     * @brief Set the file URL support component
     * @param fileUrlSupport Shared pointer to FileUrlSupport component
     */
    void setFileUrlSupport(std::shared_ptr<FileUrlSupport> fileUrlSupport);
    
    /**
     * @brief Enable or disable drag and drop functionality
     * @param enabled true to enable drag and drop
     */
    void setDragDropEnabled(bool enabled);
    
    /**
     * @brief Check if drag and drop is enabled
     * @return true if drag and drop is enabled
     */
    bool isDragDropEnabled() const { return m_dragDropEnabled; }
    
    /**
     * @brief Set visual feedback during drag operations
     * @param enabled true to show visual feedback
     */
    void setVisualFeedbackEnabled(bool enabled);
    
    /**
     * @brief Check if visual feedback is enabled
     * @return true if visual feedback is enabled
     */
    bool isVisualFeedbackEnabled() const { return m_visualFeedbackEnabled; }

protected:
    // Drag and drop event handlers
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
    // Paint event for visual feedback
    void paintEvent(QPaintEvent* event) override;

signals:
    /**
     * @brief Emitted when files are dropped and processed
     * @param processedFiles List of successfully processed files
     * @param failedFiles List of files that failed to process
     */
    void filesDropped(const QStringList& processedFiles, const QStringList& failedFiles);
    
    /**
     * @brief Emitted when a URL is dropped and processed
     * @param url Processed URL
     * @param success true if URL was processed successfully
     */
    void urlDropped(const QUrl& url, bool success);
    
    /**
     * @brief Emitted when drag operation enters the widget
     * @param hasValidData true if dragged data contains valid media
     */
    void dragEntered(bool hasValidData);
    
    /**
     * @brief Emitted when drag operation leaves the widget
     */
    void dragLeft();

private slots:
    /**
     * @brief Handle drag and drop processing completion
     * @param processedFiles List of successfully processed files
     * @param failedFiles List of files that failed to process
     */
    void onDragDropProcessed(const QStringList& processedFiles, const QStringList& failedFiles);

private:
    /**
     * @brief Check if mime data contains valid media files or URLs
     * @param mimeData Mime data to check
     * @return true if mime data contains valid media
     */
    bool hasValidMediaData(const QMimeData* mimeData) const;
    
    /**
     * @brief Extract file paths from mime data
     * @param mimeData Mime data containing URLs
     * @return List of file paths
     */
    QStringList extractFilePaths(const QMimeData* mimeData) const;
    
    /**
     * @brief Extract URLs from mime data
     * @param mimeData Mime data containing URLs
     * @return List of URLs
     */
    QList<QUrl> extractUrls(const QMimeData* mimeData) const;
    
    std::shared_ptr<FileUrlSupport> m_fileUrlSupport;
    
    bool m_dragDropEnabled;
    bool m_visualFeedbackEnabled;
    bool m_dragActive;
    bool m_hasValidDragData;
    
    // Visual feedback colors
    QColor m_validDropColor;
    QColor m_invalidDropColor;
    QColor m_borderColor;
};
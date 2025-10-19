#pragma once

#include "data/MediaFile.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QVariant>

namespace EonPlay {
namespace Data {

class DatabaseManager;

/**
 * @brief Represents a playlist in the EonPlay library
 * 
 * Manages a collection of media files with ordering and metadata.
 */
class Playlist : public QObject
{
    Q_OBJECT

public:
    explicit Playlist(QObject* parent = nullptr);
    explicit Playlist(const QString& name, QObject* parent = nullptr);
    Playlist(const Playlist& other);
    Playlist& operator=(const Playlist& other);
    ~Playlist() = default;

    // Basic properties
    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& name);

    QString description() const { return m_description; }
    void setDescription(const QString& description) { m_description = description; }

    QDateTime createdDate() const { return m_createdDate; }
    void setCreatedDate(const QDateTime& createdDate) { m_createdDate = createdDate; }

    QDateTime modifiedDate() const { return m_modifiedDate; }
    void setModifiedDate(const QDateTime& modifiedDate) { m_modifiedDate = modifiedDate; }

    // Items management
    int itemCount() const { return m_items.size(); }
    qint64 totalDuration() const;
    qint64 totalSize() const;

    const QList<MediaFile>& items() const { return m_items; }
    MediaFile item(int index) const;
    int indexOf(const MediaFile& file) const;
    bool contains(const MediaFile& file) const;
    bool containsPath(const QString& filePath) const;

    // Playlist operations
    void addItem(const MediaFile& file);
    void insertItem(int index, const MediaFile& file);
    bool removeItem(int index);
    bool removeItem(const MediaFile& file);
    bool moveItem(int fromIndex, int toIndex);
    void clear();

    // Batch operations
    void addItems(const QList<MediaFile>& files);
    void removeItems(const QList<int>& indices);

    // Playback support
    MediaFile currentItem() const { return item(m_currentIndex); }
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    
    MediaFile nextItem() const;
    MediaFile previousItem() const;
    bool hasNext() const;
    bool hasPrevious() const;

    // Shuffle and repeat modes
    enum RepeatMode {
        NoRepeat,
        RepeatOne,
        RepeatAll
    };

    bool isShuffled() const { return m_shuffled; }
    void setShuffle(bool enabled);

    RepeatMode repeatMode() const { return m_repeatMode; }
    void setRepeatMode(RepeatMode mode) { m_repeatMode = mode; }

    // Search and filtering
    QList<MediaFile> search(const QString& query) const;
    QList<MediaFile> filterByArtist(const QString& artist) const;
    QList<MediaFile> filterByAlbum(const QString& album) const;
    QList<MediaFile> filterByGenre(const QString& genre) const;

    // Sorting
    enum SortOrder {
        SortByPosition,
        SortByTitle,
        SortByArtist,
        SortByAlbum,
        SortByDuration,
        SortByDateAdded
    };

    void sort(SortOrder order, Qt::SortOrder direction = Qt::AscendingOrder);

    // Validation
    bool isValid() const;
    bool isEmpty() const { return m_items.isEmpty(); }

    // Statistics
    QString durationString() const;
    QString sizeString() const;
    QString itemCountString() const;

    // Persistence (requires DatabaseManager)
    bool save(DatabaseManager* dbManager);
    bool load(int playlistId, DatabaseManager* dbManager);
    bool remove(DatabaseManager* dbManager);

    // Serialization
    QVariantMap toVariantMap() const;
    void fromVariantMap(const QVariantMap& map);

    // Export/Import
    bool exportToM3U(const QString& filePath) const;
    bool exportToPLS(const QString& filePath) const;
    bool importFromM3U(const QString& filePath);
    bool importFromPLS(const QString& filePath);

    // Comparison operators
    bool operator==(const Playlist& other) const;
    bool operator!=(const Playlist& other) const { return !(*this == other); }

signals:
    void nameChanged(const QString& name);
    void itemAdded(int index, const MediaFile& file);
    void itemRemoved(int index, const MediaFile& file);
    void itemMoved(int fromIndex, int toIndex);
    void currentIndexChanged(int index);
    void shuffleChanged(bool enabled);
    void repeatModeChanged(RepeatMode mode);
    void playlistCleared();
    void playlistModified();

private slots:
    void updateModifiedDate();

private:
    void generateShuffleOrder();
    int getShuffledIndex(int logicalIndex) const;
    int getLogicalIndex(int shuffledIndex) const;
    void copyFrom(const Playlist& other);

    // Core properties
    int m_id;
    QString m_name;
    QString m_description;
    QDateTime m_createdDate;
    QDateTime m_modifiedDate;

    // Items
    QList<MediaFile> m_items;
    int m_currentIndex;

    // Playback modes
    bool m_shuffled;
    RepeatMode m_repeatMode;
    QList<int> m_shuffleOrder;

    // Cached statistics
    mutable qint64 m_cachedTotalDuration;
    mutable qint64 m_cachedTotalSize;
    mutable bool m_statisticsCached;
};

} // namespace Data
} // namespace EonPlay
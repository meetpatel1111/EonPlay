#pragma once

#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include <QVariant>
#include <QHash>

namespace EonPlay {
namespace Data {

/**
 * @brief Represents a media file in the EonPlay library
 * 
 * Contains metadata and playback information for audio and video files.
 */
class MediaFile
{
public:
    MediaFile();
    explicit MediaFile(const QString& filePath);
    MediaFile(const MediaFile& other);
    MediaFile& operator=(const MediaFile& other);
    ~MediaFile() = default;

    // Basic properties
    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& filePath);

    QString title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; }

    QString artist() const { return m_artist; }
    void setArtist(const QString& artist) { m_artist = artist; }

    QString album() const { return m_album; }
    void setAlbum(const QString& album) { m_album = album; }

    QString genre() const { return m_genre; }
    void setGenre(const QString& genre) { m_genre = genre; }

    int year() const { return m_year; }
    void setYear(int year) { m_year = year; }

    int trackNumber() const { return m_trackNumber; }
    void setTrackNumber(int trackNumber) { m_trackNumber = trackNumber; }

    // Duration and size
    qint64 duration() const { return m_duration; }
    void setDuration(qint64 duration) { m_duration = duration; }

    qint64 fileSize() const { return m_fileSize; }
    void setFileSize(qint64 fileSize) { m_fileSize = fileSize; }

    // Timestamps
    QDateTime dateAdded() const { return m_dateAdded; }
    void setDateAdded(const QDateTime& dateAdded) { m_dateAdded = dateAdded; }

    QDateTime dateModified() const { return m_dateModified; }
    void setDateModified(const QDateTime& dateModified) { m_dateModified = dateModified; }

    QDateTime lastPlayed() const { return m_lastPlayed; }
    void setLastPlayed(const QDateTime& lastPlayed) { m_lastPlayed = lastPlayed; }

    // Playback statistics
    int playCount() const { return m_playCount; }
    void setPlayCount(int playCount) { m_playCount = playCount; }
    void incrementPlayCount() { m_playCount++; }

    int rating() const { return m_rating; }
    void setRating(int rating);

    // Cover art
    QString coverArtPath() const { return m_coverArtPath; }
    void setCoverArtPath(const QString& coverArtPath) { m_coverArtPath = coverArtPath; }

    // Metadata hash for change detection
    QString metadataHash() const { return m_metadataHash; }
    void setMetadataHash(const QString& hash) { m_metadataHash = hash; }

    // File operations
    bool fileExists() const;
    QFileInfo fileInfo() const;
    QString fileName() const;
    QString fileExtension() const;
    QString displayName() const;

    // Validation
    bool isValid() const;
    bool hasMetadata() const;

    // Media type detection
    enum MediaType {
        Unknown,
        Audio,
        Video
    };
    
    MediaType mediaType() const;
    bool isAudio() const { return mediaType() == Audio; }
    bool isVideo() const { return mediaType() == Video; }

    // Format helpers
    QString durationString() const;
    QString fileSizeString() const;
    QString formatInfo() const;

    // Comparison operators
    bool operator==(const MediaFile& other) const;
    bool operator!=(const MediaFile& other) const { return !(*this == other); }
    bool operator<(const MediaFile& other) const;

    // Serialization
    QVariantMap toVariantMap() const;
    void fromVariantMap(const QVariantMap& map);

    // Static helpers
    static QStringList supportedAudioExtensions();
    static QStringList supportedVideoExtensions();
    static QStringList supportedExtensions();
    static bool isSupportedFile(const QString& filePath);
    static MediaType detectMediaType(const QString& filePath);

private:
    void updateFileInfo();
    QString calculateMetadataHash() const;

    // Core properties
    int m_id;
    QString m_filePath;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_genre;
    int m_year;
    int m_trackNumber;

    // File properties
    qint64 m_duration;      // Duration in milliseconds
    qint64 m_fileSize;      // File size in bytes

    // Timestamps
    QDateTime m_dateAdded;
    QDateTime m_dateModified;
    QDateTime m_lastPlayed;

    // Statistics
    int m_playCount;
    int m_rating;           // Rating 0-5

    // Additional metadata
    QString m_coverArtPath;
    QString m_metadataHash;

    // Cached file info
    mutable QFileInfo m_fileInfo;
    mutable bool m_fileInfoCached;
};

// Hash function for use in QHash
inline uint qHash(const MediaFile& file, uint seed = 0)
{
    return qHash(file.filePath(), seed);
}

} // namespace Data
} // namespace EonPlay
#include "data/MediaFile.h"
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QStringList>
#include <QRegularExpression>
#include <QDebug>

namespace EonPlay {
namespace Data {

MediaFile::MediaFile()
    : m_id(-1)
    , m_year(0)
    , m_trackNumber(0)
    , m_duration(0)
    , m_fileSize(0)
    , m_playCount(0)
    , m_rating(0)
    , m_fileInfoCached(false)
{
}

MediaFile::MediaFile(const QString& filePath)
    : MediaFile()
{
    setFilePath(filePath);
}

MediaFile::MediaFile(const MediaFile& other)
    : m_id(other.m_id)
    , m_filePath(other.m_filePath)
    , m_title(other.m_title)
    , m_artist(other.m_artist)
    , m_album(other.m_album)
    , m_genre(other.m_genre)
    , m_year(other.m_year)
    , m_trackNumber(other.m_trackNumber)
    , m_duration(other.m_duration)
    , m_fileSize(other.m_fileSize)
    , m_dateAdded(other.m_dateAdded)
    , m_dateModified(other.m_dateModified)
    , m_lastPlayed(other.m_lastPlayed)
    , m_playCount(other.m_playCount)
    , m_rating(other.m_rating)
    , m_coverArtPath(other.m_coverArtPath)
    , m_metadataHash(other.m_metadataHash)
    , m_fileInfoCached(false)
{
}

MediaFile& MediaFile::operator=(const MediaFile& other)
{
    if (this != &other) {
        m_id = other.m_id;
        m_filePath = other.m_filePath;
        m_title = other.m_title;
        m_artist = other.m_artist;
        m_album = other.m_album;
        m_genre = other.m_genre;
        m_year = other.m_year;
        m_trackNumber = other.m_trackNumber;
        m_duration = other.m_duration;
        m_fileSize = other.m_fileSize;
        m_dateAdded = other.m_dateAdded;
        m_dateModified = other.m_dateModified;
        m_lastPlayed = other.m_lastPlayed;
        m_playCount = other.m_playCount;
        m_rating = other.m_rating;
        m_coverArtPath = other.m_coverArtPath;
        m_metadataHash = other.m_metadataHash;
        m_fileInfoCached = false;
    }
    return *this;
}

void MediaFile::setFilePath(const QString& filePath)
{
    if (m_filePath != filePath) {
        m_filePath = filePath;
        m_fileInfoCached = false;
        updateFileInfo();
    }
}

void MediaFile::setRating(int rating)
{
    m_rating = qBound(0, rating, 5);
}

bool MediaFile::fileExists() const
{
    return fileInfo().exists();
}

QFileInfo MediaFile::fileInfo() const
{
    if (!m_fileInfoCached) {
        m_fileInfo = QFileInfo(m_filePath);
        m_fileInfoCached = true;
    }
    return m_fileInfo;
}

QString MediaFile::fileName() const
{
    return fileInfo().fileName();
}

QString MediaFile::fileExtension() const
{
    return fileInfo().suffix().toLower();
}

QString MediaFile::displayName() const
{
    if (!m_title.isEmpty()) {
        if (!m_artist.isEmpty()) {
            return QString("%1 - %2").arg(m_artist, m_title);
        }
        return m_title;
    }
    return fileName();
}

bool MediaFile::isValid() const
{
    return !m_filePath.isEmpty() && fileExists();
}

bool MediaFile::hasMetadata() const
{
    return !m_title.isEmpty() || !m_artist.isEmpty() || !m_album.isEmpty();
}

MediaFile::MediaType MediaFile::mediaType() const
{
    return detectMediaType(m_filePath);
}

QString MediaFile::durationString() const
{
    if (m_duration <= 0) {
        return "00:00";
    }

    qint64 seconds = m_duration / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
               .arg(hours)
               .arg(minutes, 2, 10, QChar('0'))
               .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
               .arg(minutes)
               .arg(seconds, 2, 10, QChar('0'));
    }
}

QString MediaFile::fileSizeString() const
{
    if (m_fileSize <= 0) {
        return "0 B";
    }

    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    double size = m_fileSize;
    int unitIndex = 0;

    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }

    return QString("%1 %2").arg(size, 0, 'f', unitIndex == 0 ? 0 : 1).arg(units[unitIndex]);
}

QString MediaFile::formatInfo() const
{
    QStringList info;
    
    if (!fileExtension().isEmpty()) {
        info << fileExtension().toUpper();
    }
    
    if (m_duration > 0) {
        info << durationString();
    }
    
    if (m_fileSize > 0) {
        info << fileSizeString();
    }
    
    return info.join(" • ");
}

bool MediaFile::operator==(const MediaFile& other) const
{
    return m_filePath == other.m_filePath;
}

bool MediaFile::operator<(const MediaFile& other) const
{
    // Sort by artist, then album, then track number, then title
    if (m_artist != other.m_artist) {
        return m_artist < other.m_artist;
    }
    if (m_album != other.m_album) {
        return m_album < other.m_album;
    }
    if (m_trackNumber != other.m_trackNumber) {
        return m_trackNumber < other.m_trackNumber;
    }
    return m_title < other.m_title;
}

QVariantMap MediaFile::toVariantMap() const
{
    QVariantMap map;
    map["id"] = m_id;
    map["filePath"] = m_filePath;
    map["title"] = m_title;
    map["artist"] = m_artist;
    map["album"] = m_album;
    map["genre"] = m_genre;
    map["year"] = m_year;
    map["trackNumber"] = m_trackNumber;
    map["duration"] = m_duration;
    map["fileSize"] = m_fileSize;
    map["dateAdded"] = m_dateAdded;
    map["dateModified"] = m_dateModified;
    map["lastPlayed"] = m_lastPlayed;
    map["playCount"] = m_playCount;
    map["rating"] = m_rating;
    map["coverArtPath"] = m_coverArtPath;
    map["metadataHash"] = m_metadataHash;
    return map;
}

void MediaFile::fromVariantMap(const QVariantMap& map)
{
    m_id = map.value("id", -1).toInt();
    setFilePath(map.value("filePath").toString());
    m_title = map.value("title").toString();
    m_artist = map.value("artist").toString();
    m_album = map.value("album").toString();
    m_genre = map.value("genre").toString();
    m_year = map.value("year", 0).toInt();
    m_trackNumber = map.value("trackNumber", 0).toInt();
    m_duration = map.value("duration", 0).toLongLong();
    m_fileSize = map.value("fileSize", 0).toLongLong();
    m_dateAdded = map.value("dateAdded").toDateTime();
    m_dateModified = map.value("dateModified").toDateTime();
    m_lastPlayed = map.value("lastPlayed").toDateTime();
    m_playCount = map.value("playCount", 0).toInt();
    m_rating = map.value("rating", 0).toInt();
    m_coverArtPath = map.value("coverArtPath").toString();
    m_metadataHash = map.value("metadataHash").toString();
}

QStringList MediaFile::supportedAudioExtensions()
{
    static const QStringList extensions = {
        "mp3", "flac", "wav", "aac", "ogg", "wma", "m4a", "opus", "ape", "wv"
    };
    return extensions;
}

QStringList MediaFile::supportedVideoExtensions()
{
    static const QStringList extensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "3gp", "ogv", "ts", "mts"
    };
    return extensions;
}

QStringList MediaFile::supportedExtensions()
{
    QStringList all = supportedAudioExtensions();
    all.append(supportedVideoExtensions());
    return all;
}

bool MediaFile::isSupportedFile(const QString& filePath)
{
    QFileInfo info(filePath);
    QString extension = info.suffix().toLower();
    return supportedExtensions().contains(extension);
}

MediaFile::MediaType MediaFile::detectMediaType(const QString& filePath)
{
    QFileInfo info(filePath);
    QString extension = info.suffix().toLower();
    
    if (supportedAudioExtensions().contains(extension)) {
        return Audio;
    } else if (supportedVideoExtensions().contains(extension)) {
        return Video;
    }
    
    return Unknown;
}

void MediaFile::updateFileInfo()
{
    if (!m_filePath.isEmpty()) {
        QFileInfo info(m_filePath);
        if (info.exists()) {
            m_fileSize = info.size();
            m_dateModified = info.lastModified();
            
            // Set title from filename if not already set
            if (m_title.isEmpty()) {
                m_title = info.baseName();
            }
        }
    }
}

QString MediaFile::calculateMetadataHash() const
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(m_title.toUtf8());
    hash.addData(m_artist.toUtf8());
    hash.addData(m_album.toUtf8());
    hash.addData(m_genre.toUtf8());
    hash.addData(QByteArray::number(m_year));
    hash.addData(QByteArray::number(m_trackNumber));
    hash.addData(QByteArray::number(m_duration));
    return hash.result().toHex();
}

} // namespace Data
} // namespace EonPlay
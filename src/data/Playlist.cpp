#include "data/Playlist.h"
#include "data/DatabaseManager.h"
#include <QFileInfo>
#include <QTextStream>
#include <QStringConverter>
#include <QUrl>
#include <QDir>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

namespace EonPlay {
namespace Data {

Playlist::Playlist(QObject* parent)
    : QObject(parent)
    , m_id(-1)
    , m_createdDate(QDateTime::currentDateTime())
    , m_modifiedDate(QDateTime::currentDateTime())
    , m_currentIndex(-1)
    , m_shuffled(false)
    , m_repeatMode(NoRepeat)
    , m_cachedTotalDuration(0)
    , m_cachedTotalSize(0)
    , m_statisticsCached(false)
{
    connect(this, &Playlist::itemAdded, this, &Playlist::updateModifiedDate);
    connect(this, &Playlist::itemRemoved, this, &Playlist::updateModifiedDate);
    connect(this, &Playlist::itemMoved, this, &Playlist::updateModifiedDate);
    connect(this, &Playlist::playlistCleared, this, &Playlist::updateModifiedDate);
}

Playlist::Playlist(const QString& name, QObject* parent)
    : Playlist(parent)
{
    m_name = name;
}

Playlist::Playlist(const Playlist& other)
    : QObject(other.parent())
{
    copyFrom(other);
}

Playlist& Playlist::operator=(const Playlist& other)
{
    if (this != &other) {
        copyFrom(other);
    }
    return *this;
}

void Playlist::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        updateModifiedDate();
        emit nameChanged(name);
    }
}

qint64 Playlist::totalDuration() const
{
    if (!m_statisticsCached) {
        m_cachedTotalDuration = 0;
        m_cachedTotalSize = 0;
        
        for (const MediaFile& file : m_items) {
            m_cachedTotalDuration += file.duration();
            m_cachedTotalSize += file.fileSize();
        }
        
        m_statisticsCached = true;
    }
    
    return m_cachedTotalDuration;
}

qint64 Playlist::totalSize() const
{
    if (!m_statisticsCached) {
        totalDuration(); // This will calculate both duration and size
    }
    
    return m_cachedTotalSize;
}

MediaFile Playlist::item(int index) const
{
    if (index >= 0 && index < m_items.size()) {
        return m_items[index];
    }
    return MediaFile();
}

int Playlist::indexOf(const MediaFile& file) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i] == file) {
            return i;
        }
    }
    return -1;
}

bool Playlist::contains(const MediaFile& file) const
{
    return indexOf(file) != -1;
}

bool Playlist::containsPath(const QString& filePath) const
{
    for (const MediaFile& file : m_items) {
        if (file.filePath() == filePath) {
            return true;
        }
    }
    return false;
}

void Playlist::addItem(const MediaFile& file)
{
    insertItem(m_items.size(), file);
}

void Playlist::insertItem(int index, const MediaFile& file)
{
    if (index < 0 || index > m_items.size()) {
        index = m_items.size();
    }
    
    m_items.insert(index, file);
    m_statisticsCached = false;
    
    // Update shuffle order if needed
    if (m_shuffled) {
        generateShuffleOrder();
    }
    
    emit itemAdded(index, file);
    emit playlistModified();
}

bool Playlist::removeItem(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    
    MediaFile removedFile = m_items[index];
    m_items.removeAt(index);
    m_statisticsCached = false;
    
    // Adjust current index if needed
    if (m_currentIndex > index) {
        m_currentIndex--;
    } else if (m_currentIndex == index) {
        if (m_currentIndex >= m_items.size()) {
            m_currentIndex = m_items.size() - 1;
        }
        emit currentIndexChanged(m_currentIndex);
    }
    
    // Update shuffle order if needed
    if (m_shuffled) {
        generateShuffleOrder();
    }
    
    emit itemRemoved(index, removedFile);
    emit playlistModified();
    return true;
}

bool Playlist::removeItem(const MediaFile& file)
{
    int index = indexOf(file);
    if (index != -1) {
        return removeItem(index);
    }
    return false;
}

bool Playlist::moveItem(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_items.size() ||
        toIndex < 0 || toIndex >= m_items.size() ||
        fromIndex == toIndex) {
        return false;
    }
    
    MediaFile file = m_items.takeAt(fromIndex);
    m_items.insert(toIndex, file);
    
    // Adjust current index if needed
    if (m_currentIndex == fromIndex) {
        m_currentIndex = toIndex;
        emit currentIndexChanged(m_currentIndex);
    } else if (fromIndex < m_currentIndex && toIndex >= m_currentIndex) {
        m_currentIndex--;
        emit currentIndexChanged(m_currentIndex);
    } else if (fromIndex > m_currentIndex && toIndex <= m_currentIndex) {
        m_currentIndex++;
        emit currentIndexChanged(m_currentIndex);
    }
    
    // Update shuffle order if needed
    if (m_shuffled) {
        generateShuffleOrder();
    }
    
    emit itemMoved(fromIndex, toIndex);
    emit playlistModified();
    return true;
}

void Playlist::clear()
{
    if (!m_items.isEmpty()) {
        m_items.clear();
        m_currentIndex = -1;
        m_shuffleOrder.clear();
        m_statisticsCached = false;
        
        emit currentIndexChanged(m_currentIndex);
        emit playlistCleared();
        emit playlistModified();
    }
}

void Playlist::addItems(const QList<MediaFile>& files)
{
    if (files.isEmpty()) {
        return;
    }
    
    int startIndex = m_items.size();
    m_items.append(files);
    m_statisticsCached = false;
    
    // Update shuffle order if needed
    if (m_shuffled) {
        generateShuffleOrder();
    }
    
    // Emit signals for each added item
    for (int i = 0; i < files.size(); ++i) {
        emit itemAdded(startIndex + i, files[i]);
    }
    
    emit playlistModified();
}

void Playlist::removeItems(const QList<int>& indices)
{
    if (indices.isEmpty()) {
        return;
    }
    
    // Sort indices in descending order to remove from end first
    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
    
    for (int index : sortedIndices) {
        removeItem(index);
    }
}

void Playlist::setCurrentIndex(int index)
{
    if (index < -1 || index >= m_items.size()) {
        index = -1;
    }
    
    if (m_currentIndex != index) {
        m_currentIndex = index;
        emit currentIndexChanged(index);
    }
}

MediaFile Playlist::nextItem() const
{
    if (m_items.isEmpty()) {
        return MediaFile();
    }
    
    int nextIndex = m_currentIndex;
    
    if (m_shuffled) {
        int currentShuffledPos = -1;
        for (int i = 0; i < m_shuffleOrder.size(); ++i) {
            if (m_shuffleOrder[i] == m_currentIndex) {
                currentShuffledPos = i;
                break;
            }
        }
        
        if (currentShuffledPos != -1 && currentShuffledPos < m_shuffleOrder.size() - 1) {
            nextIndex = m_shuffleOrder[currentShuffledPos + 1];
        } else if (m_repeatMode == RepeatAll) {
            nextIndex = m_shuffleOrder.isEmpty() ? 0 : m_shuffleOrder[0];
        } else {
            return MediaFile();
        }
    } else {
        if (m_currentIndex < m_items.size() - 1) {
            nextIndex = m_currentIndex + 1;
        } else if (m_repeatMode == RepeatAll) {
            nextIndex = 0;
        } else {
            return MediaFile();
        }
    }
    
    return item(nextIndex);
}

MediaFile Playlist::previousItem() const
{
    if (m_items.isEmpty()) {
        return MediaFile();
    }
    
    int prevIndex = m_currentIndex;
    
    if (m_shuffled) {
        int currentShuffledPos = -1;
        for (int i = 0; i < m_shuffleOrder.size(); ++i) {
            if (m_shuffleOrder[i] == m_currentIndex) {
                currentShuffledPos = i;
                break;
            }
        }
        
        if (currentShuffledPos > 0) {
            prevIndex = m_shuffleOrder[currentShuffledPos - 1];
        } else if (m_repeatMode == RepeatAll) {
            prevIndex = m_shuffleOrder.isEmpty() ? m_items.size() - 1 : m_shuffleOrder.last();
        } else {
            return MediaFile();
        }
    } else {
        if (m_currentIndex > 0) {
            prevIndex = m_currentIndex - 1;
        } else if (m_repeatMode == RepeatAll) {
            prevIndex = m_items.size() - 1;
        } else {
            return MediaFile();
        }
    }
    
    return item(prevIndex);
}

bool Playlist::hasNext() const
{
    if (m_items.isEmpty()) {
        return false;
    }
    
    if (m_repeatMode == RepeatOne || m_repeatMode == RepeatAll) {
        return true;
    }
    
    if (m_shuffled) {
        int currentShuffledPos = -1;
        for (int i = 0; i < m_shuffleOrder.size(); ++i) {
            if (m_shuffleOrder[i] == m_currentIndex) {
                currentShuffledPos = i;
                break;
            }
        }
        return currentShuffledPos < m_shuffleOrder.size() - 1;
    } else {
        return m_currentIndex < m_items.size() - 1;
    }
}

bool Playlist::hasPrevious() const
{
    if (m_items.isEmpty()) {
        return false;
    }
    
    if (m_repeatMode == RepeatOne || m_repeatMode == RepeatAll) {
        return true;
    }
    
    if (m_shuffled) {
        int currentShuffledPos = -1;
        for (int i = 0; i < m_shuffleOrder.size(); ++i) {
            if (m_shuffleOrder[i] == m_currentIndex) {
                currentShuffledPos = i;
                break;
            }
        }
        return currentShuffledPos > 0;
    } else {
        return m_currentIndex > 0;
    }
}

void Playlist::setShuffle(bool enabled)
{
    if (m_shuffled != enabled) {
        m_shuffled = enabled;
        
        if (enabled) {
            generateShuffleOrder();
        } else {
            m_shuffleOrder.clear();
        }
        
        emit shuffleChanged(enabled);
    }
}

QList<MediaFile> Playlist::search(const QString& query) const
{
    QList<MediaFile> results;
    QString lowerQuery = query.toLower();
    
    for (const MediaFile& file : m_items) {
        if (file.title().toLower().contains(lowerQuery) ||
            file.artist().toLower().contains(lowerQuery) ||
            file.album().toLower().contains(lowerQuery) ||
            file.fileName().toLower().contains(lowerQuery)) {
            results.append(file);
        }
    }
    
    return results;
}

QList<MediaFile> Playlist::filterByArtist(const QString& artist) const
{
    QList<MediaFile> results;
    for (const MediaFile& file : m_items) {
        if (file.artist().compare(artist, Qt::CaseInsensitive) == 0) {
            results.append(file);
        }
    }
    return results;
}

QList<MediaFile> Playlist::filterByAlbum(const QString& album) const
{
    QList<MediaFile> results;
    for (const MediaFile& file : m_items) {
        if (file.album().compare(album, Qt::CaseInsensitive) == 0) {
            results.append(file);
        }
    }
    return results;
}

QList<MediaFile> Playlist::filterByGenre(const QString& genre) const
{
    QList<MediaFile> results;
    for (const MediaFile& file : m_items) {
        if (file.genre().compare(genre, Qt::CaseInsensitive) == 0) {
            results.append(file);
        }
    }
    return results;
}

void Playlist::sort(SortOrder order, Qt::SortOrder direction)
{
    std::sort(m_items.begin(), m_items.end(), [order, direction](const MediaFile& a, const MediaFile& b) {
        bool result = false;
        
        switch (order) {
            case SortByTitle:
                result = a.title().compare(b.title(), Qt::CaseInsensitive) < 0;
                break;
            case SortByArtist:
                result = a.artist().compare(b.artist(), Qt::CaseInsensitive) < 0;
                break;
            case SortByAlbum:
                result = a.album().compare(b.album(), Qt::CaseInsensitive) < 0;
                break;
            case SortByDuration:
                result = a.duration() < b.duration();
                break;
            case SortByDateAdded:
                result = a.dateAdded() < b.dateAdded();
                break;
            case SortByPosition:
            default:
                return false; // No sorting for position
        }
        
        return direction == Qt::AscendingOrder ? result : !result;
    });
    
    // Reset current index and shuffle order after sorting
    m_currentIndex = -1;
    if (m_shuffled) {
        generateShuffleOrder();
    }
    
    emit currentIndexChanged(m_currentIndex);
    emit playlistModified();
}

bool Playlist::isValid() const
{
    return !m_name.isEmpty();
}

QString Playlist::durationString() const
{
    qint64 duration = totalDuration();
    if (duration <= 0) {
        return "00:00";
    }

    qint64 seconds = duration / 1000;
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

QString Playlist::sizeString() const
{
    qint64 size = totalSize();
    if (size <= 0) {
        return "0 B";
    }

    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    double sizeDouble = size;
    int unitIndex = 0;

    while (sizeDouble >= 1024.0 && unitIndex < units.size() - 1) {
        sizeDouble /= 1024.0;
        unitIndex++;
    }

    return QString("%1 %2").arg(sizeDouble, 0, 'f', unitIndex == 0 ? 0 : 1).arg(units[unitIndex]);
}

QString Playlist::itemCountString() const
{
    int count = itemCount();
    return QString("%1 %2").arg(count).arg(count == 1 ? "item" : "items");
}

bool Playlist::save(DatabaseManager* dbManager)
{
    if (!dbManager || !isValid()) {
        return false;
    }
    
    if (m_id == -1) {
        // Create new playlist
        m_id = dbManager->createPlaylist(m_name);
        if (m_id == -1) {
            return false;
        }
    } else {
        // Update existing playlist
        if (!dbManager->updatePlaylist(m_id, m_name)) {
            return false;
        }
        
        // Clear existing items
        dbManager->clearPlaylist(m_id);
    }
    
    // Add all items
    for (int i = 0; i < m_items.size(); ++i) {
        const MediaFile& file = m_items[i];
        if (!dbManager->addToPlaylist(m_id, file.id(), i)) {
            qWarning() << "Failed to add item to playlist:" << file.filePath();
        }
    }
    
    return true;
}

bool Playlist::load(int playlistId, DatabaseManager* dbManager)
{
    if (!dbManager || playlistId <= 0) {
        return false;
    }
    
    // Load playlist metadata
    QSqlQuery playlistQuery = dbManager->getPlaylist(playlistId);
    if (!playlistQuery.next()) {
        return false;
    }
    
    m_id = playlistId;
    m_name = playlistQuery.value("name").toString();
    m_description = playlistQuery.value("description").toString();
    m_createdDate = playlistQuery.value("created_date").toDateTime();
    m_modifiedDate = playlistQuery.value("modified_date").toDateTime();
    
    // Load playlist items
    m_items.clear();
    QSqlQuery itemsQuery = dbManager->getPlaylistItems(playlistId);
    while (itemsQuery.next()) {
        MediaFile file;
        file.setId(itemsQuery.value("media_file_id").toInt());
        file.setFilePath(itemsQuery.value("file_path").toString());
        file.setTitle(itemsQuery.value("title").toString());
        file.setArtist(itemsQuery.value("artist").toString());
        file.setAlbum(itemsQuery.value("album").toString());
        file.setDuration(itemsQuery.value("duration").toLongLong());
        
        m_items.append(file);
    }
    
    m_statisticsCached = false;
    m_currentIndex = -1;
    
    if (m_shuffled) {
        generateShuffleOrder();
    }
    
    return true;
}

bool Playlist::remove(DatabaseManager* dbManager)
{
    if (!dbManager || m_id == -1) {
        return false;
    }
    
    return dbManager->removePlaylist(m_id);
}

QVariantMap Playlist::toVariantMap() const
{
    QVariantMap map;
    map["id"] = m_id;
    map["name"] = m_name;
    map["description"] = m_description;
    map["createdDate"] = m_createdDate;
    map["modifiedDate"] = m_modifiedDate;
    map["currentIndex"] = m_currentIndex;
    map["shuffled"] = m_shuffled;
    map["repeatMode"] = static_cast<int>(m_repeatMode);
    
    QVariantList itemsList;
    for (const MediaFile& file : m_items) {
        itemsList.append(file.toVariantMap());
    }
    map["items"] = itemsList;
    
    return map;
}

void Playlist::fromVariantMap(const QVariantMap& map)
{
    m_id = map.value("id", -1).toInt();
    m_name = map.value("name").toString();
    m_description = map.value("description").toString();
    m_createdDate = map.value("createdDate").toDateTime();
    m_modifiedDate = map.value("modifiedDate").toDateTime();
    m_currentIndex = map.value("currentIndex", -1).toInt();
    m_shuffled = map.value("shuffled", false).toBool();
    m_repeatMode = static_cast<RepeatMode>(map.value("repeatMode", NoRepeat).toInt());
    
    m_items.clear();
    QVariantList itemsList = map.value("items").toList();
    for (const QVariant& itemVar : itemsList) {
        MediaFile file;
        file.fromVariantMap(itemVar.toMap());
        m_items.append(file);
    }
    
    m_statisticsCached = false;
    
    if (m_shuffled) {
        generateShuffleOrder();
    }
}

bool Playlist::exportToM3U(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    stream << "#EXTM3U\n";
    
    for (const MediaFile& mediaFile : m_items) {
        if (mediaFile.fileExists()) {
            stream << "#EXTINF:" << (mediaFile.duration() / 1000) << ","
                   << mediaFile.displayName() << "\n";
            stream << QDir::toNativeSeparators(mediaFile.filePath()) << "\n";
        }
    }
    
    return true;
}

bool Playlist::exportToPLS(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    stream << "[playlist]\n";
    
    int validItemCount = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        const MediaFile& mediaFile = m_items[i];
        if (mediaFile.fileExists()) {
            validItemCount++;
            stream << "File" << validItemCount << "=" 
                   << QDir::toNativeSeparators(mediaFile.filePath()) << "\n";
            stream << "Title" << validItemCount << "=" 
                   << mediaFile.displayName() << "\n";
            stream << "Length" << validItemCount << "=" 
                   << (mediaFile.duration() / 1000) << "\n";
        }
    }
    
    stream << "NumberOfEntries=" << validItemCount << "\n";
    stream << "Version=2\n";
    
    return true;
}

bool Playlist::importFromM3U(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QList<MediaFile> newItems;
    QString line;
    QString currentTitle;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line.startsWith("#EXTINF:")) {
            // Extract title from EXTINF line
            int commaPos = line.indexOf(',');
            if (commaPos != -1) {
                currentTitle = line.mid(commaPos + 1).trimmed();
            }
        } else if (!line.isEmpty() && !line.startsWith("#")) {
            // This should be a file path
            QString mediaPath = QDir::fromNativeSeparators(line);
            
            // Convert relative paths to absolute
            if (QFileInfo(mediaPath).isRelative()) {
                QFileInfo playlistInfo(filePath);
                mediaPath = playlistInfo.absoluteDir().absoluteFilePath(mediaPath);
            }
            
            if (MediaFile::isSupportedFile(mediaPath)) {
                MediaFile file(mediaPath);
                if (!currentTitle.isEmpty()) {
                    file.setTitle(currentTitle);
                    currentTitle.clear();
                }
                newItems.append(file);
            }
        }
    }
    
    if (!newItems.isEmpty()) {
        addItems(newItems);
        return true;
    }
    
    return false;
}

bool Playlist::importFromPLS(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QHash<int, QString> files;
    QHash<int, QString> titles;
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line.startsWith("File")) {
            QRegularExpression fileRegex("^File(\\d+)=(.+)$");
            QRegularExpressionMatch match = fileRegex.match(line);
            if (match.hasMatch()) {
                int index = match.captured(1).toInt();
                QString path = match.captured(2);
                files[index] = QDir::fromNativeSeparators(path);
            }
        } else if (line.startsWith("Title")) {
            QRegularExpression titleRegex("^Title(\\d+)=(.+)$");
            QRegularExpressionMatch match = titleRegex.match(line);
            if (match.hasMatch()) {
                int index = match.captured(1).toInt();
                QString title = match.captured(2);
                titles[index] = title;
            }
        }
    }
    
    QList<MediaFile> newItems;
    QFileInfo playlistInfo(filePath);
    
    for (auto it = files.begin(); it != files.end(); ++it) {
        int index = it.key();
        QString mediaPath = it.value();
        
        // Convert relative paths to absolute
        if (QFileInfo(mediaPath).isRelative()) {
            mediaPath = playlistInfo.absoluteDir().absoluteFilePath(mediaPath);
        }
        
        if (MediaFile::isSupportedFile(mediaPath)) {
            MediaFile file(mediaPath);
            if (titles.contains(index)) {
                file.setTitle(titles[index]);
            }
            newItems.append(file);
        }
    }
    
    if (!newItems.isEmpty()) {
        addItems(newItems);
        return true;
    }
    
    return false;
}

bool Playlist::operator==(const Playlist& other) const
{
    return m_id == other.m_id && m_name == other.m_name;
}

void Playlist::updateModifiedDate()
{
    m_modifiedDate = QDateTime::currentDateTime();
}

void Playlist::generateShuffleOrder()
{
    m_shuffleOrder.clear();
    
    if (m_items.isEmpty()) {
        return;
    }
    
    // Create sequential order
    for (int i = 0; i < m_items.size(); ++i) {
        m_shuffleOrder.append(i);
    }
    
    // Shuffle the order
    for (int i = m_shuffleOrder.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        m_shuffleOrder.swapItemsAt(i, j);
    }
}

int Playlist::getShuffledIndex(int logicalIndex) const
{
    if (!m_shuffled || logicalIndex < 0 || logicalIndex >= m_shuffleOrder.size()) {
        return logicalIndex;
    }
    
    for (int i = 0; i < m_shuffleOrder.size(); ++i) {
        if (m_shuffleOrder[i] == logicalIndex) {
            return i;
        }
    }
    
    return logicalIndex;
}

int Playlist::getLogicalIndex(int shuffledIndex) const
{
    if (!m_shuffled || shuffledIndex < 0 || shuffledIndex >= m_shuffleOrder.size()) {
        return shuffledIndex;
    }
    
    return m_shuffleOrder[shuffledIndex];
}

void Playlist::copyFrom(const Playlist& other)
{
    m_id = other.m_id;
    m_name = other.m_name;
    m_description = other.m_description;
    m_createdDate = other.m_createdDate;
    m_modifiedDate = other.m_modifiedDate;
    m_items = other.m_items;
    m_currentIndex = other.m_currentIndex;
    m_shuffled = other.m_shuffled;
    m_repeatMode = other.m_repeatMode;
    m_shuffleOrder = other.m_shuffleOrder;
    m_cachedTotalDuration = other.m_cachedTotalDuration;
    m_cachedTotalSize = other.m_cachedTotalSize;
    m_statisticsCached = other.m_statisticsCached;
    
    // Reconnect signals
    connect(this, &Playlist::itemAdded, this, &Playlist::updateModifiedDate);
    connect(this, &Playlist::itemRemoved, this, &Playlist::updateModifiedDate);
    connect(this, &Playlist::itemMoved, this, &Playlist::updateModifiedDate);
    connect(this, &Playlist::playlistCleared, this, &Playlist::updateModifiedDate);
}

} // namespace Data
} // namespace EonPlay
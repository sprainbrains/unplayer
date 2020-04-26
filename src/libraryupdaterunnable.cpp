/*
 * Unplayer
 * Copyright (C) 2015-2020 Alexey Rochev <equeim@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libraryupdaterunnable.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QMimeDatabase>
#include <QStringBuilder>
#include <QSqlError>

#include "libraryutils.h"
#include "settings.h"
#include "sqlutils.h"
#include "tagutils.h"
#include "utilsfunctions.h"

namespace unplayer
{
    namespace
    {
        inline QVariant nullIfEmpty(const QString& string)
        {
            if (string.isEmpty()) {
                return {};
            }
            return string;
        }

        bool isNoMediaDirectory(const QString& directory, std::unordered_map<QString, bool>& noMediaDirectories)
        {
            {
                const auto found(noMediaDirectories.find(directory));
                if (found != noMediaDirectories.end()) {
                    return found->second;
                }
            }
            const bool noMedia = QFileInfo(directory % QStringLiteral("/.nomedia")).isFile();
            noMediaDirectories.insert({directory, noMedia});
            return noMedia;
        }

        const QStringList prepareLibraryDirectories(QStringList&& dirs) {
            if (!dirs.isEmpty()) {
                for (QString& dir : dirs) {
                    if (!dir.endsWith(QLatin1Char('/'))) {
                        dir.push_back(QLatin1Char('/'));
                    }
                }
                dirs.removeDuplicates();

                for (int i = dirs.size() - 1; i >= 0; --i) {
                    const QString& dir = dirs[i];
                    const auto found(std::find_if(dirs.begin(), dirs.end(), [&](const QString& d) {
                        return d != dir && dir.startsWith(d);
                    }));
                    if (found != dirs.end()) {
                        dirs.removeAt(i);
                    }
                }
            }
            return std::move(dirs);
        };
    }

    LibraryTracksAdder::LibraryTracksAdder(const QSqlDatabase& db)
        : mDb(db)
    {
        if (mQuery.exec(QLatin1String("SELECT id FROM tracks ORDER BY id DESC LIMIT 1"))) {
            if (mQuery.next()) {
                mLastTrackId = mQuery.value(0).toInt();
            } else {
                mLastTrackId = 0;
            }
        } else {
            qWarning() << __func__ << "failed to get last track id" << mQuery.lastError();
        }

        getArtists();
        getAlbums();
        getGenres();
    }

    void LibraryTracksAdder::addTrackToDatabase(const QString& filePath,
                                                long long modificationTime,
                                                tagutils::Info& info,
                                                const QString& directoryMediaArt,
                                                const QString& embeddedMediaArt)
    {
        mQuery.prepare([&]() {
            QString queryString(QStringLiteral("INSERT INTO tracks (modificationTime, year, trackNumber, duration, filePath, title, discNumber, directoryMediaArt, embeddedMediaArt) "
                                               "VALUES (%1, %2, %3, %4, ?, ?, ?, ?, ?)"));
            queryString = queryString.arg(modificationTime).arg(info.year).arg(info.trackNumber).arg(info.duration);
            return queryString;
        }());

        mQuery.addBindValue(filePath);
        mQuery.addBindValue(info.title);
        mQuery.addBindValue(nullIfEmpty(info.discNumber));
        mQuery.addBindValue(nullIfEmpty(directoryMediaArt));
        mQuery.addBindValue(nullIfEmpty(embeddedMediaArt));

        if (!mQuery.exec()) {
            qWarning() << __func__ << "failed to insert track in the database" << mQuery.lastError();
            return;
        }

        const int trackId = ++mLastTrackId;

        info.artists.append(info.albumArtists);
        info.artists.removeDuplicates();

        for (const QString& artist : info.artists) {
            const int artistId = getArtistId(artist);
            if (artistId != 0) {
                addRelationship(trackId, artistId, "tracks_artists");
            }
        }

        if (!info.albums.isEmpty()) {
            if (info.albumArtists.isEmpty() && !info.artists.isEmpty()) {
                info.albumArtists.push_back(info.artists.front());
            }

            QVector<int> artistIds;
            artistIds.reserve(info.albumArtists.size());
            for (const QString& albumArtist : info.albumArtists) {
                const int artistId = getArtistId(albumArtist);
                if (artistId != 0) {
                    artistIds.push_back(artistId);
                }
            }
            std::sort(artistIds.begin(), artistIds.end());

            for (const QString& album : info.albums) {
                const int albumId = getAlbumId(album, std::move(artistIds));
                if (albumId != 0) {
                    addRelationship(trackId, albumId, "tracks_albums");
                }
            }
        }

        for (const QString& genre : info.genres) {
            const int genreId = getGenreId(genre);
            if (genreId != 0) {
                addRelationship(trackId, genreId, "tracks_genres");
            }
        }
    }

    void LibraryTracksAdder::getArtists()
    {
        getArtistsOrGenres(mArtists);
    }

    void LibraryTracksAdder::getAlbums()
    {
        enum
        {
            IdField,
            TitleField,
            ArtistIdField
        };
        if (mQuery.exec(QLatin1String("SELECT id, title, artistId "
                                      "FROM albums "
                                      "LEFT JOIN albums_artists ON albums_artists.albumId = albums.id"))) {
            QVector<int> artistIds;
            while (mQuery.next()) {
                {
                    const int artistId = mQuery.value(ArtistIdField).toInt();
                    if (artistId != 0) {
                        artistIds.push_back(artistId);
                    }
                }
                const int id = mQuery.value(IdField).toInt();
                if (id != mAlbums.lastId && mAlbums.lastId != 0) {
                    std::sort(artistIds.begin(), artistIds.end());
                    mAlbums.ids.emplace(QPair<QString, QVector<int>>(mQuery.value(TitleField).toString(), artistIds), mAlbums.lastId);
                    artistIds.clear();
                }
                mAlbums.lastId = id;
            }
        } else {
            qWarning() << __func__ << mQuery.lastError();
        }
    }

    void LibraryTracksAdder::getGenres()
    {
        getArtistsOrGenres(mGenres);
    }

    int LibraryTracksAdder::getArtistId(const QString& title)
    {
        return getArtistOrGenreId(title, mArtists);
    }

    int LibraryTracksAdder::getAlbumId(const QString& title, QVector<int>&& artistIds)
    {
        if (title.isEmpty()) {
            return 0;
        }
        const auto& map = mAlbums.ids;
        const auto found(map.find(QPair<QString, QVector<int>>(title, artistIds)));
        if (found == map.end()) {
            return addAlbum(title, std::move(artistIds));
        }
        return found->second;
    }

    int LibraryTracksAdder::getGenreId(const QString& title)
    {
        return getArtistOrGenreId(title, mGenres);
    }

    void LibraryTracksAdder::addRelationship(int firstId, int secondId, const char* table)
    {
        if (!mQuery.prepare(QStringLiteral("INSERT INTO %1 VALUES (%2, %3)").arg(QLatin1String(table)).arg(firstId).arg(secondId))) {
            qWarning() << __func__ << "failed to prepare query" << mQuery.lastError();
        }
        if (!mQuery.exec()) {
            qWarning() << __func__ << "failed to exec query" << mQuery.lastError();
        }
    }

    int LibraryTracksAdder::addAlbum(const QString& title, QVector<int>&& artistIds)
    {
        if (!mQuery.prepare(QStringLiteral("INSERT INTO albums (title) VALUES (?)"))) {
            qWarning() << __func__ << "failed to prepare query" << mQuery.lastError();
            return 0;
        }
        mQuery.addBindValue(title);
        if (!mQuery.exec()) {
            qWarning() << __func__ << "failed to exec query" << mQuery.lastError();
            return 0;
        }

        ++mAlbums.lastId;

        for (int artistId : artistIds) {
            addRelationship(mAlbums.lastId, artistId, "albums_artists");
        }

        mAlbums.ids.emplace(QPair<QString, QVector<int>>(title, std::move(artistIds)), mAlbums.lastId);
        return mAlbums.lastId;
    }

    void LibraryTracksAdder::getArtistsOrGenres(LibraryTracksAdder::ArtistsOrGenres& ids)
    {
        QString queryString(QLatin1String("SELECT id, title FROM "));
        queryString.push_back(ids.table);
        if (mQuery.exec(queryString)) {
            if (reserveFromQuery(ids.ids, mQuery) > 0) {
                while (mQuery.next()) {
                    ids.lastId = mQuery.value(0).toInt();
                    ids.ids.emplace(mQuery.value(1).toString(), ids.lastId);
                }
            }
        } else {
            qWarning() << __func__ << mQuery.lastError();
        }
    }

    int LibraryTracksAdder::getArtistOrGenreId(const QString& title, LibraryTracksAdder::ArtistsOrGenres& ids)
    {
        if (title.isEmpty()) {
            return 0;
        }
        const auto& map = ids.ids;
        const auto found(map.find(title));
        if (found == map.end()) {
            return addArtistOrGenre(title, ids);
        }
        return found->second;
    }

    int LibraryTracksAdder::addArtistOrGenre(const QString& title, LibraryTracksAdder::ArtistsOrGenres& ids)
    {
        if (!mQuery.prepare(QStringLiteral("INSERT INTO %1 (title) VALUES (?)").arg(ids.table))) {
            qWarning() << __func__ << "failed to prepare query" << mQuery.lastError();
            return 0;
        }
        mQuery.addBindValue(title);
        if (!mQuery.exec()) {
            qWarning() << __func__ << "failed to exec query" << mQuery.lastError();
            return 0;
        }
        ++ids.lastId;
        ids.ids.emplace(title, ids.lastId);
        return ids.lastId;
    }

    const QLatin1String LibraryUpdateRunnable::databaseConnectionName("unplayer_update");

    LibraryUpdateRunnable::LibraryUpdateRunnable(const QString& mediaArtDirectory)
        : mMediaArtDirectory(mediaArtDirectory),
          mCancel(false)
    {

    }

    LibraryUpdateRunnableNotifier* LibraryUpdateRunnable::notifier()
    {
        return &mNotifier;
    }

    void LibraryUpdateRunnable::cancel()
    {
        qInfo("Cancel updating database");
        mCancel = true;
    }

    void LibraryUpdateRunnable::run()
    {
        const struct FinishedGuard
        {
            ~FinishedGuard() {
                emit notifier.finished();
            }
            LibraryUpdateRunnableNotifier& notifier;
        } finishedGuard{mNotifier};

        if (mCancel) {
            return;
        }

        qInfo("Start updating database");
        QElapsedTimer timer;
        timer.start();
        QElapsedTimer stageTimer;
        stageTimer.start();

        // Open database
        mDb = LibraryUtils::openDatabase(databaseGuard.connectionName);
        if (!mDb.isOpen()) {
            return;
        }
        mQuery = QSqlQuery(mDb);

        const TransactionGuard transactionGuard(mDb);

        // Create media art directory
        if (!QDir().mkpath(mMediaArtDirectory)) {
            qWarning() << "failed to create media art directory:" << mMediaArtDirectory;
        }

        {
            std::unordered_map<QByteArray, QString> embeddedMediaArtFiles(LibraryUtils::instance()->getEmbeddedMediaArt());
            std::vector<TrackToAdd> tracksToAdd;

            {
                // Library directories
                mLibraryDirectories = prepareLibraryDirectories(Settings::instance()->libraryDirectories());
                mBlacklistedDirectories = prepareLibraryDirectories(Settings::instance()->blacklistedDirectories());

                std::vector<int> tracksToRemove;
                {
                    std::unordered_map<QString, bool> noMediaDirectories;
                    TracksInDbResult trackInDbResult(getTracksFromDatabase(tracksToRemove, noMediaDirectories));
                    auto& tracksInDb = trackInDbResult.tracksInDb;

                    if (mCancel) {
                        return;
                    }

                    qInfo("Tracks in database: %zd (took %.3f s)", tracksInDb.size(), static_cast<double>(stageTimer.restart()) / 1000.0);
                    qInfo("Tracks to remove: %zd", tracksToRemove.size());

                    qInfo("Start scanning filesystem");
                    emit mNotifier.stageChanged(LibraryUpdateRunnableNotifier::ScanningStage);

                    tracksToAdd = scanFilesystem(trackInDbResult,
                                                 tracksToRemove,
                                                 noMediaDirectories,
                                                 embeddedMediaArtFiles);

                    if (mCancel) {
                        return;
                    }

                    qInfo("End scanning filesystem (took %.3f s), need to extract tags from %zu files", static_cast<double>(stageTimer.restart()) / 1000.0, tracksToAdd.size());
                }

                if (!tracksToRemove.empty()) {
                    if (LibraryUtils::removeTracksFromDbByIds(tracksToRemove, mDb, mCancel)) {
                        qInfo("Removed %zu tracks from database (took %.3f s)", tracksToRemove.size(), static_cast<double>(stageTimer.restart()) / 1000.0);
                    }
                }
            }

            if (mCancel) {
                return;
            }

            if (!tracksToAdd.empty()) {
                qInfo("Start extracting tags from files");
                emit mNotifier.stageChanged(LibraryUpdateRunnableNotifier::ExtractingStage);
                const int count = addTracks(tracksToAdd, embeddedMediaArtFiles);
                qInfo("Added %d tracks to database (took %.3f s)", count, static_cast<double>(stageTimer.restart()) / 1000.0);
            }
        }

        if (mCancel) {
            return;
        }

        emit mNotifier.stageChanged(LibraryUpdateRunnableNotifier::FinishingStage);

        LibraryUtils::removeUnusedCategories(mDb);
        LibraryUtils::removeUnusedMediaArt(mDb, mMediaArtDirectory, mCancel);

        qInfo("End updating database (last stage took %.3f s)", static_cast<double>(stageTimer.elapsed()) / 1000.0);
        qInfo("Total time: %.3f s", static_cast<double>(timer.elapsed()) / 1000.0);
    }

    LibraryUpdateRunnable::TracksInDbResult LibraryUpdateRunnable::getTracksFromDatabase(std::vector<int>& tracksToRemove, std::unordered_map<QString, bool>& noMediaDirectories)
    {
        std::unordered_map<QString, TrackInDb> tracksInDb;
        std::unordered_map<QString, QString> mediaArtDirectoriesInDbHash;

        // Extract tracks from database

        std::unordered_map<QString, bool> embeddedMediaArtExistanceHash;
        const auto embeddedMediaArtExistanceHashEnd(embeddedMediaArtExistanceHash.end());
        const auto checkExistance = [&](QString&& mediaArt) {
            if (mediaArt.isEmpty()) {
                return true;
            }

            const auto found(embeddedMediaArtExistanceHash.find(mediaArt));
            if (found == embeddedMediaArtExistanceHashEnd) {
                const QFileInfo fileInfo(mediaArt);
                if (fileInfo.isFile() && fileInfo.isReadable()) {
                    embeddedMediaArtExistanceHash.insert({std::move(mediaArt), true});
                    return true;
                }
                embeddedMediaArtExistanceHash.insert({std::move(mediaArt), false});
                return false;
            }

            return found->second;
        };

        if (!mQuery.exec(QLatin1String("SELECT id, filePath, modificationTime, directoryMediaArt, embeddedMediaArt FROM tracks ORDER BY id"))) {
            qWarning() << "failed to get files from database" << mQuery.lastError();
            mCancel = true;
            return {};
        }

        while (mQuery.next()) {
            if (mCancel) {
                return {};
            }

            const int id(mQuery.value(0).toInt());
            QString filePath(mQuery.value(1).toString());
            const QFileInfo fileInfo(filePath);
            QString directory(fileInfo.path());

            bool remove = false;
            if (!fileInfo.exists() || fileInfo.isDir() || !fileInfo.isReadable()) {
                remove = true;
            } else {
                remove = true;
                for (const QString& dir : mLibraryDirectories) {
                    if (filePath.startsWith(dir)) {
                        remove = false;
                        break;
                    }
                }
                if (!remove) {
                    remove = isBlacklisted(filePath);
                }
                if (!remove) {
                    remove = isNoMediaDirectory(directory, noMediaDirectories);
                }
            }

            if (remove) {
                tracksToRemove.push_back(id);
            } else {
                tracksInDb.emplace(std::move(filePath), TrackInDb{id,
                                                                  !checkExistance(mQuery.value(4).toString()),
                                                                  mQuery.value(2).toLongLong()});
                mediaArtDirectoriesInDbHash.insert({std::move(directory), mQuery.value(3).toString()});
            }
        }

        return {std::move(tracksInDb), std::move(mediaArtDirectoriesInDbHash)};
    }

    std::vector<LibraryUpdateRunnable::TrackToAdd> LibraryUpdateRunnable::scanFilesystem(LibraryUpdateRunnable::TracksInDbResult& tracksInDbResult,
                                                                                         std::vector<int>& tracksToRemove,
                                                                                         std::unordered_map<QString, bool>& noMediaDirectories, std::unordered_map<QByteArray, QString>& embeddedMediaArtFiles)
    {
        std::vector<TrackToAdd> tracksToAdd;

        auto& tracksInDb = tracksInDbResult.tracksInDb;
        const auto tracksInDbEnd(tracksInDb.end());
        auto& mediaArtDirectoriesInDbHash = tracksInDbResult.mediaArtDirectoriesInDbHash;
        const auto mediaArtDirectoriesInDbHashEnd(mediaArtDirectoriesInDbHash.end());

        std::unordered_map<QString, QString> mediaArtDirectoriesHash;

        QString directory;
        QString directoryMediaArt;

        for (const QString& topLevelDirectory : mLibraryDirectories) {
            if (mCancel) {
                return tracksToAdd;
            }

            QDirIterator iterator(topLevelDirectory, QDir::Files | QDir::Readable, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
            while (iterator.hasNext()) {
                if (mCancel) {
                    return tracksToAdd;
                }

                QString filePath(iterator.next());
                const QFileInfo fileInfo(iterator.fileInfo());

                const fileutils::Extension extension = fileutils::extensionFromSuffix(fileInfo.suffix());
                if (extension == fileutils::Extension::Other) {
                    continue;
                }

                if (fileInfo.path() != directory) {
                    directory = fileInfo.path();
                    directoryMediaArt = LibraryUtils::findMediaArtForDirectory(mediaArtDirectoriesHash, directory, mCancel);

                    const auto directoryMediaArtInDb(mediaArtDirectoriesInDbHash.find(directory));
                    if (directoryMediaArtInDb != mediaArtDirectoriesInDbHashEnd) {
                        if (directoryMediaArtInDb->second != directoryMediaArt) {
                            mQuery.prepare(QStringLiteral("UPDATE tracks SET directoryMediaArt = ? WHERE instr(filePath, ?) = 1"));
                            mQuery.addBindValue(nullIfEmpty(directoryMediaArt));
                            mQuery.addBindValue(QString(directory % QLatin1Char('/')));
                            if (!mQuery.exec()) {
                                qWarning() << mQuery.lastError();
                            }
                        }
                        mediaArtDirectoriesInDbHash.erase(directoryMediaArtInDb);
                    }
                }

                const auto foundInDb(tracksInDb.find(filePath));
                if (foundInDb == tracksInDbEnd) {
                    // File is not in database

                    if (isNoMediaDirectory(fileInfo.path(), noMediaDirectories)) {
                        continue;
                    }

                    if (isBlacklisted(filePath)) {
                        continue;
                    }

                    tracksToAdd.push_back({std::move(filePath), directoryMediaArt, extension});
                    emit mNotifier.foundFilesChanged(static_cast<int>(tracksToAdd.size()));
                } else {
                    // File is in database

                    const TrackInDb& file = foundInDb->second;

                    const long long modificationTime = getLastModifiedTime(filePath);
                    if (modificationTime == file.modificationTime) {
                        // File has not changed
                        if (file.embeddedMediaArtDeleted) {
                            const QString embeddedMediaArt(LibraryUtils::instance()->saveEmbeddedMediaArt(tagutils::getTrackInfo(filePath, extension, mMimeDb).mediaArtData,
                                                                                                          embeddedMediaArtFiles,
                                                                                                          mMimeDb));
                            mQuery.prepare(QStringLiteral("UPDATE tracks SET embeddedMediaArt = ? WHERE id = ?"));
                            mQuery.addBindValue(nullIfEmpty(embeddedMediaArt));
                            mQuery.addBindValue(file.id);
                            if (!mQuery.exec()) {
                                qWarning() << mQuery.lastError();
                            }
                        }
                    } else {
                        // File has changed
                        tracksToRemove.push_back(file.id);
                        tracksToAdd.push_back({foundInDb->first, directoryMediaArt, extension});
                        emit mNotifier.foundFilesChanged(static_cast<int>(tracksToAdd.size()));
                    }
                }
            }
        }

        return tracksToAdd;
    }

    int LibraryUpdateRunnable::addTracks(const std::vector<LibraryUpdateRunnable::TrackToAdd>& tracksToAdd,
                                         std::unordered_map<QByteArray, QString>& embeddedMediaArtFiles)
    {
        int count = 0;

        LibraryTracksAdder adder(mDb);
        for (const TrackToAdd& track : tracksToAdd) {
            if (mCancel) {
                return count;
            }

            tagutils::Info trackInfo(tagutils::getTrackInfo(track.filePath, track.extension, mMimeDb));
            if (trackInfo.fileTypeValid) {
                ++count;

                if (trackInfo.title.isEmpty()) {
                    trackInfo.title = QFileInfo(track.filePath).fileName();
                }

                adder.addTrackToDatabase(track.filePath,
                                         getLastModifiedTime(track.filePath),
                                         trackInfo,
                                         track.directoryMediaArt,
                                         LibraryUtils::instance()->saveEmbeddedMediaArt(trackInfo.mediaArtData,
                                                                                        embeddedMediaArtFiles,
                                                                                        mMimeDb));
                emit mNotifier.extractedFilesChanged(count);
            }
        }

        return count;
    }

    bool LibraryUpdateRunnable::isBlacklisted(const QString& path)
    {
        for (const QString& directory : mBlacklistedDirectories) {
            if (path.startsWith(directory)) {
                return true;
            }
        }
        return false;
    }


}
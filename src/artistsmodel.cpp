/*
 * Unplayer
 * Copyright (C) 2015-2019 Alexey Rochev <equeim@gmail.com>
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

#include "artistsmodel.h"

#include <functional>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFutureWatcher>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrentRun>

#include "libraryutils.h"
#include "modelutils.h"
#include "settings.h"

#include "abstractlibrarymodel.cpp"

namespace unplayer
{
    namespace
    {
        enum Field
        {
            ArtistIdField,
            ArtistField,
            AlbumsCountField,
            TracksCountField,
            DurationField
        };
    }

    ArtistsModel::ArtistsModel()
        : mSortDescending(Settings::instance()->artistsSortDescending())
    {
        execQuery();
    }

    QVariant ArtistsModel::data(const QModelIndex& index, int role) const
    {
        const Artist& artist = mArtists[static_cast<size_t>(index.row())];

        switch (role) {
        case ArtistIdRole:
            return artist.id;
        case ArtistRole:
            return artist.artist;
        case DisplayedArtistRole:
            return artist.displayedArtist;
        case AlbumsCountRole:
            return artist.albumsCount;
        case TracksCountRole:
            return artist.tracksCount;
        case DurationRole:
            return artist.duration;
        }

        return QVariant();
    }

    bool ArtistsModel::sortDescending() const
    {
        return mSortDescending;
    }

    void ArtistsModel::toggleSortOrder()
    {
        mSortDescending = !mSortDescending;
        Settings::instance()->setArtistsSortDescending(mSortDescending);
        emit sortDescendingChanged();
        execQuery();
    }

    std::vector<LibraryTrack> ArtistsModel::getTracksForArtist(int index) const
    {
        enum {
            FilePathField,
            TitleField,
            DurationField,
            DirectoryMediaArtField,
            EmbeddedMediaArtField,
            AlbumField,
            UserMediaArtField
        };
        QString queryString(QLatin1String("SELECT filePath, tracks.title, duration, directoryMediaArt, embeddedMediaArt, albums.title, albums.userMediaArt "
                                          "FROM tracks "
                                          "LEFT JOIN tracks_artists ON tracks.id = tracks_artists.trackId "
                                          "LEFT JOIN artists ON artists.id = tracks_artists.artistId "
                                          "LEFT JOIN tracks_albums ON tracks.id = tracks_albums.trackId "
                                          "LEFT JOIN albums ON albums.id = tracks_albums.albumId "));
        const Artist& artist = mArtists[static_cast<size_t>(index)];
        if (artist.id == 0) {
            queryString += QLatin1String("WHERE artists.id IS NULL ");
        } else {
            queryString += QString::fromLatin1("WHERE artists.id = %1 ").arg(artist.id);
        }
        queryString += QLatin1String("ORDER BY albums.id IS NULL, year, albums.title, trackNumber, tracks.title");
        QSqlQuery query;
        if (query.exec(queryString)) {
            if (query.last()) {
                std::vector<LibraryTrack> tracks;
                tracks.reserve(static_cast<size_t>(query.at() + 1));
                query.seek(QSql::BeforeFirstRow);
                while (query.next()) {
                    tracks.push_back({query.value(FilePathField).toString(),
                                      query.value(TitleField).toString(),
                                      artist.artist,
                                      query.value(AlbumField).toString(),
                                      query.value(DurationField).toInt(),
                                      mediaArtFromQuery(query,
                                                        DirectoryMediaArtField,
                                                        EmbeddedMediaArtField,
                                                        UserMediaArtField)});
                }
                return tracks;
            }
        }

        qWarning() << __func__ << "failed to get tracks from database" << query.lastError();
        return {};
    }

    std::vector<LibraryTrack> ArtistsModel::getTracksForArtists(const std::vector<int>& indexes) const
    {
        std::vector<LibraryTrack> tracks;
        QSqlDatabase::database().transaction();
        for (int index : indexes) {
            std::vector<LibraryTrack> artistTracks(getTracksForArtist(index));
            tracks.insert(tracks.end(), std::make_move_iterator(artistTracks.begin()), std::make_move_iterator(artistTracks.end()));
        }
        QSqlDatabase::database().commit();
        return tracks;
    }

    QStringList ArtistsModel::getTrackPathsForArtist(int index) const
    {
        QString queryString(QLatin1String("SELECT filePath "
                                          "FROM tracks "
                                          "LEFT JOIN tracks_artists ON tracks.id = tracks_artists.trackId "
                                          "LEFT JOIN artists ON artists.id = tracks_artists.artistId "
                                          "LEFT JOIN tracks_albums ON tracks.id = tracks_albums.trackId "
                                          "LEFT JOIN albums ON albums.id = tracks_albums.albumId "));
        const Artist& artist = mArtists[static_cast<size_t>(index)];
        if (artist.id == 0) {
            queryString += QLatin1String("WHERE artists.id IS NULL ");
        } else {
            queryString += QString::fromLatin1("WHERE artists.id = %1 ").arg(artist.id);
        }
        queryString += QLatin1String("ORDER BY albums.id IS NULL, year, albums.title, trackNumber, tracks.title");
        QSqlQuery query;
        if (query.exec(queryString)) {
            if (query.last()) {
                QStringList tracks;
                tracks.reserve(query.at() + 1);
                query.seek(QSql::BeforeFirstRow);
                while (query.next()) {
                    tracks.push_back(query.value(0).toString());
                }
                return tracks;
            }
        }
        qWarning() << __func__ << "failed to get tracks from database" << query.lastError();
        return {};
    }

    QStringList ArtistsModel::getTrackPathsForArtists(const std::vector<int>& indexes) const
    {
        QStringList tracks;
        QSqlDatabase::database().transaction();
        for (int index : indexes) {
            tracks.append(getTrackPathsForArtist(index));
        }
        QSqlDatabase::database().commit();
        return tracks;
    }

    void ArtistsModel::removeArtist(int index, bool deleteFiles)
    {
        removeArtists({index}, deleteFiles);
    }

    void ArtistsModel::removeArtists(const std::vector<int>& indexes, bool deleteFiles)
    {
        if (LibraryUtils::instance()->isRemovingFiles()) {
            return;
        }

        std::vector<QString> artists;
        artists.reserve(indexes.size());
        for (int index : indexes) {
            artists.push_back(mArtists[static_cast<size_t>(index)].artist);
        }
        LibraryUtils::instance()->removeArtists(std::move(artists), deleteFiles);
        QObject::connect(LibraryUtils::instance(), &LibraryUtils::removingFilesChanged, this, [this, indexes] {
            if (!LibraryUtils::instance()->isRemovingFiles()) {
                ModelBatchRemover::removeIndexes(this, indexes);
                QObject::disconnect(LibraryUtils::instance(), &LibraryUtils::removingFilesChanged, this, nullptr);
            }
        });
    }

    QHash<int, QByteArray> ArtistsModel::roleNames() const
    {
        return {{ArtistIdRole, "artistId"},
                {ArtistRole, "artist"},
                {DisplayedArtistRole, "displayedArtist"},
                {AlbumsCountRole, "albumsCount"},
                {TracksCountRole, "tracksCount"},
                {DurationRole, "duration"}};
    }

    QString ArtistsModel::makeQueryString(std::vector<QVariant>&) const
    {
        const QLatin1String artistType(Settings::instance()->useAlbumArtist() ? "albumArtist" : "artist");

        return QString::fromLatin1("SELECT artists.id, artists.title, COUNT(DISTINCT CASE WHEN albums.id IS NULL THEN 0 ELSE albums.id END), COUNT(tracks.id), SUM(duration) "
                                   "FROM tracks "
                                   "LEFT JOIN tracks_artists ON tracks_artists.trackId = tracks.id "
                                   "LEFT JOIN artists ON artists.id = tracks_artists.artistId "
                                   "LEFT JOIN tracks_albums ON tracks_albums.trackId = tracks.id "
                                   "LEFT JOIN albums ON albums.id = tracks_albums.albumId "
                                   "GROUP BY artists.id "
                                   "ORDER BY artists.id IS NULL %1, artists.title %1").arg(mSortDescending ? QLatin1String("DESC")
                                                                                                           : QLatin1String("ASC"));
    }

    Artist ArtistsModel::itemFromQuery(const QSqlQuery& query)
    {
        const QString artist(query.value(ArtistField).toString());
        return {query.value(ArtistIdField).toInt(),
                artist,
                artist.isEmpty() ? qApp->translate("unplayer", "Unknown artist") : artist,
                query.value(AlbumsCountField).toInt(),
                query.value(TracksCountField).toInt(),
                query.value(DurationField).toInt()};
    }
}

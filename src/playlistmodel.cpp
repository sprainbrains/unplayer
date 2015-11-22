/*
 * Unplayer
 * Copyright (C) 2015 Alexey Rochev <equeim@gmail.com>
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

#include "playlistmodel.h"
#include "playlistmodel.moc"

#include <QUrl>

#include <QSparqlConnection>
#include <QSparqlQuery>
#include <QSparqlQueryOptions>
#include <QSparqlResult>

#include "playlistutils.h"

namespace Unplayer
{

struct PlaylistTrack
{
    PlaylistTrack(const QString &url)
        : url(url),
          unknownArtist(false),
          unknownAlbum(false)
    {

    }

    QString title;
    QString url;
    qint64 duration;
    QString artist;
    bool unknownArtist;
    QString album;
    bool unknownAlbum;
};

enum PlaylistModelRole
{
    TitleRole = Qt::UserRole,
    UrlRole,
    DurationRole,
    ArtistRole,
    AlbumRole
};

PlaylistModel::PlaylistModel()
    : m_rowCount(0),
      m_loadedTracks(0),
      m_loaded(false)
{

}

PlaylistModel::~PlaylistModel()
{
    qDeleteAll(m_queries);
    qDeleteAll(m_uniqueTracksHash);
}

void PlaylistModel::classBegin()
{

}

void PlaylistModel::componentComplete()
{
    QStringList tracksList = PlaylistUtils::parsePlaylist(m_url);

    if (tracksList.isEmpty()) {
        m_loaded = true;
        emit loadedChanged();
    } else {
        QSparqlConnection *connection = new QSparqlConnection("QTRACKER_DIRECT", QSparqlConnectionOptions(), this);

        QStringList::const_iterator iterator = tracksList.cbegin();
        while (iterator != tracksList.cend()) {
            QString url = QUrl(*iterator).toEncoded();
            if (m_uniqueTracksHash.contains(url)) {
                m_tracks.append(m_uniqueTracksHash.value(url));
            } else {
                PlaylistTrack *track = new PlaylistTrack(url);
                m_uniqueTracksHash.insert(url, track);
                m_tracks.append(track);

                QSparqlResult *result = connection->exec(QSparqlQuery(QString("SELECT tracker:coalesce(nie:title(?track), nfo:fileName(?track)) AS ?title\n"
                                                                              "       nie:url(?track) AS ?url\n"
                                                                              "       nfo:duration(?track) AS ?duration\n"
                                                                              "       nmm:artistName(nmm:performer(?track)) AS ?artist\n"
                                                                              "       nie:title(nmm:musicAlbum(?track)) AS ?album\n"
                                                                              "WHERE {\n"
                                                                              "    ?track nie:url \"%1\".\n"
                                                                              "}").arg(url),
                                                                      QSparqlQuery::SelectStatement));

                connect(result, &QSparqlResult::finished, this, &PlaylistModel::onQueryFinished);
                m_queries.append(result);
            }

            iterator++;
        }
    }
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const PlaylistTrack *track = m_tracks.at(index.row());

    switch (role) {
    case TitleRole:
        return track->title;
    case UrlRole:
        return track->url;
    case DurationRole:
        return track->duration;
    case ArtistRole:
        return track->artist;
    case AlbumRole:
        return track->album;
    default:
        return QVariant();
    }
}

int PlaylistModel::rowCount(const QModelIndex&) const
{
    return m_rowCount;
}

QString PlaylistModel::url() const
{
    return m_url;
}

void PlaylistModel::setUrl(const QString &newUrl)
{
    m_url = newUrl;
}

bool PlaylistModel::isLoaded() const
{
    return m_loaded;
}

QVariantMap PlaylistModel::get(int trackIndex) const
{
    QVariantMap map;

    const PlaylistTrack *track = m_tracks.at(trackIndex);

    map.insert("title", track->title);
    map.insert("url", track->url);
    map.insert("duration", track->duration);

    map.insert("artist", track->artist);
    if (track->unknownArtist)
        map.insert("rawArtist", QVariant());
    else
        map.insert("rawArtist", track->artist);

    map.insert("album", track->album);
    if (track->unknownAlbum)
        map.insert("rawAlbum", QVariant());
    else
        map.insert("rawAlbum", track->album);

    return map;
}

void PlaylistModel::removeAt(int trackIndex)
{
    beginRemoveRows(QModelIndex(), trackIndex, trackIndex);
    PlaylistTrack *track = m_tracks.takeAt(trackIndex);
    m_uniqueTracksHash.remove(m_uniqueTracksHash.key(track));
    delete track;
    m_rowCount = m_tracks.size();
    endRemoveRows();
}

void PlaylistModel::clear()
{
    beginResetModel();
    qDeleteAll(m_uniqueTracksHash);
    m_uniqueTracksHash.clear();
    m_tracks.clear();
    m_rowCount = 0;
    endResetModel();
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles.insert(TitleRole, "title");
    roles.insert(UrlRole, "url");
    roles.insert(DurationRole, "duration");
    roles.insert(ArtistRole, "artist");
    roles.insert(AlbumRole, "album");

    return roles;
}

void PlaylistModel::onQueryFinished()
{
    QSparqlResult *result = static_cast<QSparqlResult*>(sender());

    if (result->size() > 0) {
        result->next();
        QSparqlResultRow row = result->current();

        PlaylistTrack *track = m_uniqueTracksHash.value(row.value("url").toString());

        track->title = row.value("title").toString();
        track->duration = row.value("duration").toLongLong();

        QVariant artist = row.value("artist");
        if (artist.isValid()) {
            track->artist = artist.toString();
        } else {
            track->artist = tr("Unknown artist");
            track->unknownArtist = true;
        }

        QVariant album = row.value("album");
        if (album.isValid()) {
            track->album = album.toString();
        } else {
            track->album = tr("Unknown album");
            track->unknownAlbum = true;
        }
    }

    m_loadedTracks++;

    if (m_loadedTracks == m_uniqueTracksHash.size()) {
        m_loaded = true;
        emit loadedChanged();

        beginResetModel();
        m_rowCount = m_tracks.size();
        endResetModel();

        qDeleteAll(m_queries);
        m_queries.clear();
    }
}

}

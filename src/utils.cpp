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

#include "utils.h"
#include "utils.moc"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Unplayer
{

const QString Utils::m_mediaArtDirectoryPath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/media-art";

Utils::Utils()
{
    qsrand(QDateTime::currentMSecsSinceEpoch());
}

QString Utils::mediaArt(const QString &artistName, const QString &albumTitle)
{
    if (artistName.isEmpty() || albumTitle.isEmpty())
        return QString();

    QString filePath = m_mediaArtDirectoryPath +
            QDir::separator() +
            "album-" +
            mediaArtMd5(artistName) +
            "-" +
            mediaArtMd5(albumTitle) +
            ".jpeg";

    if (QFile(filePath).exists())
        return filePath;

    return QString();
}

QString Utils::mediaArtForArtist(const QString &artistName)
{
    QDir mediaArtDirectory(m_mediaArtDirectoryPath);
    QStringList nameFilters;
    nameFilters.append("album-" +
                       mediaArtMd5(artistName) +
                       "-*.jpeg");

    QStringList mediaArtList = mediaArtDirectory.entryList(nameFilters);

    if (mediaArtList.length() == 0)
        return QString();

    static int random = qrand();
    return mediaArtDirectory.filePath(mediaArtList.at(random % mediaArtList.length()));
}

QString Utils::randomMediaArt()
{
    QDir mediaArtDirectory(m_mediaArtDirectoryPath);
    QStringList mediaArtList = mediaArtDirectory.entryList(QDir::Files);

    if (mediaArtList.length() == 0)
        return QString();

    return mediaArtDirectory.filePath(mediaArtList.at(qrand() % mediaArtList.length()));
}

QString Utils::formatDuration(uint seconds)
{
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;
    seconds %= 60;

    QString etaString;

    if (hours > 0)
        etaString +=  tr("%1 h ").arg(hours);

    if (minutes > 0)
        etaString +=  tr("%1 m ").arg(minutes);

    if (hours == 0 &&
            (seconds > 0 ||
             minutes == 0))
        etaString +=  tr("%1 s").arg(seconds);

    return etaString;
}

QString Utils::escapeRegExp(const QString &string)
{
    return QRegularExpression::escape(string);
}

QString Utils::escapeSparql(QString string)
{
    return string.
            replace("\t", "\\t").
            replace("\n", "\\n").
            replace("\r", "\\r").
            replace("\b", "\\b").
            replace("\f", "\\f").
            replace("\"", "\\\"").
            replace("'", "\\'").
            replace("\\", "\\\\");
}

QString Utils::tracksSparqlQuery(bool allArtists,
                                 bool allAlbums,
                                 const QString &artist,
                                 bool unknownArtist,
                                 const QString &album,
                                 bool unknownAlbum)
{
    QString query =
            "SELECT ?title ?url ?duration ?artist ?rawArtist ?album ?rawAlbum\n"
            "WHERE {\n"
            "    {\n"
            "        SELECT tracker:coalesce(nie:title(?track), nfo:fileName(?track)) AS ?title\n"
            "               nie:url(?track) AS ?url\n"
            "               nfo:duration(?track) AS ?duration\n"
            "               nmm:trackNumber(?track) AS ?trackNumber\n"
            "               tracker:coalesce(nmm:artistName(nmm:performer(?track)), \"" + tr("Unknown artist") + "\") AS ?artist\n"
            "               nmm:artistName(nmm:performer(?track)) AS ?rawArtist\n"
            "               tracker:coalesce(nie:title(nmm:musicAlbum(?track)), \"" + tr("Unknown album") + "\") AS ?album\n"
            "               nie:title(nmm:musicAlbum(?track)) AS ?rawAlbum\n"
            "               nie:informationElementDate(?track) AS ?year\n"
            "        WHERE {\n"
            "            ?track a nmm:MusicPiece.\n"
            "        }\n"
            "        ORDER BY !bound(?rawArtist) ?rawArtist !bound(?rawAlbum) ?year ?rawAlbum ?trackNumber ?title\n"
            "    }.\n";

    if (!allArtists) {
        if (unknownArtist)
            query += "    FILTER(!bound(?rawArtist)).\n";
        else
            query += "    FILTER(?rawArtist = \"" + artist + "\").\n";
    }

    if (!allAlbums) {
        if (unknownAlbum)
            query += "    FILTER(!bound(?rawAlbum)).\n";
        else
            query += "    FILTER(?rawAlbum = \"" + album + "\").\n";
    }

    query += "}";

    return query;
}

QString Utils::mediaArtMd5(QString string)
{
    string = string.
            replace(QRegularExpression("\\([^\\)]*\\)"), QString()).
            replace(QRegularExpression("\\{[^\\}]*\\}"), QString()).
            replace(QRegularExpression("\\[[^\\]]*\\]"), QString()).
            replace(QRegularExpression("<[^>]*>"), QString()).
            replace(QRegularExpression("[\\(\\)_\\{\\}\\[\\]!@#\\$\\^&\\*\\+=|/\\\'\"?<>~`]"), QString()).
            trimmed().
            replace("\t", " ").
            replace(QRegularExpression("  +"), " ").
            normalized(QString::NormalizationForm_KD).
            toLower();

    if (string.isEmpty())
        string = " ";

    return QCryptographicHash::hash(string.toUtf8(), QCryptographicHash::Md5).toHex();
}

}

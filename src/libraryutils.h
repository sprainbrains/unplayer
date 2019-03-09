/*
 * Unplayer
 * Copyright (C) 2015-2018 Alexey Rochev <equeim@gmail.com>
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

#ifndef UNPLAYER_LIBRARYUTILS_H
#define UNPLAYER_LIBRARYUTILS_H

#include <vector>

#include <QMimeDatabase>
#include <QObject>

#include <unordered_map>
#include <unordered_set>

class QFileInfo;
class QSqlDatabase;
class QSqlQuery;

namespace unplayer
{
    namespace tagutils
    {
        struct Info;
    }

    enum class Extension
    {
        FLAC,
        AAC,

        M4A,
        MP3,

        OGA,
        OGG,
        OPUS,

        APE,
        MKA,

        WAV,
        WAVPACK,

        Other
    };

    Extension extensionFromSuffux(const QString& suffix);

    QString mediaArtFromQuery(const QSqlQuery& query, int directoryMediaArtField, int embeddedMediaArtField);

    class LibraryUtils final : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(bool databaseInitialized READ isDatabaseInitialized CONSTANT)
        Q_PROPERTY(bool createdTable READ isCreatedTable CONSTANT)
        Q_PROPERTY(bool updating READ isUpdating NOTIFY updatingChanged)
        Q_PROPERTY(int artistsCount READ artistsCount NOTIFY databaseChanged)
        Q_PROPERTY(int albumsCount READ albumsCount NOTIFY databaseChanged)
        Q_PROPERTY(int tracksCount READ tracksCount NOTIFY databaseChanged)
        Q_PROPERTY(int tracksDuration READ tracksDuration NOTIFY databaseChanged)
        Q_PROPERTY(QString randomMediaArt READ randomMediaArt NOTIFY mediaArtChanged)
    public:
        static const std::unordered_set<QString> mimeTypesExtensions;
        static const std::unordered_set<QString> videoMimeTypesExtensions;
        static const QString databaseType;
        static const int maxDbVariableCount;
        static LibraryUtils* instance();

        const QString& databaseFilePath();

        static QString findMediaArtForDirectory(std::unordered_map<QString, QString>& mediaArtHash, const QString& directoryPath);

        void initDatabase();
        Q_INVOKABLE void updateDatabase();
        Q_INVOKABLE void resetDatabase();

        bool isDatabaseInitialized();
        bool isCreatedTable();
        bool isUpdating();

        int artistsCount();
        int albumsCount();
        int tracksCount();
        int tracksDuration();
        QString randomMediaArt();
        Q_INVOKABLE QString randomMediaArtForArtist(const QString& artist);
        Q_INVOKABLE QString randomMediaArtForAlbum(const QString& artist, const QString& album);
        Q_INVOKABLE QString randomMediaArtForGenre(const QString& genre);

        Q_INVOKABLE void setMediaArt(const QString& artist, const QString& album, const QString& mediaArt);
    private:
        LibraryUtils();

        QString saveEmbeddedMediaArt(const QByteArray& data, std::unordered_map<QByteArray, QString>& embeddedMediaArtFiles);

        bool mDatabaseInitialized;
        bool mCreatedTable;
        bool mUpdating;

        QString mDatabaseFilePath;
        QString mMediaArtDirectory;
        QMimeDatabase mMimeDb;
    signals:
        void updatingChanged();
        void databaseChanged();
        void mediaArtChanged();
    };
}

#endif // UNPLAYER_LIBRARYUTILS_H

/*
 * Unplayer
 * Copyright (C) 2015-2017 Alexey Rochev <equeim@gmail.com>
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

#ifndef UNPLAYER_SETTINGS_H
#define UNPLAYER_SETTINGS_H

#include <QObject>

class QSettings;

namespace unplayer
{
    class Settings : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(bool hasLibraryDirectories READ hasLibraryDirectories NOTIFY libraryDirectoriesChanged)
    public:
        static Settings* instance();

        bool hasLibraryDirectories() const;

        QStringList libraryDirectories() const;
        void setLibraryDirectories(const QStringList& directories);
    private:
        explicit Settings(QObject* parent);

        QSettings* mSettings;
    signals:
        void libraryDirectoriesChanged();
    };
}

#endif // UNPLAYER_SETTINGS_H

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

import QtSparql 1.0

SparqlListModel {
    connection: SparqlConnection {
        driver: "QTRACKER_DIRECT"
    }
    query: "SELECT tracker:coalesce(nmm:artistName(nmm:performer(?track)), \"" + qsTr("Unknown artist") + "\") AS ?artist\n" +
           "       nmm:artistName(nmm:performer(?track)) AS ?rawArtist\n" +
           "       COUNT(DISTINCT(tracker:coalesce(nmm:musicAlbum(?track), 0))) AS ?albumsCount\n" +
           "       COUNT(?track) AS ?tracksCount\n" +
           "       SUM(nfo:duration(?track)) AS ?duration\n" +
           "WHERE {\n" +
           "    ?track a nmm:MusicPiece.\n" +
           "}\n" +
           "GROUP BY ?rawArtist\n" +
           "ORDER BY !bound(?rawArtist) ?rawArtist"
}

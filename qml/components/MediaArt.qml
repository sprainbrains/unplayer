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

import QtQuick 2.2
import Sailfish.Silica 1.0

Item {
    property bool highlighted
    property int size
    property alias source: mediaArtImage.source
    property string fallbackIcon: "image://theme/icon-m-music"

    width: size
    height: size
    opacity: enabled ? 1.0 : 0.4

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.rgba(Theme.primaryColor, 0.1)
            }
            GradientStop {
                position: 1.0
                color: Theme.rgba(Theme.primaryColor, 0.05)
            }
        }
        visible: !mediaArtImage.visible

        Image {
            anchors.centerIn: parent
            asynchronous: true
            source: highlighted ? fallbackIcon + "?" + Theme.highlightColor :
                                  fallbackIcon
        }
    }

    Image {
        id: mediaArtImage

        anchors.fill: parent
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.height: size
        visible: status === Image.Ready

        Rectangle {
            anchors.fill: parent
            color: Theme.highlightBackgroundColor
            opacity: Theme.highlightBackgroundOpacity
            visible: highlighted
        }
    }
}

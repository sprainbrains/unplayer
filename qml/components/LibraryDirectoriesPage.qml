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

import QtQuick 2.2
import Sailfish.Silica 1.0

import harbour.unplayer 0.1 as Unplayer

Page {
    property bool changed: false

    Component.onDestruction: {
        if (changed) {
            Unplayer.LibraryUtils.updateDatabase()
        }
    }

    SelectionPanel {
        id: selectionPanel
        selectionText: qsTr("%n directories selected", String(), proxyModel.selectedIndexesCount)

        PushUpMenu {
            MenuItem {
                enabled: proxyModel.selectedIndexesCount !== 0
                text: qsTr("Remove")
                onClicked: {
                    libraryDirectoriesModel.removeDirectories(proxyModel.selectedSourceIndexes)
                    changed = true
                    selectionPanel.showPanel = false
                }
            }
        }
    }

    SilicaListView {
        id: listView

        anchors {
            fill: parent
            bottomMargin: selectionPanel.visible ? selectionPanel.visibleSize : 0
        }
        clip: true

        header: PageHeader {
            title: qsTr("Library Directories")
        }
        delegate: ListItem {
            id: delegate

            menu: ContextMenu {
                MenuItem {
                    function remove() {
                        libraryDirectoriesModel.removeDirectory(model.index)
                        changed = true
                        delegate.menuOpenChanged.disconnect(remove)
                    }
                    text: qsTr("Remove")
                    onClicked: delegate.menuOpenChanged.connect(remove)
                }
            }

            onClicked: {
                if (selectionPanel.showPanel) {
                    proxyModel.select(model.index)
                }
            }

            ListView.onRemove: animateRemoval()

            Label {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: model.directory
                color: highlighted ? Theme.highlightColor : Theme.primaryColor
                truncationMode: TruncationMode.Fade
            }

            Binding {
                target: delegate
                property: "highlighted"
                value: down || menuOpen || proxyModel.isSelected(model.index)
            }

            Connections {
                target: proxyModel
                onSelectionChanged: delegate.highlighted = proxyModel.isSelected(model.index)
            }
        }

        model: Unplayer.FilterProxyModel {
            id: proxyModel
            sourceModel: Unplayer.LibraryDirectoriesModel {
                id: libraryDirectoriesModel
            }
        }

        PullDownMenu {
            SelectionMenuItem {
                text: qsTr("Select")
            }

            MenuItem {
                text: qsTr("Add directory...")
                onClicked: pageStack.push(filePickerDialogComponent)

                Component {
                    id: filePickerDialogComponent

                    FilePickerDialog {
                        title: qsTr("Select directory")
                        showFiles: false
                        onAccepted: {
                            libraryDirectoriesModel.addDirectory(filePath)
                            changed = true
                        }
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: !listView.count
            text: qsTr("No directories")
        }

        VerticalScrollDecorator { }
    }
}
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md FR-3.5 / §9.2: sectioned results — Artists, Albums, Tracks — from one
// debounced incremental query.
//
// The sections come from a role on a flat model, so the grouping is the
// ListView's own and this file contains no logic about which result is which.
Item {
    id: root

    signal openAlbum(string albumId, string title, string artist, string artistId,
                     string coverId, int year)
    signal openArtist(string artistId, string name)

    Theme { id: theme }

    QueueMenu { id: rowMenu }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: head.implicitHeight + theme.margin * 2
            color: theme.surface

            ColumnLayout {
                id: head
                anchors.fill: parent
                anchors.margins: theme.margin
                spacing: 2

                Label {
                    text: app.search.term ? qsTr("Results for “%1”").arg(app.search.term)
                                          : qsTr("Search")
                    color: theme.textPrimary
                    font.pixelSize: theme.fontLarge
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: {
                        if (app.search.searching)
                            return qsTr("Searching…")
                        if (!app.search.term)
                            return qsTr("Type in the box above to search the library.")
                        if (!app.search.hasResults)
                            return qsTr("Nothing matched.")
                        return qsTr("%1 artists · %2 albums · %3 tracks")
                            .arg(app.search.artistTotal)
                            .arg(app.search.albumTotal)
                            .arg(app.search.trackTotal)
                    }
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.search
            clip: true
            ScrollBar.vertical: ScrollBar {}

            section.property: "sectionName"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                required property string section
                width: ListView.view.width
                height: theme.rowHeight
                color: theme.surfaceOverlay

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: theme.margin
                    text: parent.section
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                    font.bold: true
                }
            }

            delegate: ItemDelegate {
                id: row
                required property int index
                required property string itemId
                required property string title
                required property int section
                required property string selectorKey

                width: ListView.view.width
                height: theme.rowHeight

                // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                // padding above and below its content.
                topPadding: 0
                bottomPadding: 0

                background: Rectangle {
                    color: row.hovered ? theme.surfaceHover : "transparent"
                }

                contentItem: Label {
                    text: row.title
                    color: theme.textPrimary
                    font.pixelSize: theme.fontNormal
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    // A track plays; an artist and an album open, because that
                    // is what the user is looking for when they searched for
                    // one rather than for a song.
                    if (row.section === 0)
                        root.openArtist(row.itemId, row.title)
                    else if (row.section === 1)
                        root.openAlbum(row.itemId, row.title, "", "", "", -1)
                    else
                        app.library.enqueue(row.selectorKey, row.itemId, Library.PlayNow)
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        rowMenu.selectorKey = row.selectorKey
                        rowMenu.selectorValue = row.itemId
                        rowMenu.selectorFilters = []
                        rowMenu.popup()
                    }
                }
            }
        }
    }
}

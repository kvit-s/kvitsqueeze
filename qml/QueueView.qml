// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md §8.4: the queue, with the active track highlighted, auto-scrolled to
// on change, reorderable by drag, and removable a row at a time.
//
// Every edit here is a command to the server, and the model is rebuilt from
// what the server says afterwards. A drag that the server rejects therefore
// snaps back — which is the correct outcome, and visible rather than silent.
Item {
    id: root

    Theme { id: theme }
    MixControl { id: mixControl }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── One narrow strip: what this is, one line of state, three icons.
        //
        // It used to be a three-line block with four worded buttons, which took
        // 90-odd pixels off the top of a list whose whole job is to be a list.
        // The words moved into tooltips; the state line moved onto one line;
        // the mix's own explanation is now on hover, where a sentence a
        // returning user has already read does not have to be re-read.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: theme.rowHeight + theme.spacing
            color: theme.surfaceRaised

            RowLayout {
                id: header
                anchors.fill: parent
                anchors.leftMargin: theme.margin
                anchors.rightMargin: 4
                spacing: theme.spacing

                Label {
                    text: qsTr("Queue")
                    color: theme.textPrimary
                    font.pixelSize: theme.fontNormal
                    font.bold: true
                }

                Label {
                    id: state
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                    text: {
                        if (app.mix.active) {
                            // Under a mix the total is meaningless — it is the
                            // size of a window, not of what will play. What is
                            // ahead is the number worth stating.
                            var ahead = Math.max(0, app.queue.count
                                                    - app.queue.currentIndex - 1)
                            return app.mix.mixName + "  ·  "
                                   + qsTr("%n ahead", "", ahead)
                                   + "  ·  " + qsTr("a moving window")
                        }
                        if (app.queue.count === 0)
                            return qsTr("Empty")
                        var text = qsTr("%n track(s)", "", app.queue.count)
                        var total = theme.longDuration(app.queue.totalDuration)
                        if (total)
                            text += "  ·  " + total
                        if (app.queue.truncated)
                            text += "  ·  " + qsTr("showing the first %1")
                                .arg(app.queue.count)
                        return text
                    }

                    // prd.md FR-3.9.6. Three words on the line say *that* the
                    // queue is a window; this says what that means, for the one
                    // reading where twenty rows would otherwise look like the
                    // app had truncated something.
                    ToolTip.visible: stateHover.hovered && app.mix.active
                    ToolTip.text: qsTr("A mix keeps a moving window: played "
                                       + "tracks drop off the top and fresh "
                                       + "ones arrive at the bottom. This is "
                                       + "not everything it will play.")
                    HoverHandler { id: stateHover }
                }

                // The mix belongs on this screen and not only on now-playing.
                // Somebody whose listening *is* a random mix reads the queue,
                // not the cover — the first build shipped the start buttons on
                // now-playing only, which meant the person the feature was
                // built for never saw them.
                IconButton {
                    glyph: theme.iconMix
                    glyphSize: theme.fontNormal
                    active: app.mix.active
                    tooltip: app.mix.active
                             ? qsTr("%1 is running").arg(app.mix.mixName)
                             : qsTr("Random mix…")
                    onClicked: mixControl.openMenu()
                }
                IconButton {
                    glyph: theme.iconSave
                    glyphSize: theme.fontNormal
                    enabled: app.queue.count > 0
                    tooltip: qsTr("Save as playlist…")
                    onClicked: saveDialog.open()
                }
                IconButton {
                    glyph: theme.iconClear
                    glyphSize: theme.fontNormal
                    enabled: app.queue.count > 0
                    tooltip: qsTr("Clear the queue")
                    onClicked: app.queue.clear()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.queue
            clip: true
            focus: true
            ScrollBar.vertical: ScrollBar {}

            // prd.md FR-4.1: auto-scroll to the active track on change. Only
            // when it is off screen — yanking the view back while somebody is
            // reading further down the queue is worse than not scrolling.
            Connections {
                target: app.queue
                function onCurrentIndexChanged() {
                    const index = app.queue.currentIndex
                    if (index < 0)
                        return
                    const top = list.indexAt(0, list.contentY)
                    const bottom = list.indexAt(0, list.contentY + list.height - 1)
                    if (index < top || index > bottom || top < 0)
                        list.positionViewAtIndex(index, ListView.Center)
                }
            }

            displaced: Transition {
                NumberAnimation { properties: "y"; duration: theme.animation }
            }

            delegate: ItemDelegate {
                id: row
                required property int index
                required property string title
                required property string artist
                required property string album
                required property string coverId
                required property double duration
                required property bool isCurrent

                width: ListView.view.width
                height: theme.trackHeight

                // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                // padding above and below, which this row does not have to
                // give. Without these two lines the artist line is drawn over
                // the row below.
                topPadding: 0
                bottomPadding: 0

                background: Rectangle {
                    color: row.isCurrent ? theme.surfaceOverlay
                         : row.hovered ? theme.surfaceHover : "transparent"
                }

                contentItem: RowLayout {
                    spacing: theme.spacing

                    // The drag handle is a separate grip rather than the whole
                    // row: a row that moves when you meant to click it is the
                    // most annoying possible way to lose your place.
                    Label {
                        text: row.isCurrent ? theme.iconPlay : String(row.index + 1)
                        font.family: row.isCurrent ? theme.iconFont : Qt.application.font.family
                        font.pixelSize: theme.fontSmall
                        color: row.isCurrent ? theme.accent : theme.textFaint
                        Layout.preferredWidth: 28
                        horizontalAlignment: Text.AlignHCenter

                        DragHandler {
                            id: grip
                            target: null
                            onActiveChanged: {
                                if (active) {
                                    dragState.from = row.index
                                } else if (dragState.from >= 0 && dragState.to >= 0
                                           && dragState.from !== dragState.to) {
                                    app.queue.move(dragState.from, dragState.to)
                                    dragState.from = -1
                                    dragState.to = -1
                                }
                            }
                            onCentroidChanged: {
                                if (!active)
                                    return
                                const point = list.mapFromItem(row, grip.centroid.position)
                                const target = list.indexAt(list.width / 2,
                                                            point.y + list.contentY)
                                if (target >= 0)
                                    dragState.to = target
                            }
                        }
                    }

                    Artwork {
                        Layout.preferredWidth: row.height - 8
                        Layout.preferredHeight: row.height - 8
                        coverId: row.coverId
                        requestSize: 100
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: row.title
                            color: row.isCurrent ? theme.accent : theme.textPrimary
                            font.pixelSize: theme.fontNormal
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: row.artist + (row.album ? " — " + row.album : "")
                            visible: text.length > 0
                            color: theme.textMuted
                            font.pixelSize: theme.fontSmall
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        text: theme.duration(row.duration)
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                    }

                    IconButton {
                        glyph: theme.iconRemove
                        glyphSize: theme.fontSmall
                        opacity: row.hovered ? 1 : 0
                        tooltip: qsTr("Remove from the queue")
                        onClicked: app.queue.removeIndex(row.index)
                    }
                }

                onDoubleClicked: app.queue.playIndex(row.index)
            }

            Label {
                anchors.centerIn: parent
                visible: app.queue.count === 0
                horizontalAlignment: Text.AlignHCenter
                color: theme.textMuted

                // An empty queue under a running mix is a gap, not a dead end.
                // The plugin tops up on a ten-second timer after a track
                // change, so skipping fast can outrun it — and "play something
                // from the library" would be exactly the wrong advice, because
                // doing that ends the mix.
                text: app.mix.active
                      ? qsTr("The mix is topping up.\nThis takes a few seconds "
                             + "after a run of skips.")
                      : qsTr("The queue is empty.\nPlay something from the library.")
            }
        }
    }

    // Where a drag started and where it is now. Kept outside the delegate
    // because the delegate that started the drag may be recycled during it.
    QtObject {
        id: dragState
        property int from: -1
        property int to: -1
    }

    Dialog {
        id: saveDialog
        title: qsTr("Save the queue as a playlist")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel

        onAccepted: {
            app.queue.saveAs(nameField.text)
            nameField.text = ""
        }

        TextField {
            id: nameField
            width: 320
            placeholderText: qsTr("Playlist name")
            onAccepted: saveDialog.accept()
        }
    }
}

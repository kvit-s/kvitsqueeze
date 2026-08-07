pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md FR-9.2: raw control traffic, engine counters, and a copy button for
// bug reports.
//
// The engine counters are where prd.md FR-2.5's rule is most visible: under
// Backend B every one of them is scraped from squeezelite's log output, so
// several are simply unknown and this screen says "unknown" rather than
// printing a zero that reads like a measurement.
Item {
    id: root

    Theme { id: theme }

    function unknownOr(value, suffix) {
        return value < 0 ? qsTr("unknown") : value + (suffix ? " " + suffix : "")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: head.implicitHeight + theme.margin * 2
            color: theme.surfaceRaised

            ColumnLayout {
                id: head
                anchors.fill: parent
                anchors.margins: theme.margin
                spacing: theme.spacing

                Label {
                    text: qsTr("Diagnostics")
                    color: theme.textPrimary
                    font.pixelSize: theme.fontLarge
                    font.bold: true
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: theme.margin

                    Repeater {
                        model: [
                            { "k": qsTr("Engine"),        "v": app.engine.stateText },
                            { "k": qsTr("Decoder"),       "v": app.engine.decoder || qsTr("unknown") },
                            { "k": qsTr("Source rate"),   "v": root.unknownOr(app.engine.sourceSampleRate, "Hz") },
                            { "k": qsTr("Source depth"),  "v": root.unknownOr(app.engine.sourceBitDepth, "bit") },
                            { "k": qsTr("Output rate"),   "v": root.unknownOr(app.engine.outputSampleRate, "Hz") },
                            { "k": qsTr("Underruns"),     "v": root.unknownOr(app.engine.underruns, "") },
                            { "k": qsTr("Output device"), "v": app.engine.activeOutputDevice || qsTr("unknown") }
                        ]
                        delegate: Row {
                            id: counter
                            required property var modelData
                            spacing: 6
                            Label {
                                text: counter.modelData.k + ":"
                                color: theme.textFaint
                                font.pixelSize: theme.fontSmall
                            }
                            Label {
                                text: counter.modelData.v
                                color: theme.textPrimary
                                font.pixelSize: theme.fontSmall
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.spacing

                    Button {
                        text: qsTr("Copy diagnostics")
                        onClicked: {
                            clipboard.text = app.diagnostics.asText()
                            clipboard.selectAll()
                            clipboard.copy()
                            clipboard.deselect()
                        }
                    }
                    Button {
                        text: app.diagnostics.paused ? qsTr("Resume") : qsTr("Pause")
                        onClicked: app.diagnostics.paused = !app.diagnostics.paused
                    }
                    Button {
                        text: qsTr("Clear")
                        onClicked: app.diagnostics.clear()
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: qsTr("%n line(s)", "", app.diagnostics.count)
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        ListView {
            id: log
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.diagnostics
            clip: true
            ScrollBar.vertical: ScrollBar {}

            // Follow the tail only while the user is already at it. Yanking
            // the view down while somebody is reading is the classic way to
            // make a log panel useless.
            property bool atTail: true
            onContentYChanged: atTail = (contentY + height >= contentHeight - 8)
            onCountChanged: if (atTail) positionViewAtEnd()

            delegate: Row {
                id: line
                required property string text
                required property string sourceName
                required property string timestamp
                required property int source

                width: ListView.view.width
                spacing: 8

                Label {
                    text: line.timestamp
                    color: theme.textFaint
                    font.family: "Consolas"
                    font.pixelSize: theme.fontSmall
                }
                Label {
                    width: 66
                    text: line.sourceName
                    color: line.source === Diagnostics.ControlOut ? theme.accent
                         : line.source === Diagnostics.Engine ? theme.warning
                                                              : theme.textMuted
                    font.family: "Consolas"
                    font.pixelSize: theme.fontSmall
                }
                Label {
                    width: line.width - 150
                    text: line.text
                    color: theme.textPrimary
                    font.family: "Consolas"
                    font.pixelSize: theme.fontSmall
                    elide: Text.ElideRight
                }
            }
        }
    }

    // The only way to reach the clipboard from QML without a C++ helper: an
    // off-screen editable text item that can be selected and copied.
    TextEdit {
        id: clipboard
        visible: false
        width: 0
        height: 0
    }
}

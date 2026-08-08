pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// The random mix, on the now-playing screen (prd.md FR-3.9).
//
// Sized for the listener whose main use of the app is a mix: every mix type is
// one click, not one click into a menu, and whether a mix is running is a
// standing statement rather than a toast that has already gone.
//
// The three-state indicator is the part that matters. The mix plugin emits no
// event when a mix starts or stops, so the app polls — and while nobody has
// answered, the honest thing to draw is "unknown", not "off". A dark indicator
// over a live mix is the one failure a listener has no way to diagnose from
// the outside (prd.md FR-2.5's rule, applied where it bites next).
ColumnLayout {
    id: root

    spacing: 6

    Theme { id: theme }
    MixControl { id: control }

    // ── What is running, if anything.
    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: theme.spacing
        visible: app.mix.active || !app.mix.known

        Rectangle {
            Layout.preferredHeight: theme.rowHeight
            Layout.preferredWidth: pill.implicitWidth + theme.margin * 2
            radius: height / 2
            color: app.mix.active ? theme.surfaceOverlay : "transparent"
            border.width: app.mix.active ? 0 : 1
            border.color: theme.border

            RowLayout {
                id: pill
                anchors.centerIn: parent
                spacing: 6

                Label {
                    text: theme.iconMix
                    font.family: theme.iconFont
                    font.pixelSize: theme.fontSmall
                    color: app.mix.active ? theme.accent : theme.textFaint
                }
                Label {
                    text: app.mix.active ? app.mix.mixName
                                         : qsTr("Mix state unknown")
                    color: app.mix.active ? theme.textPrimary : theme.textFaint
                    font.pixelSize: theme.fontSmall
                }
            }

            // Said here rather than in a log nobody opens: this is the state
            // the app cannot ask about any faster than it already is.
            ToolTip.visible: hover.hovered && !app.mix.active
            ToolTip.text: qsTr("The server has not said whether a mix is "
                               + "running. The mix plugin announces nothing, "
                               + "so this is asked for rather than pushed.")
            HoverHandler { id: hover }
        }

        Button {
            text: qsTr("Stop")
            visible: app.mix.active
            flat: true
            onClicked: control.stop()
        }
    }

    // ── Start one. Five buttons, because a menu would make the most common
    // action in this app two clicks deep.
    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 4

        Label {
            text: qsTr("Random mix")
            color: theme.textFaint
            font.pixelSize: theme.fontSmall
            rightPadding: 4
        }

        Repeater {
            model: [
                { "type": Mix.Songs,   "label": qsTr("Songs") },
                { "type": Mix.Albums,  "label": qsTr("Albums") },
                { "type": Mix.Artists, "label": qsTr("Artists") },
                { "type": Mix.Years,   "label": qsTr("Years") },
                { "type": Mix.Works,   "label": qsTr("Works") }
            ]

            delegate: Button {
                id: mixButton
                required property var modelData

                text: mixButton.modelData.label
                flat: true
                // The mix that is running is shown as pressed, so re-rolling
                // it and starting a different one look different before the
                // click rather than after it.
                highlighted: app.mix.active
                             && app.mix.mixType === mixButton.modelData.type
                onClicked: control.start(mixButton.modelData.type)
            }
        }
    }

    // ── The scope, stated whether or not anyone asked.
    //
    // It is a server-side pref shared with every other controller and it
    // survives restarts, so a narrowing done weeks ago is still in force. That
    // is unguessable from the mix itself — it just quietly plays less music.
    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 6

        Label {
            text: app.mix.genreSummary
            visible: text.length > 0
            color: theme.textFaint
            font.pixelSize: theme.fontSmall
        }
        Button {
            text: qsTr("Choose genres…")
            flat: true
            font.pixelSize: theme.fontSmall
            onClicked: control.openGenres()
        }
    }

    // The summary is only true once it has been read, and reading it costs a
    // request — so it is fetched when a screen that shows it appears, not kept
    // warm for one that might never be opened.
    Component.onCompleted: app.mix.refreshGenres()
}

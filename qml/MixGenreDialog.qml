pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// The genres a random mix may draw from (prd.md FR-3.9).
//
// Two things this says out loud, because both have surprised people:
//
//   * The scope is the *server's*, not this window's. It is shared with the
//     LMS web UI and every other controller, and it survives a restart — so a
//     selection narrowed weeks ago is still narrowing the mix today. That is
//     the whole reason the summary line lives beside the mix control instead
//     of only in here.
//   * It applies to the next mix as much as the running one. Changing it does
//     not re-roll what is already queued.
Dialog {
    id: root

    required property var mix

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 80 : 460, 480)
    height: Math.min(parent ? parent.height - 80 : 520, 600)

    modal: true
    title: qsTr("Genres the mix may draw from")
    standardButtons: Dialog.Close

    Theme { id: theme }

    // Reconcile on the way in: the scope may have been changed from another
    // controller since the last look, and this is the only screen that shows
    // it.
    onAboutToShow: root.mix.refreshGenres()

    ColumnLayout {
        anchors.fill: parent
        spacing: theme.spacing

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Shared with the server and every other controller, and "
                       + "kept until you change it again. Applies to the next "
                       + "mix as well as the one playing.")
            color: theme.textFaint
            font.pixelSize: theme.fontSmall
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            Label {
                Layout.fillWidth: true
                text: root.mix.genreSummary
                color: theme.textMuted
                font.pixelSize: theme.fontSmall
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("All")
                flat: true
                onClicked: root.mix.setAllGenresIncluded(true)
            }
            Button {
                text: qsTr("None")
                flat: true
                onClicked: root.mix.setAllGenresIncluded(false)
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.mix.genres
            clip: true
            ScrollBar.vertical: ScrollBar {}

            delegate: CheckDelegate {
                id: genreRow
                required property string name
                required property bool included

                width: ListView.view.width
                height: theme.rowHeight
                // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                // padding above and below its content.
                topPadding: 0
                bottomPadding: 0

                text: genreRow.name
                checked: genreRow.included

                // A CheckDelegate flips its own `checked` on a click, which
                // breaks the binding above and leaves the row showing a state
                // the server never confirmed. Restoring the binding first
                // hands the row back to the model, so a write the server
                // rejects corrects itself on the next reconcile instead of
                // sitting there looking applied.
                onClicked: {
                    const wanted = !genreRow.included
                    genreRow.checked = Qt.binding(function () {
                        return genreRow.included
                    })
                    root.mix.setGenreIncluded(genreRow.name, wanted)
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - theme.margin * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: root.mix.genres.loaded && root.mix.genres.count === 0
                text: qsTr("The server listed no genres.")
                color: theme.textMuted
            }
        }
    }
}

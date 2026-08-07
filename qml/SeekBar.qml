pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

// The progress bar, and the seek that goes with it (prd.md FR-5.1, FR-5.2).
//
// prd.md FR-5.3 wants this moving at 60 fps rather than stepping once a
// second; the interpolation is in PlaybackController, and what this element
// contributes is *not fighting it*. While the handle is held the binding to
// `elapsed` is suspended, because otherwise every interpolated tick would drag
// the handle back out from under the pointer.
Slider {
    id: root

    property bool seeking: false

    Theme { id: theme }

    from: 0
    to: Math.max(1, app.player.duration)
    enabled: app.player.seekable
    live: false

    // The position arrives as a signal rather than a binding, and the binding
    // it replaces was `value: seeking ? value : elapsed` — self-referential,
    // which is a binding loop dressed up as a guard.
    Connections {
        target: app.player
        function onPositionChanged() {
            if (!root.seeking)
                root.value = Math.max(0, app.player.elapsed)
        }
    }

    // `position` rather than `value`, and this is the whole bug that made
    // dragging do nothing. Measured against the real control (SeekBarTests):
    //
    //   press at 120 s → position 0.601, value 0
    //   drag to 150 s  → moved() emitted on every step, value still 0
    //   release        → value finally becomes 151.6
    //
    // With live:false `value` is written *after* the interaction, so both
    // obvious spellings send the position the handle started from: a seek to
    // where the track already was, indistinguishable from being ignored.
    // `position` is correct from the press onwards, and valueAt() converts it.
    onPressedChanged: {
        if (pressed) {
            seeking = true
        } else {
            seeking = false
            app.player.seek(valueAt(position))
        }
    }

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: 2
        color: theme.surfaceOverlay

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: root.enabled ? theme.accent : theme.textFaint
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: 12
        implicitHeight: 12
        radius: 6
        color: theme.accent
        border.width: root.visualFocus ? 2 : 0
        border.color: theme.textPrimary
        // Hidden on an unseekable stream rather than shown greyed: there is no
        // position to point at when the server reported no duration.
        visible: root.enabled && (root.hovered || root.pressed || root.visualFocus)
    }
}

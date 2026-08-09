pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

// The progress bar, and the seek that goes with it (prd.md FR-5.1, FR-5.2).
//
// **There is one of these in the shell, and it is the divider.** It spans the
// window on the seam between the content pane and the bottom bar, where a 1 px
// border used to be. That came out of the first UI review: there had been a
// full-width bar on Now Playing *and* a short one in the bottom bar, both
// tracking the same track, both seekable, sixty pixels apart. Neither was
// wrong; the pair was. A line that is already drawn there anyway can carry the
// position for nothing, and it is the widest target in the window.
//
// So the resting state has to read as a divider and not as a control: 3 px,
// square ends, the border colour behind and the accent in front. It thickens
// to 6 px and grows a handle on hover, which is the only moment it needs to
// look like something you can grab.
//
// prd.md FR-5.3 wants this moving at 60 fps rather than stepping once a
// second; the interpolation is in PlaybackController, and what this element
// contributes is *not fighting it*. While the handle is held the binding to
// `elapsed` is suspended, because otherwise every interpolated tick would drag
// the handle back out from under the pointer.
Slider {
    id: root

    property bool seeking: false

    // Hover and press are the same state as far as the drawing goes: both mean
    // "somebody is aiming at this", and both are worth 3 px more.
    readonly property bool engaged: hovered || pressed || visualFocus

    Theme { id: theme }

    from: 0
    to: Math.max(1, app.player.duration)
    enabled: app.player.seekable
    live: false

    // The strip is taller than the line it draws. 3 px is a divider; 3 px is
    // also unclickable, and the hit area is what the pointer is actually
    // aiming at. `padding: 0` keeps `availableWidth` equal to the width, so
    // the bar reaches both edges of the window and the position it reports is
    // the position it drew.
    padding: 0
    implicitHeight: 12
    hoverEnabled: true

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
        height: root.engaged ? 6 : 3
        // Square ends and the border colour: at rest this is the seam between
        // two panels that happens to be filled in from the left.
        color: theme.border

        Behavior on height {
            NumberAnimation { duration: theme.animation }
        }

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
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
        visible: root.enabled && root.engaged
    }
}

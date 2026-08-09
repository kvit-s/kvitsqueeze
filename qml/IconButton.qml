pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

// A transport-style button: one glyph, a hover wash, and a visible focus ring.
//
// The focus ring is not decoration. prd.md FR-8.2 requires every screen to be
// fully navigable by keyboard *with* a visible focus indicator, and a control
// that only shows hover is a control a keyboard user cannot locate.
AbstractButton {
    id: root

    property string glyph
    property int glyphSize: theme.fontLarge
    // Almost always the icon font. Overridden only where the glyph is ordinary
    // text — the rail's «/» chevrons — because Segoe MDL2 Assets is a symbol
    // font and what it draws for a Latin-1 code point is not worth betting on.
    property string glyphFont: theme.iconFont
    property bool active: false          // drawn in the accent colour when on
    property color baseColor: theme.textPrimary
    property alias tooltip: hint.text

    Theme { id: theme }

    implicitWidth: Math.round(glyphSize * 2.1)
    implicitHeight: implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: hint.text

    background: Rectangle {
        radius: theme.radius
        color: root.down ? theme.surfaceOverlay
                         : root.hovered ? theme.surfaceHover : "transparent"
        border.width: root.visualFocus ? 1 : 0
        border.color: theme.accent

        Behavior on color {
            ColorAnimation { duration: theme.animation }
        }
    }

    contentItem: Text {
        text: root.glyph
        font.family: root.glyphFont
        font.pixelSize: root.glyphSize
        color: !root.enabled ? theme.textFaint
                             : root.active ? theme.accent : root.baseColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    ToolTip {
        id: hint
        visible: text.length > 0 && root.hovered
        delay: 600
    }
}

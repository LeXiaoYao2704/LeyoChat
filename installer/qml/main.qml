import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    id: root
    width: 680
    height: 560
    minimumWidth: 680
    minimumHeight: 560
    maximumWidth: 680
    maximumHeight: 560
    visible: true
    title: "LeyoChat 安装程序"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    // Brand colors
    readonly property color brandColor: "#2CA58D"
    readonly property color brandLight: "#3DB89E"
    readonly property color brandDark: "#1E8A74"
    readonly property color bgColor: "#FAFBFC"
    readonly property color textPrimary: "#1A1A2E"
    readonly property color textSecondary: "#6B7280"
    readonly property color borderColor: "#E5E7EB"

    // Allow window dragging
    property point dragStart
    MouseArea {
        id: dragArea
        anchors.fill: parent
        z: -1
        property bool dragging: false
        onPressed: (mouse) => {
            dragStart = Qt.point(mouse.x, mouse.y);
            dragging = true;
        }
        onPositionChanged: (mouse) => {
            if (dragging) {
                root.x += mouse.x - dragStart.x;
                root.y += mouse.y - dragStart.y;
            }
        }
        onReleased: dragging = false
    }

    // Main background with rounded corners
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 12
        color: root.bgColor
        border.color: root.borderColor
        border.width: 1

        // Shadow effect via layered rect
        layer.enabled: true
        layer.effect: null

        // Close button
        Rectangle {
            id: closeBtn
            width: 36; height: 36
            radius: 18
            x: parent.width - width - 12
            y: 12
            z: 100
            color: closeMa.containsMouse
                   ? (closeMa.pressed ? "#FFD4D4" : "#FFECEC")
                   : "transparent"

            Text {
                anchors.centerIn: parent
                text: "✕"
                font.pixelSize: 16
                color: closeMa.containsMouse ? "#DC2626" : root.textSecondary
            }
            MouseArea {
                id: closeMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: installer.quit()
            }
        }

        // Content area with StackView
        StackView {
            id: stack
            anchors.fill: parent
            anchors.margins: 1
            clip: true
            initialItem: optionsPage

            pushEnter: Transition {
                ParallelAnimation {
                    PropertyAnimation {
                        property: "opacity"
                        from: 0; to: 1
                        duration: 400
                        easing.type: Easing.OutCubic
                    }
                    PropertyAnimation {
                        property: "y"
                        from: 40; to: 0
                        duration: 400
                        easing.type: Easing.OutCubic
                    }
                }
            }
            pushExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1; to: 0
                    duration: 300
                    easing.type: Easing.InCubic
                }
            }
        }
    }

    Component {
        id: optionsPage
        OptionsPage {}
    }

    Component {
        id: progressPage
        ProgressPage {}
    }

    Component {
        id: completePage
        CompletePage {}
    }

    // React to state changes from C++ backend
    Connections {
        target: installer
        function onStateChanged() {
            if (installer.state === 1) { // Installing
                stack.push(progressPage);
            } else if (installer.state === 2) { // Complete
                stack.push(completePage);
            }
        }
    }
}

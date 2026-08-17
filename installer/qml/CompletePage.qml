import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: root.bgColor
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 28

        // ── Animated checkmark circle ──
        Item {
            Layout.alignment: Qt.AlignHCenter
            width: 100; height: 100

            // Background circle
            Rectangle {
                id: checkCircle
                anchors.centerIn: parent
                width: 100; height: 100
                radius: 50
                color: root.brandColor
                scale: 0
                opacity: 0

                Component.onCompleted: {
                    scale = 1.0;
                    opacity = 1.0;
                }

                Behavior on scale {
                    NumberAnimation {
                        duration: 600
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.5
                    }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 400 }
                }

                // Checkmark drawn with two rotated rectangles
                Item {
                    anchors.centerIn: parent
                    width: 40; height: 30
                    x: -2; y: 2

                    // Short stroke
                    Rectangle {
                        x: 4; y: 16
                        width: 16; height: 4
                        radius: 2
                        color: "white"
                        rotation: 45
                        transformOrigin: Item.Left
                        opacity: checkCircle.scale > 0.8 ? 1 : 0
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                    }

                    // Long stroke
                    Rectangle {
                        x: 14; y: 22
                        width: 28; height: 4
                        radius: 2
                        color: "white"
                        rotation: -45
                        transformOrigin: Item.Left
                        opacity: checkCircle.scale > 0.9 ? 1 : 0
                        Behavior on opacity {
                            NumberAnimation { duration: 200; }
                        }
                    }
                }
            }

            // Ring pulse animation
            Rectangle {
                id: pulseRing
                anchors.centerIn: parent
                width: 100; height: 100
                radius: 50
                color: "transparent"
                border.color: root.brandColor
                border.width: 3
                opacity: 0
                scale: 1

                SequentialAnimation on scale {
                    PauseAnimation { duration: 600 }
                    loops: 2
                    ParallelAnimation {
                        NumberAnimation {
                            target: pulseRing; property: "scale"
                            from: 1; to: 1.5
                            duration: 600
                            easing.type: Easing.OutQuad
                        }
                        NumberAnimation {
                            target: pulseRing; property: "opacity"
                            from: 0.6; to: 0
                            duration: 600
                        }
                    }
                }
            }
        }

        // ── Title ──
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: installer.serverMode ? "服务安装完成" : "安装完成"
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: root.textPrimary
        }

        // ── Subtitle ──
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: installer.serverMode
                  ? "LeyoChat Server 已安装并启动"
                  : "LeyoChat 已准备就绪"
            font.pixelSize: 14
            color: root.textSecondary
        }

        Item { Layout.preferredHeight: 12 }

        // ── Finish button ──
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 240; height: 48
            radius: 10
            color: finishMa.containsMouse
                   ? (finishMa.pressed ? root.brandDark : root.brandLight)
                   : root.brandColor

            Behavior on color {
                ColorAnimation { duration: 100 }
            }

            Text {
                anchors.centerIn: parent
                text: "完成"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: "white"
            }

            MouseArea {
                id: finishMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    installer.quit()
                }
            }
        }
    }
}

import QtQuick

// 消息气泡飞入动画演示

Item {
    id: demo

    property int step: 0

    // Reset and replay
    Timer {
        id: sequenceTimer
        interval: 800
        running: true
        repeat: true
        onTriggered: {
            demo.step++;
            if (demo.step > 6) {
                demo.step = 0;
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        radius: 10
        color: "#FFFFFF"
        clip: true

        // Chat header
        Column {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16

            Row {
                spacing: 8
                Rectangle {
                    width: 32; height: 32; radius: 16
                    color: "#2CA58D"
                    Text {
                        anchors.centerIn: parent
                        text: "群"
                        font.pixelSize: 12
                        color: "white"
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "LeyoChat 项目群"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "#1F2937"
                    }
                    Text {
                        text: "5 位成员在线"
                        font.pixelSize: 11
                        color: "#9CA3AF"
                    }
                }
            }
        }

        Rectangle {
            anchors.top: header.bottom
            anchors.topMargin: 8
            width: parent.width - 32
            anchors.horizontalCenter: parent.horizontalCenter
            height: 1
            color: "#F3F4F6"
        }

        // Messages
        Column {
            anchors.top: header.bottom
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            spacing: 12

            // Message 1 (left)
            Row {
                spacing: 8
                opacity: demo.step >= 1 ? 1 : 0
                transform: Translate { y: demo.step >= 1 ? 0 : 20 }
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                Behavior on transform { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                Rectangle {
                    width: 28; height: 28; radius: 14
                    color: "#6366F1"
                    Text { anchors.centerIn: parent; text: "王"; font.pixelSize: 10; color: "white" }
                }
                Rectangle {
                    width: msg1.implicitWidth + 20; height: msg1.implicitHeight + 14
                    radius: 12; color: "#F0F2F5"
                    Text { id: msg1; anchors.centerIn: parent; text: "新版本什么时候发布？"; font.pixelSize: 12; color: "#1F2937" }
                }
            }

            // Message 2 (right, self)
            Row {
                anchors.right: parent.right
                layoutDirection: Qt.RightToLeft
                spacing: 8
                opacity: demo.step >= 2 ? 1 : 0
                transform: Translate { y: demo.step >= 2 ? 0 : 20 }
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                Behavior on transform { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                Rectangle {
                    width: 28; height: 28; radius: 14; color: "#2CA58D"
                    Text { anchors.centerIn: parent; text: "我"; font.pixelSize: 10; color: "white" }
                }
                Rectangle {
                    width: msg2.implicitWidth + 20; height: msg2.implicitHeight + 14
                    radius: 12; color: "#2CA58D"
                    Text { id: msg2; anchors.centerIn: parent; text: "这周五发布 🚀"; font.pixelSize: 12; color: "white" }
                }
            }

            // Message 3 (left)
            Row {
                spacing: 8
                opacity: demo.step >= 3 ? 1 : 0
                transform: Translate { y: demo.step >= 3 ? 0 : 20 }
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                Behavior on transform { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                Rectangle {
                    width: 28; height: 28; radius: 14; color: "#F59E0B"
                    Text { anchors.centerIn: parent; text: "李"; font.pixelSize: 10; color: "white" }
                }
                Rectangle {
                    width: msg3.implicitWidth + 20; height: msg3.implicitHeight + 14
                    radius: 12; color: "#F0F2F5"
                    Text { id: msg3; anchors.centerIn: parent; text: "新安装器的动画效果超赞 ✨"; font.pixelSize: 12; color: "#1F2937" }
                }
            }

            // Message 4 (left)
            Row {
                spacing: 8
                opacity: demo.step >= 4 ? 1 : 0
                transform: Translate { y: demo.step >= 4 ? 0 : 20 }
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                Behavior on transform { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                Rectangle {
                    width: 28; height: 28; radius: 14; color: "#EC4899"
                    Text { anchors.centerIn: parent; text: "赵"; font.pixelSize: 10; color: "white" }
                }
                Rectangle {
                    width: msg4.implicitWidth + 20; height: msg4.implicitHeight + 14
                    radius: 12; color: "#F0F2F5"
                    Text { id: msg4; anchors.centerIn: parent; text: "期待 👏👏"; font.pixelSize: 12; color: "#1F2937" }
                }
            }

            // Typing indicator
            Row {
                spacing: 8
                opacity: demo.step >= 5 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 300 } }

                Rectangle {
                    width: 28; height: 28; radius: 14; color: "#8B5CF6"
                    Text { anchors.centerIn: parent; text: "陈"; font.pixelSize: 10; color: "white" }
                }
                Rectangle {
                    width: 56; height: 28
                    radius: 14; color: "#F0F2F5"

                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        Repeater {
                            model: 3
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: "#9CA3AF"
                                SequentialAnimation on y {
                                    loops: Animation.Infinite
                                    PauseAnimation { duration: index * 150 }
                                    NumberAnimation { from: 0; to: -4; duration: 300; easing.type: Easing.OutQuad }
                                    NumberAnimation { from: -4; to: 0; duration: 300; easing.type: Easing.InQuad }
                                    PauseAnimation { duration: 600 - index * 150 }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Caption
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 32
        text: "即时消息 · 安全沟通"
        font.pixelSize: 13
        font.weight: Font.Medium
        color: "#6B7280"
    }
}

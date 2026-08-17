import QtQuick
import QtQuick.Layouts

// 主题切换动画演示：模拟一个迷你聊天界面在亮色/暗色间渐变切换

Item {
    id: demo

    property bool darkMode: false

    // Toggle every 3 seconds
    Timer {
        interval: 3000
        running: true
        repeat: true
        onTriggered: demo.darkMode = !demo.darkMode
    }

    // Animated colors
    property color animBg: darkMode ? "#1E1E2E" : "#FFFFFF"
    property color animSidebar: darkMode ? "#16162A" : "#F3F4F6"
    property color animText: darkMode ? "#E5E7EB" : "#1F2937"
    property color animTextMuted: darkMode ? "#9CA3AF" : "#6B7280"
    property color animBubbleSelf: darkMode ? "#2CA58D" : "#2CA58D"
    property color animBubbleOther: darkMode ? "#2D2D44" : "#F0F2F5"
    property color animAccent: "#2CA58D"

    Behavior on animBg { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }
    Behavior on animSidebar { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }
    Behavior on animText { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }
    Behavior on animTextMuted { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }
    Behavior on animBubbleOther { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        radius: 10
        color: demo.animBg
        clip: true

        Behavior on color { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Sidebar
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width * 0.30
                color: demo.animSidebar
                radius: 10

                Behavior on color { ColorAnimation { duration: 800 } }

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // Title
                    Text {
                        text: "消息"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: demo.animText
                    }

                    // Conversation items
                    Repeater {
                        model: ["张三", "项目群", "李四"]
                        Rectangle {
                            width: parent.width
                            height: 40
                            radius: 8
                            color: index === 0 ? Qt.rgba(demo.animAccent.r, demo.animAccent.g, demo.animAccent.b, 0.15)
                                               : "transparent"

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                spacing: 8

                                Rectangle {
                                    width: 28; height: 28
                                    radius: 14
                                    color: demo.animAccent
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.charAt(0)
                                        font.pixelSize: 12
                                        color: "white"
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData
                                    font.pixelSize: 12
                                    color: demo.animText
                                }
                            }
                        }
                    }
                }
            }

            // Chat area
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    // Header
                    Text {
                        text: "张三"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: demo.animText
                    }

                    Rectangle { width: parent.width; height: 1; color: demo.animSidebar }

                    // Messages
                    Item {
                        width: parent.width
                        height: parent.height - 60

                        Column {
                            anchors.fill: parent
                            spacing: 10

                            // Other's message
                            Row {
                                spacing: 6
                                Rectangle {
                                    width: 24; height: 24; radius: 12
                                    color: "#6366F1"
                                    Text { anchors.centerIn: parent; text: "张"; font.pixelSize: 10; color: "white" }
                                }
                                Rectangle {
                                    width: msgText1.implicitWidth + 20
                                    height: msgText1.implicitHeight + 12
                                    radius: 12
                                    color: demo.animBubbleOther
                                    Text {
                                        id: msgText1
                                        anchors.centerIn: parent
                                        text: "下午的会议准备好了吗？"
                                        font.pixelSize: 12
                                        color: demo.animText
                                    }
                                }
                            }

                            // Self message (right aligned)
                            Row {
                                anchors.right: parent.right
                                spacing: 6
                                layoutDirection: Qt.RightToLeft
                                Rectangle {
                                    width: 24; height: 24; radius: 12
                                    color: demo.animAccent
                                    Text { anchors.centerIn: parent; text: "我"; font.pixelSize: 10; color: "white" }
                                }
                                Rectangle {
                                    width: msgText2.implicitWidth + 20
                                    height: msgText2.implicitHeight + 12
                                    radius: 12
                                    color: demo.animBubbleSelf
                                    Text {
                                        id: msgText2
                                        anchors.centerIn: parent
                                        text: "准备好了 👍"
                                        font.pixelSize: 12
                                        color: "white"
                                    }
                                }
                            }

                            // Another message
                            Row {
                                spacing: 6
                                Rectangle {
                                    width: 24; height: 24; radius: 12
                                    color: "#6366F1"
                                    Text { anchors.centerIn: parent; text: "张"; font.pixelSize: 10; color: "white" }
                                }
                                Rectangle {
                                    width: msgText3.implicitWidth + 20
                                    height: msgText3.implicitHeight + 12
                                    radius: 12
                                    color: demo.animBubbleOther
                                    Text {
                                        id: msgText3
                                        anchors.centerIn: parent
                                        text: "好的，三点会议室见"
                                        font.pixelSize: 12
                                        color: demo.animText
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Theme toggle indicator
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            width: toggleRow.implicitWidth + 16
            height: 28
            radius: 14
            color: Qt.rgba(demo.animAccent.r, demo.animAccent.g, demo.animAccent.b, 0.2)

            Row {
                id: toggleRow
                anchors.centerIn: parent
                spacing: 6
                Text {
                    text: demo.darkMode ? "🌙" : "☀️"
                    font.pixelSize: 14
                }
                Text {
                    text: demo.darkMode ? "深色模式" : "浅色模式"
                    font.pixelSize: 11
                    color: demo.animText
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // Caption
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 32
        text: "一键切换 · 深色 / 浅色主题"
        font.pixelSize: 13
        font.weight: Font.Medium
        color: "#6B7280"
    }
}

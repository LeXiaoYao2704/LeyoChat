import QtQuick
import QtQuick.Layouts

// 页面切换动画演示：模拟侧边栏点击切换不同功能页面

Item {
    id: demo

    property int currentPage: 0
    readonly property var pages: [
        { icon: "💬", label: "消息", color: "#2CA58D" },
        { icon: "👥", label: "通讯录", color: "#6366F1" },
        { icon: "📁", label: "文件", color: "#F59E0B" },
        { icon: "⚙️", label: "设置", color: "#8B5CF6" }
    ]

    // Auto-switch pages
    Timer {
        interval: 2500
        running: true
        repeat: true
        onTriggered: demo.currentPage = (demo.currentPage + 1) % 4
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        radius: 10
        color: "#FFFFFF"
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Sidebar (narrow icon sidebar)
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 56
                color: "#F9FAFB"
                radius: 10

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Repeater {
                        model: demo.pages
                        Rectangle {
                            width: 40; height: 40
                            radius: 10
                            color: demo.currentPage === index
                                   ? Qt.rgba(0, 0, 0, 0.06) : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: 200 }
                            }

                            // Selection rail
                            Rectangle {
                                width: 3; height: 20
                                radius: 2
                                anchors.left: parent.left
                                anchors.leftMargin: -2
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.color
                                opacity: demo.currentPage === index ? 1 : 0
                                Behavior on opacity {
                                    NumberAnimation { duration: 200 }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.icon
                                    font.pixelSize: 16
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.label
                                    font.pixelSize: 8
                                    color: demo.currentPage === index
                                           ? modelData.color : "#9CA3AF"
                                    Behavior on color {
                                        ColorAnimation { duration: 200 }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Separator
            Rectangle { Layout.fillHeight: true; width: 1; color: "#E5E7EB" }

            // Content area with slide animation
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true

                Repeater {
                    model: demo.pages
                    Item {
                        id: pageItem
                        anchors.fill: parent

                        property bool isCurrent: demo.currentPage === index
                        property real slideOffset: isCurrent ? 0 : 60
                        property real fadeOpacity: isCurrent ? 1 : 0

                        Behavior on slideOffset {
                            NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                        }
                        Behavior on fadeOpacity {
                            NumberAnimation { duration: 350; easing.type: Easing.OutCubic }
                        }

                        opacity: fadeOpacity
                        transform: Translate { y: pageItem.slideOffset }

                        visible: fadeOpacity > 0.01

                        Column {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            // Page header
                            Row {
                                spacing: 8
                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 20
                                }
                                Text {
                                    text: modelData.label
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    color: "#1F2937"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Rectangle { width: parent.width; height: 1; color: "#F3F4F6" }

                            // Content placeholder rows
                            Repeater {
                                model: 4
                                Row {
                                    spacing: 10
                                    Rectangle {
                                        width: 32; height: 32
                                        radius: 16
                                        color: Qt.rgba(
                                            Qt.color(modelData.color).r,
                                            Qt.color(modelData.color).g,
                                            Qt.color(modelData.color).b, 0.15)
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 16; height: 16; radius: 8
                                            color: modelData.color
                                            opacity: 0.5
                                        }
                                    }
                                    Column {
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 4
                                        Rectangle {
                                            width: 100 + Math.random() * 80
                                            height: 10; radius: 5
                                            color: "#E5E7EB"
                                        }
                                        Rectangle {
                                            width: 60 + Math.random() * 40
                                            height: 8; radius: 4
                                            color: "#F3F4F6"
                                        }
                                    }
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
        text: "流畅切换 · 多功能工作台"
        font.pixelSize: 13
        font.weight: Font.Medium
        color: "#6B7280"
    }
}

import QtQuick
import QtQuick.Layouts

// 文件传输动画演示

Item {
    id: demo

    property real transferProgress: 0

    SequentialAnimation on transferProgress {
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 3000; easing.type: Easing.InOutQuad }
        PauseAnimation { duration: 1500 }
        NumberAnimation { from: 1; to: 0; duration: 0 }
        PauseAnimation { duration: 500 }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        radius: 10
        color: "#FFFFFF"
        clip: true

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 20
            width: parent.width - 60

            // Title
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "群文件共享"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: "#1F2937"
            }

            // File cards
            Repeater {
                model: [
                    { name: "项目方案 v3.docx", size: "2.4 MB", icon: "📄", color: "#3B82F6" },
                    { name: "Q2 数据报告.xlsx", size: "1.8 MB", icon: "📊", color: "#10B981" },
                    { name: "产品截图.zip", size: "15.6 MB", icon: "📦", color: "#F59E0B" }
                ]

                Rectangle {
                    Layout.fillWidth: true
                    height: 60
                    radius: 10
                    color: "#F9FAFB"
                    border.color: "#E5E7EB"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        // File icon
                        Rectangle {
                            width: 40; height: 40
                            radius: 10
                            color: Qt.rgba(
                                Qt.color(modelData.color).r,
                                Qt.color(modelData.color).g,
                                Qt.color(modelData.color).b, 0.12)

                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon
                                font.pixelSize: 20
                            }
                        }

                        // File info
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: modelData.name
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: "#1F2937"
                            }

                            // Progress bar for each file (staggered)
                            Item {
                                Layout.fillWidth: true
                                height: 6

                                property real fileProgress: {
                                    var offset = index * 0.2;
                                    var p = demo.transferProgress - offset;
                                    return Math.max(0, Math.min(1, p / (1 - offset)));
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 3
                                    color: "#E5E7EB"
                                }
                                Rectangle {
                                    width: parent.width * parent.fileProgress
                                    height: parent.height
                                    radius: 3
                                    color: modelData.color

                                    Behavior on width {
                                        NumberAnimation { duration: 100 }
                                    }
                                }
                            }
                        }

                        // Size / status
                        Text {
                            text: {
                                var offset = index * 0.2;
                                var p = demo.transferProgress - offset;
                                var fp = Math.max(0, Math.min(1, p / (1 - offset)));
                                if (fp >= 0.99) return "✓";
                                if (fp > 0) return Math.round(fp * 100) + "%";
                                return modelData.size;
                            }
                            font.pixelSize: 12
                            font.weight: text === "✓" ? Font.Bold : Font.Normal
                            color: text === "✓" ? "#10B981" : "#6B7280"
                        }
                    }
                }
            }

            // Transfer speed indicator
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                opacity: demo.transferProgress > 0 && demo.transferProgress < 0.95 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 300 } }

                // Animated upload arrow
                Text {
                    text: "↑"
                    font.pixelSize: 16
                    color: "#2CA58D"

                    SequentialAnimation on y {
                        loops: Animation.Infinite
                        NumberAnimation { from: 2; to: -2; duration: 500 }
                        NumberAnimation { from: -2; to: 2; duration: 500 }
                    }
                }

                Text {
                    text: "12.4 MB/s"
                    font.pixelSize: 12
                    color: "#6B7280"
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
        text: "文件共享 · 高速传输"
        font.pixelSize: 13
        font.weight: Font.Medium
        color: "#6B7280"
    }
}

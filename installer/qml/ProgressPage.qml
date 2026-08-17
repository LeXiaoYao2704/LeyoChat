import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "showcase"

Item {
    id: page

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: root.bgColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 0

        // ── Showcase carousel (top ~70%) ──
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: "#F0F2F5"
            clip: true

            SwipeView {
                id: carousel
                anchors.fill: parent
                anchors.margins: 2

                ThemeSwitchDemo {}
                PageSlideDemo {}
                MessageBubbleDemo {}
                FileTransferDemo {}
            }

            // Auto-rotate every 5 seconds
            Timer {
                interval: 5000
                running: true
                repeat: true
                onTriggered: {
                    var next = (carousel.currentIndex + 1) % carousel.count;
                    carousel.currentIndex = next;
                }
            }

            // Page indicator dots
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14
                spacing: 8
                z: 10

                Repeater {
                    model: carousel.count
                    Rectangle {
                        width: carousel.currentIndex === index ? 20 : 8
                        height: 8
                        radius: 4
                        color: carousel.currentIndex === index
                               ? root.brandColor : "#D1D5DB"
                        Behavior on width {
                            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                        }
                        Behavior on color {
                            ColorAnimation { duration: 250 }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 20 }

        // ── Progress area (bottom ~30%) ──
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            // Status text
            Text {
                Layout.fillWidth: true
                text: installer.statusText || "正在准备..."
                font.pixelSize: installer.progress >= 1.0 ? 16 : 13
                font.weight: installer.progress >= 1.0 ? Font.DemiBold : Font.Normal
                color: installer.progress >= 1.0 ? root.brandColor : root.textSecondary
                elide: Text.ElideMiddle

                Behavior on font.pixelSize {
                    NumberAnimation { duration: 300 }
                }
                Behavior on color {
                    ColorAnimation { duration: 300 }
                }
            }

            // Progress bar
            Rectangle {
                Layout.fillWidth: true
                height: 8
                radius: 4
                color: "#E5E7EB"

                Rectangle {
                    id: progressFill
                    width: parent.width * installer.progress
                    height: parent.height
                    radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: root.brandColor }
                        GradientStop { position: 1.0; color: root.brandLight }
                    }

                    Behavior on width {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }

                    // Shimmer animation on progress bar
                    Rectangle {
                        id: shimmer
                        width: 60
                        height: parent.height
                        radius: parent.radius
                        opacity: 0.3
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: "white" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }

                        SequentialAnimation on x {
                            loops: Animation.Infinite
                            NumberAnimation {
                                from: -60
                                to: progressFill.width
                                duration: 1200
                                easing.type: Easing.InOutQuad
                            }
                            PauseAnimation { duration: 400 }
                        }
                    }
                }
            }

            // Percentage
            Text {
                Layout.alignment: Qt.AlignRight
                text: Math.round(installer.progress * 100) + "%"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: root.brandColor
            }
        }
    }
}

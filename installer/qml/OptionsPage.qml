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
        width: parent.width - 120
        spacing: 24

        // Logo
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 80; height: 80
            radius: 20
            gradient: Gradient {
                GradientStop { position: 0.0; color: root.brandLight }
                GradientStop { position: 1.0; color: root.brandDark }
            }

            Text {
                anchors.centerIn: parent
                text: "H"
                font.pixelSize: 42
                font.bold: true
                color: "white"
            }

            // Subtle entrance animation
            scale: 0.8
            opacity: 0
            Component.onCompleted: {
                scale = 1.0;
                opacity = 1.0;
            }
            Behavior on scale {
                NumberAnimation { duration: 500; easing.type: Easing.OutBack }
            }
            Behavior on opacity {
                NumberAnimation { duration: 400 }
            }
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: installer.productName
            font.pixelSize: 28
            font.weight: Font.DemiBold
            color: root.textPrimary
        }

        // Subtitle
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: installer.serverMode
                  ? "统一文件与消息后台服务  v" + installer.appVersion
                  : "安全高效的企业即时通讯  v" + installer.appVersion
            font.pixelSize: 14
            color: root.textSecondary
        }

        Item { Layout.preferredHeight: 8 }

        // Install path
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "安装路径"
                font.pixelSize: 13
                font.weight: Font.Medium
                color: root.textPrimary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    radius: 8
                    color: "white"
                    border.color: pathInput.activeFocus ? root.brandColor : root.borderColor
                    border.width: pathInput.activeFocus ? 2 : 1

                    Behavior on border.color {
                        ColorAnimation { duration: 150 }
                    }

                    TextInput {
                        id: pathInput
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: TextInput.AlignVCenter
                        font.pixelSize: 13
                        color: root.textPrimary
                        clip: true
                        text: installer.installPath
                        onTextChanged: installer.installPath = text
                        selectByMouse: true
                    }
                }

                Rectangle {
                    width: 40; height: 40
                    radius: 8
                    color: browseMa.containsMouse
                           ? (browseMa.pressed ? root.brandDark : root.brandLight)
                           : root.brandColor

                    Behavior on color {
                        ColorAnimation { duration: 100 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "…"
                        font.pixelSize: 18
                        font.bold: true
                        color: "white"
                    }
                    MouseArea {
                        id: browseMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: installer.browseDirectory()
                    }
                }
            }
        }

        // Auto-launch checkbox
        RowLayout {
            Layout.fillWidth: true
            visible: !installer.serverMode
            Layout.preferredHeight: visible ? implicitHeight : 0
            spacing: 10

            Rectangle {
                width: 22; height: 22
                radius: 6
                color: installer.autoLaunch ? root.brandColor : "white"
                border.color: installer.autoLaunch ? root.brandColor : root.borderColor
                border.width: installer.autoLaunch ? 0 : 1.5

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: 14
                    font.bold: true
                    color: "white"
                    opacity: installer.autoLaunch ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation { duration: 150 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: installer.autoLaunch = !installer.autoLaunch
                }
            }

            Text {
                text: "安装完成后自动启动 LeyoChat"
                font.pixelSize: 13
                color: root.textPrimary
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: installer.autoLaunch = !installer.autoLaunch
                }
            }
        }

        // Auto-startup checkbox
        RowLayout {
            Layout.fillWidth: true
            visible: !installer.serverMode
            Layout.preferredHeight: visible ? implicitHeight : 0
            spacing: 10

            Rectangle {
                width: 22; height: 22
                radius: 6
                color: installer.autoStartup ? root.brandColor : "white"
                border.color: installer.autoStartup ? root.brandColor : root.borderColor
                border.width: installer.autoStartup ? 0 : 1.5

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: 14
                    font.bold: true
                    color: "white"
                    opacity: installer.autoStartup ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation { duration: 150 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: installer.autoStartup = !installer.autoStartup
                }
            }

            Text {
                text: "开机时自动启动 LeyoChat"
                font.pixelSize: 13
                color: root.textPrimary
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: installer.autoStartup = !installer.autoStartup
                }
            }
        }

        Item { Layout.preferredHeight: 8 }

        // Install button
        Rectangle {
            id: installBtn
            Layout.fillWidth: true
            height: 48
            radius: 10
            color: installBtnMa.containsMouse
                   ? (installBtnMa.pressed ? root.brandDark : root.brandLight)
                   : root.brandColor

            Behavior on color {
                ColorAnimation { duration: 100 }
            }

            Text {
                anchors.centerIn: parent
                text: installer.serverMode ? "安装服务" : "一键安装"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: "white"
            }

            MouseArea {
                id: installBtnMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: installer.startInstall()
            }

            // Hover glow effect
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.color: root.brandLight
                border.width: 2
                opacity: installBtnMa.containsMouse ? 0.5 : 0
                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }
            }
        }
    }
}

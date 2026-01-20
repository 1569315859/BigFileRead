import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    modal: true
    width: 420
    height: 320
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor

    background: Rectangle {
        color: panelColor
        border.color: _themeManager.borderColor
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Title
        Text {
            text: qsTr("Register BigFileViewer")
            font.bold: true
            font.pixelSize: 16
            color: textColor
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 8
        }

        Label {
            text: qsTr("Machine ID:")
            color: textColor
            font.pixelSize: 13
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            TextField {
                text: _appController.machineId
                readOnly: true
                Layout.fillWidth: true
                implicitHeight: 32
                font.pixelSize: 11
                font.family: "Consolas"
                color: textColor
                background: Rectangle { 
                    color: Qt.darker(bgColor, 1.1)
                    border.color: _themeManager.borderColor
                    radius: 4 
                }
            }
            Button {
                text: qsTr("Copy")
                implicitHeight: 32
                implicitWidth: 60
                onClicked: _appController.copyToClipboard(_appController.machineId)
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: _themeManager.borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Label {
            text: qsTr("License Key:")
            color: textColor
            font.pixelSize: 13
            font.bold: true
        }

        TextField {
            id: licenseInput
            Layout.fillWidth: true
            implicitHeight: 32
            placeholderText: qsTr("Paste your license key here...")
            font.pixelSize: 12
            color: textColor
            background: Rectangle { 
                color: bgColor
                border.color: _themeManager.borderColor
                radius: 4 
            }
        }

        Label {
            id: statusLabel
            text: ""
            font.pixelSize: 12
            color: "red"
            visible: text !== ""
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            
            Button {
                text: qsTr("Get License")
                implicitHeight: 32
                onClicked: _appController.openUrl("https://bigfileviewer.com/buy")
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: _themeManager.borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    rightPadding: 12
                }
            }
            Button {
                text: qsTr("Activate")
                implicitHeight: 32
                onClicked: {
                    if (_appController.activateLicense(licenseInput.text)) {
                        statusLabel.text = qsTr("Activation Successful!")
                        statusLabel.color = "#4CAF50"
                        root.close()
                    } else {
                        statusLabel.text = qsTr("Invalid License Key")
                        statusLabel.color = "#F44336"
                    }
                }
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 16
                    rightPadding: 16
                }
            }
            Button {
                text: qsTr("Close")
                implicitHeight: 32
                onClicked: root.close()
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: _themeManager.borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    rightPadding: 12
                }
            }
        }
    }
}

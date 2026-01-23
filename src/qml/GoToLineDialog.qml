import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    modal: true
    width: 300
    height: 160
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    
    // Output property
    property int targetLine: -1
    
    signal accepted()

    background: Rectangle {
        color: panelColor
        border.color: _themeManager.borderColor
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Title
        Text {
            text: qsTr("Go To Line")
            font.bold: true
            font.pixelSize: 16
            color: textColor
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: qsTr("Line Number:") + " (1 - " + _logModel.totalLineCount().toLocaleString() + ")"
            color: textColor
            font.pixelSize: 13
        }

        TextField {
            id: lineInput
            Layout.fillWidth: true
            implicitHeight: 32
            placeholderText: qsTr("Enter line number...")
            font.pixelSize: 12
            color: textColor
            background: Rectangle { 
                color: bgColor
                border.color: _themeManager.borderColor
                radius: 4 
            }
            validator: IntValidator { bottom: 1; top: _logModel.totalLineCount() }
            focus: true
            onAccepted: {
                var line = parseInt(lineInput.text)
                if (!isNaN(line)) {
                    targetLine = line
                    root.accepted()
                    root.close()
                }
            }
        }
        
        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            
            Button {
                text: qsTr("OK")
                implicitHeight: 30
                implicitWidth: 70
                onClicked: {
                    var line = parseInt(lineInput.text)
                    if (!isNaN(line)) {
                        targetLine = line
                        root.accepted()
                        root.close()
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
                }
            }
            Button {
                text: qsTr("Cancel")
                implicitHeight: 30
                implicitWidth: 70
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
                }
            }
        }
    }
    
    onOpened: {
        lineInput.text = ""
        lineInput.forceActiveFocus()
    }
}

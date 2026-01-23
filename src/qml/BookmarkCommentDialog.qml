import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    modal: true
    width: 400
    height: 200
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    
    // Input/Output properties
    property int viewRow: -1
    property string initialComment: ""
    property string linePreview: ""
    
    signal commentSaved(int row, string comment)

    background: Rectangle {
        color: panelColor
        border.color: _themeManager.borderColor
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Title
        Text {
            text: qsTr("Edit Bookmark Comment")
            font.bold: true
            font.pixelSize: 16
            color: textColor
            Layout.alignment: Qt.AlignHCenter
        }

        // Line preview
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: Qt.darker(panelColor, 1.1)
            radius: 4
            clip: true
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8
                
                Rectangle {
                    width: 3
                    height: parent.height - 8
                    color: accentColor
                    radius: 1
                }
                
                Text {
                    Layout.fillWidth: true
                    text: root.linePreview || qsTr("Line %1").arg(viewRow + 1)
                    color: Qt.darker(textColor, 1.2)
                    font.pixelSize: 11
                    font.family: "Consolas"
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        // Comment label
        Label {
            text: qsTr("Comment:")
            color: textColor
            font.pixelSize: 13
        }

        // Comment input
        TextField {
            id: commentInput
            Layout.fillWidth: true
            implicitHeight: 36
            placeholderText: qsTr("Enter bookmark note...")
            font.pixelSize: 12
            color: textColor
            background: Rectangle { 
                color: bgColor
                border.color: commentInput.activeFocus ? accentColor : _themeManager.borderColor
                border.width: commentInput.activeFocus ? 2 : 1
                radius: 4 
            }
            focus: true
            onAccepted: saveAndClose()
        }
        
        Item { Layout.fillHeight: true }

        // Buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            
            Button {
                text: qsTr("Remove Bookmark")
                implicitHeight: 30
                implicitWidth: 120
                visible: _logModel.isBookmarked(root.viewRow)
                onClicked: {
                    _logModel.toggleBookmark(root.viewRow)
                    root.close()
                }
                background: Rectangle {
                    color: parent.down ? Qt.darker("#e74c3c", 1.2) : (parent.hovered ? "#e74c3c" : panelColor)
                    border.color: "#e74c3c"
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.hovered ? "white" : "#e74c3c"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Save")
                implicitHeight: 30
                implicitWidth: 70
                onClicked: saveAndClose()
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
    
    function saveAndClose() {
        _logModel.setBookmarkComment(root.viewRow, commentInput.text)
        root.commentSaved(root.viewRow, commentInput.text)
        root.close()
    }
    
    function openForRow(row, preview) {
        root.viewRow = row
        root.linePreview = preview || ""
        root.initialComment = _logModel.getBookmarkComment(row)
        commentInput.text = root.initialComment
        root.open()
        commentInput.forceActiveFocus()
        commentInput.selectAll()
    }
    
    onOpened: {
        commentInput.forceActiveFocus()
    }
}

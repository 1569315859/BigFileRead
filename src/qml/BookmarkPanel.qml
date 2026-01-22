import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: _themeManager.panelBackground
    
    property color textColor: _themeManager.textColor
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor
    
    property var bookmarkModel: []
    
    signal bookmarkClicked(int viewRow)
    signal editBookmark(int viewRow, string preview)
    signal removeBookmark(int viewRow)
    
    // Refresh bookmarks from model
    function refresh() {
        bookmarkModel = _logModel.getAllBookmarks()
    }
    
    Connections {
        target: _logModel
        function onBookmarksChanged() {
            root.refresh()
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Text {
                text: qsTr("Bookmarks")
                font.bold: true
                font.pixelSize: 14
                color: textColor
            }
            
            Text {
                text: "(" + bookmarkModel.length + ")"
                font.pixelSize: 12
                color: Qt.darker(textColor, 1.3)
            }
            
            Item { Layout.fillWidth: true }
            
            // Save button
            Button {
                implicitWidth: 28
                implicitHeight: 28
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Export Bookmarks")
                onClicked: {
                    // TODO: Open file save dialog
                    _logModel.autoSaveBookmarks()
                }
                background: Rectangle {
                    color: parent.hovered ? Qt.darker(bgColor, 1.1) : "transparent"
                    radius: 4
                }
                contentItem: Text {
                    text: "💾"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            // Clear all button
            Button {
                implicitWidth: 28
                implicitHeight: 28
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Clear All Bookmarks")
                enabled: bookmarkModel.length > 0
                onClicked: clearConfirmDialog.open()
                background: Rectangle {
                    color: parent.hovered ? Qt.darker(bgColor, 1.1) : "transparent"
                    radius: 4
                }
                contentItem: Text {
                    text: "🗑️"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }
        }
        
        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }
        
        // Empty state
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: bookmarkModel.length === 0
            
            Column {
                anchors.centerIn: parent
                spacing: 8
                
                Text {
                    text: "📑"
                    font.pixelSize: 32
                    anchors.horizontalCenter: parent.horizontalCenter
                    opacity: 0.5
                }
                
                Text {
                    text: qsTr("No bookmarks yet")
                    color: Qt.darker(textColor, 1.3)
                    font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: qsTr("Press Ctrl+B to add a bookmark")
                    color: Qt.darker(textColor, 1.5)
                    font.pixelSize: 11
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
        
        // Bookmark list
        ListView {
            id: bookmarkList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: bookmarkModel.length > 0
            clip: true
            model: bookmarkModel
            spacing: 4
            
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            
            delegate: Rectangle {
                width: bookmarkList.width - 12
                height: contentColumn.height + 12
                color: mouseArea.containsMouse ? Qt.darker(bgColor, 1.05) : bgColor
                radius: 4
                border.color: mouseArea.containsMouse ? accentColor : borderColor
                border.width: mouseArea.containsMouse ? 1 : 0
                
                property var bookmarkData: modelData
                
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (bookmarkData.viewRow >= 0) {
                            root.bookmarkClicked(bookmarkData.viewRow)
                        }
                    }
                    onDoubleClicked: {
                        if (bookmarkData.viewRow >= 0) {
                            root.editBookmark(bookmarkData.viewRow, bookmarkData.preview || "")
                        }
                    }
                }
                
                ColumnLayout {
                    id: contentColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 4
                    
                    // Line number and actions row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        
                        // Bookmark icon
                        Text {
                            text: "🔖"
                            font.pixelSize: 12
                        }
                        
                        // Line number
                        Text {
                            text: qsTr("Line %1").arg(bookmarkData.displayLine || "?")
                            font.bold: true
                            font.pixelSize: 12
                            color: accentColor
                        }
                        
                        // Hidden indicator
                        Text {
                            visible: bookmarkData.viewRow < 0
                            text: qsTr("(filtered)")
                            font.pixelSize: 10
                            color: Qt.darker(textColor, 1.5)
                            font.italic: true
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        // Edit button
                        Button {
                            implicitWidth: 22
                            implicitHeight: 22
                            visible: mouseArea.containsMouse && bookmarkData.viewRow >= 0
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Edit Comment")
                            onClicked: root.editBookmark(bookmarkData.viewRow, bookmarkData.preview || "")
                            background: Rectangle {
                                color: parent.hovered ? Qt.darker(bgColor, 1.2) : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: "✏️"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        
                        // Delete button
                        Button {
                            implicitWidth: 22
                            implicitHeight: 22
                            visible: mouseArea.containsMouse
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Remove Bookmark")
                            onClicked: {
                                if (bookmarkData.viewRow >= 0) {
                                    _logModel.toggleBookmark(bookmarkData.viewRow)
                                }
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#e74c3c" : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: "×"
                                font.pixelSize: 14
                                font.bold: true
                                color: parent.hovered ? "white" : textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    
                    // Comment (if any)
                    Text {
                        Layout.fillWidth: true
                        visible: bookmarkData.comment && bookmarkData.comment.length > 0
                        text: bookmarkData.comment || ""
                        color: textColor
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    
                    // Line preview
                    Text {
                        Layout.fillWidth: true
                        visible: bookmarkData.preview && bookmarkData.preview.length > 0
                        text: bookmarkData.preview || ""
                        color: Qt.darker(textColor, 1.4)
                        font.pixelSize: 10
                        font.family: "Consolas"
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                    
                    // Created time
                    Text {
                        visible: bookmarkData.createdAt && bookmarkData.createdAt.length > 0
                        text: formatDateTime(bookmarkData.createdAt)
                        color: Qt.darker(textColor, 1.6)
                        font.pixelSize: 9
                    }
                }
            }
        }
    }
    
    // Clear confirmation dialog
    Popup {
        id: clearConfirmDialog
        modal: true
        width: 280
        height: 120
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        
        background: Rectangle {
            color: _themeManager.panelBackground
            border.color: borderColor
            radius: 6
        }
        
        contentItem: ColumnLayout {
            spacing: 16
            
            Text {
                text: qsTr("Clear all bookmarks?")
                font.bold: true
                font.pixelSize: 14
                color: textColor
                Layout.alignment: Qt.AlignHCenter
            }
            
            Text {
                text: qsTr("This action cannot be undone.")
                font.pixelSize: 12
                color: Qt.darker(textColor, 1.3)
                Layout.alignment: Qt.AlignHCenter
            }
            
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10
                
                Button {
                    text: qsTr("Clear All")
                    implicitHeight: 28
                    implicitWidth: 80
                    onClicked: {
                        _logModel.clearAllBookmarks()
                        clearConfirmDialog.close()
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker("#e74c3c", 1.2) : "#e74c3c"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Button {
                    text: qsTr("Cancel")
                    implicitHeight: 28
                    implicitWidth: 70
                    onClicked: clearConfirmDialog.close()
                    background: Rectangle {
                        color: parent.down ? Qt.darker(bgColor, 1.3) : bgColor
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
    
    // Helper function to format date time
    function formatDateTime(isoString) {
        if (!isoString) return ""
        var date = new Date(isoString)
        var now = new Date()
        var diff = now - date
        
        // Less than 1 hour
        if (diff < 3600000) {
            var mins = Math.floor(diff / 60000)
            return mins <= 1 ? qsTr("Just now") : qsTr("%1 minutes ago").arg(mins)
        }
        // Less than 24 hours
        if (diff < 86400000) {
            var hours = Math.floor(diff / 3600000)
            return hours === 1 ? qsTr("1 hour ago") : qsTr("%1 hours ago").arg(hours)
        }
        // Less than 7 days
        if (diff < 604800000) {
            var days = Math.floor(diff / 86400000)
            return days === 1 ? qsTr("Yesterday") : qsTr("%1 days ago").arg(days)
        }
        // Otherwise show date
        return date.toLocaleDateString()
    }
    
    Component.onCompleted: refresh()
}

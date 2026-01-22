import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

Popup {
    id: root
    modal: true
    width: 550
    height: 450
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor
    
    signal fileSelected(string filePath)

    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 6
    }
    
    // Connections to DirectoryWatcher
    Connections {
        target: _directoryWatcher
        
        function onNewFileDetected(filePath) {
            // 可以弹出通知或自动加载
            console.log("New file detected:", filePath)
        }
        
        function onWatchedFilesChanged() {
            fileListModel.clear()
            var files = _directoryWatcher.watchedFiles
            for (var i = 0; i < files.length; i++) {
                fileListModel.append(files[i])
            }
        }
    }
    
    ListModel {
        id: fileListModel
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Title
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: qsTr("Directory Monitor")
                font.bold: true
                font.pixelSize: 16
                color: textColor
            }
            
            Item { Layout.fillWidth: true }
            
            // Watching indicator
            Rectangle {
                visible: _directoryWatcher.isWatching
                width: 8
                height: 8
                radius: 4
                color: "#27ae60"
                
                SequentialAnimation on opacity {
                    running: _directoryWatcher.isWatching
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 800 }
                    NumberAnimation { to: 1.0; duration: 800 }
                }
            }
            
            Text {
                visible: _directoryWatcher.isWatching
                text: qsTr("Watching")
                font.pixelSize: 11
                color: "#27ae60"
            }
        }

        // Directory selection
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            TextField {
                id: directoryInput
                Layout.fillWidth: true
                implicitHeight: 32
                text: _directoryWatcher.watchedDirectory
                placeholderText: qsTr("Select a directory to monitor...")
                font.pixelSize: 12
                color: textColor
                readOnly: true
                background: Rectangle {
                    color: bgColor
                    border.color: borderColor
                    radius: 4
                }
            }
            
            Button {
                text: qsTr("Browse")
                implicitHeight: 32
                implicitWidth: 80
                onClicked: folderDialog.open()
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
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
        
        // Filter settings
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Label {
                text: qsTr("Filter:")
                color: textColor
                font.pixelSize: 12
            }
            
            TextField {
                id: filterInput
                Layout.fillWidth: true
                implicitHeight: 28
                text: _directoryWatcher.fileFilter.join(", ")
                placeholderText: "*.log, *.txt"
                font.pixelSize: 11
                color: textColor
                background: Rectangle {
                    color: bgColor
                    border.color: borderColor
                    radius: 4
                }
                onEditingFinished: {
                    var filters = text.split(",").map(function(f) { return f.trim() })
                    _directoryWatcher.fileFilter = filters
                }
            }
            
            Label {
                text: qsTr("Interval:")
                color: textColor
                font.pixelSize: 12
            }
            
            SpinBox {
                id: intervalSpinBox
                implicitWidth: 100
                implicitHeight: 28
                from: 1
                to: 60
                value: _directoryWatcher.pollInterval / 1000
                onValueChanged: _directoryWatcher.pollInterval = value * 1000
                
                contentItem: TextInput {
                    text: parent.textFromValue(parent.value, parent.locale) + "s"
                    font.pixelSize: 11
                    color: textColor
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    readOnly: !parent.editable
                    validator: parent.validator
                }
            }
        }
        
        // Control buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Button {
                text: _directoryWatcher.isWatching ? qsTr("Stop Watching") : qsTr("Start Watching")
                implicitHeight: 32
                implicitWidth: 130
                enabled: directoryInput.text.length > 0 || _directoryWatcher.isWatching
                onClicked: {
                    if (_directoryWatcher.isWatching) {
                        _directoryWatcher.stopWatching()
                    } else {
                        _directoryWatcher.startWatching(directoryInput.text)
                    }
                }
                background: Rectangle {
                    color: _directoryWatcher.isWatching 
                        ? (parent.down ? Qt.darker("#e74c3c", 1.2) : "#e74c3c")
                        : (parent.down ? Qt.darker(accentColor, 1.2) : accentColor)
                    radius: 4
                    opacity: parent.enabled ? 1.0 : 0.5
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
                text: qsTr("Refresh")
                implicitHeight: 32
                implicitWidth: 80
                enabled: _directoryWatcher.isWatching
                onClicked: _directoryWatcher.refresh()
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
                    radius: 4
                    opacity: parent.enabled ? 1.0 : 0.5
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Open Latest")
                implicitHeight: 32
                implicitWidth: 100
                enabled: fileListModel.count > 0
                onClicked: {
                    var latestFile = _directoryWatcher.getLatestFile()
                    if (latestFile) {
                        root.fileSelected(latestFile)
                        root.close()
                    }
                }
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : (parent.enabled ? accentColor : panelColor)
                    border.color: parent.enabled ? "transparent" : borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "white" : textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: parent.enabled ? 1.0 : 0.5
                }
            }
        }
        
        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }
        
        // File list header
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: qsTr("Files in Directory")
                font.bold: true
                font.pixelSize: 13
                color: textColor
            }
            
            Text {
                text: "(" + fileListModel.count + ")"
                font.pixelSize: 12
                color: Qt.darker(textColor, 1.3)
            }
        }
        
        // File list
        ListView {
            id: fileListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: fileListModel
            spacing: 2
            
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            
            delegate: Rectangle {
                width: fileListView.width - 12
                height: 50
                color: mouseArea.containsMouse ? Qt.darker(bgColor, 1.05) : bgColor
                radius: 4
                border.color: model.isNew ? accentColor : (mouseArea.containsMouse ? borderColor : "transparent")
                border.width: model.isNew ? 2 : 1
                
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: {
                        root.fileSelected(model.filePath)
                        root.close()
                    }
                }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 10
                    
                    // File icon
                    Text {
                        text: model.isNew ? "🆕" : "📄"
                        font.pixelSize: 16
                    }
                    
                    // File info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        Text {
                            Layout.fillWidth: true
                            text: model.fileName
                            font.bold: true
                            font.pixelSize: 12
                            color: textColor
                            elide: Text.ElideMiddle
                        }
                        
                        Text {
                            Layout.fillWidth: true
                            text: formatFileSize(model.size) + " • " + formatDateTime(model.lastModified)
                            font.pixelSize: 10
                            color: Qt.darker(textColor, 1.4)
                        }
                    }
                    
                    // Open button
                    Button {
                        implicitWidth: 60
                        implicitHeight: 26
                        text: qsTr("Open")
                        visible: mouseArea.containsMouse
                        onClicked: {
                            root.fileSelected(model.filePath)
                            root.close()
                        }
                        background: Rectangle {
                            color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
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
                }
            }
            
            // Empty state
            Item {
                anchors.fill: parent
                visible: fileListModel.count === 0
                
                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    
                    Text {
                        text: "📂"
                        font.pixelSize: 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        opacity: 0.5
                    }
                    
                    Text {
                        text: _directoryWatcher.isWatching 
                            ? qsTr("No matching files found")
                            : qsTr("Select a directory to start")
                        color: Qt.darker(textColor, 1.3)
                        font.pixelSize: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
        
        // Bottom buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            
            Button {
                text: qsTr("Close")
                implicitHeight: 32
                implicitWidth: 80
                onClicked: root.close()
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
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
    
    // Folder dialog
    Platform.FolderDialog {
        id: folderDialog
        title: qsTr("Select Directory to Monitor")
        
        onAccepted: {
            var path = _appController.urlToLocalPath(folder)
            directoryInput.text = path
            _directoryWatcher.startWatching(path)
        }
    }
    
    // Helper functions
    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
    }
    
    function formatDateTime(isoString) {
        if (!isoString) return ""
        var date = new Date(isoString)
        return date.toLocaleString()
    }
    
    onOpened: {
        // Refresh file list on open
        fileListModel.clear()
        var files = _directoryWatcher.watchedFiles
        for (var i = 0; i < files.length; i++) {
            fileListModel.append(files[i])
        }
    }
}

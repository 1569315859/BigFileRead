import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    width: 500
    height: 400
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    
    property color textColor: _themeManager ? _themeManager.textColor : palette.text
    property color panelColor: _themeManager ? _themeManager.panelBackground : palette.window
    property color bgColor: _themeManager ? _themeManager.backgroundColor : palette.base
    property color accentColor: _themeManager ? _themeManager.accentColor : palette.highlight
    property color borderColor: _themeManager ? _themeManager.borderColor : palette.mid
    
    palette.text: textColor
    palette.windowText: textColor
    palette.window: panelColor
    palette.base: bgColor
    palette.highlight: accentColor
    palette.buttonText: textColor
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: _updateChecker.updateAvailable ? qsTr("Update Available") : qsTr("Check for Updates")
                font.pixelSize: 18
                font.bold: true
                color: textColor
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: "X"
                flat: true
                onClicked: root.close()
            }
        }
        
        // Current version info
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8
            
            Label { text: qsTr("Current Version:"); color: Qt.darker(textColor, 1.3) }
            Label { text: _updateChecker.currentVersion; color: "#fff" }
            
            Label { text: qsTr("Latest Version:"); color: "#888"; visible: _updateChecker.updateAvailable }
            Label { text: _updateChecker.latestVersion; color: "#4fc3f7"; visible: _updateChecker.updateAvailable; font.bold: true }
        }
        
        // Update available section
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#252525"
            radius: 4
            visible: _updateChecker.updateAvailable
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: qsTr("Release Notes:")
                    font.bold: true
                    color: "#aaa"
                }
                
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    TextArea {
                        readOnly: true
                        text: _updateChecker.releaseNotes
                        color: "#ddd"
                        background: null
                        wrapMode: TextArea.Wrap
                    }
                }
            }
        }
        
        // Checking indicator
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: _updateChecker.checking && !_updateChecker.updateAvailable
            
            Item { Layout.fillHeight: true }
            
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: _updateChecker.checking
            }
            
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Checking for updates...")
                color: "#888"
            }
            
            Item { Layout.fillHeight: true }
        }
        
        // No update available
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !_updateChecker.checking && !_updateChecker.updateAvailable && _updateChecker.latestVersion !== ""
            
            Item { Layout.fillHeight: true }
            
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("You are using the latest version.")
                color: "#81c784"
                font.pixelSize: 14
            }
            
            Item { Layout.fillHeight: true }
        }
        
        // Download progress
        ColumnLayout {
            Layout.fillWidth: true
            visible: _updateChecker.downloadProgress > 0 && _updateChecker.downloadProgress < 100
            spacing: 4
            
            ProgressBar {
                Layout.fillWidth: true
                value: _updateChecker.downloadProgress / 100
            }
            
            Label {
                text: qsTr("Downloading: %1%").arg(_updateChecker.downloadProgress)
                color: "#888"
            }
        }
        
        // Settings
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Settings")
            visible: !_updateChecker.updateAvailable
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                
                CheckBox {
                    text: qsTr("Check for updates automatically")
                    checked: _updateChecker.autoCheckEnabled
                    onCheckedChanged: _updateChecker.autoCheckEnabled = checked
                }
                
                RowLayout {
                    enabled: _updateChecker.autoCheckEnabled
                    
                    Label { text: qsTr("Check every"); color: enabled ? "#fff" : "#666" }
                    SpinBox {
                        from: 1
                        to: 30
                        value: _updateChecker.checkIntervalDays
                        onValueChanged: _updateChecker.checkIntervalDays = value
                    }
                    Label { text: qsTr("days"); color: enabled ? "#fff" : "#666" }
                }
                
                CheckBox {
                    text: qsTr("Include pre-release versions")
                    checked: _updateChecker.includePreReleases
                    onCheckedChanged: _updateChecker.includePreReleases = checked
                }
            }
        }
        
        // Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            Button {
                text: qsTr("Check Now")
                visible: !_updateChecker.updateAvailable
                enabled: !_updateChecker.checking
                onClicked: _updateChecker.checkForUpdates()
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Skip This Version")
                visible: _updateChecker.updateAvailable
                onClicked: {
                    _updateChecker.skipVersion(_updateChecker.latestVersion)
                    root.close()
                }
            }
            
            Button {
                text: qsTr("Download")
                visible: _updateChecker.updateAvailable && _updateChecker.downloadProgress === 0
                highlighted: true
                onClicked: _updateChecker.downloadUpdate()
            }
            
            Button {
                text: qsTr("Install")
                visible: _updateChecker.downloadProgress === 100
                highlighted: true
                onClicked: _updateChecker.installUpdate()
            }
            
            Button {
                text: qsTr("Cancel")
                visible: _updateChecker.downloadProgress > 0 && _updateChecker.downloadProgress < 100
                onClicked: _updateChecker.cancelDownload()
            }
        }
    }
    
    Connections {
        target: _updateChecker
        
        function onCheckFailed(error) {
            errorLabel.text = error
            errorLabel.visible = true
        }
        
        function onDownloadFailed(error) {
            errorLabel.text = error
            errorLabel.visible = true
        }
    }
    
    // Error label (hidden by default)
    Label {
        id: errorLabel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        color: "#ff5252"
        visible: false
        wrapMode: Text.Wrap
    }
}

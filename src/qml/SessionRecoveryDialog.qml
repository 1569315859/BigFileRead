import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @file SessionRecoveryDialog.qml
 * @brief Session recovery prompt dialog - shown on startup when crash recovery is available
 */
Dialog {
    id: root
    title: qsTr("Session Recovery")
    modal: true
    width: 500
    height: 320
    anchors.centerIn: parent
    closePolicy: Popup.NoAutoClose
    
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
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    // Recovery info
    property string recoveryInfo: _sessionRecovery ? _sessionRecovery.recoveryInfo : ""
    property var sessionData: null
    
    signal recoveryAccepted()
    signal recoveryDiscarded()
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        
        // Icon and title
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            
            Rectangle {
                width: 56
                height: 56
                radius: 28
                color: "#FFF3E0"
                
                Text {
                    anchors.centerIn: parent
                    text: "⚠️"
                    font.pixelSize: 28
                }
            }
            
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                
                Label {
                    text: qsTr("Recover Previous Session?")
                    font.pixelSize: 18
                    font.bold: true
                    color: textColor
                }
                
                Label {
                    text: qsTr("BigFileViewer closed unexpectedly. Would you like to recover your previous session?")
                    font.pixelSize: 13
                    color: Qt.darker(textColor, 1.3)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        
        // Session info
        GroupBox {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("Session Information")
            
            background: Rectangle {
                color: Qt.darker(panelColor, 1.05)
                border.color: borderColor
                radius: 4
                y: parent.topPadding - parent.bottomPadding
                height: parent.height - parent.topPadding + parent.bottomPadding
            }
            
            ScrollView {
                anchors.fill: parent
                clip: true
                
                TextArea {
                    text: root.recoveryInfo || qsTr("No session details available")
                    color: textColor
                    font.family: "Consolas"
                    font.pixelSize: 12
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }
        }
        
        // Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Discard")
                implicitWidth: 100
                implicitHeight: 36
                
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    _sessionRecovery.discardRecovery()
                    root.recoveryDiscarded()
                    root.close()
                }
            }
            
            Button {
                text: qsTr("Recover Session")
                implicitWidth: 140
                implicitHeight: 36
                highlighted: true
                
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    _sessionRecovery.recoverSession()
                    root.recoveryAccepted()
                    root.close()
                }
            }
        }
    }
    
    // Connect to session recovery signals
    Connections {
        target: _sessionRecovery
        
        function onSessionRecovered(sessionData) {
            console.log("[SessionRecoveryDialog] Session recovered successfully")
        }
        
        function onRecoveryFailed(error) {
            console.error("[SessionRecoveryDialog] Recovery failed:", error)
        }
        
        function onRequestOpenFile(filePath) {
            console.log("[SessionRecoveryDialog] Request to open file:", filePath)
            _tabManager.openTab(filePath)
        }
        
        function onRequestScrollTo(filePath, lineNumber) {
            console.log("[SessionRecoveryDialog] Request to scroll to line:", lineNumber)
        }
        
        function onRequestRestoreBookmark(filePath, lineNumber, comment) {
            console.log("[SessionRecoveryDialog] Request to restore bookmark at line:", lineNumber)
        }
        
        function onRequestApplyFilter(filter) {
            console.log("[SessionRecoveryDialog] Request to apply filter:", filter)
        }
    }
}

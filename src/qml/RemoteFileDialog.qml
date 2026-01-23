import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

/**
 * @brief Remote File Browser Dialog
 * 
 * Enterprise feature: Open log files from remote servers via libssh2/SFTP
 * No external SSH tools required - uses built-in libssh2 library
 */
Dialog {
    id: root
    
    title: qsTr("Remote File Browser")
    width: 650
    height: 550
    modal: true
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape  // Don't close on outside click
    
    // Remote file manager reference
    property var remoteManager: null
    
    // Signal when file is ready to open
    signal fileSelected(string localPath, string remotePath)
    
    contentItem: ColumnLayout {
        spacing: 12
        
        // ===== Library Info =====
        Label {
            visible: root.remoteManager
            text: "✓ " + (root.remoteManager ? root.remoteManager.libraryVersion : "")
            font.pixelSize: 11
            color: "#4CAF50"
            opacity: 0.8
        }
        
        // ===== Connection Bar =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            ComboBox {
                id: profileCombo
                Layout.preferredWidth: 180
                model: ListModel { id: profileModel }
                textRole: "name"
                
                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        var profile = profileModel.get(currentIndex)
                        if (profile && profile.name !== "(New Connection)") {
                            hostField.text = profile.host
                            portField.text = profile.port.toString()
                            usernameField.text = profile.username
                            authMethodCombo.currentIndex = profile.authMethod === "password" ? 1 : 0
                            keyPathField.text = profile.keyPath || ""
                        }
                    }
                }
            }
            
            TextField {
                id: hostField
                Layout.fillWidth: true
                placeholderText: qsTr("hostname or IP")
            }
            
            TextField {
                id: portField
                Layout.preferredWidth: 60
                text: "22"
                placeholderText: "22"
                validator: IntValidator { bottom: 1; top: 65535 }
            }
            
            TextField {
                id: usernameField
                Layout.preferredWidth: 100
                placeholderText: qsTr("user")
            }
            
            Button {
                id: connectBtn
                text: root.remoteManager && root.remoteManager.isConnected 
                      ? qsTr("Disconnect") : qsTr("Connect")
                highlighted: !(root.remoteManager && root.remoteManager.isConnected)
                enabled: !root.remoteManager || !root.remoteManager.loading  // Disable while loading
                onClicked: {
                    if (root.remoteManager && root.remoteManager.isConnected) {
                        root.remoteManager.disconnect()
                    } else {
                        connectToServer()
                    }
                }
            }
        }
        
        // ===== Authentication Row =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Label { text: qsTr("Auth:") }
            
            ComboBox {
                id: authMethodCombo
                Layout.preferredWidth: 120
                model: [qsTr("SSH Key"), qsTr("Password")]
            }
            
            TextField {
                id: keyPathField
                Layout.fillWidth: true
                visible: authMethodCombo.currentIndex === 0
                placeholderText: qsTr("Path to private key (~/.ssh/id_rsa)")
                text: "~/.ssh/id_rsa"
            }
            
            Button {
                text: "..."
                visible: authMethodCombo.currentIndex === 0
                implicitWidth: 36
                onClicked: keyFileDialog.open()
            }
            
            TextField {
                id: passwordField
                Layout.fillWidth: true
                visible: authMethodCombo.currentIndex === 1
                echoMode: TextInput.Password
                placeholderText: qsTr("Password")
            }
            
            Button {
                text: qsTr("Save Profile")
                flat: true
                onClicked: saveProfileDialog.open()
            }
        }
        
        // ===== Path Bar =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            
            Button {
                text: "⬆"
                implicitWidth: 36
                enabled: root.remoteManager && root.remoteManager.isConnected
                onClicked: root.remoteManager.navigateUp()
                ToolTip.text: qsTr("Parent Directory")
                ToolTip.visible: hovered
            }
            
            Button {
                text: "🏠"
                implicitWidth: 36
                enabled: root.remoteManager && root.remoteManager.isConnected
                onClicked: root.remoteManager.listDirectory("~")
                ToolTip.text: qsTr("Home Directory")
                ToolTip.visible: hovered
            }
            
            TextField {
                id: pathField
                Layout.fillWidth: true
                text: root.remoteManager ? root.remoteManager.currentPath : ""
                placeholderText: "/path/to/logs"
                
                onAccepted: {
                    if (root.remoteManager) {
                        root.remoteManager.listDirectory(text)
                    }
                }
            }
            
            Button {
                text: "🔄"
                implicitWidth: 36
                enabled: root.remoteManager && root.remoteManager.isConnected
                onClicked: root.remoteManager.listDirectory()
                ToolTip.text: qsTr("Refresh")
                ToolTip.visible: hovered
            }
        }
        
        // ===== File List =====
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: _themeManager.backgroundColor
            border.color: _themeManager.borderColor
            radius: 4
            
            ListView {
                id: fileListView
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                
                model: ListModel { id: fileModel }
                
                delegate: ItemDelegate {
                    width: fileListView.width
                    height: 32
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 8
                        
                        Label {
                            text: model.isDir ? "📁" : (model.isLink ? "🔗" : "📄")
                            font.pixelSize: 16
                        }
                        
                        Label {
                            text: model.name
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                            color: model.isDir ? _themeManager.linkColor : _themeManager.textColor
                            font.bold: model.isDir
                        }
                        
                        Label {
                            text: model.isDir ? "" : formatSize(model.size)
                            font.pixelSize: 11
                            opacity: 0.7
                            Layout.preferredWidth: 70
                            horizontalAlignment: Text.AlignRight
                        }
                        
                        Label {
                            text: model.modified || ""
                            font.pixelSize: 11
                            opacity: 0.6
                            Layout.preferredWidth: 100
                        }
                    }
                    
                    onClicked: {
                        fileListView.currentIndex = index
                    }
                    
                    onDoubleClicked: {
                        if (model.isDir) {
                            root.remoteManager.navigateTo(model.name)
                        } else {
                            openSelectedFile()
                        }
                    }
                }
                
                highlight: Rectangle {
                    color: _themeManager.highlightColor
                    opacity: 0.3
                }
                
                ScrollBar.vertical: ScrollBar {}
            }
            
            // Empty state
            Label {
                anchors.centerIn: parent
                text: root.remoteManager && root.remoteManager.isConnected 
                      ? qsTr("Directory is empty")
                      : qsTr("Connect to a server to browse files")
                opacity: 0.5
                visible: fileModel.count === 0 && !(root.remoteManager && root.remoteManager.loading)
            }
            
            // Loading indicator
            BusyIndicator {
                anchors.centerIn: parent
                running: root.remoteManager && root.remoteManager.loading
                visible: running
            }
        }
        
        // ===== Auto-refresh Row =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            CheckBox {
                id: autoRefreshCheck
                text: qsTr("Auto-refresh")
                checked: root.remoteManager ? root.remoteManager.autoRefreshEnabled : false
                onCheckedChanged: {
                    if (root.remoteManager) {
                        root.remoteManager.autoRefreshEnabled = checked
                    }
                }
            }
            
            Label { 
                text: qsTr("every")
                enabled: autoRefreshCheck.checked
            }
            
            SpinBox {
                id: refreshIntervalSpin
                from: 1
                to: 60
                value: root.remoteManager ? root.remoteManager.autoRefreshInterval : 5
                enabled: autoRefreshCheck.checked
                
                onValueChanged: {
                    if (root.remoteManager) {
                        root.remoteManager.autoRefreshInterval = value
                    }
                }
            }
            
            Label { 
                text: qsTr("seconds")
                enabled: autoRefreshCheck.checked
            }
        }
        
        // ===== Status Area =====
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: statusLabel.implicitHeight > 40 ? 60 : 20
            Layout.maximumHeight: 80
            clip: true
            
            Label {
                id: statusLabel
                width: parent.width
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        
        // ===== Buttons =====
        RowLayout {
            Layout.fillWidth: true
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            
            Button {
                text: qsTr("Open")
                highlighted: true
                enabled: fileListView.currentIndex >= 0 && 
                         fileModel.count > 0 &&
                         !fileModel.get(fileListView.currentIndex).isDir
                
                onClicked: openSelectedFile()
            }
        }
    }
    
    // ===== Save Profile Dialog =====
    Dialog {
        id: saveProfileDialog
        title: qsTr("Save Connection Profile")
        width: 300
        height: 150
        modal: true
        anchors.centerIn: parent
        
        contentItem: ColumnLayout {
            spacing: 12
            
            TextField {
                id: profileNameField
                Layout.fillWidth: true
                placeholderText: qsTr("Profile name")
            }
            
            RowLayout {
                Layout.fillWidth: true
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Cancel")
                    onClicked: saveProfileDialog.close()
                }
                
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    enabled: profileNameField.text.length > 0
                    onClicked: {
                        root.remoteManager.saveProfile(
                            profileNameField.text,
                            hostField.text,
                            parseInt(portField.text) || 22,
                            usernameField.text,
                            authMethodCombo.currentIndex === 0 ? "key" : "password",
                            keyPathField.text
                        )
                        saveProfileDialog.close()
                        loadProfiles()
                    }
                }
            }
        }
    }
    
    // ===== Key File Dialog =====
    Platform.FileDialog {
        id: keyFileDialog
        title: qsTr("Select Private Key")
        nameFilters: ["All files (*)"]
        
        onAccepted: {
            keyPathField.text = file.toString().replace("file:///", "")
        }
    }
    
    function connectToServer() {
        if (!root.remoteManager) return
        
        // 先显示连接中状态
        statusLabel.text = qsTr("Connecting...")
        statusLabel.color = _themeManager.textColor
        
        var credential = authMethodCombo.currentIndex === 0 ? keyPathField.text : passwordField.text
        
        root.remoteManager.connectToServer(
            hostField.text,
            parseInt(portField.text) || 22,
            usernameField.text,
            authMethodCombo.currentIndex === 0 ? "key" : "password",
            credential
        )
    }
    
    function openSelectedFile() {
        if (fileListView.currentIndex < 0) return
        
        var file = fileModel.get(fileListView.currentIndex)
        if (file.isDir) return
        
        var remotePath = root.remoteManager.currentPath
        if (!remotePath.endsWith('/')) remotePath += '/'
        remotePath += file.name
        
        root.remoteManager.downloadAndOpen(remotePath)
        statusLabel.text = qsTr("Downloading...")
        statusLabel.color = _themeManager.textColor
    }
    
    function loadProfiles() {
        profileModel.clear()
        profileModel.append({"name": "(New Connection)", "host": "", "port": 22, "username": "", "authMethod": "key", "keyPath": ""})
        
        if (root.remoteManager) {
            var profiles = root.remoteManager.getProfiles()
            for (var i = 0; i < profiles.length; i++) {
                profileModel.append(profiles[i])
            }
        }
    }
    
    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + " MB"
        return (bytes / 1024 / 1024 / 1024).toFixed(1) + " GB"
    }
    
    Connections {
        target: root.remoteManager
        
        function onConnectionChanged() {
            if (root.remoteManager.isConnected) {
                statusLabel.text = qsTr("Connected to %1").arg(root.remoteManager.currentHost)
                statusLabel.color = "#4CAF50"
            } else {
                statusLabel.text = qsTr("Disconnected")
                statusLabel.color = _themeManager.textColor
                fileModel.clear()
            }
        }
        
        function onPathChanged() {
            pathField.text = root.remoteManager.currentPath
        }
        
        function onDirectoryListed(files) {
            fileModel.clear()
            for (var i = 0; i < files.length; i++) {
                fileModel.append(files[i])
            }
        }
        
        function onFileReady(localPath, remotePath) {
            statusLabel.text = qsTr("Downloaded: %1").arg(remotePath.split('/').pop())
            statusLabel.color = "#4CAF50"
            
            root.fileSelected(localPath, remotePath)
            root.close()
        }
        
        function onFileRefreshed(localPath) {
            // Notify that file was refreshed (for live monitoring)
            console.log("Remote file refreshed:", localPath)
        }
        
        function onConnectionTested(success, message) {
            statusLabel.text = message
            statusLabel.color = success ? "#4CAF50" : "#F44336"
        }
        
        function onErrorOccurred(error) {
            statusLabel.text = error
            statusLabel.color = "#F44336"
        }
        
        function onProfilesChanged() {
            loadProfiles()
        }
    }
    
    onOpened: {
        loadProfiles()
        statusLabel.text = ""
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief GitHub Issue Creation Dialog
 * 
 * Enterprise feature: Create GitHub issues from selected log content
 */
Dialog {
    id: root
    
    title: qsTr("Create GitHub Issue")
    width: 580
    height: 560
    modal: true
    anchors.centerIn: parent
    
    // Input properties
    property var selectedLines: []
    property var lineNumbers: []
    property string filePath: ""
    
    // GitHub integration reference
    property var githubIntegration: null
    
    contentItem: ColumnLayout {
        spacing: 12
        
        // Configuration status
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: root.githubIntegration && root.githubIntegration.isConfigured 
                   ? Qt.rgba(0.2, 0.7, 0.3, 0.1) 
                   : Qt.rgba(1, 0.5, 0, 0.1)
            radius: 4
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8
                
                Label {
                    text: root.githubIntegration && root.githubIntegration.isConfigured 
                          ? "✓ " + qsTr("Connected to GitHub") + (root.githubIntegration.username ? " (@" + root.githubIntegration.username + ")" : "")
                          : "⚠ " + qsTr("GitHub not configured")
                    color: root.githubIntegration && root.githubIntegration.isConfigured 
                           ? "#2E7D32" : "#E65100"
                }
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Settings")
                    flat: true
                    onClicked: settingsDialog.open()
                }
            }
        }
        
        // Issue form
        GroupBox {
            title: qsTr("Issue Details")
            Layout.fillWidth: true
            Layout.fillHeight: true
            enabled: root.githubIntegration && root.githubIntegration.isConfigured
            
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 12
                rowSpacing: 8
                
                Label { text: qsTr("Repository:") + " *" }
                ComboBox {
                    id: repoCombo
                    Layout.fillWidth: true
                    textRole: "display"
                    
                    model: ListModel { id: repoModel }
                    
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && root.githubIntegration) {
                            var repo = repoModel.get(currentIndex)
                            if (repo) {
                                root.githubIntegration.fetchLabels(repo.owner, repo.name)
                            }
                        }
                    }
                }
                
                Label { text: qsTr("Title:") + " *" }
                TextField {
                    id: titleField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Brief description of the issue")
                }
                
                Label { 
                    text: qsTr("Body:")
                    Layout.alignment: Qt.AlignTop
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    
                    TextArea {
                        id: bodyField
                        placeholderText: qsTr("Detailed description (log content will be appended)")
                        wrapMode: TextArea.Wrap
                    }
                }
                
                Label { text: qsTr("Labels:") }
                Flow {
                    id: labelsFlow
                    Layout.fillWidth: true
                    spacing: 4
                    
                    Repeater {
                        model: ListModel { id: labelModel }
                        
                        delegate: Button {
                            text: model.name
                            checkable: true
                            flat: true
                            padding: 4
                            
                            background: Rectangle {
                                color: checked ? "#" + model.color : "transparent"
                                border.color: "#" + model.color
                                border.width: 1
                                radius: 3
                            }
                            
                            contentItem: Text {
                                text: parent.text
                                color: parent.checked ? (isLightColor(model.color) ? "#000" : "#fff") : "#" + model.color
                                font.pixelSize: 11
                            }
                            
                            function isLightColor(hex) {
                                var r = parseInt(hex.substr(0, 2), 16)
                                var g = parseInt(hex.substr(2, 2), 16)
                                var b = parseInt(hex.substr(4, 2), 16)
                                return (r * 0.299 + g * 0.587 + b * 0.114) > 186
                            }
                        }
                    }
                    
                    Label {
                        visible: labelModel.count === 0
                        text: qsTr("Select a repository to see labels")
                        font.italic: true
                        opacity: 0.6
                    }
                }
                
                Label { text: qsTr("Assignee:") }
                ComboBox {
                    id: assigneeCombo
                    Layout.fillWidth: true
                    textRole: "login"
                    
                    model: ListModel { 
                        id: assigneeModel 
                        ListElement { login: "(none)" }
                    }
                }
            }
        }
        
        // Log preview
        GroupBox {
            title: qsTr("Log Content Preview")
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            
            ScrollView {
                anchors.fill: parent
                
                TextArea {
                    id: logPreview
                    readOnly: true
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 11
                    wrapMode: TextArea.NoWrap
                    text: root.selectedLines.join("\n")
                    color: _themeManager.textColor
                    opacity: 0.8
                }
            }
        }
        
        // Status and buttons
        RowLayout {
            Layout.fillWidth: true
            
            BusyIndicator {
                running: root.githubIntegration && root.githubIntegration.loading
                visible: running
                implicitWidth: 24
                implicitHeight: 24
            }
            
            Label {
                id: statusLabel
                Layout.fillWidth: true
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            
            Button {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            
            Button {
                text: qsTr("Create Issue")
                highlighted: true
                enabled: root.githubIntegration && 
                         root.githubIntegration.isConfigured &&
                         !root.githubIntegration.loading &&
                         repoCombo.currentIndex >= 0 &&
                         titleField.text.length > 0
                
                onClicked: createIssue()
            }
        }
    }
    
    // ===== Settings Dialog =====
    Dialog {
        id: settingsDialog
        title: qsTr("GitHub Settings")
        width: 450
        height: 320
        modal: true
        anchors.centerIn: parent
        
        contentItem: ColumnLayout {
            spacing: 12
            
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8
                
                Label { text: qsTr("GitHub URL:") }
                TextField {
                    id: baseUrlField
                    Layout.fillWidth: true
                    placeholderText: "https://api.github.com"
                    text: root.githubIntegration ? root.githubIntegration.baseUrl : ""
                }
                
                Label { }
                Label {
                    text: qsTr("For GitHub Enterprise: https://github.yourcompany.com")
                    font.pixelSize: 11
                    opacity: 0.6
                }
                
                Label { text: qsTr("Personal Access Token:") }
                RowLayout {
                    Layout.fillWidth: true
                    
                    TextField {
                        id: tokenField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: qsTr("ghp_xxxxx or github_pat_xxxxx")
                    }
                    
                    Button {
                        text: tokenField.echoMode === TextInput.Password ? "👁" : "🔒"
                        flat: true
                        implicitWidth: 36
                        onClicked: {
                            tokenField.echoMode = tokenField.echoMode === TextInput.Password 
                                ? TextInput.Normal : TextInput.Password
                        }
                    }
                }
                
                Label { }
                Label {
                    text: qsTr("Get token from: GitHub → Settings → Developer settings → Personal access tokens")
                    font.pixelSize: 11
                    opacity: 0.6
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                
                Label { }
                Label {
                    text: qsTr("Required scopes: repo (Full control of private repositories)")
                    font.pixelSize: 11
                    opacity: 0.6
                }
            }
            
            Item { Layout.fillHeight: true }
            
            RowLayout {
                Layout.fillWidth: true
                
                Label {
                    id: testStatusLabel
                    font.pixelSize: 12
                }
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Test Connection")
                    enabled: tokenField.text.length > 0
                    onClicked: {
                        saveGitHubSettings()
                        root.githubIntegration.testConnection()
                        testStatusLabel.text = qsTr("Testing...")
                        testStatusLabel.color = _themeManager.textColor
                    }
                }
            }
            
            RowLayout {
                Layout.fillWidth: true
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Cancel")
                    onClicked: settingsDialog.close()
                }
                
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    onClicked: {
                        saveGitHubSettings()
                        settingsDialog.close()
                        loadRepositories()
                    }
                }
            }
        }
    }
    
    function saveGitHubSettings() {
        if (!root.githubIntegration) return
        
        root.githubIntegration.setBaseUrl(baseUrlField.text)
        root.githubIntegration.setAccessToken(tokenField.text)
        root.githubIntegration.saveConfiguration()
    }
    
    function loadRepositories() {
        if (!root.githubIntegration || !root.githubIntegration.isConfigured) return
        
        root.githubIntegration.fetchRepositories(true)
    }
    
    function createIssue() {
        if (!root.githubIntegration) return
        
        var repo = repoModel.get(repoCombo.currentIndex)
        if (!repo) return
        
        // Format body with log content
        var body = bodyField.text
        if (body.length > 0) {
            body += "\n\n"
        }
        body += root.githubIntegration.formatLogForGitHub(
            root.selectedLines, root.filePath, root.lineNumbers)
        
        // Collect selected labels
        var labels = []
        for (var i = 0; i < labelModel.count; i++) {
            var labelBtn = labelsFlow.children[i]
            if (labelBtn && labelBtn.checked) {
                labels.push(labelModel.get(i).name)
            }
        }
        
        // Get assignee
        var assignees = []
        if (assigneeCombo.currentIndex > 0) {  // Skip "(none)"
            var assignee = assigneeModel.get(assigneeCombo.currentIndex)
            if (assignee && assignee.login !== "(none)") {
                assignees.push(assignee.login)
            }
        }
        
        root.githubIntegration.createIssue(
            repo.owner,
            repo.name,
            titleField.text,
            body,
            labels,
            assignees
        )
        
        statusLabel.text = qsTr("Creating issue...")
        statusLabel.color = _themeManager.textColor
    }
    
    Connections {
        target: root.githubIntegration
        
        function onRepositoriesFetched(repos) {
            repoModel.clear()
            for (var i = 0; i < repos.length; i++) {
                repoModel.append({
                    "owner": repos[i].owner,
                    "name": repos[i].name,
                    "fullName": repos[i].fullName,
                    "private": repos[i].private,
                    "display": repos[i].fullName + (repos[i].private ? " 🔒" : "")
                })
            }
            if (repoModel.count > 0) {
                repoCombo.currentIndex = 0
            }
        }
        
        function onLabelsFetched(owner, repo, labels) {
            labelModel.clear()
            for (var i = 0; i < labels.length; i++) {
                labelModel.append({
                    "name": labels[i].name,
                    "color": labels[i].color,
                    "description": labels[i].description
                })
            }
            
            // Also fetch collaborators
            root.githubIntegration.fetchCollaborators(owner, repo)
        }
        
        function onCollaboratorsFetched(owner, repo, collaborators) {
            assigneeModel.clear()
            assigneeModel.append({"login": "(none)"})
            for (var i = 0; i < collaborators.length; i++) {
                assigneeModel.append({
                    "login": collaborators[i].login,
                    "avatarUrl": collaborators[i].avatarUrl
                })
            }
        }
        
        function onConnectionTested(success, message) {
            testStatusLabel.text = message
            testStatusLabel.color = success ? "#4CAF50" : "#F44336"
        }
        
        function onIssueCreated(issueNumber, issueUrl) {
            statusLabel.text = qsTr("Created: #%1").arg(issueNumber)
            statusLabel.color = "#4CAF50"
            
            // Open in browser
            Qt.openUrlExternally(issueUrl)
            
            // Close dialog after short delay
            closeTimer.start()
        }
        
        function onIssueCreateFailed(error) {
            statusLabel.text = qsTr("Failed: %1").arg(error)
            statusLabel.color = "#F44336"
        }
    }
    
    Timer {
        id: closeTimer
        interval: 2000
        onTriggered: root.close()
    }
    
    onOpened: {
        statusLabel.text = ""
        loadRepositories()
        
        // Auto-generate title from first log line
        if (root.selectedLines.length > 0 && titleField.text.length === 0) {
            var firstLine = root.selectedLines[0]
            if (firstLine.length > 100) {
                firstLine = firstLine.substring(0, 100) + "..."
            }
            titleField.text = firstLine
        }
    }
}

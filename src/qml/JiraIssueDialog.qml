import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief Jira Issue Creation Dialog
 * 
 * Enterprise feature: Create Jira issues from selected log content
 */
Dialog {
    id: root
    
    title: qsTr("Create Jira Issue")
    width: 550
    height: 520
    modal: true
    closePolicy: Popup.CloseOnEscape
    
    // 设置居中显示
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    
    // Input properties
    property var selectedLines: []
    property var lineNumbers: []
    property string filePath: ""
    
    // Jira integration reference
    property var jiraIntegration: null
    
    contentItem: ColumnLayout {
        spacing: 12
        
        // Configuration status
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: root.jiraIntegration && root.jiraIntegration.isConfigured 
                   ? Qt.rgba(0.2, 0.7, 0.3, 0.1) 
                   : Qt.rgba(1, 0.5, 0, 0.1)
            radius: 4
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8
                
                Label {
                    text: root.jiraIntegration && root.jiraIntegration.isConfigured 
                          ? "✓ " + qsTr("Connected to Jira")
                          : "⚠ " + qsTr("Jira not configured")
                    color: root.jiraIntegration && root.jiraIntegration.isConfigured 
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
            enabled: root.jiraIntegration && root.jiraIntegration.isConfigured
            
            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 8
                spacing: 8
                
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8
                    
                    Label { text: qsTr("Project:") + " *"; color: _themeManager.textColor }
                    ComboBox {
                        id: projectCombo
                        Layout.fillWidth: true
                        textRole: "display"
                        
                        model: ListModel { id: projectModel }
                        
                        onCurrentIndexChanged: {
                            if (currentIndex >= 0 && root.jiraIntegration) {
                                var proj = projectModel.get(currentIndex)
                                if (proj) {
                                    root.jiraIntegration.fetchIssueTypes(proj.key)
                                }
                            }
                        }
                    }
                    
                    Label { text: qsTr("Issue Type:") + " *"; color: _themeManager.textColor }
                    ComboBox {
                        id: issueTypeCombo
                        Layout.fillWidth: true
                        textRole: "display"
                        
                        model: ListModel { id: issueTypeModel }
                    }
                    
                    Label { text: qsTr("Summary:") + " *"; color: _themeManager.textColor }
                    TextField {
                        id: summaryField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Brief description of the issue")
                        color: _themeManager.textColor
                    }
                    
                    Label { text: qsTr("Labels:"); color: _themeManager.textColor }
                    TextField {
                        id: labelsField
                        Layout.fillWidth: true
                        placeholderText: qsTr("bug, critical, production (comma separated)")
                        color: _themeManager.textColor
                    }
                    
                    Label { text: qsTr("Priority:"); color: _themeManager.textColor }
                    ComboBox {
                        id: priorityCombo
                        Layout.fillWidth: true
                        model: ["", "Highest", "High", "Medium", "Low", "Lowest"]
                        currentIndex: 0
                    }
                }
                
                // Description 单独一行
                Label { 
                    text: qsTr("Description:")
                    color: _themeManager.textColor
                }
                
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 80
                    clip: true
                    
                    TextArea {
                        id: descriptionField
                        placeholderText: qsTr("Detailed description (log content will be appended)")
                        wrapMode: TextArea.Wrap
                        color: _themeManager.textColor
                    }
                }
            }
        }
        
        // Log preview
        GroupBox {
            title: qsTr("Log Content Preview")
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            
            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 8
                
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
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
        }
        
        // Status and buttons
        RowLayout {
            Layout.fillWidth: true
            
            BusyIndicator {
                running: root.jiraIntegration && root.jiraIntegration.loading
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
                enabled: root.jiraIntegration && 
                         root.jiraIntegration.isConfigured &&
                         !root.jiraIntegration.loading &&
                         projectCombo.currentIndex >= 0 &&
                         issueTypeCombo.currentIndex >= 0 &&
                         summaryField.text.length > 0
                
                onClicked: createIssue()
            }
        }
    }
    
    // ===== Settings Dialog =====
    Dialog {
        id: settingsDialog
        title: qsTr("Jira Settings")
        width: 450
        height: 350
        modal: true
        closePolicy: Popup.CloseOnEscape
        
        // 设置居中显示
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        
        contentItem: ColumnLayout {
            spacing: 12
            
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8
                
                Label { text: qsTr("Jira URL:") }
                TextField {
                    id: baseUrlField
                    Layout.fillWidth: true
                    placeholderText: "https://yourcompany.atlassian.net"
                    text: root.jiraIntegration ? root.jiraIntegration.baseUrl : ""
                }
                
                Label { }
                Label {
                    text: qsTr("For Jira Cloud: https://yoursite.atlassian.net")
                    font.pixelSize: 11
                    opacity: 0.6
                }
                
                Label { text: qsTr("Email/Username:") }
                TextField {
                    id: usernameField
                    Layout.fillWidth: true
                    placeholderText: "your-email@company.com"
                    text: root.jiraIntegration ? root.jiraIntegration.username : ""
                }
                
                Label { text: qsTr("API Token:") }
                RowLayout {
                    Layout.fillWidth: true
                    
                    TextField {
                        id: tokenField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: qsTr("Jira API token or PAT")
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
                    text: qsTr("Get token from: Jira → Account Settings → Security → API Tokens")
                    font.pixelSize: 11
                    opacity: 0.6
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
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
                    enabled: baseUrlField.text.length > 0 && 
                             usernameField.text.length > 0 &&
                             tokenField.text.length > 0
                    onClicked: {
                        saveJiraSettings()
                        root.jiraIntegration.testConnection()
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
                        saveJiraSettings()
                        settingsDialog.close()
                        loadProjects()
                    }
                }
            }
        }
    }
    
    function saveJiraSettings() {
        if (!root.jiraIntegration) return
        
        root.jiraIntegration.baseUrl = baseUrlField.text
        root.jiraIntegration.username = usernameField.text
        root.jiraIntegration.setApiToken(tokenField.text)
        root.jiraIntegration.saveConfiguration()
    }
    
    function loadProjects() {
        if (!root.jiraIntegration || !root.jiraIntegration.isConfigured) return
        
        root.jiraIntegration.fetchProjects()
    }
    
    function createIssue() {
        if (!root.jiraIntegration) return
        
        var proj = projectModel.get(projectCombo.currentIndex)
        var issueType = issueTypeModel.get(issueTypeCombo.currentIndex)
        
        if (!proj || !issueType) return
        
        // Format description with log content
        var desc = descriptionField.text
        if (desc.length > 0) {
            desc += "\n\n"
        }
        desc += root.jiraIntegration.formatLogForJira(
            root.selectedLines, root.filePath, root.lineNumbers)
        
        // Parse labels
        var labels = labelsField.text.split(",").filter(function(l) { 
            return l.trim().length > 0 
        })
        
        var priority = priorityCombo.currentIndex > 0 ? priorityCombo.currentText : ""
        
        root.jiraIntegration.createIssue(
            proj.key,
            issueType.name,
            summaryField.text,
            desc,
            labels,
            priority
        )
        
        statusLabel.text = qsTr("Creating issue...")
        statusLabel.color = _themeManager.textColor
    }
    
    Connections {
        target: root.jiraIntegration
        
        function onProjectsFetched(projects) {
            projectModel.clear()
            for (var i = 0; i < projects.length; i++) {
                projectModel.append({
                    "key": projects[i].key,
                    "name": projects[i].name,
                    "display": projects[i].key + " - " + projects[i].name
                })
            }
            if (projectModel.count > 0) {
                projectCombo.currentIndex = 0
            }
        }
        
        function onIssueTypesFetched(projectKey, types) {
            issueTypeModel.clear()
            for (var i = 0; i < types.length; i++) {
                if (!types[i].subtask) {  // Skip subtasks
                    issueTypeModel.append({
                        "id": types[i].id,
                        "name": types[i].name,
                        "display": types[i].name
                    })
                }
            }
            if (issueTypeModel.count > 0) {
                issueTypeCombo.currentIndex = 0
            }
        }
        
        function onConnectionTested(success, message) {
            testStatusLabel.text = message
            testStatusLabel.color = success ? "#4CAF50" : "#F44336"
        }
        
        function onIssueCreated(issueKey, issueUrl) {
            statusLabel.text = qsTr("Created: %1").arg(issueKey)
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
        loadProjects()
        
        // Auto-generate summary from first log line
        if (root.selectedLines.length > 0 && summaryField.text.length === 0) {
            var firstLine = root.selectedLines[0]
            if (firstLine.length > 100) {
                firstLine = firstLine.substring(0, 100) + "..."
            }
            summaryField.text = firstLine
        }
    }
}

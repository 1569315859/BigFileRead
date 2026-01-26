/**
 * DatabaseConnectionDialog.qml
 * Database connection configuration dialog for BigFileViewer
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: qsTr("Database Connection")
    modal: true
    width: 550
    height: 500
    anchors.centerIn: parent
    
    property var databaseConnector: _databaseConnector
    property string selectedProfile: ""
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
    
    signal connectionSuccess(string tempFilePath)
    
    standardButtons: Dialog.Close
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    Component.onCompleted: {
        if (databaseConnector) {
            refreshDrivers()
            refreshProfiles()
        }
    }
    
    Connections {
        target: databaseConnector
        function onConnectionTested(success, message) {
            testResultLabel.text = message
            testResultLabel.color = success ? "green" : "red"
            testButton.enabled = true
        }
        function onConnectionChanged() {
            connectButton.text = databaseConnector.isConnected ? qsTr("Disconnect") : qsTr("Connect")
            statusLabel.text = databaseConnector.isConnected ? 
                qsTr("Connected to: %1").arg(databaseConnector.currentProfile) : 
                qsTr("Not connected")
            
            if (databaseConnector.isConnected) {
                refreshTables()
            }
        }
        function onQueryCompleted(result) {
            queryingIndicator.visible = false
            if (result.success) {
                resultModel.clear()
                for (var i = 0; i < result.rows.length; i++) {
                    resultModel.append(result.rows[i])
                }
                resultCountLabel.text = qsTr("%1 rows, %2 ms").arg(result.rowCount).arg(result.executionTimeMs)
            } else {
                resultCountLabel.text = qsTr("Error: %1").arg(result.errorMessage)
            }
        }
        function onQueryingChanged() {
            queryingIndicator.visible = databaseConnector.isQuerying
        }
        function onProfilesChanged() {
            refreshProfiles()
        }
    }
    
    function refreshDrivers() {
        driverModel.clear()
        var drivers = databaseConnector.getDriverInfo()
        for (var i = 0; i < drivers.length; i++) {
            driverModel.append(drivers[i])
        }
    }
    
    function refreshProfiles() {
        profileModel.clear()
        profileModel.append({name: qsTr("-- New Connection --"), isNew: true})
        var profiles = databaseConnector.getProfiles()
        for (var i = 0; i < profiles.length; i++) {
            profiles[i].isNew = false
            profileModel.append(profiles[i])
        }
    }
    
    function refreshTables() {
        tableModel.clear()
        var tables = databaseConnector.getTables()
        for (var i = 0; i < tables.length; i++) {
            tableModel.append({name: tables[i]})
        }
    }
    
    function buildProfile() {
        return {
            name: profileNameField.text,
            driver: driverCombo.currentValue,
            host: hostField.text,
            port: parseInt(portField.text) || 0,
            database: databaseField.text,
            username: usernameField.text,
            password: passwordField.text,
            connectionOptions: optionsField.text,
            defaultQuery: defaultQueryField.text,
            autoConnect: autoConnectCheck.checked
        }
    }
    
    ListModel { id: driverModel }
    ListModel { id: profileModel }
    ListModel { id: tableModel }
    ListModel { id: resultModel }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            TabButton { text: qsTr("Connection") }
            TabButton { text: qsTr("Query") }
            TabButton { text: qsTr("Export") }
        }
        
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // ===== Connection Tab =====
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    
                    // Profile selector
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Label { text: qsTr("Profile:") }
                        ComboBox {
                            id: profileCombo
                            Layout.fillWidth: true
                            model: profileModel
                            textRole: "name"
                            onCurrentIndexChanged: {
                                if (currentIndex > 0) {
                                    var profile = databaseConnector.getProfile(currentText)
                                    if (profile) {
                                        profileNameField.text = profile.name || ""
                                        driverCombo.currentIndex = driverCombo.indexOfValue(profile.driver)
                                        hostField.text = profile.host || "localhost"
                                        portField.text = profile.port > 0 ? profile.port.toString() : ""
                                        databaseField.text = profile.database || ""
                                        usernameField.text = profile.username || ""
                                        passwordField.text = ""  // Don't load password
                                        optionsField.text = profile.connectionOptions || ""
                                        defaultQueryField.text = profile.defaultQuery || ""
                                        autoConnectCheck.checked = profile.autoConnect || false
                                    }
                                }
                            }
                        }
                        Button {
                            text: qsTr("Delete")
                            enabled: profileCombo.currentIndex > 0
                            onClicked: {
                                databaseConnector.deleteProfile(profileCombo.currentText)
                            }
                        }
                    }
                    
                    // Connection form
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 8
                        
                        Label { text: qsTr("Profile Name:") }
                        TextField {
                            id: profileNameField
                            Layout.fillWidth: true
                            placeholderText: qsTr("My Database")
                        }
                        
                        Label { text: qsTr("Driver:") }
                        ComboBox {
                            id: driverCombo
                            Layout.fillWidth: true
                            model: driverModel
                            textRole: "description"
                            valueRole: "name"
                            delegate: ItemDelegate {
                                width: parent.width
                                text: model.description
                                enabled: model.available
                                opacity: model.available ? 1.0 : 0.5
                            }
                            onCurrentValueChanged: {
                                // Update default port
                                var drivers = databaseConnector.getDriverInfo()
                                for (var i = 0; i < drivers.length; i++) {
                                    if (drivers[i].name === currentValue && drivers[i].defaultPort > 0) {
                                        if (portField.text === "" || portField.text === "0") {
                                            portField.text = drivers[i].defaultPort.toString()
                                        }
                                        break
                                    }
                                }
                                
                                // Show/hide fields based on driver
                                var isSqlite = currentValue === "QSQLITE"
                                hostRow.visible = !isSqlite
                                portRow.visible = !isSqlite
                                usernameRow.visible = !isSqlite
                                passwordRow.visible = !isSqlite
                            }
                        }
                        
                        Label { 
                            id: hostRowLabel
                            text: qsTr("Host:")
                            visible: hostRow.visible
                        }
                        RowLayout {
                            id: hostRow
                            Layout.fillWidth: true
                            TextField {
                                id: hostField
                                Layout.fillWidth: true
                                text: "localhost"
                                placeholderText: "localhost"
                            }
                        }
                        
                        Label {
                            text: qsTr("Port:")
                            visible: portRow.visible
                        }
                        RowLayout {
                            id: portRow
                            Layout.fillWidth: true
                            TextField {
                                id: portField
                                Layout.preferredWidth: 100
                                validator: IntValidator { bottom: 0; top: 65535 }
                                placeholderText: "3306"
                            }
                            Item { Layout.fillWidth: true }
                        }
                        
                        Label { text: qsTr("Database:") }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: databaseField
                                Layout.fillWidth: true
                                placeholderText: driverCombo.currentValue === "QSQLITE" ? 
                                    qsTr("Path to .db file") : qsTr("Database name")
                            }
                            Button {
                                text: "..."
                                visible: driverCombo.currentValue === "QSQLITE"
                                onClicked: {
                                    // Open file dialog for SQLite
                                    // TODO: Integrate with file dialog
                                }
                            }
                        }
                        
                        Label {
                            text: qsTr("Username:")
                            visible: usernameField.visible
                        }
                        TextField {
                            id: usernameField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Username")
                        }
                        
                        Label {
                            text: qsTr("Password:")
                            visible: passwordField.visible
                        }
                        TextField {
                            id: passwordField
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: qsTr("Password")
                        }
                        
                        Label { text: qsTr("Options:") }
                        TextField {
                            id: optionsField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Connection options (optional)")
                        }
                        
                        Label { text: qsTr("Default Query:") }
                        TextField {
                            id: defaultQueryField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Query to run on connect (optional)")
                        }
                        
                        Item { Layout.preferredWidth: 1 }
                        CheckBox {
                            id: autoConnectCheck
                            text: qsTr("Auto-connect on startup")
                        }
                    }
                    
                    // Test result
                    Label {
                        id: testResultLabel
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        visible: text !== ""
                    }
                    
                    // Buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        
                        Button {
                            id: testButton
                            text: qsTr("Test Connection")
                            onClicked: {
                                testResultLabel.text = qsTr("Testing...")
                                testResultLabel.color = "gray"
                                enabled = false
                                databaseConnector.testConnection(buildProfile())
                            }
                        }
                        
                        Button {
                            text: qsTr("Save Profile")
                            enabled: profileNameField.text !== ""
                            onClicked: {
                                databaseConnector.saveProfile(buildProfile())
                            }
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Label {
                            id: statusLabel
                            text: qsTr("Not connected")
                            color: databaseConnector && databaseConnector.isConnected ? "green" : "gray"
                        }
                        
                        Button {
                            id: connectButton
                            text: databaseConnector && databaseConnector.isConnected ? 
                                qsTr("Disconnect") : qsTr("Connect")
                            highlighted: true
                            onClicked: {
                                if (databaseConnector.isConnected) {
                                    databaseConnector.disconnect()
                                } else {
                                    databaseConnector.connectWithProfile(buildProfile())
                                }
                            }
                        }
                    }
                }
            }
            
            // ===== Query Tab =====
            ColumnLayout {
                spacing: 10
                
                // Tables list
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label { text: qsTr("Tables:") }
                    ComboBox {
                        id: tableCombo
                        Layout.fillWidth: true
                        model: tableModel
                        textRole: "name"
                        enabled: databaseConnector && databaseConnector.isConnected
                    }
                    Button {
                        text: qsTr("Preview")
                        enabled: tableCombo.currentText !== ""
                        onClicked: {
                            databaseConnector.previewTable(tableCombo.currentText, 100)
                        }
                    }
                    Button {
                        text: qsTr("Refresh")
                        enabled: databaseConnector && databaseConnector.isConnected
                        onClicked: refreshTables()
                    }
                }
                
                // SQL input
                Label { text: qsTr("SQL Query:") }
                TextArea {
                    id: sqlInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    placeholderText: qsTr("SELECT * FROM logs WHERE timestamp > '2024-01-01' LIMIT 1000")
                    font.family: "Consolas, Monaco, monospace"
                    enabled: databaseConnector && databaseConnector.isConnected
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Button {
                        text: qsTr("Execute")
                        highlighted: true
                        enabled: databaseConnector && databaseConnector.isConnected && sqlInput.text !== ""
                        onClicked: {
                            databaseConnector.executeQuery(sqlInput.text, 10000)
                        }
                    }
                    
                    Button {
                        text: qsTr("Cancel")
                        enabled: databaseConnector && databaseConnector.isQuerying
                        onClicked: databaseConnector.cancelQuery()
                    }
                    
                    BusyIndicator {
                        id: queryingIndicator
                        visible: false
                        running: visible
                        width: 24
                        height: 24
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Label {
                        id: resultCountLabel
                        text: ""
                    }
                }
                
                // Results table placeholder
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: palette.base
                    border.color: palette.mid
                    
                    Label {
                        anchors.centerIn: parent
                        text: resultModel.count > 0 ? 
                            qsTr("%1 rows loaded").arg(resultModel.count) :
                            qsTr("Execute a query to see results")
                        color: palette.placeholderText
                    }
                }
            }
            
            // ===== Export Tab =====
            ColumnLayout {
                spacing: 10
                
                Label {
                    text: qsTr("Convert query results to log format for viewing in BigFileViewer")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 8
                    
                    Label { text: qsTr("Timestamp Column:") }
                    TextField {
                        id: timestampColumnField
                        Layout.fillWidth: true
                        placeholderText: qsTr("e.g., created_at, timestamp, log_time")
                    }
                    
                    Label { text: qsTr("Message Column:") }
                    TextField {
                        id: messageColumnField
                        Layout.fillWidth: true
                        placeholderText: qsTr("e.g., message, log_message, content")
                    }
                    
                    Label { text: qsTr("Level Column (optional):") }
                    TextField {
                        id: levelColumnField
                        Layout.fillWidth: true
                        placeholderText: qsTr("e.g., level, severity, log_level")
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Button {
                        text: qsTr("Export & Open in Viewer")
                        highlighted: true
                        enabled: resultModel.count > 0 && timestampColumnField.text !== "" && messageColumnField.text !== ""
                        onClicked: {
                            var filePath = databaseConnector.exportResultsToFile(
                                timestampColumnField.text,
                                messageColumnField.text,
                                levelColumnField.text
                            )
                            if (filePath) {
                                dialog.connectionSuccess(filePath)
                                dialog.close()
                            }
                        }
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Label {
                        text: resultModel.count > 0 ? 
                            qsTr("%1 rows ready to export").arg(resultModel.count) :
                            qsTr("Run a query first")
                        color: resultModel.count > 0 ? palette.text : palette.placeholderText
                    }
                }
                
                Item { Layout.fillHeight: true }
            }
        }
    }
}

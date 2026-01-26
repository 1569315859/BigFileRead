/**
 * CloudStorageDialog.qml
 * Cloud Storage Connection Dialog for BigFileViewer
 * 
 * Supports:
 * - Amazon S3
 * - Azure Blob Storage
 * - Google Cloud Storage
 * 
 * Features:
 * - Runtime SDK download
 * - Credential management
 * - Bucket/container browsing
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: qsTr("Cloud Storage")
    width: 700
    height: 550
    modal: true
    anchors.centerIn: parent
    
    property var cloudManager: _cloudManager
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
    
    // Current state
    property string selectedProvider: ""
    property bool sdkAvailable: false
    
    signal fileSelected(string localPath, string cloudPath)
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    Component.onCompleted: {
        // Check SDK availability for all providers
        checkAllSdks()
    }
    
    function checkAllSdks() {
        s3SdkAvailable = cloudManager.checkSdkAvailable("s3")
        azureSdkAvailable = cloudManager.checkSdkAvailable("azure")
        gcsSdkAvailable = cloudManager.checkSdkAvailable("gcs")
    }
    
    function buildCredentials() {
        var creds = {}
        
        switch (selectedProvider) {
            case "s3":
                creds.accessKeyId = s3AccessKey.text
                creds.secretAccessKey = s3SecretKey.text
                creds.region = s3Region.currentText
                if (s3SessionToken.text) {
                    creds.sessionToken = s3SessionToken.text
                }
                break
                
            case "azure":
                creds.accountName = azureAccountName.text
                if (azureConnectionString.text) {
                    creds.connectionString = azureConnectionString.text
                } else {
                    creds.tenantId = azureTenantId.text
                    creds.clientId = azureClientId.text
                    creds.clientSecret = azureClientSecret.text
                }
                break
                
            case "gcs":
                creds.project = gcsProject.text
                if (gcsKeyFile.text) {
                    creds.keyFile = gcsKeyFile.text
                }
                break
        }
        
        return creds
    }
    
    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + " MB"
        return (bytes / 1024 / 1024 / 1024).toFixed(2) + " GB"
    }
    
    property bool s3SdkAvailable: false
    property bool azureSdkAvailable: false
    property bool gcsSdkAvailable: false
    
    Connections {
        target: cloudManager
        
        function onConnectionChanged() {
            if (cloudManager.isConnected) {
                stackView.push(browserPage)
            }
        }
        
        function onBucketsListed(buckets) {
            bucketModel.clear()
            for (var i = 0; i < buckets.length; i++) {
                bucketModel.append(buckets[i])
            }
        }
        
        function onObjectsListed(objects) {
            objectModel.clear()
            for (var i = 0; i < objects.length; i++) {
                objectModel.append(objects[i])
            }
        }
        
        function onFileDownloaded(localPath, cloudPath) {
            root.fileSelected(localPath, cloudPath)
            root.close()
        }
        
        function onSdkDownloadComplete(success, message) {
            sdkDownloadDialog.close()
            if (success) {
                checkAllSdks()
                messageDialog.text = message
                messageDialog.open()
            }
        }
        
        function onErrorOccurred(error) {
            errorLabel.text = error
            errorLabel.visible = true
        }
        
        function onConnectionTested(success, message) {
            testResultLabel.text = message
            testResultLabel.color = success ? "green" : "red"
            testResultLabel.visible = true
        }
    }
    
    ListModel { id: bucketModel }
    ListModel { id: objectModel }
    
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: providerPage
    }
    
    // Page 1: Provider Selection
    Component {
        id: providerPage
        
        ColumnLayout {
            spacing: 16
            
            Label {
                text: qsTr("Select Cloud Provider")
                font.pixelSize: 18
                font.bold: true
            }
            
            GridLayout {
                columns: 3
                rowSpacing: 16
                columnSpacing: 16
                Layout.fillWidth: true
                
                // S3
                Rectangle {
                    width: 180
                    height: 120
                    radius: 8
                    color: s3MouseArea.containsMouse ? "#e3f2fd" : "#f5f5f5"
                    border.color: selectedProvider === "s3" ? "#1976d2" : "transparent"
                    border.width: 2
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Label {
                            text: "☁️"
                            font.pixelSize: 32
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Label {
                            text: "Amazon S3"
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Label {
                            text: s3SdkAvailable ? qsTr("Ready") : qsTr("SDK Required")
                            color: s3SdkAvailable ? "green" : "orange"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                    
                    MouseArea {
                        id: s3MouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            selectedProvider = "s3"
                            if (s3SdkAvailable) {
                                stackView.push(credentialsPage)
                            } else {
                                sdkDownloadDialog.provider = "s3"
                                sdkDownloadDialog.providerName = "AWS CLI"
                                sdkDownloadDialog.open()
                            }
                        }
                    }
                }
                
                // Azure
                Rectangle {
                    width: 180
                    height: 120
                    radius: 8
                    color: azureMouseArea.containsMouse ? "#e3f2fd" : "#f5f5f5"
                    border.color: selectedProvider === "azure" ? "#1976d2" : "transparent"
                    border.width: 2
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Label {
                            text: "📦"
                            font.pixelSize: 32
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Label {
                            text: "Azure Blob"
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Label {
                            text: azureSdkAvailable ? qsTr("Ready") : qsTr("SDK Required")
                            color: azureSdkAvailable ? "green" : "orange"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                    
                    MouseArea {
                        id: azureMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            selectedProvider = "azure"
                            if (azureSdkAvailable) {
                                stackView.push(credentialsPage)
                            } else {
                                sdkDownloadDialog.provider = "azure"
                                sdkDownloadDialog.providerName = "Azure CLI"
                                sdkDownloadDialog.open()
                            }
                        }
                    }
                }
                
                // GCS
                Rectangle {
                    width: 180
                    height: 120
                    radius: 8
                    color: gcsMouseArea.containsMouse ? "#e3f2fd" : "#f5f5f5"
                    border.color: selectedProvider === "gcs" ? "#1976d2" : "transparent"
                    border.width: 2
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Label {
                            text: "🗄️"
                            font.pixelSize: 32
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Label {
                            text: "Google Cloud"
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Label {
                            text: gcsSdkAvailable ? qsTr("Ready") : qsTr("SDK Required")
                            color: gcsSdkAvailable ? "green" : "orange"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                    
                    MouseArea {
                        id: gcsMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            selectedProvider = "gcs"
                            if (gcsSdkAvailable) {
                                stackView.push(credentialsPage)
                            } else {
                                sdkDownloadDialog.provider = "gcs"
                                sdkDownloadDialog.providerName = "Google Cloud SDK"
                                sdkDownloadDialog.open()
                            }
                        }
                    }
                }
            }
            
            // Saved credentials
            GroupBox {
                title: qsTr("Saved Connections")
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                ListView {
                    id: savedCredentialsList
                    anchors.fill: parent
                    model: cloudManager.getSavedCredentials()
                    clip: true
                    
                    delegate: ItemDelegate {
                        width: savedCredentialsList.width
                        height: 48
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            
                            Label {
                                text: modelData.provider === "s3" ? "☁️" :
                                      modelData.provider === "azure" ? "📦" : "🗄️"
                                font.pixelSize: 20
                            }
                            
                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                
                                Label {
                                    text: modelData.name
                                    font.bold: true
                                }
                                
                                Label {
                                    text: modelData.provider.toUpperCase()
                                    font.pixelSize: 11
                                    color: "#666"
                                }
                            }
                            
                            Button {
                                text: qsTr("Connect")
                                onClicked: {
                                    var creds = cloudManager.getCredentials(modelData.name)
                                    selectedProvider = modelData.provider
                                    cloudManager.connectToCloud(selectedProvider, creds)
                                }
                            }
                            
                            Button {
                                text: "🗑️"
                                flat: true
                                onClicked: {
                                    cloudManager.deleteCredentials(modelData.name)
                                }
                            }
                        }
                    }
                    
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No saved connections")
                        visible: savedCredentialsList.count === 0
                        color: "#999"
                    }
                }
            }
        }
    }
    
    // Page 2: Credentials Input
    Component {
        id: credentialsPage
        
        ColumnLayout {
            spacing: 16
            
            RowLayout {
                Button {
                    text: "← " + qsTr("Back")
                    flat: true
                    onClicked: stackView.pop()
                }
                
                Label {
                    text: {
                        switch (selectedProvider) {
                            case "s3": return qsTr("Amazon S3 Credentials")
                            case "azure": return qsTr("Azure Blob Credentials")
                            case "gcs": return qsTr("Google Cloud Credentials")
                            default: return qsTr("Credentials")
                        }
                    }
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                }
            }
            
            // S3 Credentials
            GridLayout {
                columns: 2
                rowSpacing: 12
                columnSpacing: 12
                Layout.fillWidth: true
                visible: selectedProvider === "s3"
                
                Label { text: qsTr("Access Key ID:") }
                TextField {
                    id: s3AccessKey
                    Layout.fillWidth: true
                    placeholderText: "AKIA..."
                }
                
                Label { text: qsTr("Secret Access Key:") }
                TextField {
                    id: s3SecretKey
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                }
                
                Label { text: qsTr("Region:") }
                ComboBox {
                    id: s3Region
                    Layout.fillWidth: true
                    model: ["us-east-1", "us-west-2", "eu-west-1", "eu-central-1", "ap-northeast-1", "ap-southeast-1"]
                    editable: true
                }
                
                Label { text: qsTr("Session Token (optional):") }
                TextField {
                    id: s3SessionToken
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                }
            }
            
            // Azure Credentials
            GridLayout {
                columns: 2
                rowSpacing: 12
                columnSpacing: 12
                Layout.fillWidth: true
                visible: selectedProvider === "azure"
                
                Label { text: qsTr("Account Name:") }
                TextField {
                    id: azureAccountName
                    Layout.fillWidth: true
                }
                
                Label { text: qsTr("Connection String:") }
                TextField {
                    id: azureConnectionString
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: "DefaultEndpointsProtocol=https;..."
                }
                
                Label { 
                    text: qsTr("— OR —") 
                    Layout.columnSpan: 2
                    Layout.alignment: Qt.AlignHCenter
                    color: "#666"
                }
                
                Label { text: qsTr("Tenant ID:") }
                TextField {
                    id: azureTenantId
                    Layout.fillWidth: true
                }
                
                Label { text: qsTr("Client ID:") }
                TextField {
                    id: azureClientId
                    Layout.fillWidth: true
                }
                
                Label { text: qsTr("Client Secret:") }
                TextField {
                    id: azureClientSecret
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                }
            }
            
            // GCS Credentials
            GridLayout {
                columns: 2
                rowSpacing: 12
                columnSpacing: 12
                Layout.fillWidth: true
                visible: selectedProvider === "gcs"
                
                Label { text: qsTr("Project ID:") }
                TextField {
                    id: gcsProject
                    Layout.fillWidth: true
                }
                
                Label { text: qsTr("Service Account Key File:") }
                RowLayout {
                    Layout.fillWidth: true
                    
                    TextField {
                        id: gcsKeyFile
                        Layout.fillWidth: true
                        placeholderText: qsTr("Path to JSON key file")
                    }
                    
                    Button {
                        text: qsTr("Browse...")
                        onClicked: {
                            // Would use FileDialog in real implementation
                        }
                    }
                }
            }
            
            // Save credentials option
            RowLayout {
                CheckBox {
                    id: saveCredsCheck
                    text: qsTr("Save credentials")
                }
                
                TextField {
                    id: credentialName
                    placeholderText: qsTr("Connection name")
                    visible: saveCredsCheck.checked
                    Layout.fillWidth: true
                }
            }
            
            // Test result
            Label {
                id: testResultLabel
                visible: false
                Layout.fillWidth: true
            }
            
            // Error label
            Label {
                id: errorLabel
                visible: false
                color: "red"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            
            Item { Layout.fillHeight: true }
            
            // Buttons
            RowLayout {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("Test Connection")
                    onClicked: {
                        var creds = root.buildCredentials()
                        cloudManager.testConnection(selectedProvider, creds)
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Connect")
                    highlighted: true
                    onClicked: {
                        var creds = root.buildCredentials()
                        
                        if (saveCredsCheck.checked && credentialName.text) {
                            cloudManager.saveCredentials(credentialName.text, selectedProvider, creds)
                        }
                        
                        cloudManager.connectToCloud(selectedProvider, creds)
                    }
                }
            }
        }
    }
    
    // Page 3: File Browser
    Component {
        id: browserPage
        
        ColumnLayout {
            spacing: 12
            
            // Header
            RowLayout {
                Button {
                    text: "← " + qsTr("Disconnect")
                    flat: true
                    onClicked: {
                        cloudManager.disconnect()
                        stackView.pop(null)
                    }
                }
                
                Label {
                    text: cloudManager.currentProvider.toUpperCase()
                    font.bold: true
                }
                
                Label {
                    text: "/"
                    visible: cloudManager.currentBucket !== ""
                }
                
                Label {
                    text: cloudManager.currentBucket
                    visible: cloudManager.currentBucket !== ""
                }
                
                Label {
                    text: "/" + cloudManager.currentPath
                    visible: cloudManager.currentPath !== ""
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                
                Item { Layout.fillWidth: true }
                
                BusyIndicator {
                    running: cloudManager.loading
                    width: 24
                    height: 24
                }
            }
            
            // Navigation
            RowLayout {
                Button {
                    text: "⬆️ " + qsTr("Up")
                    enabled: cloudManager.currentBucket !== "" || cloudManager.currentPath !== ""
                    onClicked: cloudManager.navigateUp()
                }
                
                Button {
                    text: "🔄 " + qsTr("Refresh")
                    onClicked: {
                        if (cloudManager.currentBucket === "") {
                            cloudManager.listBuckets()
                        } else {
                            cloudManager.listObjects(cloudManager.currentBucket, cloudManager.currentPath)
                        }
                    }
                }
            }
            
            // Bucket list (when not browsing a bucket)
            ListView {
                id: bucketListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: bucketModel
                visible: cloudManager.currentBucket === ""
                clip: true
                
                delegate: ItemDelegate {
                    width: bucketListView.width
                    height: 48
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        
                        Label {
                            text: "📁"
                            font.pixelSize: 20
                        }
                        
                        Label {
                            text: model.name
                            Layout.fillWidth: true
                        }
                        
                        Label {
                            text: model.creationDate || ""
                            color: "#666"
                            font.pixelSize: 11
                        }
                    }
                    
                    onClicked: {
                        cloudManager.listObjects(model.name, "")
                    }
                }
            }
            
            // Object list (when browsing a bucket)
            ListView {
                id: objectListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: objectModel
                visible: cloudManager.currentBucket !== ""
                clip: true
                
                delegate: ItemDelegate {
                    width: objectListView.width
                    height: 48
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        
                        Label {
                            text: model.isDir ? "📁" : "📄"
                            font.pixelSize: 20
                        }
                        
                        Label {
                            text: model.name
                            Layout.fillWidth: true
                        }
                        
                        Label {
                            text: model.isDir ? "" : root.formatSize(model.size)
                            color: "#666"
                            font.pixelSize: 11
                        }
                        
                        Label {
                            text: model.modified || ""
                            color: "#666"
                            font.pixelSize: 11
                        }
                    }
                    
                    onClicked: {
                        if (model.isDir) {
                            var newPath = cloudManager.currentPath
                            if (newPath !== "") {
                                newPath += "/"
                            }
                            newPath += model.name
                            cloudManager.navigateTo(cloudManager.currentBucket, newPath)
                        }
                    }
                    
                    onDoubleClicked: {
                        if (!model.isDir) {
                            var fullKey = cloudManager.currentPath
                            if (fullKey !== "") {
                                fullKey += "/"
                            }
                            fullKey += model.name
                            cloudManager.downloadFile(cloudManager.currentBucket, fullKey)
                        }
                    }
                }
            }
            
            // Status bar
            Label {
                text: cloudManager.currentBucket === "" 
                    ? qsTr("%1 buckets").arg(bucketModel.count)
                    : qsTr("%1 items").arg(objectModel.count)
                color: "#666"
            }
        }
    }
    
    // SDK Download Dialog
    Dialog {
        id: sdkDownloadDialog
        title: qsTr("SDK Required")
        modal: true
        width: 400
        
        property string provider: ""
        property string providerName: ""
        
        ColumnLayout {
            spacing: 16
            width: parent.width
            
            Label {
                text: qsTr("The %1 is required to access this cloud storage.\n\nWould you like to download it now?").arg(sdkDownloadDialog.providerName)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            
            ProgressBar {
                id: sdkProgressBar
                from: 0
                to: 1
                value: cloudManager.sdkDownloadProgress
                visible: cloudManager.sdkDownloading
                Layout.fillWidth: true
            }
            
            Label {
                text: qsTr("Downloading... %1%").arg(Math.round(cloudManager.sdkDownloadProgress * 100))
                visible: cloudManager.sdkDownloading
            }
        }
        
        footer: DialogButtonBox {
            Button {
                text: cloudManager.sdkDownloading ? qsTr("Cancel") : qsTr("Download")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    if (cloudManager.sdkDownloading) {
                        cloudManager.cancelSdkDownload()
                    } else {
                        cloudManager.downloadSdk(sdkDownloadDialog.provider)
                    }
                }
            }
            
            Button {
                text: qsTr("Later")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                visible: !cloudManager.sdkDownloading
            }
        }
    }
    
    // Message Dialog
    Dialog {
        id: messageDialog
        title: qsTr("Information")
        modal: true
        width: 400
        
        property alias text: messageLabel.text
        
        Label {
            id: messageLabel
            wrapMode: Text.Wrap
            width: parent.width
        }
        
        standardButtons: Dialog.Ok
    }
    
    standardButtons: Dialog.Close
}

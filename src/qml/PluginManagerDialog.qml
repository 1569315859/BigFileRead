import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: pluginManagerDialog
    title: qsTr("Plugin Manager")
    modal: true
    width: Math.min(parent.width * 0.85, 900)
    height: Math.min(parent.height * 0.9, 700)
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
    
    standardButtons: Dialog.Close
    
    contentItem: ColumnLayout {
        spacing: 10
        
        // Tab bar for available/installed/settings
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            TabButton {
                text: qsTr("Installed Plugins")
            }
            TabButton {
                text: qsTr("Available")
            }
            TabButton {
                text: qsTr("Settings")
            }
        }
        
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // Installed plugins tab
            ColumnLayout {
                spacing: 10
                
                // Toolbar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search plugins...")
                    }
                    
                    Button {
                        text: qsTr("Refresh")
                        icon.name: "view-refresh"
                        onClicked: _pluginManager.refreshPlugins()
                    }
                    
                    Button {
                        text: qsTr("Install from file...")
                        onClicked: installFileDialog.open()
                    }
                }
                
                // Plugin list
                ListView {
                    id: pluginList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    model: _pluginManager.availablePlugins.filter(function(p) {
                        if (!searchField.text) return true;
                        return p.name.toLowerCase().includes(searchField.text.toLowerCase()) ||
                               p.description.toLowerCase().includes(searchField.text.toLowerCase());
                    })
                    
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: pluginRow.implicitHeight + 20
                        color: index % 2 === 0 ? palette.base : palette.alternateBase
                        
                        RowLayout {
                            id: pluginRow
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 15
                            
                            // Plugin icon
                            Rectangle {
                                width: 48
                                height: 48
                                radius: 8
                                color: {
                                    switch(modelData.type) {
                                        case "parser": return "#4CAF50";
                                        case "analyzer": return "#2196F3";
                                        case "exporter": return "#FF9800";
                                        default: return "#9E9E9E";
                                    }
                                }
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.name.charAt(0).toUpperCase()
                                    font.pixelSize: 20
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            // Plugin info
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                
                                RowLayout {
                                    spacing: 10
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                        font.pixelSize: 14
                                    }
                                    
                                    Label {
                                        text: "v" + modelData.version
                                        opacity: 0.7
                                        font.pixelSize: 12
                                    }
                                    
                                    Rectangle {
                                        width: typeLabel.width + 10
                                        height: typeLabel.height + 4
                                        radius: 3
                                        color: {
                                            switch(modelData.type) {
                                                case "parser": return "#E8F5E9";
                                                case "analyzer": return "#E3F2FD";
                                                case "exporter": return "#FFF3E0";
                                                default: return "#F5F5F5";
                                            }
                                        }
                                        
                                        Label {
                                            id: typeLabel
                                            anchors.centerIn: parent
                                            text: modelData.type
                                            font.pixelSize: 11
                                            color: {
                                                switch(modelData.type) {
                                                    case "parser": return "#2E7D32";
                                                    case "analyzer": return "#1565C0";
                                                    case "exporter": return "#E65100";
                                                    default: return "#616161";
                                                }
                                            }
                                        }
                                    }
                                }
                                
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.description || qsTr("No description")
                                    wrapMode: Text.WordWrap
                                    opacity: 0.8
                                    font.pixelSize: 12
                                }
                                
                                RowLayout {
                                    spacing: 10
                                    
                                    Label {
                                        text: qsTr("by %1").arg(modelData.author || "Unknown")
                                        font.pixelSize: 11
                                        opacity: 0.6
                                    }
                                    
                                    Label {
                                        text: "•"
                                        opacity: 0.6
                                    }
                                    
                                    Label {
                                        text: modelData.status
                                        font.pixelSize: 11
                                        color: modelData.running ? "#4CAF50" : 
                                               modelData.enabled ? "#2196F3" : palette.text
                                    }
                                }
                            }
                            
                            // Actions
                            ColumnLayout {
                                spacing: 5
                                
                                Switch {
                                    checked: modelData.enabled
                                    onToggled: _pluginManager.enablePlugin(modelData.id, checked)
                                    
                                    ToolTip.visible: hovered
                                    ToolTip.text: checked ? qsTr("Disable plugin") : qsTr("Enable plugin")
                                }
                                
                                RowLayout {
                                    spacing: 5
                                    
                                    Button {
                                        visible: modelData.enabled
                                        text: modelData.running ? qsTr("Stop") : qsTr("Start")
                                        flat: true
                                        onClicked: {
                                            if (modelData.running) {
                                                _pluginManager.stopPlugin(modelData.id);
                                            } else {
                                                _pluginManager.startPlugin(modelData.id);
                                            }
                                        }
                                    }
                                    
                                    Button {
                                        text: "⚙"
                                        flat: true
                                        visible: modelData.enabled
                                        onClicked: configDialog.openForPlugin(modelData.id)
                                        
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Configure")
                                    }
                                    
                                    Button {
                                        text: "🗑"
                                        flat: true
                                        onClicked: {
                                            uninstallConfirm.pluginId = modelData.id;
                                            uninstallConfirm.pluginName = modelData.name;
                                            uninstallConfirm.open();
                                        }
                                        
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Uninstall")
                                    }
                                }
                            }
                        }
                    }
                    
                    // Empty state
                    Label {
                        anchors.centerIn: parent
                        visible: pluginList.count === 0
                        text: qsTr("No plugins installed.\nInstall plugins from the 'Available' tab or install from file.")
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.6
                    }
                }
            }
            
            // Available plugins tab (marketplace)
            ColumnLayout {
                spacing: 10
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    
                    TextField {
                        id: marketplaceSearch
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search online plugins...")
                        onAccepted: _pluginManager.searchOnlinePlugins(text)
                    }
                    
                    Button {
                        text: qsTr("Search")
                        onClicked: _pluginManager.searchOnlinePlugins(marketplaceSearch.text)
                    }
                }
                
                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("Online plugin marketplace coming soon.\n\nFor now, install plugins manually using 'Install from file...'")
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: 0.6
                }
            }
            
            // Settings tab
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 20
                    
                    GroupBox {
                        Layout.fillWidth: true
                        title: qsTr("Plugin Directories")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10
                            
                            ListView {
                                id: dirList
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                clip: true
                                
                                model: _pluginManager.pluginDirectories()
                                
                                delegate: RowLayout {
                                    width: ListView.view.width
                                    
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData
                                        elide: Text.ElideMiddle
                                    }
                                    
                                    Button {
                                        text: "×"
                                        flat: true
                                        onClicked: _pluginManager.removePluginDirectory(modelData)
                                    }
                                }
                            }
                            
                            RowLayout {
                                Layout.fillWidth: true
                                
                                TextField {
                                    id: newDirField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Add plugin directory...")
                                }
                                
                                Button {
                                    text: qsTr("Browse...")
                                    onClicked: dirDialog.open()
                                }
                                
                                Button {
                                    text: qsTr("Add")
                                    enabled: newDirField.text.length > 0
                                    onClicked: {
                                        _pluginManager.addPluginDirectory(newDirField.text);
                                        newDirField.text = "";
                                    }
                                }
                            }
                        }
                    }
                    
                    GroupBox {
                        Layout.fillWidth: true
                        title: qsTr("Security")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10
                            
                            CheckBox {
                                text: qsTr("Warn before enabling plugins with dangerous permissions")
                                checked: true
                            }
                            
                            CheckBox {
                                text: qsTr("Run plugins in sandboxed subprocess (recommended)")
                                checked: true
                            }
                            
                            CheckBox {
                                text: qsTr("Allow plugins to access network")
                                checked: true
                            }
                            
                            CheckBox {
                                text: qsTr("Allow plugins to execute external commands")
                                checked: false
                            }
                        }
                    }
                    
                    GroupBox {
                        Layout.fillWidth: true
                        title: qsTr("Auto-start")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10
                            
                            CheckBox {
                                text: qsTr("Automatically start enabled plugins on application launch")
                                checked: true
                            }
                            
                            CheckBox {
                                text: qsTr("Restart plugins that crash")
                                checked: true
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Uninstall confirmation dialog
    Dialog {
        id: uninstallConfirm
        title: qsTr("Uninstall Plugin")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        
        property string pluginId: ""
        property string pluginName: ""
        
        Label {
            text: qsTr("Are you sure you want to uninstall '%1'?\nThis action cannot be undone.").arg(uninstallConfirm.pluginName)
        }
        
        onAccepted: _pluginManager.uninstallPlugin(pluginId)
    }
    
    // Plugin configuration dialog
    Dialog {
        id: configDialog
        title: qsTr("Plugin Configuration")
        modal: true
        width: 500
        height: 400
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        
        property string pluginId: ""
        property var pluginInfo: ({})
        
        function openForPlugin(id) {
            pluginId = id;
            pluginInfo = _pluginManager.getPluginInfo(id);
            open();
        }
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            
            Label {
                text: qsTr("Configuration for: %1").arg(configDialog.pluginInfo.name || "")
                font.bold: true
            }
            
            Label {
                text: qsTr("Plugin configuration UI will be loaded from the plugin manifest.")
                opacity: 0.7
            }
            
            Item { Layout.fillHeight: true }
        }
    }
    
    // Connections
    Connections {
        target: _pluginManager
        
        function onPluginStarted(pluginId) {
            console.log("Plugin started:", pluginId);
        }
        
        function onPluginStopped(pluginId) {
            console.log("Plugin stopped:", pluginId);
        }
        
        function onPluginError(pluginId, error) {
            errorPopup.text = qsTr("Plugin error (%1): %2").arg(pluginId).arg(error);
            errorPopup.open();
        }
    }
    
    // Error popup
    Popup {
        id: errorPopup
        anchors.centerIn: parent
        width: 400
        height: 100
        modal: true
        
        property alias text: errorLabel.text
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            
            Label {
                id: errorLabel
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            
            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("OK")
                onClicked: errorPopup.close()
            }
        }
    }
}

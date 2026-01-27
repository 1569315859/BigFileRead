import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

/**
 * @brief 过滤模板管理对话框
 * 
 * 提供保存、加载、管理过滤模板的功能
 */
Dialog {
    id: root
    
    title: qsTr("Filter Templates")
    width: 600
    height: 500
    modal: true
    closePolicy: Popup.CloseOnEscape
    
    // 设置居中显示
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    
    // 当前过滤条件（用于保存新模板）
    property string currentKeyword: ""
    property bool currentCaseSensitive: false
    property bool currentUseRegex: false
    property bool currentIncludeContext: false
    property int currentContextLines: 0
    property string currentLogLevel: ""
    
    // 选中的模板
    property var selectedTemplate: null
    
    // 应用模板信号
    signal templateApplied(var template)
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // 工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Button {
                text: qsTr("Save Current Filter")
                icon.name: "document-save"
                enabled: root.currentKeyword.length > 0
                onClicked: saveDialog.open()
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Import")
                icon.name: "document-import"
                onClicked: importFileDialog.open()
            }
            
            Button {
                text: qsTr("Export")
                icon.name: "document-export"
                enabled: templateList.count > 0
                onClicked: exportFileDialog.open()
            }
        }
        
        // 模板列表
        GroupBox {
            title: qsTr("Saved Templates")
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                
                // 搜索框
                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search templates...")
                    
                    onTextChanged: filterTemplates()
                }
                
                // 模板列表
                ListView {
                    id: templateList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    model: filteredTemplates
                    
                    delegate: ItemDelegate {
                        width: templateList.width
                        height: 60
                        
                        required property var modelData
                        required property int index
                        
                        highlighted: root.selectedTemplate && root.selectedTemplate.name === modelData.name
                        
                        onClicked: {
                            root.selectedTemplate = modelData
                        }
                        
                        onDoubleClicked: {
                            root.selectedTemplate = modelData
                            applySelectedTemplate()
                        }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12
                            
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                
                                Label {
                                    text: modelData.name
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                Label {
                                    text: modelData.keyword
                                    font.pixelSize: 12
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                RowLayout {
                                    spacing: 8
                                    
                                    Label {
                                        visible: modelData.caseSensitive
                                        text: qsTr("Case Sensitive")
                                        font.pixelSize: 10
                                        padding: 2
                                        background: Rectangle {
                                            color: "#e3f2fd"
                                            radius: 2
                                        }
                                    }
                                    
                                    Label {
                                        visible: modelData.useRegex
                                        text: qsTr("Regex")
                                        font.pixelSize: 10
                                        padding: 2
                                        background: Rectangle {
                                            color: "#fff3e0"
                                            radius: 2
                                        }
                                    }
                                    
                                    Label {
                                        visible: modelData.logLevel && modelData.logLevel.length > 0
                                        text: modelData.logLevel || ""
                                        font.pixelSize: 10
                                        padding: 2
                                        background: Rectangle {
                                            color: "#e8f5e9"
                                            radius: 2
                                        }
                                    }
                                    
                                    Label {
                                        text: qsTr("Used %1 times").arg(modelData.useCount || 0)
                                        font.pixelSize: 10
                                        opacity: 0.5
                                    }
                                }
                            }
                            
                            // 操作按钮
                            RowLayout {
                                spacing: 4
                                
                                ToolButton {
                                    icon.name: "edit-rename"
                                    ToolTip.text: qsTr("Rename")
                                    ToolTip.visible: hovered
                                    onClicked: {
                                        root.selectedTemplate = modelData
                                        renameDialog.open()
                                    }
                                }
                                
                                ToolButton {
                                    icon.name: "edit-delete"
                                    ToolTip.text: qsTr("Delete")
                                    ToolTip.visible: hovered
                                    onClicked: {
                                        root.selectedTemplate = modelData
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }
                    }
                    
                    // 空列表提示
                    Label {
                        anchors.centerIn: parent
                        visible: templateList.count === 0
                        text: qsTr("No saved templates\nUse 'Save Current Filter' to create one")
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                    }
                }
            }
        }
        
        // 模板详情
        GroupBox {
            title: qsTr("Template Details")
            Layout.fillWidth: true
            visible: root.selectedTemplate !== null
            
            GridLayout {
                anchors.fill: parent
                columns: 4
                columnSpacing: 16
                rowSpacing: 8
                
                Label { text: qsTr("Name:"); font.bold: true }
                Label { 
                    text: root.selectedTemplate ? root.selectedTemplate.name : ""
                    Layout.fillWidth: true
                }
                
                Label { text: qsTr("Created:"); font.bold: true }
                Label { 
                    text: root.selectedTemplate && root.selectedTemplate.createdAt 
                          ? formatDateTime(root.selectedTemplate.createdAt) : ""
                }
                
                Label { text: qsTr("Keyword:"); font.bold: true }
                Label { 
                    text: root.selectedTemplate ? root.selectedTemplate.keyword : ""
                    Layout.columnSpan: 3
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
        
        // 底部按钮
        DialogButtonBox {
            Layout.fillWidth: true
            
            Button {
                text: qsTr("Apply")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                enabled: root.selectedTemplate !== null
            }
            
            Button {
                text: qsTr("Close")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }
    
    // 过滤后的模板列表
    property var filteredTemplates: []
    
    function filterTemplates() {
        var allTemplates = filterTemplateManager ? filterTemplateManager.templates : []
        var searchText = searchField.text.toLowerCase()
        
        if (searchText.length === 0) {
            filteredTemplates = allTemplates
        } else {
            filteredTemplates = allTemplates.filter(function(tmpl) {
                return tmpl.name.toLowerCase().indexOf(searchText) >= 0 ||
                       tmpl.keyword.toLowerCase().indexOf(searchText) >= 0
            })
        }
    }
    
    function formatDateTime(isoString) {
        if (!isoString) return ""
        var date = new Date(isoString)
        return date.toLocaleDateString() + " " + date.toLocaleTimeString()
    }
    
    function applySelectedTemplate() {
        if (root.selectedTemplate) {
            var template = filterTemplateManager.applyTemplate(root.selectedTemplate.name)
            root.templateApplied(template)
            root.close()
        }
    }
    
    onAccepted: applySelectedTemplate()
    
    onOpened: {
        filterTemplates()
        root.selectedTemplate = null
        searchField.text = ""
    }
    
    // 保存模板对话框
    Dialog {
        id: saveDialog
        title: qsTr("Save Filter Template")
        width: 400
        modal: true
        closePolicy: Popup.CloseOnEscape
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            Label { text: qsTr("Template Name:") }
            
            TextField {
                id: newTemplateName
                Layout.fillWidth: true
                placeholderText: qsTr("Enter template name")
            }
            
            Label {
                text: qsTr("Current Filter Settings:")
                font.bold: true
            }
            
            Label {
                text: qsTr("Keyword: %1").arg(root.currentKeyword)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            
            RowLayout {
                Label { text: root.currentCaseSensitive ? qsTr("✓ Case Sensitive") : qsTr("✗ Case Insensitive") }
                Label { text: root.currentUseRegex ? qsTr("✓ Regular Expression") : qsTr("✗ Plain Text") }
            }
            
            Label {
                visible: root.currentLogLevel.length > 0
                text: qsTr("Log Level: %1").arg(root.currentLogLevel)
            }
            
            DialogButtonBox {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("Save")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    enabled: newTemplateName.text.length > 0
                }
                
                Button {
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (filterTemplateManager) {
                var success = filterTemplateManager.saveTemplate(
                    newTemplateName.text,
                    root.currentKeyword,
                    root.currentCaseSensitive,
                    root.currentUseRegex,
                    root.currentIncludeContext,
                    root.currentContextLines,
                    root.currentLogLevel
                )
                
                if (success) {
                    filterTemplates()
                    newTemplateName.text = ""
                }
            }
        }
        
        onOpened: {
            newTemplateName.text = ""
            newTemplateName.forceActiveFocus()
        }
    }
    
    // 重命名对话框
    Dialog {
        id: renameDialog
        title: qsTr("Rename Template")
        width: 350
        modal: true
        closePolicy: Popup.CloseOnEscape
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            Label { text: qsTr("New Name:") }
            
            TextField {
                id: renameField
                Layout.fillWidth: true
                text: root.selectedTemplate ? root.selectedTemplate.name : ""
            }
            
            DialogButtonBox {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("Rename")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    enabled: renameField.text.length > 0
                }
                
                Button {
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (filterTemplateManager && root.selectedTemplate) {
                filterTemplateManager.renameTemplate(
                    root.selectedTemplate.name,
                    renameField.text
                )
                filterTemplates()
            }
        }
        
        onOpened: {
            renameField.text = root.selectedTemplate ? root.selectedTemplate.name : ""
            renameField.selectAll()
            renameField.forceActiveFocus()
        }
    }
    
    // 删除确认对话框
    Dialog {
        id: deleteConfirmDialog
        title: qsTr("Delete Template")
        width: 350
        modal: true
        closePolicy: Popup.CloseOnEscape
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            Label {
                text: qsTr("Are you sure you want to delete template '%1'?").arg(
                    root.selectedTemplate ? root.selectedTemplate.name : ""
                )
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            
            DialogButtonBox {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("Delete")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                }
                
                Button {
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (filterTemplateManager && root.selectedTemplate) {
                filterTemplateManager.deleteTemplate(root.selectedTemplate.name)
                root.selectedTemplate = null
                filterTemplates()
            }
        }
    }
    
    // 导入文件对话框
    FileDialog {
        id: importFileDialog
        title: qsTr("Import Templates")
        nameFilters: [qsTr("JSON Files (*.json)")]
        fileMode: FileDialog.OpenFile
        
        onAccepted: {
            if (filterTemplateManager) {
                var path = selectedFile.toString().replace("file:///", "")
                filterTemplateManager.importTemplates(path, false)
                filterTemplates()
            }
        }
    }
    
    // 导出文件对话框
    FileDialog {
        id: exportFileDialog
        title: qsTr("Export Templates")
        nameFilters: [qsTr("JSON Files (*.json)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        
        onAccepted: {
            if (filterTemplateManager) {
                var path = selectedFile.toString().replace("file:///", "")
                filterTemplateManager.exportTemplates(path)
            }
        }
    }
}

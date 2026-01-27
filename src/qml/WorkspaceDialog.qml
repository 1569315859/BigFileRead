import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

/**
 * @brief 工作区管理对话框
 * 
 * 提供保存、加载、管理工作区的功能
 */
Dialog {
    id: root
    
    title: qsTr("Workspace Manager")
    width: 650
    height: 550
    modal: true
    closePolicy: Popup.CloseOnEscape
    
    // 设置居中显示
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    
    // 当前状态（用于保存）
    property var currentState: ({})
    
    // 选中的工作区
    property var selectedWorkspace: null
    
    // 加载工作区信号
    signal workspaceLoaded(var workspace)
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // 工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Button {
                text: qsTr("Save Current State")
                icon.name: "document-save"
                onClicked: saveDialog.open()
            }

            Button {
                text: qsTr("Quick Save")
                icon.name: "document-save-as"
                enabled: workspaceManager && workspaceManager.currentWorkspace.length > 0
                onClicked: {
                    if (workspaceManager) {
                        workspaceManager.setCurrentState(root.currentState)
                        workspaceManager.quickSave()
                    }
                }

                ToolTip.text: qsTr("Update current workspace: %1").arg(
                    workspaceManager ? workspaceManager.currentWorkspace : ""
                )
                ToolTip.visible: hovered
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
                enabled: root.selectedWorkspace !== null
                onClicked: exportFileDialog.open()
            }
        }
        
        // 当前工作区指示
        RowLayout {
            Layout.fillWidth: true
            visible: workspaceManager && workspaceManager.currentWorkspace.length > 0
            
            Label {
                text: qsTr("Current Workspace:")
                font.bold: true
            }
            Label {
                text: workspaceManager ? workspaceManager.currentWorkspace : ""
                color: "#2196F3"
            }
        }
        
        // 工作区列表
        GroupBox {
            title: qsTr("Saved Workspaces (%1)").arg(
                workspaceManager ? workspaceManager.workspaceCount : 0
            )
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                
                ListView {
                    id: workspaceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    model: workspaceManager ? workspaceManager.workspaces : []
                    
                    delegate: ItemDelegate {
                        width: workspaceList.width
                        height: 80
                        
                        required property var modelData
                        required property int index
                        
                        highlighted: root.selectedWorkspace && 
                                     root.selectedWorkspace.name === modelData.name
                        
                        onClicked: {
                            root.selectedWorkspace = modelData
                        }
                        
                        onDoubleClicked: {
                            root.selectedWorkspace = modelData
                            applySelectedWorkspace()
                        }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12
                            
                            // 工作区图标
                            Rectangle {
                                width: 48
                                height: 48
                                radius: 8
                                color: workspaceManager && 
                                       workspaceManager.currentWorkspace === modelData.name 
                                       ? "#2196F3" : "#607D8B"
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.name.charAt(0).toUpperCase()
                                    font.pixelSize: 20
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                
                                RowLayout {
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    
                                    Label {
                                        visible: workspaceManager && 
                                                 workspaceManager.currentWorkspace === modelData.name
                                        text: qsTr("Current")
                                        font.pixelSize: 10
                                        padding: 4
                                        color: "white"
                                        background: Rectangle {
                                            color: "#4CAF50"
                                            radius: 3
                                        }
                                    }
                                }
                                
                                Label {
                                    text: modelData.description || modelData.filePath || qsTr("No description")
                                    font.pixelSize: 12
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                RowLayout {
                                    spacing: 16
                                    
                                    Label {
                                        text: qsTr("Created: %1").arg(formatDate(modelData.createdAt))
                                        font.pixelSize: 10
                                        opacity: 0.5
                                    }

                                    Label {
                                        text: qsTr("Last used: %1").arg(formatDate(modelData.lastUsed))
                                        font.pixelSize: 10
                                        opacity: 0.5
                                    }
                                }
                            }
                            
                            // 操作按钮
                            ColumnLayout {
                                spacing: 4
                                
                                ToolButton {
                                    icon.name: "edit-rename"
                                    ToolTip.text: qsTr("Rename")
                                    ToolTip.visible: hovered
                                    onClicked: {
                                        root.selectedWorkspace = modelData
                                        renameDialog.open()
                                    }
                                }

                                ToolButton {
                                    icon.name: "edit-delete"
                                    ToolTip.text: qsTr("Delete")
                                    ToolTip.visible: hovered
                                    onClicked: {
                                        root.selectedWorkspace = modelData
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }
                    }
                    
                    // 空列表提示
                    Label {
                        anchors.centerIn: parent
                        visible: workspaceList.count === 0
                        text: qsTr("No saved workspaces\nUse 'Save Current State' to create one")
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                    }
                }
            }
        }
        
        // 工作区详情
        GroupBox {
            title: qsTr("Workspace Details")
            Layout.fillWidth: true
            visible: root.selectedWorkspace !== null

            GridLayout {
                anchors.fill: parent
                columns: 4
                columnSpacing: 16
                rowSpacing: 8

                Label { text: qsTr("File:"); font.bold: true }
                Label { 
                    text: root.selectedWorkspace ? root.selectedWorkspace.filePath || qsTr("None") : ""
                    Layout.columnSpan: 3
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label { text: qsTr("Filter:"); font.bold: true }
                Label { 
                    text: root.selectedWorkspace && root.selectedWorkspace.filterKeyword 
                          ? root.selectedWorkspace.filterKeyword : qsTr("None")
                }

                Label { text: qsTr("Bookmarks:"); font.bold: true }
                Label { 
                    text: root.selectedWorkspace && root.selectedWorkspace.bookmarks 
                          ? root.selectedWorkspace.bookmarks.length + qsTr(" items")
                          : "0" + qsTr(" items")
                }
            }
        }
        
        // 底部按钮
        DialogButtonBox {
            Layout.fillWidth: true

            Button {
                text: qsTr("Load")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                enabled: root.selectedWorkspace !== null
            }

            Button {
                text: qsTr("Close")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }

    function formatDate(isoString) {
        if (!isoString) return qsTr("Unknown")
        var date = new Date(isoString)
        return date.toLocaleDateString()
    }
    
    function applySelectedWorkspace() {
        if (root.selectedWorkspace && workspaceManager) {
            var workspace = workspaceManager.loadWorkspace(root.selectedWorkspace.name)
            root.workspaceLoaded(workspace)
            root.close()
        }
    }
    
    onAccepted: applySelectedWorkspace()
    
    onOpened: {
        root.selectedWorkspace = null
    }
    
    // 保存工作区对话框
    Dialog {
        id: saveDialog
        title: qsTr("Save Workspace")
        width: 400
        modal: true
        closePolicy: Popup.CloseOnEscape
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label { text: qsTr("Workspace Name:") }

            TextField {
                id: newWorkspaceName
                Layout.fillWidth: true
                placeholderText: qsTr("Enter workspace name")
            }

            Label { text: qsTr("Description (optional):") }

            TextArea {
                id: newWorkspaceDesc
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                placeholderText: qsTr("Enter description")
            }

            Label {
                text: qsTr("Current file, filter, bookmarks and view settings will be saved.")
                font.pixelSize: 11
                opacity: 0.7
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            DialogButtonBox {
                Layout.fillWidth: true

                Button {
                    text: qsTr("Save")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    enabled: newWorkspaceName.text.length > 0
                }

                Button {
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (workspaceManager) {
                workspaceManager.setCurrentState(root.currentState)
                workspaceManager.saveWorkspace(
                    newWorkspaceName.text,
                    newWorkspaceDesc.text
                )
                newWorkspaceName.text = ""
                newWorkspaceDesc.text = ""
            }
        }
        
        onOpened: {
            newWorkspaceName.text = ""
            newWorkspaceDesc.text = ""
            newWorkspaceName.forceActiveFocus()
        }
    }
    
    // 重命名对话框
    Dialog {
        id: renameDialog
        title: qsTr("Rename Workspace")
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
                text: root.selectedWorkspace ? root.selectedWorkspace.name : ""
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
            if (workspaceManager && root.selectedWorkspace) {
                workspaceManager.renameWorkspace(
                    root.selectedWorkspace.name,
                    renameField.text
                )
            }
        }
        
        onOpened: {
            renameField.text = root.selectedWorkspace ? root.selectedWorkspace.name : ""
            renameField.selectAll()
            renameField.forceActiveFocus()
        }
    }
    
    // 删除确认对话框
    Dialog {
        id: deleteConfirmDialog
        title: qsTr("Delete Workspace")
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
                text: qsTr("Are you sure you want to delete workspace '%1'?").arg(
                    root.selectedWorkspace ? root.selectedWorkspace.name : ""
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
            if (workspaceManager && root.selectedWorkspace) {
                workspaceManager.deleteWorkspace(root.selectedWorkspace.name)
                root.selectedWorkspace = null
            }
        }
    }
    
    // 导入文件对话框
    FileDialog {
        id: importFileDialog
        title: qsTr("Import Workspace")
        nameFilters: [qsTr("JSON Files (*.json)")]
        fileMode: FileDialog.OpenFile

        onAccepted: {
            if (workspaceManager) {
                var path = selectedFile.toString().replace("file:///", "")
                workspaceManager.importWorkspace(path, false)
            }
        }
    }

    // 导出文件对话框
    FileDialog {
        id: exportFileDialog
        title: qsTr("Export Workspace")
        nameFilters: [qsTr("JSON Files (*.json)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        
        onAccepted: {
            if (workspaceManager && root.selectedWorkspace) {
                var path = selectedFile.toString().replace("file:///", "")
                workspaceManager.exportWorkspace(root.selectedWorkspace.name, path)
            }
        }
    }
}

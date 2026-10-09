import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1120
    height: 720
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "Modern GL Viewer"
    color: "#080b12"

    readonly property color ink: "#f4f3ef"
    readonly property color muted: "#9298a8"
    readonly property color accent: "#e8ff59"
    readonly property color panel: "#121722"
    readonly property color stroke: "#29303d"

    function shortPath(url) {
        const path = url.toString().replace("file://", "")
        const pieces = path.split("/")
        return pieces.length > 2
                ? "…/" + pieces[pieces.length - 2] + "/" + pieces[pieces.length - 1]
                : path
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#151b27" }
            GradientStop { position: 0.48; color: "#090d15" }
            GradientStop { position: 1.0; color: "#05070b" }
        }
    }

    Rectangle {
        width: 460
        height: 460
        radius: 230
        x: root.width - 260
        y: -230
        color: "#16e8ff59"
        border.color: "#30e8ff59"
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 52
        spacing: 0

        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                width: 42
                height: 42
                radius: 10
                color: root.accent

                Text {
                    anchors.centerIn: parent
                    text: "M"
                    color: "#090b0f"
                    font.pixelSize: 22
                    font.weight: Font.Black
                }
            }

            ColumnLayout {
                spacing: 1
                Text {
                    text: "MODERN GL VIEWER"
                    color: root.ink
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.4
                }
                Text {
                    text: "REAL-TIME ENGINE WORKBENCH"
                    color: root.muted
                    font.pixelSize: 10
                    font.letterSpacing: 1.8
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "QUIT"
                flat: true
                onClicked: Qt.quit()
                contentItem: Text {
                    text: parent.text
                    color: root.muted
                    font.pixelSize: 11
                    font.letterSpacing: 1.5
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item { Layout.preferredHeight: 68 }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 72

            ColumnLayout {
                Layout.preferredWidth: 430
                Layout.alignment: Qt.AlignTop
                spacing: 22

                Text {
                    Layout.fillWidth: true
                    text: "Build worlds.\nInspect every frame."
                    color: root.ink
                    font.pixelSize: 49
                    font.weight: Font.Black
                    font.letterSpacing: -1.7
                    lineHeight: 0.96
                }

                Text {
                    Layout.fillWidth: true
                    text: "Load Wavefront geometry and runtime GLSL into the backend-neutral rendering engine."
                    color: root.muted
                    font.pixelSize: 16
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: 18
                    Repeater {
                        model: ["OPENGL 4.1", "C++20", "LIVE GLSL"]
                        delegate: Text {
                            required property string modelData
                            text: modelData
                            color: root.accent
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.3
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.maximumWidth: 510
                Layout.alignment: Qt.AlignTop
                implicitHeight: launchPanel.implicitHeight + 52
                radius: 18
                color: root.panel
                border.color: root.stroke
                border.width: 1

                ColumnLayout {
                    id: launchPanel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 26
                    spacing: 15

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "LAUNCH CONFIGURATION"
                            color: root.ink
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.3
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: menuController.hasError ? "#ff6b6b" : root.accent
                        }
                    }

                    AssetRow {
                        label: "MODEL"
                        value: root.shortPath(menuController.modelSource)
                        onBrowseRequested: modelDialog.open()
                    }
                    AssetRow {
                        label: "VERTEX SHADER"
                        value: root.shortPath(menuController.vertexShaderSource)
                        onBrowseRequested: vertexDialog.open()
                    }
                    AssetRow {
                        label: "FRAGMENT SHADER"
                        value: root.shortPath(menuController.fragmentShaderSource)
                        onBrowseRequested: fragmentDialog.open()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 5

                        Button {
                            text: "RESTORE DEFAULTS"
                            flat: true
                            onClicked: menuController.restoreDefaults()
                            contentItem: Text {
                                text: parent.text
                                color: root.muted
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1.1
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: menuController.status
                            color: menuController.hasError ? "#ff8989" : root.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.maximumWidth: 180
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        text: "LAUNCH ENGINE  →"
                        onClicked: menuController.launchEngine()
                        background: Rectangle {
                            radius: 10
                            color: parent.down ? "#cfe63f" : root.accent
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#090b0f"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 1.2
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        implicitHeight: 1
                        color: root.stroke
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "COURSE"
                            color: root.ink
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.3
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "GREENWICH"
                            flat: true
                            onClicked: menuController.useGreenwich()
                            contentItem: Text {
                                text: parent.text
                                color: root.muted
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1.1
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: clubField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            enabled: menuController.hasClubDirectory
                            placeholderText: menuController.hasClubDirectory
                                             ? "UK golf club, or a county"
                                             : "No course backend configured"
                            text: menuController.clubQuery
                            color: root.ink
                            font.pixelSize: 13
                            onTextChanged: menuController.clubQuery = text
                            onAccepted: menuController.searchClubs()
                            background: Rectangle {
                                radius: 8
                                color: "#0b1019"
                                border.color: clubField.activeFocus ? root.accent : root.stroke
                                border.width: 1
                            }
                        }

                        Button {
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 44
                            text: "SEARCH"
                            enabled: menuController.hasClubDirectory
                            onClicked: menuController.searchClubs()
                            background: Rectangle {
                                radius: 8
                                color: parent.enabled
                                       ? (parent.hovered ? "#252d3a" : "#1a202b")
                                       : "#141922"
                                border.color: root.stroke
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? root.ink : "#5b6373"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1.0
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    // Only present while there is something to choose from.
                    // A permanently empty list reads as a broken search.
                    Rectangle {
                        Layout.fillWidth: true
                        visible: menuController.clubResults.length > 0
                        implicitHeight: Math.min(
                            168, Math.max(44, menuController.clubResults.length * 56))
                        radius: 10
                        color: "#0b1019"
                        border.color: root.stroke
                        border.width: 1
                        clip: true

                        ListView {
                            id: clubList
                            anchors.fill: parent
                            anchors.margins: 1
                            model: menuController.clubResults
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { }

                            delegate: ItemDelegate {
                                required property int index
                                required property string modelData

                                width: clubList.width
                                height: 56
                                onClicked: menuController.selectClub(index)

                                background: Rectangle {
                                    color: parent.hovered ? "#19202b" : "transparent"
                                }
                                contentItem: ColumnLayout {
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData
                                        color: root.ink
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        // Two clubs share a name often
                                        // enough that the address is what
                                        // tells them apart.
                                        text: index < menuController.clubAddresses.length
                                              ? menuController.clubAddresses[index] : ""
                                        color: "#6f7787"
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Text {
                            Layout.fillWidth: true
                            text: menuController.siteName
                            color: root.accent
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: menuController.siteSummary
                            color: root.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: menuController.weatherSummary
                            color: root.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: menuController.sunSummary
                            color: root.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "The engine opens in its native rendering window. The course's elevation sets the air the ball flies through, and its latitude and hour set the sun."
                        color: "#6f7787"
                        font.pixelSize: 10
                        lineHeight: 1.25
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "MGV / ENGINE SHELL"
                color: "#565e6d"
                font.pixelSize: 9
                font.letterSpacing: 1.5
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "OBJ  ·  CUSTOM SHADERS  ·  PHYSICS  ·  LIVE COURSE CONDITIONS"
                color: "#565e6d"
                font.pixelSize: 9
                font.letterSpacing: 1.5
            }
        }
    }

    component AssetRow: Rectangle {
        id: assetRow
        required property string label
        required property string value
        signal browseRequested()

        Layout.fillWidth: true
        implicitHeight: 64
        radius: 10
        color: "#0b1019"
        border.color: root.stroke
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: assetRow.label
                    color: "#747d8d"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                }
                Text {
                    Layout.fillWidth: true
                    text: assetRow.value
                    color: root.ink
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
            }

            Button {
                Layout.preferredWidth: 84
                Layout.preferredHeight: 44
                text: "BROWSE"
                onClicked: assetRow.browseRequested()
                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? "#252d3a" : "#1a202b"
                    border.color: root.stroke
                }
                contentItem: Text {
                    text: parent.text
                    color: root.ink
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.0
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    FileDialog {
        id: modelDialog
        title: "Select Wavefront model"
        nameFilters: ["Wavefront OBJ (*.obj)"]
        onAccepted: menuController.modelSource = selectedFile
    }
    FileDialog {
        id: vertexDialog
        title: "Select vertex shader"
        nameFilters: ["GLSL vertex shaders (*.vert *.glsl)", "All files (*)"]
        onAccepted: menuController.vertexShaderSource = selectedFile
    }
    FileDialog {
        id: fragmentDialog
        title: "Select fragment shader"
        nameFilters: ["GLSL fragment shaders (*.frag *.glsl)", "All files (*)"]
        onAccepted: menuController.fragmentShaderSource = selectedFile
    }
}

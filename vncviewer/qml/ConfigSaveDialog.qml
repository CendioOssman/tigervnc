import QtQuick 2.12
import Qt.labs.platform 1.0

FileDialog { // Do not use QtQuick.Dialogs.FileDialog to avoid QTBUG-55459.
    fileMode: FileDialog.SaveFile
    nameFilters: [ qsTr("TigerVNC configuration (*.tigervnc)"), qsTr("All files (*)"), ]
}

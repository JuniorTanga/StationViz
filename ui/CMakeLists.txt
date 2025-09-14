qt_add_qml_module(appStationViz
  URI ui
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/HeaderBar.qml
    qml/NavDrawer.qml
    qml/SldView.qml
    qml/Dashboard.qml
    qml/SclExplorer.qml
    qml/SldPage.qml
    qml/LiveMonitor.qml
    qml/TestConsole.qml
    qml/LogsReports.qml
    qml/Settings.qml
    qml/Theme.js
    qml/IconResolver.js
  RESOURCES
    qml/qml.qrc
)

target_link_libraries(appStationViz PRIVATE Qt6::Qml Qt6::Quick)

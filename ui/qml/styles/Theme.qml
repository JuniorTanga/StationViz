import QtQuick
//import "./ThemeSettings.js" as _ // (pas utilisé mais évite certains outils)

QtObject {
  id: t
  // Source de vérité globale

  property bool darkMode: false
  onDarkModeChanged: darkMode = !darkMode

  // ------ Palette Light ------
  readonly property color _L_text:        "#1F2937"
  readonly property color _L_subtext:     "#6B7280"
  readonly property color _L_window:      "#F7F8FA"
  readonly property color _L_panel:       "#FFFFFF"
  readonly property color _L_surface:     "#FFFFFF"
  readonly property color _L_edge:        "#607D8B"
  readonly property color _L_node:        "#263238"
  readonly property color _L_accent:      "#2F855A"
  readonly property color _L_chipBg:      "#F4F6F8"
  readonly property color _L_chipBorder:  "#D0D5DA"
  readonly property color _L_groupBg:     "#EEF2F6"
  readonly property color _L_groupBorder: "#D7DEE6"

  // ------ Palette Dark ------
  readonly property color _D_text:        "#E5E7EB"
  readonly property color _D_subtext:     "#9CA3AF"
  readonly property color _D_window:      "#121212"
  readonly property color _D_panel:       "#1E1E1E"
  readonly property color _D_surface:     "#1A1A1A"
  readonly property color _D_edge:        "#90A4AE"
  readonly property color _D_node:        "#CFD8DC"
  readonly property color _D_accent:      "#34D399"
  readonly property color _D_chipBg:      "#262A2F"
  readonly property color _D_chipBorder:  "#3A3F45"
  readonly property color _D_groupBg:     "#1F2429"
  readonly property color _D_groupBorder: "#2B3036"

  // ------ Exposition (switch) ------
  property color text:        darkMode ? _D_text        : _L_text
  property color subtext:     darkMode ? _D_subtext     : _L_subtext
  property color window:      darkMode ? _D_window      : _L_window
  property color panel:       darkMode ? _D_panel       : _L_panel
  property color surface:     darkMode ? _D_surface     : _L_surface
  property color edge:        darkMode ? _D_edge        : _L_edge
  property color node:        darkMode ? _D_node        : _L_node
  property color accent:      darkMode ? _D_accent      : _L_accent
  property color chipBg:      darkMode ? _D_chipBg      : _L_chipBg
  property color chipBorder:  darkMode ? _D_chipBorder  : _L_chipBorder
  property color groupBg:     darkMode ? _D_groupBg     : _L_groupBg
  property color groupBorder: darkMode ? _D_groupBorder : _L_groupBorder

  // Spécifique SLD
  property color feeder:      darkMode ? "#D1D5DB" : "#374151"
  property color coupler:     darkMode ? "#10B981" : "#2E7D32"
  property color transformer: darkMode ? "#90A4AE" : "#546E7A"

  // Etats
  property color ok:          darkMode ? "#34D399" : "#2E7D32"
  property color warn:        darkMode ? "#F59E0B" : "#ED6C02"
  property color err:         darkMode ? "#EF4444" : "#C62828"
}

import QtQuick

QtObject {
  id: t

  // Mode
  property bool darkMode: false

  // -------- Palette LIGHT --------
  readonly property color _L_text:        "#1F2937"
  readonly property color _L_subtext:     "#6B7280"
  readonly property color _L_window:      "#F5F7FB"
  readonly property color _L_panel:       "#FFFFFF"
  readonly property color _L_surface:     "#FFFFFF"
  readonly property color _L_border:      "#DBE3EE"   // NEW
  readonly property color _L_card:        "#F8FBFF"   // NEW
  readonly property color _L_accentText:  "#0F172A"   // NEW
  // SLD
  readonly property color _L_edge:        "#607D8B"
  readonly property color _L_node:        "#263238"
  readonly property color _L_accent:      "#2F855A"
  readonly property color _L_chipBg:      "#F4F6F8"
  readonly property color _L_chipBorder:  "#D0D5DA"
  readonly property color _L_groupBg:     "#EEF2F6"
  readonly property color _L_groupBorder: "#D7DEE6"
  readonly property color _L_feeder:      "#374151"
  readonly property color _L_coupler:     "#2E7D32"
  readonly property color _L_transformer: "#546E7A"
  readonly property color _L_ok:          "#2E7D32"
  readonly property color _L_warn:        "#ED6C02"
  readonly property color _L_err:         "#C62828"

  // -------- Palette DARK --------
  readonly property color _D_text:        "#E5E7EB"
  readonly property color _D_subtext:     "#9CA3AF"
  readonly property color _D_window:      "#121212"
  readonly property color _D_panel:       "#1E1E1E"
  readonly property color _D_surface:     "#1A1A1A"
  readonly property color _D_border:      "#2B3036"   // NEW
  readonly property color _D_card:        "#1F2429"   // NEW
  readonly property color _D_accentText:  "#E5E7EB"   // NEW
  // SLD
  readonly property color _D_edge:        "#90A4AE"
  readonly property color _D_node:        "#CFD8DC"
  readonly property color _D_accent:      "#34D399"
  readonly property color _D_chipBg:      "#262A2F"
  readonly property color _D_chipBorder:  "#3A3F45"
  readonly property color _D_groupBg:     "#1F2429"
  readonly property color _D_groupBorder: "#2B3036"
  readonly property color _D_feeder:      "#D1D5DB"
  readonly property color _D_coupler:     "#10B981"
  readonly property color _D_transformer: "#90A4AE"
  readonly property color _D_ok:          "#34D399"
  readonly property color _D_warn:        "#F59E0B"
  readonly property color _D_err:         "#EF4444"

  // -------- Exposition (utilisées par l'UI) --------
  property color text:        darkMode ? _D_text        : _L_text
  property color subtext:     darkMode ? _D_subtext     : _L_subtext
  property color window:      darkMode ? _D_window      : _L_window
  property color panel:       darkMode ? _D_panel       : _L_panel
  property color surface:     darkMode ? _D_surface     : _L_surface
  property color border:      darkMode ? _D_border      : _L_border      // NEW
  property color card:        darkMode ? _D_card        : _L_card        // NEW
  property color accentText:  darkMode ? _D_accentText  : _L_accentText  // NEW

  // SLD & chips
  property color edge:        darkMode ? _D_edge        : _L_edge
  property color node:        darkMode ? _D_node        : _L_node
  property color accent:      darkMode ? _D_accent      : _L_accent
  property color chipBg:      darkMode ? _D_chipBg      : _L_chipBg
  property color chipBorder:  darkMode ? _D_chipBorder  : _L_chipBorder
  property color groupBg:     darkMode ? _D_groupBg     : _L_groupBg
  property color groupBorder: darkMode ? _D_groupBorder : _L_groupBorder

  // Types SLD
  property color feeder:      darkMode ? _D_feeder      : _L_feeder
  property color coupler:     darkMode ? _D_coupler     : _L_coupler
  property color transformer: darkMode ? _D_transformer : _L_transformer

  // Etats
  property color ok:          darkMode ? _D_ok          : _L_ok
  property color warn:        darkMode ? _D_warn        : _L_warn
  property color err:         darkMode ? _D_err         : _L_err
}

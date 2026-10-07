"""Node names and virtual 1280x720 transforms for the F10 panel."""

PANEL_BACKGROUND_NAME = "ac_panel_bg"
PANEL_TEXT_SUFFIX = "_t"
PANEL_BACKGROUND_TRANSFORM = (140.0, 30.0, 1.0)
PANEL_FIXED_ELEMENTS = (
    ("ac_tab_set", (165.0, 82.0, (0.19, 0.42)),
     (172.0, 87.0, 0.22)),
    ("ac_tab_prot", (365.0, 82.0, (0.19, 0.42)),
     (372.0, 87.0, 0.22)),
    ("ac_tab_prev", (565.0, 82.0, (0.19, 0.42)),
     (572.0, 87.0, 0.22)),
    ("ac_tab_old", (765.0, 82.0, (0.19, 0.42)),
     (772.0, 87.0, 0.22)),
    ("ac_close", (990.0, 82.0, (0.13, 0.42)),
     (991.0, 92.0, 0.16)),
    ("ac_prev", (165.0, 540.0, (0.18, 0.45)),
     (167.0, 547.0, 0.22)),
    ("ac_next", (355.0, 540.0, (0.18, 0.45)),
     (357.0, 547.0, 0.22)),
    ("ac_apply", (755.0, 540.0, (0.18, 0.45)),
     (757.0, 547.0, 0.22)),
    ("ac_remove", (945.0, 540.0, (0.18, 0.45)),
     (947.0, 547.0, 0.22)),
)
PANEL_GROUP_ELEMENTS = tuple(
    (f"ac_group_{index + 1}",
     (160.0 + index * 317.0, 145.0, (0.30, 0.34)),
     (194.0 + index * 317.0, 148.0, 0.29))
    for index in range(3)
)
PANEL_SETTING_COUNTS = (17, 18, 17)
PANEL_SETTING_ELEMENTS = tuple(
    (f"ac_set_{sum(PANEL_SETTING_COUNTS[:column]) + row + 1}",
     (160.0 + column * 317.0, 191.0 + row * 25.0, (0.30, 0.21)),
     (202.0 + column * 317.0, 186.0 + row * 25.0, 0.27))
    for column, count in enumerate(PANEL_SETTING_COUNTS)
    for row in range(count)
)
PANEL_PROTECTION_ELEMENTS = (
    ("ac_prot_1", (160.0, 145.0, (0.93, 0.36)),
     (175.0, 149.0, 0.30)),
)
PANEL_PROTECTION_CARDS = tuple(
    (f"ac_prot_{index + 2}",
     (160.0 + (index % 3) * 317.0,
      195.0 + (index // 3) * 80.0, (0.30, 0.62)),
     (202.0 + (index % 3) * 317.0,
      203.0 + (index // 3) * 80.0, 0.27))
    for index in range(9)
)
PANEL_PROTECTION_SELECTORS = (
    ("ac_prot_11", (160.0, 445.0, (0.45, 0.48)),
     (225.0, 450.0, 0.40)),
    ("ac_prot_12", (640.0, 445.0, (0.45, 0.48)),
     (705.0, 450.0, 0.40)),
)
PANEL_COMPACT_ELEMENTS = (
    PANEL_FIXED_ELEMENTS + PANEL_GROUP_ELEMENTS + PANEL_SETTING_ELEMENTS +
    PANEL_PROTECTION_CARDS + PANEL_PROTECTION_SELECTORS
)
PANEL_TEXTS = (
    ("ac_title", (205.0, 48.0, 0.30)),
    ("ac_status", (205.0, 642.0, 0.24)),
)

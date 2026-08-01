"""Node names and virtual 1280x720 transforms for the F10 panel."""

PANEL_BACKGROUND_TRANSFORM = (140.0, 30.0, 1.0)
PANEL_FIXED_ELEMENTS = (
    ("panel_tab_settings", (165.0, 82.0, (0.19, 0.42)),
     (165.0, 87.0, 0.18)),
    ("panel_tab_protection", (365.0, 82.0, (0.19, 0.42)),
     (365.0, 87.0, 0.18)),
    ("panel_close", (990.0, 82.0, (0.13, 0.42)),
     (995.0, 87.0, 0.12)),
    ("panel_prev", (165.0, 540.0, (0.18, 0.45)),
     (165.0, 546.0, 0.18)),
    ("panel_next", (355.0, 540.0, (0.18, 0.45)),
     (355.0, 546.0, 0.18)),
    ("panel_apply", (755.0, 540.0, (0.18, 0.45)),
     (755.0, 546.0, 0.18)),
    ("panel_remove", (945.0, 540.0, (0.18, 0.45)),
     (945.0, 546.0, 0.18)),
)
PANEL_GROUP_ELEMENTS = tuple(
    (f"panel_group_{index + 1}",
     (160.0 + index * 317.0, 125.0, (0.30, 0.34)),
     (175.0 + index * 317.0, 128.0, 0.27))
    for index in range(3)
)
PANEL_SETTING_COUNTS = (17, 18, 10)
PANEL_SETTING_ELEMENTS = tuple(
    (f"panel_setting_row_{sum(PANEL_SETTING_COUNTS[:column]) + row + 1}",
     (160.0 + column * 317.0, 162.0 + row * 25.0, (0.30, 0.23)),
     (190.0 + column * 317.0, 163.0 + row * 25.0, 0.24))
    for column, count in enumerate(PANEL_SETTING_COUNTS)
    for row in range(count)
)
PANEL_PROTECTION_ELEMENTS = (
    ("panel_protection_row_1", (160.0, 145.0, (0.93, 0.36)),
     (175.0, 151.0, 0.27)),
)
PANEL_PROTECTION_CARDS = tuple(
    (f"panel_protection_row_{index + 2}",
     (160.0 + (index % 3) * 317.0,
      195.0 + (index // 3) * 80.0, (0.30, 0.62)),
     (190.0 + (index % 3) * 317.0,
      205.0 + (index // 3) * 80.0, 0.24))
    for index in range(9)
)
PANEL_PROTECTION_SELECTORS = (
    ("panel_protection_row_11", (160.0, 445.0, (0.45, 0.48)),
     (205.0, 452.0, 0.36)),
    ("panel_protection_row_12", (640.0, 445.0, (0.45, 0.48)),
     (685.0, 452.0, 0.36)),
)
PANEL_COMPACT_ELEMENTS = (
    PANEL_FIXED_ELEMENTS + PANEL_GROUP_ELEMENTS + PANEL_SETTING_ELEMENTS +
    PANEL_PROTECTION_CARDS + PANEL_PROTECTION_SELECTORS
)
PANEL_TEXTS = (
    ("panel_title", (205.0, 48.0, 0.30)),
    ("panel_status", (205.0, 642.0, 0.24)),
)

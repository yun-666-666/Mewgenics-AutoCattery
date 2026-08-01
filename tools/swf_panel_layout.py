"""Node names and virtual 1280x720 transforms for the F10 panel."""

PANEL_BACKGROUND_TRANSFORM = (140.0, 30.0, 1.0)
PANEL_FIXED_ELEMENTS = (
    ("panel_tab_settings", (165.0, 82.0, (0.19, 0.38)),
     (165.0, 88.0, 0.13)),
    ("panel_tab_protection", (365.0, 82.0, (0.19, 0.38)),
     (365.0, 88.0, 0.13)),
    ("panel_close", (990.0, 82.0, (0.13, 0.38)),
     (990.0, 88.0, 0.09)),
    ("panel_prev", (165.0, 600.0, (0.18, 0.42)),
     (165.0, 607.0, 0.12)),
    ("panel_next", (355.0, 600.0, (0.18, 0.42)),
     (355.0, 607.0, 0.12)),
    ("panel_apply", (755.0, 600.0, (0.18, 0.42)),
     (755.0, 607.0, 0.12)),
    ("panel_remove", (945.0, 600.0, (0.18, 0.42)),
     (945.0, 607.0, 0.12)),
)
PANEL_GROUP_ELEMENTS = tuple(
    (f"panel_group_{index + 1}",
     (160.0 + index * 317.0, 125.0, (0.30, 0.30)),
     (175.0 + index * 317.0, 130.0, 0.18))
    for index in range(3)
)
PANEL_SETTING_COUNTS = (17, 18, 10)
PANEL_SETTING_ELEMENTS = tuple(
    (f"panel_setting_row_{sum(PANEL_SETTING_COUNTS[:column]) + row + 1}",
     (160.0 + column * 317.0, 158.0 + row * 25.0, (0.30, 0.22)),
     (175.0 + column * 317.0, 161.0 + row * 25.0, 0.18))
    for column, count in enumerate(PANEL_SETTING_COUNTS)
    for row in range(count)
)
PANEL_PROTECTION_ELEMENTS = tuple(
    (f"panel_protection_row_{index + 1}",
     (165.0, 145.0 + index * 36.0, (0.93, 0.30)),
     (180.0, 150.0 + index * 36.0, 0.27))
    for index in range(12)
)
PANEL_COMPACT_ELEMENTS = (
    PANEL_FIXED_ELEMENTS + PANEL_GROUP_ELEMENTS + PANEL_SETTING_ELEMENTS
)
PANEL_TEXTS = (
    ("panel_title", (205.0, 48.0, 0.30)),
    ("panel_status", (205.0, 662.0, 0.22)),
)

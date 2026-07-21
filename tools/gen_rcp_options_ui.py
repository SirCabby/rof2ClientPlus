#!/usr/bin/env python3
"""Generate the rof2ClientPlus options window as a STANDALONE skin file,
uifiles/default/EQUI_RcpOptions.xml, and add an <Include> for it to a copy of the stock
EQUI.xml (also written to uifiles/default). Both deploy into the client's uifiles/default
(base skin), so /rcpoptions loads under EVERY UI skin that inherits default's EQUI.xml --
a user can run their own custom skin on top. This replaces the earlier delivery (the
window rode inside the EQUI_OptionsWindow.xml override, force-loaded from uifiles/rcp by a
runtime redirect that hijacked the Options window and blocked custom skins).

WHY A STANDALONE FILE IS FINE (it was long believed to crash -- it does NOT):
A Jul-2026 "day of world-entry crashes" concluded a separate included window crashes the
client (illegal-instruction jump into .rsrc during "Parsing UI XML", c000001d, once past
~a dozen controls). CONFIRMED 2026-07-17 the real culprit was the RUNTIME include-merge:
the mod's OLD LoadSidl hook wrote a temp EQUI.xml with the <Include> merged in and had the
client RE-PARSE it (WriteTemporaryUI). A STATIC on-disk <Include> in the skin's real
EQUI.xml, parsed by the client's OWN normal UI load (how every community skin adds a
window), loads the full 129-control window fine -- confirmed in-game. So: no runtime EQUI
merge, no XMLRead redirect; ship the file and edit EQUI.xml on disk.
(Separately, a /loadskin rebuild frees our live window -- CSidlManagerBase::LoadSidl
0x870C60 is destructive -- so src/ui_manager.cpp's LoadSidl hook drops RcpOptionsUI's
cached handles on any EQUI.xml (re)load to stay crash-safe. That hook must NOT use
rcp->callbacks: CallbackManager is not instantiated in this build.)

LAYOUT: a TABBED window. The "tabs" are a strip of checkbox buttons + per-tab
control groups that the C++ shows/hides (a real SIDL TabBox/Page renders but
does not route mouse input when the window is instantiated by us at runtime,
so only proven primitives are used: Button/Label/Slider as direct Screen
children). Tab contents OVERLAP in the same region; one group visible at a
time. The Colors tab is Zeal-style: one button per nameplate color role, its
text tinted the live color at runtime (CRNormal@0x12c); clicking opens the
client's stock color picker (CColorPickerWnd::Open 0x659AF0).

ORDER CONTRACT: the ROLES list below must match the NpColorRole enum order in
src/nameplate.cpp, and control ScreenIDs must match src/rcp_options_ui.cpp.

Also: ONE ELEMENT PER LINE (no stock file ever inlines
`<Location><X>..</X>..`), no `--` inside XML comments, plain-ASCII Text.

This reads the VENDORED stock EQUI.xml (tools/stock-uifiles/EQUI.xml) to add the Include,
and expects uifiles/default/EQUI_OptionsWindow.xml to exist (run gen_option_overrides.py
first) only so it can strip any stale RcpOptions block out of it. Idempotent. Run order:
gen_option_overrides.py, then this.

Usage:  python3 tools/gen_rcp_options_ui.py
"""

import os

# ---- colors ----
WHITE = (255, 255, 255)
YELLOW = (255, 255, 0)

# ---- window + layout ----
WINDOW_CX = 624  # Wide enough for the 9-tab strip at the proven 64 px tab width (tab 8 right edge 612), with headroom.
WINDOW_CY = 424  # Nameplate tab needs ~356; the extra room lets the Ring-tab graphic combobox (pushed
                 # down a row by the melee-range checkbox) drop its list (bottom ~410) inside the window.
TAB_Y = 6            # tab-strip row
TAB_W, TAB_H = 64, 20
CONTENT_Y = 34       # first content row under the tab strip
COL_X = 12           # content column x
SLIDER_W = 190
VAL_X = 208          # value-label x (right of sliders)

# Nameplate color roles: MUST mirror the NpColorRole enum order in
# src/nameplate.cpp (index == role). Names are what players call the bands.
# (No "other guild": players with no special state keep the client's default color.)
ROLES = [
    "Con: even", "Con: yellow", "Con: red", "Con: green", "Con: light blue", "Con: blue",
    "Target", "PvP", "AFK", "Linkdead", "LFG", "Group", "Raid", "Roleplay",
    "My guild", "Corpse", "GM", "Player",
]


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def loc_size(x, y, cx, cy):
    return [
        "    <Location>",
        f"      <X>{x}</X>",
        f"      <Y>{y}</Y>",
        "    </Location>",
        "    <Size>",
        f"      <CX>{cx}</CX>",
        f"      <CY>{cy}</CY>",
        "    </Size>",
    ]


def color(tag, rgb):
    r, g, b = rgb
    return [
        f"    <{tag}>",
        f"      <R>{r}</R>",
        f"      <G>{g}</G>",
        f"      <B>{b}</B>",
        f"    </{tag}>",
    ]


def button(name, x, y, cx, cy, text, tooltip, rgb=WHITE):
    out = [f'  <Button item="{name}">', f"    <ScreenID>{name}</ScreenID>", "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <Style_Checkbox>true</Style_Checkbox>", f"    <TooltipReference>{esc(tooltip)}</TooltipReference>",
            f"    <Text>{esc(text)}</Text>"]
    out += color("TextColor", rgb)
    out += ["    <ButtonDrawTemplate>", "      <Normal>A_BtnNormal</Normal>", "      <Pressed>A_BtnPressed</Pressed>",
            "      <Flyby>A_BtnFlyby</Flyby>", "      <Disabled>A_BtnDisabled</Disabled>",
            "      <PressedFlyby>A_BtnPressedFlyby</PressedFlyby>", "    </ButtonDrawTemplate>", "  </Button>"]
    return out


def label(name, x, y, cx, cy, text, rgb=WHITE):
    out = [f'  <Label item="{name}">', f"    <ScreenID>{name}</ScreenID>", "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += [f"    <Text>{esc(text)}</Text>"]
    out += color("TextColor", rgb)
    out += ["    <NoWrap>true</NoWrap>", "  </Label>"]
    return out


def slider(name, x, y, cx, cy):
    out = [f'  <Slider item="{name}">', f"    <ScreenID>{name}</ScreenID>", "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <SliderArt>SDT_DefSlider</SliderArt>", "  </Slider>"]
    return out


def combobox(name, x, y, cx, cy, list_height=110):
    # A native RoF2 Combobox (WDT_Inner / BDT_Combo, matching stock combos like BUGW_BugTypes). The
    # C++ repopulates its <Choices> at runtime (CComboWnd::DeleteAll + InsertChoice) and polls
    # GetCurChoice, so the single seed choice below is just a placeholder before the first populate.
    out = [f'  <Combobox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>", "    <DrawTemplate>WDT_Inner</DrawTemplate>"]
    out += loc_size(x, y, cx, cy)
    out += color("TextColor", WHITE)
    out += [f"    <ListHeight>{list_height}</ListHeight>", "    <Button>BDT_Combo</Button>",
            "    <Choices>None</Choices>", "    <Style_Border>true</Style_Border>", "  </Combobox>"]
    return out


def listbox(name, x, y, cx, cy, col_width):
    # A native RoF2 Listbox (CListWnd) with one text column + a vertical scrollbar (Style_VScroll), the
    # same schema stock windows use (e.g. FW_FriendsList in EQUI_FriendsWnd.xml). The C++ fills its rows
    # at runtime (CListWnd::AddString) and polls GetCurSel, so no <Choices> are seeded here.
    out = [f'  <Listbox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>", "    <DrawTemplate>WDT_Inner</DrawTemplate>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <Columns>", f"      <Width>{col_width}</Width>", "    </Columns>",
            "    <Style_Border>true</Style_Border>", "    <Style_VScroll>true</Style_VScroll>", "  </Listbox>"]
    return out


def listbox_cols(name, x, y, cx, cy, col_widths):
    # Multi-column native Listbox: one <Columns> block per column (stock schema, e.g. GT_MemberList in
    # EQUI_GuildManagementWnd.xml). The C++ fills col 0 via AddString and the rest via SetItemText(row,
    # col, ...), clears the column-header row, and polls GetCurSel. Column N renders at a fixed x (sum
    # of prior widths), which vertically aligns that column's text -- how we right-align the state.
    out = [f'  <Listbox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>", "    <DrawTemplate>WDT_Inner</DrawTemplate>"]
    out += loc_size(x, y, cx, cy)
    for w in col_widths:
        out += ["    <Columns>", f"      <Width>{w}</Width>", "    </Columns>"]
    out += ["    <Style_Border>true</Style_Border>", "    <Style_VScroll>true</Style_VScroll>", "  </Listbox>"]
    return out


def build_controls():
    """Returns the control list; the order is also the <Pieces> order."""
    c = []

    # ---- Tab strip (checkbox buttons; C++ enforces radio behavior) ----
    tabs = [("Rcp_TabGeneral", "General", "General settings"),
            ("Rcp_TabMouse", "Mouse", "Mouse-look settings"),
            ("Rcp_TabNameplate", "Nameplate", "Nameplate toggles"),
            ("Rcp_TabColors", "Colors", "Nameplate colors"),
            ("Rcp_TabDisplay", "Display", "Display, chase-camera, and world settings"),
            ("Rcp_TabRing", "Ring", "Target ring settings"),
            ("Rcp_TabSounds", "Sounds", "Sound toggles"),
            ("Rcp_TabCombat", "Combat", "Floating combat damage"),
            ("Rcp_TabModels", "Models", "Classic vs modern item models")]
    for i, (name, text, tip) in enumerate(tabs):
        c.append((name, button, (name, COL_X + i * (TAB_W + 3), TAB_Y, TAB_W, TAB_H, text, tip)))

    # ---- Tab 1: Mouse ----
    y = CONTENT_Y
    c.append(("Rcp_Enabled", button, ("Rcp_Enabled", COL_X, y, 250, 20, "Enable mouse-look sensitivity",
                                      "Enable rof2ClientPlus mouse-look sensitivity")))
    y += 26
    c.append(("Rcp_SensXLabel", label, ("Rcp_SensXLabel", COL_X, y, 170, 14, "Sensitivity X (horizontal)")))
    c.append(("Rcp_SensX", slider, ("Rcp_SensX", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_SensXValue", label, ("Rcp_SensXValue", VAL_X, y + 16, 58, 16, "1.0", YELLOW)))
    y += 42
    c.append(("Rcp_SensYLabel", label, ("Rcp_SensYLabel", COL_X, y, 170, 14, "Sensitivity Y (vertical)")))
    c.append(("Rcp_SensY", slider, ("Rcp_SensY", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_SensYValue", label, ("Rcp_SensYValue", VAL_X, y + 16, 58, 16, "1.0", YELLOW)))
    y += 42
    c.append(("Rcp_SmoothLabel", label, ("Rcp_SmoothLabel", COL_X, y, 170, 14, "Smoothing")))
    c.append(("Rcp_Smooth", slider, ("Rcp_Smooth", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_SmoothValue", label, ("Rcp_SmoothValue", VAL_X, y + 16, 58, 16, "0.00", YELLOW)))
    y += 42
    c.append(("Rcp_LockMouse", button, ("Rcp_LockMouse", COL_X, y, 250, 20, "Lock mouse to window",
                                        "Confine the mouse cursor to the game window (released when you alt-tab away)")))
    y += 26
    c.append(("Rcp_Equip", button, ("Rcp_Equip", COL_X, y, 250, 20, "Right-click to equip",
                                    "Right-click a wearable item in a bag to auto-equip it into the best slot "
                                    "(a clicky item casts instead; Alt+right-click equips it). Same as /rcpequip.")))

    # (The former Camera tab's chase-camera controls now live at the bottom of the Display tab.)

    # ---- Tab 2: Nameplate (custom billboard + color toggles + sliders) ----
    # (Name generation is always active - not an option.)
    # Custom-font billboard master toggle (drives font_overlay, hides the default names).
    c.append(("Rcp_NpBillboard", button, ("Rcp_NpBillboard", COL_X, CONTENT_Y, 250, 20,
                                          "Custom nameplates (3D font + bars)",
                                          "Draw custom-font 3D nameplates and hide the default names (/rcpfont)")))
    # Per-bar toggles on one row (HP for all; mana/stamina self only).
    bar_y = CONTENT_Y + 22
    c.append(("Rcp_NpHpBar", button, ("Rcp_NpHpBar", COL_X, bar_y, 84, 20, "HP bar", "Show the health bar")))
    c.append(("Rcp_NpManaBar", button, ("Rcp_NpManaBar", COL_X + 88, bar_y, 84, 20, "Mana bar",
                                        "Show your mana bar (self only)")))
    c.append(("Rcp_NpStamBar", button, ("Rcp_NpStamBar", COL_X + 176, bar_y, 84, 20, "Stam bar",
                                        "Show your stamina bar (self only)")))
    # Color / target toggles.
    np = [("Rcp_NpConColors", "Con colors (NPCs)", "Tint NPC nameplates by con level"),
          ("Rcp_NpStateColors", "State colors (players)", "Tint players by guild/AFK/LFG/LD/roleplay/PVP; corpses too"),
          ("Rcp_NpTargetColor", "Target color", "Highlight the current target's nameplate"),
          ("Rcp_NpTargetBlink", "Target blink", "Pulse the target's nameplate (any coloring mode, PCs and NPCs)"),
          ("Rcp_NpTargetMarker", "Target name markers", "Wrap the target's name in brackets"),
          ("Rcp_NpTargetHealth", "Target health %", "Append the target's HP percent to its nameplate"),
          ("Rcp_NpHideSelf", "Hide own nameplate", "Blank your own nameplate (unless it is your target)")]
    np_y = CONTENT_Y + 44
    for i, (name, text, tip) in enumerate(np):
        c.append((name, button, (name, COL_X, np_y + i * 22, 250, 20, text, tip)))
    y = np_y + len(np) * 22 + 4
    c.append(("Rcp_BlinkSpeedLabel", label, ("Rcp_BlinkSpeedLabel", COL_X, y, 170, 14, "Blink speed (cycle time)")))
    c.append(("Rcp_BlinkSpeed", slider, ("Rcp_BlinkSpeed", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_BlinkSpeedValue", label, ("Rcp_BlinkSpeedValue", VAL_X, y + 16, 58, 16, "1.20s", YELLOW)))
    y += 40
    c.append(("Rcp_NpDistLabel", label, ("Rcp_NpDistLabel", COL_X, y, 200, 14, "Nameplate draw distance")))
    c.append(("Rcp_NpDist", slider, ("Rcp_NpDist", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_NpDistValue", label, ("Rcp_NpDistValue", VAL_X, y + 16, 58, 16, "5000", YELLOW)))

    # ---- Tab 3: Nameplate colors (Zeal-style role buttons; runtime tints the
    #      text via CRNormal and opens the stock color picker on click) ----
    for i, role in enumerate(ROLES):
        col, row = (0, i) if i < 9 else (1, i - 9)
        c.append((f"Rcp_Role{i}", button, (f"Rcp_Role{i}", COL_X + col * 132, CONTENT_Y + row * 20, 128, 18, role,
                                           f"Edit the '{role}' nameplate color")))

    # ---- Tab 4: Display (world/display settings + the former Camera tab's chase controls) ----
    y = CONTENT_Y
    c.append(("Rcp_NoFog", button, ("Rcp_NoFog", COL_X, y, 250, 20, "Remove distance fog",
                                    "Remove the client's distance fog haze in every zone, day and night (/rcpfog)")))
    y += 26
    c.append(("Rcp_ClassicSpellIcons", button, ("Rcp_ClassicSpellIcons", COL_X, y, 250, 20, "Classic spell icons",
                                                "Show the original pre-2013 spell book / gem / buff icon art instead "
                                                "of the revamped icons, swapped live (/rcpspellicons)")))
    y += 30
    c.append(("Rcp_FarClipLabel", label, ("Rcp_FarClipLabel", COL_X, y, 190, 14, "Terrain distance (0 = off)")))
    c.append(("Rcp_FarClip", slider, ("Rcp_FarClip", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_FarClipValue", label, ("Rcp_FarClipValue", VAL_X, y + 16, 58, 16, "off", YELLOW)))
    y += 42
    c.append(("Rcp_ActorClipLabel", label, ("Rcp_ActorClipLabel", COL_X, y, 190, 14, "Actor distance (0 = off)")))
    c.append(("Rcp_ActorClip", slider, ("Rcp_ActorClip", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_ActorClipValue", label, ("Rcp_ActorClipValue", VAL_X, y + 16, 58, 16, "off", YELLOW)))
    y += 48
    # Chase camera (folded in from the former Camera tab; C++ shows these with the Display group).
    c.append(("Rcp_CamHeader", label, ("Rcp_CamHeader", COL_X, y, 200, 14, "Chase camera", YELLOW)))
    y += 18
    c.append(("Rcp_ChaseEnabled", button, ("Rcp_ChaseEnabled", COL_X, y, 250, 20, "Enable chase camera",
                                           "Adjust the mouse-wheel third-person chase camera (/rcpchase)")))
    y += 22
    c.append(("Rcp_ChaseCollision", button, ("Rcp_ChaseCollision", COL_X, y, 250, 20, "Wall collision pull-in",
                                             "Pull the camera in when world geometry blocks the view")))
    y += 26
    c.append(("Rcp_ChaseDistLabel", label, ("Rcp_ChaseDistLabel", COL_X, y, 200, 14, "Max zoom out (0 = native)")))
    c.append(("Rcp_ChaseDist", slider, ("Rcp_ChaseDist", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_ChaseDistValue", label, ("Rcp_ChaseDistValue", VAL_X, y + 16, 58, 16, "native", YELLOW)))

    # ---- Tab 5: Target ring (solid-color donut under the target; /rcpring) ----
    y = CONTENT_Y
    c.append(("Rcp_RingEnabled", button, ("Rcp_RingEnabled", COL_X, y, 250, 20, "Enable target ring",
                                          "Draw a solid-color ring on the ground under your target (/rcpring)")))
    y += 22
    c.append(("Rcp_RingHideSelf", button, ("Rcp_RingHideSelf", COL_X, y, 250, 20, "Hide when targeting yourself",
                                           "Do not draw the ring when your target is yourself")))
    y += 22
    c.append(("Rcp_RingConColor", button, ("Rcp_RingConColor", COL_X, y, 250, 20, "Con colors (by target level)",
                                           "Color the ring by the target's con level instead of the fixed color below")))
    y += 22
    c.append(("Rcp_RingMelee", button, ("Rcp_RingMelee", COL_X, y, 250, 20, "Scale to melee range",
                                        "Size the ring's outer edge to the target's melee range (how close you must be to land a melee hit); overrides the outer radius")))
    y += 26
    # Color swatch: the C++ tints this button's text to the live ring color and opens the stock
    # color picker on click (same mechanism as the nameplate color roles). Used when con colors are off.
    c.append(("Rcp_RingColor", button, ("Rcp_RingColor", COL_X, y, 160, 18, "Ring color",
                                        "Click to pick the fixed target-ring color (used when con colors are off)")))
    y += 26
    c.append(("Rcp_RingOuterLabel", label, ("Rcp_RingOuterLabel", COL_X, y, 200, 14, "Outer radius")))
    c.append(("Rcp_RingOuter", slider, ("Rcp_RingOuter", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_RingOuterValue", label, ("Rcp_RingOuterValue", VAL_X, y + 16, 58, 16, "8.0", YELLOW)))
    y += 42
    c.append(("Rcp_RingInnerLabel", label, ("Rcp_RingInnerLabel", COL_X, y, 200, 14, "Inner radius (donut hole)")))
    c.append(("Rcp_RingInner", slider, ("Rcp_RingInner", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_RingInnerValue", label, ("Rcp_RingInnerValue", VAL_X, y + 16, 58, 16, "6.0", YELLOW)))
    y += 42
    c.append(("Rcp_RingOpacityLabel", label, ("Rcp_RingOpacityLabel", COL_X, y, 200, 14, "Opacity")))
    c.append(("Rcp_RingOpacity", slider, ("Rcp_RingOpacity", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_RingOpacityValue", label, ("Rcp_RingOpacityValue", VAL_X, y + 16, 58, 16, "85%", YELLOW)))
    y += 42
    # Ring graphic (texture) picker: a native Combobox. The C++ repopulates its choices from the .tga
    # files in uifiles/rcp/targetrings (CComboWnd::DeleteAll + InsertChoice) and polls GetCurChoice.
    c.append(("Rcp_RingGraphicLabel", label, ("Rcp_RingGraphicLabel", COL_X, y + 3, 84, 16, "Ring graphic:")))
    c.append(("Rcp_RingGraphic", combobox, ("Rcp_RingGraphic", COL_X + 86, y, 220, 22)))
    c.append(("Rcp_RingSpin", button, ("Rcp_RingSpin", COL_X, y + 28, 250, 20, "Rotate ring graphic",
                                       "Slowly rotate the ring graphic; when off, the graphic faces the target's heading")))

    # ---- Tab 6: Sounds (track + adjust individual sounds; /rcpsound) ----
    # A tracked-sound list you curate: pick a recently-played sound from the combobox to add it, click a
    # row in the scrollable list to select it, set its volume with the slider (0 = mute), or remove it.
    # The list is a native CListWnd (scrollbar, any number of rows); the C++ populates + polls it.
    y = CONTENT_Y
    c.append(("Rcp_SndAddLabel", label, ("Rcp_SndAddLabel", COL_X, y, 320, 14, "Add a played sound to the list:")))
    c.append(("Rcp_SndAddCombo", combobox, ("Rcp_SndAddCombo", COL_X, y + 16, 300, 22)))
    y += 44
    c.append(("Rcp_SndListLabel", label, ("Rcp_SndListLabel", COL_X, y, 320, 14,
                                          "Tracked sounds (click one to adjust; scroll for more):")))
    y += 18
    c.append(("Rcp_SndList", listbox, ("Rcp_SndList", COL_X, y, 300, 250, 282)))
    y += 256
    c.append(("Rcp_SndVolLabel", label, ("Rcp_SndVolLabel", COL_X, y + 2, 56, 16, "Volume")))
    c.append(("Rcp_SndVol", slider, ("Rcp_SndVol", COL_X + 60, y, 150, 16)))
    c.append(("Rcp_SndVolValue", label, ("Rcp_SndVolValue", COL_X + 218, y + 2, 60, 16, "-", YELLOW)))
    y += 26
    c.append(("Rcp_SndReset", button, ("Rcp_SndReset", COL_X, y, 220, 20, "Remove selected from list",
                                       "Stop tracking the selected sound; it plays normally again")))

    # ---- Tab 0: General (window title + chat timestamps) ----
    y = CONTENT_Y
    c.append(("Rcp_WindowTitle", button, ("Rcp_WindowTitle", COL_X, y, 340, 20, "Show character name in window title",
                                          "Put your logged-in character's name in the game window's title bar, "
                                          "restored when you camp. Same as /rcpwindow title.")))
    y += 26
    c.append(("Rcp_Timestamp", button, ("Rcp_Timestamp", COL_X, y, 320, 20, "Show chat timestamps",
                                        "Prefix every chat line with the local time, the way Zeal does "
                                        "(default [HH:MM:SS]). Same as /timestamp on|off.")))
    y += 26
    c.append(("Rcp_TimestampHint", label, ("Rcp_TimestampHint", COL_X, y, 400, 14,
                                           "Format is set with  /timestamp format <strftime>")))
    # Automatic AA experience (/rcpaaexp): gate AA XP by how far into the current level you are.
    y += 24
    c.append(("Rcp_AaExpHeader", label, ("Rcp_AaExpHeader", COL_X, y, 300, 14, "Automatic AA experience", YELLOW)))
    y += 18
    c.append(("Rcp_AaExpEnabled", button, ("Rcp_AaExpEnabled", COL_X, y, 360, 20, "Auto-manage AA experience",
                                           "Automatically switch AA experience on or off based on how far into "
                                           "the current level you are. Same as /rcpaaexp on|off.")))
    y += 26
    c.append(("Rcp_AaExpThreshLabel", label, ("Rcp_AaExpThreshLabel", COL_X, y, 200, 14, "Turn AA on above level XP")))
    c.append(("Rcp_AaExpThreshValue", label, ("Rcp_AaExpThreshValue", VAL_X, y + 16, 58, 16, "50%", YELLOW)))
    c.append(("Rcp_AaExpThresh", slider, ("Rcp_AaExpThresh", COL_X, y + 16, SLIDER_W, 16)))
    y += 40
    c.append(("Rcp_AaExpActiveLabel", label, ("Rcp_AaExpActiveLabel", COL_X, y, 200, 14, "AA % when active")))
    c.append(("Rcp_AaExpActiveValue", label, ("Rcp_AaExpActiveValue", VAL_X, y + 16, 58, 16, "100%", YELLOW)))
    c.append(("Rcp_AaExpActive", slider, ("Rcp_AaExpActive", COL_X, y + 16, SLIDER_W, 16)))
    y += 42
    c.append(("Rcp_AaExpStatus", label, ("Rcp_AaExpStatus", COL_X, y, 400, 14, "", YELLOW)))

    # ---- Tab 7: Combat (floating combat damage; /rcpfcd) ----
    y = CONTENT_Y
    c.append(("Rcp_FcdEnabled", button, ("Rcp_FcdEnabled", COL_X, y, 260, 20, "Enable floating combat damage",
                                         "Show rising, fading damage numbers over things as they are hit (/rcpfcd)")))
    y += 26
    # Source filters (two columns).
    c.append(("Rcp_FcdMine", button, ("Rcp_FcdMine", COL_X, y, 150, 20, "My damage",
                                      "Show damage dealt by you or your pet")))
    c.append(("Rcp_FcdMelee", button, ("Rcp_FcdMelee", COL_X + 156, y, 110, 20, "Melee",
                                       "Show melee (non-spell) hits")))
    y += 22
    c.append(("Rcp_FcdIncoming", button, ("Rcp_FcdIncoming", COL_X, y, 150, 20, "Damage to me",
                                          "Show damage dealt to you")))
    c.append(("Rcp_FcdSpells", button, ("Rcp_FcdSpells", COL_X + 156, y, 110, 20, "Spells",
                                        "Show spell / non-melee hits")))
    y += 22
    c.append(("Rcp_FcdOthers", button, ("Rcp_FcdOthers", COL_X, y, 150, 20, "Others' damage",
                                        "Show everyone else's damage (mob-on-mob, other players and pets)")))
    y += 28
    c.append(("Rcp_FcdBigLabel", label, ("Rcp_FcdBigLabel", COL_X, y, 180, 14, "Big-hit threshold")))
    c.append(("Rcp_FcdBig", slider, ("Rcp_FcdBig", COL_X, y + 16, SLIDER_W, 16)))
    c.append(("Rcp_FcdBigValue", label, ("Rcp_FcdBigValue", VAL_X, y + 16, 58, 16, "100", YELLOW)))
    y += 42
    # Color swatches: the C++ tints each button's text to the live color and opens the stock color
    # picker on click (same mechanism as the ring color / nameplate color roles).
    c.append(("Rcp_FcdColMine", button, ("Rcp_FcdColMine", COL_X, y, 200, 18, "My damage color",
                                         "Click to pick the color for your own damage")))
    y += 22
    c.append(("Rcp_FcdColIncoming", button, ("Rcp_FcdColIncoming", COL_X, y, 200, 18, "Damage-to-me color",
                                             "Click to pick the color for damage done to you")))
    y += 22
    c.append(("Rcp_FcdColOther", button, ("Rcp_FcdColOther", COL_X, y, 200, 18, "Others' damage color",
                                          "Click to pick the color for everyone else's damage")))
    y += 22
    c.append(("Rcp_FcdColCrit", button, ("Rcp_FcdColCrit", COL_X, y, 200, 18, "Big-hit color",
                                         "Click to pick the color for hits at or above the big-hit threshold")))

    # ---- Tab 8: Models (classic vs modern; weapons left, creatures right) ----
    # Two side-by-side scrollable lists. Click a row to toggle it between classic and modern; the buttons
    # flip them all. The C++ populates each native CListWnd (weapons from model_settings, creatures from
    # npc_model_settings) and polls its selected row + the two buttons.
    y = CONTENT_Y
    NPC_X = 320
    c.append(("Rcp_ModelHint", label, ("Rcp_ModelHint", COL_X, y, 300, 14,
                                       "Items - click a row to toggle:")))
    c.append(("Rcp_NpcHint", label, ("Rcp_NpcHint", NPC_X, y, 290, 14,
                                     "Creatures - click a row to toggle:")))
    y += 18
    # Two columns each: name (left) + state (its own column, so CLASSIC/MODERN align on the right).
    c.append(("Rcp_ModelList", listbox_cols, ("Rcp_ModelList", COL_X, y, 300, 300, [210, 72])))
    c.append(("Rcp_NpcList", listbox_cols, ("Rcp_NpcList", NPC_X, y, 290, 300, [210, 62])))
    y += 306
    c.append(("Rcp_ModelAllClassic", button, ("Rcp_ModelAllClassic", COL_X, y, 140, 20, "All classic",
                                              "Switch every listed weapon graphic to its classic version")))
    c.append(("Rcp_ModelAllNew", button, ("Rcp_ModelAllNew", COL_X + 148, y, 140, 20, "All modern",
                                          "Switch every listed weapon graphic back to its modern version")))
    c.append(("Rcp_NpcAllClassic", button, ("Rcp_NpcAllClassic", NPC_X, y, 135, 20, "All classic",
                                            "Switch every listed creature to its classic model")))
    c.append(("Rcp_NpcAllNew", button, ("Rcp_NpcAllNew", NPC_X + 143, y, 135, 20, "All modern",
                                        "Switch every listed creature back to its modern model")))
    y += 24
    c.append(("Rcp_ModelCount", label, ("Rcp_ModelCount", COL_X, y, 300, 16, "", YELLOW)))
    c.append(("Rcp_NpcCount", label, ("Rcp_NpcCount", NPC_X, y, 290, 16, "", YELLOW)))
    return c


CONTROLS = build_controls()

MARK_BEGIN = "  <!-- RCP-OPTIONS-BEGIN (generated by tools/gen_rcp_options_ui.py; do not hand-edit) -->"
MARK_END = "  <!-- RCP-OPTIONS-END -->"


def gen_rcp_block():
    """The marker-delimited block: all control defs, then the RcpOptions Screen."""
    lines = [MARK_BEGIN,
             "  <!-- The rof2ClientPlus options window. Shipped as a standalone skin window",
             "       file (EQUI_RcpOptions.xml) in uifiles/default and pulled in by an Include",
             "       added to that skin's EQUI.xml, the way a UI skin normally adds a window.",
             "       TABBED: the Rcp_Tab* buttons + per-tab control groups whose visibility the",
             "       mod toggles (a real TabBox does not route input when the window is created",
             "       at runtime). Groups overlap in the same region. -->", ""]
    for _name, fn, args in CONTROLS:
        lines += fn(*args)
        lines.append("")
    lines += ['  <Screen item="RcpOptions">', "    <ScreenID>RcpOptions</ScreenID>",
              "    <RelativePosition>false</RelativePosition>", "    <Location>", "      <X>10</X>", "      <Y>10</Y>",
              "    </Location>", "    <Size>", f"      <CX>{WINDOW_CX}</CX>", f"      <CY>{WINDOW_CY}</CY>", "    </Size>",
              "    <Text>rof2ClientPlus Options</Text>", "    <Style_VScroll>false</Style_VScroll>",
              "    <Style_HScroll>false</Style_HScroll>", "    <Style_Transparent>false</Style_Transparent>",
              "    <TooltipReference />", "    <DrawTemplate>WDT_Def</DrawTemplate>",
              "    <Style_Titlebar>true</Style_Titlebar>", "    <Style_Closebox>true</Style_Closebox>",
              "    <Style_Minimizebox>true</Style_Minimizebox>", "    <Style_Border>true</Style_Border>",
              "    <Style_Sizable>false</Style_Sizable>"]
    for name, _fn, _args in CONTROLS:
        lines.append(f"    <Pieces>{name}</Pieces>")
    lines += ["  </Screen>", MARK_END]
    return "\n".join(lines)


RCP_INCLUDE = "<Include>EQUI_RcpOptions.xml</Include>"
EQUI_MARKER = "<!-- rof2ClientPlus UI override: load the /rcpoptions window (standalone file) -->"


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "uifiles", "default")
    stock_dir = os.path.join(root, "tools", "stock-uifiles")
    os.makedirs(out_dir, exist_ok=True)

    # 1. Standalone window file EQUI_RcpOptions.xml -- its OWN composite member, exactly
    #    how any UI skin adds a window. (The prior "standalone crashes" were the mod's
    #    RUNTIME include-merge via WriteTemporaryUI, not this static on-disk delivery.)
    # The "rof2ClientPlus UI override" marker also flags this as OUR file so
    # `make install`'s bakstock() never backs it up (there is no stock original).
    header = ('<?xml version="1.0" encoding="us-ascii"?>\n'
              '<XML ID="EQInterfaceDefinitionLanguage">\n'
              '\t<!-- rof2ClientPlus UI override: the /rcpoptions window, a standalone skin file '
              '(no stock original, so never backed up). -->\n'
              '\t<Schema xmlns="EverQuestData" xmlns:dt="EverQuestDataTypes" />\n\n')
    standalone = header + gen_rcp_block() + "\n</XML>\n"
    with open(os.path.join(out_dir, "EQUI_RcpOptions.xml"), "w", encoding="ascii") as f:
        f.write(standalone)

    # 2. Composite EQUI.xml with our <Include> added (from the VENDORED stock; idempotent).
    equi = open(os.path.join(stock_dir, "EQUI.xml"), encoding="latin-1").read()
    if RCP_INCLUDE not in equi:
        equi = equi.replace("\t</Composite>", f"\t\t{EQUI_MARKER}\n\t\t{RCP_INCLUDE}\n\t</Composite>", 1)
    with open(os.path.join(out_dir, "EQUI.xml"), "w", encoding="latin-1") as f:
        f.write(equi)

    # 3. The Options-window override must NOT also carry the RcpOptions block now that it
    #    lives in its own file (a duplicate ScreenID would collide). gen_option_overrides.py
    #    regenerates it fresh (no block); strip a stale block here defensively.
    host = os.path.join(out_dir, "EQUI_OptionsWindow.xml")
    if os.path.exists(host):
        t = open(host, encoding="ascii").read()
        if MARK_BEGIN in t:
            pre, rest = t.split(MARK_BEGIN, 1)
            _, post = rest.split(MARK_END, 1)
            with open(host, "w", encoding="ascii") as f:
                f.write(pre + post.lstrip("\n"))

    print(f"wrote standalone EQUI_RcpOptions.xml ({len(CONTROLS)} controls + Screen) "
          f"and EQUI.xml (+1 Include)")


if __name__ == "__main__":
    main()

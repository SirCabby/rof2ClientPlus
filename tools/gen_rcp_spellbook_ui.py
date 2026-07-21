#!/usr/bin/env python3
"""Generate the rof2ClientPlus spell book window as a STANDALONE skin file,
uifiles/default/EQUI_RcpSpellBook.xml (the /rcpbook Zeal-style spell list).

Delivery is identical to EQUI_RcpOptions.xml: a standalone window file in the client's
base skin (uifiles/default), pulled in by an <Include> that gen_rcp_options_ui.py adds
to its generated EQUI.xml (that script is the SOLE writer of EQUI.xml; this one never
touches it). The mod instantiates the Screen lazily via CreateXWndFromTemplate and
drives every control by polling (src/spellbook_ui.cpp).

Window contents (ScreenIDs are the contract with src/spellbook_ui.cpp):
  Rcp_SbSearchLabel / Rcp_SbSearch   -- live substring filter on spell name
  Rcp_SbUnscribed                    -- "Show unscribed" checkbox
  Rcp_SbStatus                       -- row-count readout
  Rcp_SbList                         -- the multi-column spell list (native CListWnd;
                                        col 0 is the icon column, headers are SIDL
                                        <Heading>s so the native header-click sort fires)
  Rcp_SbDesc                         -- spell description pane (native CStmlWnd)

Same XML rules as the options generator: ONE ELEMENT PER LINE, no `--` inside
comments, plain-ASCII text, RelativePosition on every control, full
ButtonDrawTemplate on checkbox buttons. The "rof2ClientPlus UI override" marker in
the header flags this as OUR file so `make install`'s bakstock() never backs it up.

Usage:  python3 tools/gen_rcp_spellbook_ui.py
"""

import os

WHITE = (255, 255, 255)
YELLOW = (255, 255, 0)

# ---- window + layout (tight margins; the list/description split is draggable
# at runtime -- src/spellbook_ui.cpp resizes both controls, so the sizes here
# are just the initial split) ----
WINDOW_CX = 834
COL_X = 6
LIST_Y = 28
LIST_CX = WINDOW_CX - 2 * COL_X   # 822: columns + the vertical scrollbar inside the box
LIST_CY = 402
SPLIT_GAP = 8                     # divider strip between list and description (the drag-handle button)
DESC_Y = LIST_Y + LIST_CY + SPLIT_GAP   # 436
DESC_CY = 120
WINDOW_CY = DESC_Y + DESC_CY + 4        # 562

# Column widths + headings: MUST match the column enum in src/spellbook_ui.cpp
# (order and count; the C++ paints cells by these indexes). Col 0 is the icon
# ("Gem") column -- the C++ puts a CTextureAnimation in the cell, no text.
COLUMNS = [
    (30, "Gem"),
    (180, "Name"),
    (34, "Lvl"),
    (44, "Mana"),
    (52, "Cast"),
    (56, "Recast"),
    (60, "Duration"),
    (64, "Target"),
    (66, "Skill"),
    (108, "Category"),
    (110, "Subcategory"),
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


def editbox(name, x, y, cx, cy):
    # A native RoF2 Editbox (WDT_Inner / bordered, matching stock edit fields like
    # OMP_PathToAddressbookField in EQUI_OptionsWindow.xml). The C++ reads its live
    # InputText each frame; typing routes natively once the box has focus.
    out = [f'  <Editbox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <DrawTemplate>WDT_Inner</DrawTemplate>", "    <Style_Border>true</Style_Border>"]
    out += color("TextColor", WHITE)
    out += ["  </Editbox>"]
    return out


def combobox(name, x, y, cx, cy, list_height=300):
    # A native RoF2 Combobox (WDT_Inner / BDT_Combo, same schema as the proven
    # options-window combos). The C++ repopulates its choices at runtime and
    # polls GetCurChoice; the seed choice is a placeholder.
    out = [f'  <Combobox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>", "    <DrawTemplate>WDT_Inner</DrawTemplate>"]
    out += loc_size(x, y, cx, cy)
    out += color("TextColor", WHITE)
    out += [f"    <ListHeight>{list_height}</ListHeight>", "    <Button>BDT_Combo</Button>",
            "    <Choices>All spells</Choices>", "    <Style_Border>true</Style_Border>", "  </Combobox>"]
    return out


def gauge(name, x, y, cx, cy, eqtype, fill_rgb):
    # A native gauge wired to a client EQType. CRITICAL for this window: the
    # client's memorize machinery TICKS INSIDE the EQType-9 gauge evaluation
    # (disasm: gauge data-provider case 9 -> CSpellBookWnd mem ticker@0x75DB20,
    # which decrements the countdown and sends the server packet) - the stock
    # spell book had to be open only because IT hosted the only EQType-9 gauge.
    # Hosting one here makes memorization run off this window alone, and it is
    # the memorize progress bar the user sees. Schema mirrors SBW_Memorize_Gauge.
    r, g, b = fill_rgb
    out = [f'  <Gauge item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <GaugeOffsetX>-1</GaugeOffsetX>", "    <GaugeOffsetY>0</GaugeOffsetY>",
            "    <Style_Transparent>true</Style_Transparent>",
            f"    <FillTint>", f"      <R>{r}</R>", f"      <G>{g}</G>", f"      <B>{b}</B>", "    </FillTint>",
            "    <LinesFillTint>", "      <R>0</R>", "      <G>0</G>", "      <B>0</B>", "    </LinesFillTint>",
            "    <DrawLinesFill>false</DrawLinesFill>", f"    <EQType>{eqtype}</EQType>",
            "    <GaugeDrawTemplate>", "      <Fill>A_GaugeFill</Fill>",
            "      <LinesFill>A_GaugeLinesFill</LinesFill>", "    </GaugeDrawTemplate>", "  </Gauge>"]
    return out


def stmlbox(name, x, y, cx, cy):
    # A native RoF2 STMLbox (CStmlWnd) for the description pane; the C++ pushes
    # markup via the window's SetWindowText override (which routes to SetSTMLText).
    out = [f'  <STMLbox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>"]
    out += loc_size(x, y, cx, cy)
    out += ["    <DrawTemplate>WDT_Inner</DrawTemplate>", "    <Style_Border>true</Style_Border>",
            "    <Style_VScroll>true</Style_VScroll>", "  </STMLbox>"]
    return out


def listbox_headed(name, x, y, cx, cy, cols):
    # Multi-column native Listbox WITH column headings (stock schema, e.g.
    # OKP_KeyboardAssignmentList in EQUI_OptionsWindow.xml). Unlike the options
    # window's lists, the header row is KEPT here: heading clicks drive the native
    # sort state (SortCol/bSortAsc), which the C++ mirrors with its own re-sort.
    out = [f'  <Listbox item="{name}">', f"    <ScreenID>{name}</ScreenID>",
           "    <RelativePosition>true</RelativePosition>", "    <DrawTemplate>WDT_Inner</DrawTemplate>"]
    out += loc_size(x, y, cx, cy)
    for w, heading in cols:
        out += ["    <Columns>", f"      <Width>{w}</Width>", f"      <Heading>{esc(heading)}</Heading>", "    </Columns>"]
    out += ["    <Style_Border>true</Style_Border>", "    <Style_VScroll>true</Style_VScroll>", "  </Listbox>"]
    return out


def build_controls():
    """Returns the control list; the order is also the <Pieces> order."""
    c = []
    c.append(("Rcp_SbSearchLabel", label, ("Rcp_SbSearchLabel", COL_X, 7, 48, 16, "Search:")))
    c.append(("Rcp_SbSearch", editbox, ("Rcp_SbSearch", COL_X + 48, 3, 150, 22)))
    # Filter picker: choices are rebuilt at runtime from the spells on show
    # ("All spells" + every skill / category / subcategory present).
    c.append(("Rcp_SbFilter", combobox, ("Rcp_SbFilter", COL_X + 204, 3, 170, 22)))
    c.append(("Rcp_SbUnscribed", button, ("Rcp_SbUnscribed", COL_X + 380, 4, 130, 20, "Show unscribed",
                                          "List ONLY the spells your class could use at your current level that "
                                          "are not in your spell book (shown dimmed; they cannot be memorized)")))
    c.append(("Rcp_SbStatus", label, ("Rcp_SbStatus", COL_X + 516, 7, 148, 16, "", YELLOW)))
    # The memorize (EQType 9) + scribe (EQType 10) progress gauges: these DRIVE
    # the client's memorize/scribe countdowns (see gauge()); they render as thin
    # bars that fill while a memorize/scribe runs and are invisible when idle.
    c.append(("Rcp_SbMemGauge", gauge, ("Rcp_SbMemGauge", COL_X + 668, 6, 150, 8, 9, (200, 0, 200))))
    c.append(("Rcp_SbScribeGauge", gauge, ("Rcp_SbScribeGauge", COL_X + 668, 16, 150, 8, 10, (176, 64, 0))))
    c.append(("Rcp_SbList", listbox_headed, ("Rcp_SbList", COL_X, LIST_Y, LIST_CX, LIST_CY, COLUMNS)))
    # The splitter drag handle: a real (native) thin button filling the gap
    # between list and description. The C++ starts a drag when it is pressed
    # (native click routing - no coordinate-space assumptions) and then tracks
    # relative mouse movement.
    c.append(("Rcp_SbSplit", button, ("Rcp_SbSplit", COL_X, LIST_Y + LIST_CY, LIST_CX, SPLIT_GAP, "",
                                      "Drag up or down to resize the list against the description")))
    c.append(("Rcp_SbDesc", stmlbox, ("Rcp_SbDesc", COL_X, DESC_Y, LIST_CX, DESC_CY)))
    return c


CONTROLS = build_controls()


def gen_block():
    lines = ["  <!-- The rof2ClientPlus spell book window (/rcpbook): a Zeal-style searchable,",
             "       sortable spell list with a clickable icon column (pick up to memorize) and",
             "       a description pane. Shipped as a standalone skin window file pulled in by",
             "       an Include in this skin's EQUI.xml (written by gen_rcp_options_ui.py). -->", ""]
    for _name, fn, args in CONTROLS:
        lines += fn(*args)
        lines.append("")
    lines += ['  <Screen item="RcpSpellBook">', "    <ScreenID>RcpSpellBook</ScreenID>",
              "    <RelativePosition>false</RelativePosition>", "    <Location>", "      <X>60</X>", "      <Y>40</Y>",
              "    </Location>", "    <Size>", f"      <CX>{WINDOW_CX}</CX>", f"      <CY>{WINDOW_CY}</CY>", "    </Size>",
              "    <Text>Spell Book</Text>", "    <Style_VScroll>false</Style_VScroll>",
              "    <Style_HScroll>false</Style_HScroll>", "    <Style_Transparent>false</Style_Transparent>",
              "    <TooltipReference />", "    <DrawTemplate>WDT_Def</DrawTemplate>",
              "    <Style_Titlebar>true</Style_Titlebar>", "    <Style_Closebox>true</Style_Closebox>",
              "    <Style_Minimizebox>true</Style_Minimizebox>", "    <Style_Border>true</Style_Border>",
              "    <Style_Sizable>true</Style_Sizable>"]
    for name, _fn, _args in CONTROLS:
        lines.append(f"    <Pieces>{name}</Pieces>")
    lines += ["  </Screen>"]
    return "\n".join(lines)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "uifiles", "default")
    os.makedirs(out_dir, exist_ok=True)

    # The "rof2ClientPlus UI override" marker flags this as OUR file so `make
    # install`'s bakstock() never backs it up (there is no stock original).
    header = ('<?xml version="1.0" encoding="us-ascii"?>\n'
              '<XML ID="EQInterfaceDefinitionLanguage">\n'
              '\t<!-- rof2ClientPlus UI override: the /rcpbook spell book window, a standalone skin file '
              '(no stock original, so never backed up). Generated by tools/gen_rcp_spellbook_ui.py. -->\n'
              '\t<Schema xmlns="EverQuestData" xmlns:dt="EverQuestDataTypes" />\n\n')
    with open(os.path.join(out_dir, "EQUI_RcpSpellBook.xml"), "w", encoding="ascii") as f:
        f.write(header + gen_block() + "\n</XML>\n")

    total_w = sum(w for w, _h in COLUMNS)
    print(f"wrote standalone EQUI_RcpSpellBook.xml ({len(CONTROLS)} controls + Screen; "
          f"{len(COLUMNS)} columns, {total_w}px of {LIST_CX}px list)")


if __name__ == "__main__":
    main()

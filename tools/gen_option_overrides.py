#!/usr/bin/env python3
"""Generate rof2ClientPlus override copies of the stock RoF2 option windows with
text-clipping fixed. Reads the VENDORED pristine stock windows (tools/stock-uifiles/),
applies a deterministic layout transform, and writes the results into the repo's
uifiles/default/ folder -- these deploy into the client's uifiles/default skin so they
load as the base for EVERY UI skin (the /rcpoptions window rides inside the Options one).
Reading the vendored stock (not the deployed copy) keeps regeneration idempotent even
after install has overwritten the client's default files.

The stock Options window crams two columns into a 400px window with a font that
runs ~8-9 px/char, so the trailing word of many checkbox labels clips
("Receive Guild Invites" -> "Receive Guild", "Allow window resizing" -> "Allow
window", etc). EQ SIDL is a FLAT control list (containment is by <Pieces>/<Pages>
reference, not XML nesting) and the OPTW_OptionsSubwindows TabBox auto-stretches
to the parent Screen, so we can:
  * shift the whole right column right by SHIFT px (preserves all right-column
    internal alignment, since every right control moves together),
  * widen the Screen + the 9 option Pages to hold it, and
  * widen the checkbox buttons into the freed space so labels fit.

The Advanced Options window is a separate, simpler file: a few right-column
"Allow ... Shaders" labels and the asterisk footer are too narrow; widen them
and give the window a little more width for the footer.

The Item Display window (EQUI_ItemDisplay.xml) gets a layout fix of its own:
  * The IDW_ItemInfo%d labels (item name / Magic-Lore flags / Class / Race /
    equip-slots lines) are 20 px tall but stacked on a 16 px pitch, so each
    label overlaps the next by 4 px and the equip-slots line ("Primary
    Secondary") renders with the tops of its glyphs cut off. Re-pitch them to
    20 px so labels never overlap. This is safe to do in isolation: the client
    lays out everything BELOW the info lines at runtime relative to the last
    visible info label (ornament/aug socket screens = lastInfo.bottom+5, stat
    grid rows relative to the socket screen, window auto-height from the
    running Y), so the whole body just follows the labels down.
  * IDW_ModButton (the 10x10 base-vs-modified stats toggle) sits at (10,135) in
    stock, visible by default, and the client only MOVES it (to the left of the
    first free stat row) when the shown item actually has stat mods -- for every
    plain item it is a stray checkbox overlapping the ornamentation socket row.
    Park it offscreen at (-20,-20); the client repositions it with an absolute
    rect before it is ever legitimately shown, and IDW_ModButtonLabel is glued
    to the button's live rect at runtime, so both surface correctly when used.
  * Default window Size 400x230 -> 420x520 so a fresh window shows the full
    stat block without cutting content (the client still auto-fits per item).
  * IDW_ConvertButton (10x10 at (10,135)) + IDW_ConvertButtonLabel (an STMLbox at
    (10,60) 600x20) are REMOVED entirely. They belong to a NEWER client's
    item-convert feature and appear only in custom skins built from that newer
    default UI (CasterHybridUI/MeleeUI); the 2013-05-10 client has no code for
    these ScreenIDs (eqlib lists ConvertButton/ConvertStml for later builds
    only), so it never hides or repositions them. The STMLbox is non-transparent
    by default and its dark background paints OVER the Race info line (its fill
    nearly matches the window background, so it reads as "clipped text"), and
    the button renders as a stray grey box over the ornament socket row. Not
    present in the stock RoF2 file, so this is a no-op for the default override.
"""
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "uifiles" / "default"   # override copies deploy into the client's default skin


def _stock_uifiles_dir():
    """Pristine stock Options windows to transform. Default: the vendored copies in
    tools/stock-uifiles/ (so regeneration never reads our own deployed override). A CLI
    arg can point at a fresh client's uifiles/default to re-vendor from a new client."""
    if len(sys.argv) > 1:
        return Path(sys.argv[1])
    return REPO / "tools" / "stock-uifiles"


DEFAULT = _stock_uifiles_dir()

SHIFT = 50           # right-column horizontal shift
RIGHT_THRESHOLD = 186  # original X at/above this = right column
LEFT_CHECK_CX = 225  # widened width for left-column checkbox buttons
RIGHT_CHECK_CX = 185  # widened width for right-column checkbox buttons
SCREEN_CX_NEW = 470  # OptionsWindow width (was 400)
SCREEN_CY_NEW = 610  # OptionsWindow height. The stock XML CY is 530, but the
                     # tab pages stretch to the window and the General tab's
                     # content runs to ~y593; the client had been restoring a
                     # saved Height=610 (because the window was sizable). Once we
                     # make it non-sizable the XML CY is authoritative, so pin it
                     # to the height that actually shows every row.
PAGE_CX_NEW = 457    # option page width (was 387)

MAIN_PAGES = {
    "OptionsGeneralPage", "OptionsDisplayPage", "OptionsMousePage",
    "OptionsKeyboardPage", "OptionsChatPage", "OptionsColorPage",
    "OptionsMailPage", "OptionsVoicePage", "OptionsSharePage",
}

# The leading marker string "rof2ClientPlus UI override" also lets `make install`
# recognize an already-modified file and refuse to back it up over the pristine stock.
BANNER = ("<!-- rof2ClientPlus UI override: copy of the stock {name} with label\n"
          "     widths/positions adjusted so option text is not clipped. Installed into\n"
          "     the client's uifiles/default (base skin) by `make install`, so it loads\n"
          "     under every UI skin; the pristine stock is saved next to it as\n"
          "     {name}.rcpbak. Regenerated from the vendored stock (tools/stock-uifiles)\n"
          "     by tools/gen_option_overrides.py; prefer re-running that over hand-editing. -->\n")

BANNER_ITEMDISPLAY = (
    "<!-- rof2ClientPlus UI override: copy of the stock {name} with the\n"
    "     IDW_ItemInfo lines re-pitched 16->20 px (they are 20 px tall, so the stock\n"
    "     pitch made neighbours overlap and clipped the equip-slots line), the\n"
    "     IDW_ModButton stats-toggle parked offscreen until the client places it for\n"
    "     real (stock leaves it visible at (10,135) over the ornament socket row),\n"
    "     and a taller default window. Installed into the client's uifiles/default\n"
    "     (base skin) by `make install`; pristine stock saved next to it as\n"
    "     {name}.rcpbak. Regenerated from the vendored stock (tools/stock-uifiles)\n"
    "     by tools/gen_option_overrides.py; prefer re-running that over hand-editing. -->\n")

CTRL_RE = re.compile(r'(<(\w+) item="([^"]+)">)(.*?)(</\2>)', re.S)
# The stock EQUI_ItemDisplay.xml writes some controls as `item = "..."` (spaces).
CTRL_LOOSE_RE = re.compile(r'(<(\w+) item\s*=\s*"([^"]+)">)(.*?)(</\2>)', re.S)
LOC_X_RE = re.compile(r'(<Location>\s*<X>)(-?\d+)(</X>)', re.S)
LOC_Y_RE = re.compile(r'(<Location>\s*<X>-?\d+</X>\s*<Y>)(-?\d+)(</Y>)', re.S)
SIZE_CX_RE = re.compile(r'(<Size>\s*<CX>)(-?\d+)(</CX>)', re.S)
SIZE_CY_RE = re.compile(r'(<Size>\s*<CX>-?\d+</CX>\s*<CY>)(-?\d+)(</CY>)', re.S)


def get_x(body):
    m = LOC_X_RE.search(body)
    return int(m.group(2)) if m else None


def get_text(body):
    m = re.search(r'<Text>(.*?)</Text>', body, re.S)
    return m.group(1).strip() if m else ""


def is_checkbox(body):
    return "<Style_Checkbox>true</Style_Checkbox>" in body or "CheckboxWithText" in body


def set_x(body, new_x):
    return LOC_X_RE.sub(lambda m: m.group(1) + str(new_x) + m.group(3), body, count=1)


def set_y(body, new_y):
    return LOC_Y_RE.sub(lambda m: m.group(1) + str(new_y) + m.group(3), body, count=1)


def set_cx(body, new_cx):
    return SIZE_CX_RE.sub(lambda m: m.group(1) + str(new_cx) + m.group(3), body, count=1)


def set_cy(body, new_cy):
    return SIZE_CY_RE.sub(lambda m: m.group(1) + str(new_cy) + m.group(3), body, count=1)


def transform_options(text):
    changes = []

    def repl(m):
        open_tag, tag, item, body, close = m.groups()
        x = get_x(body)
        new_body = body

        # Widen the option pages / the window screen.
        if tag == "Page" and item in MAIN_PAGES:
            mcx = SIZE_CX_RE.search(new_body)
            if mcx and mcx.group(2) == "387":
                new_body = set_cx(new_body, PAGE_CX_NEW)
                changes.append(f"page {item}: CX 387->{PAGE_CX_NEW}")
        if tag == "Screen" and item == "OptionsWindow":
            new_body = set_cx(new_body, SCREEN_CX_NEW)
            new_body = set_cy(new_body, SCREEN_CY_NEW)
            changes.append(f"screen {item}: CX ->{SCREEN_CX_NEW}, CY ->{SCREEN_CY_NEW}")
            # The window has no resize borders (all SizableBorder* are false), so
            # Style_Sizable=true only serves to persist Width/Height in the char
            # UI ini and restore the stale 400px width over our XML default. Make
            # it non-sizable (like the Advanced window, which has no saved size)
            # so the XML CX is authoritative.
            if "<Style_Sizable>true</Style_Sizable>" in new_body:
                new_body = new_body.replace(
                    "<Style_Sizable>true</Style_Sizable>",
                    "<Style_Sizable>false</Style_Sizable>", 1)
                changes.append(f"screen {item}: Style_Sizable true->false")

        # Widen checkbox buttons (only those that actually carry text).
        if tag == "Button" and is_checkbox(new_body) and get_text(new_body) \
                and SIZE_CX_RE.search(new_body):
            cur = int(SIZE_CX_RE.search(new_body).group(2))
            if x is not None and x <= 20:
                if LEFT_CHECK_CX > cur:
                    new_body = set_cx(new_body, LEFT_CHECK_CX)
                    changes.append(f"L-check {item}: CX {cur}->{LEFT_CHECK_CX}")
            elif x is not None and x >= RIGHT_THRESHOLD:
                if RIGHT_CHECK_CX > cur:
                    new_body = set_cx(new_body, RIGHT_CHECK_CX)
                    changes.append(f"R-check {item}: CX {cur}->{RIGHT_CHECK_CX}")

        # Shift the whole right column right (classification uses original X).
        if x is not None and x >= RIGHT_THRESHOLD:
            new_body = set_x(new_body, x + SHIFT)

        return open_tag + new_body + close

    out = CTRL_RE.sub(repl, text)
    return out, changes


def transform_advanced(text):
    changes = []
    widen = {
        "ADOW_VertexShadersLabel": 155,
        "ADOW_11PixelShadersLabel": 155,
        "ADOW_14PixelShadersLabel": 155,
        "ADOW_20PixelShadersLabel": 155,
    }

    def repl(m):
        open_tag, tag, item, body, close = m.groups()
        new_body = body
        if item in widen:
            cur = int(SIZE_CX_RE.search(new_body).group(2))
            new_body = set_cx(new_body, widen[item])
            changes.append(f"{item}: CX {cur}->{widen[item]}")
        elif item == "ADOW_OnZoningLabel":  # asterisk footer
            new_body = set_x(new_body, 145)
            new_body = set_cx(new_body, 232)
            changes.append(f"{item}: X 162->145, CX 170->232")
        elif tag == "Screen" and item == "AdvancedDisplayOptionsWindow":
            new_body = set_cx(new_body, 385)
            changes.append(f"{item}: CX 360->385")
        return open_tag + new_body + close

    out = CTRL_RE.sub(repl, text)
    return out, changes


ITEMINFO_PITCH = 20      # was 16; labels are CY=20, so 16 made neighbours overlap
ITEMINFO_Y0 = 10         # first info line keeps its stock Y
ITEMDISPLAY_CX_NEW = 420  # window default size (was 400x230); the client still
ITEMDISPLAY_CY_NEW = 520  # auto-fits the window per shown item


REMOVED_ITEMDISPLAY_CONTROLS = ("IDW_ConvertButton", "IDW_ConvertButtonLabel")


def transform_itemdisplay(text):
    """Fix EQUI_ItemDisplay.xml: non-overlapping info-line pitch, ModButton parked
    offscreen, larger default window, dead newer-client convert controls removed.
    Works on the stock default file and on the custom-skin variants (any
    IDW_ItemInfo%d count) alike."""
    changes = []
    iteminfo_re = re.compile(r'IDW_ItemInfo(\d+)$')

    # Strip the newer-client convert controls (definitions + Pieces references).
    # Unknown to the 2013-05-10 client, and the label STML's opaque background
    # covers the Race info line. No-op when absent (stock file).
    for name in REMOVED_ITEMDISPLAY_CONTROLS:
        def_re = re.compile(
            r'[ \t]*<(\w+) item\s*=\s*"' + name + r'">.*?</\1>\s*?\n', re.S)
        text, n_def = def_re.subn('', text)
        piece_re = re.compile(r'[ \t]*<Pieces>' + name + r'</Pieces>\s*?\n')
        text, n_piece = piece_re.subn('', text)
        if n_def or n_piece:
            changes.append(f"removed {name}: {n_def} definition, {n_piece} piece ref")

    def repl(m):
        open_tag, tag, item, body, close = m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)
        new_body = body

        mi = iteminfo_re.match(item)
        if tag == "Label" and mi:
            n = int(mi.group(1))
            new_y = ITEMINFO_Y0 + ITEMINFO_PITCH * (n - 1)
            my = LOC_Y_RE.search(new_body)
            if my and int(my.group(2)) != new_y:
                new_body = set_y(new_body, new_y)
                changes.append(f"{item}: Y {my.group(2)}->{new_y}")
        elif tag == "Button" and item == "IDW_ModButton":
            mx, my = LOC_X_RE.search(new_body), LOC_Y_RE.search(new_body)
            new_body = set_x(new_body, -20)
            new_body = set_y(new_body, -20)
            changes.append(f"{item}: loc ({mx.group(2)},{my.group(2)})->(-20,-20)")
        elif tag == "Screen" and item == "ItemDisplayWindow":
            mcx, mcy = SIZE_CX_RE.search(new_body), SIZE_CY_RE.search(new_body)
            new_body = set_cx(new_body, ITEMDISPLAY_CX_NEW)
            new_body = set_cy(new_body, ITEMDISPLAY_CY_NEW)
            changes.append(f"{item}: size {mcx.group(2)}x{mcy.group(2)}"
                           f"->{ITEMDISPLAY_CX_NEW}x{ITEMDISPLAY_CY_NEW}")

        return open_tag + new_body + close

    out = CTRL_LOOSE_RE.sub(repl, text)
    return out, changes


def add_banner(text, name, banner=BANNER):
    return text.replace(
        '<XML ID="EQInterfaceDefinitionLanguage">',
        '<XML ID="EQInterfaceDefinitionLanguage">\n' + banner.format(name=name),
        1,
    )


def run(fname, transform, name, banner=BANNER):
    src = (DEFAULT / fname).read_text(encoding="latin-1")
    out, changes = transform(src)
    out = add_banner(out, name, banner)
    (OUT / fname).write_text(out, encoding="latin-1")
    print(f"=== {fname}: {len(changes)} changes ===")
    for c in changes:
        print("  " + c)
    return out


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    run("EQUI_OptionsWindow.xml", transform_options, "EQUI_OptionsWindow.xml")
    run("EQUI_AdvancedDisplayOptionsWnd.xml", transform_advanced,
        "EQUI_AdvancedDisplayOptionsWnd.xml")
    run("EQUI_ItemDisplay.xml", transform_itemdisplay, "EQUI_ItemDisplay.xml",
        banner=BANNER_ITEMDISPLAY)

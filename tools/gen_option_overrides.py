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

CTRL_RE = re.compile(r'(<(\w+) item="([^"]+)">)(.*?)(</\2>)', re.S)
LOC_X_RE = re.compile(r'(<Location>\s*<X>)(-?\d+)(</X>)', re.S)
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


def add_banner(text, name):
    return text.replace(
        '<XML ID="EQInterfaceDefinitionLanguage">',
        '<XML ID="EQInterfaceDefinitionLanguage">\n' + BANNER.format(name=name),
        1,
    )


def run(fname, transform, name):
    src = (DEFAULT / fname).read_text(encoding="latin-1")
    out, changes = transform(src)
    out = add_banner(out, name)
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

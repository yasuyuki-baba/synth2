#!/usr/bin/env python3
"""Render a vector GUI study; this is not a running native-plugin screenshot."""
import argparse
import ctypes as C
import ctypes.util
from html import escape
import math
from pathlib import Path
from preview_editor import ROOT, load_model


def render(model):
    a = model["appearance"]
    t = model["typography"]
    nodes = ['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="920" viewBox="0 0 1280 920">',
             f'<rect width="1280" height="920" rx="5" fill="{a["background"]}"/>',
             f'<path d="M0,5 Q0,0 5,0 H1275 Q1280,0 1280,5 V118 H0 Z" fill="{a["header"]}"/>',
             f'<path d="M0,3 H1280 M0,117 H1280" stroke="{a["headerEdge"]}"/>']

    def box(x, y, w, h, color=None, radius=3, stroke=None):
        color = color or a["control"]
        stroke = stroke or a["controlEdge"]
        nodes.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{color}" stroke="{stroke}"/>')

    def text(x, y, label, size=12, color=None, align="start", weight="normal",font="Arial, sans-serif"):
        color = color or a["text"]
        nodes.append(f'<text x="{x}" y="{y}" font-family="{font}" font-size="{size}" font-weight="{weight}" fill="{color}" text-anchor="{align}">{escape(str(label))}</text>')

    values = [0.] * 128
    for index, value in model["init"]["values"].items():
        values[int(index)] = value
    text(20, 53, "SYNTH", 29, weight="bold")
    text(126, 53, "2", 29, weight="bold")
    text(20, 74, "VIRTUAL ANALOG SYNTHESIZER", 9)
    box(240, 20, 638, 46, a["display"])
    text(256, 48, "Init", t["patchTitle"],a["displayText"],font="monospace")
    text(858, 48, "⌄", 14, a["muted"], "middle")
    for x, label in [(890, "‹"), (944, "›")]:
        box(x, 20, 44, 46)
        text(x+22, 49, label, 21, align="middle")
    text(1020, 30, "MASTER VOLUME", 10)
    text(1260, 30, "100%", 10, align="end")
    box(1020, 46, 240, 4, a["text"], 2, "none")
    nodes.append(f'<circle cx="1253" cy="48" r="6" fill="{a["text"]}"/>')
    text(20, 102, "0 sounds in library", 12)
    for x, w, label in [(756,64,"Add..."),(826,52,"Save")]:
        box(x, 82, w, 30)
        text(x+w/2, 101, label, 11, align="middle")
    for x, label in [(900,"Synth"),(1020,"MIDI"),(1140,"Library")]:
        box(x,82,119,30,"#442c34" if x==900 else a["control"])
        text(x+59,101,label,11,align="middle")

    for section in model["panels"]:
        x,y,w,h = section["bounds"]
        tone = section["accent"]
        box(x,y,w,h,a["filterPanel"] if section["title"]=="FILTER" else a["panel"],3,a["border"])
        text(x+14,y+21,section["title"],t["heading"],tone,weight="bold")
        nodes.append(f'<path d="M{x+14},{y+32} H{x+w-12}" stroke="{a["border"]}" stroke-width=".5"/>')
        for control in section["controls"]:
            x,y,w,h = control["bounds"]
            index = control["parameter"]
            meta = model["parameters"][index]
            value = values[index]
            text(x+w/2,y+11,control["label"],t["label"],a["muted"],"middle")
            if meta["toggle"] or meta["choices"]:
                box(x+4,y+20,w-8,min(28,h-24),radius=4)
                label = "ON" if value else "OFF"
                if not meta["toggle"]:
                    label = next((label for v,label in meta["choices"] if v==value),"Default" if value==0 else "Custom")
                label = "Rnd smooth" if label == "Random smooth" else label
                size = t["button"]
                text(x+w/2,y+37,label,size,align="middle")
                if meta["toggle"]:
                    nodes.append(f'<circle cx="{x+12}" cy="{y+32.5}" r="2" fill="{a["accent"] if value else a["track"]}"/>')
            else:
                diameter = control["diameter"]
                cx,cy = x+w/2,y+17+diameter/2
                radius = diameter/2-1
                normalized = (value-meta["minimum"])/(meta["maximum"]-meta["minimum"])
                end = math.radians(135+normalized*270)
                def point(a,r): return cx+r*math.cos(a),cy+r*math.sin(a)
                # Polyline arcs use the same 270-degree sweep as the native knob.
                for fraction, stroke in [(1,a["track"]),(normalized,control["tone"])]:
                    points = " ".join(f"{point(math.radians(135+270*fraction*i/60),radius)[0]:.2f},{point(math.radians(135+270*fraction*i/60),radius)[1]:.2f}" for i in range(61))
                    nodes.append(f'<polyline points="{points}" fill="none" stroke="{stroke}" stroke-width="1.25" stroke-linecap="round"/>')
                nodes.append(f'<circle cx="{cx}" cy="{cy}" r="{radius-3}" fill="{a["control"]}" stroke="{a["controlEdge"]}" stroke-width="1"/>')
                x1,y1 = point(end,(radius-3)*.56)
                x2,y2 = point(end,(radius-3)*.96)
                nodes.append(f'<path d="M{x1:.2f},{y1:.2f} L{x2:.2f},{y2:.2f}" stroke="{a["pointer"]}" stroke-width="1.8"/>')
                readout = meta["unitDisplay"][max(0,min(127,int(value+.5)))] if meta["unitDisplay"] else f"{value:g}"
                text(cx,y+29+diameter,readout,t["value"],a["muted"],"middle")
    text(20,818,"No sounds yet. Use Add sounds...",11,a["muted"])
    text(1090,818,"Click a value to type  /  Drag corner to scale",10,a["muted"],"end")
    box(1100,802,160,28,a["control"],4,a["accent"])
    text(1180,820,"Keyboard",12,a["text"],"middle")
    text(20,914,"Play: A W S E D F T G Y H U J K O L   /   Octave: Z down, X up",11,a["muted"])
    whites = [note for note in range(36,85) if note%12 not in (1,3,6,8,10)]
    key_width = 1240/len(whites)
    box(20,840,1240,60,"#080b0d",5,"#46505a")
    for i,note in enumerate(whites):
        box(20+i*key_width,840,key_width,60,"#dad7cd",2,"#5b5653")
        if note%12==0: text(20+(i+.5)*key_width,892,f"C{note//12-1}",9,"#647279","middle")
    for note in range(36,85):
        if note%12 in (1,3,6,8,10):
            ordinal = sum(n<note for n in whites)
            box(20+(ordinal-.3)*key_width,840,key_width*.6,38,a["control"],2,"#101013")
    nodes.append('</svg>')
    return "\n".join(nodes)


def png(svg, target):
    """Optional Linux librsvg/Cairo rendering without a window server/browser."""
    rsvg = C.CDLL(ctypes.util.find_library("rsvg-2"))
    cairo = C.CDLL(ctypes.util.find_library("cairo"))
    gobject = C.CDLL(ctypes.util.find_library("gobject-2.0"))
    rsvg.rsvg_handle_new_from_data.argtypes = [C.c_void_p,C.c_size_t,C.c_void_p]
    rsvg.rsvg_handle_new_from_data.restype = C.c_void_p
    rsvg.rsvg_handle_render_cairo.argtypes = [C.c_void_p,C.c_void_p]
    rsvg.rsvg_handle_render_cairo.restype = C.c_int
    cairo.cairo_image_surface_create.argtypes = [C.c_int,C.c_int,C.c_int]
    cairo.cairo_image_surface_create.restype = C.c_void_p
    cairo.cairo_create.argtypes = [C.c_void_p]
    cairo.cairo_create.restype = C.c_void_p
    cairo.cairo_surface_write_to_png.argtypes = [C.c_void_p,C.c_char_p]
    cairo.cairo_surface_write_to_png.restype = C.c_int
    cairo.cairo_destroy.argtypes = [C.c_void_p]
    cairo.cairo_surface_destroy.argtypes = [C.c_void_p]
    gobject.g_object_unref.argtypes = [C.c_void_p]
    encoded = svg.encode()
    handle = rsvg.rsvg_handle_new_from_data(encoded,len(encoded),None)
    if not handle: raise RuntimeError("Unable to load the generated SVG")
    surface = cairo.cairo_image_surface_create(0,1280,920)
    context = cairo.cairo_create(surface)
    try:
        if not rsvg.rsvg_handle_render_cairo(handle,context): raise RuntimeError("SVG rendering failed")
        if cairo.cairo_surface_write_to_png(surface,str(target).encode()): raise RuntimeError("PNG writing failed")
    finally:
        cairo.cairo_destroy(context)
        cairo.cairo_surface_destroy(surface)
        gobject.g_object_unref(handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter",type=Path,default=ROOT/"build-oos-core/ExportEditorLayout")
    parser.add_argument("--output",type=Path,default=ROOT/"build/editor/synth2-study.svg")
    parser.add_argument("--png",action="store_true")
    args = parser.parse_args()
    svg = render(load_model(args.exporter))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(svg)
    if args.png: png(svg,args.output.with_suffix('.png'))
    print(args.output)


if __name__ == '__main__': main()

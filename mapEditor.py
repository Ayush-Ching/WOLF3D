"""
Wolfenstein-style 2D map editor (Tkinter)
- Three layers: walls, floors, ceilings
- Palette of integer IDs -> colors (editable)
- Zoom with mouse wheel
- Pan by dragging with middle mouse button or right mouse button
- Left-click to paint the active layer with selected palette ID
- Shift+Left-click to erase (set 0)
- Save to map.txt (walls), floor.txt, ceil.txt with S
- Load existing files with L
- New map size with N
"""

import tkinter as tk
from tkinter import colorchooser, simpledialog, filedialog, messagebox
import math
import os

CELL_COUNT_W = 64   # default map size (width)
CELL_COUNT_H = 64   # default map size (height)
DEFAULT_TILE = 0

class MapEditor:
    def __init__(self, root):
        self.root = root
        root.title("Wolf3D Map Editor - walls/floor/ceil -> map.txt/floor.txt/ceil.txt")

        # Data
        self.w = CELL_COUNT_W
        self.h = CELL_COUNT_H
        self.walls = [[DEFAULT_TILE for _ in range(self.w)] for _ in range(self.h)]
        self.floors = [[DEFAULT_TILE for _ in range(self.w)] for _ in range(self.h)]
        self.ceils = [[DEFAULT_TILE for _ in range(self.w)] for _ in range(self.h)]

        # Viewing transform
        self.scale = 20.0  # pixel size of a cell (zoom)
        self.min_scale = 6.0
        self.max_scale = 120.0
        self.offset_x = 0.0  # world pixels offset
        self.offset_y = 0.0

        # Interaction
        self.dragging = False
        self.last_mouse = (0,0)
        self.panning_button = 2  # middle mouse button primary panning; right button fallback
        self.active_layer = 'walls'  # 'walls'|'floors'|'ceils'
        self.palette = {1: "#222222", 2: "#8b4513", 3: "#888888", 4: "#004400"}  # id->color
        self.selected_id = 1

        self.create_ui()
        self.redraw()

        # Bindings
        root.bind("<MouseWheel>", self.on_mousewheel)  # Windows
        root.bind("<Button-4>", self.on_mousewheel)    # Linux scroll up
        root.bind("<Button-5>", self.on_mousewheel)    # Linux scroll down
        root.bind("<ButtonPress-1>", self.on_canvas_left_down)
        root.bind("<B1-Motion>", self.on_canvas_left_drag)
        root.bind("<ButtonRelease-1>", self.on_canvas_left_up)
        root.bind("<ButtonPress-3>", self.on_right_press)
        root.bind("<B3-Motion>", self.on_pan_motion)
        root.bind("<ButtonRelease-3>", self.on_pan_release)
        root.bind("<ButtonPress-2>", self.on_mid_press)
        root.bind("<B2-Motion>", self.on_pan_motion)
        root.bind("<ButtonRelease-2>", self.on_pan_release)

        root.bind("s", lambda e: self.save_all())
        root.bind("S", lambda e: self.save_all())
        root.bind("l", lambda e: self.load_all())
        root.bind("L", lambda e: self.load_all())
        root.bind("n", lambda e: self.ask_new_size())
        root.bind("N", lambda e: self.ask_new_size())

    def create_ui(self):
        frm = tk.Frame(self.root)
        frm.pack(fill=tk.BOTH, expand=True)

        # Canvas for map
        self.canvas = tk.Canvas(frm, bg="#1a1a1a")
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.canvas.configure(cursor="crosshair")

        # Right-side controls
        ctrl = tk.Frame(frm, width=260, padx=6, pady=6)
        ctrl.pack(side=tk.RIGHT, fill=tk.Y)

        # Layer selector
        tk.Label(ctrl, text="Active Layer", font=("Arial", 10, "bold")).pack(anchor="w")
        layers = tk.Frame(ctrl)
        layers.pack(fill=tk.X)
        for layer in ('walls','floors','ceils'):
            b = tk.Radiobutton(layers, text=layer.title(), value=layer,
                               indicatoron=False, width=8,
                               command=lambda l=layer: self.set_layer(l))
            b.pack(side=tk.LEFT, padx=2, pady=4)
            if layer==self.active_layer: b.select()

        # Palette controls
        tk.Label(ctrl, text="Palette (ID → color)", font=("Arial", 10, "bold")).pack(anchor="w", pady=(8,0))
        self.palette_frame = tk.Frame(ctrl)
        self.palette_frame.pack(fill=tk.X, pady=(4,6))
        self.render_palette()

        btns = tk.Frame(ctrl)
        btns.pack(fill=tk.X, pady=(6,6))
        tk.Button(btns, text="Add ID", command=self.add_palette_id).pack(side=tk.LEFT, padx=4)
        tk.Button(btns, text="Edit ID", command=self.edit_selected_palette).pack(side=tk.LEFT, padx=4)
        tk.Button(btns, text="Remove ID", command=self.remove_selected_palette).pack(side=tk.LEFT, padx=4)

        # Selected info
        self.info_label = tk.Label(ctrl, text=self.status_text(), justify="left")
        self.info_label.pack(anchor="w", pady=(10,6))

        # Save / load / new
        tk.Button(ctrl, text="Save (S)", command=self.save_all).pack(fill=tk.X, pady=2)
        tk.Button(ctrl, text="Load (L)", command=self.load_all).pack(fill=tk.X, pady=2)
        tk.Button(ctrl, text="New size (N)", command=self.ask_new_size).pack(fill=tk.X, pady=2)

        # Tips
        tips = (
            "Controls:\n"
            "- Left click: paint active layer with selected ID\n"
            "- Shift + Left click: erase (set 0)\n"
            "- Mouse wheel: zoom in/out at cursor\n"
            "- Middle/right drag: pan map\n"
            "- S: save, L: load, N: new size\n"
        )
        tk.Label(ctrl, text=tips, justify="left", fg="#ddd").pack(anchor="w", pady=(8,0))

    def render_palette(self):
        for w in self.palette_frame.winfo_children():
            w.destroy()
        # palette entries are listed; clicking selects id
        for pid, col in sorted(self.palette.items()):
            b = tk.Button(self.palette_frame, text=str(pid), width=4,
                          command=lambda p=pid: self.select_palette_id(p))
            b.pack(side=tk.LEFT, padx=3, pady=2)
            # color swatch
            sw = tk.Label(self.palette_frame, bg=col, width=2)
            sw.pack(side=tk.LEFT, padx=(0,8))

        # show selected id
        self.sel_label = tk.Label(self.palette_frame, text=f"Sel: {self.selected_id}")
        self.sel_label.pack(side=tk.LEFT, padx=6)

    def select_palette_id(self, pid):
        if pid in self.palette:
            self.selected_id = pid
            self.sel_label.config(text=f"Sel: {self.selected_id}")
            self.update_info()

    def add_palette_id(self):
        num = simpledialog.askinteger("New ID", "Enter numeric ID (integer > 0):", minvalue=1)
        if num is None:
            return
        if num in self.palette:
            messagebox.showerror("Exists", f"ID {num} already exists in palette.")
            return
        chosen = colorchooser.askcolor(title=f"Choose color for ID {num}")
        if chosen and chosen[1]:
            self.palette[num] = chosen[1]
            self.selected_id = num
            self.render_palette()
            self.update_info()

    def edit_selected_palette(self):
        pid = self.selected_id
        if pid not in self.palette:
            messagebox.showinfo("No selection", "No palette ID selected.")
            return
        chosen = colorchooser.askcolor(color=self.palette[pid], title=f"Pick color for ID {pid}")
        if chosen and chosen[1]:
            self.palette[pid] = chosen[1]
            self.render_palette()
            self.update_info()

    def remove_selected_palette(self):
        pid = self.selected_id
        if pid not in self.palette:
            return
        confirm = messagebox.askyesno("Remove", f"Remove ID {pid} from palette? This won't change map values.")
        if confirm:
            del self.palette[pid]
            # pick another selected id
            self.selected_id = next(iter(self.palette), 0)
            self.render_palette()
            self.update_info()

    def set_layer(self, layer):
        self.active_layer = layer
        self.update_info()

    def world_to_cell(self, wx, wy):
        # convert canvas/world pixel coords to cell coordinates (integers)
        # world pixel refers to pixels before offset/scale transform
        # But we store offset as world pixels shift, where cell (0,0) top-left at world 0,0
        cx = (wx - self.offset_x) / self.scale
        cy = (wy - self.offset_y) / self.scale
        ix = int(math.floor(cx))
        iy = int(math.floor(cy))
        return ix, iy

    def cell_to_canvas(self, ix, iy):
        cx = ix * self.scale + self.offset_x
        cy = iy * self.scale + self.offset_y
        return cx, cy

    def redraw(self):
        c = self.canvas
        c.delete("all")
        width = c.winfo_width() or c.winfo_reqwidth()
        height = c.winfo_height() or c.winfo_reqheight()

        # Determine visible cell range
        left = int(math.floor((-self.offset_x) / self.scale)) - 1
        top = int(math.floor((-self.offset_y) / self.scale)) - 1
        right = int(math.ceil((width - self.offset_x) / self.scale)) + 1
        bottom = int(math.ceil((height - self.offset_y) / self.scale)) + 1

        left = max(left, 0)
        top = max(top, 0)
        right = min(right, self.w)
        bottom = min(bottom, self.h)

        # Draw floors (background)
        for iy in range(top, bottom):
            for ix in range(left, right):
                fid = self.floors[iy][ix]
                if fid != 0 and fid in self.palette:
                    x, y = self.cell_to_canvas(ix, iy)
                    c.create_rectangle(x, y, x + self.scale, y + self.scale, fill=self.palette[fid], outline="")

        # Draw walls on top of floors
        for iy in range(top, bottom):
            for ix in range(left, right):
                wid = self.walls[iy][ix]
                if wid != 0 and wid in self.palette:
                    x, y = self.cell_to_canvas(ix, iy)
                    # walls draw with full tile; slightly darker border to appear solid
                    c.create_rectangle(x, y, x + self.scale, y + self.scale,
                                       fill=self.palette[wid], outline="black")

        # Draw ceilings as semi overlay
        for iy in range(top, bottom):
            for ix in range(left, right):
                cid = self.ceils[iy][ix]
                if cid != 0 and cid in self.palette:
                    x, y = self.cell_to_canvas(ix, iy)
                    # overlay using stipple/alpha isn't trivial; approximate with cross lines
                    # fill with a slightly transparent effect by drawing smaller rectangle
                    pad = max(1, self.scale * 0.12)
                    c.create_rectangle(x + pad, y + pad, x + self.scale - pad, y + self.scale - pad,
                                       fill=self.palette[cid], outline="white", stipple="gray50")

        # Grid lines (only major if zoomed out)
        line_step = 1
        if self.scale < 10:
            # draw every few cells thicker to avoid too many lines
            # choose step so approx lines separated by >= 8 pixels
            step_pixels = 8
            step = max(1, int(math.ceil(step_pixels / self.scale)))
            line_step = step

        # vertical lines
        for ix in range(left, right + 1, line_step):
            x = ix * self.scale + self.offset_x
            c.create_line(x, top * self.scale + self.offset_y, x, bottom * self.scale + self.offset_y, fill="#444")
        # horizontal
        for iy in range(top, bottom + 1, line_step):
            y = iy * self.scale + self.offset_y
            c.create_line(left * self.scale + self.offset_x, y, right * self.scale + self.offset_x, y, fill="#444")

        # Draw a small marker for mouse-over cell
        # optional: draw coordinates text
        # update info label
        self.update_info()

    def status_text(self):
        return f"Layer: {self.active_layer} | Sel ID: {self.selected_id} | Map: {self.w} x {self.h} | Zoom: {self.scale:.1f}px"

    def update_info(self):
        self.info_label.config(text=self.status_text())

    def on_mousewheel(self, event):
        # get mouse position relative to canvas
        cx = self.canvas.winfo_pointerx() - self.canvas.winfo_rootx()
        cy = self.canvas.winfo_pointery() - self.canvas.winfo_rooty()
        # Determine wheel direction
        if event.num == 4 or event.delta > 0:
            factor = 1.12
        else:
            factor = 1 / 1.12
        old_scale = self.scale
        new_scale = max(self.min_scale, min(self.max_scale, old_scale * factor))
        # Zoom about mouse cursor: adjust offset so the world point under cursor remains under cursor
        wx = (cx - self.offset_x) / old_scale
        wy = (cy - self.offset_y) / old_scale
        self.scale = new_scale
        self.offset_x = cx - wx * self.scale
        self.offset_y = cy - wy * self.scale
        self.redraw()

    def on_canvas_left_down(self, event):
        # paint at clicked cell
        x = event.x
        y = event.y
        ix, iy = self.world_to_cell(x, y)
        if 0 <= ix < self.w and 0 <= iy < self.h:
            if event.state & 0x0001:  # Shift held -> erase
                val = 0
            else:
                val = self.selected_id
            self.set_cell(ix, iy, val)
            self.redraw()
        # start possible drag for drawing if user moves
        self.dragging = True
        self.last_mouse = (event.x, event.y)

    def on_canvas_left_drag(self, event):
        if not self.dragging:
            return
        x = event.x
        y = event.y
        ix, iy = self.world_to_cell(x, y)
        if 0 <= ix < self.w and 0 <= iy < self.h:
            if (event.state & 0x0001):  # Shift -> erase
                val = 0
            else:
                val = self.selected_id
            self.set_cell(ix, iy, val)
            self.redraw()
        self.last_mouse = (event.x, event.y)

    def on_canvas_left_up(self, event):
        self.dragging = False

    def on_mid_press(self, event):
        self.pan_start(event)

    def on_right_press(self, event):
        self.pan_start(event)

    def pan_start(self, event):
        self.pan_started = True
        self.last_mouse = (event.x, event.y)

    def on_pan_motion(self, event):
        if not hasattr(self, 'pan_started') or not self.pan_started:
            return
        dx = event.x - self.last_mouse[0]
        dy = event.y - self.last_mouse[1]
        self.offset_x += dx
        self.offset_y += dy
        self.last_mouse = (event.x, event.y)
        self.redraw()

    def on_pan_release(self, event):
        self.pan_started = False

    def set_cell(self, ix, iy, val):
        if self.active_layer == 'walls':
            self.walls[iy][ix] = val
        elif self.active_layer == 'floors':
            self.floors[iy][ix] = val
        else:
            self.ceils[iy][ix] = val

    def save_matrix_to_file(self, mat, filename):
        try:
            with open(filename, "w") as f:
                for row in mat:
                    f.write(" ".join(str(int(x)) for x in row) + "\n")
            return True, ""
        except Exception as e:
            return False, str(e)

    def save_all(self):
        ok, err = self.save_matrix_to_file(self.walls, "map.txt")
        if not ok:
            messagebox.showerror("Save failed", f"Failed saving map.txt: {err}")
            return
        ok, err = self.save_matrix_to_file(self.floors, "floor.txt")
        if not ok:
            messagebox.showerror("Save failed", f"Failed saving floor.txt: {err}")
            return
        ok, err = self.save_matrix_to_file(self.ceils, "ceil.txt")
        if not ok:
            messagebox.showerror("Save failed", f"Failed saving ceil.txt: {err}")
            return
        messagebox.showinfo("Saved", "Saved map.txt, floor.txt, ceil.txt to current folder.")

    def load_matrix_from_file(self, filename, expect_w=None, expect_h=None):
        if not os.path.exists(filename):
            return None, f"{filename} not found"
        mat = []
        try:
            with open(filename, "r") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    parts = line.split()
                    row = [int(p) for p in parts]
                    mat.append(row)
        except Exception as e:
            return None, str(e)
        # optional shape check
        if expect_h is not None and len(mat) != expect_h:
            return None, f"Height mismatch: file has {len(mat)} rows, expected {expect_h}"
        if expect_w is not None and any(len(r) != expect_w for r in mat):
            return None, f"Width mismatch on rows (expected {expect_w})"
        return mat, ""
    # ----------------------------------------------------------------------
    # LOADING
    # ----------------------------------------------------------------------
    def load_all(self):
        mw, err = self.load_matrix_from_file("map.txt")
        if mw is None:
            messagebox.showerror("Load failed", err)
            return
        fl, err = self.load_matrix_from_file("floor.txt")
        if fl is None:
            messagebox.showerror("Load failed", err)
            return
        cl, err = self.load_matrix_from_file("ceil.txt")
        if cl is None:
            messagebox.showerror("Load failed", err)
            return

        # dimension checks
        h = len(mw)
        w = len(mw[0])
        if len(fl) != h or len(cl) != h or len(fl[0]) != w or len(cl[0]) != w:
            messagebox.showerror("Load Error", "Matrix sizes differ among map.txt / floor.txt / ceil.txt")
            return

        self.w, self.h = w, h
        self.walls = mw
        self.floors = fl
        self.ceils = cl
        self.redraw()
        messagebox.showinfo("Loaded", "Loaded all map layers successfully.")

    # ----------------------------------------------------------------------
    # NEW MAP SIZE
    # ----------------------------------------------------------------------
    def ask_new_size(self):
        nw = simpledialog.askinteger("New Width", "Width:", minvalue=4, initialvalue=self.w)
        if nw is None: return
        nh = simpledialog.askinteger("New Height", "Height:", minvalue=4, initialvalue=self.h)
        if nh is None: return

        self.w, self.h = nw, nh
        self.walls = [[0]*nw for _ in range(nh)]
        self.floors = [[0]*nw for _ in range(nh)]
        self.ceils  = [[0]*nw for _ in range(nh)]
        self.redraw()

    # ----------------------------------------------------------------------
    # UNDO SYSTEM
    # ----------------------------------------------------------------------
    def _push_undo(self, ix, iy, old, new):
        """Store a single-cell change in undo stack."""
        if not hasattr(self, "undo_stack"):
            self.undo_stack = []

        self.undo_stack.append((self.active_layer, ix, iy, old, new))

    def undo(self):
        if not hasattr(self, "undo_stack") or len(self.undo_stack) == 0:
            return

        layer, ix, iy, old, new = self.undo_stack.pop()

        # apply old value back
        if layer == "walls":
            self.walls[iy][ix] = old
        elif layer == "floors":
            self.floors[iy][ix] = old
        else:
            self.ceils[iy][ix] = old

        self.redraw()

    # Bind Ctrl+Z
    def bind_shortcuts(self):
        self.root.bind("<Control-z>", lambda e: self.undo())

    # Modify set_cell to include undo:
    def set_cell(self, ix, iy, val):
        if self.active_layer == "walls":
            old = self.walls[iy][ix]
            if old != val:
                self._push_undo(ix, iy, old, val)
                self.walls[iy][ix] = val
        elif self.active_layer == "floors":
            old = self.floors[iy][ix]
            if old != val:
                self._push_undo(ix, iy, old, val)
                self.floors[iy][ix] = val
        else:
            old = self.ceils[iy][ix]
            if old != val:
                self._push_undo(ix, iy, old, val)
                self.ceils[iy][ix] = val

    # ----------------------------------------------------------------------
    # CLEAR ACTIVE LAYER
    # ----------------------------------------------------------------------
    def clear_active_layer(self):
        confirm = messagebox.askyesno("Clear Layer", f"Clear the entire '{self.active_layer}' layer?")
        if not confirm:
            return

        if self.active_layer == "walls":
            self.walls = [[0]*self.w for _ in range(self.h)]
        elif self.active_layer == "floors":
            self.floors = [[0]*self.w for _ in range(self.h)]
        else:
            self.ceils = [[0]*self.w for _ in range(self.h)]

        self.redraw()

    # ----------------------------------------------------------------------
    # ADD BUTTONS TO UI (CALL THIS AT END OF create_ui)
    # ----------------------------------------------------------------------
    # Add these 2 lines at bottom of create_ui():
    # tk.Button(ctrl, text="Undo (Ctrl+Z)", command=self.undo).pack(fill=tk.X, pady=2)
    # tk.Button(ctrl, text="Clear Layer", command=self.clear_active_layer).pack(fill=tk.X, pady=2)

if __name__ == "__main__":
    root = tk.Tk()
    app = MapEditor(root)
    root.geometry("1100x700")
    root.mainloop()

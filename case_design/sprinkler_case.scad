// ═══════════════════════════════════════════════════════════════════════
//  ESP32 8-Relay Sprinkler Controller Case
//  Target board: ACEIRMC B0DTK2PB26 (ESP32-WROOM-32E + 8x Songle relay)
//
//  Renders in OpenSCAD: https://openscad.org/downloads.html
//  Export: File → Export → Export as STL
//  Print: 0.2mm layer height, 3 walls, 20% infill, no supports needed
//
//  Parts: Print BOTTOM and LID separately.
//  Assembly: 4x M3x6 screws + M3 brass inserts (or just M3 self-tap)
// ═══════════════════════════════════════════════════════════════════════

// ─── RENDER CONTROL ─────────────────────────────────────────────────────
// Change this to "lid", "bottom", or "both"
RENDER = "both";

// ─── BOARD DIMENSIONS (measured / confirmed from board photo) ────────────
board_l     = 131;   // board length (long axis, terminal side to power side)
board_w     = 82;    // board width
board_thick = 1.6;   // PCB thickness

// ─── COMPONENT HEIGHTS ABOVE PCB ─────────────────────────────────────────
relay_h       = 21;   // tallest component (relay body)
esp32_h       = 4;    // ESP32 module sits lower than relays
terminal_h    = 13;   // screw terminal block height
terminal_depth = 14;  // how far terminals extend past board LEFT edge

// ─── CASE PARAMETERS ─────────────────────────────────────────────────────
wall        = 3.0;    // wall thickness
floor_t     = 2.5;    // floor/ceiling thickness
standoff_h  = 5;      // PCB standoff height (space under board for wiring)
standoff_r  = 3;      // standoff outer radius
standoff_hole = 1.7;  // M3.5 screw hole radius (drill 3.2mm or 3.5mm)
corner_r    = 4;      // exterior corner radius
lid_overlap = 3;      // how far lid lip drops into base

// ─── MOUNTING HOLES (from corner of PCB, confirmed LC-Tech board pattern) ─
mh_x = [3.5, board_l - 3.5];   // X positions of mounting holes
mh_y = [3.5, board_w - 3.5];   // Y positions of mounting holes

// ─── DERIVED DIMENSIONS ────────────────────────────────────────────────
// Interior of base must accommodate board + terminals on left side
// Terminal blocks extend LEFT (negative X in board space)
int_l = board_l + terminal_depth + wall + 2;  // extra left clearance + right wall gap
int_w = board_w + 4;                           // 2mm clearance each side
int_h = standoff_h + board_thick + relay_h + 2; // floor→top of tallest component

ext_l = int_l + wall * 2;
ext_w = int_w + wall * 2;
ext_h_base = int_h + floor_t;
lid_h  = floor_t + 6;   // lid thickness

// Board origin inside case (offset so terminals have clearance on left)
board_ox = wall + terminal_depth + 1;  // PCB left edge X inside case
board_oy = wall + 2;                    // PCB bottom edge Y

// ─── CUTOUT POSITIONS ────────────────────────────────────────────────────
// LEFT side: 8 relay terminal wire pass-throughs (two rows: COM+NO per relay)
// Each relay block is ~19mm wide, starting from board_oy

// RIGHT side: DC power input connector (green 3-pin terminal)
// Located at approximately board_l - 5mm from left edge, centered on board

// BOTTOM-RIGHT: 6-pin programming header + IO0/EN buttons

// ─── COLOURS FOR PREVIEW ─────────────────────────────────────────────────
base_colour = "#1a3a5c";
lid_colour  = "#2d6a9f";

// ════════════════════════════════════════════════════════════════════════
//  MODULES
// ════════════════════════════════════════════════════════════════════════

module rounded_box(l, w, h, r) {
    hull() {
        for (x = [r, l-r]) for (y = [r, w-r])
            translate([x, y, 0]) cylinder(r=r, h=h, $fn=32);
    }
}

module standoff(x, y) {
    translate([board_ox + x, board_oy + y, floor_t]) {
        difference() {
            cylinder(r=standoff_r, h=standoff_h, $fn=24);
            cylinder(r=standoff_hole, h=standoff_h + 1, $fn=20);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════
//  BASE (bottom half)
// ════════════════════════════════════════════════════════════════════════

module base() {
    color(base_colour)
    difference() {
        // Outer shell
        rounded_box(ext_l, ext_w, ext_h_base, corner_r);

        // Hollow interior
        translate([wall, wall, floor_t])
            cube([int_l, int_w, ext_h_base]);

        // ── LEFT SIDE: terminal wire slots ──────────────────────────────
        // 8 relays × 2 wire holes each (spaced ~16.3mm apart along Y axis)
        // Two columns: COM wires and NO wires
        for (i = [0:7]) {
            relay_y_center = board_oy + 5 + i * (board_w - 10) / 7.5;

            // Slot for both COM and NO wire (one tall slot per relay row)
            translate([-1, relay_y_center - 5, floor_t + standoff_h + 4])
                cube([wall + 2, 10, 12]);
        }

        // ── RIGHT SIDE: DC power input connector ────────────────────────
        // 3-position green screw terminal, ~28mm wide, center of right wall
        translate([ext_l - wall - 1,
                   board_oy + board_w/2 - 15,
                   floor_t + standoff_h + 3])
            cube([wall + 2, 30, 16]);

        // ── FRONT FACE (bottom): programming header slot ────────────────
        // 6-pin header + IO0/EN buttons near bottom-right corner of board
        translate([board_ox + board_l - 25 - 1,
                   -1,
                   floor_t + standoff_h + 2])
            cube([32, wall + 2, 14]);

        // ── STATUS LED window ────────────────────────────────────────────
        // GPIO23 LED, approx location on board
        translate([board_ox + board_l - 50,
                   ext_w - wall - 1,
                   floor_t + standoff_h + board_thick + 5])
            cube([8, wall + 2, 8]);

        // ── Wall mount keyhole slots (back of case) ───────────────────
        for (xp = [ext_l * 0.25, ext_l * 0.75]) {
            translate([xp, ext_w - 1, ext_h_base - 15]) {
                // Keyhole: circle at top (screw head), slot below
                rotate([-90, 0, 0]) {
                    cylinder(r=4, h=wall+2, $fn=24);
                    translate([-2, -12, 0]) cube([4, 12, wall+2]);
                    translate([0, -12, 0]) cylinder(r=2, h=wall+2, $fn=20);
                }
            }
        }

        // ── Lid alignment lip recess (top rim) ─────────────────────────
        translate([wall - 1, wall - 1, ext_h_base - lid_overlap])
            cube([int_l + 2, int_w + 2, lid_overlap + 1]);
    }

    // ── Standoffs ────────────────────────────────────────────────────────
    for (x = mh_x) for (y = mh_y)
        standoff(x, y);

    // ── Lid alignment ridge ───────────────────────────────────────────────
    translate([wall, wall, ext_h_base - lid_overlap])
        difference() {
            rounded_box(int_l, int_w, lid_overlap, corner_r - wall);
            translate([1, 1, -1])
                cube([int_l - 2, int_w - 2, lid_overlap + 2]);
        }
}

// ════════════════════════════════════════════════════════════════════════
//  LID (top half)
// ════════════════════════════════════════════════════════════════════════

module lid() {
    color(lid_colour)
    difference() {
        union() {
            // Outer lid panel
            rounded_box(ext_l, ext_w, floor_t, corner_r);

            // Inner alignment lip (drops into base)
            translate([wall + 0.3, wall + 0.3, -lid_overlap + 0.5])
                difference() {
                    rounded_box(int_l - 0.6, int_w - 0.6, lid_overlap, corner_r - wall - 0.5);
                    translate([1, 1, -1])
                        cube([int_l - 2.6, int_w - 2.6, lid_overlap + 2]);
                }
        }

        // ── Ventilation slots (relay heat dissipation) ────────────────
        // Rows of slots above relay positions
        for (i = [0:5]) {
            translate([board_ox - 5 + i * 20, ext_w * 0.2, -1])
                cube([12, ext_w * 0.6, floor_t + 2]);
        }

        // ── Label emboss area (front-center of lid) ───────────────────
        translate([ext_l/2 - 35, ext_w/2 - 8, floor_t - 0.8])
            linear_extrude(1.2)
                text("SPRINKLER", size=7, font="Liberation Sans:style=Bold",
                     halign="center", valign="center");

        // ── Corner screw holes (lid-to-base fastening) ────────────────
        for (x = [10, ext_l - 10]) for (y = [10, ext_w - 10])
            translate([x, y, -1])
                cylinder(r=1.7, h=floor_t + 2, $fn=20);
    }
}

// ════════════════════════════════════════════════════════════════════════
//  RENDER
// ════════════════════════════════════════════════════════════════════════

if (RENDER == "bottom" || RENDER == "both") {
    base();
}

if (RENDER == "lid" || RENDER == "both") {
    // Offset lid above base for "exploded" preview
    translate([0, 0, RENDER == "both" ? ext_h_base + 10 : 0])
        lid();
}

// ── GHOST BOARD (preview only — comment out before exporting STL) ─────
// Uncomment the block below to see the PCB inside the case:
/*
%translate([board_ox, board_oy, floor_t + standoff_h]) {
    color("#1a6b1a", 0.6) cube([board_l, board_w, board_thick]);
    // Relay outlines
    for (i = [0:7]) {
        color("#4488cc", 0.5)
        translate([5, 3 + i * (board_w-10)/7.5, board_thick])
            cube([25, 16, relay_h]);
    }
}
*/

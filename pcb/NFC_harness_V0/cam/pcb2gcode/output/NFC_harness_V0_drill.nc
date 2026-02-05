( pcb2gcode 2.5.0 )
( Software-independent Gcode )

( This file uses 1 drill bit sizes. )
( Bit sizes: [1mm] )

G94       (Millimeters per minute feed rate.)
G21       (Units == Millimeters.)
G91.1     (Incremental arc distance mode.)
G90       (Absolute coordinates.)
G00 S10000     (RPM spindle speed.)

G00 Z0.50000 (Retract)
T1
M5      (Spindle stop.)
G04 P1.00000
(MSG, Change tool bit to drill size 1mm)
M3      (Spindle on clockwise.)
G0 Z0.50000
G04 P1.00000

G1 F10.00000
G0 X25.47500 Y34.58500
G1 Z-1.80000
G1 Z0.50000
G0 X25.47500 Y37.12500
G1 Z-1.80000
G1 Z0.50000
G0 X48.67500 Y36.79000
G1 Z-1.80000
G1 Z0.50000
G0 X48.67500 Y34.25000
G1 Z-1.80000
G1 Z0.50000

G00 Z0.500 ( All done -- retract )

M5      (Spindle off.)
G04 P1.000000
M9      (Coolant off.)
M2      (Program end.)


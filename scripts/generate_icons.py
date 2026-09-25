"""
Generator for TSA Structural Analysis professional SVG icon library.
Creates consistent, scalable 24x24 vector SVG icons adhering to Microsoft Fluent UI & CAD standards.
"""

import os
from pathlib import Path

BASE_DIR = Path(r"E:\Book\Dev\TSA\resources\icons")

def write_svg(rel_path: str, svg_content: str):
    dest = BASE_DIR / rel_path
    dest.parent.mkdir(parents=True, exist_ok=True)
    with open(dest, "w", encoding="utf-8") as f:
        f.write(svg_content.strip() + "\n")
    print(f"Created: {rel_path}")

# ==============================================================================
# 1. FILE ICONS
# ==============================================================================

write_svg("file/file_new.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M14 2H6C4.89543 2 4 2.89543 4 4V20C4 21.1046 4.89543 22 6 22H18C19.1046 22 20 21.1046 20 20V8L14 2Z" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M14 2V8H20" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M12 11V17M9 14H15" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
</svg>""")

write_svg("file/file_open.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 4H10L12 6H20C21.1046 6 22 6.89543 22 8V18C22 19.1046 21.1046 20 20 20H4C2.89543 20 2 19.1046 2 18V6C2 4.89543 2.89543 4 4 4Z" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M2 11H22L19 20H5L2 11Z" fill="#38BDF8" fill-opacity="0.15" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("file/file_save.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M19 21H5C3.89543 21 3 20.1046 3 19V5C3 3.89543 3.89543 3 5 3H16L21 8V19C21 20.1046 20.1046 21 19 21Z" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M17 21V13H7V21" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M7 3V7H14V3" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("file/file_save_as.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M16 21H5C3.89543 21 3 20.1046 3 19V5C3 3.89543 3.89543 3 5 3H16L21 8V13" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M7 3V7H14V3" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M19.5 15.5L21.5 17.5M16 22L19.5 15.5L22 18L18.5 22H16Z" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("file/file_close.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M14 2H6C4.89543 2 4 2.89543 4 4V20C4 21.1046 4.89543 22 6 22H18C19.1046 22 20 21.1046 20 20V8L14 2Z" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M14 2V8H20" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M9.5 12.5L14.5 17.5M14.5 12.5L9.5 17.5" stroke="#F43F5E" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("file/file_import.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 14V19C4 20.1046 4.89543 21 6 21H18C19.1046 21 20 20.1046 20 19V14" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M12 3V15M12 15L7.5 10.5M12 15L16.5 10.5" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("file/file_export.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 14V19C4 20.1046 4.89543 21 6 21H18C19.1046 21 20 20.1046 20 19V14" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M12 15V3M12 3L7.5 7.5M12 3L16.5 7.5" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("file/file_print.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M6 9V3H18V9" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M6 18H4C2.89543 18 2 17.1046 2 16V11C2 9.89543 2.89543 9 4 9H20C21.1046 9 22 9.89543 22 11V16C22 17.1046 21.1046 18 20 18H18" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <rect x="6" y="14" width="12" height="7" rx="1" fill="#38BDF8" fill-opacity="0.15" stroke="#38BDF8" stroke-width="1.75"/>
</svg>""")

write_svg("file/file_exit.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M9 21H5C3.89543 21 3 20.1046 3 19V5C3 3.89543 3.89543 3 5 3H9" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M16 17L21 12L16 7M21 12H9" stroke="#F43F5E" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

# ==============================================================================
# 2. EDIT ICONS
# ==============================================================================

write_svg("edit/undo.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M3 9H14C17.3137 9 20 11.6863 20 15C20 18.3137 17.3137 21 14 21H8" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M7 5L3 9L7 13" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("edit/redo.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M21 9H10C6.68629 9 4 11.6863 4 15C4 18.3137 6.68629 21 10 21H16" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M17 5L21 9L17 13" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("edit/cut.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="6" cy="18" r="3" stroke="#94A3B8" stroke-width="1.75"/>
  <circle cx="6" cy="6" r="3" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M8.5 16.5L20 5M8.5 7.5L20 19" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("edit/copy.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="8" y="8" width="12" height="12" rx="2" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M4 16V6C4 4.89543 4.89543 4 6 4H16" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("edit/paste.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="5" y="5" width="14" height="16" rx="2" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M9 3H15V5C15 5.55228 14.5523 6 14 6H10C9.44772 6 9 5.55228 9 5V3Z" fill="#38BDF8" stroke="#38BDF8" stroke-width="1.5"/>
  <path d="M9 12H15M9 16H13" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("edit/delete.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 7H20M10 11V17M14 11V17M5 7L6 19C6 20.1046 6.89543 21 8 21H16C17.1046 21 18 20.1046 18 19L19 7M9 7V4C9 3.44772 9.44772 3 10 3H14C14.5523 3 15 3.44772 15 4V7" stroke="#F43F5E" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("edit/duplicate.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="7" y="7" width="10" height="10" rx="1.5" stroke="#94A3B8" stroke-width="1.75"/>
  <rect x="11" y="11" width="10" height="10" rx="1.5" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M4 14V4C4 3.44772 4.44772 3 5 3H15" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("edit/move.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2V22M12 2L8 6M12 2L16 6M12 22L8 18M12 22L16 18" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M2 12H22M2 12L6 8M2 12L6 16M22 12L18 8M22 12L18 16" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("edit/rotate.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="3" fill="#38BDF8"/>
  <path d="M21 12C21 7.02944 16.9706 3 12 3C8.42398 3 5.34084 5.08641 3.90088 8.11877M3 3V8.5H8.5" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M3 12C3 16.9706 7.02944 21 12 21C15.576 21 18.6592 18.9136 20.0991 15.8812M21 21V15.5H15.5" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("edit/mirror.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="12" y1="2" x2="12" y2="22" stroke="#F43F5E" stroke-width="1.75" stroke-dasharray="3 3"/>
  <path d="M10 6L3 18H10V6Z" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M14 6L21 18H14V6Z" fill="#38BDF8" fill-opacity="0.2" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

# ==============================================================================
# 3. MODELING ICONS
# ==============================================================================

write_svg("modeling/select.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 3L11 20L13.5 13.5L20 11L4 3Z" fill="#38BDF8" stroke="#0284C7" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("modeling/draw_node.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="12" y1="3" x2="12" y2="21" stroke="#94A3B8" stroke-width="1.5" stroke-dasharray="2 2"/>
  <line x1="3" y1="12" x2="21" y2="12" stroke="#94A3B8" stroke-width="1.5" stroke-dasharray="2 2"/>
  <circle cx="12" cy="12" r="4.5" fill="#38BDF8" stroke="#0284C7" stroke-width="1.75"/>
</svg>""")

write_svg("modeling/node_add.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="10" cy="14" r="4" fill="#38BDF8" stroke="#0284C7" stroke-width="1.5"/>
  <path d="M10 5V23M1 14H19" stroke="#94A3B8" stroke-width="1.25" stroke-dasharray="2 2"/>
  <circle cx="18" cy="6" r="4" fill="#10B981"/>
  <path d="M18 4V8M16 6H20" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/draw_bar.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="5" y1="19" x2="19" y2="5" stroke="#38BDF8" stroke-width="3" stroke-linecap="round"/>
  <circle cx="5" cy="19" r="3.5" fill="#0284C7" stroke="#FFFFFF" stroke-width="1.5"/>
  <circle cx="19" cy="5" r="3.5" fill="#0284C7" stroke="#FFFFFF" stroke-width="1.5"/>
</svg>""")

write_svg("modeling/draw_beam.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="12" x2="21" y2="12" stroke="#38BDF8" stroke-width="3.5" stroke-linecap="round"/>
  <circle cx="4" cy="12" r="3" fill="#0284C7" stroke="#FFFFFF" stroke-width="1.5"/>
  <circle cx="20" cy="12" r="3" fill="#0284C7" stroke="#FFFFFF" stroke-width="1.5"/>
  <path d="M2 18H6M20 18H18L20 15L22 18" stroke="#94A3B8" stroke-width="1.5"/>
</svg>""")

write_svg("modeling/beam_add.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="14" x2="17" y2="14" stroke="#38BDF8" stroke-width="3" stroke-linecap="round"/>
  <circle cx="4" cy="14" r="2.5" fill="#0284C7" stroke="#FFFFFF" stroke-width="1"/>
  <circle cx="16" cy="14" r="2.5" fill="#0284C7" stroke="#FFFFFF" stroke-width="1"/>
  <circle cx="18" cy="6" r="4.5" fill="#10B981"/>
  <path d="M18 4V8M16 6H20" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/draw_column.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="12" y1="4" x2="12" y2="18" stroke="#38BDF8" stroke-width="3.5" stroke-linecap="round"/>
  <circle cx="12" cy="4" r="3" fill="#0284C7" stroke="#FFFFFF" stroke-width="1.5"/>
  <rect x="7" y="18" width="10" height="3" rx="0.5" fill="#94A3B8" stroke="#64748B" stroke-width="1"/>
</svg>""")

write_svg("modeling/column_add.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="9" y1="6" x2="9" y2="19" stroke="#38BDF8" stroke-width="3" stroke-linecap="round"/>
  <circle cx="9" cy="6" r="2.5" fill="#0284C7" stroke="#FFFFFF" stroke-width="1"/>
  <rect x="5" y="19" width="8" height="2.5" rx="0.5" fill="#94A3B8"/>
  <circle cx="18" cy="6" r="4.5" fill="#10B981"/>
  <path d="M18 4V8M16 6H20" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/draw_slab.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 3L22 8L12 13L2 8L12 3Z" fill="#38BDF8" fill-opacity="0.25" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M2 8V12L12 17L22 12V8" stroke="#0284C7" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M7 5.5L17 10.5M17 5.5L7 10.5" stroke="#38BDF8" stroke-width="1" stroke-dasharray="2 2"/>
</svg>""")

write_svg("modeling/slab_add.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M10 5L18 9L10 13L2 9L10 5Z" fill="#38BDF8" fill-opacity="0.25" stroke="#38BDF8" stroke-width="1.5" stroke-linejoin="round"/>
  <path d="M2 9V13L10 17L18 13V9" stroke="#0284C7" stroke-width="1.5" stroke-linejoin="round"/>
  <circle cx="18" cy="6" r="4.5" fill="#10B981"/>
  <path d="M18 4V8M16 6H20" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/draw_wall.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M3 19V7L10 3V15L3 19Z" fill="#94A3B8" fill-opacity="0.3" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M10 3L21 8V20L10 15V3Z" fill="#38BDF8" fill-opacity="0.2" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <line x1="3" y1="19" x2="14" y2="24" stroke="#94A3B8" stroke-width="1.75"/>
</svg>""")

write_svg("modeling/struct_truss.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="2" y1="17" x2="22" y2="17" stroke="#38BDF8" stroke-width="2"/>
  <line x1="4" y1="7" x2="20" y2="7" stroke="#38BDF8" stroke-width="2"/>
  <path d="M2 17L4 7L8 17L12 7L16 17L20 7L22 17" stroke="#0284C7" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="2" cy="17" r="1.5" fill="#FFFFFF"/>
  <circle cx="22" cy="17" r="1.5" fill="#FFFFFF"/>
</svg>""")

write_svg("modeling/struct_foundation.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="9" y="3" width="6" height="8" fill="#38BDF8" fill-opacity="0.3" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M4 14L9 11H15L20 14V19H4V14Z" fill="#94A3B8" fill-opacity="0.3" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <line x1="2" y1="21" x2="22" y2="21" stroke="#64748B" stroke-width="1.75"/>
</svg>""")

write_svg("modeling/support_fixed.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="12" y1="3" x2="12" y2="14" stroke="#38BDF8" stroke-width="3" stroke-linecap="round"/>
  <line x1="4" y1="14" x2="20" y2="14" stroke="#94A3B8" stroke-width="2"/>
  <path d="M5 20L8 14M9 20L12 14M13 20L16 14M17 20L20 14" stroke="#94A3B8" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/support_pinned.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="6" r="3" fill="#38BDF8" stroke="#0284C7" stroke-width="1.5"/>
  <path d="M12 9L7 17H17L12 9Z" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <line x1="4" y1="17" x2="20" y2="17" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M5 21L8 17M11 21L14 17M17 21L20 17" stroke="#64748B" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/support_roller.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="5" r="2.5" fill="#38BDF8" stroke="#0284C7" stroke-width="1.5"/>
  <path d="M12 7.5L8 14H16L12 7.5Z" stroke="#94A3B8" stroke-width="1.5" stroke-linejoin="round"/>
  <circle cx="9" cy="17" r="1.5" stroke="#38BDF8" stroke-width="1.25"/>
  <circle cx="15" cy="17" r="1.5" stroke="#38BDF8" stroke-width="1.25"/>
  <line x1="5" y1="20" x2="19" y2="20" stroke="#94A3B8" stroke-width="1.75"/>
</svg>""")

write_svg("modeling/load_point.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="12" y1="2" x2="12" y2="18" stroke="#EF4444" stroke-width="2.5" stroke-linecap="round"/>
  <path d="M7 13L12 19L17 13" stroke="#EF4444" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="12" cy="21" r="2" fill="#0284C7"/>
</svg>""")

write_svg("modeling/load_dist.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="4" x2="21" y2="4" stroke="#EF4444" stroke-width="2"/>
  <path d="M5 4V16M5 16L3 12M5 16L7 12" stroke="#EF4444" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M12 4V16M12 16L10 12M12 16L14 12" stroke="#EF4444" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M19 4V16M19 16L17 12M19 16L21 12" stroke="#EF4444" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="2" y1="18" x2="22" y2="18" stroke="#38BDF8" stroke-width="2"/>
</svg>""")

write_svg("modeling/load_moment.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="3" fill="#38BDF8"/>
  <path d="M18 12C18 8.68629 15.3137 6 12 6C9.28476 6 6.98462 7.79815 6.22384 10.2785" stroke="#EF4444" stroke-width="2" stroke-linecap="round"/>
  <path d="M5 6L6.5 11L11.5 10.5" stroke="#EF4444" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("modeling/load_seismic.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M2 13L6 13L9 6L13 19L16 10L18 13L22 13" stroke="#F59E0B" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M4 18L7 18M12 21L15 21" stroke="#EF4444" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

write_svg("modeling/grid_cartesian.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L22 7.5L12 13L2 7.5L12 2Z" stroke="#38BDF8" stroke-width="1.5" stroke-linejoin="round"/>
  <path d="M7 4.75L17 10.25M17 4.75L7 10.25" stroke="#94A3B8" stroke-width="1.25"/>
  <path d="M12 13V22M2 7.5V16.5L12 22M22 7.5V16.5L12 22" stroke="#64748B" stroke-width="1.25" stroke-linejoin="round"/>
</svg>""")

write_svg("modeling/grid_cylindrical.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <ellipse cx="12" cy="12" rx="9" ry="5" stroke="#38BDF8" stroke-width="1.5"/>
  <ellipse cx="12" cy="12" rx="5" ry="2.75" stroke="#94A3B8" stroke-width="1.25"/>
  <line x1="12" y1="3" x2="12" y2="21" stroke="#38BDF8" stroke-width="1.25"/>
  <line x1="3" y1="12" x2="21" y2="12" stroke="#38BDF8" stroke-width="1.25"/>
</svg>""")

write_svg("modeling/levels.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M6 3L11 9H1L6 3Z" fill="#38BDF8" stroke="#0284C7" stroke-width="1.5"/>
  <line x1="6" y1="9" x2="6" y2="22" stroke="#38BDF8" stroke-width="1.5"/>
  <line x1="11" y1="9" x2="23" y2="9" stroke="#94A3B8" stroke-width="1.75"/>
  <line x1="6" y1="16" x2="23" y2="16" stroke="#94A3B8" stroke-width="1.5" stroke-dasharray="3 2"/>
</svg>""")

write_svg("modeling/grid_labels.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="8" cy="8" r="6" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M6.5 10.5L8 5.5L9.5 10.5M7 9.5H9" stroke="#38BDF8" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="8" y1="14" x2="8" y2="22" stroke="#94A3B8" stroke-width="1.75"/>
  <line x1="14" y1="8" x2="22" y2="8" stroke="#94A3B8" stroke-width="1.75"/>
</svg>""")

write_svg("modeling/snap.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="5" y="5" width="14" height="14" transform="rotate(45 12 12)" stroke="#F59E0B" stroke-width="2"/>
  <circle cx="12" cy="12" r="2.5" fill="#EF4444"/>
</svg>""")

write_svg("modeling/rulers.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M3 3H21V8H8V21H3V3Z" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <line x1="6" y1="8" x2="6" y2="5" stroke="#38BDF8" stroke-width="1.5"/>
  <line x1="11" y1="8" x2="11" y2="4" stroke="#38BDF8" stroke-width="1.5"/>
  <line x1="16" y1="8" x2="16" y2="5" stroke="#38BDF8" stroke-width="1.5"/>
  <line x1="8" y1="11" x2="5" y2="11" stroke="#38BDF8" stroke-width="1.5"/>
  <line x1="8" y1="16" x2="4" y2="16" stroke="#38BDF8" stroke-width="1.5"/>
</svg>""")

write_svg("modeling/measure.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="5" x2="3" y2="19" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="21" y1="5" x2="21" y2="19" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="3" y1="12" x2="21" y2="12" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M6 9L3 12L6 15M18 9L21 12L18 15" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("modeling/geom_cube.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7V17L12 22L3 17V7L12 2Z" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M12 2V12M21 7L12 12M3 7L12 12M12 12V22" stroke="#0284C7" stroke-width="1.75"/>
</svg>""")

write_svg("modeling/geom_line.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="4" y1="20" x2="20" y2="4" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round"/>
  <circle cx="4" cy="20" r="2.5" fill="#0284C7"/>
  <circle cx="20" cy="4" r="2.5" fill="#0284C7"/>
</svg>""")

write_svg("modeling/geom_polyline.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M3 18L9 8L16 14L21 6" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="3" cy="18" r="2" fill="#0284C7"/>
  <circle cx="9" cy="8" r="2" fill="#0284C7"/>
  <circle cx="16" cy="14" r="2" fill="#0284C7"/>
  <circle cx="21" cy="6" r="2" fill="#0284C7"/>
</svg>""")

write_svg("modeling/geom_circle.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="8.5" stroke="#38BDF8" stroke-width="1.75"/>
  <circle cx="12" cy="12" r="2" fill="#0284C7"/>
  <line x1="12" y1="12" x2="18" y2="6" stroke="#F59E0B" stroke-width="1.5"/>
</svg>""")

# ==============================================================================
# 4. SECTIONS & MATERIALS
# ==============================================================================

write_svg("sections/section_i.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M5 4H19V7H13.5V17H19V20H5V17H10.5V7H5V4Z" fill="#38BDF8" fill-opacity="0.25" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("sections/section_ipe.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M7 2H17V5H13V19H17V22H7V19H11V5H7V2Z" fill="#38BDF8" fill-opacity="0.3" stroke="#0284C7" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("sections/section_hea.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 4H20V7.5H13.5V16.5H20V20H4V16.5H10.5V7.5H4V4Z" fill="#38BDF8" fill-opacity="0.25" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("sections/section_heb.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 3H20V8H14V16H20V21H4V16H10V8H4V3Z" fill="#0284C7" fill-opacity="0.35" stroke="#0284C7" stroke-width="2" stroke-linejoin="round"/>
</svg>""")

write_svg("sections/section_rect.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="5" y="4" width="14" height="16" rx="1" fill="#94A3B8" fill-opacity="0.25" stroke="#38BDF8" stroke-width="1.75"/>
  <line x1="5" y1="4" x2="19" y2="20" stroke="#94A3B8" stroke-width="1" stroke-dasharray="2 2"/>
</svg>""")

write_svg("sections/section_circle.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="8" stroke="#38BDF8" stroke-width="1.75"/>
  <circle cx="12" cy="12" r="5" stroke="#94A3B8" stroke-width="1.5"/>
</svg>""")

write_svg("sections/section_custom.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M5 4H10V15H19V19H5V4Z" fill="#38BDF8" fill-opacity="0.2" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <circle cx="16" cy="7" r="2.5" stroke="#F59E0B" stroke-width="1.5"/>
</svg>""")

write_svg("sections/material_concrete.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7V17L12 22L3 17V7L12 2Z" fill="#94A3B8" fill-opacity="0.3" stroke="#64748B" stroke-width="1.75" stroke-linejoin="round"/>
  <circle cx="9" cy="10" r="1" fill="#475569"/>
  <circle cx="15" cy="12" r="1.5" fill="#475569"/>
  <circle cx="11" cy="16" r="1.2" fill="#475569"/>
</svg>""")

write_svg("sections/material_steel.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7V17L12 22L3 17V7L12 2Z" fill="#38BDF8" fill-opacity="0.25" stroke="#0284C7" stroke-width="1.75" stroke-linejoin="round"/>
  <line x1="7" y1="5" x2="17" y2="19" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
</svg>""")

# ==============================================================================
# 5. VIEW & 3D ICONS
# ==============================================================================

write_svg("view/view_3d.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7L12 12L3 7L12 2Z" fill="#38BDF8" fill-opacity="0.35" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M3 7V17L12 22V12L3 7Z" fill="#10B981" fill-opacity="0.25" stroke="#10B981" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M21 7V17L12 22V12L21 7Z" fill="#EF4444" fill-opacity="0.25" stroke="#EF4444" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("view/view_iso.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 3L21 8L12 13L3 8L12 3Z" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M3 8V17L12 22V13M21 8V17L12 22" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
</svg>""")

write_svg("view/view_top.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="4" y="4" width="16" height="16" rx="2" fill="#38BDF8" fill-opacity="0.2" stroke="#38BDF8" stroke-width="1.75"/>
  <line x1="4" y1="20" x2="20" y2="20" stroke="#EF4444" stroke-width="2"/>
  <line x1="4" y1="20" x2="4" y2="4" stroke="#10B981" stroke-width="2"/>
  <text x="17" y="18" fill="#EF4444" font-family="sans-serif" font-size="6" font-weight="bold">X</text>
  <text x="6" y="8" fill="#10B981" font-family="sans-serif" font-size="6" font-weight="bold">Y</text>
</svg>""")

write_svg("view/view_front.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="4" y="4" width="16" height="16" rx="2" fill="#10B981" fill-opacity="0.2" stroke="#10B981" stroke-width="1.75"/>
  <line x1="4" y1="20" x2="20" y2="20" stroke="#EF4444" stroke-width="2"/>
  <line x1="4" y1="20" x2="4" y2="4" stroke="#38BDF8" stroke-width="2"/>
  <text x="17" y="18" fill="#EF4444" font-family="sans-serif" font-size="6" font-weight="bold">X</text>
  <text x="6" y="8" fill="#38BDF8" font-family="sans-serif" font-size="6" font-weight="bold">Z</text>
</svg>""")

write_svg("view/view_side.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="4" y="4" width="16" height="16" rx="2" fill="#EF4444" fill-opacity="0.2" stroke="#EF4444" stroke-width="1.75"/>
  <line x1="4" y1="20" x2="20" y2="20" stroke="#10B981" stroke-width="2"/>
  <line x1="4" y1="20" x2="4" y2="4" stroke="#38BDF8" stroke-width="2"/>
  <text x="17" y="18" fill="#10B981" font-family="sans-serif" font-size="6" font-weight="bold">Y</text>
  <text x="6" y="8" fill="#38BDF8" font-family="sans-serif" font-size="6" font-weight="bold">Z</text>
</svg>""")

write_svg("view/coord_system.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="5" y1="19" x2="21" y2="19" stroke="#EF4444" stroke-width="2" stroke-linecap="round"/>
  <path d="M18 16L21 19L18 22" stroke="#EF4444" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="5" y1="19" x2="5" y2="3" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <path d="M2 6L5 3L8 6" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="5" y1="19" x2="13" y2="11" stroke="#10B981" stroke-width="2" stroke-linecap="round"/>
  <circle cx="5" cy="19" r="2" fill="#FFFFFF"/>
</svg>""")

write_svg("view/section_cut.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="4" y="5" width="16" height="16" stroke="#94A3B8" stroke-width="1.5" stroke-dasharray="2 2"/>
  <path d="M12 2L22 7L12 22L2 17L12 2Z" fill="#F59E0B" fill-opacity="0.35" stroke="#F59E0B" stroke-width="2" stroke-linejoin="round"/>
</svg>""")

write_svg("view/fit_all.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 9V4H9M15 4H20V9M20 15V20H15M9 20H4V15" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <rect x="8" y="8" width="8" height="8" rx="1" stroke="#94A3B8" stroke-width="1.5"/>
</svg>""")

write_svg("view/zoom_window.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="10" cy="10" r="7" stroke="#38BDF8" stroke-width="1.75"/>
  <line x1="15.5" y1="15.5" x2="21" y2="21" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <rect x="7" y="7" width="6" height="6" stroke="#F59E0B" stroke-width="1.5" stroke-dasharray="2 2"/>
</svg>""")

write_svg("view/zoom_in.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="10.5" cy="10.5" r="7.5" stroke="#38BDF8" stroke-width="1.75"/>
  <line x1="16" y1="16" x2="21" y2="21" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <line x1="10.5" y1="7.5" x2="10.5" y2="13.5" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round"/>
  <line x1="7.5" y1="10.5" x2="13.5" y2="10.5" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("view/zoom_out.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="10.5" cy="10.5" r="7.5" stroke="#38BDF8" stroke-width="1.75"/>
  <line x1="16" y1="16" x2="21" y2="21" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <line x1="7.5" y1="10.5" x2="13.5" y2="10.5" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round"/>
</svg>""")

write_svg("view/pan.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M8 12V6C8 5.44772 8.44772 5 9 5C9.55228 5 10 5.44772 10 6V11M10 6C10 5.44772 10.4477 5 11 5C11.5523 5 12 5.44772 12 6V11M12 6C12 5.44772 12.4477 5 13 5C13.5523 5 14 5.44772 14 6V12M14 7C14 6.44772 14.4477 6 15 6C15.5523 6 16 6.44772 16 7V14C16 17.3137 13.3137 20 10 20H9C6.79086 20 5 18.2091 5 16V13C5 12.4477 5.44772 12 6 12C6.55228 12 7 12.4477 7 13V15" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("view/orbit.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="3" fill="#38BDF8"/>
  <ellipse cx="12" cy="12" rx="9" ry="4" transform="rotate(-30 12 12)" stroke="#94A3B8" stroke-width="1.5"/>
  <path d="M20 7L22 9.5L19 10" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("view/view_wireframe.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7V17L12 22L3 17V7L12 2Z" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M12 2V12M21 7L12 12M3 7L12 12" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M3 17L12 12M21 17L12 12M12 22V12" stroke="#94A3B8" stroke-width="1.5" stroke-dasharray="2 2"/>
</svg>""")

write_svg("view/view_shaded.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7L12 12L3 7L12 2Z" fill="#38BDF8" fill-opacity="0.7" stroke="#0284C7" stroke-width="1.5" stroke-linejoin="round"/>
  <path d="M3 7V17L12 22V12L3 7Z" fill="#0284C7" fill-opacity="0.45" stroke="#0284C7" stroke-width="1.5" stroke-linejoin="round"/>
  <path d="M21 7V17L12 22V12L21 7Z" fill="#0369A1" fill-opacity="0.6" stroke="#0284C7" stroke-width="1.5" stroke-linejoin="round"/>
</svg>""")

write_svg("view/view_transparent.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L21 7V17L12 22L3 17V7L12 2Z" fill="#38BDF8" fill-opacity="0.15" stroke="#38BDF8" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M12 12L21 7M12 12L3 7M12 12V22" stroke="#38BDF8" stroke-width="1.5" stroke-dasharray="3 3"/>
</svg>""")

write_svg("view/visibility.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M2 12C2 12 5.5 5 12 5C18.5 5 22 12 22 12C22 12 18.5 19 12 19C5.5 19 2 12 2 12Z" stroke="#94A3B8" stroke-width="1.75" stroke-linejoin="round"/>
  <circle cx="12" cy="12" r="3.5" fill="#38BDF8" stroke="#0284C7" stroke-width="1.5"/>
</svg>""")

write_svg("view/fullscreen.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M3 9V3H9M15 3H21V9M21 15V21H15M9 21H3V15" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

# ==============================================================================
# 6. STRUCTURE OPS
# ==============================================================================

write_svg("structure/struct_move.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="6" cy="18" r="3" stroke="#94A3B8" stroke-width="1.5"/>
  <circle cx="18" cy="6" r="3" fill="#38BDF8" stroke="#0284C7" stroke-width="1.5"/>
  <path d="M7.5 16.5L16.5 7.5M16.5 7.5H12M16.5 7.5V12" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("structure/struct_copy.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="4" y1="18" x2="14" y2="18" stroke="#94A3B8" stroke-width="2"/>
  <line x1="8" y1="12" x2="18" y2="12" stroke="#38BDF8" stroke-width="2"/>
  <line x1="12" y1="6" x2="22" y2="6" stroke="#38BDF8" stroke-width="2"/>
</svg>""")

write_svg("structure/struct_align.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="3" x2="3" y2="21" stroke="#EF4444" stroke-width="2"/>
  <rect x="3" y="6" width="12" height="4" rx="0.5" fill="#38BDF8" fill-opacity="0.3" stroke="#38BDF8" stroke-width="1.5"/>
  <rect x="3" y="14" width="16" height="4" rx="0.5" fill="#38BDF8" fill-opacity="0.3" stroke="#38BDF8" stroke-width="1.5"/>
</svg>""")

write_svg("structure/struct_connect.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="12" x2="21" y2="12" stroke="#38BDF8" stroke-width="2.5"/>
  <line x1="12" y1="3" x2="12" y2="21" stroke="#38BDF8" stroke-width="2.5"/>
  <circle cx="12" cy="12" r="3.5" fill="#10B981" stroke="#FFFFFF" stroke-width="1.5"/>
</svg>""")

write_svg("structure/struct_split.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="12" x2="10" y2="12" stroke="#38BDF8" stroke-width="2.5"/>
  <line x1="14" y1="12" x2="21" y2="12" stroke="#38BDF8" stroke-width="2.5"/>
  <line x1="12" y1="4" x2="12" y2="20" stroke="#F43F5E" stroke-width="1.75" stroke-dasharray="3 3"/>
</svg>""")

write_svg("structure/struct_merge.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="12" x2="21" y2="12" stroke="#38BDF8" stroke-width="3" stroke-linecap="round"/>
  <path d="M8 8L12 12L8 16M16 8L12 12L16 16" stroke="#10B981" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

# ==============================================================================
# 7. ANALYSIS & RESULTS
# ==============================================================================

write_svg("analysis/mesh_generate.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="3" y="3" width="18" height="18" rx="1.5" stroke="#38BDF8" stroke-width="1.75"/>
  <line x1="3" y1="9" x2="21" y2="9" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="3" y1="15" x2="21" y2="15" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="9" y1="3" x2="9" y2="21" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="15" y1="3" x2="15" y2="21" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="3" y1="9" x2="9" y2="3" stroke="#38BDF8" stroke-width="1.25"/>
  <line x1="9" y1="21" x2="21" y2="9" stroke="#38BDF8" stroke-width="1.25"/>
</svg>""")

write_svg("analysis/analysis_run.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="4" y="4" width="16" height="16" rx="3" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M10 8L16 12L10 16V8Z" fill="#10B981" stroke="#059669" stroke-width="1.5" stroke-linejoin="round"/>
  <line x1="9" y1="2" x2="9" y2="4" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="15" y1="2" x2="15" y2="4" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="9" y1="20" x2="9" y2="22" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="15" y1="20" x2="15" y2="22" stroke="#94A3B8" stroke-width="1.5"/>
</svg>""")

write_svg("analysis/analysis_modal.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M2 12C5 4 8 4 12 12C16 20 19 20 22 12" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round"/>
  <line x1="2" y1="12" x2="22" y2="12" stroke="#94A3B8" stroke-width="1" stroke-dasharray="2 2"/>
</svg>""")

write_svg("analysis/analysis_static.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="8" x2="21" y2="8" stroke="#38BDF8" stroke-width="2"/>
  <path d="M12 8V18M12 18L9 21H15L12 18Z" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M5 8L3 14H7L5 8ZM19 8L17 14H21L19 8Z" stroke="#F59E0B" stroke-width="1.5"/>
</svg>""")

write_svg("analysis/results_disp.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="8" x2="21" y2="8" stroke="#94A3B8" stroke-width="1.5" stroke-dasharray="2 2"/>
  <path d="M3 8Q12 20 21 8" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round"/>
  <line x1="12" y1="8" x2="12" y2="14" stroke="#EF4444" stroke-width="1.5"/>
  <path d="M10 12L12 14L14 12" stroke="#EF4444" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("analysis/results_diagram.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="15" x2="21" y2="15" stroke="#94A3B8" stroke-width="2"/>
  <path d="M3 15Q12 4 21 15" fill="#EF4444" fill-opacity="0.25" stroke="#EF4444" stroke-width="2"/>
  <line x1="7" y1="15" x2="7" y2="10" stroke="#EF4444" stroke-width="1.25"/>
  <line x1="12" y1="15" x2="12" y2="7" stroke="#EF4444" stroke-width="1.25"/>
  <line x1="17" y1="15" x2="17" y2="10" stroke="#EF4444" stroke-width="1.25"/>
</svg>""")

write_svg("analysis/results_moment.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="2" y1="8" x2="22" y2="8" stroke="#94A3B8" stroke-width="2"/>
  <path d="M2 8Q12 22 22 8Z" fill="#EF4444" fill-opacity="0.3" stroke="#EF4444" stroke-width="2"/>
  <text x="10" y="6" fill="#EF4444" font-family="sans-serif" font-size="6" font-weight="bold">M</text>
</svg>""")

write_svg("analysis/results_shear.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="2" y1="12" x2="22" y2="12" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M2 12V6H12V18H22V12" stroke="#38BDF8" stroke-width="2"/>
  <text x="10" y="5" fill="#38BDF8" font-family="sans-serif" font-size="6" font-weight="bold">V</text>
</svg>""")

write_svg("analysis/results_axial.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="2" y1="12" x2="22" y2="12" stroke="#94A3B8" stroke-width="1.75"/>
  <rect x="3" y="8" width="18" height="4" fill="#10B981" fill-opacity="0.3" stroke="#10B981" stroke-width="1.5"/>
  <text x="10" y="6" fill="#10B981" font-family="sans-serif" font-size="6" font-weight="bold">N</text>
</svg>""")

write_svg("analysis/results_reactions.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="3" fill="#38BDF8"/>
  <path d="M12 21V15M12 15L10 17M12 15L14 17" stroke="#EF4444" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M3 12H9M9 12L7 10M9 12L7 14" stroke="#10B981" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("analysis/results_stress.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="4" y="4" width="16" height="16" rx="2" stroke="#94A3B8" stroke-width="1.5"/>
  <rect x="4" y="4" width="16" height="5" fill="#EF4444" fill-opacity="0.8"/>
  <rect x="4" y="9" width="16" height="6" fill="#F59E0B" fill-opacity="0.8"/>
  <rect x="4" y="15" width="16" height="5" fill="#38BDF8" fill-opacity="0.8"/>
</svg>""")

write_svg("analysis/results_force.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <line x1="3" y1="12" x2="21" y2="12" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M3 12Q12 4 21 12" stroke="#EF4444" stroke-width="2"/>
  <path d="M3 12Q12 20 21 12" stroke="#38BDF8" stroke-width="2"/>
</svg>""")

write_svg("analysis/design_check.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M12 2L20 5V11C20 16.5 16.5 20.5 12 22C7.5 20.5 4 16.5 4 11V5L12 2Z" fill="#10B981" fill-opacity="0.2" stroke="#10B981" stroke-width="1.75" stroke-linejoin="round"/>
  <path d="M8.5 11.5L11 14L16 9" stroke="#10B981" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("analysis/design_sizing.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M4 19C4 13.4772 8.47715 9 14 9H20" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M12 20C12 16.6863 14.6863 14 18 14" stroke="#38BDF8" stroke-width="2"/>
  <line x1="12" y1="12" x2="17" y2="7" stroke="#EF4444" stroke-width="2" stroke-linecap="round"/>
</svg>""")

# ==============================================================================
# 8. COMMON & INTERFACE
# ==============================================================================

write_svg("common/settings.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="3.5" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M19.4 15A1.65 1.65 0 0 0 19.73 16.82L20.08 17.17A2 2 0 1 1 17.25 20L16.9 19.65A1.65 1.65 0 0 0 15.08 19.32A1.65 1.65 0 0 0 14 20.87V21.5A2 2 0 1 1 10 21.5V20.87A1.65 1.65 0 0 0 8.92 19.32A1.65 1.65 0 0 0 7.1 19.65L6.75 20A2 2 0 1 1 3.92 17.17L4.27 16.82A1.65 1.65 0 0 0 4.6 15A1.65 1.65 0 0 0 3.05 13.92H2.5A2 2 0 1 1 2.5 9.92H3.05A1.65 1.65 0 0 0 4.6 8.84A1.65 1.65 0 0 0 4.27 7.02L3.92 6.67A2 2 0 1 1 6.75 3.84L7.1 4.19A1.65 1.65 0 0 0 8.92 4.52A1.65 1.65 0 0 0 10 2.97V2.5A2 2 0 1 1 14 2.5V2.97A1.65 1.65 0 0 0 15.08 4.52A1.65 1.65 0 0 0 16.9 4.19L17.25 3.84A2 2 0 1 1 20.08 6.67L19.73 7.02A1.65 1.65 0 0 0 19.4 8.84A1.65 1.65 0 0 0 20.95 9.92H21.5A2 2 0 1 1 21.5 13.92H20.95A1.65 1.65 0 0 0 19.4 15Z" stroke="#94A3B8" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("common/apply.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M20 6L9 17L4 12" stroke="#10B981" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("common/cancel.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M18 6L6 18M6 6L18 18" stroke="#F43F5E" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("common/theme_dark.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <path d="M21 12.79A9 9 0 1 1 11.21 3A7 7 0 0 0 21 12.79Z" fill="#F59E0B" fill-opacity="0.25" stroke="#F59E0B" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>""")

write_svg("common/theme_light.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="5" fill="#F59E0B" fill-opacity="0.3" stroke="#F59E0B" stroke-width="1.75"/>
  <line x1="12" y1="1" x2="12" y2="3" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="12" y1="21" x2="12" y2="23" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="4.22" y1="4.22" x2="5.64" y2="5.64" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="18.36" y1="18.36" x2="19.78" y2="19.78" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="1" y1="12" x2="3" y2="12" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="21" y1="12" x2="23" y2="12" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="4.22" y1="19.78" x2="5.64" y2="18.36" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
  <line x1="18.36" y1="5.64" x2="19.78" y2="4.22" stroke="#F59E0B" stroke-width="2" stroke-linecap="round"/>
</svg>""")

write_svg("common/help.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="9" stroke="#38BDF8" stroke-width="1.75"/>
  <path d="M9.09 9C9.3251 8.33167 9.78915 7.76811 10.4 7.39913C11.0108 7.03016 11.7289 6.87978 12.4372 6.97293C13.1455 7.06608 13.7997 7.39659 14.292 7.9103C14.7843 8.42401 15.084 9.08865 15.1425 9.79512C15.201 10.5016 15.0146 11.2062 14.614 11.7946C14.2134 12.383 13.6238 12.8188 12.94 13.03C12.4 13.2 12 13.7 12 14.3V15" stroke="#38BDF8" stroke-width="1.75" stroke-linecap="round"/>
  <circle cx="12" cy="18" r="1" fill="#38BDF8"/>
</svg>""")

write_svg("common/shortcuts.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="2" y="5" width="20" height="14" rx="2" stroke="#94A3B8" stroke-width="1.75"/>
  <line x1="6" y1="9" x2="7" y2="9" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <line x1="10" y1="9" x2="11" y2="9" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <line x1="14" y1="9" x2="15" y2="9" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <line x1="18" y1="9" x2="18.5" y2="9" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
  <line x1="8" y1="15" x2="16" y2="15" stroke="#38BDF8" stroke-width="2" stroke-linecap="round"/>
</svg>""")

write_svg("common/about.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <circle cx="12" cy="12" r="9" stroke="#6366F1" stroke-width="1.75"/>
  <line x1="12" y1="8" x2="12" y2="8.01" stroke="#6366F1" stroke-width="2.5" stroke-linecap="round"/>
  <line x1="12" y1="11" x2="12" y2="16" stroke="#6366F1" stroke-width="2" stroke-linecap="round"/>
</svg>""")

write_svg("common/model_tree.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="3" y="3" width="6" height="4" rx="1" stroke="#38BDF8" stroke-width="1.5"/>
  <rect x="13" y="10" width="8" height="4" rx="1" stroke="#94A3B8" stroke-width="1.5"/>
  <rect x="13" y="17" width="8" height="4" rx="1" stroke="#94A3B8" stroke-width="1.5"/>
  <path d="M6 7V12C6 12.5523 6.44772 13 7 13H13M6 12V19C6 19.5523 6.44772 20 7 20H13" stroke="#38BDF8" stroke-width="1.5"/>
</svg>""")

write_svg("common/properties.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="3" y="3" width="18" height="18" rx="2" stroke="#94A3B8" stroke-width="1.75"/>
  <line x1="3" y1="9" x2="21" y2="9" stroke="#94A3B8" stroke-width="1.5"/>
  <line x1="9" y1="9" x2="9" y2="21" stroke="#94A3B8" stroke-width="1.5"/>
  <circle cx="6" cy="6" r="1" fill="#38BDF8"/>
  <circle cx="9" cy="6" r="1" fill="#38BDF8"/>
</svg>""")

write_svg("common/console.svg", """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">
  <rect x="2" y="4" width="20" height="16" rx="2" stroke="#94A3B8" stroke-width="1.75"/>
  <path d="M6 8L10 12L6 16" stroke="#10B981" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="12" y1="16" x2="18" y2="16" stroke="#10B981" stroke-width="2" stroke-linecap="round"/>
</svg>""")

print("ALL ICONS GENERATED SUCCESSFULLY.")

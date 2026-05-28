#pragma once

constexpr int IDM_FILE_NEW        = 1001;
constexpr int IDM_FILE_OPEN       = 1002;
constexpr int IDM_FILE_SAVE       = 1003;
constexpr int IDM_FILE_SAVEAS     = 1004;
constexpr int IDM_FILE_IMPORT     = 1005;
constexpr int IDM_FILE_EXIT       = 1099;

constexpr int IDM_MACHINE_POST    = 2001;
constexpr int IDM_MACHINE_VERIFY  = 2002;
constexpr int IDM_MACHINE_BACKPLOT= 2003;
constexpr int IDM_MACHINE_SIM     = 2004;
constexpr int IDM_MACHINE_GEN_POCKET  = 2005; // Generate a 2D dynamic pocket toolpath
constexpr int IDM_MACHINE_GEN_CONTOUR = 2006; // Generate a 2D dynamic contour toolpath
constexpr int IDM_MACHINE_REGEN       = 2007; // Regenerate all toolpaths
constexpr int IDM_MACHINE_SUMMARY     = 2008; // Show machining summary
constexpr int IDM_MACHINE_CHAMFER     = 2009; // Generate a chamfer toolpath
constexpr int IDM_MACHINE_THREAD      = 2010; // Generate a thread mill toolpath
constexpr int IDM_MACHINE_PROBE_Z     = 2011; // Probe Z surface
constexpr int IDM_MACHINE_PROBE_BORE  = 2012; // Probe bore / boss center
constexpr int IDM_MACHINE_PROBE_CORNER= 2013; // Probe corner
constexpr int IDM_MACHINE_3D_WATERLINE= 2014; // Generate 3D waterline (Z-level) toolpath
constexpr int IDM_MACHINE_3D_SCALLOP  = 2015; // Generate 3D scallop toolpath
constexpr int IDM_MACHINE_3D_RASTER   = 2016; // Generate 3D raster toolpath
constexpr int IDM_MACHINE_5AXIS       = 2017; // Generate 5-axis swarf toolpath
constexpr int IDM_SETUP_CONSTRAINTS   = 2021; // Setup constraints workflow
constexpr int IDM_SETUP_POST_PROFILE  = 2022; // Setup post profile workflow
constexpr int IDM_SETUP_TOOL_DB       = 2023; // Setup SQL tool/material workflow
constexpr int IDM_SETUP_PERF_MODE     = 2024; // Setup performance mode
constexpr int IDM_SETUP_GUIDANCE      = 2025; // Show context workflow guidance
constexpr int IDM_SETUP_AUDIT_LOG     = 2026; // Show operation audit trail

// --------------------------------------------------------------------------
// Wire EDM commands (2030–2039)
// --------------------------------------------------------------------------
constexpr int IDM_EDM_CUT_2AXIS     = 2030; // Wire EDM 2-axis contour cut
constexpr int IDM_EDM_CUT_4AXIS     = 2031; // Wire EDM 4-axis taper cut
constexpr int IDM_EDM_SKIM          = 2032; // Wire EDM skim/finish passes
constexpr int IDM_EDM_NO_CORE       = 2033; // Wire EDM no-core (slug elimination)
constexpr int IDM_EDM_RECOGNISE     = 2034; // Auto-recognise EDM stock features

// --------------------------------------------------------------------------
// Router commands (2040–2049)
// --------------------------------------------------------------------------
constexpr int IDM_ROUTER_CONTOUR    = 2040; // Router 2D contour
constexpr int IDM_ROUTER_POCKET     = 2041; // Router 2D pocket
constexpr int IDM_ROUTER_NEST       = 2042; // Nested sheet routing
constexpr int IDM_ROUTER_5AXIS      = 2043; // 5-axis surface routing
constexpr int IDM_ROUTER_AGGREGATE  = 2044; // Multi-aggregate head routing

// --------------------------------------------------------------------------
// Art / Relief commands (2050–2059)
// --------------------------------------------------------------------------
constexpr int IDM_ART_IMAGE_RELIEF  = 2050; // Image-to-relief mesh
constexpr int IDM_ART_VECTOR_TEX    = 2051; // Vector-to-texture emboss
constexpr int IDM_ART_ORGANIC       = 2052; // Organic surface smoothing
constexpr int IDM_ART_TOOLPATH      = 2053; // Relief toolpath generation

// --------------------------------------------------------------------------
// §4 Enhancement commands (2060–2079)
// --------------------------------------------------------------------------
constexpr int IDM_DYN_ARC_FIT       = 2060; // Apply improved arc fitting to selected toolpath
constexpr int IDM_DYN_TROCH_PEEL    = 2061; // Enhanced trochoidal peeling roughing
constexpr int IDM_3D_AUTO_BOUNDARY  = 2062; // Auto-select 3D HST machining boundary
constexpr int IDM_3D_MIXED_CUSP     = 2063; // Mixed cusp height raster
constexpr int IDM_MA_DEBURR         = 2064; // 5-axis deburr pass
constexpr int IDM_MA_CHAMFER5       = 2065; // 5-axis chamfer pass
constexpr int IDM_TURN_SEMIFINISH   = 2066; // Semi-finish turning pass
constexpr int IDM_TURN_CUST_THREAD  = 2067; // Custom thread profile turning
constexpr int IDM_VERIFY_PROBE_SIM  = 2068; // Probe path simulation

constexpr int IDM_VIEW_WIREFRAME       = 3001;
constexpr int IDM_VIEW_SHADED          = 3002;
constexpr int IDM_VIEW_TRANSLU         = 3003;
constexpr int IDM_VIEW_ISOMETRIC       = 3004;
constexpr int IDM_VIEW_FRONT           = 3005;
constexpr int IDM_VIEW_TOP             = 3006;
constexpr int IDM_VIEW_RIGHT           = 3007;
constexpr int IDM_VIEW_BACK            = 3008;
constexpr int IDM_VIEW_BOTTOM          = 3009;
constexpr int IDM_VIEW_LEFT            = 3010;
constexpr int IDM_VIEW_FIT             = 3011;  // F3 – Zoom to fit all entities
constexpr int IDM_VIEW_ZOOM_SELECTED   = 3012;  // F2 – Zoom to selected entities
constexpr int IDM_VIEW_TOGGLE_GRID     = 3013;  // F4 – Toggle grid display
constexpr int IDM_VIEW_TOGGLE_GNOMON   = 3014;  // F5 – Toggle dynamic gnomon

// Wireframe tab commands – Points group
constexpr int IDM_WF_POINT           = 4001;  // Point Position (coordinates) [P]
constexpr int IDM_WF_POINT_DYNAMIC   = 4008;  // Point Dynamic (along curve/surface/mesh)
constexpr int IDM_WF_POINT_NODE      = 4009;  // Point Node Points (spline control nodes)
constexpr int IDM_WF_POINT_SEGMENT   = 4010;  // Point Segment (evenly spaced points)

// Wireframe tab commands – Lines group
constexpr int IDM_WF_LINE            = 4002;  // Line Endpoints [L]
constexpr int IDM_WF_LINE_CLOSEST    = 4011;  // Line Closest (shortest between two entities)
constexpr int IDM_WF_LINE_BISECT     = 4012;  // Line Bisect (bisects angle between two lines)
constexpr int IDM_WF_LINE_PERP       = 4013;  // Line Perpendicular
constexpr int IDM_WF_LINE_PARALLEL   = 4014;  // Line Parallel (offset distance)
constexpr int IDM_WF_LINE_NORMAL     = 4015;  // Line Normal (to point/grid/chain)

// Wireframe tab commands – Arcs group
constexpr int IDM_WF_ARC             = 4003;  // Arc 3 Points [A]
constexpr int IDM_WF_CIRCLE          = 4005;  // Circle Center Point [C]
constexpr int IDM_WF_CIRCLE_EDGE     = 4016;  // Circle Edge Points (2- or 3-point)
constexpr int IDM_WF_ARC_TANGENT     = 4017;  // Arc Tangent (1/2/3 entities)
constexpr int IDM_WF_ARC_ENDPOINTS   = 4018;  // Arc Endpoints (two pts + radius)
constexpr int IDM_WF_ARC_POLAR       = 4019;  // Arc Polar (centre + radius + angles)

// Wireframe tab commands – Splines group
constexpr int IDM_WF_SPLINE          = 4004;  // Spline Manual
constexpr int IDM_WF_SPLINE_AUTO     = 4020;  // Spline Automatic (fit through points)
constexpr int IDM_WF_SPLINE_BLENDED  = 4021;  // Spline Blended (connect two curves)

// Wireframe tab commands – Shapes group
constexpr int IDM_WF_RECTANGLE       = 4006;  // Rectangle
constexpr int IDM_WF_RECT_SHAPES     = 4022;  // Rectangular Shapes (rounded corners / chamfers)
constexpr int IDM_WF_POLYGON         = 4007;  // Polygon
constexpr int IDM_WF_ELLIPSE         = 4023;  // Ellipse (centre + major/minor axes)
constexpr int IDM_WF_HELIX           = 4024;  // Spiral / Helix
constexpr int IDM_WF_BBOX            = 4025;  // Bounding Box (2D/3D)

// Wireframe tab commands – Curves (extraction) group
constexpr int IDM_WF_CURVE_ONE_EDGE  = 4026;  // Curve One Edge (from solid/surface edge)
constexpr int IDM_WF_CURVE_ALL_EDGES = 4027;  // Curve All Edges
constexpr int IDM_WF_SILHOUETTE      = 4044;  // Silhouette Boundary (projected outer boundary)
constexpr int IDM_WF_CURVE_SLICE_PLN = 4028;  // Curve Slice by Plane
constexpr int IDM_WF_CURVE_SLICE_CRV = 4029;  // Curve Slice Along Curve
constexpr int IDM_WF_CURVE_FLOWLINE  = 4030;  // Curve Flowline (U/V)
constexpr int IDM_WF_CURVE_INTERSECT = 4031;  // Curve at Intersection

// Wireframe tab commands – Modify group
constexpr int IDM_WF_MOD_FILLET      = 4032;  // Fillet Entities
constexpr int IDM_WF_MOD_CHAMFER     = 4033;  // Chamfer Entities
constexpr int IDM_WF_MOD_DYN_TRIM    = 4034;  // Dynamic Trim
constexpr int IDM_WF_MOD_BREAK_TWO   = 4035;  // Break Two Pieces
constexpr int IDM_WF_MOD_BREAK_INT   = 4036;  // Break at Intersection
constexpr int IDM_WF_MOD_JOIN        = 4037;  // Join Entities
constexpr int IDM_WF_MOD_INTERSECT   = 4038;  // Modify at Intersection
constexpr int IDM_WF_MOD_PROJECT     = 4039;  // Project geometry onto a plane or surface
constexpr int IDM_WF_MOD_OFFSET      = 4040;  // Offset / Offset Chains
constexpr int IDM_WF_MOD_ROLL        = 4041;  // Roll / Unroll around a cylinder

// Wireframe construction-plane and Z-depth commands
constexpr int IDM_WF_SET_CPLANE      = 4042;  // Cycle to next Cplane [F8]
constexpr int IDM_WF_SET_ZDEPTH      = 4043;  // Prompt for Z-depth value [F9]

// Surfaces tab commands
constexpr int IDM_SURF_LOFT            = 4101;  // Ruled/Lofted surface between cross-sections
constexpr int IDM_SURF_REVOLVE         = 4102;  // Surface of revolution
constexpr int IDM_SURF_FILLET          = 4103;  // Fillet blend surface between two surfaces
constexpr int IDM_SURF_OFFSET          = 4104;  // Offset surface by distance
constexpr int IDM_SURF_TRIM            = 4105;  // Trim surface with a wireframe curve
constexpr int IDM_SURF_UNTRIM          = 4106;  // Remove trim boundaries from surface
constexpr int IDM_SURF_EXTEND          = 4107;  // Extend a surface edge
constexpr int IDM_SURF_FLAT_BOUNDARY   = 4108;  // Flat surface within a closed wireframe loop
constexpr int IDM_SURF_SWEPT           = 4109;  // Drive a cross-section profile along a path
constexpr int IDM_SURF_NET             = 4110;  // Net surface from U/V wireframe grid
constexpr int IDM_SURF_FENCE           = 4111;  // Project a surface from a curve at a vector/distance
constexpr int IDM_SURF_DRAFT_SURF      = 4112;  // Extend a surface from a curve at a draft angle
constexpr int IDM_SURF_TRIM_TO_SURF    = 4113;  // Trim one surface using another intersecting surface
constexpr int IDM_SURF_FROM_SOLID      = 4114;  // Extract individual surfaces from a solid body

// Solids tab commands – Create group (history-based, from wireframe chains)
constexpr int IDM_SOLID_EXTRUDE   = 4201;  // Extrude a 2D profile into a solid
constexpr int IDM_SOLID_REVOLVE   = 4202;  // Revolve a profile around an axis
constexpr int IDM_SOLID_SWEEP     = 4209;  // Sweep a profile along a path curve
constexpr int IDM_SOLID_LOFT      = 4210;  // Loft / blend multiple cross-sections
constexpr int IDM_SOLID_THICKEN   = 4211;  // Add thickness to a surface to make a solid

// Solids tab commands – Primitives group (direct geometric shapes, no wireframe needed)
constexpr int IDM_SOLID_BLOCK     = 4212;  // Rectangular solid from dimensions or 2 points
constexpr int IDM_SOLID_CYLINDER  = 4213;  // Cylindrical solid (radius + height)
constexpr int IDM_SOLID_SPHERE    = 4208;  // Solid sphere (centre + radius)
constexpr int IDM_SOLID_CONE      = 4214;  // Tapered conical solid
constexpr int IDM_SOLID_TORUS     = 4215;  // Donut-shaped solid

// Solids tab commands – Modify group (refine an existing solid)
constexpr int IDM_SOLID_FILLET    = 4207;  // Round off edges with constant/variable radius
constexpr int IDM_SOLID_CHAMFER   = 4216;  // Flat angled break on edges
constexpr int IDM_SOLID_SHELL     = 4206;  // Hollow out a solid to a wall thickness
constexpr int IDM_SOLID_DRAFT     = 4217;  // Taper vertical faces for moulding/casting
constexpr int IDM_SOLID_TRIM      = 4218;  // Cut solid with a plane, surface, or solid

// Solids tab commands – Boolean group (combine/subtract solid bodies)
constexpr int IDM_SOLID_UNION     = 4203;  // Add: merge two or more solids
constexpr int IDM_SOLID_SUBTRACT  = 4204;  // Remove: cut one solid from another
constexpr int IDM_SOLID_INTERSECT = 4205;  // Common: keep only the overlapping volume

// Solids tab commands – Advanced / Specialized
constexpr int IDM_SOLID_HOLE      = 4219;  // Hole wizard (simple/counterbore/countersink/threaded)
constexpr int IDM_SOLID_IMPRESS   = 4220;  // Impression: negative of a solid (mold / electrode)
constexpr int IDM_SOLID_FROM_SURF = 4221;  // Convert closed surfaces to a watertight solid

// Solids history-tree context-menu commands
constexpr int IDM_SOLID_TREE_EDIT     = 4230; // Edit the selected feature's parameters
constexpr int IDM_SOLID_TREE_SUPPRESS = 4231; // Toggle suppression of the selected feature
constexpr int IDM_SOLID_TREE_DELETE   = 4232; // Remove the selected feature from the tree

// Model Prep tab commands
constexpr int IDM_PREP_HEAL       = 4301;
constexpr int IDM_PREP_REM_FILLET = 4302;
constexpr int IDM_PREP_BOUNDS     = 4303;
constexpr int IDM_PREP_CLASSIFY   = 4304;
constexpr int IDM_PREP_DRAFT      = 4305;
constexpr int IDM_PREP_SPLIT      = 4306;

// Edit menu command IDs
constexpr int IDM_EDIT_UNDO            = 5001;  // Ctrl+Z
constexpr int IDM_EDIT_REDO            = 5002;  // Ctrl+Y
constexpr int IDM_EDIT_COPY            = 5003;  // Ctrl+C
constexpr int IDM_EDIT_PASTE           = 5004;  // Ctrl+V
constexpr int IDM_EDIT_DELETE          = 5005;  // Delete – remove selected entities
constexpr int IDM_EDIT_ANALYZE         = 5006;  // End – display Analyze dialog
constexpr int IDM_TOGGLE_SELECT_MODE   = 5007;  // Spacebar – toggle selection mode

// Geometry transform command IDs
constexpr int IDM_GEOM_MOVE            = 6001;  // M – move selected geometry
constexpr int IDM_GEOM_ROTATE          = 6002;  // R – rotate selected geometry
constexpr int IDM_GEOM_SCALE           = 6003;  // S – scale selected geometry

// Toolpath manager / display command IDs
constexpr int IDM_TOOLPATH_MGR_TOGGLE  = 6101;  // T – open/focus Toolpath Manager
constexpr int IDM_TOOLPATH_TOGGLE_DISP = 6102;  // Ctrl+Shift+T – toggle toolpath display
constexpr int IDM_TOOLPATH_COPY_PARAMS = 6103;  // Ctrl+Shift+C – copy toolpath parameters

// Unit and context-menu command IDs
constexpr int IDM_UNIT_TOGGLE         = 7001;  // Toggle metric / imperial

// Viewport right-click context-menu command IDs
constexpr int IDM_CTX_FIT             = 7100;  // Fit to Screen
constexpr int IDM_CTX_ISO             = 7101;  // Isometric View
constexpr int IDM_CTX_FRONT           = 7102;  // Front View
constexpr int IDM_CTX_TOP             = 7103;  // Top View
constexpr int IDM_CTX_RIGHT           = 7104;  // Right View
constexpr int IDM_CTX_CLEAR_COLORS    = 7105;  // Clear Colors (reset entity colours)
constexpr int IDM_CTX_CHANGE_COLOR    = 7106;  // Change Color (selected entities)

constexpr int IDM_HELP_ABOUT      = 9001;
constexpr int IDM_COPILOT_TOGGLE  = 9002;  // Toggle the AI Copilot panel
constexpr int IDM_HELP_TOPICS     = 9003;  // F1 – Open help topics

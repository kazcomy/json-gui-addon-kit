/**
 * @file demo_json.h
 * @brief Demo JSON Data for UI Testing and Development.
 *
 * This module provides demo UI data in two formats:
 * 1. Full JSON text for testing and development (when DEMO_JSON_TEXT_FOR_TEST is defined)
 * 2. Precompiled element data for production use (default)
 *
 * The demo includes multiple screens demonstrating various UI elements:
 * - Text labels and numeric displays
 * - Lists and barrel selectors
 * - Popup overlays for alerts
 * - Compact layouts leveraging supported element primitives only
 *
 * This allows developers to test the UI system without external JSON input.
 */
/* ============================================================================
 * Demo JSON Data - Full (test only) and precompiled production form
 * ============================================================================ */
#ifndef DEMO_JSON_H
#define DEMO_JSON_H

/**
 * @brief Load and parse demo JSON data (test mode).
 * @return 0 on success, negative value on error
 */
int load_and_parse_demo_json(void);

/* Minimal hello (sanity) - new short tokens */
static const char hello_json_flat[] =
  "{ \"t\": \"h\", \"n\": 2 }"
  "{ \"t\": \"s\" }"
  "{ \"t\": \"t\",   \"p\":0, \"x\":0,  \"y\":0,  \"tx\":\"CH32V003\" }";

/* Scenario: BASIC (screens first, then simple elements on screen 0) */
static const char demo_json_basic_flat[] =
  "{ \"t\": \"h\", \"n\": 4 }"
  "{ \"t\": \"s\" }" /* id0 */
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":0,  \"tx\":\"Hello\" }"
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":16, \"tx\":\"Power:24%\" }"
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":32, \"tx\":\"Demo Ready\" }";

/* Scenario: LIST (screens first, then list on screen1 with children) */
static const char demo_json_list_flat[] =
  "{ \"t\": \"h\", \"n\": 5 }"
  "{ \"t\": \"s\" }"                                                /* id0 */
  "{ \"t\": \"l\",  \"p\":0, \"x\":0, \"y\":8, \"r\":3, \"sy\":1 }" /* id1 list */
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"ItemA\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"ItemB\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"ItemC\" }";

/* Scenario: LINK (list rows own local screens defined structurally) */
static const char demo_json_link_flat[] =
  "{ \"t\": \"h\", \"n\": 12 }"
  "{ \"t\": \"s\" }"                                                  /* id0: main screen */
  "{ \"t\": \"l\",  \"p\":0, \"x\":0, \"y\":8, \"r\":3, \"sy\":1 }" /* id1: main list */
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"Config\" }"         /* id2: row label */
  "{ \"t\": \"s\",  \"p\":1 }"                                        /* id3: local screen for Config */
  "{ \"t\": \"b\",  \"p\":3, \"x\":64, \"y\":8, \"v\":0 }"          /* id4: barrel under local screen */
  "{ \"t\": \"t\",  \"p\":4, \"x\":0, \"tx\":\"Mode A\" }"
  "{ \"t\": \"t\",  \"p\":4, \"x\":0, \"tx\":\"Mode B\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"Status\" }"        /* id7: second row */
  "{ \"t\": \"s\",  \"p\":1 }"                                        /* id8: local screen for Status */
  "{ \"t\": \"t\",  \"p\":8, \"x\":0, \"y\":0, \"tx\":\"System OK\" }"
  "{ \"t\": \"t\",  \"p\":8, \"x\":0, \"y\":16, \"tx\":\"Temp 24C\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"Exit\" }";           /* id11: simple row */

/* Scenario: BARREL (screens first, then barrel and inline labels) */
static const char demo_json_barrel_flat[] =
  "{ \"t\": \"h\", \"n\": 6 }"
  "{ \"t\": \"s\" }"                                       /* id0 */
  "{ \"t\": \"b\",  \"p\":0, \"x\":0, \"y\":16, \"v\":1 }" /* id1 barrel */
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"AUTO\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"ECO\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"NORM\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":8, \"tx\":\"SPORT\" }";


/* Scenario: TRIGGER DEMO (textual prompt with dynamic counter) */
static const char demo_json_radio_flat[] =
  "{ \"t\": \"h\", \"n\": 4 }"
  "{ \"t\": \"s\" }"
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":0,  \"tx\":\"Trigger Count\" }"
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":16, \"tx\":\"Count:5/10\" }"
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":32, \"tx\":\"Press master button to change\" }";

/* Scenario: DIGIT EDITOR */
static const char demo_json_digit_flat[] =
  "{ \"t\": \"h\", \"n\": 5 }"
  "{ \"t\": \"s\" }"
  "{ \"t\": \"t\",  \"p\":0, \"x\":0,  \"y\":8,  \"tx\":\"LABEL\" }"
  "{ \"t\": \"b\",  \"p\":0, \"x\":0,  \"y\":24, \"v\":0 }"
  "{ \"t\": \"t\",  \"p\":2, \"x\":8,  \"tx\":\"OFF\" }"
  "{ \"t\": \"t\",  \"p\":2, \"x\":8,  \"tx\":\"ON\" }";

/* Scenario: OVERLAY (full-screen overlay layered over base screen) */
static const char demo_json_overlay_flat[] =
  "{ \"t\": \"h\", \"n\": 4 }"
  "{ \"t\": \"s\" }"                /* id0 base screen */
  "{ \"t\": \"s\", \"ov\":1 }"     /* id1 overlay screen (full) */
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":16, \"tx\":\"BASE\" }"
  "{ \"t\": \"t\", \"p\":1, \"x\":0,  \"y\":0,  \"tx\":\"HELLO\" }";

/* Scenario: MULTI (two base screens with structural local screens) */
static const char demo_json_multi_flat[] =
  "{ \"t\": \"h\", \"n\": 29 }"
  "{ \"t\": \"s\" }"                                 /* id0: screen 0 */
  "{ \"t\": \"s\" }"                                 /* id1: screen 1 */
  "{ \"t\": \"s\", \"ov\":1 }"                   /* id2: overlay screen (full) */
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":0,  \"tx\":\"SCREEN0\" }" /* id3 */
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":16, \"tx\":\"Load:45%\" }"/* id4 */
  "{ \"t\": \"t\", \"p\":0, \"x\":0,  \"y\":32, \"tx\":\"Settings\" }"/* id5 */
  "{ \"t\": \"t\", \"p\":1, \"x\":0,  \"y\":0,  \"tx\":\"Vehicle\" }"/* id6 */
  "{ \"t\": \"l\", \"p\":1, \"x\":0,  \"y\":8,  \"r\":4, \"sy\":1 }"/* id7 list */
  "{ \"t\": \"t\", \"p\":7, \"x\":8,  \"tx\":\"DriveMode\" }"/* id8 list row */
  "{ \"t\": \"b\", \"p\":8, \"x\":72, \"y\":8,  \"v\":1 }"/* id9 button */
  "{ \"t\": \"t\", \"p\":9, \"x\":0,  \"tx\":\"AUTO\" }"/* id10 barrel item */
  "{ \"t\": \"t\", \"p\":9, \"x\":0,  \"tx\":\"ECO\" }"/* id11 barrel item */
  "{ \"t\": \"t\", \"p\":9, \"x\":0,  \"tx\":\"SPORT\" }" /* id12 barrel item */
  "{ \"t\": \"t\", \"p\":7, \"x\":8,  \"tx\":\"Climate\" }" /* id13 list row */
  "{ \"t\": \"s\", \"p\":13 }"                      /* id14 local screen for Climate */
  "{ \"t\": \"t\", \"p\":14, \"x\":0,  \"y\":0,  \"tx\":\"Cabin 22C\" }" /* id15 */
  "{ \"t\": \"t\", \"p\":14, \"x\":0,  \"y\":16, \"tx\":\"Fan Auto\" }" /* id16 */
  "{ \"t\": \"t\", \"p\":7, \"x\":8,  \"tx\":\"Diagnostics\" }" /* id17 list row */
  "{ \"t\": \"l\", \"p\":17, \"x\":0,  \"y\":0,  \"r\":3, \"sy\":0 }"   /*id18 nested list under Diagnostics */
  "{ \"t\": \"t\", \"p\":18, \"x\":8,  \"tx\":\"Sensors\" }" /* id19 list row */
  "{ \"t\": \"b\", \"p\":19, \"x\":72, \"y\":0,  \"v\":1 }" /* id20 button */
  "{ \"t\": \"t\", \"p\":20, \"x\":0,  \"tx\":\"OK\" }"
  "{ \"t\": \"t\", \"p\":20, \"x\":0,  \"tx\":\"WARN\" }"
  "{ \"t\": \"t\", \"p\":18, \"x\":8,  \"tx\":\"Details\" }" /* id23 list row */
  "{ \"t\": \"s\", \"p\":23 }"                                      /* local screen inside nested list */
  "{ \"t\": \"t\", \"p\":24, \"x\":0,  \"y\":0,  \"tx\":\"Diag Log\" }"
  "{ \"t\": \"t\", \"p\":24, \"x\":0,  \"y\":16, \"tx\":\"Last Err: None\" }"
  "{ \"t\": \"t\", \"p\":2, \"x\":0,  \"y\":0,  \"tx\":\"trg det\" }" /* id27 */
  "{ \"t\": \"tr\", \"p\":0, \"x\":0,  \"y\":48 }"; /* id28 trigger */

/* Full default scenario to satisfy native unit tests: 4 base screens + overlay, 32 elements total. */
static const char demo_json_full_flat[] =
  "{ \"t\": \"h\", \"n\": 32 }"
  "{ \"t\": \"s\" }"                                /* id0: screen 0 */
  "{ \"t\": \"s\" }"                                /* id1: screen 1 */
  "{ \"t\": \"s\" }"                                /* id2: screen 2 */
  "{ \"t\": \"s\" }"                                /* id3: screen 3 */
  "{ \"t\": \"s\", \"ov\":1 }"                   /* id4: overlay */
  "{ \"t\": \"t\",  \"p\":0, \"x\":0,  \"y\":0,  \"tx\":\"CH32V003\" }"
  "{ \"t\": \"t\",  \"p\":0, \"x\":0,  \"y\":16, \"tx\":\"Battery:68%\" }"
  "{ \"t\": \"t\",  \"p\":0, \"x\":0,  \"y\":32, \"tx\":\"System Ready\" }"
  "{ \"t\": \"t\",  \"p\":1, \"x\":0,  \"y\":0,  \"tx\":\"Drive Modes\" }"
  "{ \"t\": \"l\",  \"p\":1, \"x\":0,  \"y\":8,  \"r\":3, \"sy\":1 }"
  "{ \"t\": \"t\",  \"p\":9, \"x\":8,  \"tx\":\"Mode\" }"
  "{ \"t\": \"s\",  \"p\":9 }"                                        /* local screen for Mode */
  "{ \"t\": \"b\",  \"p\":11, \"x\":72, \"y\":8,  \"v\":1 }"
  "{ \"t\": \"t\",  \"p\":12, \"x\":0,  \"tx\":\"AUTO\" }"
  "{ \"t\": \"t\",  \"p\":12, \"x\":0,  \"tx\":\"ECO\" }"
  "{ \"t\": \"t\",  \"p\":12, \"x\":0,  \"tx\":\"SPORT\" }"
  "{ \"t\": \"t\",  \"p\":9, \"x\":8,  \"tx\":\"Climate\" }"
  "{ \"t\": \"s\",  \"p\":9 }"                                        /* local screen for Climate */
  "{ \"t\": \"t\",  \"p\":17, \"x\":0,  \"y\":0,  \"tx\":\"Cabin 22C\" }"
  "{ \"t\": \"t\",  \"p\":17, \"x\":0,  \"y\":16, \"tx\":\"Fan Auto\" }"
  "{ \"t\": \"t\",  \"p\":9, \"x\":8,  \"tx\":\"Lighting\" }"
  "{ \"t\": \"t\",  \"p\":2, \"x\":0,  \"y\":0,  \"tx\":\"LISTVIEWER\" }"
  "{ \"t\": \"t\",  \"p\":2, \"x\":0,  \"y\":16, \"tx\":\"Sensors\" }"
  "{ \"t\": \"l\",  \"p\":2, \"x\":0,  \"y\":24, \"r\":3, \"sy\":0 }"
  "{ \"t\": \"t\",  \"p\":23, \"x\":8,  \"tx\":\"CHG 68%\" }"
  "{ \"t\": \"t\",  \"p\":23, \"x\":8,  \"tx\":\"Temp 24C\" }"
  "{ \"t\": \"t\",  \"p\":23, \"x\":8,  \"tx\":\"Pressure 101kPa\" }"
  "{ \"t\": \"t\",  \"p\":3, \"x\":0,  \"y\":0,  \"tx\":\"Summary\" }"
  "{ \"t\": \"b\",  \"p\":27, \"x\":0,  \"y\":16, \"v\":0 }"
  "{ \"t\": \"t\",  \"p\":28, \"x\":8,  \"tx\":\"OFF\" }"
  "{ \"t\": \"t\",  \"p\":28, \"x\":8,  \"tx\":\"ON\" }"
  "{ \"t\": \"t\",  \"p\":4, \"x\":0,  \"y\":0,  \"tx\":\"GOOD MORNING!\" }";

/* ---------------- Scenario selector (compile-time) ---------------- */
#if defined(DEMO_JSON_SCENARIO_BASIC)
#define demo_json_flat demo_json_basic_flat
#elif defined(DEMO_JSON_SCENARIO_LIST)
#define demo_json_flat demo_json_list_flat
#elif defined(DEMO_JSON_SCENARIO_BARREL)
#define demo_json_flat demo_json_barrel_flat
#elif defined(DEMO_JSON_SCENARIO_RADIO)
#define demo_json_flat demo_json_radio_flat
#elif defined(DEMO_JSON_SCENARIO_DIGIT)
#define demo_json_flat demo_json_digit_flat
#elif defined(DEMO_JSON_SCENARIO_OVERLAY)
#define demo_json_flat demo_json_overlay_flat
#elif defined(DEMO_JSON_SCENARIO_MULTI)
#define demo_json_flat demo_json_multi_flat
#else
#define demo_json_flat demo_json_full_flat
#endif

#endif /* DEMO_JSON_H */

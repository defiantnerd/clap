#pragma once

#include "../../plugin.h"
#include "../../color.h"
#include "../../events.h"
#include "../../id.h"
#include "../../string-sizes.h"

// clang-format off
/*
  clap.embedded-hardware

  Lets a plugin find out what hardware it has, where the physical controls are and
  what is printed on them, and talk to them: it receives buttons, knobs, encoders,
  faders, sticks, triggers, touch and motion, and drives LEDs, LED rings and strips,
  rumble motors and displays. The hardware may be what the plugin runs inside - a
  pedal, a desktop box, the channel strip of a digital mixing desk with the plugin on
  an insert, a handheld game device where the plugin is the whole app - or controllers
  plugged into the computer a host runs on, a Push, a Maschine, a gamepad. A null
  result from get_extension means there is no hardware: the plugin runs in an ordinary
  host, and a plugin built for hardware must still work there as a plain CLAP.

    auto *eh = (const clap_host_embedded_hardware_t *)host->get_extension(
        host, CLAP_EXT_EMBEDDED_HARDWARE);
    auto *reg = (const clap_host_event_registry_t *)host->get_extension(
        host, CLAP_EXT_EVENT_REGISTRY);
    uint16_t space;
    if (eh && reg && reg->query(host, CLAP_EXT_EMBEDDED_HARDWARE, &space))
      ...  // Events with header.space_id == space are ours.

  A host offering this extension must offer the event registry and answer the query.
  The query is the plugin's opt-in: see -- Input --.

  Devices: the pieces of hardware the host shows the plugin, zero or more, which can
  come and go. A pedal or a handheld shows one, itself, with DEVICE_HOST; a host on a
  computer may show several controllers, or none until one is plugged in. Which devices
  a plugin instance sees is the host's choice: a host may give a controller to the
  plugin on the selected track and move it to another instance later, and the first
  then sees it removed and the second sees it attached, through hardware_changed and
  RESET, with no handover of its own.

  Panels: each device has zero or more panels - a main panel on the front, the
  shoulder buttons on the top edge, an expander which may come and go, a game
  controller plugged into a handheld, the built-in controls of a handheld - and each
  panel carries elements: the physical things a user touches, looks at or feels. A
  device with no panels still describes itself.

  Faces: a panel lies on one face of its device's case: the front, where the screen
  and most controls are, the back, or one of the four edges, e.g. the top edge with the
  shoulder buttons of a handheld or the bottom edge with its headphone jack. A device
  with controls on several faces lists one panel per face. A panel which is not part of
  the case, such as a game controller plugged in over USB, is DETACHED. Every face is
  seen from outside the device: front, back, left and right with the device upright;
  top looked down onto with the front towards the viewer, bottom looked up at with the
  front away from the viewer, so on both x runs left to right as on the front.

  Coordinates: each panel has its own origin at its top left corner as seen from
  outside, x to the right and y downwards, in millimetres. An element is placed by its
  centre, its bounding width and height and a clockwise rotation in degrees about that
  centre. A panel is itself placed on its face of the device by x_mm/y_mm, measured from
  that face's top left corner in the same orientation, so a plugin which wants to draw
  the whole device can. A panel or element whose host cannot measure it - a generic
  USB pad, a motion sensor - says so with NO_GEOMETRY and has zeros there.
  Values follow conventions of their own, stated once here: a stick's and a motion
  sensor's y runs up, and motion's z out of the front, as on every game controller;
  everything placed on a panel, touch points included, has y running down.

  Identity: device, panel and element ids are chosen by the host from one id space, so
  no two things share an id and an element id alone says which panel and device an
  event is from; events carry no device id. Ids are never enumeration indices. For
  everything the host can recognise - a device by its model and serial, its own panels,
  an expander by its serial or slot - they are stable across boots, firmware updates
  and re-attaching, so a plugin may persist them, e.g. in a user's control assignment.
  A unit the host cannot tell from another of its kind, typically a generic USB pad,
  gets fresh ids on every attach, never reused while the host runs, so that no stale id
  ever addresses a different controller.

  Roles: an element's id says which control it is on this device; its role, if it has
  one, says what it is on any device - the south face button, the left stick, the
  status light - so a plugin can ask for "the d-pad" without knowing the device. Inputs
  take values from clap_embedded_hardware_input_role, below 1024, and outputs from
  clap_embedded_hardware_output_role, from 1024 up, so a role alone never names two
  things. NO_ROLE is UINT32_MAX, never 0.

  Composite parts: an encoder with a push switch and an LED ring around it is three
  elements, the encoder, a button and an LED ring, the latter two with parent_id set to
  the encoder, which has the PUSHABLE flag. A stick's click, the ring round a stick, a
  scribble display beside a knob and a touch screen over a display are linked the same
  way. Each part keeps its own kind and events, so a plugin which only knows buttons
  still works the push switch.

  Details: element_info holds what every element has - kind, role, place, print. What
  only some kinds have - a switch's positions, an LED's colours, a display's pixels -
  is a detail, a struct of its own read with get_element_detail() by detail type; each
  kind below says which details it has. An element has any number of details, so new
  hardware, a ring with a heart rate sensor, is a new kind with a new detail and a new
  event type, and nothing which exists changes. A plugin skips a kind, detail type or
  event type it does not know.

  Printed labels: label is what is printed on or next to the element ("Mix", "A", "△"),
  as the user reads it, in UTF-8 with symbols where Unicode has them; alt_label is a
  second print, typically a shift function. label_color, cap_color and label_position
  say how the print looks and where it sits, so a game can prompt "press A" in the
  right colour and a host can draw legends. A print never changes; what changes goes on
  a display.

  Controls the host keeps for itself (often power and volume) are not listed at all; a
  control the host lists but also acts on (menu + volume for brightness, a charging
  light) has HOST_SHARED, and a plugin should not bind anything critical to it. An
  element with the role POWER, VOLUME_UP or VOLUME_DOWN is always HOST_SHARED: the
  host lists it only so a plugin can draw it or react to it.

  Properties: what the lists do not say, the host answers by key in get_property(),
  e.g. the colour of the case, so a plugin can match its screen to it. Keys are lower
  case with underscores; the ones this header names are the
  CLAP_EMBEDDED_HARDWARE_PROPERTY_ defines, and a vendor's own keys start with
  its reverse domain name ("com.example.amp_model"). Values are UTF-8, in the format
  the key states; colours are "#rrggbb". A property is asked of a device or of one
  panel, and a panel without a value of its own answers with its device's.

  -- Input --

  Inputs arrive as events in the space the event registry returns for
  CLAP_EXT_EMBEDDED_HARDWARE: in process() like any other event, sample timed, or
  on the main thread through the plugin's process_main_thread(), or both. The plugin
  chooses per element and per event type in input_routing(), so a knob's VALUE can go
  to process() while its TOUCH goes to the main thread, and a menu button can stay away
  from the audio thread altogether. With both threads, each gets its own copy, with no
  ordering between the two.

  A plugin opts in by asking the event registry for the space. Until a plugin instance
  has asked, and its init() has returned, the host sends it no event in this space,
  not even a RESET, and does not call its input_routing(): a plain CLAP may not check
  header.space_id, and would take a button for a note. Once it has asked, a plugin
  without the plugin side of this extension gets BUTTON, SWITCH, VALUE, DELTA and
  TOUCH in process(), and the high-rate kinds (AXIS, MOTION, TOUCH_POINT) nowhere, so
  it is not flooded on a handheld.

  Checking spaces: because hardware arrives as events, alongside notes and parameters,
  it is critical that a plugin tests header.space_id and header.type carefully and
  fully on every event, in process() and in process_main_thread(). The type numbers
  of this space overlap the core ones: a plugin which reads header.type without first
  checking header.space_id == CLAP_CORE_EVENT_SPACE_ID takes a BUTTON for a NOTE_ON, a
  SWITCH for a NOTE_OFF and a stick's AXIS for a PARAM_VALUE. The opt-in protects a
  plugin only until it asks for the space; from then on, every switch over
  header.type sits inside a check of header.space_id.

  Every input event carries timestamp_ns, the time the hardware reported it on the
  clock get_time_ns() reads, the same for the copy on each thread. Live events - a
  finger moving something now - have CLAP_EVENT_IS_LIVE in header.flags; events which
  replay a state after a RESET do not.

  RESET: forget all held state of one panel (or of every panel, for CLAP_INVALID_ID) on
  the thread it arrives on: every button released, every touch gone, nothing moving.
  Replayed, non-live events for the current state of whatever is routed to that thread
  follow immediately: a BUTTON for every button held, a TOUCH for every element
  touched, the SWITCH of every switch, the VALUE of every pot, fader, pedal, touch
  strip and analog button, the AXIS of every stick away from its centre and a
  TOUCH_POINT BEGIN for every finger down. Between two RESETs, every press on a thread
  is followed by exactly one release on that thread. A host sends RESET
  - for every panel when delivery starts on a thread: on the audio thread at the first
    process() after each start_processing(), or after the plugin asked for the space if
    it was already processing; on the main thread when input_routing() has first been
    asked;
  - for a panel when it is attached, its device's included (then its state follows),
    and when it is removed (with nothing following);
  - on a thread an element or event type was routed away from, after input_routing()
    is asked again;
  - after resume();
  and may send one at any other time; a plugin handles a RESET whenever it comes.
  Events for the audio thread which arrive while the plugin is not processing are
  dropped, not queued; the RESET at the next start restores what matters.

  State: get_input_state() reads where an input is now, on the main thread, for a
  plugin which wants it before any event. It asks for one event type of one element
  and fills in the event of that type a RESET would replay, so a pressable, touchable
  knob is three questions - BUTTON on its push switch, TOUCH and VALUE on the knob -
  each answered in the event struct the plugin already reads:

    clap_embedded_hardware_event_value_t v;
    if (eh->get_input_state(host, knob, CLAP_EMBEDDED_HARDWARE_EVENT_VALUE, &v.header,
                            sizeof(v)))
      gain = v.value;

  -- Output --

  The plugin drives outputs with events in the same space: PIXELS for LEDs, rings and
  strips, RUMBLE for motors and TEXT for displays. It pushes them into process()'s
  out_events or into process_main_thread()'s out queue, whichever thread it is on.
  set_pixels() does what PIXELS does, straight from the main thread, without waiting
  for a process_main_thread() call. Every way writes the same state, and the most
  recent write wins; lights can be slow, so a host may rate limit and show only the
  latest colour of each pixel. Outputs the host also drives (HOST_SHARED) may be
  overridden by it. When the plugin is destroyed the host stops its rumble and takes
  its lights back; on suspend it turns them off, on resume it restores what the plugin
  last set. Rumble is never restored or replayed: it is an event in time, not a state,
  so a rumble cut short by suspend, focus loss or destroy stays stopped.

  The plugin says which outputs it takes, and from which threads, in output_routing(),
  as it says where inputs go in input_routing(): ROUTE_PROCESS for out_events,
  ROUTE_MAIN_THREAD for process_main_thread()'s out queue and set_pixels(). An output
  it takes is the plugin's from then: the host stops driving it, a status light or a
  legend of its own, and shows what the plugin sends, dark or still until it does. An
  output routed 0 stays the host's, which drives it as it would with no plugin. The
  host ignores an output event from a thread the plugin did not route it to, and
  set_pixels() returns false for it. An output routed away again goes back to the
  host, as at destroy. Until the plugin has asked for the event space every output is
  the host's; a plugin without the plugin side, which cannot be asked, takes every
  output from both threads. The host asks output_routing() before delivery starts, and
  ignores what the plugin sends before it has asked, which is often from inside init():
  a plugin sends its outputs' first state when delivery has started, at its first
  RESET on either thread.

  -- Focus --

  A host may put something else in front of a panel's outputs for a while - its own
  menu, a volume overlay, another plugin brought forward on a shared surface - while
  the plugin keeps running and processing. It says so with a FOCUS event, focused 0,
  and again with focused 1 when the plugin's outputs show again. FOCUS is a hint: a
  plugin without focus may stop sending PIXELS, TEXT and RUMBLE for that panel and stop
  requesting frames for its displays there, but need not. The host still takes every
  output, keeps the latest state of its lights and displays (never rumble, see
  -- Output --) and shows it when focus returns, so a plugin which skipped updates
  sends what changed then, and requests a frame for its displays.
  Focus is about outputs only. A host which also takes a panel's inputs for its menu
  sends a RESET for the panel both when it takes them, so that nothing the plugin
  holds stays pressed while the menu is open, and when it gives them back, followed
  by their current state.

  A panel has focus when delivery starts. FOCUS goes, like RESET, to process() and,
  for a plugin with the plugin side, to process_main_thread(), whatever the routing,
  and only to a plugin which has asked for the event space. A RESET of a panel without
  focus is followed by FOCUS 0 for it, so a plugin learns the focus with the state.

  -- Displays --

  Each DISPLAY element is a surface of its own; a device may have none, one main screen,
  or several, e.g. a small one beside some knob (parent_id is then that knob). There
  are two kinds:
  - TEXT: a character display of columns x rows. It shows TEXT events, drawn by the
    host in the device's own characters.
  - GRAPHIC: a pixel display of width x height. It shows TEXT events drawn by the host
    in the device's font, or what the plugin draws: the plugin claims the surface in
    display_open() and then draws it through one api. a surface has one owner; while
    the plugin owns it the host ignores TEXT for that surface and draws over it only
    for its own system overlays (volume, battery). A surface the plugin does not claim
    stays the host's, e.g. for its own legends.
  The plugin draws at the display's own size; one which wants a fixed canvas, e.g.
  320x240 for a pixel art UI, scales it up itself.

  APIs: how the plugin draws a claimed surface. An API is named by a string, as
  clap.gui names window APIs, with a version of its own, so one API can change without
  the extension's id changing: CLAP_EMBEDDED_HARDWARE_DISPLAY_API_PIXELS is
  "pixels/2". An API a vendor adds starts with its reverse domain name
  ("com.example.sdl-renderer/1"). The host offers an API through display_open(), whose
  config names it: one the host supports (display_is_api_supported) and, if the plugin
  has one, its choice (display_get_preferred_api). A plugin which cannot draw that API
  returns false from display_open(), and the host may offer another. Every host
  supports pixels/2 on every GRAPHIC display, so a plugin which draws pixels/2 draws
  anywhere.

  An API hands the plugin a surface twice: once in the config, for as long as the
  display is open (a GL context's get_proc_address, a DRM lease), and once in each
  frame display_render() gets (a pixel buffer, a framebuffer). Each is a union of
  pointers to that API's own structs, read by config.api and frame.api, with ptr for an
  API this header does not know; an API which needs nothing there has it null. Each
  API says what it puts in them, and whether the host calls display_render() at all.

  Presenting: each API says how a frame reaches the screen. With an API the host
  renders on request, as pixels/2, the host presents what display_render() returned
  RENDERED at the display's next refresh, whatever its rate (60 Hz, 120 Hz, variable),
  and present_time_ns says when that is. An API in which the plugin owns a swapchain,
  e.g. a later vulkan/1, presents frames itself with present modes of its own in its
  own surface struct, and never gets display_render().

  pixels/2: the plugin draws XRGB8888 into the host's buffer, frame.surface.pixels,
  and the host converts to what the display can show, as color_format says, so a
  1-bit OLED is drawn in two colours. config.surface is null. Drawing is pull based:
  the plugin calls display_request_frame() for a surface when it has something new to
  show, and the host calls display_render() for it at most once per refresh; apart
  from one display_render() after each display_set_visible(true) the host never
  renders without a request, so an idle plugin costs nothing. To animate, call
  display_request_frame() from display_render(). display_render() returns RENDERED,
  UNCHANGED when nothing differs from the previous frame, so the host uploads nothing,
  or FAILED, when the host keeps the previous frame. With RENDERED the plugin may list
  in damage the rectangles which changed since the previous frame, and the host
  uploads only those; none listed means the whole canvas. With buffer_age, a plugin
  which knows what changed redraws and sends only that, which matters for a large
  panel on a slow bus and costs nothing for a scribble strip.

  -- Lifecycle --

  Suspend: the host calls suspend() while the plugin is still processing, then stops
  processing (and may deactivate), hides every display it owns (display_set_visible
  false, and may display_close), stops rumble and turns the plugin's lights off. The
  device may never wake, so save anything precious. Resume: the host calls resume(),
  then restores the plugin's lights, restarts processing, which brings a RESET on the
  audio thread, sends a RESET on the main thread and shows the displays again.
  request_quit() asks the host to end the plugin, back to a launcher or off. Both are
  about the host as a whole, not one controller: a controller which is unplugged is a
  device removed.

  Threading: host functions are [main-thread] except request_process_main_thread,
  display_request_frame and get_time_ns, which are [thread-safe] and wait-free. The
  plugin's functions are all [main-thread]; display_render() and process_main_thread()
  may run concurrently with process().

  Structs: every event, and every struct but the three which carry pointers, has no
  padding and the same layout on 32 and 64-bit ABIs, with uint32_t in place of bool, so
  header.size and every offset are the same everywhere and no uninitialised byte is
  copied with an event. display_config, frame and pixels_surface are only ever passed
  by pointer, never copied, and follow the ABI's pointer size, as CLAP's own structs
  do. A padding field only aligns what follows it; every info, detail and config
  struct ends in a reserved block, so it can grow without a new id. Padding and
  reserved fields are 0.

  Not covered: a panel the plugin declares for the host to bind physical
  controls to (keys, encoders, legends);
  curves the host draws on a surface, in the data model of clap.mini-curve-display;
  layers and plugin-owned buffers for pixels; display APIs beyond pixels/2, such as
  GLES, Vulkan or an exclusive screen (e.g. KMS/DRM), which the API hook leaves room
  for; moving motorized faders; a writable data folder, battery and background states.
*/
// clang-format on

static CLAP_CONSTEXPR const char CLAP_EXT_EMBEDDED_HARDWARE[] = "clap.embedded-hardware/2";

// Add to clap_plugin_descriptor.features to mark a plugin built for hardware, so a
// launcher on a device can list those first.
#define CLAP_PLUGIN_FEATURE_EMBEDDED_HARDWARE "embedded-hardware"

#ifdef __cplusplus
extern "C" {
#endif

// Display APIs, see -- Displays --. Every host supports pixels/2 on a GRAPHIC display.
#define CLAP_EMBEDDED_HARDWARE_DISPLAY_API_PIXELS "pixels/2"

// Keys for get_property(). The colour of the case, "#rrggbb"; asked of a panel, the
// colour of that panel, e.g. an expander in a colour of its own.
#define CLAP_EMBEDDED_HARDWARE_PROPERTY_HOUSING_COLOR "housing_color"

// In a role field: the element has no role.
static const CLAP_CONSTEXPR uint32_t CLAP_EMBEDDED_HARDWARE_NO_ROLE = UINT32_MAX;

/* -- Devices -- */

enum clap_embedded_hardware_device_flags {
   // The host runs inside this device: a pedal, a desk, a handheld. At most one device
   // has it, and suspend() and request_quit() are about it.
   CLAP_EMBEDDED_HARDWARE_DEVICE_HOST = 1 << 0,

   // The device can be attached or removed while the plugin runs, e.g. a controller on
   // USB. See hardware_changed and RESET.
   CLAP_EMBEDDED_HARDWARE_DEVICE_REMOVABLE = 1 << 1,
};

// Every field but id, flags and product may be "" when the host does not know it; a
// generic USB pad may have no more than a product name.
typedef struct clap_embedded_hardware_device_info {
   // See identity at the top
   clap_id id;

   // clap_embedded_hardware_device_flags
   uint32_t flags;

   // For display, never "", e.g. "Demo Box", "RG29XX Plus", "USB Gamepad"
   char product[CLAP_NAME_SIZE];

   // For display, e.g. "Example Audio", "RetroHandheld Inc."
   char vendor[CLAP_NAME_SIZE];

   // Reverse DNS, stable for every unit of this model, e.g. "com.example.demobox"; ""
   // if the host cannot tell the model.
   char model_id[CLAP_NAME_SIZE];

   // The processor, for a plugin which tunes itself to it, e.g. "Allwinner H700". Only
   // meaningful with DEVICE_HOST.
   char chip[CLAP_NAME_SIZE];

   // This unit's serial number
   char serial[CLAP_NAME_SIZE];

   // Free form
   char hardware_revision[CLAP_NAME_SIZE];
   char firmware_version[CLAP_NAME_SIZE];

   uint32_t reserved[4];
} clap_embedded_hardware_device_info_t;

/* -- Panels -- */

enum clap_embedded_hardware_panel_flags {
   // The panel can be attached or removed while its device stays: an expander, a game
   // controller plugged into a handheld. See hardware_changed and RESET.
   CLAP_EMBEDDED_HARDWARE_PANEL_REMOVABLE = 1 << 0,

   // The host does not know the panel's size or where its controls are, e.g. a generic
   // USB pad; sizes and positions on it are 0, and roles are the way to find things.
   CLAP_EMBEDDED_HARDWARE_PANEL_NO_GEOMETRY = 1 << 1,
};

// The face of the device's case a panel lies on, see faces at the top.
enum clap_embedded_hardware_face {
   CLAP_EMBEDDED_HARDWARE_FACE_FRONT = 0,
   CLAP_EMBEDDED_HARDWARE_FACE_BACK = 1,
   CLAP_EMBEDDED_HARDWARE_FACE_TOP = 2,
   CLAP_EMBEDDED_HARDWARE_FACE_BOTTOM = 3,
   CLAP_EMBEDDED_HARDWARE_FACE_LEFT = 4,
   CLAP_EMBEDDED_HARDWARE_FACE_RIGHT = 5,

   // Not part of the case, e.g. a game controller on a cable or wireless; x_mm and
   // y_mm are 0.
   CLAP_EMBEDDED_HARDWARE_FACE_DETACHED = 6,
};

typedef struct clap_embedded_hardware_panel_info {
   clap_id id;
   clap_id device_id;

   // clap_embedded_hardware_panel_flags
   uint32_t flags;

   // clap_embedded_hardware_face
   uint32_t face;

   // For display, e.g. "Main", "Shoulder buttons", "Expander", "8BitDo SN30 Pro".
   char name[CLAP_NAME_SIZE];

   // The panel's size
   double width_mm;
   double height_mm;

   // Where the panel's top left corner sits on its face of the device, in the same
   // orientation as the panel's own coordinates.
   double x_mm;
   double y_mm;

   uint32_t reserved[4];
} clap_embedded_hardware_panel_info_t;

/* -- Elements -- */

enum clap_embedded_hardware_element_kind {
   // Inputs

   // A push button; see MOMENTARY/LATCHING, VELOCITY, PRESSURE, ANALOG. Sends BUTTON,
   // and VALUE with PRESSURE or ANALOG. Details: CONTINUOUS with PRESSURE or ANALOG.
   CLAP_EMBEDDED_HARDWARE_KIND_BUTTON = 0,

   // A switch with fixed positions, 2 for a toggle, a lid or a headphone jack. Sends
   // SWITCH. Details: SWITCH.
   CLAP_EMBEDDED_HARDWARE_KIND_SWITCH = 1,

   // An absolute rotary control, a potentiometer. Sends VALUE. Details: CONTINUOUS.
   CLAP_EMBEDDED_HARDWARE_KIND_POT = 2,

   // An absolute linear control. Sends VALUE. Details: CONTINUOUS.
   CLAP_EMBEDDED_HARDWARE_KIND_FADER = 3,

   // An endless relative rotary control. Sends DELTA; its push switch, if PUSHABLE, is a
   // BUTTON whose parent is the encoder. Details: ENCODER.
   CLAP_EMBEDDED_HARDWARE_KIND_ENCODER = 4,

   // A touch sensitive strip, one finger; sends VALUE for the position and TOUCH.
   // Details: CONTINUOUS.
   CLAP_EMBEDDED_HARDWARE_KIND_TOUCH_STRIP = 5,

   // An expression pedal, usually through a jack. Sends VALUE. Details: CONTINUOUS.
   CLAP_EMBEDDED_HARDWARE_KIND_PEDAL = 6,

   // An analog stick, springing back to its centre. Sends AXIS; its click is a BUTTON
   // whose parent is the stick. Details: AXES.
   CLAP_EMBEDDED_HARDWARE_KIND_STICK = 7,

   // A touch screen or pad. Sends TOUCH_POINT. One laid over a display has that display
   // as its parent. Details: TOUCH_SURFACE.
   CLAP_EMBEDDED_HARDWARE_KIND_TOUCH_SURFACE = 8,

   // An accelerometer or gyroscope, see its role. Sends MOTION. Details: AXES.
   CLAP_EMBEDDED_HARDWARE_KIND_MOTION = 9,

   // Outputs

   // A single LED. Takes PIXELS. Details: LIGHT.
   CLAP_EMBEDDED_HARDWARE_KIND_LED = 64,

   // LEDs on an arc. Takes PIXELS. Details: LIGHT.
   CLAP_EMBEDDED_HARDWARE_KIND_LED_RING = 65,

   // LEDs on a line through the element's centre, along its width and turned by
   // rotation_deg; pixel 0 is at the left end before rotation. Takes PIXELS. Details:
   // LIGHT.
   CLAP_EMBEDDED_HARDWARE_KIND_LED_STRIP = 66,

   // A display. Takes TEXT, or is drawn through display_open. Details: DISPLAY.
   CLAP_EMBEDDED_HARDWARE_KIND_DISPLAY = 67,

   // A vibration motor. Takes RUMBLE. Details: RUMBLE.
   CLAP_EMBEDDED_HARDWARE_KIND_RUMBLE = 68,
};

enum clap_embedded_hardware_shape {
   CLAP_EMBEDDED_HARDWARE_SHAPE_RECT = 0,

   // An ellipse inscribed in the bounding box; a circle when width equals height.
   CLAP_EMBEDDED_HARDWARE_SHAPE_ROUND = 1,
};

enum clap_embedded_hardware_element_flags {
   // BUTTON: pressed for as long as it is held. The default.
   CLAP_EMBEDDED_HARDWARE_FLAG_MOMENTARY = 1 << 0,

   // BUTTON: mechanically stays down until pressed again
   CLAP_EMBEDDED_HARDWARE_FLAG_LATCHING = 1 << 1,

   // BUTTON: velocity in BUTTON events is meaningful
   CLAP_EMBEDDED_HARDWARE_FLAG_VELOCITY = 1 << 2,

   // BUTTON: the element reports pressure while held, as VALUE events.
   CLAP_EMBEDDED_HARDWARE_FLAG_PRESSURE = 1 << 3,

   // Any input: the element senses a finger on it and sends TOUCH, e.g. a
   // capacitive knob.
   CLAP_EMBEDDED_HARDWARE_FLAG_TOUCH_SENSE = 1 << 4,

   // POT, FADER: the device can move it. There is no way to ask it to; the flag tells
   // the plugin that the control may move under its own power.
   CLAP_EMBEDDED_HARDWARE_FLAG_MOTORIZED = 1 << 5,

   // POT, ENCODER: clicks into detents, see the CONTINUOUS and ENCODER details.
   CLAP_EMBEDDED_HARDWARE_FLAG_DETENTED = 1 << 6,

   // POT, FADER: has a single notch in the centre of its travel, which the user feels,
   // near VALUE 0.5. The host reports the raw reading, as for a stick: it does not snap
   // values in the notch to 0.5, and a plugin which wants a dead zone applies its own.
   CLAP_EMBEDDED_HARDWARE_FLAG_CENTER_NOTCH = 1 << 7,

   // BUTTON: an analog trigger. Sends VALUE, 0 released to 1 fully pulled, over its
   // whole travel, and BUTTON press and release at a threshold the host chooses.
   CLAP_EMBEDDED_HARDWARE_FLAG_ANALOG = 1 << 8,

   // Any element: the host also acts on it, e.g. menu + volume for brightness, a
   // charging light. Inputs are still delivered and outputs still taken, but the host
   // may override an output, and a plugin should not bind critical actions to an input.
   CLAP_EMBEDDED_HARDWARE_FLAG_HOST_SHARED = 1 << 9,

   // Any element: its position is unknown, see PANEL_NO_GEOMETRY; x, y, width,
   // height and rotation are 0.
   CLAP_EMBEDDED_HARDWARE_FLAG_NO_GEOMETRY = 1 << 10,

   // ENCODER, POT, STICK: the control can be pushed. The push switch is a BUTTON element
   // of its own with this element as parent_id, which has the id and sends the events;
   // the flag says it is there without searching the list. A host sets the flag exactly
   // when it lists that BUTTON.
   CLAP_EMBEDDED_HARDWARE_FLAG_PUSHABLE = 1 << 11,
};

// Where a print sits, as the user faces the panel.
enum clap_embedded_hardware_label_position {
   // Unknown, or no print
   CLAP_EMBEDDED_HARDWARE_LABEL_UNKNOWN = 0,

   // On the control itself, e.g. a letter on a face button.
   CLAP_EMBEDDED_HARDWARE_LABEL_ON = 1,

   CLAP_EMBEDDED_HARDWARE_LABEL_BELOW = 2,
   CLAP_EMBEDDED_HARDWARE_LABEL_ABOVE = 3,
   CLAP_EMBEDDED_HARDWARE_LABEL_LEFT = 4,
   CLAP_EMBEDDED_HARDWARE_LABEL_RIGHT = 5,
};

// What every element has. What only some kinds have is in its details.
typedef struct clap_embedded_hardware_element_info {
   clap_id id;
   clap_id panel_id;

   // The element this one is part of, e.g. the encoder an LED ring surrounds, the
   // stick a click belongs to, the knob a scribble display labels; CLAP_INVALID_ID
   // if none.
   clap_id parent_id;

   // clap_embedded_hardware_element_kind
   uint32_t kind;

   // What the element is on any device: an input_role for inputs, an output_role for
   // outputs; CLAP_EMBEDDED_HARDWARE_NO_ROLE if none.
   uint32_t role;

   // clap_embedded_hardware_element_flags
   uint32_t flags;

   // The centre of the element on its panel, and its bounding box before rotation.
   double x_mm;
   double y_mm;
   double width_mm;
   double height_mm;

   // Clockwise, about the centre
   double rotation_deg;

   // clap_embedded_hardware_shape
   uint32_t shape;

   // clap_embedded_hardware_label_position
   uint32_t label_position;

   // What is printed on or next to the element, "" if nothing.
   char label[CLAP_NAME_SIZE];

   // A secondary print, typically a shift function, "" if none.
   char alt_label[CLAP_NAME_SIZE];

   // The colour of the print, e.g. a green "A"; alpha 0 if plain or unknown.
   clap_color_t label_color;

   // The colour of the control itself, e.g. a red record button; alpha 0 if unknown.
   clap_color_t cap_color;

   uint32_t reserved[4];
} clap_embedded_hardware_element_info_t;

/* -- Element details -- */

// What get_element_detail() reads; each kind says which it has.
enum clap_embedded_hardware_detail_type {
   // clap_embedded_hardware_detail_switch
   CLAP_EMBEDDED_HARDWARE_DETAIL_SWITCH = 0,

   // clap_embedded_hardware_detail_continuous
   CLAP_EMBEDDED_HARDWARE_DETAIL_CONTINUOUS = 1,

   // clap_embedded_hardware_detail_encoder
   CLAP_EMBEDDED_HARDWARE_DETAIL_ENCODER = 2,

   // clap_embedded_hardware_detail_axes
   CLAP_EMBEDDED_HARDWARE_DETAIL_AXES = 3,

   // clap_embedded_hardware_detail_touch_surface
   CLAP_EMBEDDED_HARDWARE_DETAIL_TOUCH_SURFACE = 4,

   // clap_embedded_hardware_detail_light
   CLAP_EMBEDDED_HARDWARE_DETAIL_LIGHT = 5,

   // clap_embedded_hardware_detail_display
   CLAP_EMBEDDED_HARDWARE_DETAIL_DISPLAY = 6,

   // clap_embedded_hardware_detail_rumble
   CLAP_EMBEDDED_HARDWARE_DETAIL_RUMBLE = 7,
};

// Starts every detail; cast to the struct type names.
typedef struct clap_embedded_hardware_detail_header {
   // The bytes the host filled, header included
   uint32_t size;

   // clap_embedded_hardware_detail_type
   uint32_t type;
} clap_embedded_hardware_detail_header_t;

typedef struct clap_embedded_hardware_detail_switch {
   clap_embedded_hardware_detail_header_t header;

   // At least 2; SWITCH events carry 0..position_count-1
   uint32_t position_count;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_switch_t;

// POT, FADER, PEDAL, TOUCH_STRIP, and BUTTON with PRESSURE or ANALOG
typedef struct clap_embedded_hardware_detail_continuous {
   clap_embedded_hardware_detail_header_t header;

   // Distinct values the device reads over the travel, 0 if unknown
   uint32_t resolution;

   // POT with DETENTED: the detents over the travel. 0 otherwise.
   uint32_t detent_count;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_continuous_t;

typedef struct clap_embedded_hardware_detail_encoder {
   clap_embedded_hardware_detail_header_t header;

   // DELTA steps per revolution, the detents with DETENTED; 0 if unknown.
   uint32_t steps_per_revolution;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_encoder_t;

// STICK, MOTION
typedef struct clap_embedded_hardware_detail_axes {
   clap_embedded_hardware_detail_header_t header;

   // STICK: 2, x and y. MOTION: 3.
   uint32_t axis_count;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_axes_t;

typedef struct clap_embedded_hardware_detail_touch_surface {
   clap_embedded_hardware_detail_header_t header;

   // The most fingers it tracks at once
   uint32_t max_touch_points;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_touch_surface_t;

enum clap_embedded_hardware_color_model {
   // A single colour, native_color, at brightness_steps levels
   CLAP_EMBEDDED_HARDWARE_COLOR_MONO = 0,

   // Two dies, typically red and green, which together make an amber.
   CLAP_EMBEDDED_HARDWARE_COLOR_BICOLOR = 1,

   // Full colour
   CLAP_EMBEDDED_HARDWARE_COLOR_RGB = 2,
};

// LED, LED_RING, LED_STRIP
typedef struct clap_embedded_hardware_detail_light {
   clap_embedded_hardware_detail_header_t header;

   // clap_embedded_hardware_color_model
   uint32_t color_model;

   // The colour of a MONO LED when fully lit; alpha is unused.
   clap_color_t native_color;

   // Distinct brightness levels per colour channel, 2 for plain on/off.
   uint32_t brightness_steps;

   // LED: 1. LED_RING, LED_STRIP: the number of LEDs.
   uint32_t pixel_count;

   // LED_RING: where pixel 0 sits, in degrees clockwise from 12 o'clock, and how far
   // the ring runs, clockwise, to the centre of the last pixel. A full ring of n
   // pixels starting at the top has 0 and 360 * (n - 1) / n. 0 otherwise.
   double arc_start_deg;
   double arc_sweep_deg;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_light_t;

enum clap_embedded_hardware_display_kind {
   // A character display: width columns by height rows. Takes TEXT.
   CLAP_EMBEDDED_HARDWARE_DISPLAY_TEXT = 1,

   // A pixel display: width x height pixels. Takes TEXT, or the plugin's own pixels
   // through display_open.
   CLAP_EMBEDDED_HARDWARE_DISPLAY_GRAPHIC = 2,
};

// The colours a display can show natively. The plugin draws pixels/2 in XRGB8888
// whatever this says, and the host converts.
enum clap_embedded_hardware_display_color_format {
   // TEXT displays, and unknown
   CLAP_EMBEDDED_HARDWARE_DISPLAY_COLOR_FORMAT_NONE = 0,

   // On or off, in native_color, e.g. a monochrome OLED.
   CLAP_EMBEDDED_HARDWARE_DISPLAY_COLOR_FORMAT_MONO1 = 1,

   // 256 levels of native_color
   CLAP_EMBEDDED_HARDWARE_DISPLAY_COLOR_FORMAT_GRAY8 = 2,

   // 16 bit colour, 5 bits red, 6 green, 5 blue.
   CLAP_EMBEDDED_HARDWARE_DISPLAY_COLOR_FORMAT_RGB565 = 3,

   // 32 bit native endian 0xXXRRGGBB, as DRM_FORMAT_XRGB8888; X is ignored.
   CLAP_EMBEDDED_HARDWARE_DISPLAY_COLOR_FORMAT_XRGB8888 = 4,
};

typedef struct clap_embedded_hardware_detail_display {
   clap_embedded_hardware_detail_header_t header;

   // clap_embedded_hardware_display_kind
   uint32_t display_kind;

   // TEXT: columns and rows. GRAPHIC: pixels, as the user sees the display; the host
   // hides any rotation the panel is mounted with.
   uint32_t width;
   uint32_t height;

   // clap_embedded_hardware_display_color_format
   uint32_t color_format;

   // MONO1, GRAY8: the colour when fully lit; alpha is unused.
   clap_color_t native_color;

   uint32_t reserved[5];
} clap_embedded_hardware_detail_display_t;

typedef struct clap_embedded_hardware_detail_rumble {
   clap_embedded_hardware_detail_header_t header;

   // Distinct intensities, 2 for plain on/off.
   uint32_t intensity_steps;

   uint32_t reserved[4];
} clap_embedded_hardware_detail_rumble_t;

/* -- Roles -- */

// What an input is on any device. Face buttons are named by position, not by printed letter,
// because the letters move between layouts: most handhelds use the SNES layout (A east, B south)
// while Xbox style devices put A south. The print is in label.
enum clap_embedded_hardware_input_role {
   // D-pad
   CLAP_EMBEDDED_HARDWARE_INPUT_DPAD_UP = 0,
   CLAP_EMBEDDED_HARDWARE_INPUT_DPAD_DOWN = 1,
   CLAP_EMBEDDED_HARDWARE_INPUT_DPAD_LEFT = 2,
   CLAP_EMBEDDED_HARDWARE_INPUT_DPAD_RIGHT = 3,

   // Face buttons, by position in the diamond
   CLAP_EMBEDDED_HARDWARE_INPUT_FACE_SOUTH = 16,
   CLAP_EMBEDDED_HARDWARE_INPUT_FACE_EAST = 17,
   CLAP_EMBEDDED_HARDWARE_INPUT_FACE_WEST = 18,
   CLAP_EMBEDDED_HARDWARE_INPUT_FACE_NORTH = 19,
   // Six-button layouts (Saturn / Mega Drive style C and Z).
   CLAP_EMBEDDED_HARDWARE_INPUT_FACE_EXTRA_1 = 20,
   CLAP_EMBEDDED_HARDWARE_INPUT_FACE_EXTRA_2 = 21,

   // Shoulders and triggers; L2 and R2 may be ANALOG.
   CLAP_EMBEDDED_HARDWARE_INPUT_L1 = 32,
   CLAP_EMBEDDED_HARDWARE_INPUT_R1 = 33,
   CLAP_EMBEDDED_HARDWARE_INPUT_L2 = 34,
   CLAP_EMBEDDED_HARDWARE_INPUT_R2 = 35,
   // Rear grip buttons or paddles
   CLAP_EMBEDDED_HARDWARE_INPUT_BACK_L1 = 36,
   CLAP_EMBEDDED_HARDWARE_INPUT_BACK_R1 = 37,
   CLAP_EMBEDDED_HARDWARE_INPUT_BACK_L2 = 38,
   CLAP_EMBEDDED_HARDWARE_INPUT_BACK_R2 = 39,

   // STICK elements; their clicks (L3 / R3) are BUTTONs with the stick as parent.
   CLAP_EMBEDDED_HARDWARE_INPUT_STICK_L = 48,
   CLAP_EMBEDDED_HARDWARE_INPUT_STICK_R = 49,
   CLAP_EMBEDDED_HARDWARE_INPUT_STICK_L_CLICK = 50,
   CLAP_EMBEDDED_HARDWARE_INPUT_STICK_R_CLICK = 51,

   // System buttons
   CLAP_EMBEDDED_HARDWARE_INPUT_START = 64,
   CLAP_EMBEDDED_HARDWARE_INPUT_SELECT = 65,
   CLAP_EMBEDDED_HARDWARE_INPUT_MENU = 66,     // Menu, home, guide, "M"
   CLAP_EMBEDDED_HARDWARE_INPUT_FUNCTION = 67, // A second system button, "F", "FN"
   // POWER, VOLUME_UP and VOLUME_DOWN are always HOST_SHARED, see the top.
   CLAP_EMBEDDED_HARDWARE_INPUT_POWER = 68,
   // A reset button the plugin can read; a hard reset which cuts the power never
   // reaches the plugin and is not listed.
   CLAP_EMBEDDED_HARDWARE_INPUT_RESET = 69,
   CLAP_EMBEDDED_HARDWARE_INPUT_VOLUME_UP = 70,
   CLAP_EMBEDDED_HARDWARE_INPUT_VOLUME_DOWN = 71,

   // SWITCHes with 2 positions, sensing rather than played: position 1 is engaged.
   CLAP_EMBEDDED_HARDWARE_INPUT_LID = 80,             // 1 = lid closed
   CLAP_EMBEDDED_HARDWARE_INPUT_HEADPHONES = 81,      // 1 = plug inserted
   CLAP_EMBEDDED_HARDWARE_INPUT_FUNCTION_SWITCH = 82, // Two position slider, 1 = on

   // Surfaces and sensors
   CLAP_EMBEDDED_HARDWARE_INPUT_TOUCHSCREEN = 96,
   CLAP_EMBEDDED_HARDWARE_INPUT_ACCELEROMETER = 97,
   CLAP_EMBEDDED_HARDWARE_INPUT_GYROSCOPE = 98,

   // An element without a role is found by its label, kind and position.
};

// What an output is on any device
enum clap_embedded_hardware_output_role {
   // The only motor, or the heavy low frequency one of a pair.
   CLAP_EMBEDDED_HARDWARE_OUTPUT_RUMBLE = 1024,
   // The light high frequency motor of a pair.
   CLAP_EMBEDDED_HARDWARE_OUTPUT_RUMBLE_HIGH = 1025,

   // Power / charge / notification light
   CLAP_EMBEDDED_HARDWARE_OUTPUT_LED_STATUS = 1040,
   // Rings around the sticks, LED_RINGs with the stick as parent.
   CLAP_EMBEDDED_HARDWARE_OUTPUT_LED_STICK_L = 1041,
   CLAP_EMBEDDED_HARDWARE_OUTPUT_LED_STICK_R = 1042,
   // Light bar on a game controller
   CLAP_EMBEDDED_HARDWARE_OUTPUT_LED_LIGHTBAR = 1043,
   // A lit logo or emblem on the case, often also a power or charge light, then
   // HOST_SHARED; label holds the logo's text, if it has any.
   CLAP_EMBEDDED_HARDWARE_OUTPUT_LED_LOGO = 1044,

   // The device's main screen, a DISPLAY
   CLAP_EMBEDDED_HARDWARE_OUTPUT_SCREEN = 1072,
};

/* -- Events -- */

// These types overlap the core event types (BUTTON is NOTE_ON's 0), so test
// header.space_id before header.type on every event; see checking spaces at the top.
//
// Common to every event in this space:
// - header.space_id is the id the event registry gives for CLAP_EXT_EMBEDDED_HARDWARE
// - header.time is the sample offset in process(), and 0 on the main thread
// - input events have CLAP_EVENT_IS_LIVE in header.flags when a finger made them now,
//   and not when they replay state after a RESET or answer get_input_state
// - input events carry timestamp_ns, see get_time_ns.
enum clap_embedded_hardware_event_type {
   // Inputs

   // clap_embedded_hardware_event_button
   CLAP_EMBEDDED_HARDWARE_EVENT_BUTTON = 0,

   // clap_embedded_hardware_event_switch
   CLAP_EMBEDDED_HARDWARE_EVENT_SWITCH = 1,

   // clap_embedded_hardware_event_value
   CLAP_EMBEDDED_HARDWARE_EVENT_VALUE = 2,

   // clap_embedded_hardware_event_delta; no state, get_input_state returns false
   CLAP_EMBEDDED_HARDWARE_EVENT_DELTA = 3,

   // clap_embedded_hardware_event_touch
   CLAP_EMBEDDED_HARDWARE_EVENT_TOUCH = 4,

   // clap_embedded_hardware_event_axis, from a STICK
   CLAP_EMBEDDED_HARDWARE_EVENT_AXIS = 5,

   // clap_embedded_hardware_event_axis, from a MOTION sensor; apart from AXIS
   // so a plugin can leave out a high rate stream.
   CLAP_EMBEDDED_HARDWARE_EVENT_MOTION = 6,

   // clap_embedded_hardware_event_touch_point, from a TOUCH_SURFACE; one event per
   // finger, so get_input_state returns false and fingers come only as events.
   CLAP_EMBEDDED_HARDWARE_EVENT_TOUCH_POINT = 7,

   // clap_embedded_hardware_event_reset; delivered to every thread which gets
   // any event of the panel, whatever the routing says.
   CLAP_EMBEDDED_HARDWARE_EVENT_RESET = 8,

   // clap_embedded_hardware_event_focus; delivered as RESET is
   CLAP_EMBEDDED_HARDWARE_EVENT_FOCUS = 9,

   // Outputs

   // clap_embedded_hardware_event_pixels
   CLAP_EMBEDDED_HARDWARE_EVENT_PIXELS = 16,

   // clap_embedded_hardware_event_rumble
   CLAP_EMBEDDED_HARDWARE_EVENT_RUMBLE = 17,

   // clap_embedded_hardware_event_text
   CLAP_EMBEDDED_HARDWARE_EVENT_TEXT = 18,
};

typedef struct clap_embedded_hardware_event_button {
   clap_event_header_t header;

   clap_id element_id;

   // 1 pressed, 0 released
   uint32_t pressed;

   uint64_t timestamp_ns;

   // 0..1, on press; 1 for an element without VELOCITY and on release.
   double velocity;
} clap_embedded_hardware_event_button_t;

typedef struct clap_embedded_hardware_event_switch {
   clap_event_header_t header;

   clap_id element_id;

   // The position the switch is in, 0..position_count-1 of its SWITCH detail.
   uint32_t selected_index;

   uint64_t timestamp_ns;
} clap_embedded_hardware_event_switch_t;

typedef struct clap_embedded_hardware_event_value {
   clap_event_header_t header;

   clap_id  element_id;
   uint32_t padding;
   uint64_t timestamp_ns;

   // 0..1: the left or bottom end of the travel is 0, a pot fully anti-clockwise is
   // 0, a released trigger is 0.
   double value;
} clap_embedded_hardware_event_value_t;

typedef struct clap_embedded_hardware_event_delta {
   clap_event_header_t header;

   clap_id element_id;

   // Positive is clockwise; in the encoder's steps, so a detented encoder sends one
   // per click. Accumulated if the device saw several since the last event.
   int32_t steps;

   uint64_t timestamp_ns;
} clap_embedded_hardware_event_delta_t;

typedef struct clap_embedded_hardware_event_touch {
   clap_event_header_t header;

   clap_id element_id;

   // 1 a finger is on it, 0 it left.
   uint32_t touched;

   uint64_t timestamp_ns;
} clap_embedded_hardware_event_touch_t;

// Every axis of an input at once, so a stick moving diagonally is one event
// - STICK: [0] x, -1 left to +1 right; [1] y, -1 down to +1 up; centre calibrated,
//   no dead zone applied
// - MOTION, ACCELEROMETER: m/s^2, including gravity as phones report it, so a device
//   held upright and still reads about +9.81 on y; GYROSCOPE: rad/s, anti-clockwise
//   positive about each axis. [0] x right, [1] y up, [2] z out of the front
// Unused entries are 0. The host may coalesce consecutive moves of one input.
typedef struct clap_embedded_hardware_event_axis {
   clap_event_header_t header;

   clap_id element_id;

   // The element's axis_count
   uint32_t axis_count;

   uint64_t timestamp_ns;
   double   values[3];
} clap_embedded_hardware_event_axis_t;

enum clap_embedded_hardware_touch_phase {
   CLAP_EMBEDDED_HARDWARE_TOUCH_BEGIN = 0,
   CLAP_EMBEDDED_HARDWARE_TOUCH_MOVE = 1,
   CLAP_EMBEDDED_HARDWARE_TOUCH_END = 2,
};

// One finger on a TOUCH_SURFACE. Its x and y run 0..1 across the element, x right and y
// down; on a surface laid over a display they run across that display as the user sees
// it. A touch in progress may move outside 0..1.
typedef struct clap_embedded_hardware_event_touch_point {
   clap_event_header_t header;

   clap_id element_id;

   // Stable from BEGIN to END, reused afterwards
   uint32_t touch_id;

   uint64_t timestamp_ns;

   // clap_embedded_hardware_touch_phase
   uint32_t phase;
   uint32_t padding;

   double x;
   double y;
} clap_embedded_hardware_event_touch_point_t;

// See RESET in the comment at the top.
typedef struct clap_embedded_hardware_event_reset {
   clap_event_header_t header;

   // The panel whose state to forget, CLAP_INVALID_ID for every panel of every device.
   // A panel no longer in the lists has been removed.
   clap_id panel_id;

   uint32_t padding;
   uint64_t timestamp_ns;
} clap_embedded_hardware_event_reset_t;

typedef struct clap_embedded_hardware_event_focus {
   clap_event_header_t header;

   // The panel, CLAP_INVALID_ID for every panel of every device
   clap_id panel_id;

   // 1: the plugin's outputs on the panel are shown. 0: they may not be, for now.
   uint32_t focused;
   uint64_t timestamp_ns;
} clap_embedded_hardware_event_focus_t;

enum clap_embedded_hardware_pixels_mode {
   // The event carries count colours, one per pixel.
   CLAP_EMBEDDED_HARDWARE_PIXELS_EACH = 0,

   // The event carries one colour for all count pixels; first = 0 and count =
   // UINT32_MAX set every pixel of the element.
   CLAP_EMBEDDED_HARDWARE_PIXELS_FILL = 1,
};

// Sets pixels first .. first + count - 1 of an LED, LED_RING or LED_STRIP; the rest
// keep their colour, and pixels beyond the element's pixel_count are ignored.
//
// The colours follow the struct in the same event, so header.size is
// CLAP_EMBEDDED_HARDWARE_PIXELS_SIZE(count) for EACH and
// CLAP_EMBEDDED_HARDWARE_PIXELS_SIZE(1) for FILL; a host reads no colour past
// header.size. There is no cap on count, but an out queue may refuse a large event,
// so a plugin keeps them to what it needs.
//
// Colours: alpha is the brightness, red/green/blue the colour. A MONO LED lights at
// alpha scaled by the brightest of red, green and blue, so 255,255,255,255 is fully on
// and 0,0,0,0 off for every model. A BICOLOR LED uses red and green.
typedef struct clap_embedded_hardware_event_pixels {
   clap_event_header_t header;

   clap_id  element_id;
   uint32_t first;
   uint32_t count;

   // clap_embedded_hardware_pixels_mode
   uint32_t mode;

   // Followed by the colours, clap_color_t each
} clap_embedded_hardware_event_pixels_t;

#define CLAP_EMBEDDED_HARDWARE_PIXELS_SIZE(count)                                                  \
   ((uint32_t)(sizeof(clap_embedded_hardware_event_pixels_t) + (count) * sizeof(clap_color_t)))

static inline clap_color_t *
clap_embedded_hardware_pixels_colors(clap_embedded_hardware_event_pixels_t *ev) {
   return (clap_color_t *)(ev + 1);
}

static inline const clap_color_t *
clap_embedded_hardware_pixels_colors_const(const clap_embedded_hardware_event_pixels_t *ev) {
   return (const clap_color_t *)(ev + 1);
}

// Runs a RUMBLE element at intensity for duration_ms, replacing whatever it was doing.
// Intensity 0 or duration_ms 0 stops it; there is no "run forever". The host may clamp
// long durations. From process(), the host starts it at header.time as closely as the
// hardware allows.
typedef struct clap_embedded_hardware_event_rumble {
   clap_event_header_t header;

   clap_id  element_id;
   uint32_t duration_ms;

   // 0..1
   double intensity;
} clap_embedded_hardware_event_rumble_t;

// The longest line a TEXT event carries, terminating zero included.
#define CLAP_EMBEDDED_HARDWARE_TEXT_SIZE 64

// Sets one line of a display the host draws: any TEXT display, or a GRAPHIC one the
// plugin has not claimed. The line is replaced whole; the host cuts it at the
// display's width and shows what the device's characters or font can show. A line
// past the display's rows is ignored, and line UINT32_MAX clears every line.
typedef struct clap_embedded_hardware_event_text {
   clap_event_header_t header;

   clap_id  element_id;
   uint32_t line;

   // UTF-8, zero terminated
   char text[CLAP_EMBEDDED_HARDWARE_TEXT_SIZE];
} clap_embedded_hardware_event_text_t;

/* -- Displays the plugin draws -- */

// In pixels, x right and y down from the canvas's top left corner.
typedef struct clap_embedded_hardware_rect {
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} clap_embedded_hardware_rect_t;

// pixels/2's surface in a frame: the host's buffer, valid only during display_render.
typedef struct clap_embedded_hardware_pixels_surface {
   void    *data;
   uint32_t stride_bytes;

   // clap_embedded_hardware_display_color_format: XRGB8888 in pixels/2
   uint32_t color_format;

   // 0: the contents are undefined, draw everything. n: the buffer holds the frame
   // presented n frames ago, so only what changed since needs redrawing.
   uint32_t buffer_age;

   // The room in damage, at least 1.
   uint32_t damage_capacity;

   // The plugin writes the rectangles which changed since the previous frame here and
   // their number in damage_count; 0 means the whole canvas. With more than
   // damage_capacity, it writes one rectangle around them all, or 0.
   clap_embedded_hardware_rect_t *damage;
   uint32_t                       damage_count;

   uint32_t reserved[3];
} clap_embedded_hardware_pixels_surface_t;

typedef struct clap_embedded_hardware_display_config {
   // The API the host offers, a CLAP_EMBEDDED_HARDWARE_DISPLAY_API_ string or a
   // vendor's; the plugin compares it by value, as strcmp does.
   const char *api;

   // What the API hands over for as long as the display is open, selected by api; null
   // for pixels/2.
   union {
      const void *ptr;
   } surface;

   // The display's size in pixels, as in its DISPLAY detail.
   uint32_t width;
   uint32_t height;

   // Nominal refresh of the display
   double refresh_rate_hz;

   uint32_t reserved[4];
} clap_embedded_hardware_display_config_t;

typedef struct clap_embedded_hardware_frame {
   // As in the config
   const char *api;

   // What the API hands over for this frame, selected by api.
   union {
      // pixels/2
      clap_embedded_hardware_pixels_surface_t *pixels;
      void                                    *ptr;
   } surface;

   // Estimate of when this frame reaches the display, on the get_time_ns clock, for
   // animation.
   uint64_t present_time_ns;

   // As in the config
   uint32_t width;
   uint32_t height;

   uint32_t reserved[4];
} clap_embedded_hardware_frame_t;

enum clap_embedded_hardware_render_result {
   // The frame is drawn; present it, or the damage of it.
   CLAP_EMBEDDED_HARDWARE_RENDER_RENDERED = 0,

   // Nothing differs from the previous frame; upload nothing.
   CLAP_EMBEDDED_HARDWARE_RENDER_UNCHANGED = 1,

   // Drawing failed; keep the previous frame. The host may display_close a display
   // which keeps failing.
   CLAP_EMBEDDED_HARDWARE_RENDER_FAILED = 2,
};

/* -- The host side -- */

typedef struct clap_host_embedded_hardware {
   // The devices shown to the plugin now.
   // [main-thread]
   uint32_t(CLAP_ABI *count_devices)(const clap_host_t *host);

   // [main-thread]
   bool(CLAP_ABI *get_device)(const clap_host_t                    *host,
                              uint32_t                              index,
                              clap_embedded_hardware_device_info_t *info);

   // The panels of one device attached now.
   // [main-thread]
   uint32_t(CLAP_ABI *count_panels)(const clap_host_t *host, clap_id device_id);

   // [main-thread]
   bool(CLAP_ABI *get_panel)(const clap_host_t                   *host,
                             clap_id                              device_id,
                             uint32_t                             index,
                             clap_embedded_hardware_panel_info_t *panel);

   // The elements on one panel.
   // [main-thread]
   uint32_t(CLAP_ABI *count_elements)(const clap_host_t *host, clap_id panel_id);

   // [main-thread]
   bool(CLAP_ABI *get_element)(const clap_host_t                     *host,
                               clap_id                                panel_id,
                               uint32_t                               index,
                               clap_embedded_hardware_element_info_t *element);

   // One detail of an element, see details at the top. The plugin passes the struct
   // detail_type names and its size in capacity; the host fills up to capacity bytes,
   // header included, and zeroes the rest. Returns false for an unknown id, a detail
   // the element does not have or a capacity smaller than the struct in this header.
   // [main-thread]
   bool(CLAP_ABI *get_element_detail)(const clap_host_t                      *host,
                                      clap_id                                 element_id,
                                      uint32_t                                detail_type,
                                      clap_embedded_hardware_detail_header_t *detail,
                                      uint32_t                                capacity);

   // Where an input is now, as the event of event_type a RESET would replay, header
   // included: the plugin passes that event's struct and its size in capacity. See
   // state at the top. Returns false for an unknown id, an output, an event type the
   // element does not send, DELTA and TOUCH_POINT, which have no single state, or a
   // capacity smaller than the event.
   // [main-thread]
   bool(CLAP_ABI *get_input_state)(const clap_host_t   *host,
                                   clap_id              element_id,
                                   uint16_t             event_type,
                                   clap_event_header_t *state,
                                   uint32_t             capacity);

   // The answers from clap_plugin_embedded_hardware.input_routing or
   // output_routing have changed; the host asks again for every input, output and
   // event type, then sends RESET where an input was routed away, and takes back an
   // output routed away.
   // [main-thread]
   void(CLAP_ABI *routing_changed)(const clap_host_t *host);

   // Asks for a process_main_thread() call soon, e.g. to send a rumble or text from a
   // UI action. Wait-free.
   // [thread-safe]
   void(CLAP_ABI *request_process_main_thread)(const clap_host_t *host);

   // Now, on the monotonic clock timestamp_ns and present_time_ns use, in nanoseconds:
   // CLOCK_MONOTONIC on Linux, the performance counter on Windows, Mach absolute time
   // on macOS. The clock does not advance while the device is suspended, so timestamps
   // and present_time_ns do not jump across a resume; a host whose platform clock does
   // advance subtracts the time spent suspended. Wait-free.
   // [thread-safe]
   uint64_t(CLAP_ABI *get_time_ns)(const clap_host_t *host);

   // Sets pixels first .. first + count - 1 of an output, as a PIXELS event in EACH
   // mode does, but from the main thread, whether or not the plugin is processing.
   // Returns false for an unknown id, an element without pixels or one the plugin has
   // not routed ROUTE_MAIN_THREAD in output_routing.
   // [main-thread]
   bool(CLAP_ABI *set_pixels)(const clap_host_t  *host,
                              clap_id             element_id,
                              uint32_t            first,
                              uint32_t            count,
                              const clap_color_t *colors);

   // Asks for one display_render() of a display the plugin opened, at its next
   // refresh, for an API which renders on request, as pixels/2 does; calls before that
   // coalesce. Wait-free, so a meter can ask from the audio thread.
   // [thread-safe]
   void(CLAP_ABI *display_request_frame)(const clap_host_t *host, clap_id display_id);

   // Asks the host to end the plugin: back to a launcher, or off. The host tears the
   // plugin down through the usual CLAP lifecycle and may ask the user first, so the
   // plugin keeps running normally until then. The reason is shown to the user if the host
   // can, e.g. a crash message; null or "" for none.
   // [main-thread]
   void(CLAP_ABI *request_quit)(const clap_host_t *host, const char *reason);

   // A property of a device or of one panel, by id and key; see properties above.
   // Copies the value, UTF-8 and NUL terminated, into value, truncated to capacity
   // bytes, and returns its length without the NUL, as snprintf does, so a plugin can
   // ask again with a bigger buffer. Returns -1, with value "" if capacity allows, for
   // a key the host does not know or an unknown id.
   // [main-thread]
   int32_t(CLAP_ABI *get_property)(
      const clap_host_t *host, clap_id id, const char *key, char *value, uint32_t capacity);

   // Whether the host can offer this API on a GRAPHIC display; always true for
   // CLAP_EMBEDDED_HARDWARE_DISPLAY_API_PIXELS. See -- Displays --
   // [main-thread]
   bool(CLAP_ABI *display_is_api_supported)(const clap_host_t *host,
                                            clap_id            display_id,
                                            const char        *api);
} clap_host_embedded_hardware_t;

/* -- The plugin side -- */

enum clap_embedded_hardware_routing {
   // The events arrive in process()
   CLAP_EMBEDDED_HARDWARE_ROUTE_PROCESS = 1 << 0,

   // The events arrive in process_main_thread(), on the main thread.
   CLAP_EMBEDDED_HARDWARE_ROUTE_MAIN_THREAD = 1 << 1,
};

typedef struct clap_plugin_embedded_hardware {
   // Where to deliver one event type of one input element, as
   // clap_embedded_hardware_routing flags; 0 drops them, which also tells the
   // host the plugin does not use them for now. Asked after init() has returned, once
   // the plugin has asked the event registry for the space (typically in init(), which
   // is why the host waits for it to return), for every input and every
   // event type its kind sends (see each kind), and again after hardware_changed and
   // routing_changed.
   // [main-thread]
   uint32_t(CLAP_ABI *input_routing)(const clap_plugin_t *plugin,
                                     clap_id              element_id,
                                     uint16_t             event_type);

   // Which threads the plugin sends one event type of one output element from, as
   // clap_embedded_hardware_routing flags; 0 leaves the output to the host. See
   // -- Output --. Asked when input_routing is, for every output and every event type
   // it takes (LED, LED_RING, LED_STRIP: PIXELS; RUMBLE: RUMBLE; DISPLAY: TEXT).
   // [main-thread]
   uint32_t(CLAP_ABI *output_routing)(const clap_plugin_t *plugin,
                                      clap_id              element_id,
                                      uint16_t             event_type);

   // Exchanges main-thread events, as process() does audio-thread ones: the inputs
   // routed ROUTE_MAIN_THREAD, with header.time 0, in in; outputs (PIXELS, RUMBLE,
   // TEXT) into out. Called when there are inputs to deliver, and after
   // request_process_main_thread(), when in may be empty. Never called from inside a
   // call the plugin is making. A valid host only puts this extension's events in in,
   // but a plugin checks header.space_id and header.type, carefully and fully as in
   // process(), and skips what it does not know; see checking spaces at the top.
   // [main-thread]
   void(CLAP_ABI *process_main_thread)(const clap_plugin_t        *plugin,
                                       const clap_input_events_t  *in,
                                       const clap_output_events_t *out);

   // Devices or panels came or went; re-read the lists. The ids of devices, panels and
   // elements which are still there are unchanged. Held state is cleared by the RESET
   // for each panel, not by this call.
   // [main-thread]
   void(CLAP_ABI *hardware_changed)(const clap_plugin_t *plugin);

   // A GRAPHIC display: the API the plugin would rather draw it with. Assign one of the
   // CLAP_EMBEDDED_HARDWARE_DISPLAY_API_ strings, or a string of the plugin's
   // that lives as long as the plugin, and return true; false for no preference. The
   // host need not honour it; the config says which API it offers.
   // [main-thread]
   bool(CLAP_ABI *display_get_preferred_api)(const clap_plugin_t *plugin,
                                             clap_id              display_id,
                                             const char         **api);

   // The host offers a GRAPHIC display, to be drawn with config->api, once it has asked
   // input_routing; true claims it, false leaves it to the host and to TEXT events, or
   // refuses that API, and the host may offer another. Allocate drawing resources here.
   // [main-thread]
   bool(CLAP_ABI *display_open)(const clap_plugin_t                           *plugin,
                                clap_id                                        display_id,
                                const clap_embedded_hardware_display_config_t *config);

   // Shown or hidden, e.g. under a system menu or on suspend. Hidden is a good time to
   // stop animating.
   // [main-thread]
   void(CLAP_ABI *display_set_visible)(const clap_plugin_t *plugin,
                                       clap_id              display_id,
                                       bool                 visible);

   // Draws one frame into frame->surface, as frame->api says: for pixels/2 into
   // frame->surface.pixels->data, listing what changed in its damage. Only called while
   // visible, and only for an API which renders on request. Returns a
   // clap_embedded_hardware_render_result.
   // [main-thread]
   uint32_t(CLAP_ABI *display_render)(const clap_plugin_t                  *plugin,
                                      clap_id                               display_id,
                                      const clap_embedded_hardware_frame_t *frame);

   // The host takes a claimed display back, e.g. when its panel goes, on suspend or
   // before destroy; free what display_open allocated.
   // [main-thread]
   void(CLAP_ABI *display_close)(const clap_plugin_t *plugin, clap_id display_id);

   // The host's device goes to sleep or wakes; see -- Lifecycle -- at the top.
   // [main-thread]
   void(CLAP_ABI *suspend)(const clap_plugin_t *plugin);
   void(CLAP_ABI *resume)(const clap_plugin_t *plugin);
} clap_plugin_embedded_hardware_t;

#ifdef __cplusplus
}
#endif

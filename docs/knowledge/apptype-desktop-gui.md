---
name: apptype-desktop-gui
description: Architecture of desktop GUI applications - the event loop, signal and slot or callback communication, widget hierarchy, declarative UI files, separating presentation from domain, and connection types across threads
domain: software-architecture
tags: desktop,gui,event-loop,signals-slots,qt,gtk,separated-presentation
apply_when: "designing a desktop application in Qt, GTK or a similar toolkit; UI code contains business logic; deciding how objects talk to each other in an event-driven UI; making UI logic testable"
sources: "Qt documentation - Signals and Slots, doc.qt.io (read); Qt documentation - Model/View Programming, doc.qt.io (read); GTK documentation - Getting started with GTK 4; List Widget Overview, docs.gtk.org (read); GLib documentation - The Main Event Loop, docs.gtk.org (read); Qt documentation - Threading Technologies; Threads and QObjects (read); Python documentation - threading; glossary entry Global Interpreter Lock (read); Fowler - GUI Architectures, martinfowler.com/eaaDev (read)"
last_reviewed: 2026-09-25
confidence: medium
---

# Desktop GUI architecture

Presentation patterns (MVC, MVP, Presentation Model) are compared in `style-ui-mvc-family`.
This note covers what is specific to a desktop toolkit: the runtime model and how a whole
application is organized around it.

## The toolkit's runtime model

- **Event-driven.** GTK states that the toolkit listens for events such as a button click
  and passes them to the application. A `GtkApplication` object manages the lifecycle;
  `g_application_run()` runs the main loop that processes events and emits signals.
- **Widget hierarchy.** The interface is a tree of widgets under a window, laid out by
  containers (GtkBox, GtkGrid, GtkStack in GTK).
- **Signals connect objects.** In Qt, a signal is emitted when something happens and a slot
  is a function called in response. Qt documents the emitter as neither knowing nor caring
  which slots receive the signal, the signatures must match (type safety), and connections
  are cleaned up automatically. GTK connects handlers with `g_signal_connect()`.

## The main loop owns one thread

GLib: the main loop manages all event sources (file descriptors, timeouts, idle callbacks) for
GLib and GTK applications; each main context runs in a single thread, and sources have
priorities so higher-priority events are dispatched first. Handlers therefore run one at a
time on that thread. The docs imply rather than state the consequence (inference): a long
handler freezes the interface, so slow work belongs on another thread or an async source, with
results returned through the loop.

## Connection types and threads

Qt distinguishes direct and queued connections. A direct connection runs the slot
immediately, like a function call. With a queued connection, code after `emit` continues at
once and the slot runs later, in the thread of the receiver. Qt provides a context object to
say in which thread a receiver should run. This is the toolkit's built-in way to cross
threads without sharing state directly. Cost: Qt says emitting a signal connected to slots
is roughly ten times slower than calling the receivers directly, which is negligible next to
allocation or system calls but matters in a tight inner loop.

## Model/view separation for data-heavy widgets

| Concept | Qt | GTK 4 |
|---|---|---|
| Model | `QAbstractItemModel` and ready-made models | `GListModel` implementations (`GListStore`, `GtkStringList`) |
| View | list, table and tree views addressing items by model index | `GtkListView`, `GtkGridView` |
| Presentation | delegate renders and edits items | list item factory maps items to widgets |
| Selection and adaptation | shared selection models; proxy models sort or filter without touching the source | selection, filter and sort models |

Qt describes its model/view design as MVC with view and controller combined, still separating
data from presentation. GTK 4 recycles a limited number of list item widgets by binding them to
new items, so state must live in the model items, never in the widgets; this keeps widget
memory constant for lists of millions of rows.

## Choosing the worker mechanism (Qt and Python)

Qt's options: `QThread` for long-lived workers or work needing its own event loop (worker object
moved to the thread with `moveToThread`), `QThreadPool` with `QRunnable` for single operations or
task queues with thread reuse, and Qt Concurrent for map/filter/reduce over containers. Rules
from the Qt docs: widget classes are not reentrant and may only be used from the main thread;
a QObject lives in the thread that created it; connect across threads with queued connections;
a child QObject must be created in the parent's thread.

**For PyQt and other Python GUIs the language runtime matters too.** CPython's global
interpreter lock lets only one thread execute Python bytecode at a time. Python's docs state
that to use multiple cores for computation you should use `multiprocessing` or
`concurrent.futures.ProcessPoolExecutor`, while threads remain appropriate for I/O-bound work.
Exceptions: some extension modules release the lock during compute-intensive work (the docs
name compression and hashing as examples), and the lock is always released during I/O. As of
Python 3.13 a free-threaded build without the lock is available as a build option.

Decision aid (synthesis of the two documentations; whether a given library, such as an image
library, releases the lock must be checked in that library's documentation, not assumed):

| Work | Mechanism |
|---|---|
| I/O-bound (network, disk) | worker thread or async source; the lock is released during I/O |
| CPU-bound in pure Python | separate process (`ProcessPoolExecutor` or `multiprocessing`); send results back to the GUI thread by signal or main-loop callback |
| CPU-bound inside a library that releases the lock | worker thread is enough |
| Very short operation | run it directly; a worker adds cost |

Processes add serialization of inputs and outputs and higher memory use (inference); for large
images, pass file paths or shared memory rather than pickled copies where the library allows.

## Structure of the application

1. **Separate presentation from domain.** Fowler's central guidance: a clear division between
   domain objects that model the real world and presentation objects that are the screen
   elements. Domain objects then work without the UI and can serve several presentations
   (GUI, command line, web) without duplicated rules.
2. **Keep synchronization implicit where possible.** With observer synchronization, views
   observe the model and react when it changes, so controllers do not coordinate screen
   updates by hand. Signals and slots are the toolkit form of this idea.
3. **Keep the view thin.** The further MVP or Presentation Model variants move logic out of
   the widget, the more can be tested without a display (`style-ui-mvc-family`,
   `quality-testability`).
4. **Declare the UI, do not hardcode it.** GTK's GtkBuilder loads interfaces from XML files,
   separating presentation from logic and allowing UI changes without recompiling. GResource
   embeds UI files and assets in the binary; GSettings holds preferences with schemas and can
   bind them to widget properties, decoupling configuration from code.
5. **Depend inward.** Domain code must not import toolkit types; adapt at the edge
   (`style-hexagonal`, `principle-layering-dependency-rule`).

## Use when

- Any interactive desktop application with more than a few screens, or one whose logic must
  be reused or tested outside the toolkit.

## Do not use when

- Full layering and presentation models for a one-window utility: Forms and Controls with
  direct widget handling is a valid, simpler choice (`style-ui-mvc-family`).
- Do not use a signal for every call. Direct calls are clearer inside one component and cost
  less.

## Trade-offs

- Signals decouple emitter from receiver, but control flow becomes harder to trace than a
  direct call.
- Queued (asynchronous) delivery lets work proceed in another thread, but state may have
  changed by the time the slot runs (`dist-communication-styles` discusses the same effect
  across processes).
- Declarative UI files give flexibility but move errors from compile time to load time.

## Common mistakes

- Business rules inside slot or handler functions, so they cannot be tested or reused.
- Widgets used as the data store (fatal with GTK 4's recycled list items).
- Blocking the main loop with file, network or heavy computation inside a handler.
- Using a thread for CPU-bound pure-Python work and expecting the UI and the work to both run at full speed: the lock serializes bytecode.
- Touching widgets from a worker thread.
- Connections whose lifetime is not tied to the objects involved.
- Doing long work directly in a handler instead of handing it to another thread through the
  toolkit's queued mechanism (check your toolkit's threading rules).

## Related

- Notes: `style-ui-mvc-family`, `style-hexagonal`, `style-event-driven`,
  `principle-layering-dependency-rule`, `quality-testability`, `apptype-game-engine`,
  `cross-configuration`

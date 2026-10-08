# Architecture

What this codebase is built out of, judged against the patterns the field has
settled on, and what is worth changing. Written 2026-10-08.

The point of the exercise is not to collect patterns. It is to find the
places where the current shape will resist the next thing we want to build.

## What is already here, and what it is called

These are not aspirations. Each is in the code today.

| Pattern | Where |
|---|---|
| **Ports and adapters** (GoF Strategy, Bridge) | `RenderBackend`, `CollisionDetector`, `NetworkEventSource`, `NetworkEventDecoder`, `LaunchMonitor`, `ShotDataParser`, `JsonParser`, `Clock`, `InputSink`, `CameraTarget`, `GroundHeights`, `EngineLauncher` |
| **Pimpl / opaque pointer** | `Engine`, `Renderer`, `Application`, `ObjLoader`, `PhysicsWorld`, `Round` |
| **Command** + **Event Queue** | `EngineCommand` variant, queued under a mutex, drained at a tick boundary |
| **State** | `game::Round` |
| **Flyweight** | `Renderer::set_scene` deduplicates meshes, pipelines, textures and samplers by shared identity; `MaterialInstance` holds the extrinsic bindings against an intrinsic `Material` |
| **Handle indirection** | `EntityId`, `RenderableId`, `BodyId`, `AnimationPlayerId` |
| **Builder** | `RigidBodyBuilder` |
| **Update Method / Game Loop** | `Engine::tick` |
| **Null Object** | `RenderBackend::update_mesh` and `begin_shadow_pass` default to doing nothing, so a backend that cannot do a thing is never driven through it |

Ports and adapters is applied consistently and is the single best thing about
the codebase. It is why physics, golf, networking and the round are all
testable without a window, and why 187 of the tests run headless.

**Service Locator is deliberately absent.** Everything is constructor
injected. Nystrom lists Service Locator with heavy caveats and he is right;
it should stay absent.

## Where the shape will resist the next thing

Ordered by when it will actually bite, not by how interesting it is.

### 1. Render passes are not data — and this blocks the roadmap

`src/opengl/opengl_backend.cpp` is 1322 lines, the largest file in the
project, and it hardcodes the frame: shadow cascades, then colour, then
composite. Targets are members. Ordering is control flow.

Every remaining visual item — bloom, ambient occlusion, depth of field, the
range finder's mask — adds a target, a pass, and an ordering constraint, in
that file, by hand. Three or four more and it is unmaintainable.

The canonical answer is the frame graph, as O'Donnell described Frostbite's
at GDC 2017: declare passes with the resources they read and write, let the
system topologically sort them, allocate transient targets by lifetime, and
cull passes whose output nothing consumes.

We do not need the whole thing. There are no barriers or layout transitions
to infer in OpenGL, and there is no bindless memory aliasing to do. What we
need is the half that pays: **passes as values, with declared inputs and
outputs, and transient targets owned by the graph rather than by the
backend.** That turns "add bloom" into registering a pass instead of editing
a god function, and it makes the pass list inspectable, which is most of
debugging a renderer.

This is the highest-value structural change available and it is the one
standing in front of the visual roadmap.

### 2. `EntityRecord` is a fat struct of optionals

```
struct EntityRecord { EntityId id; RenderableId renderable;
                      std::optional<BodyId> body;
                      std::optional<AnimationPlayerId> animation_player;
                      Transform transform; };
```

Adding an aspect means editing the record and every loop that walks it. That
is exactly the problem Nystrom's **Component** pattern addresses.

It does **not** follow that we should adopt an ECS. We have around sixty
entities and four aspects. The literature's sparse-set-versus-archetype
trade-off — sparse sets cheaper to modify, archetypes faster to iterate —
only starts to matter at scales we are nowhere near, and EnTT or flecs would
be a dependency bought to solve a problem we do not have.

The right-sized change is separate component tables keyed by `EntityId`,
which is the Component pattern without the framework.

### 3. Lookups are linear scans

Ten `std::ranges::find` by identifier across the engine, renderer and physics
world. `synchronize_physics` walks every entity and does a find inside the
loop, so it is quadratic in entity count.

At sixty entities this is free and it would be silly to care. It stops being
free the moment anything is instanced per-entity rather than per-mesh.

The fix is a dense array with a sparse identifier-to-index map — which is
what a sparse-set ECS is, arrived at because we needed it rather than because
it was fashionable. Worth doing at the same time as (2), since they touch the
same code.

### 4. `Engine::Impl` is a god object

*(The air-density part of this is now done — see the note at the end.)*

Eighteen members spanning rendering, physics, animation, networking,
hardware, input, command queueing, entity storage and the wet trail. Lifting
the round out removed a hundred and twenty lines of it; there is more.

The clearest remaining piece is in the command visitor, which computes air
density from temperature inline:

```cpp
float temp_k = value.temperature_c + 273.15f;
float rho = 101325.0f / (287.058f * temp_k);
```

That is atmospheric physics with a hardcoded sea-level pressure, living in a
switch statement in the runtime. It belongs in `mgv::physics` beside the
other units, where it can be tested and where the pressure can come from the
weather service that already fetches it.

### 5. No event bus — and that is currently correct

`RoundEvent` is returned up the call stack to whoever called `advance_round`.
That is the simplest thing that works, and with one consumer it is the right
answer. When the UI, audio and telemetry all want the same events, it becomes
Observer or Nystrom's Event Queue. Not before.

## A trade-off worth naming rather than fixing

`EngineCommand` is a closed `std::variant` of eleven alternatives. Every new
command touches the variant and its visitor, which is an open/closed
violation, and the textbook remedy is a polymorphic `Command` with an
`execute`.

The closed variant is not obviously wrong here. It gives exhaustive checking
at compile time, needs no allocation to queue, and keeps commands as plain
values that cross a thread boundary safely. A polymorphic command would buy
extensibility we have not needed in eleven commands and cost all three.

Leave it. Revisit if the count doubles or if commands start needing to be
authored outside the engine's own translation unit.

## What not to do

- **Do not adopt an ECS framework.** See (2).
- **Do not introduce a Singleton for the renderer, the engine or the clock.**
  The clock is already injected, which is why physics is deterministic under
  test.
- **Do not add a Service Locator.** See above.
- **Do not make the round polymorphic.** Five states with simple transitions
  are clearer as an enum and a switch than as five classes; GoF State earns
  its keep when states carry behaviour and data of their own, and these do
  not.

## Order of work

1. Passes as data, with graph-owned transient targets. Unblocks the visual
   roadmap and tames the largest file.
2. Component tables plus a sparse identifier map. Fixes (2) and (3) together.
3. ~~Move air density into `mgv::physics`.~~ Done; see below.
4. An event bus, when there is a second consumer.

## Done since writing this

**Air density, and what pulling on it turned up.** The weather command did
more than compute density badly. It took the pressure as sea level
regardless of where the round was played, which discards the several percent
of carry that altitude is worth. It derived turf wetness from temperature --
colder than fifteen degrees meant soaking -- while `WeatherCondition` was
already carrying a measured `turf_wetness` and an `is_raining` the command
had no field for. And it open-coded the compass convention a third time,
after the sun and the shot model.

`physics::air_density` now takes temperature, pressure and humidity and
applies Dalton's law over Tetens' saturation curve, so humid air comes out
*less* dense than dry air, which is the opposite of what most people expect
and is tested for that reason. `mgv::bearing_to_direction` is the one place
the convention is written down, and the sun, the wind and shot aiming all
go through it.

## Sources

- Robert Nystrom, [*Game Programming Patterns*](https://gameprogrammingpatterns.com/contents.html) — Command, Flyweight, Observer, Prototype, Singleton and State revisited; Component, Event Queue and Service Locator under decoupling; Data Locality, Dirty Flag, Object Pool and Spatial Partition under optimisation
- Yuriy O'Donnell, [*FrameGraph: Extensible Rendering Architecture in Frostbite*](https://www.slideshare.net/DICEStudio/framegraph-extensible-rendering-architecture-in-frostbite), GDC 2017
- Gamma, Helm, Johnson, Vlissides, *Design Patterns* (1994), for the vocabulary
- [EnTT](https://github.com/skypjack/entt) and [flecs](https://github.com/SanderMertens/flecs) as the reference sparse-set and archetype implementations, and [ecs_benchmark](https://github.com/abeimler/ecs_benchmark) for the trade-off between them

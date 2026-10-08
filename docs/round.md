# The round

`mgv::game::Round` is the state machine for a round of golf: what is
happening, and where the camera should be while it happens.

It knows nothing of the renderer, the physics world, or the engine. It is
given what was observed and returns what it decided, which is what lets it be
tested without a window — see `tests/game/round_test.cpp`.

## Why it is not in the engine

It was. The state machine lived inside `Engine::tick`, between the animation
step and the physics step, and it had grown the shape that implies: camera
positions as literals in the middle of a tick, timings as bare numbers, the
cup at a hardcoded point that was not where the pin had been placed, and ten
`std::cout` calls acting as the user interface.

The engine is a renderer runtime. The rules of golf are not its business, and
nothing about them could be tested without standing up a GL context.

## States

| State | Meaning | Camera |
|---|---|---|
| `addressing` | A ball at rest, waiting to be struck | the player's |
| `in_flight` | The ball is moving | behind and above it |
| `walking` | Following the ball to where it stopped | walks the ground |
| `holing_out` | The ball is in the cup | drops to look in |
| `sighting` | Looking through the range finder | hand-held, wandering |

`RoundUpdate::directs_camera` says whether the round is driving. It is false
while addressing: the player is lining up a shot and the view is theirs. It is
true for the states the round is staging.

## Ports

`GroundHeights` is the ground the round walks the camera over. A port rather
than the terrain itself, because the rules of a round do not depend on how the
ground is generated, and because a test wants a hill it can describe in two
lines. Walking interpolates between two stances and then puts the result on
the ground, rather than interpolating through it; a straight line between two
points on a slope goes underground in the middle.

## Events

`RoundEvent` is a value, not a line of console output. The engine has no
business deciding that the way to tell a player their ball is holed is to
write to standard output. Frontends present them however they present things;
`mgv_capture` prints them, which is how the sequence below was checked.

## Where the hole is

One number, `CourseSessionDescription::hole`. The pin, the collar, the round's
idea of "holed", and the direction a shot is aimed all come from it.

Aiming is a round's business too. `Round::aim_bearing_degrees` gives the
bearing from a ball to the hole in the convention the shot model launches
along, and `play_test_shot` uses it. `InputAction::fire_test_shot` handled
inside the engine still fires straight down -Z: that is a viewer debug
command, not a played shot, and on this course it plays away from the green.

## Driving it

Frontends call `advance_round` each frame, alongside `stream_course`. The
round is deliberately outside `Engine::tick`, which means it sees the previous
tick's elapsed time — `Engine::last_tick_seconds` — rather than keeping a
second clock.

```sh
QT_QPA_PLATFORM=wayland ./build/wsl-qt/mgv_capture --out shot.png --shot --frames 240
round: ball struck
round: ball came to rest
round: reached ball
```

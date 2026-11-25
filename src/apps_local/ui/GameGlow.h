#pragma once

// A game's result shown in light: a quick triple flash on a win, one slow
// swell and fade on a loss. Uses the frontlight (cool on a win and warm on a
// loss where the light has both) and, on the Paper Mono, the status LED
// (green on a win, red on a loss). Off when the "Light on Win or Loss" setting
// is off; inert on boards with neither.
//
// Non-blocking: the pattern runs in a short task of its own and puts the
// light back exactly as the player had it.
namespace gameglow {

void win();
void lose();

}  // namespace gameglow

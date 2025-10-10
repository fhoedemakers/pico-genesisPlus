# Issues

- Waveshare RP2350-PiZero: cannot handle the higher clock frequency at all — the image keeps dropping out. At 252 MHz it works fine.

- DVI Driver gets stuck in an endless loop. 
This occurs only in-game, not in the menu. In-game it can occur within a few seconds (WaveShare RP2350Zero) or after several minutes of play. The screen goes black or blank with no-signal. The emulator on core0 is still running, but core1 (DVI Driver) is stuck in an endless loop.

This is solved by a copilot assisted change in pico_lib/util/queue.h

What changed

- Replaced std::vector push + erase pattern (which was O(n) and risked capacity misuse) with a fixed-size circular buffer (head/tail/count).
- Added capacity(), empty(), and tryDeque() helpers.
- enque now returns bool (fails gracefully if full instead of asserting only).
- Added optional spin limits to deque(spinLimit) and waitUntilContentAvailable(spinLimit) that will panic with a diagnostic instead of looping forever.
- Removed costly erase(begin()) (which could invalidate assumptions and cause subtle races if capacity reserve differed from size usage).

Why this helps your random endless loop

- Previous code relied on reserve() then push_back() and erase(begin()). If something elsewhere accidentally called queue_.resize() (or a reallocation occurred due to reserve mismatch during changes), the assertion path or timing could leave the consumer sleeping forever if a lost __sev() happens between empty checks.
- The new ring buffer always keeps elements in-place and uses __sev() only when an actual enqueue succeeds, reducing missed wake events.

Follow-up suggestions

- Audit all enque() call sites: they should now check the returned bool (or you can wrap with an assert if overflow is unacceptable).
- If you want a build-time safety net, define a macro to map old enque(x); usage to something that asserts the return value.
- Consider instrumenting if panics occur (spinLimit argument) by supplying a non-zero spinLimit in critical dequeue sites during debugging.
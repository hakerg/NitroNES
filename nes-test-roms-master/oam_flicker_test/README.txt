oam_flicker_test
2020-04-09
https://forums.nesdev.com/viewtopic.php?f=3&t=19915

To assemble: snarfblasm.exe oam_flicker_test.asm


This test allows rendering to be disabled at a user-controlled target location in order to observe its effects on OAM. The target is timed using blargg's PPU/NMI synchronization code ( http://forums.nesdev.com/viewtopic.php?t=6589 ) so that there is a jitter of just 1 dot across frames during rendering; this does mean, however, that the test is NTSC-only without changes being made to the timing to support PAL. PPUs that don't skip a dot every other frame (RGB PPUs and possibly NESRGB with dejitter) will likely have mildly-incorrect timing.

The test displays sprite tiles in an 8x8 tile grid with 0 on the top left and increasing in index from top to bottom, left to right. If a tile moves due to OAM corruption, there will usually be a lower-priority tile on the right end that can disappear to show where the tile has moved to. Tiles are offset in y position by 1 pixel to make it more clear exactly where tiles have moved to.

This ROM allows for testing of a variety of cases:

1. If rendering is disabled during certain parts of the scanline, it can cause a row of OAM to corrupt. These locations appear to begin around the start of the scanline and around the start of hblank. This usually results in one row (2 tiles) of OAM flickering on the following frame, as seen in Isolated Warrior (particularly when paused or in a bonus room). This occurs even if OAM is properly set up during vblank. This issue is confirmed to occur on RP2C02 revisions E, G, and H, but seems to not occur on revision B. At the time of writing, this issue is unemulated and was the motivating factor for this ROM. For testing this case, all that is needed is the D-pad to move the target location around the screen. Joypad 2 button A can be used to check persistence and is described in detail in the Controls section.

2. When rendering is disabled, OAM address ($2003) retains its value from rendering and can be incremented by 1 such that it's no longer a multiple of 4. This can cause problems if not set before OAM DMA or the next frame. See https://forums.nesdev.com/viewtopic.php?f=3&t=18414 for more information.

3. When rendering is disabled, it is delayed by a couple dots. Greyscale does not have this delay. When the target location is moved on top of a sprite tile, there should be a couple dots that show greyscale sprite pixels before the solid grey from disabled rendering starts.

4. OAM decay can occur when rendering and OAM DMA are both disabled. The extent of the decay depends on the location at which rendering is disabled (more time disabled means more decay) and likely on the particular console being tested. The start button can be used to control whether OAM DMA is performed.

5. OAM DMA is used by the test to maintain the PPU/NMI sync, so when it's disabled, the test tries to fall back on DMC DMA. The third cycle after the $4015 write is a lone write cycle, which will shorten DMC DMA length by 1 cycle if it lands there, providing the same effect as OAM DMA. While this method maintains a perfect sync on some emulators, it fails on real hardware, frequently breaking out of sync for a moment. Despite not working properly, this DMC DMA sync method is left in place because of the different behavior between emulators and real hardware.


The onscreen text is broken into two sections showing state for the PPU mask ($2001) write and the OAM writes. Where applicable, lines are prefixed with the button that can be used to modify that state. The meaning of the lines is as follows:

- Parity #: This alternates between 0 and 1 to help distinguish in video or screenshots between the two states of the PPU/NMI sync. For a given reset or power-on, a number will always correspond to the same dot (left or right) that the PPU mask write can land on. However, one shouldn't count on numbers having the same meaning across resets or power cycles.

(Write PPU)
- UD ##: The amount of scanline delay before the write to PPU mask, controlled with the up and down D-pad directions. Each unit is 114 CPU cycles, which will cause a drift of 1 dot right when moving down and 1 dot left when moving up. Because of this drift, this control should be used to move 1 dot forward or backward for testing behavior where the specific scanline does not matter.
- LR ##: The amount of cycle delay before the write to PPU mask, controlled with the left and right D-pad directions. Each unit is 1 CPU cycle, which will move by 3 dots at a time.
- A 2001 #: 1 indicates that rendering is disabled at the target location, while 0 does not.
- B Fast: This simply hints that B can be held to modify the D-pad directions to work by being held rather than pressed. It does not indicate any state.

(Write OAM)
- Se 2003 #: 1 indicates that OAM address is set to 0 during vblank, while 0 indicates OAM address is not written. This is controlled with the select button.
- St 4014 #: 1 indicates that OAM DMA is performed during vblank, while 0 indicates it is not. This is controlled with the start button.


Controls:
(Joypad 1)
- Up/Down: Decrement/increment scanline delay.
- Left/Right: Decrement/increment cycle delay.
- B: When held, D-pad directions are processed while held. Otherwise, they are processed only when pressed.
- A: Toggles whether rendering is disabled.
- Select: Toggles whether OAM address is set during vblank. If so, it is set to 0.
- Start: Toggles whether OAM DMA is performed during vblank. If not, the test falls back on DMC DMA to assist with PPU/NMI sync, which does not work reliably.

(Joypad 2):
- A: Performs a 4-frame sequence at the target location to test the result of test case #1 more precisely. This test involves disabling OAM DMA and should be performed near the bottom of the screen to reduce any OAM decay that may occur. Results can vary based on frame parity because of the 1 dot of jitter. During each of the 4 frames, the OAM address write always occurs, while OAM DMA and the PPU mask write to disable rendering go as follows:
+--Frame 1: OAM DMA, but no PPU mask write. This puts sprites into a good state for rendering during frame 2.
+--Frame 2: OAM DMA and PPU mask write. This should cause the glitch to occur on frame 3.
+--Frame 3: OAM DMA, but no PPU mask write. Despite the OAM DMA occurring, the glitch should occur on this frame.
+--Frame 4: No OAM DMA and no PPU mask write. This frame shows whether the glitch in frame 3 is persistent.
+--Frame 5: The test is complete, and the settings from frame 4 will persist until manually changed or another test is run.
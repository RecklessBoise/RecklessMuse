## RecklessMuse 1.0.1

Bug fixes:

- **Arpeggiator / sequencer no longer keep playing after the MIDI notes end.** Factory "Sequence" presets no longer have Latch enabled, and the built-in sequencer now plays only while a key is held (or when LATCH / HOLD is on).
- **Transport stop and All Notes Off** now stop the arpeggiator and the sequencer, even when latched.
- **Mono and unison sounds are centred.** Pan spread used to push the single mono voice to the left; it now only spreads polyphonic voices.
- **Safety net:** a voice or the output stage recovers automatically instead of staying silent if an invalid value ever appears.
- New regression tests (notes stop, sequencer gating, transport stop, stereo balance) and a DAW-like stress test.

Download `RecklessMuse-macOS.zip`, unzip, then in Terminal: `cd ~/Downloads/RecklessMuse && ./install.sh`

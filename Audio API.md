VDP Enhanced Audio API
======================

The Agon-VDP supports audio commands via `VDU 23,0,&85`, followed by further data to give the exact command required.

The first byte _usually_ indicates the channel, and the second byte the command.

Parameters vary in number and meaning depending on the command.  Some parameters are bytes, some are 16-bit values sent as two bytes in little-endian order.  In the documentation below, 16-bit values are shown as `value1; value2;`, as per BBC BASIC syntax, and bytes as `value1, value2, value3, ...`.

Examples are given in BBC BASIC.

A common source of errors when sending commands to the VDP from BASIC via VDU commands is to forget to use a `;` after a number to indicate a 16-bit value should be sent.  If you see unexpected behaviour from your BASIC code that is the most likely source of the problem.

When a command is processed the VDP may send a message back to MOS with the status of that command.  (Generally a return value of `1` indicates success, and `0` failure, but there are some exceptions to this, most notably the Status command.  Not all audio comments return a status.)  When the MOS receives an audio command status value it will set the audio bit of the VDP protocol flags to indicate that an audio message has been received.  It also sets two [system variable values](https://github.com/breakintoprogram/agon-docs/wiki/MOS-API#system-variables), `sysvar_audioChannel` and `sysvar_audioSuccess` with the results of the command.  `sysvar_audioChannel` is the channel number, and `sysvar_audioSuccess` provides the status of that command.  These values can be read using a `mos_sysvars` API call, or an [OSBYTE call from BASIC as documented here](https://github.com/breakintoprogram/agon-docs/wiki/BBC-BASIC-for-Agon#accessing-the-mos-system-variables).

It should be noted that VDP protocol flags are not automatically cleared by MOS, so it is the responsibility of the application to clear the audio bit of the VDP protocol flags before sending a command.  At present there is no way to do this directly from BBC BASIC.

The full audio command set supported by the Agon-VDP is as follows:

## Command 0: Play note

`VDU 23, 0, &85, channel, 0, volume, frequency; duration;`

If the channel is not already busy, this command will play a note on the specified channel.

The volume is a value from 0 to 127, where 0 is silent and 127 is full volume.  Values above 127 will be treated as 127.

The frequency is a 16-bit value specifying in Hz the frequency of the note to be played.

The duration is a 16-bit value specifying in milliseconds the duration of the note to be played.  Specifying a value of -1 (65535) will cause the note to be played indefinitely until the channel is silenced by setting its volume to zero.

Returns 1 if the note was successfully queued for playback, or 0 if the channel was already in use.

NB attempting to play a note on an inactive channel will return 1, but the note will not be played.  The channel must be active (see the status command) before a note can be played on it.

## Command 1: Status

`VDU 23, 0, &85, channel, 1`

Returns a bit mask indicating the status of the specified channel, or 255 if the channel is not valid, or has been disabled.  The bit mask is as follows:

| Bit | Name | Meaning |
| --- | ---- | ------- |
| 0 | Active | When set this indicates the channel is in use (has an active waveform) |
| 1 | Playing | Indicates the channel is actively playing a note, and thus will reject calls to play a new note |
| 2 | Indefinite | Set if the channel is playing an indefinite duration note |
| 3 | Has Volume Envelope | Set if the channel has a volume envelope |
| 4 | Has Frequency Envelope | Set if the channel has a frequency envelope |

Bits 5-7 are reserved for future use and, for enabled channels, will always be zero.

For example, calling this command on a channel that is playing a note with no envelopes set will return a value of 3 (00000011).  A channel with a volume envelope set where playback is in the "release" phase of the envelope, and thus is free to play a new note, will return a value of 9 (00001001).  A completely silent channel with no envelopes set returns 0, whereas a silent channel that has a frequency envelope set will return 16 (00010000).


## Command 2: Set volume

`VDU 23, 0, &85, channel, 2, volume`

Sets the volume of the specified channel.  The volume is a value from 0 to 127, where 0 is silent and 127 is full volume.  Values above 127 will be treated as 127.

Using this command provides more direct control over a channel than the play note command.  It can be used to adjust the volume of a channel that is already playing a note.

Setting a non-zero volume level on a channel that is silent and not playing a note will cause the channel to play a note at the specified volume for an indefinite duration.  The note will be played at the frequency that was last set on the channel.

Setting the volume to zero will silence the channel.  This can curtail the playback of a note that is currently playing, or silence a channel that is playing an indefinite duration note.

Does not currently return a status.

### Special case: Volume envelope

If the channel is actively playing a note with a volume envelope set on the channel then, rather than silencing the channel immediately, the volume envelope will be allowed to complete (enter its "release" phase) before the channel is silenced.  A channel with volume envelope in its "release" phase is not considered to be actively playing a note, so during this phase setting the volume to zero will immediately silence the channel.

Calling this command on a channel that has an active volume envelope adjusts the base volume level being applied to that envelope.  For more information see the documentation for the volume envelope command.


## Command 3: Set frequency

`VDU 23, 0, &85, channel, 3, frequency;`

Sets the frequency of the specified channel.  The frequency is a 16-bit value specifying in Hz the frequency of the note to be played.

Using this command provides more direct control over a channel than the play note command.  It can be used to adjust the frequency of a channel that is already playing a note.

Does not currently return a status.

### Special case: Frequency envelope

If the channel has a frequency envelope set then adjusting the frequency value whilst a note is playing will adjust the base frequency level being applied to that envelope.  For more information see the documentation for the frequency envelope command.


## Command 4: Set waveform

`VDU 23, 0, &85, channel, 4, waveformOrSample`

Sets the waveform type for a channel to use.  The `waveformOrSample` value is a single byte treated as a signed value.

Using a negative value for the waveform indicates that a sample should be used instead.  For more information see the documentation for the sample command.

By default a channel is set to use waveform 0 (square wave).

Valid waveform values are as follows:

| Value | Waveform |
| ----- | -------- |
| 0 | Square wave |
| 1 | Triangle wave |
| 2 | Sawtooth wave |
| 3 | Sine wave |
| 4 | Noise (simple white noise with no frequency support) |
| 5 | VIC Noise (emulates a VIC6561; supports frequency) |

Channels with a sample waveform set do not support frequency adjustment.

A channel with a sample set will ignore the frequency value set on the channel or in the play note command.  Samples will also automatically repeat if the note played is longer than the sample itself, or it is played for an indefinite duration.

Does not currently return a status.


## Command 5: Sample management

`VDU 23, 0, &85, sample, 5, sampleCommand, [parameters]`

These commands are used to manage samples on the VDP which can be assigned to a channel as a waveform for playback.

As they are intended to manage samples, the `sample` value is a sample number rather than a channel number.  This sample number is a signed 8-bit value and must be a negative number to be considered valid.  As a result, a total of 128 samples can be supported, from -1 to -128.  The sample number is used to identify the sample when assigning it to a channel using the "set waveform" command.

Sample commands will return 1 if the command is processed successfully, or 0 to indicate a failure.

### Command 5, 0: Load sample

`VDU 23, 0, &85, sample, 5, 0, length; lengthHighByte, <sampleData>`

This command is used to transfer a sample over to the VDP for later playback.

The VDP supports 8-bit PCM samples at 16kHz only.  The sample data is sent as a series of bytes of the given length.

The length provided to this command is a 24-bit value, sent in little-endian order.  It has been documented above as a 16-bit value followed by an 8-bit value for simplicity and/or compatibility with BBC BASIC.  Using a 24-bit length gives us the flexibility to support samples larger than 64kB.

Specifying a sample number that has already been used will overwrite the existing sample with the new sample data.  If the existing sample had been assigned to a channel for playback then playback will be stopped and the channel will be set to use the default waveform (square wave).

A simple example of how to send a sample to the VDP is as follows:

```
 10 REM Load a sample into the VDP
 20 infile=OPENIN "sample.raw"
 30 length=EXT#infile
 40 REM Send sample info to the VDP for sample -1
 50 VDU 23, 0, &85, -1, 5, 0, length MOD 256, length DIV 256, length DIV 65536
 60 REM Send sample data to the VDP
 70 REPEAT
 80   VDU BGET#infile
 90 UNTIL EOF#infile
100 CLOSE#infile
110 REM Set channel 1 to use sample -1
120 VDU 23, 0, &85, 1, 4, -1
130 REM Play sample on channel 1
140 SOUND 1, -10, 10, length / 50
```

NB This example can be very slow as it sends the sample data byte-by-byte, taking just over 1s to send 2kb of data.  During this time your computer will be unresponsive, and it is not possible to output to screen any kind of progress as any such `PRINT` command will be interpretted as part of the sample data.  Unfortunately at present there is no way to send data in bulk to the VDP from BBC BASIC, or to read chunks of files into memory in one go.  For faster transfer of sample data you will need to write a program in assembly language and make use of file access APIs from MOS and the RST #18 vector to send larger chunks of data to the VDP.

As noted above, this command will return 1 on success or 0 for failure.  In the event of a failure the VDP will ignore and discard the sample data being sent to it.

Failure may occur if an invalid sample number was given, or if the VDP could not allocate sufficient memory to store the sample.

### Command 5, 1: Clear sample

`VDU 23, 0, &85, sample, 5, 1`

Removes the given sample number from the VDP.  If the sample had been assigned to a channel for playback then playback will be stopped and the channel will be set to use the default waveform (square wave).

This command will return 1 on success or 0 for failure.


## Command 6: Volume envelope

`VDU 23, 0, &85, channel, 6, type, [parameters]`

These commands are used to set a volume envelope on a channel.

When a volume envelope has been set on a channel then the volume level of the channel will be adjusted over time according to the envelope.  This can be used to create a variety of effects.

The volume level given either by the play note command or the set volume command is used as the base/target volume level for the envelope.  The envelope will then adjust the volume level from this base level.

When a volume envelope has been applied to a channel it will be applied to all notes played on that channel until the envelope is changed or removed, just like it would on a synthesiser.

Volume envelopes have the concept of a "release" phase.  When a channel playing a note is in the "release" phase the channel may still be making a noise, but it is no longer considered to be busy and so it is free to be used for another note.

It should be noted that volume envelopes are compatible with sample playback.  If a channel is set to play a sample and has a volume envelope set then it will be applied to the sample playback.  As the "release" phase of a volume envelope is not considered to be part of the duration of a note though, this means that some attention needs to be paid to the duration of notes when playing samples.  You are advised to either use envelopes without a release phase (i.e. with a release duration of `0`), or to subtract the duration of the release phase from the duration of the note when playing a sample.  Giving the complete duration of the sample when playing a note would otherwise result in the sample starting to repeat when the "release" phase begins.

Volume envelope commands do not currently return a status.

The following envelope types are supported:

### Type 0: None

`VDU 23, 0, &85, channel, 6, 0`

Disables the volume envelope on the given channel.

### Type 1: ADSR

`VDU 23, 0, &85, channel, 6, 1, attack; decay; sustain, release;`

Sets the volume envelope on the given channel to use an ADSR envelope.

Values for attack, decay and release are all 16-bit numbers giving a time for that phase in milliseconds.  The sustain value is an 8-bit number giving the level to sustain at, which is a modifier to the target level.

This is a common type of envelope used in many synthesizers.  It is made up of four stages:

* Attack - The volume level rises from 0 to the target volume level over the given number of milliseconds.
* Decay - The volume level falls from the sustain level to the release level over the given number of milliseconds.
* Sustain - The volume level is held at the sustain level until the note is released.
* Release - The volume level falls from the sustain level to 0 over the given number of milliseconds.

The volume level during the sustain phase is calculated using the simple formula:
```
sustainLevel = (targetLevel * sustain) / 127
```

When playing a note on a channel that has an ADSR envelope set the note duration is handled slightly differently.  A note will always play for a minimum of the Attack and Decay phases.  Calling "play note" with a duration of zero will therefore play a note for the Attack and Decay phases.  The sustain phase will only be entered if the note duration is longer than the Attack and Decay phases.  The Release phase will be entered either when the note duration has elapsed or the Attack and Decay phases have completed, whichever is longer.

An example of setting an ADSR envelope on a channel is as follows:

```
 10 REM Set channel 1 to use an ADSR envelope
 20 VDU 23, 0, &85, 1, 6, 1, 400; 100; 100, 2000;
 30 REM Play a note on channel 1
 40 VDU 23, 0, &85, 1, 0, 60, 440; 1500;
```

In this example we are defining an ADSR envelope, and then playing a note.  We are using the `VDU`` command to play the note, rather than the `SOUND` command just for clarity.

The ADSR envelope set here has an attack phase of 400ms, a decay phase of 100ms, a relative sustain level of 100 and a release phase of 2000ms.  The note we are playing is an "A" (440Hz) with a duration of 1500ms and a base/target volume of 60.

What you hear when this code runs is a note that starts off quiet and over 0.4s it gradually gets louder until it reaches the target volume of 60 - this is the attack phase.  The note quickly over the next 0.1s reduces its volume down to the sustain level of 47 (calculated using (60 * 100) / 127) - this is the decay phase.  The note then holds at this volume level for a further 1s, at which point a total of 1500ms has elapsed (the note duration we sent to the play note command) - this is the sustain phase.  Finally the note gradually reduces its volume over 2s until it is silent - this is the release phase.

As noted elsewhere, during this release phase the channel is considered to be free and so can be used to play another note.  Playing another note will interrupt the release phase and the new note will immediately play.


## Command 7: Frequency envelope

`VDU 23, 0, &85, channel, 7, type, [parameters]`

These commands are used to set a frequency envelope on a channel.

When a frequency envelope has been set on a channel then the frequency of the channel will be adjusted over time according to the envelope.  This can be used to create a variety of effects.

The frequency given either by the play note command or the set frequency command is used as the base/target frequency for the envelope.  The envelope will then adjust the frequency from this base frequency.

When a frequency envelope has been applied to a channel it will be applied to all notes played on that channel until the envelope is changed or removed, just like it would on a synthesiser.

Frequency envelope commands do not currently return a status.

The following envelope types are supported:

### Type 0: None

`VDU 23, 0, &85, channel, 7, 0`

Disables the frequency envelope on the given channel.

### Type 1: Stepped frequency envelope

`VDU 23, 0, &85, channel, 7, 1, phaseCount, controlByte, stepLength; [phase1Adjustment; phase1NumberOfSteps; phase2Adjustment; phase2NumberOfSteps; ...]`

The design of the stepped frequency envelope is based on the pitch envelope provided as part of the `ENVELOPE` command in BBC BASIC.

The number of phases is set by the `phaseCount` parameter.

The `controlByte` parameter is a bit mask that controls how the envelope will operate.  The following bits are defined:

| Bit | Name | Meaning |
| --- | ---- | ------- |
| 0 | Repeats | If set then the envelope will repeat indefinitely whilst the note continues to play.  If not set then the envelope will only be applied once. |
| 1 | Cumulative | If Repeat has been set then the envelope will apply cumulatively on each repeat of the envelope.  If it is clear then the base frequency is used on each iteration thru the envelope. |
| 2 | Restrict | If set the envelope frequency generator will be restricted to returning values the range 0-65535.  If the frequency goes outside this range then the envelope will set a zero frequency.  When clear the calculated frequency is permitted to go outside this range, and the actual frequency set will be treated as `MOD 65536`. |

Currently no other bits are defined, and are reserved for future use, so should be set to zero.

The `stepLength` parameter is a 16-bit number giving the length of each step in milliseconds.

Each phase is defined by two parameters.  Firstly there is an adjustment value, given as a 16-bit number.  This is the amount that the frequency will be adjusted by for each step in the phase.  Secondly there is a number of steps for the phase, given as a 16-bit number.

(In comparison, the BBC Micro's `ENVELOPE` command has a fixed three phase design, all time values were given in centiseconds, and would automatically repeat unless the top bit of the step length value was set.  The pitch envelope could not be applied cumulatively.  Two phase envelopes were achieved by setting the number of steps in the second phase to zero.)

The following example sets a stepped frequency envelope on channel 1:

```
 10 REM Set channel 1 to use a stepped frequency envelope
 20 VDU 23, 0, &85, 1, 7, 1, 2, 1, 30; 40; 6; -30; 4;
 30 REM Play a note on channel 1
 40 SOUND 1, -10, 100, 30
```

This will play a simple "siren" like sound that goes up and down, which repeats for the duration of the note (1.5s).

The envelope described here has two phases, and the steps are each set to be 30ms long.  The first phase has an adjustment of 40, and 6 steps.  The second phase has an adjustment of -30, and 4 steps.  The envelope will therefore start at the base frequency, and then increase by 40Hz every 30ms for 6 steps.  It will then decrease by 30Hz every 30ms for 4 steps.  The envelope will then repeat from the start.

We can change the control byte value to 3, as follows for a different effect:
```
 20 VDU 23, 0, &85, 1, 7, 1, 2, 3, 30; 40; 6; -30; 4;
```

You may have noticed that with the envelope as defined we are raising by a total of 240Hz, and then lowering by a total of 120Hz...

With this new control byte value, on each successive loop through the frequency envelope the note will get higher and higher.


## Command 8: Enable Channel

`VDU 23, 0, &85, channel, 8`

This command will enable the given channel.

By default, an Agon Light will start up with three channels enabled, numbered 0-2.  This command can be used to enable additional channels, up to 32 of them.

Please note that channels do not have to be enabled in order.  For example, you can enable channel 31 without having enabled channels 0-30 first.

Attempting to enable an already enabled channel will have no effect.

It should be noted that enabling a channel has a cost, and so it is recommended that you only enable channels that you are actually using.  The performance of the VDP with a large number of channels enabled has not been tested.

This command does not currently return a value.


## Command 9: Disable Channel

`VDU 23, 0, &85, channel, 9`

This command will disable the given channel.

The channel is immediately disabled.  Any sound that may have been playing is instantly stopped.

Attempting to disable an already disabled channel will have no effect.

Re-enabling a disabled channel will give you a fresh channel, with none of the previous settings for that channel being retained.

This command does not currently return a value.


## Command 10: Reset Channel

`VDU 23, 0, &85, channel, 10`

This command will reset the given channel.

This is equivalent to disabling and then enabling the channel.

As with the disable command, any sound that may have been playing is instantly stopped.

Following a reset, the channel will be in the same state as it was when it was first enabled.  This includes the frequency, volume, and envelope settings.  Resetting a channel is a fast way to stop the sound on a channel, and clear any envelopes that may have been set.

This command does not currently return a value.

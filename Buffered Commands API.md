VDP Buffered Commands API
=========================

The VDP Buffered Commands API is a low-level API that allows for the creation of buffers on the VDP, and for commands to be sent to the VDP to be stored in those buffers.  These commands can then be executed later.

Through the use of the APIs, it is possible to both send commands to the VDP in a "packetised" form, as well as to have "functions" or "stored procedures" that can be saved on the VDP and executed later.

These commands are collected under `VDU 23, 0, &A0, bufferId; command, [<arguments>]`.

All commands must specify a buffer ID as a 16-bit integer.  As with all other VDP commands, these 16-bit values are sent as two bytes in little-endian order, and are documented as per BBC BASIC syntax, such as `bufferId;`.

Examples below are given in BBC BASIC.

A common source of errors when sending commands to the VDP from BASIC via VDU statements is to forget to use a `;` after a number to indicate a 16-bit value should be sent.  If you see unexpected behaviour from your BASIC code that is the most likely source of the problem.

At this time the VDP Buffered Commands API does not send any messages back to MOS to indicate the status of a command.  This will likely change in the future, but that may require changes to agon-mos to support it.

## Command 0: Write to a buffer

`VDU 23, 0 &A0, bufferId; 0, length; <buffer-data>`

This command is used to store a sequence of bytes in a buffer on the VDP.  It is primarily intended to be used to store a sequence of commands to be executed later.  The buffer is not executed by this command.

The `bufferId` is a 16-bit integer that identifies the buffer to write to.  Writing to the same buffer ID multiple times will add new streams to that buffer.  This allows a buffer to be built up over time, essentially allowing for a command to be sent across to the VDP in multiple separate packets.  This can also allow for the total size of a buffer to exceed 16-bits.

As writing to a single buffer ID is cumulative with this command, care should be taken to ensure that the buffer is cleared out before writing to it.

This command may be used, for instance, to build up a command that writes a large amount of data to the VDP, such as sending a large sprite or a sound sample.  In between each packet of data sent to a buffer, the user can then perform other operations, such as updating the screen to indicate progress.  This allows for long-running operations to be performed without blocking the screen, and larger amounts of data to be transferred over to the VDP than may otherwise be practical given the limitations of the eZ80.

## Command 1: Call a buffer

`VDU 23, 0 &A0, bufferId; 1`

This command will attempt to execute all of the commands stored in the buffer with the given ID.  If the buffer does not exist, or is empty, then this command will do nothing.

Essentially, this command passes the contents of the buffer to the VDP's VDU command processor system, and executes them as if they were sent directly to the VDP.

As noted against command 0, it is possible to build up a buffer over time by sending across multiple commands to write to the same buffer ID.

Care should be taken when using this command within a buffer, as it is possible to create an infinite loop.  For instance, if a buffer contains a command to call itself, then this will result in an infinite loop.  This will cause the VDP to hang, and the only way to recover from this is to reset the VDP.

A `bufferId` of -1 (65535) will be ignored, as it has a special meaning for some of the other commands below.

## Command 2: Clear a buffer

`VDU 23, 0 &A0, bufferId; 2`

This command will clear the buffer with the given ID.  If the buffer does not exist then this command will do nothing.

Please note that this clears out all of the streams sent to a buffer via command 0, not just the last one.  i.e. if you have built up a buffer over time by sending multiple commands to write to the same buffer ID, this command will clear out all of those commands.

Calling this command with a `bufferId` value of -1 (65535) will clear out all buffers.

## Command 3: Create a buffer

`VDU 23, 0 &A0, bufferId; 3, length;`

This command will create a new writable buffer with the given ID.  If a buffer with the given ID already exists then this command will do nothing.

The `length` parameter is a 16-bit integer that specifies the maximum size of the buffer.  This is the maximum number of bytes that can be stored in the buffer.  If the buffer is full then no more data can be written to it, and subsequent writes will be ignored.

This command is primarily intended for use to create a buffer that can be used to capture output, or to store data that can be used for other commands.

A `bufferId` of -1 (65535) and 0 will be ignored, as these values have special meanings for writable buffers.

## Command 4: Set output stream to a buffer

`VDU 23, 0 &A0, bufferId; 4`

Sets then current output stream to the buffer with the given ID.  With two exceptions, noted below, this needs to be a writable buffer created with command 3.  If the buffer does not exist, or is not writable, then this command will do nothing.

Following this command, any subsequent VDU commands that send response packets will have those packets written to the specified output buffer.  This allows the user to capture the response packets from a command sent to the VDP.

By default, the output stream (for the main VDU command processor) is the communications channel from the VDP to MOS running on the eZ80.

Passing a buffer ID of -1 (65535) to this command will remove/detach the output buffer.  From that point onwards, any subsequent VDU commands that send response packets will have those responses discarded/ignored.

Passing a buffer ID of 0 to this command will set the output buffer back to its original value for the current command stream.  Typically that will be the communications channel from the VDP to MOS running on the eZ80.

When used inside a buffered command sequence, this command will only affect the output stream for that sequence of commands, and any other buffered command sequences that are called from within that sequence.  Once the buffered command sequence has completed, the output stream will effectively be reset to its original value.

It is strongly recommended to only use this command from within a buffered command sequence.  Whilst it is possible to use this command from within a normal VDU command sequence, it is not recommended as it may cause unexpected behaviour.

## Command 5: Adjust buffer contents

`VDU 23, 0, &A0, bufferId; 5, operation, offset; [count;] [operand]`

This command will adjust the contents of a buffer, at a given offset.  The exact nature of the adjustment will depend on the operation used.

The basic set of adjustment operations are as follows:

| Operation | Description |
| --------- | ----------- |
| 0 | NOT |
| 1 | Negate |
| 2 | Set value |
| 3 | Add |
| 4 | Add with carry |
| 5 | AND |
| 6 | OR |
| 7 | XOR |

All of these operations will modify a byte found at the given offset in the buffer.  The only exception to that is the "Add with carry" operation, which will also store the "carry" value in the byte at the _next_ offset.  With the exception of NOT and Negate, each command requires an operand value to be specified.

To flip the bits of a byte at offset 12 in buffer 3, you would need to use the NOT operation, and so the following command would be used:
```
VDU 23, 0, &A0, 3; 5, 0, 12;
```

To add 42 to the byte at offset 12 in buffer 3, you would need to use the Add operation, and so the following command would be used:
```
VDU 23, 0, &A0, 3; 5, 3, 12; 42
```

When using add with carry, the carry value is stored in the byte at the next offset.  So to add 42 to the byte at offset 12 in buffer 3, and store the carry value in the byte at offset 13, you would need to use the Add with carry operation, and so the following command would be used:
```
VDU 23, 0, &A0, 3; 5, 4, 12; 42
```

### Advanced operations

Whilst these operations are useful, they are not particularly powerful as they only operate one one byte at a time, with a fixed operand value, and potentially cannot reach all bytes in a buffer.  To address this, the API supports a number of advanced operations.

The operation value used is an 8-bit value that can have bits set to modify the behaviour of the operation.  The following bits are defined:

| Bit | Description |
| --- | ----------- |
| 0x10 | Offsets are given as 24-bit values, not 16-bit values |
| 0x20 | Operand is a buffer-fetched value (buffer ID and an offset) |
| 0x40 | Multiple target values should be adjusted |
| 0x80 | Multiple operand values should be used |

These bits can be combined together to modify the behaviour of the operation.

Fundamentally, this command adjusts values of a buffer at a given offset one byte at a time.  When either of the "multiple" variants are used, a 16-bit `count` must be provided to indicate how many bytes should be altered.

Support for 24-bit offsets are provided to allow for buffers larger than 64kb to be adjusted.  When this mode is used, all offsets given as part of the command must be 24-bit values sent as 3 bytes in little-endian order.

The "buffer-fetched value" mode allows for the operand value to be fetched from a buffer.  The operand sent as part of the command in this case is a pair of 16-bit values giving the buffer ID and offset to indicate where the actual operand value should be fetched from.  If the 24-bit offset mode is used, then the operand value is a 24-bit value sent as 3 bytes in little-endian order.

The "multiple target values" mode allows for multiple bytes to be adjusted at once.  When this mode is used, the `count` value must be provided to indicate how many bytes should be adjusted.  Unless the "multiple operand values" mode is also used, the operand value is used for all bytes adjusted.

The "multiple operand values" mode allows for multiple operand values to be used.  When this mode is used, the `count` value must be provided to indicate how many operand values should be used.  This can allow, for instance, to add together several bytes in a buffer.  When this mode is used in conjunction with the "multiple target values" mode, the number of operand values must match the number of target values, and the operation happens one byte at a time.

Some examples of advanced operations are as follows:

Flip the bits of 7 bytes in buffer 3 starting at offset 12:
```
VDU 23, 0, &A0, 3; 5, 0x40, 12; 7;
```
This uses operation 0 (NOT) with the "multiple target values" modifier (0x40).

Add 42 to each of the 7 bytes in buffer 3 starting at offset 12:
```
VDU 23, 0, &A0, 3; 5, 0x43, 12; 7; 42
```

Set the value in buffer 3 at offset 12 to the sum of the five values 1, 2, 3, 4, 5:
```
VDU 23, 0, &A0, 3; 5, 2, 12; 0  : REM clear out the value at offset 12 (set it to 0)
VDU 23, 0, &A0, 3; 5, 0x83, 12; 5; 1, 2, 3, 4, 5
```

AND together 7 bytes in buffer 3 starting at offset 12 with the 7 bytes in buffer 4 starting at offset 42:
```
VDU 23, 0, &A0, 3; 5, 0xE5, 12; 7; 4; 42;
```

As we are working on a little-endian system, integers longer than one byte are sent with their least significant byte first.  This means that the add with carry operation can be used to add together integers of any size, so long as they are the same size.  To do this, both the "multiple target values" and "multiple operand values" modes must be used.

The following commands will add together a 16-bit, 24-bit, 32-bit, and 40-bit integers, all targetting the value stored in buffer 3 starting at offset 12, and all using the operand value of 42:
```
VDU 23, 0, &A0, 3; 5, 0xC4, 12; 2; 42;  : REM 2 bytes; a 16-bit integer
VDU 23, 0, &A0, 3; 5, 0xC4, 12; 3; 42; 0  : REM 3 bytes; a 24-bit integer
VDU 23, 0, &A0, 3; 5, 0xC4, 12; 4; 42; 0;  : REM 4 bytes; a 32-bit integer
VDU 23, 0, &A0, 3; 5, 0xC4, 12; 5; 42; 0; 0  : REM 5 bytes; a 40-bit integer
```
Take note of how the operand value is padded out with zeros to match the size of the target value.  `42;` is used as a base to send a 16-bit value, with zeros added of either 8-bit or 16-bits to pad it out to the required size.  The "carry" value will be stored at the next offset in the target buffer after the complete target value.  So for a 16-bit value, the carry will be stored at offset 14, for a 24-bit value it will be stored at offset 15, and so on.


## Command 6: Conditionally call a buffer

`VDU 23, 0, &A0, bufferId; 6, operation, checkBufferId; checkOffset; [operand]`

This command will conditionally call a buffer if the condition operation passes.  This command works in a similar manner to the "Adjust buffer contents" command.

The basic set of condition operations are as follows:

| Operation | Description |
| --------- | ----------- |
| 0 | Exists (value is non-zero) |
| 1 | Not exists (value is zero) |
| 2 | Equal |
| 3 | Not equal |
| 4 | Less than |
| 5 | Greater than |
| 6 | Less than or equal |
| 7 | Greater than or equal |
| 8 | AND |
| 9 | OR |

The value that is being checked is fetched from the specified check buffer ID and offset.  With the exception of "Exists" and "Not exists", each command requires an operand value to be specified to check against.

The operation value used is an 8-bit value that can have bits set to modify the behaviour of the operation.  The following bits are defined:

| Bit | Description |
| --- | ----------- |
| 0x10 | Offsets are given as 24-bit values, not 16-bit values |
| 0x20 | Operand is a buffer-fetched value (buffer ID and an offset) |

These modifiers can be combined together to modify the behaviour of the operation.

At this time, unlike with the "adjust" command, multiple target values and multiple operand values are not supported.  All comparisons are therefore only conducted on single 8-bit values.  (If comparisons of 16-bit values are required, multiple calls can be combined.)  Support for them may be added in the future.

The `AND` and `OR` operations are logical operations, and so the operand value is used as a boolean value.  Any non-zero value is considered to be true, and zero is considered to be false.  These operations therefore are most useful when used with buffer-fetched operand values (operations &28, &29, &38 and &39).

Some examples of condition operations are as follows:

Call buffer 7 if the value in buffer 12 at offset 5 exists (is non-zero):
```
VDU 23, 0, &A0, 7; 6, 0, 12; 5;
```

Call buffer 8 if the value in buffer 12 at offset 5 does not exist (is zero):
```
VDU 23, 0, &A0, 8; 6, 1, 12; 5;
```

Combining the above two examples is effectively equivalent to "if the value exists, call buffer 7, otherwise call buffer 8":
```
VDU 23, 0, &A0, 7; 6, 0, 12; 5;
VDU 23, 0, &A0, 8; 6, 1, 12; 5;
```

Call buffer 3 if the value in buffer 4 at offset 12 is equal to 42:
```
VDU 23, 0, &A0, 3; 6, 2, 4; 12; 42
```

Call buffer 5 if the value in buffer 2 at offset 7 is less than the value in buffer 2 at offset 8:
```
VDU 23, 0, &A0, 5; 6, &24, 2; 7; 2; 8;
```

## Examples

What follows are some examples of how the VDP Buffered Commands API can be used to perform various tasks.

### Loading a sample

Sound sample files can be large, and so it is not practical to send them over to the VDP in a single packet.  Even with optimised machine code, it could take several seconds to send a single sample over to the VDP.  This would block the screen, and make it impossible to show progress to the user.  Using the VDP Buffered Commands API we can send a sample over to the VDP in multiple packets.

The following example will load a sound sample from a file called `sound.bin` and send it over to the VDP.  Lines 10-50 prepare things, opening up the file and getting its length.  Lines 60-90 clear out buffer 1 and then store in buffer 1 the beginning of the command to load a sample.  The loop from lines 110 to 190 sends the sample one block at a time, adding the sample data to buffer 1.  Finally, lines 200-230 execute buffer 1 to load the sample, and then play it.

```
 10 blockSize% = 1000
 20 infile% = OPENIN "sound.bin"
 30 length% = EXT#infile%
 40 PRINT "Sound sample length: "; length%; "bytes"
 50 remaining% = length%
 60 VDU 23, 0, &A0, 1; 2       : REM Clear out buffer 1
 70 VDU 23, 0, &A0, 1; 0, 9;   : REM Send next 9 bytes to buffer 1
 80 REM Load sample into sample ID -1 (a 9 byte command)
 90 VDU 23, 0, &85, -1, 5, 0, length% AND &FF, (length% DIV 256) AND &FF, (length% DIV 65536) AND &FF
100 PRINT "Loading sample";
110 REPEAT
120   IF remaining% < blockSize% THEN blockSize% = remaining%
130   remaining% = remaining% - blockSize%
140   PRINT ".";       : REM Show progress
150   VDU 23, 0, &A0, 1; 0, blockSize%; : REM Send next blockSize% bytes to buffer 1
160   FOR i% = 1 TO blockSize%
170     VDU BGET#infile%
180   NEXT
190 UNTIL remaining% = 0
200 CLOSE #infile%
210 VDU 23, 0, &A0, 1; 1       : REM Execute buffer 1 to load the sample
220 VDU 23, 0, &85, 1, 4, -1   : REM Set audio channel 1 to use sample -1
230 VDU 23, 0, &85, 1, 0, 100, 750; length% DIV 16;  : REM Play sample on channel 1
```

Please note that the BASIC code here is not fast, owing to the fact that it has to read the sample file one byte at a time.  This is because BBC BASIC does not provide a way to read a chunk of a file at once.  This is not a limitation of the VDP Buffered Commands API, but rather of BBC BASIC.

This can be optimised by writing a small machine code routine to read a chunk of a file at once, and then calling that from BASIC.  This is left as an exercise for the reader.

Whilst this example illustrates loading a sample, it is easily adaptable to loading in a bitmap.

### Repeating a command

This example will print out "Hello " 10 times.

This is admitedly a contrived example, as there is an obvious way to achieve what this code does in plain BASIC, but it is intended to illustrate the API.  The technique used here can be fairly easily adapted to more complex scenarios.

This example uses three buffers.  The first buffer is used to print out a string.  The second buffer is used to store a value that will be used to control how many times the string printing buffer is called.  The third buffer is used to call the string printing buffer the required number of times, and is gradually built up.

```
 10 REM Clear the buffers we're going to use (1-3)
 20 VDU 23, 0, &A0, 1; 2       : REM Clear out buffer 1
 30 VDU 23, 0, &A0, 2; 2       : REM Clear out buffer 2
 40 VDU 23, 0, &A0, 3; 2       : REM Clear out buffer 3
 50 VDU 23, 0, &A0, 1; 0, 6;   : REM Send the next 6 bytes to buffer 1
 60 PRINT "Hello ";            : REM The print will be captured into buffer 1
 70 REM Create a writable buffer with ID 2, 1 byte long for our iteration counter
 80 VDU 23, 0, &A0, 2; 3, 1;
 90 REM set our iteration counter to 10 - adjust (5), set value (2), offset 0, value 10
100 VDU 23, 0, &A0, 2; 5, 2, 0; 10
110 REM gradually build up our command buffer in buffer 3
120 VDU 23, 0, &A0, 3; 0, 6;   : REM 6 bytes for the following "call" command
130 VDU 23, 0, &A0, 1; 1       : REM Call buffer 1 to print "Hello "
140 VDU 23, 0, &A0, 3; 0, 10;  : REM 10 bytes for the following "adjust" command
150 REM Decrement the iteration counter in buffer 2
160 VDU 23, 0, &A0, 2; 5, 3, 0; -1
170 VDU 23, 0, &A0, 3; 0, 11;  : REM 11 bytes for the following "conditional call" command
180 REM If the iteration counter is not zero, then call buffer 3 again
190 VDU 23, 0, &A0, 3; 6, 0, 2; 0;
200 REM That's all the commands for buffer 3
210 REM Now call buffer 3 to execute those commands
220 VDU 23, 0, &A0, 3; 1
```

It should be noted that after this code has been run the iteration counter in buffer ID 2 will have been reduced to zero.  Calling buffer 3 again at that point will result in the counter looping around to 255 on its first decrement, and then counting down from there, so you will see the loop run 256 times.

To avoid this, the iteration counter in buffer 2 should be reset to the desired value before calling buffer 3 again.

As a very simple example, you can imagine replacing buffer 1 with a buffer that draws something to the screen.  Those drawing calls could use relative positioning, allowing for repeated patterns to be drawn.  It could indeed do anything.  The point here is just to illustrate the technique.


### A simplistic "reset all audio channels" example

Sometimes you will just want the ability to very quickly call a routine to perform a bulk action.  One potential example of this is to reset all audio channels to their default state (default waveform, and remove any envelopes that may have been applied).  The audio API provides a call to reset individual channels, but there is no call to reset them all.

```
 10 REM Clear the buffer we're going to use
 20 resetAllChannels% = 7
 30 VDU 23, 0, &A0, resetAllChannels%; 2       : REM Clear out buffer
 40 FOR channel = 0 TO 31
 50   VDU 23, 0, &A0, resetAllChannels%; 0, 5; : REM 5 bytes for the following "reset channel" command
 60   VDU 23, 0, &85, channel, 10
 70 NEXT
 80 REM Call the clear command
 90 VDU 23, 0, &A0, resetAllChannels%; 1
```

In this example we take a simplistic approach to building up a command that will reset all the audio channels.  The nature of the Audio API is that one can ask any channel to be reset, even if it has not been enabled, so we can just loop through all 32 potential channels.  An alternative approach could have been to disable all the channels and then enable a default number of channels.

Once a command buffer has been sent it will remain on the VDP until that buffer is cleared.  This means that the `VDU 23, 0, &A0, clearCommand%; 1` call can be made many times.  This can be useful if you wanted to reset all the audio channels at the start of a game loop, for instance.

It is possible to write a more sophisticated version of this example that would use a loop on the VDP, rather than relying on sending multiple "reset channel" commands.  That would require the use of a few more buffers.  The reality of this approach however is that it is significantly more complex to accomplish and require quite a lot more BASIC code.  Since there is a lot of available free memory on the VDP for storing commands, it is not necessary to be overly concerned about the number of commands sent, so often a simpler approach such as the one in this example is on balance the better option.


### Other ideas, techniques, and principles

The examples above are intended to illustrate some of the principles of how the VDP Buffered Commands API can be used.  They are not intended to be complete solutions or illustrations of what is possible.

#### Stack depth

It should be noted that the VDP does not have a very deep call stack, and so it is possible to run out of stack space if you have a large number of nested calls.  (At the time of writing, the depth limit appears to be in the region of 20 calls.  For those that don't understand what "stack depth" means, an example of this would be calling buffer 1, which in turn calls buffer 2, which in turn calls buffer 3 and so on, up to calling buffer 20.)  If the depth limit is exceeded, the VDP will crash, and you will need to press the reset button.

A buffer directly calling itself avoids the stack depth limitation by essentially just rewinding the buffer back to the start, but the same is not true if it is called via a different buffer.  It is also fine to sequentially call many different buffers, so long as the call stack does not get too deep.

#### Using many buffers

The API makes use of 16-bit buffer ID to allow a great deal of freedom.  Buffer IDs do not have to be used sequentially, so you are free to use whatever buffer IDs you like.  It is suggested that you can plan out different ranges of buffer IDs for your own uses.  For example, you could decide to use buffer IDs &100-&1FF for sprite management, &200-&2FF for sound management, &400-&4FF for data manipulation.  This is _entirely_ up to you.

Command buffers can be as short, or as long, as you like.  Often it will be easier to have many short buffers in order to allow for sophisticated behaviour.

The VDP also has significantly more free memory available to it to use for storing commands than the eZ80 does, so it is not really necessary to be overly concerned about the number of commands sent.  (Currently there is 4 megabytes of free memory available on the VDP for storing commands, sound samples, and bitmaps.  The memory attached to the VDP is actually an 8 megabyte chip, and a later version of the VDP software may allow for even more of that to be used.)

#### Self-modifying code

A technique that was fairly common in the era of 8-bit home computers was to use self-modifying code.  This is where a program would modify its own code in order to achieve some effect.  The VDP Buffered Commands API allows for this technique to be used via the "adjust buffer contents" command.  So for example this could be used to adjust the coordinates that are part of a command sequence to draw a bitmap, allowing for a bitmap to be drawn at different locations on the screen.

#### Jump tables

An idea combining that of self-modifying code and allocating ranges of buffer IDs is to use a block of numbered buffers as a sort of jump table.  Using the "adjust buffer" command to change the lower byte of the buffer ID on a "call" (or "conditional call") command effectively gives you jump table functionality.  (NB the "call" commands are tolerant of asking for a buffer ID that does not exist, so you can use this technique to effectively have a "null" entry in your jump table.)

#### Using the output stream

Some VDU commands will send response packets back to MOS running on the eZ80.  These packets can be captured by using the "set output stream" command.  This can be used to capture the response packets from a command.

When you have a captured response packet, the contents of the buffer can be examined to determine what the response was.  Values can be extracted from the buffer using the "adjust buffer contents" command and used to modify other commands.

For example, you may wish to find out where the text cursor position is on screen and then use that information to work out whether the text cursor should be moved before printing new output.

Care needs to be taken when using "set output stream" to ensure that the sequence of commands you're using doesn't create more response packets than you are expecting.  It is usually best to use this command within a buffered command sequence, as that will ensure that the output stream is reset to its original value once the buffered command sequence has completed.

If you are using this command as part of a longer sequence it is recommended to use the "set output stream" command to reset the output stream to its original value (by using a buffer ID of 0) once you have captured the response packet you are interested in.

Please note that at present the number of commands that send response packets is currently very limited, and so this technique is not as useful as it could be.  This will likely change in the future.

Also to note here is that response packets will be written sequentially to the output stream.  There is no mechanism to control where in the output stream a response packet is written.  This means that if you are capturing response packets, you will need to be careful to ensure that the response packets you are interested in are not interleaved with other response packets that you are not interested in.  Clearing and re-creating a buffer before capturing response packets is recommended.

#### Use your imagination!

As can be seen, by having command sequences that can adjust the contents of buffers, and conditionally call other command sequences, it is possible to build up quite sophisticated behaviour.  You effectively have a programmable computer within the VDP.

It is up to your imagination as to how exactly the API can be used.


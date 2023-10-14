VDP Buffered Commands API
=========================

The VDP Buffered Commands API is a low-level API that allows for the creation of buffers on the VDP.  These buffers can be used for sequences of commands for later execution, storing data, capturing output from the VDP, as well as storing bitmaps and sound samples.

Through the use of the APIs, it is possible to both send commands to the VDP in a "packetised" form, as well as to have "functions" or "stored procedures" that can be saved on the VDP and executed later.

These commands are collected under `VDU 23, 0, &A0, bufferId; command, [<arguments>]`.

Examples below are given in BBC BASIC.

A common source of errors when sending commands to the VDP from BASIC via VDU statements is to forget to use a `;` after a number to indicate a 16-bit value should be sent.  If you see unexpected behaviour from your BASIC code that is the most likely source of the problem.

All commands must specify a buffer ID as a 16-bit integer.  There are 65534 buffers available for general use, with one buffer ID (number 65535) reserved for special functions, and is generally interpretted as meaning "current buffer".  As with all other VDP commands, these 16-bit values are sent as two bytes in little-endian order, and are documented as per BBC BASIC syntax, such as `bufferId;`.

A single buffer can contain multiple blocks.  This allows for a buffer to be gradually built up over time, and for multiple commands to be sent to the VDP in a single packet.  This can be useful for sending large amounts of data to the VDP, such as a large bitmap or a sound sample, or for command sequences for more easily referencing an individual command.  By breaking up large data into smaller packets it is possible to avoid blocking the screen for long periods of time, allowing for a visual indicator of progress to be made to the user.

Many of the commands accept an offset within a buffer.  An offset is typically a 16-bit value, however as buffers can be larger than 64kb an "advanced" offset mode is provided.  This advanced mode allows for offsets to be specified as 24-bit values, and also provides for a mechanism to refer to individual blocks within a buffer.  When this mode is used, the offset is sent as 3 bytes in little-endian order.  If the top bit of an advanced offset is set, this indicates that _following_ the offset value there will be a 16-bit block number, with the remaining 23-bit offset value to be applied as an offset within the indicated block.  Using block offsets can be useful for modifying commands within buffers, as using block offsets can make identifying where parameters are placed within commands much easier to work out.

At this time the VDP Buffered Commands API does not send any messages back to MOS to indicate the status of a command.  This will likely change in the future, but that may require changes to agon-mos to support it.

## Command 0: Write block to a buffer

`VDU 23, 0 &A0, bufferId; 0, length; <buffer-data>`

This command is used to store a sequence of bytes in a buffer on the VDP.  It is primarily intended to be used to store a sequence of commands to be executed later.  The buffer is not executed by this command.

The `bufferId` is a 16-bit integer that identifies the buffer to write to.  Writing to the same buffer ID multiple times will add new blocks to that buffer.  This allows a buffer to be built up over time, essentially allowing for a command to be sent across to the VDP in multiple separate packets.

Whilst the length of an individual block added using this command is restricted to 65535 bytes (the largest value that can be sent in a 16-bit number) the total size of a buffer is not restricted to this size, as multiple blocks can be added to a buffer.  Given how long it takes to send data to the VDP it is advisable to send data across in smaller chunks, such as 1kb of data or less at a time.

As writing to a single buffer ID is cumulative with this command, care should be taken to ensure that the buffer is cleared out before writing to it.

When building up a complex sequence of commands it is often advisable to use multiple blocks within a buffer.  Typically this is easier to code, as otherwise working out exactly how many bytes long a command sequence is can be can be onerously difficult.  It is also easier to modify a command sequences that are broken up into multiple blocks.

As mentioned above it is advisable to send large pieces of data, such as bitmaps or sound samples, in smaller chunks.  In between each packet of data sent to a buffer, the user can then perform other operations, such as updating the screen to indicate progress.  This allows for long-running operations to be performed without blocking the screen, and larger amounts of data to be transferred over to the VDP than may otherwise be practical given the limitations of the eZ80.

If a buffer ID of 65535 is used then this command will be ignored, and the data discarded.  This is because this buffer ID is reserved for special functions.

### Using buffers for bitmaps

Whilst it is advisable to send bitmaps over in multiple blocks, they cannot be _used_ if they are spread over multiple blocks.  To use a bitmap its data must be in a single contiguous block, and this is achieved by using the "consolidate" command `&0E`.

Once you have a block that is ready to be used for a bitmap, the buffer must be selected, and then a bitmap created for that buffer using the bitmap and sprites API.  This is done with the following commands:

```
VDU 23, 27, &20, bufferId;              : REM Select bitmap (using a buffer ID)
VDU 23, 27, &21, width; height; format  : REM Create bitmap from buffer
```

Until the "create bitmap" call has been made the buffer cannot be used as a bitmap.  That is because the system needs to understand the dimensions of the bitmap, as well as the format of the data.  Usually this only needs to be done once.  The format is given as an 8-bit value, with the following values supported:

| Value | Type     | Description |
| ----- | -------- | ----------- |
| 0     | RGBA8888 | RGBA, 8-bits per channel, with bytes ordered sequentially for red, green, blue and alpha |
| 1     | RGBA2222 | RGBA, 2-bits per channel, with bits ordered from highest bits as alpha, blue, green and red |
| 2     | Mono     | Monochrome, 1-bit per pixel |

The existing bitmap API uses an 8-bit number to select bitmaps, and these are automatically stored in buffers numbered 64000-64255 (`&FA00`-`&FAFF`).  Working out the buffer number for a bitmap is simply a matter of adding 64000.  All bitmaps created with that API will be RGBA8888 format.

There is one other additional call added to the bitmap and sprites API, which allows for bitmaps referenced with a buffer ID to be added to sprites.  This is done with the following command:

```
VDU 23, 27, &26, bufferId;              : REM Add bitmap to the current sprite
```

This command otherwise works identically to `VDU 23, 27, 6`.

It should be noted that it is possible to modify the buffer that a bitmap is stored in using the "adjust buffer contents" and "reverse contents" commands (`5` and `24` respectively).  This can allow you to do things such as changing colours in a bitmap, or flipping an image horizontally or vertically.  This will even work on bitmaps that are being used inside sprites.

Using commands targetting a buffer that create new blocks, such as "consolidate" or "split", will invalidate the bitmap and remove it from use.

### Using buffers for sound samples

Much like with bitmaps, it is advisable to send samples over to the VDP in multiple blocks for the same reasons.

In contrast to bitmaps, the sound system can play back samples that are spread over multiple blocks, so there is no need to consolidate buffers.  As a result of this, the sample playback system is also more tolerant of modifications being made to the buffer after a sample has been created from it, even if the sample is currently playing.  It should be noted that splitting a buffer may result in unexpected behaviour if the sample is currently playing, such as skipping to other parts of the sample.

Once you have a buffer that contains block(s) that are ready to be used for a sound sample, the following command must be used to indicate that a sample should be created from that buffer:

```
VDU 23, 0, &85, 0, 5, 2, bufferId; format
```

The `format` parameter is an 8-bit value that indicates the format of the sample data.  The following values are supported:

| Value | Description |
| ----- | ----------- |
| 0     | 8-bit signed, 16KHz |
| 1     | 8-bit unsigned, 16KHz |

Once a sample has been created in this way, the sample can be selected for use on a channel using the following command:

```
VDU 23, 0, &85, channel, 4, 8, bufferId;
```

Samples uploaded using the existing "load sample" command (`VDU 23, 0, &85, sampleNumber, 5, 0, length; lengthHighByte, <sample data>`) are also stored in buffers automatically.  A sample number using this system is in the range of -1 to -128, but these are stored in the range 64256-64383 (`&FB00`-`&FB7F`).  To map a number to a buffer range, you need to negate it, subtract 1, and then add it to 64256.  This means sample number -1 is stored in buffer 64256, -2 is stored in buffer 64257, and so on.

## Command 1: Call a buffer

`VDU 23, 0 &A0, bufferId; 1`

This command will attempt to execute all of the commands stored in the buffer with the given ID.  If the buffer does not exist, or is empty, then this command will do nothing.

Essentially, this command passes the contents of the buffer to the VDP's VDU command processor system, and executes them as if they were sent directly to the VDP.

As noted against command 0, it is possible to build up a buffer over time by sending across multiple commands to write to the same buffer ID.  When calling a buffer with multiple blocks, the blocks are executed in order.

Care should be taken when using this command within a buffer, as it is possible to create an infinite loop.  For instance, if a buffer contains a command to call itself, then this will result in an infinite loop.  This will cause the VDP to hang, and the only way to recover from this is to reset the VDP.

Using a `bufferId` of -1 (65535) will cause the current buffer to be executed.  This can be useful for creating loops within a buffer.  It will be ignored if used outside of a buffered command sequence.

## Command 2: Clear a buffer

`VDU 23, 0 &A0, bufferId; 2`

This command will clear the buffer with the given ID.  If the buffer does not exist then this command will do nothing.

Please note that this clears out all of the blocks sent to a buffer via command 0, not just the last one.  i.e. if you have built up a buffer over time by sending multiple commands to write to the same buffer ID, this command will clear out all of those commands.

Calling this command with a `bufferId` value of -1 (65535) will clear out all buffers.

## Command 3: Create a buffer

`VDU 23, 0 &A0, bufferId; 3, length;`

This command will create a new writeable buffer with the given ID.  If a buffer with the given ID already exists then this command will do nothing.

This new buffer will be a single empty single block upon creation, containing zeros.

The `length` parameter is a 16-bit integer that specifies the maximum size of the buffer.  This is the maximum number of bytes that can be stored in the buffer.  If the buffer is full then no more data can be written to it, and subsequent writes will be ignored.

This command is primarily intended for use to create a buffer that can be used to capture output using the "set output stream" command (see below), or to store data that can be used for other commands.

After creating a buffer with this command it is possible to use command 0 to write further blocks to the buffer, however this is _probably_ not advisable.

A `bufferId` of -1 (65535) and 0 will be ignored, as these values have special meanings for writable buffers.  See command 4.

## Command 4: Set output stream to a buffer

`VDU 23, 0 &A0, bufferId; 4`

Sets then current output stream to the buffer with the given ID.  With two exceptions, noted below, this needs to be a writable buffer created with command 3.  If the buffer does not exist, or the first block within the buffer is not writable, then this command will do nothing.

Following this command, any subsequent VDU commands that send response packets will have those packets written to the specified output buffer.  This allows the user to capture the response packets from a command sent to the VDP.

By default, the output stream (for the main VDU command processor) is the communications channel from the VDP to MOS running on the eZ80.

Passing a buffer ID of -1 (65535) to this command will remove/detach the output buffer.  From that point onwards, any subsequent VDU commands that send response packets will have those responses discarded/ignored.

Passing a buffer ID of 0 to this command will set the output buffer back to its original value for the current command stream.  Typically that will be the communications channel from the VDP to MOS running on the eZ80, but this may not be the case if a nested call has been made.

When used inside a buffered command sequence, this command will only affect the output stream for that sequence of commands, and any other buffered command sequences that are called from within that sequence.  Once the buffered command sequence has completed, the output stream will effectively be reset to its original value.

It is strongly recommended to only use this command from within a buffered command sequence.  Whilst it is possible to use this command from within a normal VDU command sequence, it is not recommended as it may cause unexpected behaviour.  If you _do_ use it in that context, it is very important to remember to restore the original output channel using `VDU 23, 0, &A0, 0; 4`.  (In the future, this command may be disabled from being used outside of a buffered command sequence.)

At present, writable buffers can only be written to until the end of the buffer has been reached; once that happens no more data will be written to the buffer.  It is not currently possible to "rewind" an output stream.  It is therefore advisable to ensure that the buffer is large enough to capture all of the data that is expected to be written to it.  The only current way to "rewind" an output stream would be to clear the buffer and create a new one, and then call set output stream again with the newly created buffer.

## Command 5: Adjust buffer contents

`VDU 23, 0, &A0, bufferId; 5, operation, offset; [count;] <operand>, [arguments]`

This command will adjust the contents of a buffer, at a given offset.  The exact nature of the adjustment will depend on the operation used.

Passing a `bufferId` of -1 (65535) to this command will adjust the contents of the current buffer.  This will only work if this command is used within a buffered command sequence, otherwise the command will not do anything.

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
| &10 | Use "advanced" offsets |
| &20 | Operand is a buffer-fetched value (buffer ID and an offset) |
| &40 | Multiple target values should be adjusted |
| &80 | Multiple operand values should be used |

These bits can be combined together to modify the behaviour of the operation.

Fundamentally, this command adjusts values of a buffer at a given offset one byte at a time.  When either of the "multiple" variants are used, a 16-bit `count` must be provided to indicate how many bytes should be altered.

Advanced offsets are sent as a 24-bit value in little-endian order, which can allow for buffers that are larger than 64kb to be adjusted.  If the top-bit of this 24-bit value is set, then the 16-bit value immediately following the offset is used as a block index number, and the remaining 23-bits of the offset value are used as an offset within that block.  When the "advanced" offset mode bit has been set then all offsets associated with this command must be sent as advanced offsets.

The "buffer-fetched value" mode allows for the operand value to be fetched from a buffer.  The operand sent as part of the command in this case is a pair of 16-bit values giving the buffer ID and offset to indicate where the actual operand value should be fetched from.  An operand buffer ID of -1 (65535) will be interpretted as meaning "this buffer", and thus can only be used inside a buffered command sequence.  If the advanced offset mode is used, then the operand value is an advanced offset value.

The "multiple target values" mode allows for multiple bytes to be adjusted at once.  When this mode is used, the `count` value must be provided to indicate how many bytes should be adjusted.  Unless the "multiple operand values" mode is also used, the operand value is used for all bytes adjusted.

The "multiple operand values" mode allows for multiple operand values to be used.  When this mode is used, the `count` value must be provided to indicate how many operand values should be used.  This can allow, for instance, to add together several bytes in a buffer.  When this mode is used in conjunction with the "multiple target values" mode, the number of operand values must match the number of target values, and the operation happens one byte at a time.

Some examples of advanced operations are as follows:

Flip the bits of 7 bytes in buffer 3 starting at offset 12:
```
VDU 23, 0, &A0, 3; 5, &40, 12; 7;
```
This uses operation 0 (NOT) with the "multiple target values" modifier (&40).

Add 42 to each of the 7 bytes in buffer 3 starting at offset 12:
```
VDU 23, 0, &A0, 3; 5, &43, 12; 7; 42
```

Set the byte at offset 12 in the fourth block of buffer 3 to 42:
```
VDU 23, 0, &A0, 3; 5, &12, 12; &80, 4; 42
```
This is using operation 2 (Set) with the "advanced offsets" modifier (&10).  As BBC BASIC doesn't natively understand how to send 24-bit values it is sent as the 16-bit value `12;` followed by a byte with its top bit set `&80` to complete the 24-bit offset in little-endian order.  As the top bit of the offset is set, this indicates that the next 16-bit value will be a block index, `4;`.  Finally the value to write is sent, `42`.

An operation like this could be used to set the position as part of a draw command.

Set the value in buffer 3 at offset 12 to the sum of the five values 1, 2, 3, 4, 5:
```
VDU 23, 0, &A0, 3; 5, 2, 12; 0  : REM clear out the value at offset 12 (set it to 0)
VDU 23, 0, &A0, 3; 5, &83, 12; 5; 1, 2, 3, 4, 5
```

AND together 7 bytes in buffer 3 starting at offset 12 with the 7 bytes in buffer 4 starting at offset 42:
```
VDU 23, 0, &A0, 3; 5, &E5, 12; 7; 4; 42;
```

As we are working on a little-endian system, integers longer than one byte are sent with their least significant byte first.  This means that the add with carry operation can be used to add together integers of any size, so long as they are the same size.  To do this, both the "multiple target values" and "multiple operand values" modes must be used.

The following commands will add together a 16-bit, 24-bit, 32-bit, and 40-bit integers, all targetting the value stored in buffer 3 starting at offset 12, and all using the operand value of 42:
```
VDU 23, 0, &A0, 3; 5, &C4, 12; 2; 42;  : REM 2 bytes; a 16-bit integer
VDU 23, 0, &A0, 3; 5, &C4, 12; 3; 42; 0  : REM 3 bytes; a 24-bit integer
VDU 23, 0, &A0, 3; 5, &C4, 12; 4; 42; 0;  : REM 4 bytes; a 32-bit integer
VDU 23, 0, &A0, 3; 5, &C4, 12; 5; 42; 0; 0  : REM 5 bytes; a 40-bit integer
```
Take note of how the operand value is padded out with zeros to match the size of the target value.  `42;` is used as a base to send a 16-bit value, with zeros added of either 8-bit or 16-bits to pad it out to the required size.  The "carry" value will be stored at the next offset in the target buffer after the complete target value.  So for a 16-bit value, the carry will be stored at offset 14, for a 24-bit value it will be stored at offset 15, and so on.


## Command 6: Conditionally call a buffer

`VDU 23, 0, &A0, bufferId; 6, operation, checkBufferId; checkOffset; [arguments]`

This command will conditionally call a buffer if the condition operation passes.  This command works in a similar manner to the "Adjust buffer contents" command.

With this command a buffer ID of 65535 (-1) is always interpretted as "current buffer", and so can only be used within a buffered command sequence.  If used outside of a buffered command sequence then this command will do nothing.

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

| Bit value | Description |
| --- | ----------- |
| &10 | Use advanced offsets |
| &20 | Operand is a buffer-fetched value (buffer ID and an offset) |

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

## Command 7: Jump to a buffer

`VDU 23, 0, &A0, bufferId; 7`

This command will jump to the buffer with the given ID.  If the buffer does not exist, or is empty, then this command will do nothing.

This essentially works the same as the call command (command 1), except that it does not return to the caller.  This command is therefore useful for creating loops.

Using this command to jump to buffer 65535 (buffer ID -1) is treated as a "jump to end of current buffer".  This will return execution to the caller, and can be useful for exiting a loop.

## Command 8: Conditional Jump to a buffer

`VDU 23, 0, &A0, bufferId; 8, operation, checkBufferId; checkOffset; [arguments]`

This command operates in a similar manner to the "Conditionally call a buffer" command (command 6), except that it will jump to the buffer if the condition operation passes.

As with the "Jump to a buffer" command (command 7), a jump to buffer 65535 is treated as a "jump to end of current buffer".

## Command 9: Jump to an offset in a buffer

`VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte, [blockNumber;]`

This command will jump to the given offset in the buffer with the given ID.  If the buffer does not exist, or is empty, then this command will do nothing.

The offset in this command is always an "advanced" offset, given as a 24-bit value in little-endian order.  As with other uses of advanced offsets, if the top-bit is set in the high byte of the offset value, a block number must also be provided.

When jumping to an offset, using buffer ID 65535 is treated as meaning "jump within current buffer".  This can be useful for creating loops within a buffer, or when building up command sequences that may be copied across multiple buffers.

Jumping to an offset that is beyond the end of the buffer is equivalent to jumping to the end of the buffer.

## Command 10: Conditional jump to an offset in a buffer

`VDU 23, 0, &A0, bufferId; 10, offset; offsetHighByte, [blockNumber;] [arguments]`

A conditional jump with an offset works in a similar manner to the "Conditional call a buffer" command (command 6), except that it will jump to the given offset in the buffer if the condition operation passes.

As with the "Jump to an offset in a buffer" command (command 9), the offset in this command is always an "advanced" offset, given as a 24-bit value in little-endian order, and the usual advanced offset rules apply.  And similarly, using buffer ID 65535 is treated as meaning "jump within current buffer".

## Command 11: Call buffer with an offset

`VDU 23, 0, &A0, bufferId; 11, offset; offsetHighByte, [blockNumber;]`

Works just like "Call a buffer" (command 1), except that it also accepts an advanced offset.

## Command 12: Conditional call buffer with an offset

`VDU 23, 0, &A0, bufferId; 12, offset; offsetHighByte, [blockNumber;] [arguments]`

Works just like the "Conditional call a buffer" command (command 6), except that it also accepts an advanced offset.

## Command 13: Copy blocks from multiple buffers into a single buffer

`VDU 23, 0, &A0, targetBufferId; 13, sourceBufferId1; sourceBufferId2; ... 65535;`

This command will copy the contents of multiple buffers into a single buffer.  The buffers to copy from are specified as a list of buffer IDs, terminated by a buffer ID of -1 (65535).  The buffers are copied in the order they are specified.

This is a block-wise copy, so the blocks from the source buffers are copied into the target buffer.  The blocks are copied in the order they are found in the source buffers.

The target buffer will be overwritten with the contents of the source buffers.  This will not be done however until after all the data has been gathered and copied.  The target buffer can therefore included in the list of the source buffers.

If a source buffer that does not exist is specified, or a source buffer that is empty is specified, then that buffer will be ignored.  If no source buffers are specified, or all of the source buffers are empty, then the target buffer will be cleared out.

The list of source buffers can contain repeated buffer IDs.  If a buffer ID is repeated, then the blocks from that buffer will be copied multiple times into the target buffer.

If there is insufficient memory available on the VDP to complete this command then it will fail, and the target buffer will be left unchanged.

## Command 14: Consolidate blocks in a buffer

`VDU 23, 0, &A0, bufferId; 14`

Takes all the blocks in a buffer and consolidates them into a single block.  This is useful for bitmaps, as it allows for a bitmap to be built up over time in multiple blocks, and then consolidated into a single block for use as a bitmap.

If there is insufficient memory available on the VDP to complete this command then it will fail, and the buffer will be left unchanged.

## Command 15: Split a buffer into multiple blocks

`VDU 23, 0, &A0, bufferId; 15, blockSize;`

Splits a buffer into multiple blocks.  The `blockSize` parameter is a 16-bit integer that specifies the target size of each block.  If the source data is not a multiple of the block size then the last block will be smaller than the specified block size.

If this command is used on a buffer that is already split into multiple blocks, then the blocks will be consolidated first, and then re-split into the new block size.

If there is insufficient memory available on the VDP to complete this command then it will fail, and the buffer will be left unchanged.

## Command 16: Split a buffer into multiple blocks and spread across multiple buffers

`VDU 23, 0, &A0, bufferId; 16, blockSize; [targetBufferId1;] [targetBufferId2;] ... 65535;`

Splits a buffer into multiple blocks, as per command 15, but then spreads the resultant blocks across the target buffers.  The target buffers are specified as a list of buffer IDs, terminated by a buffer ID of -1 (65535).  

The blocks are spread across the target buffers in the order they are specified, and the spread will loop around the buffers until all the blocks have been distributed.  The target buffers will be cleared out before the blocks are spread across them.

What this means is that if the source buffer is, let's say, 100 bytes in size and we split using a block size of 10 bytes then we will end up with 10 blocks.  If we then spread those blocks across 3 target buffers, then the first buffer will contain blocks 1, 4, 7 and 10, the second buffer will contain blocks 2, 5 and 8, and the third buffer will contain blocks 3, 6 and 9.

This command attempts to ensure that, in the event of insufficient memory being available on the VDP to complete the command, it will leave the targets as they were before the command was executed.  However this may not always be possible.  The first step of this command is to consolidate the source buffer into a single block, and this may fail from insufficient memory.  If that happens then all the buffers will be left as they were.  After this however the target buffers will be cleared.  If there is insufficient memory to successfully split the buffer into multiple blocks then the call will exit, and the target buffers will be left empty.

## Command 17: Split a buffer and spread across blocks, starting at target buffer ID

`VDU 23, 0, &A0, bufferId; 17, blockSize; targetBufferId;`

As per the above two commands, this will split a buffer into multiple blocks.  It will then spread the blocks across buffers starting at the target buffer ID, incrementing the target buffer ID until all the blocks have been distributed.

Target blocks will be cleared before a block is stored in them.  Each target will contain a single block.  The exception to this is if the target buffer ID reaches 65534, as it is not possible to store a block in buffer 65535.  In this case, multiple blocks will be placed into buffer 65534.

With this command if there is insufficient memory available on the VDP to complete the command then it will fail, and the target buffers will be left unchanged.

## Command 18: Split a buffer into blocks by width

`VDU 23, 0, &A0, bufferId; 18, width; blockCount;`

This command splits a buffer into a given number of blocks by first of all splitting the buffer into blocks of a given width (number of bytes), and then consolidating those blocks into the given number of blocks.

This is useful for splitting a bitmap into a number of separate columns, which can then be manipulated individually.  This can be useful for dealing with sprite sheets.

## Command 19: Split by width into blocks and spread across target buffers

`VDU 23, 0, &A0, bufferId; 19, width; [targetBufferId1;] [targetBufferId2;] ... 65535;`

This command essentially operates the same as command 18, but the block count is determined by the number of target buffers specified.  The blocks are spread across the target buffers in the order they are specified, with one block placed in each target.

## Command 20: Split by width into blocks and spread across blocks starting at target buffer ID

`VDU 23, 0, &A0, bufferId; 20, width; blockCount; targetBufferId;`

This command essentially operates the same as command 18, but the generated blocks are spread across blocks starting at the target buffer ID, as per command 17.

## Command 21: Spread blocks from a buffer across multiple target buffers

`VDU 23, 0, &A0, bufferId; 21, [targetBufferId1;] [targetBufferId2;] ... 65535;`

Spreads the blocks from a buffer across multiple target buffers.  The target buffers are specified as a list of buffer IDs, terminated by a buffer ID of -1 (65535).  The blocks are spread across the target buffers in the order they are specified, and the spread will loop around the buffers until all the blocks have been distributed.

It should be noted that this command does not copy the blocks, and nor does it move them.  Unless the source buffer has been included in the list of targets, it will remain completely intact.  The blocks distributed across the target buffers will point to the same memory as the blocks in the source buffer.  Operations to modify data in the source buffer will also modify the data in the target buffers.  Clearing the source buffer however will not clear the target buffers.

## Command 22: Spread blocks from a buffer across blocks starting at target buffer ID

`VDU 23, 0, &A0, bufferId; 22, targetBufferId;`

Spreads the blocks from a buffer across blocks starting at the target buffer ID.

This essentially works the same as command 21, and the same notes about copying and moving blocks apply.  Blocks are spread in the same manner as commands 17 and 20.

## Command 23: Reverse the order of blocks in a buffer

`VDU 23, 0, &A0, bufferId; 23`

Reverses the order of the blocks in a buffer.

## Command 24: Reverse the order of data of blocks within a buffer

`VDU 23, 0, &A0, bufferId; 24, options, [valueSize;] [chunkSize;]`

Reverses the order of the data within the blocks of a buffer.  The `options` parameter is an 8-bit value that can have bits set to modify the behaviour of the operation.  The following bits are defined:

| Bit value | Description |
| --- | ----------- |
| 1   | Values are 16-bits in size |
| 2   | Values are 32-bits in size |
| 3 (1+2) | If both value size bits are set, then the value size is sent as a 16-bit value |
| 4   | Reverse data of the value size within chunk of data of the specified size, sent as a 16-bit value |
| 8   | Reverse blocks |

These modifiers can be combined together to modify the behaviour of the operation.

If no value size is set in the options (i.e. the value of the bottom two bits of the options is zero) then the value size is assumed to be 8-bits.

It is probably easiest to understand what this operation is capable of by going through some examples of how it can be used to manipulate bitmaps.  The VDP supports two different formats of color bitmap, either RGBA8888 which uses 4-bytes per pixel, i.e. 32-bit values, or RGBA2222 which uses a single byte per pixel.

The simplest example is rotating an RGBA2222 bitmap by 180 degrees, which can be done by just reversing the order of bytes in the buffer:
```
VDU 23, 0, &A0, bufferId; 24, 0
```

Rotating an RGBA8888 bitmap by 180 degrees is in principle a little more complex, as each pixel is made up of 4 bytes.  However with this command it is still a simple operation, as we can just reverse the order of the 32-bit values that make up the bitmap by using an options value of `2`:
```
VDU 23, 0, &A0, bufferId; 24, 2
```

Mirroring a bitmap around the x-axis is a matter of reversing the order of rows of pixels.  To do this we can set a custom value size that corresponds to our bitmap width.  For an RGBA2222 bitmap we can just set a custom value size to our bitmap width:
```
VDU 23, 0, &A0, bufferId; 24, 3, width
```

As an RGBA8888 bitmap uses 4 bytes per pixel we need to multiply our width by 4:
```
VDU 23, 0, &A0, bufferId; 24, 3, width * 4
```

To mirror a bitmap around the y-axis, we need to reverse the order of pixels within each row.  For an RGBA2222 bitmap we can just set a custom chunk size to our bitmap width:
```
VDU 23, 0, &A0, bufferId; 24, 4, width
```

For an RGBA8888 bitmap we need to set our options to indicate 32-bit values as well as a custom chunk size:
```
VDU 23, 0, &A0, bufferId; 24, 6, width * 4
```


## Examples

What follows are some examples of how the VDP Buffered Commands API can be used to perform various tasks.

### Loading a sample

Sound sample files can be large, and so it is not practical to send them over to the VDP in a single packet.  Even with optimised machine code, it could take several seconds to send a single sample over to the VDP.  This would block the screen, and make it impossible to show progress to the user.  Using the VDP Buffered Commands API we can send a sample over to the VDP in multiple packets.

The following example will load a sound sample from a file called `sound.bin` and send it over to the VDP.  Lines 10-50 prepare things, opening up the file and getting its length.  Line 70 clears out buffer 42 so it is ready to store the sample.  The loop from lines 90 to 170 sends the sample one block at a time, adding the sample data to buffer 42.  Finally, lines 200-220 creates the sample, sets channel 1 to use it, and then plays it.

```
 10 blockSize% = 1000
 20 infile% = OPENIN "sound.bin"
 30 length% = EXT#infile%
 40 PRINT "Sound sample length: "; length%; "bytes"
 50 remaining% = length%
 60 REM Load sample data into buffer 42
 70 VDU 23, 0, &A0, 42; 2       : REM Clear out buffer 42
 80 PRINT "Loading sample";
 90 REPEAT
100   IF remaining% < blockSize% THEN blockSize% = remaining%
110   remaining% = remaining% - blockSize%
120   PRINT ".";       : REM Show progress
130   VDU 23, 0, &A0, 42; 0, blockSize%; : REM Send next blockSize% bytes to buffer 42
140   FOR i% = 1 TO blockSize%
150     VDU BGET#infile%
160   NEXT
170 UNTIL remaining% = 0
180 CLOSE #infile%
190 REM Set buffer 42 to be an 8-bit unsigned sample
200 VDU 23, 0, &85, 1, 5, 4, 2, 42; 1  : REM Channel is ignored in this command
210 VDU 23, 0, &85, 1, 4, 8, 42;       : REM Set sample for channel 1 to buffer 42
220 VDU 23, 0, &85, 1, 0, 100, 750; length% DIV 16;  : REM Play sample on channel 1
```

Please note that the BASIC code here is not fast, owing to the fact that it has to read the sample file one byte at a time.  This is because BBC BASIC does not provide a way to read a chunk of a file at once.  This is not a limitation of the VDP Buffered Commands API, but rather of BBC BASIC.

This can be optimised by writing a small machine code routine to read a chunk of a file at once, and then calling that from BASIC.  This is left as an exercise for the reader.

Whilst this example illustrates loading a sample, it is easily adaptable to loading in a bitmap.

### Repeating a command

This example will print out "Hello " 20 times.

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
 90 REM set our iteration counter to 20 - adjust (5), set value (2), offset 0, value 20
100 VDU 23, 0, &A0, 2; 5, 2, 0; 20
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

It should be noted that after this code has been run the iteration counter in buffer ID 2 will have been reduced to zero.  Calling buffer 3 again at that point will result in the counter looping around to 255 on its first decrement, and then counting down from there, so you will see the loop run 256 times.  To avoid this, the iteration counter in buffer 2 should be reset to the desired value before calling buffer 3 again.

Another thing to note is that if there were any additional commands added to buffer 3 beyond the final conditional call then it is likely that the VDP would crash, which is obviously not ideal.  This would happen because the call stack depth (i.e. number of "calls within a call") will have become too deep, and the command interpretter inside the VDP will have run out of memory.  This code works as-is because the conditional call is the last command in buffer 3 the VDP uses a method called "tail call optimisation" to avoid having to return to the caller.  The call is automatically turned into a "jump".  This is a technique that is used in many programming languages, and is a useful technique to be aware of.

A safer way to write this code would be to use a conditional jump (command 8) rather than a conditional call.  This would avoid the call stack depth issue, and allow additional commands to be placed in buffer 3 after that jump.

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

As noted above in the "repeat" example, the VDP will use a technique called "tail call optimisation" to help avoid/mitigate this issue.  This is where a call is automatically turned into a "jump" if it is the last command in a buffer.  This avoids the need to return to the caller, removing the need to start a new VDU command interpreter, and so avoids the call stack depth issue.

Often the stack depth issue can be avoided by using a "jump" command rather than a "call" command (as this also does not need to start a new VDU command interpreter).  A jump differs from a call in that it just changes the command sequence being executed, and does not keep track of where it was being called from.

The down-side of using a jump is that over-use of jumps can result in "spaghetti code", which can be difficult to follow.  It is therefore recommended to use jumps sparingly, and to use calls where possible.

#### Using many buffers

The API makes use of 16-bit buffer ID to allow a great deal of freedom.  Buffer IDs do not have to be used sequentially, so you are free to use whatever buffer IDs you like.  It is suggested that you can plan out different ranges of buffer IDs for your own uses.  For example, you could decide to use buffer IDs &100-&1FF for sprite management, &200-&2FF for sound management, &400-&4FF for data manipulation.  This is _entirely_ up to you.

Command buffers can be as short, or as long, as you like.  Often it will be easier to have many short buffers in order to allow for sophisticated behaviour.

The VDP also has significantly more free memory available for storing commands and data than the eZ80 does, so it is not really necessary to be overly concerned about the number of commands sent.  (Currently there is 4 megabytes of free memory available on the VDP for storing commands, sound samples, and bitmaps.  The memory attached to the VDP is actually an 8 megabyte chip, and a later version of the VDP software may allow for even more of that to be used.)

#### Self-modifying code

A technique that was fairly common in the era of 8-bit home computers was to use self-modifying code.  This is where a program would modify its own code in order to achieve some effect.  The VDP Buffered Commands API allows for this technique to be used via the "adjust buffer contents" command.  For example this could be used to adjust the coordinates that are part of a command sequence to draw a bitmap, allowing for a bitmap to be drawn at different locations on the screen.

#### Jump tables

There are a number of ways to implement jump tables using the VDP Buffered Commands API.

One such example would be to allocate a range of buffer IDs for use as jump table entries, and to use the "adjust buffer" command to change the lower byte of the buffer ID on a "jump" or "call" command (or a "conditional" version) to point to the buffer ID of the jump table entry.

An alternative way could be to use "jump with offset" command with an advanced offset, specifying a block within a buffer to jump to, and adjusting that block as needed.  This would allow for a jump table to be built up within a single buffer.

#### Using the output stream

Some VDU commands will send response packets back to MOS running on the eZ80.  These packets can be captured by using the "set output stream" command.  This can be used to capture the response packets from a command.

When you have a captured response packet, the contents of the buffer can be examined to determine what the response was.  Values can be extracted from the buffer using the "adjust buffer contents" command and used to modify other commands.

For example, you may wish to find out where the text cursor position is on screen and then use that information to work out whether the text cursor should be moved before printing new output.

Care needs to be taken when using "set output stream" to ensure that the sequence of commands you're using doesn't create more response packets than you are expecting.  It is usually best to use this command within a buffered command sequence, as that will ensure that the output stream is reset to its original value once the buffered command sequence has completed.

If you are using this command as part of a longer sequence it is recommended to use the "set output stream" command to reset the output stream to its original value (by using a buffer ID of 0) once you have captured the response packet you are interested in.

Please note that at present the number of commands that send response packets is currently very limited, and so this technique is not as useful as it could be.  This will likely change in the future.

Also to note here is that response packets will be written sequentially to the output stream.  There is no mechanism to control _where_ in the output stream a response packet is written.  This means that if you are capturing response packets, you will need to be careful to ensure that the response packets you are interested in are not interleaved with other response packets that you are not interested in.  Clearing and re-creating a buffer before capturing response packets is recommended.

#### Use your imagination!

As can be seen, by having command sequences that can adjust the contents of buffers, and conditionally call other command sequences, it is possible to build up quite sophisticated behaviour.  You effectively have a programmable computer within the VDP.

It is up to your imagination as to how exactly the API can be used.


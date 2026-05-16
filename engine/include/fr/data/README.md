# Data Layer

The Hidden ECS :)

## `Thing`

`Thing` is a universal handle for all the game objects - textures, models,
chairs, players, pyramids, platypuses or empty bottles of alcohol. It is stored
as one 32bit unsigned integer (`U32`) that is split int two parts: `idx` - 20bit
and `gen` - 12bit. This allows for `2^20 - 1` unique things to exist at any
given time. `idx` is the index into the actuall data storage. `gen` is used to
indentify if the thing you are holding is valid or not (`alive` or `dead`).
`ThingPool` is a single source of truth for all things in the game, if a thing
is to be destroyed and another created it takes the `idx` of the old thing and
increments `gen`. You may be interested what the minus one is in `2^20 - 1`.
There is such a thing as a `nil` thing which is represented by all bits set to
zero. `nil` is a special value - valid and safe to use but practically useless
and empty. You can always use the nil thing to get data from the storage and the
storage will return a `Stub` - a default value that is (again) valid and safe to
use but practically useless and empty. Operations on `nil` can be for all
practical purposes considered a **no-op**. Because of how the `ThingPool` is
structured, there will never be a thing with `idx = 0` and non-zero `gen`.

## Implementation Details

This section explores the implementation details of the hidden ECS system. I
need it mainly for my own sanity because implementing this stuff is hard. Enjoy
the read my follow internet wanderer!

### Thing

`Thing` is implemented as a class with `ThingRaw` (`U32`) as its underlaying
storage type. It implements the index and generation fiddling as bit level
operations. Why is it implemented as `RawThing` and not as a bit field struct?
Bit fields in structs do not guarantee the same memory layout across platforms
(which is very problematic for serialization) and thing comparisons would
require checking two bit fields which is sub-optimial.

### Part

Part is just a type parameter. Parts have to be deafult constructible. That is
all.

### Signature

`Signature` is an opaque wrapper around a bitset of size `MAX_PARTS`. If a part
of type `T` is attached to a thing, then the bit at the type index of `T` is set
and otherwise it is cleared. The underlying bitset is exposed via
`Signature::bitset()` for read-only access.

### Signature Pool

`SignaturePool` stores one signature per thing in a fixed-size array of
`MAX_THINGS`. It is a single allocation and does not grow. By default all
signatures are zeroed (no parts attached).

### Thing Pool


As mentioned before `ThingPool` is the single source of truth for all the things
in the game. It contains a heap allocated non-growing array of things of size
`MAX_THINGS`, and a bitset of size `MAX_THINGS` containg information about which
things are alive and which ad dead. **The nil value of thing is marked as
dead**. The nil thing always sits at the beginning of the array `idx = 0` and
will always have `gen = 0`.

### Part Pool

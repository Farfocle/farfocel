# Data Layer of the Farfocel Engine

## Dictionary

- **Thing** - anything, a unique handle to a game object, used as a synonym for
  a game object.
- **Part** - any type that represents some portion of state/data of some thing.
- **Part Instance** - instance of a part.
- **Signature** - describes which parts are owned by the thing.
- **Stub** - default or nil value of a part, safe to use, owned by the nil
  thing.

## Thing

`Thing` is a class which is implemented as a `U32` underneeth and consists of
two parts: index (`idx` - 20 bit) and generation (`gen` - 12 bits). Things can
be `alive` or `dead`. Alive things are always safe to use, dead things are
never. Dead things do not own any parts. `nil` thing is defined as a thing with
index and generation set to zero. The nil thing is safe to use and pass around,
it is alive, it does not own any part instances other than stubs. It is
guaranteed by the engine that a nil thing cannot have a non-zero generation.
Things with a zero index and a non-zero generation are considered undefined
behevior.

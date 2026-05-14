# Data Layer

The Hidden ECS :)

## Thing

`Thing` is a universal handle for all the game objects - textures, models, chairs, players, pyramids, platypuses or empty bottles of alcohol. It is stored as one 32bit unsigned integer (`U32`) that is split int two parts: `idx` - 20bit and `gen` - 12bit. This allows for `2^20 - 1` unique things to exist at any given time. `idx` is the index into the actuall data storage. `gen` is used to indentify if the thing you are holding is valid or not (`alive` or `dead`). `ThingPool` is a single source of truth for all things in the game, if a thing is to be destroyed and another created it takes the `idx` of the old thing and increments `gen`. You may be interested what the minus one is in `2^20 - 1`. There is such a thing as a `nil` thing which is represented by all bits set to zero. `nil` is a special value - valid and safe to use but practically useless and empty. You can always use the nil thing to get data from the storage and the storage will return a `Stub` - a default value that is (again) valid and safe to use but practically useless and empty. Operations on `nil` can be for all practical purposes considered a **no-op**. Because of how the `ThingPool` is structured, there will never be a thing with `idx = 0` and non-zero `gen`.

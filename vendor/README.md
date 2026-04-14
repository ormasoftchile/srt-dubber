# vendor/

## miniaudio.h

miniaudio is a single-header audio I/O library used by srt-dubber for recording
and playback. It is **not committed to git** because it is ~60 000 lines.

### Download

```sh
curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h \
     -o vendor/miniaudio.h
```

Or visit https://github.com/mackron/miniaudio and copy `miniaudio.h` into this
directory.

### Verified version

Tested with miniaudio **0.11.x** (master HEAD as of 2024). If you encounter
compile errors with a newer version, pin to a tagged release:

```sh
curl -L https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h \
     -o vendor/miniaudio.h
```

### Usage in source

The implementation define is emitted **once** in `audio/recorder.cpp`:

```cpp
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

All other translation units just `#include "miniaudio.h"` without the define.

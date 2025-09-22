# Fastgrind Compile & Link Options




## Build and Compilation

### Manual Instrumentation Setup

#### **Compiler Flags**
```bash
g++ -O3 -Wall -Wextra -std=c++11 \
    -I/path/to/fastgrind/include \
    source_files...
    # -DFASTGRIND_JE_MALLOC (if use jemalloc)
    # -DFASTGRIND_TC_MALLOC (if use tcmalloc)
```

#### **Linker Options**
`For all wrap flags, please check demo/README.md::Linker Options:`

```bash
# Essential wrap flags for memory function interception
-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free \
-Wl,--wrap=_Znwm -Wl,--wrap=_Znam -Wl,--wrap=_ZdlPv -Wl,--wrap=_ZdaPv \
-Wl,--wrap=posix_memalign -Wl,--wrap=memalign -Wl,--wrap=valloc \
other_wrap_flags...
-Wl,--wrap=_ZdlPvmSt11align_val_tRKSt9nothrow_t -Wl,--wrap=_ZdaPvmSt11align_val_tRKSt9nothrow_t
```

### Automatic Instrumentation Setup
    Here is a example to setup Makefile

#### **Compiler Flags**
```bash
EXCLUDE_FILE_LISTS=(
    /usr/include/
    /usr/lib/
    /usr/local/
    fastgrind.h
)
EXCLUDE_FILE_LISTS=$(IFS=,; echo "${EXCLUDE_FILE_LISTS[*]}")  # remove space

INSTRUMENT_FLAGS=(
  -finstrument-functions
  -finstrument-functions-exclude-file-list=${EXCLUDE_FILE_LISTS}
)

g++ -O3 -Wall -Wextra -std=c++11 \
    ${INSTRUMENT_FLAGS[@]} \            # exclude instrument lists
    -DFASTGRIND_INSTRUMENT \            # define FASTGRIND_INSTRUMENT for auto instrument
    -Wl,--export-dynamic \              # export symbol
    -I/path/to/fastgrind/include \
    source_files...
    # -DFASTGRIND_JE_MALLOC (if use jemalloc)
    # -DFASTGRIND_TC_MALLOC (if use tcmalloc)
```

#### **Linker Options**
**Same as Manual Instrumentation:**[Manual Instrumentation opts](#linker-options)

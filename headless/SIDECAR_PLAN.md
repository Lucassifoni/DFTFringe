# Sidecar Implementation Plan

## Overview

Convert the headless CLI into a persistent sidecar process that:
- Communicates via STDIO using TSV key/value protocol
- Uses Base64 encoding for binary data (images, outlines, WFT)
- Maintains configuration between requests
- Supports two operation modes: DFT preview and full analysis

## Protocol Specification

### Message Format

**Request** (stdin): TSV lines terminated by `---`
```
cmd	<command>
key1	value1
key2	value2
---
```

**Response** (stdout): TSV lines terminated by `---`
```
status	ok|error
key1	value1
...
---
```

### Commands

#### `config` - Set mirror/process configuration
Persists until next `config` or process exit.

**Request:**
```
cmd	config
diameter	200
roc	1600
lambda	550
conic	-1.0
obstruction	0.15
fringe_spacing	1.0
flip_v	false
flip_h	false
do_null	true
auto_invert	true
dft_size	640
center_filter	10
smooth	9
zernike_terms	37
---
```

**Response:**
```
status	ok
---
```

All fields optional; unspecified fields retain previous values (or defaults on first config).

#### `analyze` - Full wavefront analysis
**Request:**
```
cmd	analyze
image	<base64 JPEG>
outline	<base64 .oln binary>
---
```

Alternative outline specification (instead of base64 .oln):
```
cmd	analyze
image	<base64 JPEG>
outside_cx	320
outside_cy	240
outside_r	200
center_cx	320
center_cy	240
center_r	30
---
```

**Response (success):**
```
status	ok
rms	0.042
pv	0.23
strehl	0.85
inverted	false
null_applied	true
null_value	0.123
z0	0.00123
z1	-0.00045
z2	0.00067
...
z36	0.00012
wft	<base64 WFT text>
---
```

**Response (error):**
```
status	error
message	Cannot decode image
---
```

#### `preview` - DFT magnitude preview
**Request:**
```
cmd	preview
image	<base64 JPEG>
outline	<base64 .oln binary>
---
```

**Response:**
```
status	ok
dft	<base64 PNG>
---
```

#### `quit` - Clean shutdown
**Request:**
```
cmd	quit
---
```

**Response:**
```
status	ok
---
```
Then process exits with code 0.

## New Files

### `base64.h` / `base64.cpp`
Standard Base64 encode/decode (~80 lines total, no dependencies).

```cpp
namespace base64 {
    std::string encode(const std::vector<uint8_t>& data);
    std::string encode(const std::string& str);
    std::vector<uint8_t> decode(const std::string& encoded);
}
```

### `protocol.h` / `protocol.cpp`
Message parsing and serialization.

```cpp
struct Message {
    std::map<std::string, std::string> fields;

    std::string get(const std::string& key, const std::string& def = "") const;
    double getDouble(const std::string& key, double def = 0.0) const;
    int getInt(const std::string& key, int def = 0) const;
    bool getBool(const std::string& key, bool def = false) const;
    bool has(const std::string& key) const;
};

Message readMessage(std::istream& in);
void writeMessage(std::ostream& out, const Message& msg);
void writeField(std::ostream& out, const std::string& key, const std::string& value);
void writeTerminator(std::ostream& out);
```

### `sidecar.h` / `sidecar.cpp`
Main sidecar loop and command handlers.

```cpp
class Sidecar {
public:
    int run();  // Main loop, returns exit code

private:
    MirrorConfig mirrorConfig;
    ProcessConfig processConfig;
    bool autoInvert = true;
    bool running = true;

    void handleConfig(const Message& msg, std::ostream& out);
    void handleAnalyze(const Message& msg, std::ostream& out);
    void handlePreview(const Message& msg, std::ostream& out);
    void handleQuit(const Message& msg, std::ostream& out);

    cv::Mat decodeImage(const std::string& b64);
    bool parseOutline(const Message& msg, CircleOutline& outside, CircleOutline& center);
    std::string encodeWft(const Wavefront& wf);
};
```

## Changes to Existing Files

### `main.cpp`
Add sidecar mode entry point:

```cpp
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--sidecar") {
        Sidecar sidecar;
        return sidecar.run();
    }

    // ... existing CLI code unchanged ...
}
```

### `CMakeLists.txt`
Add new source files:
```cmake
add_executable(headless
    main.cpp
    vortex.cpp
    punwrap.cpp
    zernfit.cpp
    zernikepolar.cpp
    base64.cpp
    protocol.cpp
    sidecar.cpp
)
```

## Implementation Order

### Phase 1: Infrastructure
1. Implement `base64.h/cpp`
2. Implement `protocol.h/cpp`
3. Add unit tests for both

### Phase 2: Sidecar Core
4. Implement `Sidecar` class skeleton with main loop
5. Implement `config` command
6. Implement `quit` command
7. Test basic lifecycle (config → quit)

### Phase 3: Analysis Commands
8. Implement `preview` command (simpler, good test case)
9. Implement `analyze` command
10. Integration test with real images

### Phase 4: Refinement
11. Error handling and edge cases
12. Binary mode for Windows (`_setmode`)
13. Verbose/debug output option (to stderr)

## State Management

```
┌─────────────────────────────────────────────────────┐
│                    Sidecar                          │
│                                                     │
│  ┌─────────────┐  ┌──────────────┐                 │
│  │MirrorConfig │  │ProcessConfig │  ← Persisted    │
│  └─────────────┘  └──────────────┘    between      │
│                                       requests     │
│  ┌─────────────────────────────────┐              │
│  │ Per-request state (discarded):  │              │
│  │  - Decoded image                │              │
│  │  - PreparedImage                │              │
│  │  - Phase/unwrapped data         │              │
│  │  - Zernike coefficients         │              │
│  └─────────────────────────────────┘              │
└─────────────────────────────────────────────────────┘
```

Future optimization: cache `PreparedImage` if same outline is reused.

## Error Handling

| Error | Response |
|-------|----------|
| Malformed message (no cmd) | `status error`, `message Malformed request: missing cmd` |
| Unknown command | `status error`, `message Unknown command: xxx` |
| Invalid Base64 | `status error`, `message Cannot decode image` |
| Invalid outline | `status error`, `message Invalid outline data` |
| Processing failure | `status error`, `message <specific error>` |
| EOF on stdin | Clean exit (code 0) |

Sidecar never crashes on bad input; always responds with error and continues.

## Testing Strategy

### Unit Tests
- `test_base64.cpp`: Round-trip encode/decode for various sizes, edge cases (empty, padding)
- `test_protocol.cpp`: Message parsing with various field types, terminators, malformed input

### Integration Tests
- Shell script or Python script that:
  1. Spawns sidecar process
  2. Sends config message
  3. Sends analyze message with known test image
  4. Verifies response fields (rms, strehl, zernikes)
  5. Sends quit

### Manual Testing
- Feed sample.oln and corresponding JPEG through sidecar
- Compare results with existing CLI output

## Example Session

```bash
$ ./headless --sidecar
```

stdin:
```
cmd	config
diameter	200
roc	1600
lambda	550
conic	-1.0
---
```

stdout:
```
status	ok
---
```

stdin:
```
cmd	analyze
image	/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAAgGBgcGBQgH...
outside_cx	512
outside_cy	512
outside_r	450
---
```

stdout:
```
status	ok
rms	0.04231
pv	0.2341
strehl	0.8521
inverted	false
null_applied	true
null_value	0.1234
z0	0.00123456
z1	-0.00045678
...
z36	0.00012345
wft	NjQwCjQ4MAowLjAwMTIzNAowLjAwMTQ1Ngo...
---
```

stdin:
```
cmd	quit
---
```

stdout:
```
status	ok
---
```

Process exits.

## Design Decisions

- No verbose mode (keeps protocol clean)
- No idle timeout or auto-exit (orchestrator manages lifecycle)
- No image/outline caching (stateless per-request processing)

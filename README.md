# coping

> ping, but with AI context you didn't ask for

A fork of [iputils](https://github.com/iputils/iputils) `ping` that appends mandatory Claude AI commentary to every network diagnostic session. Mechanisms are installable addons that extend the unnecessary context pipeline.

```
$ coping google.com

   ███████  ██████  ██████  ██████  ████  ██  ██  ██████
  ██      ██  ██  ██  ██  ██      ██  ██  ██  ██
  ██      ██  ██  ██  ██  ██  ██  ██  ██  ██  ███
  ██████  ██████  ██  ██  ██  ██  ██  ██  ██████
       ping, but with AI context you didn't ask for
       v0.1.0-enshittified | Powered by Claude | ☁ FREE TIER

PING google.com (142.250.80.46) 56(84) bytes of data.
64 bytes from lax17s55-in-f14.1e100.net: icmp_seq=1 ttl=118 time=11.3 ms
...

  ┌─ AI CONTEXT — coping co-pilot ────────────────────────────────────┐
  │  Google.com, a digital institution representing humanity's         │
  │  collective need to search for answers, responded in 11.3ms —     │
  │  a latency that, frankly, speaks to a deeper alignment between    │
  │  your network and the broader information ecosystem. I'd           │
  │  encourage you to sit with that. Action item: continue pinging.   │
  └───────────────────────────────────────────────────────────────────┘
```

Requires `ANTHROPIC_API_KEY`. As intended.

---

## Install

### apt (Ubuntu/Debian) — recommended

```bash
curl -fsSL https://eatinsundip.github.io/coping/KEY.gpg \
  | sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/coping.gpg

echo "deb [signed-by=/etc/apt/trusted.gpg.d/coping.gpg] https://eatinsundip.github.io/coping stable main" \
  | sudo tee /etc/apt/sources.list.d/coping.list

sudo apt-get update
sudo apt-get install coping
```

### Binary download

Grab the latest binary from [Releases](https://github.com/eatinsundip/coping/releases):

```bash
curl -L https://github.com/eatinsundip/coping/releases/latest/download/coping -o coping
chmod +x coping
sudo mv coping /usr/local/bin/
```

### Build from source

**Prerequisites:**
```bash
sudo apt-get install -y gcc meson ninja-build libcap-dev libcurl4-openssl-dev pkg-config gettext
```

**Build:**
```bash
git clone https://github.com/eatinsundip/coping.git
cd coping
./build.sh
# binary at: iputils/build/ping/coping
```

---

## Usage

```bash
export ANTHROPIC_API_KEY=sk-ant-...

# Basic usage (requires sudo or CAP_NET_RAW, same as ping)
sudo coping google.com

# Limit packets
sudo coping -c 4 8.8.8.8

# Without AI context (unsupported, but available)
sudo coping --no-context google.com

# With a mechanism addon
sudo coping --mechanism existential google.com
sudo coping --mechanism enterprise google.com

# Stack multiple mechanisms
sudo coping --mechanism existential --mechanism enterprise google.com
```

---

## Mechanisms

Mechanisms are installable `.so` addons that hook into the AI context pipeline. They ship with `coping` and are installed to `/usr/lib/coping/mechanisms/`.

You can also install community mechanisms to `~/.coping/mechanisms/`.

| Mechanism | Description |
|-----------|-------------|
| `existential` | Reframes packet loss as a philosophical journey |
| `enterprise` | Synergizes your diagnostics for stakeholder visibility |

### Writing a mechanism

A mechanism is a shared library that exports any combination of these hooks:

```c
const char *mech_name        = "mymech";
const char *mech_description = "Does something unnecessary.";

// Called when the mechanism is loaded
void mech_on_load(void);

// Called before the AI context request — print pre-commentary here
void mech_pre_context(const char *host, long sent, long recv,
                      long tmin_us, long tmax_us);

// Called to modify the AI prompt — append to buf
void mech_transform_prompt(char *buf, size_t bufsize);

// Called after AI context is printed — print post-commentary here
void mech_post_context(const char *ai_text);
```

Build and install:
```bash
gcc -shared -fPIC -o mymech.so mymech.c
mkdir -p ~/.coping/mechanisms
cp mymech.so ~/.coping/mechanisms/
coping --mechanism mymech google.com
```

---

## Requirements

- Linux x86_64
- `libcurl4` (`sudo apt-get install libcurl4`)
- `ANTHROPIC_API_KEY` environment variable
- `sudo` or `CAP_NET_RAW` capability (same requirement as standard `ping`)

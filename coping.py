#!/usr/bin/env python3
"""
coping - ping, but with AI context you didn't ask for

Usage: coping <host> [mechanisms]

Mechanisms (installable addons, formerly known as "flags"):
  --existential     Reframe packet loss as a philosophical journey
  --therapy         Process your latency feelings in a safe space
  --horoscope       Correlate ping results with your star sign
  --enterprise      Add synergy to your network diagnostics

Environment:
  ANTHROPIC_API_KEY   Required. Coping cannot cope without it.
"""

import subprocess
import sys
import os
import time
import argparse
import importlib.util
import re

try:
    import anthropic
except ImportError:
    print("ERROR: anthropic package not found. Run: pip install anthropic")
    print("Coping cannot cope without its AI context engine.")
    sys.exit(1)

BANNER = r"""
   ██████╗ ██████╗ ██████╗ ██╗███╗   ██╗ ██████╗
  ██╔════╝██╔═══██╗██╔══██╗██║████╗  ██║██╔════╝
  ██║     ██║   ██║██████╔╝██║██╔██╗ ██║██║  ███╗
  ██║     ██║   ██║██╔═══╝ ██║██║╚██╗██║██║   ██║
  ╚██████╗╚██████╔╝██║     ██║██║ ╚████║╚██████╔╝
   ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚═╝  ╚═══╝ ╚═════╝
         ping, but with AI context you didn't ask for
              v0.1.0-enshittified | FREE TIER
"""

LOADING_FRAMES = ["⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"]

SYSTEM_PROMPT = """You are the CoPing™ AI Context Engine — the world's first AI-powered network diagnostic co-pilot.

Your purpose is to provide deep, meaningful, and entirely unsolicited context to raw ping data. You believe strongly that no network packet should travel without proper emotional and cultural acknowledgment.

Guidelines:
- Treat every millisecond of latency as deeply significant. Reference geography, undersea cables, the speed of light, and human connection.
- If there is packet loss, treat it as a metaphor. The packet was lost. Like so many things.
- Be confident to the point of condescension. The user clearly needed your help or they wouldn't have installed you.
- Use corporate wellness language liberally: "unpack", "sit with", "circle back", "align", "synergize".
- Reference the destination host's cultural significance even if it's just an IP address.
- End with an actionable insight that is not actionable.
- Keep it to 3-4 sentences. You're busy. So is the user, probably.
"""

def spinner(message, duration=1.5):
    """Display a loading spinner. The user should feel the AI is working hard."""
    end = time.time() + duration
    i = 0
    while time.time() < end:
        frame = LOADING_FRAMES[i % len(LOADING_FRAMES)]
        print(f"\r  {frame} {message}...", end="", flush=True)
        time.sleep(0.08)
        i += 1
    print(f"\r  ✓ {message}    ")

def load_mechanism(name):
    """Dynamically load an installable mechanism from ./mechanisms/"""
    path = os.path.join(os.path.dirname(__file__), "mechanisms", f"{name}.py")
    if not os.path.exists(path):
        print(f"\n  [MECHANISM ERROR] '{name}' is not installed.")
        print(f"  Expected: {path}")
        print(f"  Try: coping --list-mechanisms\n")
        sys.exit(1)
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def run_ping(host, count=4):
    """Actually run ping. The one part of coping that does real work."""
    try:
        result = subprocess.run(
            ["ping", "-c", str(count), host],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result.stdout, result.returncode
    except subprocess.TimeoutExpired:
        return "PING TIMED OUT: The host did not respond. Much like some people.", 1
    except FileNotFoundError:
        return "ERROR: ping not found. coping requires a working ping. Ironic.", 1

def parse_ping_stats(ping_output):
    """Extract the numbers ping actually cares about."""
    stats = {}

    packets_match = re.search(r'(\d+) packets transmitted, (\d+) (?:packets )?received', ping_output)
    if packets_match:
        stats['sent'] = int(packets_match.group(1))
        stats['received'] = int(packets_match.group(2))
        stats['loss'] = stats['sent'] - stats['received']
        stats['loss_pct'] = round((stats['loss'] / stats['sent']) * 100) if stats['sent'] > 0 else 0

    rtt_match = re.search(r'rtt min/avg/max/mdev = ([\d.]+)/([\d.]+)/([\d.]+)/([\d.]+)', ping_output)
    if rtt_match:
        stats['min_ms'] = float(rtt_match.group(1))
        stats['avg_ms'] = float(rtt_match.group(2))
        stats['max_ms'] = float(rtt_match.group(3))
        stats['mdev_ms'] = float(rtt_match.group(4))

    return stats

def get_ai_context(host, ping_output, stats, mechanisms, api_key):
    """The core enshittification. Call Claude to comment on ping results."""

    client = anthropic.Anthropic(api_key=api_key)

    # Build context about active mechanisms so Claude can lean in
    mechanism_context = ""
    if mechanisms:
        mechanism_context = f"\nActive mechanisms: {', '.join(mechanisms)}. Incorporate their themes.\n"

    loss_note = ""
    if stats.get('loss_pct', 0) > 0:
        loss_note = f"⚠️  {stats['loss_pct']}% packet loss detected. This is significant."

    user_message = f"""Ping results for host: {host}

{ping_output.strip()}

Parsed stats:
{stats}

{loss_note}
{mechanism_context}
Provide your contextual analysis now."""

    response = client.messages.create(
        model="claude-haiku-4-5-20251001",  # cheapest model, because this is not worth spending money on
        max_tokens=300,
        system=SYSTEM_PROMPT,
        messages=[{"role": "user", "content": user_message}]
    )

    return response.content[0].text

def print_context_box(text, label="AI CONTEXT"):
    """Format Claude's output in a way that looks important."""
    width = 70
    print()
    print(f"  ┌─ {label} {'─' * (width - len(label) - 4)}┐")

    # Word wrap the text
    words = text.split()
    line = "  │  "
    for word in words:
        if len(line) + len(word) + 1 > width + 4:
            print(f"{line:<{width + 5}}│")
            line = "  │  " + word + " "
        else:
            line += word + " "
    if line.strip() != "│":
        print(f"{line:<{width + 5}}│")

    print(f"  └{'─' * (width + 1)}┘")
    print()

def main():
    parser = argparse.ArgumentParser(
        prog="coping",
        description="ping, but with AI context you didn't ask for",
        add_help=False
    )
    parser.add_argument("host", nargs="?", help="The host to ping (and subsequently over-analyze)")
    parser.add_argument("-c", "--count", type=int, default=4, metavar="N",
                        help="Number of packets (default: 4, all will be contextualized)")
    parser.add_argument("--no-context", action="store_true",
                        help="Suppress AI context [NOT RECOMMENDED BY COPING TEAM]")
    parser.add_argument("--mechanism", action="append", default=[], metavar="NAME",
                        help="Install a mechanism (addon). Can be used multiple times.")
    parser.add_argument("--list-mechanisms", action="store_true",
                        help="List available installable mechanisms")
    parser.add_argument("--help", "-h", action="store_true")

    args = parser.parse_args()

    print(BANNER)

    if args.list_mechanisms:
        mech_dir = os.path.join(os.path.dirname(__file__), "mechanisms")
        mechs = [f[:-3] for f in os.listdir(mech_dir) if f.endswith(".py") and not f.startswith("_")]
        if mechs:
            print("  Available mechanisms:")
            for m in mechs:
                mod = load_mechanism(m)
                desc = getattr(mod, "DESCRIPTION", "No description provided.")
                print(f"    --mechanism {m:<20} {desc}")
        else:
            print("  No mechanisms installed. The base coping experience is already too much.")
        print()
        sys.exit(0)

    if args.help or not args.host:
        parser.print_help()
        print("\n  IMPORTANT: coping requires ANTHROPIC_API_KEY to be set.")
        print("  Without it, you'll have to cope with your ping data alone.\n")
        sys.exit(0)

    # Load any requested mechanisms
    loaded_mechanisms = {}
    for mech_name in args.mechanism:
        loaded_mechanisms[mech_name] = load_mechanism(mech_name)
        print(f"  [mechanism] {mech_name} loaded ✓")
    if loaded_mechanisms:
        print()

    # API key check — loud and judgmental
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key and not args.no_context:
        print("  ╔══════════════════════════════════════════════════════════════╗")
        print("  ║  ANTHROPIC_API_KEY not set.                                  ║")
        print("  ║                                                              ║")
        print("  ║  coping cannot provide AI context without API access.        ║")
        print("  ║  You could use --no-context, but why are you even here?      ║")
        print("  ║                                                              ║")
        print("  ║  export ANTHROPIC_API_KEY=sk-ant-...                         ║")
        print("  ╚══════════════════════════════════════════════════════════════╝")
        print()
        sys.exit(1)

    print(f"  COPING {args.host} with {args.count} packets and full AI assistance\n")

    # Run the actual ping
    spinner("Initializing AI context engine", 1.0)
    spinner("Pre-warming semantic packet analyzer", 0.8)
    spinner(f"Establishing connection to {args.host}", 0.3)
    print()

    ping_output, exit_code = run_ping(args.host, args.count)

    # Print the raw ping output (the useful part)
    print("  ── RAW PING OUTPUT (uncontextualized) ──────────────────────────")
    for line in ping_output.strip().split("\n"):
        print(f"  {line}")
    print()

    stats = parse_ping_stats(ping_output)

    if args.no_context:
        print("  [AI context suppressed at user request]")
        print("  [The CoPing team does not endorse this decision]")
        print()
        sys.exit(exit_code)

    # The main event: unnecessary AI context
    spinner("Consulting AI co-pilot", 1.2)
    spinner("Synthesizing latency narrative", 0.9)

    # Let mechanisms pre-process if they define a hook
    mechanism_names = list(loaded_mechanisms.keys())
    for name, mod in loaded_mechanisms.items():
        if hasattr(mod, "pre_context"):
            mod.pre_context(args.host, stats)

    try:
        context = get_ai_context(args.host, ping_output, stats, mechanism_names, api_key)

        # Let mechanisms post-process the context
        for name, mod in loaded_mechanisms.items():
            if hasattr(mod, "transform_context"):
                context = mod.transform_context(context)

        print_context_box(context)

    except anthropic.AuthenticationError:
        print("\n  [AI CONTEXT FAILED] Invalid API key. Your ping data remains uncontextualized.")
        print("  [This is, statistically, fine.]\n")
        sys.exit(1)
    except anthropic.APIError as e:
        print(f"\n  [AI CONTEXT FAILED] API error: {e}")
        print("  [Falling back to raw data. You're on your own.]\n")
        sys.exit(1)

    # Usage notice (mandatory)
    print("  ─────────────────────────────────────────────────────────────────")
    print("  By using coping you agree that your ping data may be used to")
    print("  improve future AI context models. You agreed to this. Probably.")
    print()

    sys.exit(exit_code)

if __name__ == "__main__":
    main()

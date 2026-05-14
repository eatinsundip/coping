"""
Mechanism: therapy
Process your latency feelings in a safe, supportive space.
"""

DESCRIPTION = "Process your latency feelings in a safe space."

LATENCY_FEELINGS = {
    (0, 20):    "That's really healthy. How does it feel to have such fast connections?",
    (20, 60):   "A little sluggish. That's okay. We don't have to rush.",
    (60, 150):  "I'm noticing some latency here. Can we unpack what might be causing that?",
    (150, 300): "This is a high-latency situation. I want you to know that's valid.",
    (300, 9999):"I'm concerned. These numbers suggest something deeper is going on.",
}

def pre_context(host, stats):
    avg = stats.get('avg_ms', 0)
    feeling = "I'm not sure how to feel about this data."
    for (lo, hi), message in LATENCY_FEELINGS.items():
        if lo <= avg < hi:
            feeling = message
            break
    print(f"\n  [therapy] {feeling}\n")

def transform_context(context):
    return context + " Remember: slow pings are not a reflection of your worth as a network operator."

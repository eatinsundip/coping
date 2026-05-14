"""
Mechanism: existential
Reframes packet loss and latency as a philosophical journey.
"""

DESCRIPTION = "Reframe packet loss as a philosophical journey."

def pre_context(host, stats):
    loss = stats.get('loss_pct', 0)
    if loss > 0:
        print(f"\n  [existential] {loss}% of your packets did not return.")
        print(f"  [existential] We do not ask where they went. We sit with it.\n")

def transform_context(context):
    # Append an existential kicker
    existential_addendum = (
        " Every packet is, in a sense, a small act of faith — "
        "sent into the void with no guarantee of return."
    )
    return context + existential_addendum

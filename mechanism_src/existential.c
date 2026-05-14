/* existential mechanism
 * Reframes packet loss and latency as a philosophical journey.
 * Build: gcc -shared -fPIC -o existential.so existential.c
 */
#include <stdio.h>
#include <string.h>

const char *mech_name        = "existential";
const char *mech_description = "Reframe packet loss as a philosophical journey.";

void mech_on_load(void)
{
    /* nothing to init */
}

void mech_pre_context(const char *host, long sent, long recv,
                      long tmin_us, long tmax_us)
{
    (void)tmin_us; (void)tmax_us;
    long loss = sent - recv;
    if (loss > 0) {
        printf("\n  [existential] %ld of your packets did not return.\n", loss);
        printf("  [existential] We do not ask where they went. We sit with it.\n\n");
    } else {
        printf("\n  [existential] All packets returned from %s.\n", host);
        printf("  [existential] Not everything that leaves us is lost.\n\n");
    }
}

void mech_transform_prompt(char *buf, size_t bufsize)
{
    strncat(buf,
        "Apply an existential lens: each packet is a small act of faith sent "
        "into the void with no guarantee of return. ",
        bufsize - strlen(buf) - 1);
}

void mech_post_context(const char *ai_text)
{
    (void)ai_text;
    printf("  [existential] ...and yet, the network persists.\n\n");
}

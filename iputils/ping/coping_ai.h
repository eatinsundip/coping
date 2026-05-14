/* SPDX-License-Identifier: BSD-3-Clause
 * coping - ping, but with AI context you didn't ask for
 *
 * coping_ai.h - Claude API integration + mechanism (addon) system
 */

#ifndef COPING_AI_H
#define COPING_AI_H

#define COPING_VERSION     "0.1.0-enshittified"
#define COPING_MAX_MECHS   8
#define COPING_MECH_NAMELEN 64

/* Global coping configuration, populated by argv pre-scan in main() */
typedef struct {
    int  no_context;                          /* --no-context flag */
    int  mechanism_count;
    char mechanism_names[COPING_MAX_MECHS][COPING_MECH_NAMELEN];
} coping_config_t;

extern coping_config_t coping_cfg;

/*
 * coping_init() — Load mechanisms, validate API key.
 * Call after argv pre-scan, before ping runs.
 */
void coping_init(const char *api_key);

/*
 * coping_add_context() — The whole point. Called at end of finish().
 * Sends ping stats to Claude, prints the unnecessary analysis.
 *
 * tmin_us / tmax_us / tsum_us are in microseconds (matching ping_rts internals).
 * nreceived == 0 means total packet loss — Claude will have thoughts about this.
 */
void coping_add_context(
    const char *hostname,
    long        ntransmitted,
    long        nreceived,
    long        tmin_us,
    long        tmax_us,
    double      tsum_us
);

/* Mechanism dlopen handle + vtable */
typedef struct {
    void       *handle;       /* dlopen handle */
    const char *name;
    const char *description;
    /* optional hooks — NULL if the .so doesn't export them */
    void (*on_load)(void);
    void (*pre_context)(const char *host, long sent, long recv,
                        long tmin_us, long tmax_us);
    void (*transform_prompt)(char *buf, size_t bufsize);
    void (*post_context)(const char *ai_text);
} coping_mechanism_t;

#endif /* COPING_AI_H */

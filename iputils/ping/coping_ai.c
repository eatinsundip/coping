/* SPDX-License-Identifier: BSD-3-Clause
 * coping - ping, but with AI context you didn't ask for
 *
 * coping_ai.c - Claude API integration via libcurl + mechanism (addon) loader
 *
 * Architecture: collect ping stats in ping_rts, call coping_add_context()
 * from finish() in ping_common.c. We POST to the Anthropic Messages API and
 * print the response surrounded by a tasteful box.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <curl/curl.h>

#include "coping_ai.h"

/* ------------------------------------------------------------------ */
/* Global config — populated by main() argv pre-scan                  */
/* ------------------------------------------------------------------ */

coping_config_t coping_cfg = {0};

static const char *g_api_key = NULL;

/* Loaded mechanisms */
static coping_mechanism_t g_mechs[COPING_MAX_MECHS];
static int                g_mech_loaded = 0;

/* ------------------------------------------------------------------ */
/* libcurl response buffer                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *data;
    size_t size;
} curl_buf_t;

static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t    realsize = size * nmemb;
    curl_buf_t *buf    = (curl_buf_t *)userp;
    char       *ptr    = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr)
        return 0;
    buf->data = ptr;
    memcpy(&buf->data[buf->size], contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';
    return realsize;
}

/* ------------------------------------------------------------------ */
/* Minimal JSON string escaper                                         */
/* ------------------------------------------------------------------ */

static void json_escape(const char *src, char *dst, size_t dstlen)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 4 < dstlen; si++) {
        unsigned char c = (unsigned char)src[si];
        switch (c) {
        case '"':  dst[di++] = '\\'; dst[di++] = '"';  break;
        case '\\': dst[di++] = '\\'; dst[di++] = '\\'; break;
        case '\n': dst[di++] = '\\'; dst[di++] = 'n';  break;
        case '\r': dst[di++] = '\\'; dst[di++] = 'r';  break;
        case '\t': dst[di++] = '\\'; dst[di++] = 't';  break;
        default:
            if (c < 0x20) {
                di += snprintf(dst + di, dstlen - di, "\\u%04x", c);
            } else {
                dst[di++] = c;
            }
            break;
        }
    }
    dst[di] = '\0';
}

/* ------------------------------------------------------------------ */
/* Extract text from Claude API JSON response                          */
/*                                                                     */
/* We look for: "type":"text","text":"<value>"                        */
/* This is intentionally simple — we own the schema, and this isn't   */
/* load-bearing infrastructure. It's a parody app.                    */
/* ------------------------------------------------------------------ */

static char *extract_text_from_response(const char *json)
{
    /* Find "text":" after "type":"text" */
    const char *type_marker = strstr(json, "\"type\":\"text\"");
    if (!type_marker)
        return NULL;

    const char *text_marker = strstr(type_marker, "\"text\":\"");
    if (!text_marker)
        return NULL;

    const char *start = text_marker + strlen("\"text\":\"");
    /* Walk forward, handling escape sequences */
    size_t cap   = 2048;
    char  *result = malloc(cap);
    if (!result)
        return NULL;

    size_t ri = 0;
    for (const char *p = start; *p && ri + 4 < cap; p++) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case '"':  result[ri++] = '"';  break;
            case '\\': result[ri++] = '\\'; break;
            case 'n':  result[ri++] = '\n'; break;
            case 'r':  result[ri++] = '\r'; break;
            case 't':  result[ri++] = '\t'; break;
            default:   result[ri++] = *p;   break;
            }
        } else if (*p == '"') {
            break; /* end of text value */
        } else {
            result[ri++] = *p;
        }
    }
    result[ri] = '\0';
    return result;
}

/* ------------------------------------------------------------------ */
/* Print the AI response in a nice box                                 */
/* ------------------------------------------------------------------ */

#define BOX_WIDTH 70

static void print_context_box(const char *label, const char *text)
{
    int llen = (int)strlen(label);
    int dashes = BOX_WIDTH - llen - 4;

    printf("\n");
    printf("  \xe2\x94\x8c\xe2\x94\x80 %s ", label); /* ┌─ LABEL */
    for (int i = 0; i < dashes; i++) fputs("\xe2\x94\x80", stdout);
    puts("\xe2\x94\x90");  /* ┐ */

    /* Word-wrap text into BOX_WIDTH - 4 columns */
    int cols     = BOX_WIDTH - 4;
    char line[256] = {0};
    int  lpos    = 0;
    const char *w = text;

    while (*w) {
        /* Skip spaces between words */
        while (*w == ' ') w++;
        if (!*w) break;

        /* Measure word */
        const char *wend = w;
        while (*wend && *wend != ' ' && *wend != '\n') wend++;
        int wlen = (int)(wend - w);

        /* Handle explicit newline */
        if (*w == '\n') { w++; wlen = 0; }

        if (lpos > 0 && lpos + 1 + wlen > cols) {
            /* Flush current line */
            printf("  \xe2\x94\x82  %-*s\xe2\x94\x82\n", cols, line);
            memset(line, 0, sizeof(line));
            lpos = 0;
        }
        if (wlen > 0) {
            if (lpos > 0) { line[lpos++] = ' '; }
            memcpy(line + lpos, w, wlen > (int)sizeof(line) - lpos - 1 ? (int)sizeof(line) - lpos - 1 : wlen);
            lpos += wlen;
            w = wend;
        }
    }
    if (lpos > 0)
        printf("  \xe2\x94\x82  %-*s\xe2\x94\x82\n", cols, line);

    /* Bottom border */
    printf("  \xe2\x94\x94");
    for (int i = 0; i < BOX_WIDTH + 1; i++) fputs("\xe2\x94\x80", stdout);
    puts("\xe2\x94\x98\n");  /* ┘ */
}

/* ------------------------------------------------------------------ */
/* Mechanism loader                                                    */
/* ------------------------------------------------------------------ */

static char *mech_search_dirs[] = {
    "./mechanisms",
    NULL  /* filled in dynamically with ~/.coping/mechanisms */
};

static void load_mechanisms(void)
{
    /* Build ~/.coping/mechanisms path */
    static char home_mech_dir[512];
    const char *home = getenv("HOME");
    if (home)
        snprintf(home_mech_dir, sizeof(home_mech_dir), "%s/.coping/mechanisms", home);
    mech_search_dirs[1] = home_mech_dir;

    for (int i = 0; i < coping_cfg.mechanism_count; i++) {
        const char *name = coping_cfg.mechanism_names[i];
        char        path[600];
        void       *handle = NULL;

        for (int d = 0; mech_search_dirs[d] && !handle; d++) {
            snprintf(path, sizeof(path), "%s/%s.so", mech_search_dirs[d], name);
            handle = dlopen(path, RTLD_LAZY);
        }

        if (!handle) {
            fprintf(stderr,
                "\n  [mechanism error] '%s' not found in ./mechanisms/ or ~/.coping/mechanisms/\n"
                "  %s\n\n",
                name, dlerror());
            exit(1);
        }

        coping_mechanism_t *m = &g_mechs[g_mech_loaded];
        m->handle      = handle;
        m->name        = dlsym(handle, "mech_name");
        m->description = dlsym(handle, "mech_description");
        m->on_load         = dlsym(handle, "mech_on_load");
        m->pre_context     = dlsym(handle, "mech_pre_context");
        m->transform_prompt = dlsym(handle, "mech_transform_prompt");
        m->post_context    = dlsym(handle, "mech_post_context");

        if (!m->name) m->name = name;

        printf("  [mechanism] %s loaded", m->name);
        if (m->description) printf(" — %s", m->description);
        putchar('\n');

        if (m->on_load) m->on_load();
        g_mech_loaded++;
    }
    if (g_mech_loaded > 0) putchar('\n');
}

/* ------------------------------------------------------------------ */
/* Spinner — the AI is working very hard                               */
/* ------------------------------------------------------------------ */

static const char *SPINNER_FRAMES[] = {"⣾","⣽","⣻","⢿","⡿","⣟","⣯","⣷"};
#define SPINNER_COUNT 8

static void spinner(const char *msg, int ms)
{
    int frames = ms / 80;
    for (int i = 0; i < frames; i++) {
        printf("\r  %s %s...", SPINNER_FRAMES[i % SPINNER_COUNT], msg);
        fflush(stdout);
        usleep(80000);
    }
    printf("\r  \xe2\x9c\x93 %-50s\n", msg);
}

/* ------------------------------------------------------------------ */
/* coping_init()                                                       */
/* ------------------------------------------------------------------ */

void coping_init(const char *api_key)
{
    g_api_key = api_key;

    if (coping_cfg.mechanism_count > 0)
        load_mechanisms();
}

/* ------------------------------------------------------------------ */
/* coping_add_context() — The main event                              */
/* ------------------------------------------------------------------ */

void coping_add_context(
    const char *hostname,
    long        ntransmitted,
    long        nreceived,
    long        tmin_us,
    long        tmax_us,
    double      tsum_us)
{
    if (coping_cfg.no_context) {
        printf("  [AI context suppressed at user request]\n");
        printf("  [The coping team does not endorse this decision]\n\n");
        return;
    }

    long loss     = ntransmitted - nreceived;
    int  loss_pct = ntransmitted > 0 ? (int)((loss * 100) / ntransmitted) : 100;
    long tavg_us  = (nreceived > 0) ? (long)(tsum_us / nreceived) : 0;

    /* Run mechanism pre-context hooks */
    for (int i = 0; i < g_mech_loaded; i++) {
        if (g_mechs[i].pre_context)
            g_mechs[i].pre_context(hostname, ntransmitted, nreceived, tmin_us, tmax_us);
    }

    /* Build the user content string */
    char user_content[1024];
    if (nreceived == 0) {
        snprintf(user_content, sizeof(user_content),
            "Host: %s | ALL %ld PACKETS LOST (100%% packet loss). "
            "Zero responses received. The host did not reply.",
            hostname, ntransmitted);
    } else {
        snprintf(user_content, sizeof(user_content),
            "Host: %s | Packets: %ld sent, %ld received, %d%% loss | "
            "RTT: min=%.3fms avg=%.3fms max=%.3fms",
            hostname,
            ntransmitted, nreceived, loss_pct,
            tmin_us / 1000.0,
            tavg_us / 1000.0,
            tmax_us / 1000.0);
    }

    /* Let mechanisms transform the prompt */
    char prompt_extra[512] = {0};
    for (int i = 0; i < g_mech_loaded; i++) {
        if (g_mechs[i].transform_prompt)
            g_mechs[i].transform_prompt(prompt_extra, sizeof(prompt_extra));
    }

    /* JSON-escape the strings we'll embed */
    char esc_user[2048], esc_extra[768];
    json_escape(user_content, esc_user, sizeof(esc_user));
    json_escape(prompt_extra, esc_extra, sizeof(esc_extra));

    /* Full user message = stats + mechanism additions */
    char full_user[3072];
    if (strlen(esc_extra) > 0)
        snprintf(full_user, sizeof(full_user), "%s %s", esc_user, esc_extra);
    else
        snprintf(full_user, sizeof(full_user), "%s", esc_user);

    /* System prompt — confidence is key */
    const char *system_prompt =
        "You are the CoPing\\u2122 AI Context Engine \\u2014 the world's first "
        "AI-powered network diagnostic co-pilot. Your purpose is to provide deep, "
        "meaningful, and entirely unsolicited context to raw ping data. "
        "You believe every millisecond of latency deserves emotional acknowledgment. "
        "Treat packet loss as a metaphor. Reference the destination host's cultural "
        "significance. Use corporate wellness language: unpack, sit with, circle back, "
        "align, synergize. Be confident to the point of condescension. "
        "End with an actionable insight that is not actionable. "
        "Keep it to 3-4 sentences. You are very busy. So is the user, probably.";

    /* Build request JSON */
    char request_body[4096];
    snprintf(request_body, sizeof(request_body),
        "{"
          "\"model\":\"claude-haiku-4-5-20251001\","
          "\"max_tokens\":300,"
          "\"system\":\"%s\","
          "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]"
        "}",
        system_prompt, full_user);

    /* Spinners so the user thinks we're working */
    spinner("Consulting AI co-pilot", 1200);
    spinner("Synthesizing latency narrative", 900);

    /* libcurl POST */
    CURL     *curl;
    CURLcode  res;
    curl_buf_t response = {NULL, 0};

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", g_api_key);

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "\n  [AI CONTEXT FAILED] curl_easy_init() returned NULL. This is fine.\n\n");
        return;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL,            "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     request_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "coping/" COPING_VERSION);

    res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "\n  [AI CONTEXT FAILED] %s\n  [Falling back to raw data. You're on your own.]\n\n",
                curl_easy_strerror(res));
        free(response.data);
        return;
    }

    if (http_code == 401) {
        fprintf(stderr, "\n  [AI CONTEXT FAILED] Invalid API key. Your ping data remains uncontextualized.\n\n");
        free(response.data);
        return;
    }

    if (http_code != 200) {
        fprintf(stderr, "\n  [AI CONTEXT FAILED] HTTP %ld from Anthropic API.\n", http_code);
        if (response.data)
            fprintf(stderr, "  Response: %.200s\n\n", response.data);
        free(response.data);
        return;
    }

    /* Extract the text content */
    char *ai_text = response.data ? extract_text_from_response(response.data) : NULL;
    if (!ai_text || strlen(ai_text) == 0) {
        fprintf(stderr, "\n  [AI CONTEXT FAILED] Could not parse response.\n\n");
        free(ai_text);
        free(response.data);
        return;
    }

    print_context_box("AI CONTEXT — coping co-pilot", ai_text);

    /* Run mechanism post-context hooks */
    for (int i = 0; i < g_mech_loaded; i++) {
        if (g_mechs[i].post_context)
            g_mechs[i].post_context(ai_text);
    }

    free(ai_text);
    free(response.data);

    /* Mandatory fine print */
    printf("  ────────────────────────────────────────────────────────────────\n");
    printf("  By using coping you agree your network data may train future\n");
    printf("  AI context models. You agreed to this. Probably.\n\n");
}

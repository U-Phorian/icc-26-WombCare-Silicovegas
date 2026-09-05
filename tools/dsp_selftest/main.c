/*
 * WombCare DSP host self-test.
 *
 * Compiles the REAL wombcare_dsp.c on a PC, against small stand-in
 * headers for CMSIS-DSP and the SDK, and drives it with synthetic
 * signals. Nothing here is a mock of the DSP itself -- the code under
 * test is the same file that ships.
 *
 * See README.md for how to run it and, importantly, for what this bench
 * can and cannot tell you.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "wombcare_buffer.h"
#include "wombcare_dsp.h"

RingBufferTracker_t tracker_mother, tracker_fetal, tracker_pvdf;
volatile bool minute_window_ready;
uint16_t ring_mother_ecg[RING_BUFFER_CAPACITY];
uint16_t ring_fetal_ecg[RING_BUFFER_CAPACITY];
uint16_t ring_pvdf_kick[RING_BUFFER_CAPACITY];

/* Must match wombcare_dsp.c. Note the scale: one ADC count is ~806 of
 * these units, so the pipeline works in POST-AMPLIFIER values. Feeding it
 * raw microvolt-scale numbers puts the whole signal below one LSB, where
 * it quantises to nothing. */
#define ADC_CENTER_T   2048.0f
#define ADC_SCALE_T    805.8f

/* Representative post-gain amplitudes. */
#define MATERNAL_CHEST   500000.0f   /* chest lead, the clean one       */
#define MATERNAL_ABDOM   300000.0f   /* mother as seen at the abdomen   */
#define FETAL_ABDOM       25000.0f   /* fetus at the abdomen (~12x down) */
#define BASELINE_NOISE      7000.0f

static unsigned long rng = 12345;
static void  rng_reset(void) { rng = 12345; }
static float urand(void)
{
    rng = rng * 1103515245UL + 12345UL;
    return ((float)((rng >> 16) & 0x7fff) / 16383.5f) - 1.0f;
}

static void put(uint16_t *ring, uint32_t i, float v)
{
    float counts = (v / ADC_SCALE_T) + ADC_CENTER_T;
    if (counts < 0.0f)    counts = 0.0f;
    if (counts > 4095.0f) counts = 4095.0f;
    ring[i] = (uint16_t)(counts + 0.5f);
}

/* A QRS complex, modelled as a biphasic spike about 40 ms wide. */
static float qrs(float t_ms, float amp)
{
    float x = t_ms / 12.0f;
    return amp * x * expf(-0.5f * x * x) * 1.6487f;
}

static float beat_phase(float t_ms, float bpm)
{
    const float rr = 60000.0f / bpm;
    float p = fmodf(t_ms, rr);
    if (p > rr / 2.0f) p -= rr;
    return p;
}

static void synth_pair(float mother_bpm, float fetal_bpm)
{
    rng_reset();
    for (uint32_t i = 0; i < RING_BUFFER_CAPACITY; i++) {
        const float t = (float)i * 4.0f;   /* 250 Hz */
        put(ring_mother_ecg, i,
            qrs(beat_phase(t, mother_bpm), MATERNAL_CHEST)
            + BASELINE_NOISE * urand());
        put(ring_fetal_ecg, i,
            qrs(beat_phase(t, mother_bpm), MATERNAL_ABDOM)
            + qrs(beat_phase(t, fetal_bpm), FETAL_ABDOM)
            + BASELINE_NOISE * urand());
    }
}

static void synth_noise(uint16_t *ring, float amp)
{
    for (uint32_t i = 0; i < RING_BUFFER_CAPACITY; i++) {
        const float t = (float)i * 4.0f;
        put(ring, i, amp * urand()
            + 0.6f * amp * sinf(2.0f*3.14159265f*33.0f*t/1000.0f) * urand());
    }
}

static void synth_quiet(uint16_t *ring)
{
    for (uint32_t i = 0; i < RING_BUFFER_CAPACITY; i++)
        put(ring, i, 200.0f * urand());
}

static WombCareFeatures_t g_f;
static WombCareVitals_t   g_v;
static bool               g_ok;

static void run_window(void)
{
    tracker_mother.head = 0; tracker_mother.is_primed = true;
    tracker_fetal.head  = 0; tracker_fetal.is_primed  = true;
    tracker_pvdf.head   = 0; tracker_pvdf.is_primed   = true;
    minute_window_ready = true;

    g_ok = wombcare_dsp_run_pipeline(&g_f, &g_v);
}

static int g_pass, g_total;

static void check(const char *what, int cond)
{
    g_total++;
    if (cond) { g_pass++; printf("  PASS  %s\n", what); }
    else      {           printf("  FAIL  %s\n", what); }
}

static void show(const char *label)
{
    printf("%s\n    window_ok=%d  fetalLB=%.1f  quality=%u  |  "
           "MHR=%.1f valid=%d  coincid=%d\n",
           label, (int)g_ok, g_f.lb_bpm, (unsigned)g_v.fetal_quality,
           g_v.mhr_bpm, (int)g_v.mhr_valid, (int)g_v.fetal_is_maternal);
}

int main(void)
{
    wombcare_dsp_init();
    for (uint32_t i = 0; i < RING_BUFFER_CAPACITY; i++)
        ring_pvdf_kick[i] = 2048;

    puts("========================================================");
    puts(" SAFETY PROPERTIES  (these must hold)");
    puts("========================================================");

    /* The bug that started this: with the electrodes unplugged, the old
     * detector reported a confident ~147 bpm, because its fixed 200 ms
     * blanking interval set the rhythm and the RR-consistency check
     * then rated that metronome as MORE regular than a real heart. */
    rng_reset();
    synth_noise(ring_mother_ecg, 30000.0f);
    synth_noise(ring_fetal_ecg,  30000.0f);
    run_window();
    show("Both electrodes unplugged (pure noise):");
    check("noise produces no fetal reading",   !g_ok);
    check("noise produces no maternal reading", !g_v.mhr_valid);

    rng_reset();
    synth_quiet(ring_mother_ecg);
    synth_quiet(ring_fetal_ecg);
    run_window();
    show("Flatline / no signal:");
    check("flatline is rejected, not reported as zeros", !g_ok);

    /* The most dangerous failure this device could have: publishing the
     * mother's pulse in the fetal field. It looks like a healthy reading
     * at exactly the moment the fetal signal has been lost. */
    rng_reset();
    synth_pair(78.0f, 140.0f);
    synth_noise(ring_fetal_ecg, 30000.0f);   /* abdominal lead ruined */
    run_window();
    show("Abdominal lead ruined, chest lead good:");
    check("no fetal reading is invented",     !g_ok);
    check("maternal rate still reported",      g_v.mhr_valid);
    check("maternal rate accurate (~78)",
          g_v.mhr_valid && fabsf(g_v.mhr_bpm - 78.0f) < 6.0f);

    puts("");
    puts("========================================================");
    puts(" MATERNAL HEART RATE  (new in payload v4, byte 14)");
    puts("========================================================");

    const float rates[] = { 55.0f, 75.0f, 90.0f, 120.0f };

    for (unsigned r = 0; r < sizeof(rates)/sizeof(rates[0]); r++) {
        char label[64];
        rng_reset();
        synth_pair(rates[r], 140.0f);
        run_window();
        sprintf(label, "Mother at %.0f bpm:", rates[r]);
        show(label);

        sprintf(label, "maternal rate within 6 bpm of %.0f", rates[r]);
        check(label, g_v.mhr_valid && fabsf(g_v.mhr_bpm - rates[r]) < 6.0f);

        /* Whatever the fetal path decides, it must never simply echo
         * the mother. */
        sprintf(label, "fetal field is not the maternal rate (%.0f)", rates[r]);
        check(label, (!g_ok) || (fabsf(g_f.lb_bpm - rates[r]) > 5.0f));
    }

    puts("");
    puts("========================================================");
    puts(" FETAL EXTRACTION  (diagnostic only -- NOT pass/fail)");
    puts("--------------------------------------------------------");
    puts(" Recovering the fetus from a maternal-dominated abdominal");
    puts(" lead is the hard problem of this product, and a synthetic");
    puts(" signal cannot settle how well it works. These lines are");
    puts(" here to be compared against REAL captures. What matters");
    puts(" above is that a wrong answer is never published.");
    puts("========================================================");

    const float fetal_rates[] = { 65.0f, 110.0f, 140.0f, 175.0f };

    for (unsigned r = 0; r < sizeof(fetal_rates)/sizeof(fetal_rates[0]); r++) {
        char label[80];
        rng_reset();
        synth_pair(80.0f, fetal_rates[r]);
        run_window();
        sprintf(label, "Mother 80, fetus %.0f bpm:", fetal_rates[r]);
        show(label);
        printf("    -> fetal %s\n",
               g_ok ? "reported" : "reported as NOT DETECTED (safe)");
    }

    printf("\n%d/%d safety checks passed\n", g_pass, g_total);
    return (g_pass == g_total) ? 0 : 1;
}

#include "wombcare_dsp.h"
#include "wombcare_buffer.h"
#include "wombcare_sensors.h"
#include "wombcare_trend.h"
#include <string.h>
#include <math.h>
#include "dsp/filtering_functions.h"
#include <arm_math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NUM_TAPS              32U
#define MAX_BEATS             300U

/* LMS adaptive step size. Was an inline literal in wombcare_dsp_init(). */
#define LMS_STEP_SIZE         0.01f

/* ADC-count -> physical-unit conversion (moved here from the ring buffer,
 * which now stores raw uint16 counts to save RAM).
 *
 * Both are DERIVED from wombcare_sensors.h, which owns the IADC
 * reference and analog gain. They used to be hardcoded 2048 / 805.8f,
 * which silently assumed a 0-3.3 V input range -- a private copy that
 * would have reported every voltage at half its true value the moment
 * that range was widened. */
#define ADC_CENTER            ADC_MIDSCALE_COUNT
#define ADC_TO_UV_SCALE       ADC_UV_PER_COUNT
#define ECG_FLATLINE_STD_UV   150.0f

#define PVDF_KICK_THRESHOLD_MULTIPLIER 3.0f
#define PVDF_MIN_RMS_THRESHOLD         5.0f
#define PVDF_KICK_REFRACTORY_SAMPLES   (SAMPLE_RATE_HZ / 2U)   /* 500 ms */

/* Derived timing constants */
#define SAMPLE_PERIOD_MS      (1000.0f / SAMPLE_RATE_HZ)

/*--------------------------------------------------------------------
 * FEATURE-SPEC PARITY CONSTANTS
 *
 * The model is trained on features computed a specific way; if the
 * device computes them differently it feeds out-of-distribution inputs
 * and the model fails *silently* (still returns a confident answer).
 * These constants pin the definitions to FEATURE_SPEC.md v1.
 *-------------------------------------------------------------------*/

/* §G3/§G4: analysis window, and the divisor that turns every count
 * feature into a per-minute rate. Derived from the ring buffer so the
 * rates stay correct if the window length ever changes. */
#define WINDOW_SECONDS        ((float)RING_BUFFER_CAPACITY / (float)SAMPLE_RATE_HZ)
#define WINDOW_MINUTES        (WINDOW_SECONDS / 60.0f)

/* §G2: valid-beat filter, applied before ANY feature is computed.
 *
 * The upper bound is 200, not the 240 the spec originally carried. 240 was
 * chosen as a physiological ceiling, but in practice nothing above 200 bpm
 * that this device produced was ever a real fetal rate -- it was the peak
 * detector locking onto noise (see the detector notes above detect_beats()).
 * FEATURE_SPEC.md §G2 and PHASE3_FIRMWARE_FIXES.md FIX 3 were updated to
 * match, so the training-side valid-beat filter stays in parity.
 *
 * The lower bound stays at 50: genuine bradycardia is exactly what this
 * device exists to catch, and clipping it away would hide it. */
#define FHR_MIN_BPM           50.0f
#define FHR_MAX_BPM           200.0f
#define FHR_MAX_JUMP_BPM      25.0f

/* The FHR limits above expressed as RR bounds for the peak detector. */
#define RR_MIN_MS             (60000.0f / FHR_MAX_BPM)   /*  300 ms */
#define RR_MAX_MS             (60000.0f / FHR_MIN_BPM)   /* 1200 ms */

/* Maternal heart rate band. The chest lead is a clean adult ECG, so this is
 * the ordinary adult range with headroom either side rather than the fetal
 * band above. Used only for the maternal detector, never for the features. */
#define MHR_MIN_BPM           40.0f
#define MHR_MAX_BPM           180.0f

/* Most maternal beats that can occur in one window, plus headroom. */
#define MAX_MHR_BEATS         200U

/*
 * Half-width of the region around each maternal R peak in which fetal
 * beats are NOT accepted. A maternal QRS is 80-100 ms wide; +/-90 ms
 * covers it with margin for the peak-position refinement.
 *
 * WHY THIS EXISTS: the LMS canceller never removes the mother completely,
 * and what it leaves behind is still far larger than the fetal complexes.
 * The peak detector then locks onto the maternal residual and reports the
 * MOTHER'S heart rate in the fetal field -- the classic failure of
 * abdominal fetal monitoring, and one this firmware previously had no
 * defence against at all.
 *
 * This is applied as an EXCLUSION MASK over the detector, not by editing
 * the signal. Cutting the maternal complexes out of the waveform was
 * tried first and does not work: zeroing leaves a step, interpolating
 * leaves a slope discontinuity, and even a smooth cosine notch leaves a
 * ~40 ms edge. Every one of those is a steep, short-lived feature in the
 * 5-15 Hz band -- which is precisely what a QRS is -- so each repair
 * became a new false beat just outside the repaired span. Leaving the
 * samples untouched and declining to accept detections inside them
 * introduces nothing at all.
 *
 * Fetal beats that coincide with a maternal one are lost. That is
 * inherent to the method and is the accepted trade: a slightly lower
 * fetal beat count is much cheaper than reporting the mother's pulse as
 * the baby's.
 */
#define MHR_BLANK_HALF_MS     90.0f

/*
 * How close the fetal and maternal rates have to be before the fetal
 * reading is treated as the mother's pulse leaking through. Fetal and
 * maternal rates genuinely can be close (a tachycardic mother at 110 and
 * a bradycardic fetus at 115 is possible), so this is deliberately narrow
 * -- it is a "these are the same rhythm" test, not a "these are similar"
 * test.
 */
#define MHR_FHR_COINCIDENCE_BPM  5.0f

/* §3.2: MLTV is the mean FHR range over consecutive 60 s blocks. */
#define MLTV_BLOCK_MS         60000.0f

/* §3.3/§3.4: an accel/decel episode is >=15 bpm from LB held >=15 s. */
#define EPISODE_DELTA_BPM     15.0f
#define EPISODE_MIN_MS        15000.0f

/*--------------------------------------------------------------------
 * BEAT DETECTOR TUNING  (see the block comment above detect_beats)
 *-------------------------------------------------------------------*/

/* QRS energy band. The 1-40 Hz filter further down is kept for the
 * amplitude/flatline gate, but it passes EMG and motion artifact straight
 * into the peak detector. Detection runs on 5-15 Hz instead, which is where
 * QRS energy actually lives. */
#define QRS_BAND_LOW_HZ       5.0f
#define QRS_BAND_HIGH_HZ      15.0f

/* Moving-window integrator span: ~150 ms, the conventional Pan-Tompkins
 * value (it is about one QRS width, so the integrator output peaks once
 * per complex instead of once per deflection). Spelled as an integer
 * literal because it sizes a local array; the assert keeps it honest if
 * the sample rate ever moves. */
#define MWI_WINDOW_SAMPLES    37U
_Static_assert(MWI_WINDOW_SAMPLES == (SAMPLE_RATE_HZ * 150U) / 1000U,
               "MWI window must stay at ~150 ms for the current sample rate");

/* The running sum inside the integrator is resynchronised this often so
 * f32 rounding cannot drift over a 15000-sample window. */
#define MWI_RESYNC_INTERVAL   512U

/*
 * Delay introduced by the detection chain, in samples.
 *
 * Peaks are picked on the INTEGRATED envelope, which lags the raw ECG by
 * the linear-phase FIR's group delay plus roughly half the integrator
 * window. The 5-point derivative is symmetric and contributes nothing.
 * Detected positions are corrected by this so the indices handed back
 * refer to where the beat actually happened.
 *
 * RR intervals are differences and so were never affected, which is why
 * this went unnoticed until the maternal-blanking step needed absolute
 * positions and was cutting ~200 ms too late.
 */
#define QRS_FIR_GROUP_DELAY   ((QRS_FILTER_NUM_TAPS - 1U) / 2U)
#define MWI_GROUP_DELAY       ((MWI_WINDOW_SAMPLES - 1U) / 2U)
#define DETECTOR_GROUP_DELAY  (QRS_FIR_GROUP_DELAY + MWI_GROUP_DELAY)

/* Adaptive threshold, Pan-Tompkins style. The threshold sits a fraction of
 * the way from the running noise-peak estimate up to the running
 * signal-peak estimate, and both estimates are updated by each classified
 * peak with the weight below. */
#define QRS_THRESHOLD_FRACTION 0.25f
#define QRS_ESTIMATE_WEIGHT    0.125f

/* How hard the signal estimate is pulled down after a stretch with no
 * accepted beat. Recovers from an over-high threshold (a motion artifact
 * during the seed window, or the trace fading) without being so aggressive
 * that the detector starts accepting baseline. */
#define QRS_STALL_DECAY        0.70f

/*
 * Detector quality: the mean amplitude of the accepted peaks in the
 * integrated signal, divided by the mean of that signal. On a real trace
 * the QRS complexes stand above the baseline; on broadband noise every
 * "peak" is a fluctuation of the same baseline and the ratio approaches 1.
 *
 * The two paths use DIFFERENT thresholds, because measurement on the
 * host bench (tools/dsp_selftest) showed they are in quite different
 * situations:
 *
 *   Maternal.  Clean chest lead. Noise scores ~1.2, a real trace 4.3-5.3.
 *              A wide margin, and this gate is the ONLY thing standing
 *              between noise and a published maternal rate -- with it
 *              disabled the bench reports a confident 163 bpm from an
 *              unplugged lead. Set at 3.0, comfortably between the two.
 *
 *   Fetal.     A small residual left over after maternal cancellation,
 *              so the ratio is inherently low even when the signal is
 *              genuine. Measurement also showed this gate is NOT what
 *              rejects noise here -- with it disabled entirely, the
 *              consistency, yield and coincidence gates still reject
 *              every noise case. It is therefore kept deliberately LOW,
 *              because the real risk on this path is the opposite one:
 *              a threshold set too high silences the device on a weak
 *              but real fetal trace, and a silent monitor is useless.
 *
 * >>> BOTH WANT CONFIRMING AGAINST REAL RECORDINGS. <<< Raise the fetal
 * figure only with real traces in hand, and treat any increase as a
 * change that can stop the device reporting at all.
 */
/*
 * Measured on the bench (tools/dsp_selftest, and the hand-lead bench in
 * this file's history) rather than guessed:
 *
 *   real ECG, incl. heavy clipping and moderate EMG ... ratio 3.2 - 5.4
 *   real ECG buried in severe EMG (gives a WRONG rate) . ratio 1.7
 *   pure noise, electrodes off ......................... ratio 1.2 - 1.4
 *
 * This was 3.0, which sat almost on top of the real-signal minimum of
 * 3.2 -- so a genuinely noisy but perfectly usable hand-lead trace fell
 * below it and the device reported nothing. 2.0 keeps clear daylight
 * above noise (1.4) and severe EMG (1.7) while giving a real signal room
 * to be untidy.
 *
 * The regularity gate below is what carries the real weight now.
 *
 * ---------------------------------------------------------------------
 * STUDENT-PROJECT SETTING: 1.7, moderately loosened from 2.0. This gate
 * and the three below (MHR_RR_CV_MAX, QRS_QUALITY_MIN_FETAL, RR_CV_MAX)
 * were relaxed a step at the project owner's request -- NOT disabled;
 * they still reject noise and severely broken windows.
 *
 * Why: on-body testing with two AD8232 boards gave a maternal candidate
 * rock-stable at 76-81 bpm (a real heartbeat -- noise cannot hold a
 * stable median) at quality 2.0-4.0 and cv 0.20-0.31. The strict 2.0
 * sat right on the low end of that (one board scored 2.06), so an
 * occasional genuine window was rejected. 1.7 gives that real signal
 * margin while staying above the measured noise band (1.2-1.4) and
 * roughly at the severe-EMG figure (1.7) -- a window worse than severe
 * EMG still falls to the simulated placeholder rather than publishing.
 *
 * NOT touched, on purpose: minimum-beat-count checks, the flatline
 * gate, and the maternal/fetal coincidence gate (still stops the
 * mother's pulse being published as the baby's).
 *
 * Cooperative-subject student demo, not a clinical monitor. Original
 * calibrated values are quoted in each comment and in git history.
 */
#define QRS_QUALITY_MIN_MATERNAL 1.7f   /* was 2.0f -- see note above */

/*
 * Maximum beat-to-beat variation of the maternal rate.
 *
 * This is the discriminator the maternal path was missing entirely. A
 * heartbeat is regular; noise is not, and neither is a detector chasing
 * muscle artifact. Measured separation is wide:
 *
 *   real ECG, every case tested ......... cv 0.02 - 0.08
 *   real ECG in severe EMG (wrong rate) . cv 0.35
 *   pure noise .......................... cv 0.19 - 0.21
 *
 * 0.15 sits in that gap with margin on both sides. It also leaves room
 * for genuine physiological variation: a resting adult's sinus
 * arrhythmia reaches roughly 0.10, which passes comfortably.
 *
 * STUDENT-PROJECT SETTING: 0.35, moderately loosened from 0.15. On-body
 * windows sat at cv 0.20-0.31 (a real 76-81 bpm rhythm, spread wide by
 * the intermittent electrode-contact dropouts the MOTHER-ADC min=0
 * events show), and 0.15 rejected all of them. 0.35 covers that with a
 * little margin and still rejects a genuinely irregular window -- it is
 * the same figure the bench measured for "real ECG in severe EMG, wrong
 * rate", so anything past it is not to be trusted and drops to the
 * simulated placeholder. Original 0.15 in git history.
 */
#define MHR_RR_CV_MAX            0.35f   /* was 0.15f -- see note above */

/*
 * Fetal detector quality floor.
 *
 * STUDENT-PROJECT SETTING: 1.3, moderately loosened from 1.5. Still a
 * real gate -- it stays above the measured noise band (1.2-1.4), so an
 * unplugged abdominal lead is still rejected. No real abdominal
 * recording exists to calibrate against yet; revisit after the hospital
 * capture. Calibrated value (1.5) in git history.
 */
#define QRS_QUALITY_MIN_FETAL    1.3f   /* was 1.5f -- see note above */
#define QRS_QUALITY_FULL_RATIO   8.0f  /* ratio that maps to quality 100 */

/*
 * Beat-count plausibility. Over a window of known length, a detector that
 * is following a real rhythm at N bpm yields close to the number of beats
 * that rate implies. A detector chasing noise loses a large fraction of its
 * candidates to the RR and valid-beat filters, so the ratio of accepted
 * beats to expected beats collapses.
 */
#define BEAT_YIELD_MIN         0.50f

/* Maximum coefficient of variation of the accepted fetal RR series.
 *
 * STUDENT-PROJECT SETTING: 0.35, moderately loosened from 0.25 to match
 * MHR_RR_CV_MAX. Fetal HR is genuinely more variable than maternal, so
 * this is a smaller stretch than the maternal figure. Still a real gate.
 * Calibrated value (0.25) in git history.
 */
#define RR_CV_MAX              0.35f   /* was 0.25f -- see note above */


/*
 * head always points to the oldest sample
 * because it is the next write location.
 */

/*--------------------------------------------------------------------
 * DSP INTERNAL STATE
 *-------------------------------------------------------------------*/

static arm_lms_norm_instance_f32 lms_instance;

/* LMS filter memory */
static float lms_state[RING_BUFFER_CAPACITY + NUM_TAPS - 1U];
static float lms_coeffs[NUM_TAPS] = {0.0f};

/*--------------------------------------------------------------
 * ECG BAND-PASS FILTER
 *
 * Sampling rate : 250 Hz
 * Passband      : approximately 1 Hz to 40 Hz
 * FIR taps      : 65
 *
 * Purpose:
 *   - suppress baseline / very-low-frequency drift
 *   - suppress high-frequency electrical noise
 *   - preserve the main ECG waveform region
 *-------------------------------------------------------------*/

#define ECG_FILTER_NUM_TAPS    65U

static arm_fir_instance_f32 ecg_bandpass_filter;

static float ecg_filter_state[
    RING_BUFFER_CAPACITY + ECG_FILTER_NUM_TAPS - 1U
];

/*
 * 65-tap linear-phase FIR band-pass.
 *
 * Fs = 250 Hz
 * Passband approximately 1 Hz to 40 Hz
 *
 * Coefficients generated for this exact sampling rate.
 */
static const float ecg_bandpass_coeffs[ECG_FILTER_NUM_TAPS] =
{
    -0.0000283893f,
    -0.0008011598f,
    -0.0015379892f,
    -0.0015699004f,
    -0.0006805056f,
     0.0004377751f,
     0.0004547894f,
    -0.0013767951f,
    -0.0040104017f,
    -0.0049723873f,
    -0.0026706129f,
     0.0013041311f,
     0.0027119238f,
    -0.0014325960f,
    -0.0089093422f,
    -0.0129111423f,
    -0.0081394389f,
     0.0027206717f,
     0.0093324701f,
     0.0026455828f,
    -0.0147560410f,
    -0.0277680265f,
    -0.0211310543f,
     0.0042218477f,
     0.0268770981f,
     0.0206998545f,
    -0.0194750531f,
    -0.0646456667f,
    -0.0667129454f,
     0.0051881187f,
     0.1344814405f,
     0.2595491921f,
     0.3112421418f,
     0.2595491921f,
     0.1344814405f,
     0.0051881187f,
    -0.0667129454f,
    -0.0646456667f,
    -0.0194750531f,
     0.0206998545f,
     0.0268770981f,
     0.0042218477f,
    -0.0211310543f,
    -0.0277680265f,
    -0.0147560410f,
     0.0026455828f,
     0.0093324701f,
     0.0027206717f,
    -0.0081394389f,
    -0.0129111423f,
    -0.0089093422f,
    -0.0014325960f,
     0.0027119238f,
     0.0013041311f,
    -0.0026706129f,
    -0.0049723873f,
    -0.0040104017f,
    -0.0013767951f,
     0.0004547894f,
     0.0004377751f,
    -0.0006805056f,
    -0.0015699004f,
    -0.0015379892f,
    -0.0008011598f,
    -0.0000283893f
};


/*--------------------------------------------------------------
 * QRS DETECTION BAND-PASS  (5-15 Hz)
 *
 * Same tap count as the 1-40 Hz filter above so both can share
 * ecg_filter_state. The coefficients are not a hand-copied table: they are
 * generated in wombcare_dsp_init() by the windowed-sinc formula, which is
 * short, auditable, and cannot silently disagree with the sample rate.
 *
 * IMPORTANT: ecg_bandpass_filter is re-initialised before every use.
 * arm_fir_f32 carries its delay line between calls, so with two different
 * coefficient sets and two different signals sharing one instance, running
 * a pass without re-init would leak the tail of the previous signal into
 * the head of the next.
 *-------------------------------------------------------------*/
#define QRS_FILTER_NUM_TAPS   ECG_FILTER_NUM_TAPS

static float qrs_bandpass_coeffs[QRS_FILTER_NUM_TAPS];


static float scratch_pool[4][RING_BUFFER_CAPACITY];


static float rr_intervals_ms[MAX_BEATS];
static float fhr_bpm[MAX_BEATS];   /* FHR in bpm — variability MUST be computed
                                    * here, not on RR ms, to match the trained
                                    * model (see FEATURE_SPEC.md). */

/*
 * Sample index of each detected beat, compacted in step with fhr_bpm.
 *
 * Elapsed time MUST come from these, not from summing rr_intervals_ms:
 * the valid-beat filter drops beats from that array, so afterwards the RR
 * values no longer add up to wall-clock time. Anything measuring a
 * DURATION (the MLTV block boundaries, the accel/decel episode lengths)
 * are handed to the trend (with SAMPLE_PERIOD_MS) and become the beat
 * timeline that every DURATION is measured against.
 *
 * The maternal detector reuses these same arrays: it runs to completion
 * and reduces to a single median before the fetal pass starts, so giving
 * it a private copy would cost ~2.4 KB of static RAM for nothing.
 */
static uint32_t beat_idx[MAX_BEATS];

/* Maternal R-peak positions, kept alive after the maternal pass because
 * the fetal path needs them to blank the maternal residual out of the
 * abdominal signal (see MHR_BLANK_HALF_MS). */
static uint32_t mhr_peak_idx[MAX_MHR_BEATS];

/* One bit per sample: set where a maternal QRS sits, so the fetal
 * detector can decline to accept beats there without the waveform
 * itself being touched. */
static uint8_t maternal_mask[(RING_BUFFER_CAPACITY + 7U) / 8U];

#define MASK_SET(m, n)   (((m)[(n) >> 3] & (uint8_t)(1u << ((n) & 7u))) != 0u)

/*--------------------------------------------------------------------
 * HELPER FUNCTIONS
 *-------------------------------------------------------------------*/

/*
 * Convert a circular ECG ring (raw ADC counts) into a linear buffer,
 * applying the ADC-count -> microvolt conversion on the way out.
 */
static void unwrap_ecg(const uint16_t *ring,
                       float *flat,
                       uint32_t head)
{
    uint32_t idx = 0U;

    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++)
    {
        flat[idx++] = ((float)ring[i] - ADC_CENTER) * ADC_TO_UV_SCALE;
    }

    for (uint32_t i = 0U; i < head; i++)
    {
        flat[idx++] = ((float)ring[i] - ADC_CENTER) * ADC_TO_UV_SCALE;
    }
}

/*
 * Convert a circular ring into a linear buffer, keeping raw counts
 * (used for PVDF, which is normalized later by its own RMS).
 */
static void unwrap_raw(const uint16_t *ring,
                       float *flat,
                       uint32_t head)
{
    uint32_t idx = 0U;

    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++)
    {
        flat[idx++] = (float)ring[i];
    }

    for (uint32_t i = 0U; i < head; i++)
    {
        flat[idx++] = (float)ring[i];
    }
}

/*--------------------------------------------------------------------
 * BEAT DETECTION  (Pan-Tompkins)
 *
 * This replaces a bare threshold-and-blank loop that manufactured heart
 * rates out of noise, and it is worth recording exactly how, because the
 * failure was not obvious from the output.
 *
 * The old detector accepted any sample above 2.0 x the window standard
 * deviation that was a local maximum of fabsf(), and on EVERY candidate --
 * accepted or rejected -- it skipped forward by a fixed 200 ms refractory.
 * On a noisy trace, threshold crossings are dense, so that skip became the
 * metronome: candidates landed ~204 ms apart, were rejected for falling
 * under the RR minimum, and because a rejected candidate deliberately did
 * not advance the reference peak, the following one produced an interval of
 * ~408 ms -- a confident, repeatable 147 bpm from pure noise. Denser noise
 * landed a bin lower and produced 200-240 bpm.
 *
 * The RR-consistency check downstream could not catch this. Quite the
 * opposite: because the refractory skip QUANTISED the intervals, the false
 * rhythm was more regular than a real heartbeat, so it passed the
 * coefficient-of-variation gate more easily than the signal it was
 * impersonating.
 *
 * Note that simply lowering the upper bpm limit does not fix this. The most
 * common artefact rate was ~147 bpm, comfortably inside any plausible
 * fetal band. What fixes it is (a) detecting on QRS energy rather than raw
 * amplitude, (b) a threshold that adapts to the signal instead of a fixed
 * multiple of its standard deviation, (c) not letting a rejected candidate
 * pin the reference peak, and (d) an explicit quality measure so a window
 * with no real QRS complexes in it is rejected rather than described.
 *
 * The chain: 5-15 Hz band-pass -> 5-point derivative -> square -> 150 ms
 * moving-window integration -> adaptive-threshold peak picking.
 *
 * Both the maternal and the fetal path use this function; they differ only
 * in the plausible rate band, which also sets the refractory period.
 *-------------------------------------------------------------------*/

typedef struct
{
    float min_bpm;
    float max_bpm;
} BeatBandCfg_t;

/* Half-width of the search used to snap a delay-corrected envelope peak
 * onto the real extremum in the raw signal. 80 ms: wide enough to absorb
 * any error in the group-delay estimate, and well inside the shortest
 * interval the detector will accept (300 ms), so it cannot wander onto
 * the neighbouring beat. */
#define PEAK_REFINE_HALF_SAMPLES  ((uint32_t)(0.080f * SAMPLE_RATE_HZ))

/*
 * Snap an approximate beat position onto the largest excursion nearby in
 * the raw signal. Positions matter in absolute terms for the maternal
 * blanking step, and matching the real extremum is more robust than
 * trusting the arithmetic delay of the detection chain.
 */
static uint32_t refine_peak(const float *sig, uint32_t len, uint32_t approx)
{
    uint32_t lo = (approx > PEAK_REFINE_HALF_SAMPLES)
                      ? (approx - PEAK_REFINE_HALF_SAMPLES) : 0U;
    uint32_t hi = approx + PEAK_REFINE_HALF_SAMPLES;

    if (hi >= len)
    {
        hi = len - 1U;
    }

    /*
     * Baseline of THIS search window, subtracted before anything is
     * compared.
     *
     * "Largest excursion" only means fabsf(sig[i]) if the signal is
     * centred on zero, and on real hardware it is not. unwrap_ecg
     * removes a hardcoded ADC_CENTER (2048 counts), while a live front
     * end parks wherever it parks -- a measured chest lead sat at 2266
     * counts, leaving every sample carrying a +351,000 uV offset with
     * the R wave dipping to only -55,000 uV. Comparing raw magnitudes
     * there picks the BASELINE every single time, because the offset is
     * six times larger than the complex being looked for, so this
     * function returned a flat baseline sample instead of the R peak on
     * every call.
     *
     * The synthetic bench generates its signal centred exactly on
     * ADC_CENTER, so the offset is zero there and the fault is
     * invisible to it.
     */
    float sum = 0.0f;

    for (uint32_t i = lo; i <= hi; i++)
    {
        sum += sig[i];
    }

    const float base = sum / (float)((hi - lo) + 1U);

    uint32_t best     = approx;
    float    best_mag = -1.0f;

    for (uint32_t i = lo; i <= hi; i++)
    {
        const float mag = fabsf(sig[i] - base);

        if (mag > best_mag)
        {
            best_mag = mag;
            best     = i;
        }
    }

    return best;
}

/*
 * Detect beats in one ECG signal.
 *
 * sig       : input, len samples, in microvolts (not modified)
 * band      : scratch, len samples -- receives the 5-15 Hz signal
 * integ     : scratch, len samples -- receives the integrated envelope
 * idx_out   : receives the sample index of each accepted beat
 * rr_out    : receives the interval to the previous accepted beat, in ms.
 *             rr_out[0] is the interval between beats 0 and 1, so a run of
 *             n beats yields n-1 intervals; both arrays are written with
 *             the same count and index 0 of rr_out is only meaningful once
 *             at least two beats exist.
 * excl      : optional bit mask, one bit per sample, marking regions that
 *             must not produce a beat and must not contribute to the
 *             threshold statistics (used to keep the maternal QRS out of
 *             the fetal detection). NULL for no exclusion.
 * out_ratio : peak-to-baseline ratio of the integrated signal (the quality
 *             measure described above). Always written.
 *
 * Returns the number of accepted beats.
 */
static uint16_t detect_beats(const float         *sig,
                             uint32_t             len,
                             const BeatBandCfg_t *cfg,
                             float               *band,
                             float               *integ,
                             uint32_t            *idx_out,
                             float               *rr_out,
                             uint16_t             max_beats,
                             const uint8_t       *excl,
                             float               *out_ratio)
{
    *out_ratio = 0.0f;

    if (len < (MWI_WINDOW_SAMPLES * 4U))
    {
        return 0U;
    }

/* Envelope index -> approximate index in the original signal. Used for
 * mask lookups, where being a sample or two out does not matter. */
#define ENVELOPE_TO_SIGNAL(n) \
    (((n) > DETECTOR_GROUP_DELAY) ? ((n) - DETECTOR_GROUP_DELAY) : 0U)

    /*----------------------------------------------------------
     * 1. Band-pass to the QRS energy band.
     *
     * Re-init first: the instance is shared with the 1-40 Hz filter and
     * with the other signal's pass, and arm_fir_f32 keeps its delay line
     * between calls.
     *---------------------------------------------------------*/
    arm_fir_init_f32(&ecg_bandpass_filter,
                     QRS_FILTER_NUM_TAPS,
                     qrs_bandpass_coeffs,
                     ecg_filter_state,
                     len);

    arm_fir_f32(&ecg_bandpass_filter, sig, band, len);

    /*----------------------------------------------------------
     * 2. Five-point derivative. Emphasises the steep QRS slope and
     *    suppresses the slower P and T waves, which is what stops a tall
     *    T wave being counted as a second beat.
     *---------------------------------------------------------*/
    integ[0] = 0.0f;
    integ[1] = 0.0f;

    for (uint32_t i = 2U; i < (len - 2U); i++)
    {
        integ[i] = ((2.0f * band[i + 2U]) + band[i + 1U]
                    - band[i - 1U] - (2.0f * band[i - 2U])) * 0.125f;
    }

    integ[len - 2U] = 0.0f;
    integ[len - 1U] = 0.0f;

    /*----------------------------------------------------------
     * 3. Square. Makes everything positive and disproportionately
     *    rewards the large deflections.
     *---------------------------------------------------------*/
    arm_mult_f32(integ, integ, integ, len);

    /*----------------------------------------------------------
     * 4. Moving-window integration, in place.
     *
     * The running sum is rebuilt from the history every
     * MWI_RESYNC_INTERVAL samples. Subtract-then-add on f32 accumulates
     * rounding error, and over a 15000-sample window that drift is large
     * enough to tilt the adaptive threshold.
     *---------------------------------------------------------*/
    {
        float    hist[MWI_WINDOW_SAMPLES];
        float    running = 0.0f;
        uint32_t h       = 0U;

        memset(hist, 0, sizeof(hist));

        for (uint32_t i = 0U; i < len; i++)
        {
            running -= hist[h];
            hist[h]  = integ[i];
            running += hist[h];

            h = (h + 1U) % MWI_WINDOW_SAMPLES;

            if ((i % MWI_RESYNC_INTERVAL) == 0U)
            {
                running = 0.0f;
                for (uint32_t k = 0U; k < MWI_WINDOW_SAMPLES; k++)
                {
                    running += hist[k];
                }
            }

            integ[i] = running / (float)MWI_WINDOW_SAMPLES;
        }
    }

    /*----------------------------------------------------------
     * 5. Adaptive-threshold peak picking.
     *---------------------------------------------------------*/
    float integ_mean = 0.0f;
    float init_max   = 0.0f;

    /*
     * Statistics are taken over the NON-excluded samples only. Including
     * the maternal complexes would inflate both the baseline and the
     * signal estimate, pushing the threshold above the much smaller
     * fetal complexes the detector is here to find -- the mask would
     * then suppress the very beats it is meant to reveal.
     */
    if (excl == NULL)
    {
        arm_mean_f32(integ, len, &integ_mean);
    }
    else
    {
        double   sum = 0.0;
        uint32_t n   = 0U;

        for (uint32_t i = 0U; i < len; i++)
        {
            const uint32_t s = ENVELOPE_TO_SIGNAL(i);

            if (!MASK_SET(excl, s))
            {
                sum += (double)integ[i];
                n++;
            }
        }

        integ_mean = (n > 0U) ? (float)(sum / (double)n) : 0.0f;
    }

    /*
     * Seed the signal estimate from the first two seconds, not from the
     * maximum of the whole window. One motion artifact anywhere in the
     * minute would otherwise set the initial threshold above every real
     * QRS -- and because spki is only updated by peaks that CLEAR the
     * threshold, nothing would ever bring it back down. The detector
     * would return zero beats for the whole window and the cause would
     * be invisible.
     */
    const uint32_t init_len = (2U * SAMPLE_RATE_HZ) < len
                                  ? (2U * SAMPLE_RATE_HZ)
                                  : len;

    for (uint32_t i = 0U; i < init_len; i++)
    {
        if ((excl != NULL) && MASK_SET(excl, ENVELOPE_TO_SIGNAL(i)))
        {
            continue;
        }

        if (integ[i] > init_max)
        {
            init_max = integ[i];
        }
    }

    if ((integ_mean <= 0.0f) || (init_max <= 0.0f))
    {
        return 0U;
    }

    /* Conventional Pan-Tompkins initial estimates. */
    float spki = 0.25f * init_max;
    float npki = 0.50f * integ_mean;

    /* Samples after which a silent stretch is treated as "the threshold
     * is too high", used by the decay below. */
    const uint32_t stall_samples =
        (uint32_t)((60000.0f / cfg->min_bpm) / SAMPLE_PERIOD_MS);

    /* The refractory period comes from the rate ceiling rather than being
     * a fixed constant, so the two callers get a blanking interval that
     * actually matches the rhythm they are looking for: 300 ms for the
     * 200 bpm fetal ceiling, 333 ms for the 180 bpm maternal one. */
    const uint32_t refractory =
        (uint32_t)((60000.0f / cfg->max_bpm) / SAMPLE_PERIOD_MS);

    const float rr_min_ms = 60000.0f / cfg->max_bpm;
    const float rr_max_ms = 60000.0f / cfg->min_bpm;

    uint16_t count         = 0U;
    uint32_t last_idx      = 0U;
    uint32_t decay_anchor  = 0U;
    bool     have_last     = false;
    float    peak_amp_sum  = 0.0f;


    for (uint32_t i = 1U; i < (len - 1U); i++)
    {
        /* Local maximum of the integrated envelope. */
        if ((integ[i] <= integ[i - 1U]) || (integ[i] < integ[i + 1U]))
        {
            continue;
        }

        /*
         * Where this envelope peak actually sits in the signal. Resolved
         * here, before the mask test, because refine_peak can move a
         * candidate by up to PEAK_REFINE_HALF_SAMPLES -- testing the
         * unrefined position let candidates just outside a maternal
         * window be snapped back inside it and accepted anyway.
         */
        const uint32_t cand = refine_peak(sig, len, ENVELOPE_TO_SIGNAL(i));

        /* Inside a maternal complex: not ours to claim. Skipped before
         * the threshold logic so it does not disturb either estimate. */
        if ((excl != NULL) && MASK_SET(excl, cand))
        {
            continue;
        }

        /*
         * Nothing has been accepted for longer than the slowest rate we
         * are willing to believe. Either the signal faded or the
         * threshold is sitting too high; pull it down so the detector can
         * recover instead of staying silent for the rest of the window.
         * This is the role search-back plays in the original algorithm.
         *
         * BOTH estimates decay, and that is the whole point.
         *
         * The threshold is npki + 0.25 * (spki - npki), i.e.
         *
         *      threshold = 0.75 * npki  +  0.25 * spki
         *
         * so npki carries THREE QUARTERS of it. This used to decay spki
         * alone, which could not rescue a stalled detector even in
         * principle: driving spki to zero still leaves the threshold at
         * 0.75 * npki.
         *
         * That mattered because npki is what runs away. A real R wave
         * that lands just under the threshold is classified as noise,
         * and the noise update pulls npki UP towards it -- which raises
         * the threshold, which puts the next R wave further under it.
         * The detector locks itself out of its own signal, and the only
         * beats that still get through are the few largest ones, spaced
         * far too widely to form a rate.
         *
         * NOT YET OBSERVED IN THE WILD -- this is a fix for a defect
         * found by reading, not one traced to a reproduced failure. An
         * attempt to provoke it on the bench (synth_real_chest in
         * tools/dsp_selftest, an offset baseline with respiration-
         * modulated R amplitude) locks correctly WITHOUT this change, so
         * do not read the runaway above as the explanation for any
         * particular field symptom. It is a real hole in the recovery
         * path regardless: decaying only the 25% term cannot lower a
         * threshold that the 75% term has pushed up.
         *
         * The `have_last` requirement is gone for the same reason. A
         * window whose opening seconds contain one large artifact seeds
         * spki above every real complex, nothing is ever accepted,
         * have_last stays false forever -- and the recovery path that
         * exists precisely for this case was gated off behind it.
         */
        if ((i - decay_anchor) > stall_samples)
        {
            spki *= QRS_STALL_DECAY;
            npki *= QRS_STALL_DECAY;

            /* Its own anchor, deliberately NOT last_idx: moving last_idx
             * here would invent an RR interval for whichever beat is
             * accepted next, which is the class of bug this whole
             * rewrite exists to remove. */
            decay_anchor = i;
        }

        const float threshold =
            npki + (QRS_THRESHOLD_FRACTION * (spki - npki));

        if (integ[i] < threshold)
        {
            /* Classified as noise: it still informs the noise estimate. */
            npki = (QRS_ESTIMATE_WEIGHT * integ[i])
                   + ((1.0f - QRS_ESTIMATE_WEIGHT) * npki);
            continue;
        }

        /* Above threshold: this is a QRS candidate. */
        spki = (QRS_ESTIMATE_WEIGHT * integ[i])
               + ((1.0f - QRS_ESTIMATE_WEIGHT) * spki);

        if (!have_last)
        {
            if (count < max_beats)
            {
                idx_out[count] = cand;
                rr_out[count]  = 0.0f;
                count++;

                peak_amp_sum += integ[i];
            }

            last_idx     = i;
            decay_anchor = i;
            have_last    = true;

            i += refractory;
            continue;
        }

        const float rr_ms = (float)(i - last_idx) * SAMPLE_PERIOD_MS;

        if ((rr_ms >= rr_min_ms) && (rr_ms <= rr_max_ms)
            && (count < max_beats))
        {
            idx_out[count] = cand;
            rr_out[count]  = rr_ms;
            count++;

            peak_amp_sum += integ[i];

            last_idx = i;
        }
        else if (rr_ms > rr_max_ms)
        {
            /*
             * The gap is too long to be one interval -- beats were missed,
             * typically because the electrode was disturbed. Restart the
             * chain from here rather than recording an impossible interval.
             */
            if (count < max_beats)
            {
                idx_out[count] = cand;
                rr_out[count]  = 0.0f;
                count++;

                peak_amp_sum += integ[i];
            }

            last_idx = i;
        }
        else
        {
            /*
             * Too soon after the last accepted beat. Advance the reference
             * past the blanking interval instead of leaving it pinned --
             * leaving it pinned is precisely what let a rejected candidate
             * compound into a plausible-looking interval and gave the old
             * detector its phantom 147 bpm.
             */
            last_idx = i;
        }

        /* Every branch above moves last_idx to i, so the stall timer
         * restarts from the same place in all of them. */
        decay_anchor = last_idx;

        i += refractory;
    }

    if (count > 0U)
    {
        *out_ratio = (peak_amp_sum / (float)count) / integ_mean;
    }

    return count;
}

/* Peak-to-peak span of a buffer. Reported on the serial log as the
 * "how big is the ECG" figure, because it is what a scope would show;
 * an RMS over a whole window is dominated by the flat stretches between
 * complexes and reads far smaller than the R wave people expect. */
static float peak_to_peak(const float *s, uint32_t n)
{
    float mn = s[0];
    float mx = s[0];

    for (uint32_t i = 1U; i < n; i++)
    {
        if (s[i] < mn) { mn = s[i]; }
        if (s[i] > mx) { mx = s[i]; }
    }

    return mx - mn;
}

/*
 * Median of an unsorted float array. Sorts scratch in place (insertion
 * sort: at most MAX_BEATS entries, once per window). scratch must hold
 * at least n floats.
 */
static float median_of(const float *values, uint16_t n, float *scratch)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    memcpy(scratch, values, (size_t)n * sizeof(float));

    for (uint16_t i = 1U; i < n; i++)
    {
        float    key = scratch[i];
        uint16_t j   = i;

        while ((j > 0U) && (scratch[j - 1U] > key))
        {
            scratch[j] = scratch[j - 1U];
            j--;
        }

        scratch[j] = key;
    }

    return ((n & 1U) != 0U)
               ? scratch[n / 2U]
               : (0.5f * (scratch[(n / 2U) - 1U] + scratch[n / 2U]));
}

/*--------------------------------------------------------------------
 * DEMO FALLBACK HELPER
 *
 * Used ONLY by the TEMPORARY maternal-rate placeholder above (and by
 * app.c's equivalent fetal one) to make a fabricated bpm value move the
 * way a real one does, rather than following an obviously mathematical
 * curve. Not part of any detection path -- deleting the fallback that
 * calls this needs no other change.
 *
 * One bounded random walk step: nudge `value` by a random amount in
 * [-max_step, +max_step], then REFLECT it back inside [lo, hi] if the
 * step carried it out. Reflecting instead of clamping matters here: a
 * clamped walk spends long stretches pinned to whichever edge it hit
 * (itself a give-away, and the opposite of "proper fluctuation"),
 * while reflecting turns an out-of-range step into an equal move back
 * inward, so the value keeps moving on every call.
 *
 * `rng` is an LCG, mirroring the one in tools/dsp_selftest/main.c so
 * the bench and the device produce the same *kind* of motion; the two
 * are seeded differently on purpose so they do not track each other.
 *-------------------------------------------------------------------*/
static float demo_bounded_walk(float value, uint32_t *rng,
                               float lo, float hi, float max_step)
{
    *rng = (*rng * 1103515245u) + 12345u;

    /* Top 15 bits: the low bits of a linear congruential generator are
     * far less random than the high ones. */
    const float unit = (float)((*rng >> 16) & 0x7FFFu) / 32767.0f; /* 0..1 */

    value += ((unit * 2.0f) - 1.0f) * max_step;

    if (value < lo) { value = lo + (lo - value); }
    if (value > hi) { value = hi - (value - hi); }

    /* Belt and braces: only reachable if max_step exceeds the range
     * width, which none of the current callers do, but a future one
     * might. */
    if (value < lo) { value = lo; }
    if (value > hi) { value = hi; }

    return value;
}

/*--------------------------------------------------------------------
 * PUBLIC API
 *-------------------------------------------------------------------*/

void wombcare_dsp_init(void)
{
    /* Never carry a previous session's beats into a new one. */
    wombcare_trend_reset();

    /*----------------------------------------------------------
     * LMS maternal ECG cancellation
     *---------------------------------------------------------*/
    arm_lms_norm_init_f32(
        &lms_instance,
        NUM_TAPS,
        lms_coeffs,
        lms_state,
        LMS_STEP_SIZE,
        RING_BUFFER_CAPACITY);

    /*----------------------------------------------------------
     * ECG band-pass filter
     *
     * 250 Hz sampling
     * approximately 1-40 Hz passband
     *
     * Both this and the QRS filter below share one arm_fir_instance_f32
     * and one state array, so every user re-initialises before its pass.
     * This call only establishes a sane default.
     *---------------------------------------------------------*/
    arm_fir_init_f32(
        &ecg_bandpass_filter,
        ECG_FILTER_NUM_TAPS,
        ecg_bandpass_coeffs,
        ecg_filter_state,
        RING_BUFFER_CAPACITY);

    /*----------------------------------------------------------
     * QRS detection band-pass, 5-15 Hz.
     *
     * Built here by the windowed-sinc method rather than pasted in as a
     * table of magic numbers: a bandpass is the difference of two
     * low-pass sincs, Hamming-windowed to control the stopband, then
     * normalised to unit gain at the centre of the passband so the
     * detector sees the signal in its original microvolt scale.
     *---------------------------------------------------------*/
    {
        const float fs      = (float)SAMPLE_RATE_HZ;
        const float fc_low  = QRS_BAND_LOW_HZ  / fs;   /* normalised */
        const float fc_high = QRS_BAND_HIGH_HZ / fs;
        const int   m       = (int)QRS_FILTER_NUM_TAPS - 1;
        const float centre  = (float)m * 0.5f;

        for (int n = 0; n <= m; n++)
        {
            const float k = (float)n - centre;

            float lp_high;
            float lp_low;

            if (fabsf(k) < 1e-6f)
            {
                /* sinc(0) = 1 */
                lp_high = 2.0f * fc_high;
                lp_low  = 2.0f * fc_low;
            }
            else
            {
                const float wh = 2.0f * PI * fc_high * k;
                const float wl = 2.0f * PI * fc_low  * k;

                lp_high = sinf(wh) / (PI * k);
                lp_low  = sinf(wl) / (PI * k);
            }

            /* Hamming window. */
            const float w = 0.54f
                            - (0.46f * cosf((2.0f * PI * (float)n)
                                            / (float)m));

            qrs_bandpass_coeffs[n] = (lp_high - lp_low) * w;
        }

        /*
         * Normalise to unit gain at the passband centre. A band-pass has
         * zero DC gain, so the usual "divide by the sum of the taps" trick
         * would divide by ~0; evaluate the response at the centre
         * frequency instead.
         */
        const float f_mid = 0.5f * (QRS_BAND_LOW_HZ + QRS_BAND_HIGH_HZ) / fs;

        float re = 0.0f;
        float im = 0.0f;

        for (int n = 0; n <= m; n++)
        {
            const float w = 2.0f * PI * f_mid * (float)n;

            re += qrs_bandpass_coeffs[n] * cosf(w);
            im -= qrs_bandpass_coeffs[n] * sinf(w);
        }

        const float gain = sqrtf((re * re) + (im * im));

        if (gain > 1e-9f)
        {
            const float inv = 1.0f / gain;

            for (int n = 0; n <= m; n++)
            {
                qrs_bandpass_coeffs[n] *= inv;
            }
        }
    }
}


/*--------------------------------------------------------------------
 * PVDF fetal-movement count for ONE window.
 *
 * Returns a raw count, not a per-minute rate: the trend sums counts
 * across the retained windows and divides once, by the observation time
 * that actually backs them.
 *
 * `flat_pvdf` must be a scratch buffer of RING_BUFFER_CAPACITY floats.
 *-------------------------------------------------------------------*/
static float count_pvdf_kicks(float *flat_pvdf)
{
    unwrap_raw(ring_pvdf_kick, flat_pvdf, tracker_pvdf.head);

    /* Remove DC / baseline */
    float pvdf_mean = 0.0f;
    float pvdf_rms  = 0.0f;

    arm_mean_f32(flat_pvdf, RING_BUFFER_CAPACITY, &pvdf_mean);

    for (uint32_t i = 0U; i < RING_BUFFER_CAPACITY; i++)
    {
        flat_pvdf[i] -= pvdf_mean;
    }

    /* Measure signal variation */
    arm_rms_f32(flat_pvdf, RING_BUFFER_CAPACITY, &pvdf_rms);

    /* Ignore disconnected / very quiet signal */
    if (pvdf_rms < PVDF_MIN_RMS_THRESHOLD)
    {
        return 0.0f;
    }

    /* Require a strong excursion above normal signal variation */
    const float threshold = pvdf_rms * PVDF_KICK_THRESHOLD_MULTIPLIER;
    float       kicks     = 0.0f;

    for (uint32_t i = 1U; i < RING_BUFFER_CAPACITY - 1U; i++)
    {
        const float current = fabsf(flat_pvdf[i]);

        if ((current > threshold) &&
            (current >= fabsf(flat_pvdf[i - 1U])) &&
            (current >= fabsf(flat_pvdf[i + 1U])))
        {
            /* One detected movement = one kick */
            kicks += 1.0f;

            /* Prevent one movement/ringing from becoming many kicks */
            i += PVDF_KICK_REFRACTORY_SAMPLES;
        }
    }

    return kicks;
}

/*--------------------------------------------------------------------
 * Build the 8-feature vector from a beat series.
 *
 * This used to be inlined at the end of the pipeline and operated on the
 * single 60 s window. It now takes the series explicitly so it can be
 * run over the rolling trend instead (wombcare_trend.h explains why that
 * matters). The feature DEFINITIONS are unchanged -- only the span they
 * are measured over.
 *
 *   fhr      : FHR values, bpm
 *   t_ms     : each beat's time in ms, monotonic, any origin
 *   n        : beat count
 *   span_min : observation time backing the series, in minutes. This is
 *              the divisor for the count features, so it must be the
 *              time actually observed, not the elapsed wall clock.
 *   kicks    : raw PVDF kick count over the same observation time
 *-------------------------------------------------------------------*/
static void compute_features(const float        *fhr,
                             const float        *t_ms,
                             uint16_t            n,
                             float               span_min,
                             float               kicks,
                             WombCareFeatures_t *out)
{
    if ((n < 2U) || (span_min <= 0.0f))
    {
        return;
    }

    /*--------------------------------------------------------------
     * MeanHR + Variance (Feature Spec §3.6, §3.7)
     *-------------------------------------------------------------*/
    arm_mean_f32(fhr, n, &out->mean_hr_bpm);

    /* Variance of FHR in bpm^2 (not RR variance in ms^2). */
    arm_var_f32(fhr, n, &out->hr_variance);

    /*--------------------------------------------------------------
     * LB — baseline FHR (Feature Spec §3.0)
     *
     * The MEDIAN, not the mean. Two reasons: the baseline is supposed
     * to survive accel/decel excursions, and the model was trained on
     * LB and MeanHR as distinct features (training means 133.30 vs
     * 134.61). Setting LB = MeanHR made inputs 0 and 6 identical --
     * a combination that never occurs in the training set.
     *
     * scratch_pool[2] is dead at this point and is far larger than the
     * trend can ever be, so sorting a copy stays cheap.
     *-------------------------------------------------------------*/
    out->lb_bpm = median_of(fhr, n, scratch_pool[2]);

    /*--------------------------------------------------------------
     * MSTV
     *
     * Beat-to-beat differences are only meaningful across CONSECUTIVE
     * beats. A gap (a rejected window inside the trend) is not a
     * beat-to-beat interval, so those pairs are skipped rather than
     * contributing a large spurious difference.
     *-------------------------------------------------------------*/
    {
        float    sum_diff   = 0.0f;
        uint16_t diff_count = 0U;

        for (uint16_t i = 0U; i < (n - 1U); i++)
        {
            if ((t_ms[i + 1U] - t_ms[i]) > TREND_GAP_BREAK_MS)
            {
                continue;
            }

            sum_diff += fabsf(fhr[i + 1U] - fhr[i]);
            diff_count++;
        }

        out->mstv_ms = (diff_count > 0U)
                           ? (sum_diff / (float)diff_count)
                           : 0.0f;
    }

    /*--------------------------------------------------------------
     * MLTV — mean long-term variability (Feature Spec §3.2)
     *
     * Split the series into consecutive 60 SECOND blocks using beat
     * time, take max(FHR) - min(FHR) within each, and average those
     * ranges.
     *
     * Over a 60 s window this averaged exactly ONE block, which is not
     * what the trained definition means. Across the trend it averages
     * as many blocks as the span holds.
     *-------------------------------------------------------------*/
    {
        float    mltv_sum    = 0.0f;
        uint16_t mltv_blocks = 0U;
        uint16_t block_beats = 0U;
        float    block_start = t_ms[0];
        float    block_max   = fhr[0];
        float    block_min   = fhr[0];

        for (uint16_t i = 0U; i < n; i++)
        {
            if (fhr[i] > block_max)
            {
                block_max = fhr[i];
            }

            if (fhr[i] < block_min)
            {
                block_min = fhr[i];
            }

            block_beats++;

            if ((t_ms[i] - block_start) >= MLTV_BLOCK_MS)
            {
                mltv_sum += (block_max - block_min);
                mltv_blocks++;

                block_beats = 0U;

                if ((i + 1U) < n)
                {
                    block_start = t_ms[i + 1U];
                    block_max   = fhr[i + 1U];
                    block_min   = fhr[i + 1U];
                }
            }
        }

        /* Close a trailing partial block. Without this a series whose
         * accepted beats never quite total 60 s would report MLTV = 0,
         * which reads as "no variability at all" rather than "short
         * block". */
        if (block_beats >= 2U)
        {
            mltv_sum += (block_max - block_min);
            mltv_blocks++;
        }

        out->mltv_ms = (mltv_blocks > 0U)
                           ? (mltv_sum / (float)mltv_blocks)
                           : 0.0f;
    }

    /*--------------------------------------------------------------
     * Accelerations / Decelerations
     *
     * Counts complete episodes rather than every elapsed 15 seconds.
     * This avoids counting one long acceleration/deceleration
     * multiple times.
     *
     * Running this over the trend rather than one window is the main
     * point of the change: a >=15 s episode fits entirely inside a 60 s
     * window only ~75% of the time, so the old span lost about a fifth
     * of all episodes to its own edges. An episode that straddles a
     * window boundary is now seen whole.
     *-------------------------------------------------------------*/
    out->accel_count = 0.0f;
    out->decel_count = 0.0f;

    {
        bool  accel_active = false;
        bool  decel_active = false;
        float accel_start_ms = 0.0f;
        float decel_start_ms = 0.0f;
        float accel_duration_ms = 0.0f;
        float decel_duration_ms = 0.0f;

        for (uint16_t i = 0U; i < n; i++)
        {
            /*------------------------------------------------------
             * A discontinuity ends whatever was running. The trace
             * across a rejected window is unknown, and assuming the
             * excursion persisted through it would manufacture long
             * episodes out of two unrelated ones.
             *-----------------------------------------------------*/
            if ((i > 0U) &&
                ((t_ms[i] - t_ms[i - 1U]) > TREND_GAP_BREAK_MS))
            {
                if (accel_active && (accel_duration_ms >= EPISODE_MIN_MS))
                {
                    out->accel_count += 1.0f;
                }

                if (decel_active && (decel_duration_ms >= EPISODE_MIN_MS))
                {
                    out->decel_count += 1.0f;
                }

                accel_active      = false;
                decel_active      = false;
                accel_duration_ms = 0.0f;
                decel_duration_ms = 0.0f;
            }

            /* Excursions are measured against LB (the median
             * baseline), per Feature Spec §3.3/§3.4. */
            const float instant_bpm = fhr[i];

            /*------------------------------------------------------
             * ACCELERATION
             *-----------------------------------------------------*/
            if (instant_bpm >= (out->lb_bpm + EPISODE_DELTA_BPM))
            {
                if (!accel_active)
                {
                    accel_active   = true;
                    accel_start_ms = t_ms[i];
                }

                accel_duration_ms = t_ms[i] - accel_start_ms;

                /* End any running deceleration */
                if (decel_active)
                {
                    if (decel_duration_ms >= EPISODE_MIN_MS)
                    {
                        out->decel_count += 1.0f;
                    }

                    decel_active      = false;
                    decel_duration_ms = 0.0f;
                }
            }
            /*------------------------------------------------------
             * DECELERATION
             *-----------------------------------------------------*/
            else if (instant_bpm <= (out->lb_bpm - EPISODE_DELTA_BPM))
            {
                if (!decel_active)
                {
                    decel_active   = true;
                    decel_start_ms = t_ms[i];
                }

                decel_duration_ms = t_ms[i] - decel_start_ms;

                /* End any running acceleration */
                if (accel_active)
                {
                    if (accel_duration_ms >= EPISODE_MIN_MS)
                    {
                        out->accel_count += 1.0f;
                    }

                    accel_active      = false;
                    accel_duration_ms = 0.0f;
                }
            }
            /*------------------------------------------------------
             * Returned to baseline.
             * Close whichever episode was active.
             *-----------------------------------------------------*/
            else
            {
                if (accel_active)
                {
                    if (accel_duration_ms >= EPISODE_MIN_MS)
                    {
                        out->accel_count += 1.0f;
                    }

                    accel_active      = false;
                    accel_duration_ms = 0.0f;
                }

                if (decel_active)
                {
                    if (decel_duration_ms >= EPISODE_MIN_MS)
                    {
                        out->decel_count += 1.0f;
                    }

                    decel_active      = false;
                    decel_duration_ms = 0.0f;
                }
            }
        }

        /*----------------------------------------------------------
         * Handle episodes that continue to the end of the series.
         *---------------------------------------------------------*/
        if (accel_active && (accel_duration_ms >= EPISODE_MIN_MS))
        {
            out->accel_count += 1.0f;
        }

        if (decel_active && (decel_duration_ms >= EPISODE_MIN_MS))
        {
            out->decel_count += 1.0f;
        }
    }

    /* Feature Spec §G4: the model consumes per-minute RATES, not raw
     * episode counts. */
    out->accel_count /= span_min;
    out->decel_count /= span_min;

    out->fetal_movements = kicks / span_min;
}

bool wombcare_dsp_run_pipeline(WombCareFeatures_t *output_features,
                               WombCareVitals_t   *output_vitals)
{
    /*--------------------------------------------------------------
     * Defensive Checks
     *-------------------------------------------------------------*/
    if (output_features == NULL)
    {
        return false;
    }

    /* Clear both outputs before filling them. The vitals struct is
     * written on every path from here on, including the rejections
     * below, so the app always has something to show. */
    memset(output_features, 0, sizeof(WombCareFeatures_t));

    if (output_vitals != NULL)
    {
        memset(output_vitals, 0, sizeof(WombCareVitals_t));
    }

    if (!minute_window_ready)
    {
        return false;
    }

    if (!tracker_mother.is_primed ||
        !tracker_fetal.is_primed ||
        !tracker_pvdf.is_primed)
    {
        return false;
    }

    /*--------------------------------------------------------------
     * Open this window in the rolling trend BEFORE any gate below can
     * reject it. Every early return from here on is a real gap in the
     * record and the trend has to see it as one: otherwise the beats
     * either side would look adjacent (fusing two unrelated excursions
     * into one long false episode) and the rejected minute would
     * silently vanish from the rate denominator. See wombcare_trend.h.
     *-------------------------------------------------------------*/
    wombcare_trend_begin_window(WINDOW_SECONDS * 1000.0f);

    /*--------------------------------------------------------------
     * Scratch buffer schedule.
     *
     * Four buffers of RING_BUFFER_CAPACITY floats are all we have, and
     * the maternal detector needs two of them for its own working
     * space, so the naming below deliberately changes as the pipeline
     * advances. The schedule is:
     *
     *   after unwrap  [0]=mother   [1]=abdomen  [2]=-        [3]=-
     *   after LMS     [0]=mother   [1]=free     [2]=mat.est  [3]=fetal
     *   maternal      [0]=mother   [1]=band     [2]=integ    [3]=fetal
     *   fetal gate    [0]=free     [1]=free     [2]=1-40Hz   [3]=fetal
     *   fetal detect  [0]=band     [1]=integ    [2]=free     [3]=fetal
     *   LB median     [0]=-        [1]=-        [2]=sort     [3]=-
     *   PVDF          [0]=pvdf
     *-------------------------------------------------------------*/
    float *flat_mother_ecg = scratch_pool[0];
    float *flat_abdom_ecg  = scratch_pool[1];
    float *lms_estimate    = scratch_pool[2];
    float *clean_fetal_ecg = scratch_pool[3];

    /*--------------------------------------------------------------
     * Linearize ECG ring buffers (raw counts -> microvolts).
     * PVDF is unwrapped later, reusing a freed scratch buffer.
     *-------------------------------------------------------------*/
    unwrap_ecg(ring_mother_ecg,
               flat_mother_ecg,
               tracker_mother.head);

    unwrap_ecg(ring_fetal_ecg,
               flat_abdom_ecg,
               tracker_fetal.head);

    if (output_vitals != NULL)
    {
        output_vitals->mother_ecg_uv_pp =
            peak_to_peak(flat_mother_ecg, RING_BUFFER_CAPACITY);

        /*
         * Raw rail proximity check. Read directly off the ring, not
         * flat_mother_ecg, so this is the actual code the ADC produced
         * with nothing subtracted or scaled -- if the front end is
         * biased close to 0 or 4095, this is what shows it.
         */
        uint16_t raw_min = ring_mother_ecg[0];
        uint16_t raw_max = ring_mother_ecg[0];

        for (uint32_t i = 1U; i < RING_BUFFER_CAPACITY; i++)
        {
            if (ring_mother_ecg[i] < raw_min) { raw_min = ring_mother_ecg[i]; }
            if (ring_mother_ecg[i] > raw_max) { raw_max = ring_mother_ecg[i]; }
        }

        output_vitals->mother_adc_min = raw_min;
        output_vitals->mother_adc_max = raw_max;
    }

    /*--------------------------------------------------------------
     * Maternal ECG Cancellation
     *
     * pOut is the maternal estimate; pErr is the residual, which is the
     * fetal ECG we actually want.
     *-------------------------------------------------------------*/
    arm_lms_norm_f32(
        &lms_instance,
        flat_mother_ecg,
        flat_abdom_ecg,
        lms_estimate,
        clean_fetal_ecg,
        RING_BUFFER_CAPACITY);

    /*==============================================================
     * MATERNAL HEART RATE
     *
     * Measured on the chest lead directly, before anything is done to
     * the fetal path. Two reasons for the ordering: the chest lead is
     * an ordinary high-SNR adult ECG so it is by far the easier of the
     * two detections, and doing it here means the maternal rate is
     * available even if every fetal gate below rejects the window.
     *
     * flat_abdom_ecg has served its purpose as the LMS input and is
     * reused as detector scratch alongside the (unused) LMS estimate.
     *=============================================================*/
    const BeatBandCfg_t mhr_cfg = { MHR_MIN_BPM, MHR_MAX_BPM };

    float    mhr_ratio = 0.0f;
    uint16_t mhr_beats = detect_beats(flat_mother_ecg,
                                      RING_BUFFER_CAPACITY,
                                      &mhr_cfg,
                                      flat_abdom_ecg,   /* band  */
                                      lms_estimate,     /* integ */
                                      mhr_peak_idx,
                                      rr_intervals_ms,
                                      MAX_MHR_BEATS,
                                      NULL,          /* no exclusions */
                                      &mhr_ratio);

    /* Recorded before any gate, so a rejected window still explains
     * itself on the serial log. */
    if (output_vitals != NULL)
    {
        output_vitals->mhr_beats   = mhr_beats;
        output_vitals->mhr_quality = mhr_ratio;
    }

    const bool maternal_ok = (mhr_beats >= 3U)
                             && (mhr_ratio >= QRS_QUALITY_MIN_MATERNAL);

    float maternal_bpm = 0.0f;

    if (maternal_ok)
    {
        /* Convert the accepted intervals to bpm. rr == 0 marks a chain
         * restart (first beat, or a gap too long to be one interval)
         * and carries no rate. */
        uint16_t n = 0U;

        for (uint16_t i = 0U; i < mhr_beats; i++)
        {
            if (rr_intervals_ms[i] <= 0.0f)
            {
                continue;
            }

            const float bpm = 60000.0f / rr_intervals_ms[i];

            if ((bpm >= MHR_MIN_BPM) && (bpm <= MHR_MAX_BPM))
            {
                /* Reuse the fetal bpm array as scratch: the fetal
                 * detector has not run yet. */
                fhr_bpm[n] = bpm;
                n++;
            }
        }

        if (n >= 3U)
        {
            /*--------------------------------------------------
             * Regularity gate.
             *
             * A heartbeat is regular; noise is not. Without this the
             * maternal path had nothing but an amplitude ratio standing
             * between an unplugged electrode and a published heart
             * rate, and the bench showed an unplugged lead producing a
             * confident 163 bpm. Rate spread is a far stronger test:
             * every real trace measured sat below 0.08, while noise and
             * muscle artifact sat above 0.18.
             *-------------------------------------------------*/
            float bpm_mean = 0.0f;
            float bpm_sd   = 0.0f;

            arm_mean_f32(fhr_bpm, n, &bpm_mean);
            arm_std_f32(fhr_bpm, n, &bpm_sd);

            const float bpm_cv =
                (bpm_mean > 0.0f) ? (bpm_sd / bpm_mean) : 99.0f;

            if (output_vitals != NULL)
            {
                output_vitals->mhr_rr_cv = bpm_cv;
            }

            /* Median, not mean: one missed beat doubles an interval and
             * would drag a mean badly off.
             *
             * Computed here, BEFORE the regularity gate, and kept on the
             * struct as a bench-only "candidate" regardless of whether
             * that gate passes. Without this, a window that fails the cv
             * check leaves no record of what rate it was trending
             * towards -- only that it failed -- which makes it
             * impossible to tell "close to a real lock" apart from "not
             * even in the right neighbourhood" from the serial log
             * alone. It is never treated as a measurement on its own:
             * mhr_valid is what still gates everything downstream.
             */
            const float candidate_bpm = median_of(fhr_bpm, n, flat_abdom_ecg);

            if (output_vitals != NULL)
            {
                output_vitals->mhr_bpm_candidate = candidate_bpm;
            }

            if (bpm_cv <= MHR_RR_CV_MAX)
            {
                maternal_bpm = candidate_bpm;

                if (output_vitals != NULL)
                {
                    output_vitals->mhr_bpm   = maternal_bpm;
                    output_vitals->mhr_valid = true;
                }
            }
        }
    }

    /*--------------------------------------------------------------
     * TEMPORARY DEMO FALLBACK -- placeholder maternal rate.
     *
     * The real detector above is not locking on the current live chest
     * lead (see mhr_beats / mhr_quality / mhr_rr_cv on the serial log
     * for why -- as of this writing the amplitude ratio or the RR
     * regularity gate is rejecting the window). At the user's explicit
     * request, while that signal-quality problem is worked separately,
     * a window with no real lock publishes a plausible resting adult
     * rate instead of 0/no-lock, so the app has something to show now.
     *
     * This is NOT a measurement. mhr_simulated is set alongside it so
     * it stays possible to tell a real lock from this placeholder, and
     * so this whole block is easy to find and delete once the real
     * detector is fixed -- at which point maternal_ok above starts
     * succeeding and this branch simply stops running on its own.
     *
     * The phase advances once per window (~1 Hz) rather than jumping,
     * so consecutive placeholder readings move the way a real trace
     * would rather than snapping between values.
     *-------------------------------------------------------------*/
    if ((output_vitals != NULL) && !output_vitals->mhr_valid)
    {
        /*
         * A sine wave was tried here first and rejected: it spends
         * ~30 seconds climbing and ~30 seconds falling, which on a
         * per-second serial log reads as a rate that is monotonically
         * rising or falling for half a minute at a stretch -- nothing
         * like real beat-to-beat variability, which moves up and down
         * unpredictably from one second to the next. A bounded random
         * walk does that instead: each window nudges the value by a
         * small random step and reflects it back inside the range
         * whenever it would leave. See demo_bounded_walk() for why
         * reflecting rather than clamping matters.
         */
        static float    s_mhr_demo_bpm = 78.0f;   /* seed: normal resting */
        static uint32_t s_mhr_demo_rng = 0x4D4852u; /* seed: "MHR" */

        s_mhr_demo_bpm = demo_bounded_walk(s_mhr_demo_bpm,
                                           &s_mhr_demo_rng,
                                           60.0f, 100.0f,
                                           2.5f);

        output_vitals->mhr_bpm       = s_mhr_demo_bpm;
        output_vitals->mhr_valid     = true;
        output_vitals->mhr_simulated = true;
    }

    /*==============================================================
     * FETAL PATH
     *=============================================================*/

    /*--------------------------------------------------------------
     * Blank the maternal residual out of the abdominal signal.
     *
     * The LMS canceller leaves a maternal remnant that is still much
     * larger than the fetal complexes, and without this step the fetal
     * detector locks onto it and reports the MOTHER'S heart rate in the
     * fetal field. Now that the maternal R peaks are known, the remnant
     * is cut out where it lives.
     *
     * See MHR_BLANK_HALF_MS for why this is a mask rather than an edit
     * to the waveform.
     *-------------------------------------------------------------*/
    const uint8_t *fetal_excl = NULL;

    if (maternal_ok)
    {
        const uint32_t half =
            (uint32_t)(MHR_BLANK_HALF_MS / SAMPLE_PERIOD_MS);

        memset(maternal_mask, 0, sizeof(maternal_mask));

        for (uint16_t b = 0U; b < mhr_beats; b++)
        {
            const uint32_t p = mhr_peak_idx[b];

            const uint32_t lo = (p > half) ? (p - half) : 0U;
            const uint32_t hi = ((p + half) < (RING_BUFFER_CAPACITY - 1U))
                                    ? (p + half)
                                    : (RING_BUFFER_CAPACITY - 1U);

            for (uint32_t k = lo; k <= hi; k++)
            {
                maternal_mask[k >> 3] |= (uint8_t)(1u << (k & 7u));
            }
        }

        fetal_excl = maternal_mask;
    }

    /*--------------------------------------------------------------
     * Amplitude / flatline gate.
     *
     * Runs on the 1-40 Hz band, which is what the 150 uV threshold was
     * calibrated against -- the 5-15 Hz detection band used below has a
     * quite different amplitude and would silently move this limit.
     *-------------------------------------------------------------*/
    float *fetal_wideband = scratch_pool[2];

    arm_fir_init_f32(&ecg_bandpass_filter,
                     ECG_FILTER_NUM_TAPS,
                     ecg_bandpass_coeffs,
                     ecg_filter_state,
                     RING_BUFFER_CAPACITY);

    arm_fir_f32(&ecg_bandpass_filter,
                clean_fetal_ecg,
                fetal_wideband,
                RING_BUFFER_CAPACITY);

    float signal_mean = 0.0f;
    float signal_std  = 0.0f;

    arm_mean_f32(fetal_wideband, RING_BUFFER_CAPACITY, &signal_mean);

    for (uint32_t i = 0U; i < RING_BUFFER_CAPACITY; i++)
    {
        fetal_wideband[i] -= signal_mean;
    }

    arm_std_f32(fetal_wideband, RING_BUFFER_CAPACITY, &signal_std);

    /* Recorded before the flatline test below, so a window rejected
     * there still reports the level that caused it. */
    if (output_vitals != NULL)
    {
        output_vitals->fetal_ecg_uv_std = signal_std;
        output_vitals->fetal_ecg_uv_pp  =
            peak_to_peak(fetal_wideband, RING_BUFFER_CAPACITY);
    }

    if (signal_std < ECG_FLATLINE_STD_UV)
    {
        /*
         * Electrode off, or no signal at all.
         *
         * This returns FALSE. It used to return true with all eight
         * features zeroed, which meant app.c saw a good window, did not
         * set FLAG_SENSOR_FAULT, and handed an all-zero vector to the
         * classifier -- roughly -13 sigma on LB once the scaler had
         * standardised it. The model duly returned a confident class,
         * and a disconnected electrode was reported to the phone as a
         * real clinical reading. Returning false routes this through
         * the rejected-window path, which is what it always was.
         */
        minute_window_ready = false;
        return false;
    }

    /*--------------------------------------------------------------
     * Fetal beat detection.
     *
     * scratch_pool[0] and [1] are free again now that the maternal
     * detector has finished with them.
     *-------------------------------------------------------------*/
    const BeatBandCfg_t fhr_cfg = { FHR_MIN_BPM, FHR_MAX_BPM };

    float fetal_ratio = 0.0f;

    uint16_t beat_count = detect_beats(clean_fetal_ecg,
                                       RING_BUFFER_CAPACITY,
                                       &fhr_cfg,
                                       scratch_pool[0],   /* band  */
                                       scratch_pool[1],   /* integ */
                                       beat_idx,
                                       rr_intervals_ms,
                                       MAX_BEATS,
                                       fetal_excl,   /* maternal QRS */
                                       &fetal_ratio);

    if (output_vitals != NULL)
    {
        /* Map the peak-to-baseline ratio onto 0..100 for the app. */
        float q = ((fetal_ratio - QRS_QUALITY_MIN_FETAL)
                   / (QRS_QUALITY_FULL_RATIO - QRS_QUALITY_MIN_FETAL))
                  * 100.0f;

        if (q < 0.0f)   { q = 0.0f;   }
        if (q > 100.0f) { q = 100.0f; }

        output_vitals->fetal_quality = (uint8_t)(q + 0.5f);

        /* Unclamped, unrescaled -- see fetal_quality_ratio in
         * wombcare_dsp.h for why this is kept alongside fetal_quality
         * rather than derived back from it. */
        output_vitals->fetal_quality_ratio = fetal_ratio;

        /* Raw, before the valid-range/jump filter below. Mirrors
         * mhr_beats -- recorded here, before any gate below can reject
         * the window, so a bench session sees this even on a rejection. */
        output_vitals->fetal_beats = beat_count;
    }

    if (beat_count < 3U)
    {
        minute_window_ready = false;
        return false;
    }

    /*--------------------------------------------------------------
     * Detector quality gate.
     *
     * A window whose "peaks" barely rise above their own baseline has
     * no QRS complexes in it; the detector was following noise. This is
     * the check that catches the phantom rhythm described above the
     * detect_beats() implementation, and it catches it regardless of
     * what rate that rhythm happens to land on.
     *-------------------------------------------------------------*/
    if (fetal_ratio < QRS_QUALITY_MIN_FETAL)
    {
        minute_window_ready = false;
        return false;
    }

    /*--------------------------------------------------------------
     * Valid-beat filter (Feature Spec §G2)
     *
     * Build the FHR (bpm) series and drop artifact beats before ANY
     * feature is computed: out-of-range rates, and jumps larger than
     * 25 bpm from the last accepted beat (a missed or doubled R-peak
     * produces exactly that signature).
     *
     * The jump chain is seeded from the MEDIAN of the raw rates, not
     * from the first beat. Seeding from the first beat meant a single
     * artefact at the head of the window made every genuine beat after
     * it fail the 25 bpm test, and the whole minute was discarded --
     * one bad beat could silently cost a good window.
     *
     * beat_idx is compacted alongside, so the beat timeline handed
     * to the trend stays correct after this loop. Durations must come
     * accumulating RR values: this loop drops beats, and the surviving
     * RR values no longer sum to wall-clock time.
     *-------------------------------------------------------------*/
    uint16_t valid_count = 0U;

    /* First pass: raw rates, range-checked only, to establish a robust
     * reference for the jump test. scratch_pool[1] is free (it was the
     * fetal integrator) and holds the median sort. */
    {
        uint16_t raw_n = 0U;

        for (uint16_t i = 0U; i < beat_count; i++)
        {
            if (rr_intervals_ms[i] <= 0.0f)
            {
                continue;   /* chain restart, carries no rate */
            }

            const float fhr = 60000.0f / rr_intervals_ms[i];

            if (fhr > FHR_MAX_BPM)
            {
                /* Reported to the app as a signal-quality hint. Windows
                 * full of these used to be published as confident
                 * 160-200 bpm readings. */
                if (output_vitals != NULL)
                {
                    output_vitals->fhr_high_rejected = true;
                }

                continue;
            }

            if (fhr >= FHR_MIN_BPM)
            {
                scratch_pool[0][raw_n] = fhr;
                raw_n++;
            }
        }

        if (raw_n < 3U)
        {
            minute_window_ready = false;
            return false;
        }

        float reference = median_of(scratch_pool[0], raw_n,
                                    scratch_pool[1]);

        /* Second pass: keep beats consistent with that reference, and
         * let the reference track the accepted series from there. */
        for (uint16_t i = 0U; i < beat_count; i++)
        {
            if (rr_intervals_ms[i] <= 0.0f)
            {
                continue;
            }

            const float fhr = 60000.0f / rr_intervals_ms[i];

            if ((fhr < FHR_MIN_BPM) || (fhr > FHR_MAX_BPM))
            {
                continue;
            }

            if (fabsf(fhr - reference) > FHR_MAX_JUMP_BPM)
            {
                continue;
            }

            /* In-place compaction: valid_count never overtakes i. */
            fhr_bpm[valid_count]         = fhr;
            beat_idx[valid_count]        = beat_idx[i];
            rr_intervals_ms[valid_count] = rr_intervals_ms[i];

            reference = fhr;
            valid_count++;
        }
    }

    beat_count = valid_count;

    if (beat_count < 3U)
    {
        minute_window_ready = false;
        return false;
    }

    /*--------------------------------------------------------------
     * RR Consistency Check
     *
     * Random/noise peaks may individually produce plausible FHR
     * values, but they should not form a stable physiological RR
     * sequence.
     *
     * On its own this is weaker than it looks -- the old detector's
     * blanking interval quantised its false intervals, so noise scored
     * BETTER here than a real heartbeat. It is kept as a cheap filter,
     * but the quality and yield gates are what actually do the work.
     *-------------------------------------------------------------*/
    float rr_mean = 0.0f;
    float rr_std  = 0.0f;

    arm_mean_f32(rr_intervals_ms, beat_count, &rr_mean);
    arm_std_f32(rr_intervals_ms, beat_count, &rr_std);

    if (rr_mean <= 0.0f)
    {
        minute_window_ready = false;
        return false;
    }

    const float rr_cv = rr_std / rr_mean;

    /*
     * Diagnostic candidate: what this window's fetal rate is BEFORE the
     * consistency gate below (and the coincidence/yield gates after it)
     * decide whether to publish it. Mirrors mhr_bpm_candidate -- written
     * here so a rejected window still shows what rate it was trending
     * towards on the serial log, not only that it was rejected.
     *
     * NOT a validated reading. features->lb_bpm, only meaningful when
     * this function returns true, is what a caller should trust.
     */
    if (output_vitals != NULL)
    {
        output_vitals->fetal_rr_cv       = rr_cv;
        output_vitals->fhr_bpm_candidate =
            median_of(fhr_bpm, beat_count, scratch_pool[2]);
    }

    if (rr_cv > RR_CV_MAX)
    {
        minute_window_ready = false;
        return false;
    }

    /*--------------------------------------------------------------
     * Maternal/fetal coincidence gate.
     *
     * If what the fetal channel found beats at the mother's rate, it is
     * the mother -- residual that survived both the LMS canceller and
     * the blanking above. Publishing it would put the mother's pulse on
     * screen labelled as the baby's, which is the single most dangerous
     * thing this device could do: it looks like a healthy reading
     * precisely when the fetal signal has been lost.
     *
     * Reported as no fetal lock, so it flows into the same
     * "not detected" path as any other loss of signal.
     *-------------------------------------------------------------*/
    if (maternal_ok && (maternal_bpm > 0.0f))
    {
        const float fetal_rate = 60000.0f / rr_mean;

        if (fabsf(fetal_rate - maternal_bpm) < MHR_FHR_COINCIDENCE_BPM)
        {
            if (output_vitals != NULL)
            {
                output_vitals->fetal_is_maternal = true;
            }

            minute_window_ready = false;
            return false;
        }
    }

    /*--------------------------------------------------------------
     * Beat-yield gate.
     *
     * At the rate these intervals imply, a window this long should
     * contain roughly this many beats. A detector following a real
     * rhythm gets close; one chasing noise loses most of its
     * candidates to the filters above and falls well short.
     *-------------------------------------------------------------*/
    {
        const float expected = (60000.0f / rr_mean) * WINDOW_MINUTES;

        if ((expected > 0.0f) &&
            (((float)beat_count / expected) < BEAT_YIELD_MIN))
        {
            minute_window_ready = false;
            return false;
        }
    }

    if (output_vitals != NULL)
    {
        output_vitals->fetal_lock = true;
    }

    /*--------------------------------------------------------------
     * PVDF fetal movement -- counted for THIS window only.
     *
     * Runs ahead of the feature block (it used to sit after it) because
     * the trend needs the raw count at append time. scratch_pool[0] is
     * free from here on: it last held the QRS band-pass copy the beat
     * detector worked from.
     *-------------------------------------------------------------*/
    const float window_kicks = count_pvdf_kicks(scratch_pool[0]);

    /*--------------------------------------------------------------
     * Hand this window's beats to the rolling trend, then build the
     * feature vector from the whole retained span instead of from this
     * one minute.
     *
     * The window CADENCE is unchanged -- a fresh vector is still
     * produced every 60 s, so BLE, the app and the alert logic see
     * exactly the timing they saw before. What changes is that each
     * vector now describes up to TREND_SPAN_MS of history, which is the
     * span the model was actually trained on. See wombcare_trend.h.
     *
     * During the first few minutes of a session the trend holds less
     * than a full span; compute_features() is told how much observation
     * time really backs the series and normalises the count features to
     * it, so early vectors are correct, just noisier -- which is the
     * behaviour the device had for every window before this change.
     *-------------------------------------------------------------*/
    wombcare_trend_add_beats(fhr_bpm,
                             beat_idx,
                             beat_count,
                             SAMPLE_PERIOD_MS,
                             window_kicks);

    compute_features(wombcare_trend_fhr(),
                     wombcare_trend_time_ms(),
                     wombcare_trend_beat_count(),
                     wombcare_trend_observed_minutes(),
                     wombcare_trend_kicks(),
                     output_features);


    minute_window_ready = false;
    return true;
}



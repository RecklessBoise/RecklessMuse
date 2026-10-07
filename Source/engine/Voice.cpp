#include "Voice.h"

namespace rm
{
using namespace dsp;
using namespace choices;

namespace
{
int lfoShape (int wave) { return wave == 6 ? ShapeOscillator::Smooth : wave; }

int pitchLfoShape (int wave)
{
    static constexpr int map[] { ShapeOscillator::Sine, ShapeOscillator::Triangle, ShapeOscillator::Square,
                                 ShapeOscillator::SampleHold };
    return map[std::clamp (wave, 0, 3)];
}

constexpr float kLog2Mid = 9.30482f; // log2(632.5 Hz), geometric centre of the cutoff range
constexpr float kEnvOctaves = 7.0f;
} // namespace

void Voice::prepare (double internalSampleRate, int voiceIndex)
{
    fs = (float) internalSampleRate;
    index = voiceIndex;
    rng = Random (0xA5A5u + 7919u * (uint32_t) (voiceIndex + 1));

    // Free-running oscillators with random start phase, like a real analog poly.
    osc1.reset (rng.unipolar());
    osc2.reset (rng.unipolar());
    modOsc.reset (rng.unipolar(), rng.next());
    lfo1.reset (rng.unipolar(), rng.next());
    lfo2.reset (rng.unipolar(), rng.next());
    pitchLfo.reset (rng.unipolar(), rng.next());
    drift.reset (rng.unipolar(), rng.next());
    noise.reset (rng.next());
    filter1.reset();
    filter2.reset();

    tolTune1 = rng.bipolar();
    tolTune2 = rng.bipolar();
    tolCutoff = rng.bipolar();

    filterEnv.setSampleRate (internalSampleRate);
    ampEnv.setSampleRate (internalSampleRate);
    filterEnv.kill();
    ampEnv.kill();
    active = gate = false;
}

void Voice::start (int newNote, float vel, float startPitch, bool useGlide, const TimbreState& s)
{
    const bool wasActive = active;
    note = newNote;
    velocity = vel;
    targetPitch = (float) newNote;
    glideThisNote = useGlide && s.glide > 0.0005f;
    pitch = glideThisNote ? startPitch : targetPitch;
    randomValue = rng.bipolar();
    age = ++ageCounter;

    if (s.moKeyReset) modOsc.resetPhase();
    if (s.l1Reset) lfo1.resetPhase();
    if (s.l2Reset) lfo2.resetPhase();

    if (! wasActive)
    {
        filter1.reset();
        filter2.reset();
        firstTick = true;
    }

    active = gate = true;
    ctrlCountdown = 0;
    filterEnv.gateOn();
    ampEnv.gateOn();
}

void Voice::legatoTo (int newNote, bool useGlide)
{
    note = newNote;
    targetPitch = (float) newNote;
    glideThisNote = useGlide;
    if (! useGlide)
        pitch = targetPitch;
    age = ++ageCounter;
}

void Voice::release()
{
    gate = false;
    filterEnv.gateOff();
    ampEnv.gateOff();
}

void Voice::kill()
{
    active = gate = false;
    filterEnv.kill();
    ampEnv.kill();
}

void Voice::updateControl (const TimbreState& s, const PerformanceState& perf)
{
    const float ctrlDt = (float) kControlInterval / fs;

    // Glide (exponential, like an RC portamento circuit)
    if (glideThisNote && s.glide > 0.0005f)
        pitch = targetPitch + (pitch - targetPitch) * std::exp (-ctrlDt / (s.glide * 0.35f));
    else
        pitch = targetPitch;

    // Envelopes follow the current settings
    filterEnv.setParameters (s.feA, s.feD, s.feS, s.feR, s.feLoop);
    ampEnv.setParameters (s.aeA, s.aeD, s.aeS, s.aeR, s.aeLoop);

    // Control-rate modulation sources
    lfo1Value = lfo1.process (s.l1Hz * lfo1RateMul * ctrlDt, lfoShape (s.l1Wave)) * s.l1Amp;
    lfo2Value = lfo2.process (s.l2Hz * lfo2RateMul * ctrlDt, lfoShape (s.l2Wave)) * s.l2Amp;
    const float vib = pitchLfo.process (s.plHz * ctrlDt, pitchLfoShape (s.plWave))
                      * s.plAmt * 2.0f * (s.plWheel ? perf.modWheel : 1.0f);
    const float driftSemis = drift.process (0.35f * ctrlDt, ShapeOscillator::Smooth) * s.vintage * 0.07f;

    float src[numModSources] {};
    src[srcLfo1] = lfo1Value;
    src[srcLfo2] = lfo2Value;
    src[srcModOsc] = lastModOsc;
    src[srcFiltEnv] = filterEnv.getLevel();
    src[srcVcaEnv] = ampEnv.getLevel();
    src[srcVelocity] = velocity;
    src[srcModWheel] = perf.modWheel;
    src[srcAftertouch] = perf.aftertouch;
    src[srcKeyTrack] = (pitch - 60.0f) / 60.0f;
    src[srcRandom] = randomValue;
    src[srcMacro1] = perf.macro1;
    src[srcMacro2] = perf.macro2;

    float dst[numModDests] {};
    for (int i = 0; i < kNumMatrixSlots; ++i)
    {
        const int sIdx = s.mmSrc[i], dIdx = s.mmDst[i];
        if (sIdx <= srcOff || dIdx <= dstOff || sIdx >= numModSources || dIdx >= numModDests)
            continue;
        const int via = s.mmVia[i];
        const float viaGain = (via > srcOff && via < numModSources) ? src[via] : 1.0f;
        dst[dIdx] += src[sIdx] * s.mmAmt[i] * viaGain;
    }

    lfo1RateMul = std::exp2 (std::clamp (dst[dstL1Rate] * 4.0f, -8.0f, 8.0f));
    lfo2RateMul = std::exp2 (std::clamp (dst[dstL2Rate] * 4.0f, -8.0f, 8.0f));

    // Pitch (in MIDI note units)
    const float bend = perf.pitchBend * s.bendRange;
    const float common = pitch + bend + driftSemis + unisonDetune + dst[dstPitch] * 24.0f;
    const float vib1 = s.plDest != 1 ? vib : 0.0f;
    const float vib2 = s.plDest != 0 ? vib : 0.0f;
    const float p1 = common + (float) (s.o1Oct - 2) * 12.0f + s.o1Freq + vib1 + dst[dstO1Pitch] * 24.0f
                     + tolTune1 * s.vintage * 0.05f;
    const float base2 = s.o2KbTrack ? common : 48.0f + bend + driftSemis;
    const float p2 = base2 + (float) (s.o2Oct - 2) * 12.0f + s.o2Freq + vib2 + dst[dstO2Pitch] * 24.0f
                     + tolTune2 * s.vintage * 0.05f;

    // Mod oscillator
    float moHz = s.moFreq * std::exp2 (std::clamp (dst[dstModRate] * 6.0f, -12.0f, 12.0f));
    if (s.moKbTrack)
        moHz *= std::exp2 ((pitch - 60.0f) / 12.0f);
    moInc = moHz / fs;
    moPitchSemis = s.moPitchAmt * 24.0f;
    moFilterOct = s.moFilterAmt * 6.0f;
    moPwm = s.moPwmAmt;
    moVca = s.moVcaAmt;

    // Filters (log2 Hz)
    const float keyOct = (pitch - 60.0f) / 12.0f;
    const float tolOct = tolCutoff * s.vintage * 0.12f;
    const float cutAll = dst[dstCutoff] * 8.0f;
    const float c1 = std::log2 (s.f1Cutoff) + s.f1Kb * keyOct + cutAll + dst[dstF1Cutoff] * 8.0f + tolOct;
    const float f2Base = s.fLink ? std::log2 (s.f1Cutoff) + (std::log2 (s.f2Cutoff) - kLog2Mid)
                                 : std::log2 (s.f2Cutoff);
    const float c2 = f2Base + s.f2Kb * keyOct + cutAll + dst[dstF2Cutoff] * 8.0f - tolOct;
    env1Oct = s.f1Env * kEnvOctaves;
    env2Oct = s.f2Env * kEnvOctaves;
    filterVelScale = 1.0f - s.feVel + s.feVel * velocity;
    ampVelScale = 1.0f - s.aeVel + s.aeVel * velocity;

    // Resonance curve: self-oscillation starts around 88 %, and gets louder toward 100 %.
    auto resToK = [] (float r) { return 4.75f * std::pow (std::clamp (r, 0.0f, 1.0f), 1.15f); };
    const float k1 = resToK (s.f1Res + dst[dstF1Res]);
    const float k2 = resToK (s.f2Res + dst[dstF2Res]);

    // Mixer
    const float ovl = std::clamp (s.mixOverload + dst[dstOverload], 0.0f, 1.0f);
    overloadBias = 0.25f * ovl;
    overloadBiasOut = std::tanh (overloadBias);

    // VCA + pan (equal power)
    vcaLevel = std::clamp (s.vcaLevel + dst[dstVca], 0.0f, 1.0f);
    const float pan = std::clamp (s.vcaPan + s.vcaSpread * spreadPos + unisonPan + dst[dstPan], -1.0f, 1.0f);
    const float angle = (pan + 1.0f) * (kPi * 0.25f);

    float target[numRamps];
    target[rP1] = p1;
    target[rP2] = p2;
    target[rC1] = c1;
    target[rC2] = c2;
    target[rK1] = k1;
    target[rK2] = k2;
    target[rW1] = std::clamp (s.o1Wave + dst[dstO1Wave], 0.0f, 1.0f);
    target[rW2] = std::clamp (s.o2Wave + dst[dstO2Wave], 0.0f, 1.0f);
    target[rFm] = std::clamp (s.fmAmt + dst[dstFm], 0.0f, 1.0f);
    target[rL1] = std::clamp (s.mixO1 + dst[dstO1Level], 0.0f, 1.0f);
    target[rL2] = std::clamp (s.mixO2 + dst[dstO2Level], 0.0f, 1.0f);
    target[rLR] = s.mixRing;
    target[rLM] = s.mixMod;
    target[rLN] = std::clamp (s.mixNoise + dst[dstNoise], 0.0f, 1.0f);
    target[rOvl] = ovl;
    target[rGainL] = std::cos (angle) * 1.41421356f;
    target[rGainR] = std::sin (angle) * 1.41421356f;

    if (firstTick)
    {
        for (int i = 0; i < numRamps; ++i)
        {
            cur[i] = target[i];
            step[i] = 0.0f;
        }
        firstTick = false;
    }
    else
    {
        for (int i = 0; i < numRamps; ++i)
            step[i] = (target[i] - cur[i]) * (1.0f / (float) kControlInterval);
    }
}

void Voice::render (float* left, float* right, int numSamples, const TimbreState& s, const PerformanceState& perf)
{
    if (! active)
        return;

    const float invFs = 1.0f / fs;
    const float maxHz = 0.45f * fs;
    const float a4Inc = 440.0f * invFs;
    const bool stereoRouting = s.fRouting == 2;
    const bool parallel = s.fRouting == 1;
    const auto filter1Mode = s.f1Mode == 1 ? LadderFilter::HighPass : LadderFilter::LowPass;
    const int poles = s.fOrder;
    const bool moUni = s.moUnipolar;
    const int moShape = s.moWave;
    const bool pitchTo1 = s.moPitchDest != 1, pitchTo2 = s.moPitchDest != 0;
    const bool filtTo1 = s.moFilterDest != 1, filtTo2 = s.moFilterDest != 0;
    const int fmSource = s.fmSrc;
    const bool sync = s.o2Sync;

    for (int n = 0; n < numSamples; ++n)
    {
        if (--ctrlCountdown <= 0)
        {
            updateControl (s, perf);
            ctrlCountdown = kControlInterval;
        }
        for (int i = 0; i < numRamps; ++i)
            cur[i] += step[i];

        const float envF = filterEnv.process() * filterVelScale;
        const float envA = ampEnv.process();

        // Mod oscillator (audio-rate capable)
        float mo = modOsc.process (moInc, moShape);
        if (moUni) mo = 0.5f * (mo + 1.0f);
        lastModOsc = mo;

        // VCO pitch
        const float pm = mo * moPitchSemis;
        float inc1 = fastExp2 ((cur[rP1] + (pitchTo1 ? pm : 0.0f) - 69.0f) * (1.0f / 12.0f)) * a4Inc;
        float inc2 = fastExp2 ((cur[rP2] + (pitchTo2 ? pm : 0.0f) - 69.0f) * (1.0f / 12.0f)) * a4Inc;

        const float fm = cur[rFm] * 3.0f;
        if (fm > 0.0f)
        {
            switch (fmSource)
            {
                case 0: inc2 *= 1.0f + fm * lastOsc1; break;
                case 1: inc1 *= 1.0f + fm * mo; break;
                case 2: inc2 *= 1.0f + fm * mo; break;
                default: inc1 *= 1.0f + fm * mo; inc2 *= 1.0f + fm * mo; break;
            }
        }

        const float pwm = mo * moPwm;
        SyncEvent syncEvent;
        const float o1 = osc1.process (inc1, cur[rW1], pwm, nullptr, &syncEvent);
        const float o2 = osc2.process (inc2, cur[rW2], pwm, sync ? &syncEvent : nullptr, nullptr);
        lastOsc1 = o1;

        // Mixer -> overload (asymmetric soft saturation, like overdriving the Moog mixer)
        const float nz = noise.process();
        const float mix = o1 * cur[rL1] + o2 * cur[rL2] + o1 * o2 * cur[rLR] + mo * cur[rLM] + nz * cur[rLN];
        const float ovl = cur[rOvl];
        const float drive = 1.0f + 7.0f * ovl * ovl;
        const float driven = fastTanh (mix * 0.55f * drive + overloadBias) - overloadBiasOut;
        // A faint noise floor (-74 dB), like the real circuit: lets the ladder self-oscillate on its own.
        const float filterIn = driven * (1.25f - 0.35f * ovl) + nz * 2.0e-4f;

        // Filters
        const float moF = mo * moFilterOct;
        const float hz1 = std::clamp (fastExp2 (cur[rC1] + envF * env1Oct + (filtTo1 ? moF : 0.0f)), 8.0f, maxHz);
        const float hz2 = std::clamp (fastExp2 (cur[rC2] + envF * env2Oct + (filtTo2 ? moF : 0.0f)), 8.0f, maxHz);
        const float g1 = prewarp (kPi * hz1 * invFs);
        const float g2 = prewarp (kPi * hz2 * invFs);

        float outL, outR;
        if (stereoRouting)
        {
            outL = filter1.process (filterIn, g1, cur[rK1], poles, filter1Mode);
            outR = filter2.process (filterIn, g2, cur[rK2], poles, LadderFilter::LowPass);
        }
        else if (parallel)
        {
            outL = outR = 0.7f * (filter1.process (filterIn, g1, cur[rK1], poles, filter1Mode)
                                  + filter2.process (filterIn, g2, cur[rK2], poles, LadderFilter::LowPass));
        }
        else
        {
            const float y1 = filter1.process (filterIn, g1, cur[rK1], poles, filter1Mode);
            outL = outR = filter2.process (y1, g2, cur[rK2], poles, LadderFilter::LowPass);
        }

        // Safety net: a non-finite value would otherwise latch the filters forever.
        if (! std::isfinite (outL) || ! std::isfinite (outR))
        {
            filter1.reset();
            filter2.reset();
            osc1.reset (0.0f);
            osc2.reset (0.5f);
            outL = outR = 0.0f;
        }

        // VCA
        const float trem = 1.0f - moVca * (moUni ? 1.0f - mo : 0.5f - 0.5f * mo);
        const float amp = envA * ampVelScale * vcaLevel * trem * 0.8f;
        left[n] += outL * amp * cur[rGainL];
        right[n] += outR * amp * cur[rGainR];
    }

    if (! ampEnv.isActive())
    {
        active = gate = false;
        filterEnv.kill();
    }
}
} // namespace rm

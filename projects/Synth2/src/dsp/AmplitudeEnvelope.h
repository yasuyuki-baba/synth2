#pragma once

#include "ReleaseRamp.h"
#include "EnvelopeCurve.h"
#include "FractionalDial.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace synth2::dsp
{
    // Win64 amplitude control: CEnvelopedObject (0xccc0) and CVca (0x37c30).
    // Gain is normalized to a peak of one, with optional target scale and fractional modulation.
    class AmplitudeEnvelope
    {
    public:
        explicit AmplitudeEnvelope (bool useVectorRamp = false) noexcept : vectorRamp (useVectorRamp) {}
        enum class Phase { Idle, Attack, Decay, Sustain, Release };
        static float gainForDial(int dial) noexcept
        {
            dial=std::clamp(dial,0,127);
            if(dial==0)return 0;
            const float exponent=std::pow(static_cast<float>(dial)*(1.0f/127.0f),0.3f);
            return static_cast<float>(std::exp(std::log(2048.0f)*exponent)*(1.0f/2048.0f));
        }
        static float velocityForDials(int velocity,int sensitivity) noexcept
        {
            velocity=std::clamp(velocity,0,127);sensitivity=std::clamp(sensitivity,0,127);
            const float amount=(static_cast<float>(sensitivity)*(1.0f/127.0f))*(-30.0f);
            float exponent=static_cast<float>(amount)*static_cast<float>(127-velocity);
            exponent*=1.0f/127.0f;exponent*=0.05f;
            return static_cast<float>(std::exp(std::log(10.0f)*exponent));
        }

        static float gainForDial(float dial) noexcept
        {return interpolateDial(dial,[](int value){return gainForDial(value);});}
        static float velocityForDials(int velocity,float sensitivity) noexcept
        {return interpolateDial(sensitivity,[&](int value){return velocityForDials(velocity,value);});}

        void setParameters (int attackFrames,int decayFrames,float sustainLevel,int releaseFrames) noexcept
        {
            attackFrames = std::max (0,attackFrames);
            decayFrames = std::max (0,decayFrames);
            if (phase == Phase::Attack && attack != attackFrames)
                counter = attack > 0 ? static_cast<int> (static_cast<int64_t>(counter)*attackFrames/attack) : 0;
            if (phase == Phase::Decay && decay != decayFrames)
                counter = decay > 0 ? static_cast<int> (static_cast<int64_t>(counter)*decayFrames/decay) : 0;
            attack = attackFrames; decay = decayFrames;
            sustain = std::clamp (sustainLevel,0.0f,2048.0f);
            releaseFrames = std::max (0,releaseFrames);
            releaseDuration = releaseFrames;
            release.setDuration (releaseFrames);
            if (phase == Phase::Sustain) level = sustain;
        }

        void reset() noexcept
        {
            phase = Phase::Idle; level = counter = boundary = 0;
            currentGain = targetGain = 0; rampRemaining=0; lastLevel = -1; release.reset();
        }
        bool isActive() const noexcept { return phase != Phase::Idle; }
        Phase getPhase() const noexcept { return phase; }
        float getLevel() const noexcept { return level; }
        int getCounter() const noexcept { return phase == Phase::Release ? release.getCounter() : counter; }
        float getCurrentLevel() const noexcept { return static_cast<float>(level)/2048.0f; }
        // Offline diagnostics; callers serialize with rendering.
        std::array<float,3> readGainState() const noexcept { return {currentGain,targetGain,gainScale}; }
        float getCachedLevel() const noexcept { return lastLevel; }
        bool wasSilent() const noexcept { return silent; }

        void setGainScale (float value) noexcept
        {
            setGainFactors(1.0f,value);
        }
        // CVca applies the two native factors before normalizing its target.
        // Keep their product order while retaining Float32 signal precision.
        void setGainFactors(float first,float second) noexcept
        {
            first=std::isfinite(first) ? std::max(0.0f,first) : 0.0f;
            second=std::isfinite(second) ? std::max(0.0f,second) : 0.0f;
            if(first!=gainFirst || second!=gainScale){gainFirst=first;gainScale=second;lastLevel=-1;}
        }

        void setModulation (float first,float second) noexcept
        {
            // A reduced random-LFO depth retains its old target. Native CLFO
            // queries can then be negative; CVca consumes that signed value.
            if (first!=modulation1 || second!=modulation2)
            {
                modulation1=first;modulation2=second;lastLevel=-1;
            }
        }

        void noteOn() noexcept
        {
            phase = Phase::Attack;
            // Restart position is discrete compatibility metadata. Keep its
            // whole-level anchor separate from the real audio level/gain.
            const float restartLevel = std::trunc(level);
            counter = static_cast<int> (static_cast<float>(attack)*restartLevel/2048);
            release.reset();
        }
        void noteOff() noexcept
        {
            if (phase == Phase::Idle || phase == Phase::Release) return;
            release.start (releaseDuration,level);
            phase = Phase::Release;
        }

        // An explicit gain horizon may outlive this buffer. Envelope stage
        // timing remains separate from the pending sustain-gain interpolation.
        // Storage must describe its real alignment: the native SSE ramp uses
        // four independent additions, with a scalar alignment prefix and tail.
        void render (float* gains,int frames,int gainHorizon = 0) noexcept
        {
            silent=true;
            // The native envelope's zero-duration release exit keeps its
            // non-silent dispatch flag, even though it invokes no CVca segment.
            if(phase==Phase::Release && releaseDuration==0)silent=false;
            int offset = 0;
            if (phase == Phase::Release)
            {
                release.advance (frames,[&] (float target,int count)
                {
                    segment (gains+offset,count,target,release.segmentHorizon());
                    offset += count;
                },gainHorizon);
                level = release.getLevel();
                if (!release.isActive()) { phase = Phase::Idle; counter = release.getCounter(); currentGain = 0; }
            }
            while (offset < frames && phase != Phase::Release)
            {
                const int available = frames-offset;
                if (phase == Phase::Attack)
                {
                    if (counter < attack)
                    {
                        const int count = std::min (available,attack-counter);
                        level = static_cast<float>(counter+count)*2048.0f/attack;
                        segment (gains+offset,count,level);
                        offset += count; counter += count;
                    }
                    if (counter >= attack)
                    {
                        phase = Phase::Decay; level = 2048; counter = boundary = 0;
                        updateResolution();
                        if (decay == 0 || level <= sustain) { phase = Phase::Sustain; level = sustain; }
                    }
                }
                else if (phase == Phase::Decay)
                {
                    while (offset < frames && counter < decay && level > sustain)
                    {
                        const int count = std::min ({frames-offset,step,decay-counter});
                        level = compatibleControlCurve(2048-sustain,[&](int range) {
                            return exponentialEnvelopeKnot(range,decay-counter-count,decay);
                        })+sustain;
                        segment (gains+offset,count,level);
                        offset += count; counter += count;
                        if (counter >= boundary) updateResolution();
                    }
                    if (counter >= decay || level <= sustain)
                    {
                        phase = Phase::Sustain; level = sustain; counter = 0;
                    }
                }
                else
                {
                    if (phase == Phase::Sustain) segment (gains+offset,available,sustain,gainHorizon);
                    else std::fill_n (gains+offset,available,0.0f);
                    offset = frames;
                }
            }
            if (offset < frames) std::fill_n (gains+offset,frames-offset,0.0f);
        }

    private:
        void updateResolution() noexcept
        {
            if      (boundary >= decay/2)  boundary = decay;
            else if (boundary >= decay/3)  boundary = decay/2;
            else if (boundary >= decay/4)  boundary = decay/3;
            else if (boundary >= decay/10) boundary = decay/4;
            else if (boundary >= decay/20) boundary = decay/10;
            else                          boundary = decay/20;
            step = std::max (1,boundary/2);
        }

        void segment (float* output,int count,float target,int horizon = 0) noexcept
        {
            if (count <= 0) return;
            if (lastLevel == target && rampRemaining==0)
            {
                if(targetGain!=0.0f)silent=false;
                std::fill_n (output,count,targetGain);
                return;
            }
            // CVca returns non-silent for every changed-control dispatch,
            // even when its new gain and every rendered sample are zero.
            silent=false;
            if(lastLevel!=target)
            {
                targetGain=static_cast<float>(target)*gainFirst;
                targetGain*=gainScale;
                targetGain*=1.0f/2048.0f;
                if (modulation1!=0 || modulation2!=0)
                {
                    const float ratio=static_cast<float>((2048-modulation1)*(2048-modulation2))*(1.0f/4194304.0f);
                    targetGain=ratio*targetGain;
                }
                rampRemaining=horizon>0 ? horizon : count;
                rampDelta=(targetGain-currentGain)/static_cast<float>(rampRemaining);
            }
            const int total=count;
            count=std::min(count,rampRemaining);
            const float delta=rampDelta;
            int i = 0;
            if (vectorRamp && count >= 7)
            {
                while ((reinterpret_cast<uintptr_t>(output+i)&15u) != 0)
                {
                    currentGain += delta;
                    output[i++] = currentGain;
                }
                float lane3 = currentGain;
                float lane2 = lane3-delta;
                float lane1 = lane2-delta;
                float lane0 = lane1-delta;
                const float four = delta*4.0f;
                for (; i < count-3; i += 4)
                {
                    lane0 += four; lane1 += four; lane2 += four; lane3 += four;
                    output[i] = lane0; output[i+1] = lane1; output[i+2] = lane2; output[i+3] = lane3;
                }
                currentGain = lane3;
            }
            for (; i < count; ++i) { currentGain += delta; output[i] = currentGain; }
            rampRemaining-=count;
            std::fill(output+count,output+total,targetGain);
            lastLevel = target;
        }

        int rampRemaining {};
        float rampDelta {};
        bool vectorRamp {};
        bool silent {true};
        ReleaseRamp release;
        Phase phase {Phase::Idle};
        int attack {},decay {},releaseDuration {};
        int counter {},boundary {},step {1};
        float sustain {2048},level {},lastLevel {-1};
        float currentGain {},targetGain {};
        float gainScale {1.0f},gainFirst {1.0f};
        float modulation1 {},modulation2 {};
    };
}

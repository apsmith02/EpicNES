#include "apu.h"
#include <assert.h>
#include <string.h>

static const uint8_t APU_LENGTH_TABLE[32] = {
    10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

uint8_t _APU_LengthLoad(uint8_t regWriteVal) {
    return APU_LENGTH_TABLE[regWriteVal >> 3];
}


void APU_Init(APU *apu, double cpuClockMHz, double sampleRateHz)
{
    memset(apu, 0, sizeof(APU));
    apu->cpuCyclesPerSample = (cpuClockMHz * 1000000) / sampleRateHz;
    apu->master_volume = 0.5;
}

void APU_PowerOn(APU *apu)
{
    APUState* state = &apu->state;
    
    for (uint16_t r = 0x4000; r <= 0x4013; r++)
        APU_Write(apu, 0x4000, 0);
    APU_Write(apu, 0x4015, 0);
    APU_Write(apu, 0x4017, 0);
    state->fc_cycles = 0;
}

void APU_Reset(APU* apu) {
    APUState* state = &apu->state;

    APU_Write(apu, 0x4015, 0);
}

uint8_t APU_Read(APU *apu, uint16_t addr)
{
    APUState* state = &apu->state;
    if (addr == 0x4015) {
        return state->status |
        (state->ch_pulse1.length > 0)       |
        (state->ch_pulse2.length > 0) << 1;
    }
    return 0;
}

void APU_Write(APU *apu, uint16_t addr, uint8_t data)
{
    APUState* state = &apu->state;
    switch (addr) {
        case 0x4000: _APUPulse_Write0(&state->ch_pulse1, data); break;
        case 0x4001: _APUPulse_Write1(&state->ch_pulse1, data); break;
        case 0x4002: _APUPulse_Write2(&state->ch_pulse1, data); break;
        case 0x4003: _APUPulse_Write3(&state->ch_pulse1, data, (state->status & APU_STATUS_1) > 0); break;
        case 0x4004: _APUPulse_Write0(&state->ch_pulse2, data); break;
        case 0x4005: _APUPulse_Write1(&state->ch_pulse2, data); break;
        case 0x4006: _APUPulse_Write2(&state->ch_pulse2, data); break;
        case 0x4007: _APUPulse_Write3(&state->ch_pulse2, data, (state->status & APU_STATUS_2) > 0); break;
        case 0x4015:
            state->status &= ~APU_STATUS_I;
            state->status &= 0xE0;
            state->status |= data & 0x1F;
            if ((data & APU_STATUS_1) == 0) { //Pulse 1 disable
                state->ch_pulse1.length = 0;
            }
            if ((data & APU_STATUS_2) == 0) { //Pulse 2 disable
                state->ch_pulse2.length = 0;
            }
            break;
        case 0x4017: 
            state->fc_ctrl = data;
            //Side effects: Reset FC timer, and if the 5-step flag is set, generate quarter and half frame signals
            state->fc_cycles = 0;
            if (data & FC_5STEP) {
                _APU_FC_ClockQuarterFrame(apu);
                _APU_FC_ClockHalfFrame(apu);
            }
            break;
        default: break;
    }
}

void APU_CPUCycle(APU *apu)
{
    //Audio output
    apu->cycleSampleTimer++;
    if (apu->cycleSampleTimer >= apu->cpuCyclesPerSample) {
        apu->cycleSampleTimer -= apu->cpuCyclesPerSample;
        
        assert(apu->sampleBufferSize < APU_SAMPLE_CAPACITY);
        apu->sampleBuffer[apu->sampleBufferSize++] = (short)(INT16_MAX * _APU_MixAudio(apu));
    }

    //Clock frame counter
    _APU_FC_Clock(apu);
}

void *APU_GetAudioBuffer(APU *apu, size_t *len)
{
    *len = apu->sampleBufferSize * sizeof(short);
    return &apu->sampleBuffer[0];
}

void APU_ClearAudioBuffer(APU *apu)
{
    apu->sampleBufferSize = 0;
}

double _APU_MixAudio(APU *apu)
{
    APUState* state = &apu->state;
    double pulse1 = _APUPulse_Output(&state->ch_pulse1, true);
    double pulse2 = _APUPulse_Output(&state->ch_pulse2, false);
    double pulse_out = 95.88 / ((8128 / (pulse1 + pulse2)) + 100);
    return (pulse_out) * apu->master_volume;
}

void _APU_FC_Clock(APU *apu)
{
    APUState* state = &apu->state;

    //Clock pulse waves every APU cycle (2 CPU cycles)
    if (state->fc_cycles % 2 == 0) {
        _APUPulse_ClockWave(&state->ch_pulse1);
        _APUPulse_ClockWave(&state->ch_pulse2);
    }
    
    //Frame counter sequencer
    if (apu->state.fc_ctrl & FC_5STEP) {
        //5-step sequence
        switch (state->fc_cycles) {
            case 7457: //Step 1 at 3728.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                break;
            case 14913: //Step 2 at 7456.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                _APU_FC_ClockHalfFrame(apu);
                break;
            case 22371: //Step 3 at 11185.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                break;
            case 37281: //Step 5 at 18640.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                _APU_FC_ClockHalfFrame(apu);
                break;
            default: break;
        }
        state->fc_cycles = (state->fc_cycles + 1) % 37282;
    } else {
        //4-step sequence
        switch (state->fc_cycles) {
            case 7457: //Step 1 at 3728.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                break;
            case 14913: //Step 2 at 7456.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                _APU_FC_ClockHalfFrame(apu);
                break;
            case 22371: //Step 3 at 11185.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                break;
            case 29829: //Step 4 at 14914.5 APU cycles
                _APU_FC_ClockQuarterFrame(apu);
                _APU_FC_ClockHalfFrame(apu);
                break;
            default: break;
        }
        state->fc_cycles = (state->fc_cycles + 1) % 29830;
    }
}

void _APU_FC_ClockQuarterFrame(APU *apu)
{
    APUState* state = &apu->state;
    _APUEnv_Clock(&state->ch_pulse1.envelope);
    _APUEnv_Clock(&state->ch_pulse2.envelope);
}

void _APU_FC_ClockHalfFrame(APU *apu)
{
    APUState* state = &apu->state;
    _APUPulse_ClockLength(&state->ch_pulse1);
    _APUPulse_ClockLength(&state->ch_pulse2);
    _APUPulse_ClockSweep(&state->ch_pulse1, true);
    _APUPulse_ClockSweep(&state->ch_pulse2, false);
}

void _APUEnv_Clock(APUEnvelope *env)
{
    if (!env->start) {
        //Clock divider
        if (env->divider == 0) {
            env->divider = env->period;
            //Clock decay
            if (env->decay > 0) {
                env->decay--;
            } else if (env->loop) {
                env->decay = 15; //Loop
            }
        } else {
            env->divider--;
        }
    } else {
        //Reload decay and divider
        env->start = false;
        env->decay = 15;
        env->divider = env->period;
    }
}

void _APUPulse_ClockWave(APUPulse *pulse)
{
    if (pulse->timer == 0) {
        pulse->timer = pulse->period;
        //Clock sequencer
        if (pulse->sequencer > 0)
            pulse->sequencer--;
        else
            pulse->sequencer = 7;
    } else {
        pulse->timer--;
    }
}

void _APUPulse_ClockLength(APUPulse *pulse)
{
    if (pulse->length > 0 && !pulse->envelope.loop)
        pulse->length--;
}

void _APUPulse_ClockSweep(APUPulse *pulse, bool isCh1)
{
    APUSweep* sweep = &pulse->sweep;

    if (sweep->divider == 0 && sweep->enabled && sweep->shift > 0 &&
    !_APUPulse_SweepMute(pulse, isCh1)) {
        pulse->period = _APUPulse_SweepTargetPeriod(pulse, isCh1);
    }
    
    if (sweep->divider == 0 || sweep->reload) {
        sweep->divider = sweep->period;
        sweep->reload = false;
    } else {
        sweep->divider--;
    }
}

uint16_t _APUPulse_SweepTargetPeriod(APUPulse *pulse, bool isCh1)
{
    int16_t target = pulse->period >> pulse->sweep.shift;
    if (pulse->sweep.negate) {
        target = -target - isCh1;
    }
    target += pulse->period;
    return (target >= 0) ? target : 0;
}

bool _APUPulse_SweepMute(APUPulse *pulse, bool isCh1)
{
    return pulse->period < 8 || _APUPulse_SweepTargetPeriod(pulse, isCh1) > 0x7FF;
}

void _APUPulse_Write0(APUPulse *pulse, uint8_t data)
{
    pulse->duty = data >> 6;
    pulse->envelope.loop = (data & 0x20) >> 5;
    pulse->envelope.constant_volume = (data & 0x10) >> 4;
    pulse->envelope.period = data & 0x0F;
}

void _APUPulse_Write1(APUPulse *pulse, uint8_t data)
{
    APUSweep* sweep = &pulse->sweep;

    sweep->enabled = (data & 0x80) >> 7;
    sweep->period = (data & 0x70) >> 4;
    sweep->negate = (data & 0x08) >> 3;
    sweep->shift = (data & 0x07);

    sweep->reload = true;
}

void _APUPulse_Write2(APUPulse *pulse, uint8_t data)
{
    pulse->period = data;
}

void _APUPulse_Write3(APUPulse *pulse, uint8_t data, bool length_enable)
{
    pulse->period &= 0x0FF;
    pulse->period |= (data << 8) & 0x700;
    if (length_enable) {
        pulse->length = _APU_LengthLoad(data);
    }
    pulse->envelope.start = true;
    pulse->sequencer = 0;
}

uint8_t _APUPulse_Output(APUPulse *pulse, bool isCh1)
{
    if (
        !((PULSE_DUTY_WAVEFORMS[pulse->duty] >> (7 - pulse->sequencer)) & 0x01) || //Sequencer output is zero
        pulse->length == 0 || //Length counter is zero
        _APUPulse_SweepMute(pulse, isCh1) //Period < 8, or sweep target period > $7FF
    ) {
        return 0; //Silence output if any of the above are true
    }
    return pulse->envelope.constant_volume ? pulse->envelope.period : pulse->envelope.decay;
}

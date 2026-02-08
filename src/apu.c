#include "apu.h"
#include <assert.h>
#include <string.h>

//Write APU length counter load register. The upper 5 bits are an index into the length table.
void _APU_WriteLength(APULength* length, uint8_t reg_data) {
    if (length->enabled)
        length->counter = APU_LENGTH_TABLE[reg_data >> 3];
}


void APU_Init(APU* apu, APUCallbacks callbacks, double cpuClockMHz, double sampleRateHz)
{
    memset(apu, 0, sizeof(APU));
    apu->callbacks = callbacks;
    apu->cpuCyclesPerSample = (cpuClockMHz * 1000000) / sampleRateHz;

    for (int i = 0; i < APU_NUM_VOL_SETTINGS; i++)
        apu->volume[i] = 1.0;
}

void APU_PowerOn(APU *apu)
{
    APUState* state = &apu->state;
    
    for (uint16_t r = 0x4000; r <= 0x4013; r++)
        APU_Write(apu, 0x4000, 0);
    APU_Write(apu, 0x4015, 0);
    APU_Write(apu, 0x4017, 0);
    state->ch_noise.lfsr = 1;
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
        uint8_t ret = (state->ch_pulse1.length.counter > 0) |
        (state->ch_pulse2.length.counter > 0)   << 1 |
        (state->ch_triangle.length.counter > 0) << 2 |
        (state->ch_noise.length.counter > 0)    << 3 |
        (state->ch_dmc.bytes_remaining > 0)     << 4 |
        (state->fc_irq)                         << 6 |
        (state->ch_dmc.irq)                     << 7;

        state->fc_irq = false;
        return ret;
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
        case 0x4003: _APUPulse_Write3(&state->ch_pulse1, data); break;
        case 0x4004: _APUPulse_Write0(&state->ch_pulse2, data); break;
        case 0x4005: _APUPulse_Write1(&state->ch_pulse2, data); break;
        case 0x4006: _APUPulse_Write2(&state->ch_pulse2, data); break;
        case 0x4007: _APUPulse_Write3(&state->ch_pulse2, data); break;
        case 0x4008:
            state->ch_triangle.length.halt = data >> 7;
            state->ch_triangle.linear_reload_value = data & 0x7F;
            break;
        case 0x400A:
            state->ch_triangle.period &= 0xF00;
            state->ch_triangle.period |= data;
            break;
        case 0x400B:
            state->ch_triangle.period &= 0x00FF;
            state->ch_triangle.period |= (data & 0x07) << 8;
            _APU_WriteLength(&state->ch_triangle.length, data);
            //Side effects: Sets linear counter reload flag
            state->ch_triangle.linear_reload = true;
            break;
        case 0x400C:
            state->ch_noise.length.halt = data >> 5 & 1;
            state->ch_noise.envelope.constant_volume = data >> 4 & 1;
            state->ch_noise.envelope.period = data & 0x0F;
            break;
        case 0x400E:
            state->ch_noise.mode = data >> 7;
            state->ch_noise.period = NOISE_PERIOD_TABLE[0][data & 0x0F];
            break;
        case 0x400F:
            _APU_WriteLength(&state->ch_noise.length, data);
            state->ch_noise.envelope.start = true;
            break;
        case 0x4010:
            state->ch_dmc.irq_enable = data >> 7;
            state->ch_dmc.loop = data >> 6 & 1;
            state->ch_dmc.rate = DMC_RATE_TABLE[0][data & 0x0F];
            break;
        case 0x4011: state->ch_dmc.output = data & 0x7F; break;
        case 0x4012: state->ch_dmc.sample_addr = 0xC000 + (data * 64); break;
        case 0x4013: state->ch_dmc.sample_length = (data * 16) + 1; break;
        case 0x4015:
            _APULength_Enable(&state->ch_pulse1.length, data & APU_STATUS_1);
            _APULength_Enable(&state->ch_pulse2.length, data & APU_STATUS_2);
            _APULength_Enable(&state->ch_triangle.length, data & APU_STATUS_T);
            _APULength_Enable(&state->ch_noise.length, data & APU_STATUS_N);
            //DMC Enable
            if (data & APU_STATUS_D) {
                if (state->ch_dmc.bytes_remaining == 0) {
                    _APUDMC_RestartSample(&state->ch_dmc);
                }
            } else
                state->ch_dmc.bytes_remaining = 0;
            //Side effects: Clear DMC interrupt
            state->ch_dmc.irq = false;
            break;
        case 0x4017:
            state->fc_ctrl = data;
            //If the interrupt inhibit flag is set, the frame interrupt flag is cleared
            if (data & FC_IRQ_INHIBIT)
                state->fc_irq = false;
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

bool APU_IRQSignal(APU *apu) { return apu->state.fc_irq || apu->state.ch_dmc.irq; }

void *APU_GetAudioBuffer(APU *apu, size_t *len)
{
    *len = apu->sampleBufferSize * sizeof(short);
    return &apu->sampleBuffer[0];
}

void APU_ClearAudioBuffer(APU *apu)
{
    apu->sampleBufferSize = 0;
}

void APU_DMCLoadSample(APU *apu, uint8_t sampleData)
{
    APU_DMC* dmc = &apu->state.ch_dmc;

    dmc->sample_buffer = sampleData;
    dmc->sample_buffer_full = true;

    if (dmc->cur_addr == 0xFFFF)
        dmc->cur_addr = 0x8000;
    else
        dmc->cur_addr++;

    if (--dmc->bytes_remaining == 0) {
        if (dmc->loop)
            _APUDMC_RestartSample(dmc);
        else if (dmc->irq_enable)
            dmc->irq = true;
    }
}

double APU_GetChannelVolume(APU *apu, APU_Channel channel)
{
    assert(channel < APU_NUM_VOL_SETTINGS);
    return apu->volume[channel];
}

void APU_SetChannelVolume(APU *apu, APU_Channel channel, double volume)
{
    assert(channel < APU_NUM_VOL_SETTINGS);

    if (volume > 1.0)
        volume = 1.0;
    if (volume < 0.0)
        volume = 0.0;
    apu->volume[channel] = volume;
}

bool APU_GetChannelMute(APU *apu, APU_Channel channel)
{
    assert(channel < APU_NUM_VOL_SETTINGS);
    return apu->mute[channel];
}

void APU_SetChannelMute(APU *apu, APU_Channel channel, bool mute)
{
    assert(channel < APU_NUM_VOL_SETTINGS);
    apu->mute[channel] = mute;
}

double _APU_MixAudio(APU *apu)
{
    APUState* state = &apu->state;

    double pulse1 = _APUPulse_Output(&state->ch_pulse1, true)       * apu->volume[APU_CH_PULSE1] * (apu->mute[APU_CH_PULSE1] ? 0.0 : 1.0);
    double pulse2 = _APUPulse_Output(&state->ch_pulse2, false)      * apu->volume[APU_CH_PULSE2] * (apu->mute[APU_CH_PULSE2] ? 0.0 : 1.0);
    double pulse_out = 95.88 / ((8128 / (pulse1 + pulse2)) + 100);

    double triangle = _APUTriangle_Output(&state->ch_triangle)      * apu->volume[APU_CH_TRIANGLE] * (apu->mute[APU_CH_TRIANGLE] ? 0.0 : 1.0);
    double noise = _APUNoise_Output(&state->ch_noise)               * apu->volume[APU_CH_NOISE] * (apu->mute[APU_CH_NOISE] ? 0.0 : 1.0);
    double dmc = _APUDMC_Output(&state->ch_dmc)                     * apu->volume[APU_CH_DMC] * (apu->mute[APU_CH_DMC] ? 0.0 : 1.0);
    double tnd_out = 159.79 / 
        ((1 / ((triangle / 8227) + (noise / 12241) + (dmc / 22638))) + 100);

    return (pulse_out + tnd_out)                                    * apu->volume[APU_CH_MASTER] * (apu->mute[APU_CH_MASTER] ? 0.0 : 1.0);
}

void _APU_FC_Clock(APU *apu)
{
    APUState* state = &apu->state;

    //Clock pulse waves every APU cycle (2 CPU cycles)
    if (state->fc_cycles % 2 == 0) {
        _APUPulse_ClockWave(&state->ch_pulse1);
        _APUPulse_ClockWave(&state->ch_pulse2);
    }

    //Clock triangle, noise, DMC every CPU cycle
    _APUTriangle_ClockWave(&state->ch_triangle);
    _APUNoise_ClockLFSR(&state->ch_noise);
    _APUDMC_Clock(apu);
    
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
                if ((state->fc_ctrl & FC_IRQ_INHIBIT) == 0)
                    state->fc_irq = true;
                break;
            default: break;
        }
        state->fc_cycles = (state->fc_cycles + 1) % 29830;
    }
}

void _APU_FC_ClockQuarterFrame(APU *apu)
{
    APUState* state = &apu->state;
    _APUEnv_Clock(&state->ch_pulse1.envelope, state->ch_pulse1.length.halt);
    _APUEnv_Clock(&state->ch_pulse2.envelope, state->ch_pulse2.length.halt);
    _APUTriangle_ClockLinearCtr(&state->ch_triangle);
    _APUEnv_Clock(&state->ch_noise.envelope, state->ch_noise.length.halt);
}

void _APU_FC_ClockHalfFrame(APU *apu)
{
    APUState* state = &apu->state;
    _APULength_Clock(&state->ch_pulse1.length);
    _APULength_Clock(&state->ch_pulse2.length);
    _APULength_Clock(&state->ch_triangle.length);
    _APULength_Clock(&state->ch_noise.length);
    _APUPulse_ClockSweep(&state->ch_pulse1, true);
    _APUPulse_ClockSweep(&state->ch_pulse2, false);
}

void _APULength_Enable(APULength *length, bool enable)
{
    length->enabled = enable;
    if (!enable)
        length->counter = 0;
}

void _APULength_Clock(APULength *length)
{
    if (length->counter > 0 && !length->halt)
        length->counter--;
}

void _APUEnv_Clock(APUEnvelope *env, bool loop)
{
    if (!env->start) {
        //Clock divider
        if (env->divider == 0) {
            env->divider = env->period;
            //Clock decay
            if (env->decay > 0) {
                env->decay--;
            } else if (loop) {
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
        pulse->duty_pos = (pulse->duty_pos > 0) ? (pulse->duty_pos - 1) : 7;
    } else {
        pulse->timer--;
    }
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

void _APUTriangle_ClockWave(APUTriangle *tri)
{
    if (tri->timer == 0) {
        tri->timer = tri->period;
        if (tri->length.counter > 0 && tri->linear_counter > 0)
            tri->wave_pos = (tri->wave_pos + 1) % 32;
    } else {
        tri->timer--;
    }
}

void _APUTriangle_ClockLinearCtr(APUTriangle *tri)
{
    if (tri->linear_reload) {
        tri->linear_counter = tri->linear_reload_value;
    } else if (tri->linear_counter > 0) {
        tri->linear_counter--;
    }

    if (!tri->length.halt)
        tri->linear_reload = false;
}

void _APUNoise_ClockLFSR(APUNoise *noise)
{
    if (noise->timer == 0) {
        noise->timer = noise->period;

        bool feedback = (noise->lfsr & 1) ^ (noise->lfsr >> (noise->mode ? 6 : 1) & 1);
        noise->lfsr |= feedback << 15;
        noise->lfsr >>= 1;
    } else {
        noise->timer--;
    }
}

void _APUDMC_RestartSample(APU_DMC *dmc)
{
    dmc->cur_addr = dmc->sample_addr;
    dmc->bytes_remaining = dmc->sample_length;
}

void _APUDMC_Clock(APU* apu)
{
    APU_DMC* dmc = &apu->state.ch_dmc;

    //Output unit
    if (--dmc->timer <= 0) {
        dmc->timer = dmc->rate;

        //DPCM output
        if (!dmc->silence && dmc->dpcm_shift & 1) {
            if (dmc->output <= 125) dmc->output += 2;
        } else {
            if (dmc->output >= 2)   dmc->output -= 2;
        }
        dmc->dpcm_shift >>= 1;
        if (--dmc->dpcm_bits_remaining <= 0) {
            //When bits remaining reaches 0, a new output cycle is started
            dmc->dpcm_bits_remaining = 8;
            //If the sample buffer is empty, silence channel, else sample buffer is emptied into DPCM shift register
            if (!dmc->sample_buffer_full)
                dmc->silence = true;
            else {
                dmc->silence = false;
                dmc->dpcm_shift = dmc->sample_buffer;
                dmc->sample_buffer_full = false;
            }
        }
    }
    
    //Memory reader sample buffer load
    if (!dmc->sample_buffer_full && dmc->bytes_remaining > 0) {
        //Schedule a DMC DMA. When the DMC DMA transfers the sample data, the rest of the memory reader load logic is done in APU_DMCLoadSample().
        apu->callbacks.ondma(apu->callbacks.context, dmc->cur_addr);
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
    pulse->length.halt = data >> 5 & 1;
    pulse->envelope.constant_volume = data >> 4 & 1;
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
    pulse->period &= 0xF00;
    pulse->period |= data;
}

void _APUPulse_Write3(APUPulse *pulse, uint8_t data)
{
    pulse->period &= 0x0FF;
    pulse->period |= ((uint16_t)data << 8) & 0x700;
    _APU_WriteLength(&pulse->length, data);
    pulse->envelope.start = true;
    pulse->duty_pos = 0;
}

uint8_t _APUEnv_Output(APUEnvelope *env)
{
    return env->constant_volume ? env->period : env->decay;
}

uint8_t _APUPulse_Output(APUPulse *pulse, bool isCh1)
{
    if (
        !((PULSE_DUTY_WAVEFORMS[pulse->duty] >> (7 - pulse->duty_pos)) & 0x01) || //Sequencer output is zero
        pulse->length.counter == 0 || //Length counter is zero
        _APUPulse_SweepMute(pulse, isCh1) //Period < 8, or sweep target period > $7FF
    ) {
        return 0; //Silence output if any of the above are true
    }
    return _APUEnv_Output(&pulse->envelope);
}

uint8_t _APUTriangle_Output(APUTriangle* tri)
{
    if (tri->linear_counter > 0 && tri->length.counter > 0 &&
        tri->period >= 2)
        return TRIANGLE_WAVE[tri->wave_pos];
    return 0;
}

uint8_t _APUNoise_Output(APUNoise *noise)
{
    if (!(noise->lfsr & 1) && noise->length.counter > 0)
        return _APUEnv_Output(&noise->envelope);
    return 0;
}

uint8_t _APUDMC_Output(APU_DMC *dmc)
{
    return dmc->output * !dmc->silence;
}

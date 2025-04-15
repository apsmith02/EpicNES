#ifndef APU_H
#define APU_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define APU_SAMPLE_CAPACITY 1024 //Max number of samples in buffer

typedef struct {
    bool start; //When set, reloads decay counter with 15 and divider with period.
    bool loop; //When set, decay counter loops back to 15 when it reaches 0. Also used as length counter halt flag in pulse channels.
    bool constant_volume;
    uint8_t period; //Envelope period (low 4 bits of $4000/4004/400C). Used as output if constant volume flag is set.
    uint8_t divider; //Loaded with period. When clocked by frame counter, decrements. When clocked at 0, reloads with period and clocks decay counter.
    uint8_t decay; //Decremented at a rate determined by period. Used as output if constant volume flag is clear.
} APUEnvelope;

//Waveforms of pulse channel's duty cycle sequences.
static const uint8_t PULSE_DUTY_WAVEFORMS[4] = {
    0b00000001, //Duty 0 (12.5%)
    0b00000011, //Duty 1 (25%)
    0b00001111, //Duty 2 (50%)
    0b11111100  //Duty 3 (75%)
};

typedef struct {
    bool enabled;
    uint8_t period;
    bool negate;
    uint8_t shift;

    bool reload;
    uint8_t divider;
} APUSweep;

typedef struct {
    APUEnvelope envelope;
    APUSweep sweep;
    uint8_t duty;
    uint8_t length;
    uint16_t period;
    uint16_t timer; //Reloaded with period, clocks sequencer
    uint8_t sequencer; //Position in output waveform
} APUPulse;

typedef struct {
    uint8_t linear_counter;
    uint8_t linear_reload_value;
    bool control;
    bool linear_reload;
    uint8_t length;
    uint16_t period;
    uint16_t timer;
    uint8_t sequencer;
} APUTriangle;

typedef enum {
    FC_IRQ_INHIBIT  = 1 << 6, //IRQ inhibit
    FC_5STEP         = 1 << 7  //Sequencer mode (0 = 4-step, 1 = 5-step)
} APU_FCCtrl;

typedef enum {
    APU_STATUS_1 = 1,       //Pulse 1 length counter enable
    APU_STATUS_2 = 1 << 1,  //Pulse 2 length counter enable
    APU_STATUS_T = 1 << 2,  //Triangle length counter enable
    APU_STATUS_N = 1 << 3,  //Noise length counter enable
    APU_STATUS_D = 1 << 4,  //DMC enable
    APU_STATUS_F = 1 << 6,  //Frame interrupt
    APU_STATUS_I = 1 << 7   //DMC interrupt
} APU_Status;



typedef struct {
    APUPulse ch_pulse1, ch_pulse2;
    
    uint8_t status; //$4015 status
    uint8_t fc_ctrl; //Frame counter control ($4017)
    unsigned fc_cycles; //Frame counter cycle count (CPU cycles, divide by 2 for APU cycles)
} APUState;



typedef struct {
    APUState state;

    double master_volume; //Master volume of audio output

    double cpuCyclesPerSample;
    double cycleSampleTimer; //Increments every CPU cycle. When cpuCyclesPerSample cycles have run, output a sample.
    short sampleBuffer[APU_SAMPLE_CAPACITY]; //Sample output buffer
    size_t sampleBufferSize;
} APU;



/*
* Initialize APU struct. The CPU clock speed is needed to determine the number of CPU cycles per output sample.
*/
void APU_Init(APU* apu, double cpuClockMHz, double sampleRateHz);
void APU_PowerOn(APU* apu);
void APU_Reset(APU* apu);

uint8_t APU_Read(APU* apu, uint16_t addr);
void APU_Write(APU* apu, uint16_t addr, uint8_t data);
void APU_CPUCycle(APU* apu);


void* APU_GetAudioBuffer(APU* apu, size_t* len);
void APU_ClearAudioBuffer(APU* apu);


//Mix output of all channels and return audio output as a value between 0.0 and 1.0
double _APU_MixAudio(APU* apu);

//Clock frame counter by 1 CPU cycle
void _APU_FC_Clock(APU* apu);
//APU frame counter "quarter frame" clock: Clock envelopes & triangle linear counter
void _APU_FC_ClockQuarterFrame(APU* apu);
//APU frame counter "half frame" clock: Clock length counters & sweep units
void _APU_FC_ClockHalfFrame(APU* apu);

//Clock APU envelope. Clocked by frame counter every quarter frame.
void _APUEnv_Clock(APUEnvelope* env);
//Clock pulse wave timer. Clocked every APU cycle.
void _APUPulse_ClockWave(APUPulse* pulse);
//Clock pulse length counter. Clocked by frame counter every half frame.
void _APUPulse_ClockLength(APUPulse* pulse);
//Clock pulse sweep timer. Clocked by frame counter. Pulse channels 1 and 2 add the sweep period change differently.
void _APUPulse_ClockSweep(APUPulse* pulse, bool isCh1);

/*Returns the pulse channel target period calculated by the sweep unit.
* Channels 1 and 2 negate the period change amount (period >> sweep.shift) differently:
* Channel 1 does one's complement negation (-c - 1), and channel 2 does two's complement (-c).
*/
uint16_t _APUPulse_SweepTargetPeriod(APUPulse* pulse, bool isCh1);
//Returns true if the sweep unit is muting the pulse channel (current pulse period < 8, or target period > $7FF)
bool _APUPulse_SweepMute(APUPulse* pulse, bool isCh1);

//Write $4000/$4004
void _APUPulse_Write0(APUPulse* pulse, uint8_t data);
//Write $4001/$4005
void _APUPulse_Write1(APUPulse* pulse, uint8_t data);
//Write $4002/$4006
void _APUPulse_Write2(APUPulse* pulse, uint8_t data);
//Write $4003/$4007
void _APUPulse_Write3(APUPulse* pulse, uint8_t data, bool length_enable);

//Get output of APU pulse channel. This is a volume level between 0-15.
uint8_t _APUPulse_Output(APUPulse* pulse, bool isCh1);


#endif
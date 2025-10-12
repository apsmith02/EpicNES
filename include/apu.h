#ifndef APU_H
#define APU_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define APU_SAMPLE_CAPACITY 1024 //Max number of samples in buffer


//Callback to schedule a DMC DMA
typedef void(*APUDMAFn)(void *context, uint16_t addr);

typedef struct {
    void *context;
    APUDMAFn ondma;
} APUCallbacks;


static const uint8_t APU_LENGTH_TABLE[32] = {
    10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

//Waveforms of pulse channel's duty cycle sequences.
static const uint8_t PULSE_DUTY_WAVEFORMS[4] = {
    0b00000001, //Duty 0 (12.5%)
    0b00000011, //Duty 1 (25%)
    0b00001111, //Duty 2 (50%)
    0b11111100  //Duty 3 (75%)
};

static const uint8_t TRIANGLE_WAVE[32] = {
    15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

static const uint16_t NOISE_PERIOD_TABLE[2][16] = {
    {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068},//NTSC
    {4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778} //PAL
};

static const int16_t DMC_RATE_TABLE[2][16] = {
    {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54},   //NTSC
    {398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118,  98,  78,  66,  50}    //PAL
};


typedef enum {
    APU_CH_PULSE1 = 0,
    APU_CH_PULSE2,
    APU_CH_TRIANGLE,
    APU_CH_NOISE,
    APU_CH_DMC,
    APU_NUM_CHANNELS,
    APU_CH_MASTER = APU_NUM_CHANNELS,
    APU_NUM_VOL_SETTINGS
} APU_Channel;

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
    bool enabled;
    bool halt; //Halt flag is also the pulse/noise envelope loop flag and triangle linear counter control flag
    uint8_t counter;
} APULength;

typedef struct {
    bool start; //When set, reloads decay counter with 15 and divider with period.
    bool constant_volume;
    uint8_t period; //Envelope period (low 4 bits of $4000/4004/400C). Used as output if constant volume flag is set.
    uint8_t divider; //Loaded with period. When clocked by frame counter, decrements. When clocked at 0, reloads with period and clocks decay counter.
    uint8_t decay; //Decremented at a rate determined by period. Used as output if constant volume flag is clear.
} APUEnvelope;

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
    APULength length;
    uint16_t period;
    uint16_t timer; //Reloaded with period, clocks sequencer
    uint8_t duty_pos; //Position in output waveform
} APUPulse;

typedef struct {
    bool linear_reload;
    uint8_t linear_counter;
    uint8_t linear_reload_value;

    APULength length;
    uint16_t period;
    uint16_t timer;
    uint8_t wave_pos;
} APUTriangle;

typedef struct {
    APUEnvelope envelope;
    APULength length;
    uint16_t period;
    uint16_t timer;
    bool mode;
    uint16_t lfsr; //15-bit linear feedback shift register
} APUNoise;

typedef struct {
    bool irq_enable;
    bool loop;

    int16_t rate;
    int16_t timer;

    uint16_t sample_addr;
    uint16_t cur_addr;
    uint16_t sample_length;
    uint16_t bytes_remaining;
    uint8_t sample_buffer;
    bool sample_buffer_full;

    uint8_t dpcm_shift; //Sample buffer is emptied into DPCM shift register when bits remaining counter reaches 0
    int8_t dpcm_bits_remaining; //DPCM shift register bits remaining
    uint8_t output; //7-bit output level
    bool silence;
    bool irq;
} APU_DMC;


typedef struct {
    APUPulse ch_pulse1, ch_pulse2;
    APUTriangle ch_triangle;
    APUNoise ch_noise;
    APU_DMC ch_dmc;
    
    bool fc_irq;
    uint8_t fc_ctrl; //Frame counter control ($4017)
    unsigned fc_cycles; //Frame counter cycle count (CPU cycles, divide by 2 for APU cycles)
} APUState;



typedef struct {
    APUCallbacks callbacks;

    APUState state;

    double volume[APU_NUM_VOL_SETTINGS]; //Volume levels of each channel between 0.0 and 1.0
    bool mute[APU_NUM_VOL_SETTINGS];

    double cpuCyclesPerSample;
    double cycleSampleTimer; //Increments every CPU cycle. When cpuCyclesPerSample cycles have run, output a sample.
    short sampleBuffer[APU_SAMPLE_CAPACITY]; //Sample output buffer
    size_t sampleBufferSize;
} APU;



/*
* Initialize APU struct. The CPU clock speed is needed to determine the number of CPU cycles per output sample.
*/
void APU_Init(APU* apu, APUCallbacks callbacks, double cpuClockMHz, double sampleRateHz);
void APU_PowerOn(APU* apu);
void APU_Reset(APU* apu);

uint8_t APU_Read(APU* apu, uint16_t addr);
void APU_Write(APU* apu, uint16_t addr, uint8_t data);
void APU_CPUCycle(APU* apu);

bool APU_IRQSignal(APU *apu);

void* APU_GetAudioBuffer(APU* apu, size_t* len);
void APU_ClearAudioBuffer(APU* apu);

//Load the DMC sample buffer with a DPCM sample byte. For use in DMC DMA transfers.
void APU_DMCLoadSample(APU* apu, uint8_t sampleData);

//Get the output volume of an APU channel. Volume is a value between 0.0 and 1.0.
double APU_GetChannelVolume(APU* apu, APU_Channel channel);
//Set the output volume of an APU channel. Volume is a value between 0.0 and 1.0.
void APU_SetChannelVolume(APU* apu, APU_Channel channel, double volume);
//Get the output volume mute status of an APU channel.
bool APU_GetChannelMute(APU* apu, APU_Channel channel);
//Set the output volume mute status of an APU channel.
void APU_SetChannelMute(APU* apu, APU_Channel channel, bool mute);


//Mix output of all channels and return audio output as a value between 0.0 and 1.0
double _APU_MixAudio(APU* apu);

//Clock frame counter by 1 CPU cycle
void _APU_FC_Clock(APU* apu);
//APU frame counter "quarter frame" clock: Clock envelopes & triangle linear counter
void _APU_FC_ClockQuarterFrame(APU* apu);
//APU frame counter "half frame" clock: Clock length counters & sweep units
void _APU_FC_ClockHalfFrame(APU* apu);

//Enable or disable length counter via $4015
void _APULength_Enable(APULength* length, bool enable);
//Clock APU length counter. Clocked by frame counter every half frame.
void _APULength_Clock(APULength* length);

//Clock APU envelope. Clocked by frame counter every quarter frame.
void _APUEnv_Clock(APUEnvelope* env, bool loop);
//Clock pulse wave timer. Clocked every APU cycle.
void _APUPulse_ClockWave(APUPulse* pulse);
//Clock pulse sweep timer. Clocked by frame counter. Pulse channels 1 and 2 add the sweep period change differently.
void _APUPulse_ClockSweep(APUPulse* pulse, bool isCh1);

//Clock triangle wave timer. Clocked every CPU cycle.
void _APUTriangle_ClockWave(APUTriangle* tri);
//Clock triangle linear counter.  Clocked by frame counter every quarter frame.
void _APUTriangle_ClockLinearCtr(APUTriangle* tri);

//Clock noise LFSR timer. Clocked every CPU cycle.
void _APUNoise_ClockLFSR(APUNoise* noise);

void _APUDMC_RestartSample(APU_DMC* dmc);
//Clocked every CPU cycle.
void _APUDMC_Clock(APU* apu);

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
void _APUPulse_Write3(APUPulse* pulse, uint8_t data);

//Get APU envelope volume output.
uint8_t _APUEnv_Output(APUEnvelope* env);
//Get output of APU pulse channel. This is a volume level between 0-15.
uint8_t _APUPulse_Output(APUPulse* pulse, bool isCh1);
//Get output of APU triangle channel. This is a volume level between 0-15.
uint8_t _APUTriangle_Output(APUTriangle* tri);
//Get output of noise channel. This is a volume level between 0-15.
uint8_t _APUNoise_Output(APUNoise* noise);
//Get output of DMC channel. This is a volume level between 0-127.
uint8_t _APUDMC_Output(APU_DMC* dmc);

#endif
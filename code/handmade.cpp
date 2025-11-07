// This code is going to be completely cross-platform

#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
    int ToneHz = 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;
    
    int16 *SampleOut = SoundBuffer->Samples;
    for(DWORD SampleIndex = 0;
	SampleIndex < SoundBuffer->SampleCount;
	SampleIndex++)
    {
	real32 SineValue = sinf(tSine);
	int16 SampleValue = (int16)(SineValue * ToneVolume);
	*SampleOut++ = SampleValue;
	*SampleOut++ = SampleValue;

	tSine += 2.0f*Pi32*1.0f/(real32)WavePeriod;
    }
}

internal void
RenderCoolGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; X++)
        {
            // Pixel in memory: 00 00 00 00
            //                  RR GG BB xx
            // 0x xxBBGGRR
            // Little endian architecture?

            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset,
		    game_sound_output_buffer *SoundBuffer)
{
    // TODO(casey): Allow sample offset here for more robust platform options
    GameOutputSound(SoundBuffer);
    RenderCoolGradient(Buffer, BlueOffset, GreenOffset);
}

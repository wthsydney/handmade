#if !defined(HANDMADE_H)

// TODO(casey): Services that the game provides to the platform layer.
// NOTE(casey): Services that the game provides to the platform layer.
// (this may expand in the future) - sound on seperate thread, etc.

// THREE THINGS - controller/keyboard input, bitmap buffer to use, sound buffer to use

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);

#define HANDMADE_H
#endif

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

// TODO(casey): This is a global for now
global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// NOTE(Patryk): Okay so this code is for making sure that people don't rely
// on incorrect versions of specific libraries. In this case XInput. Normally
// I would'n do it, but right now I'm following the course.

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// NOTE(Patryk): We are doing the same thing for DirectSound as we did for XInput
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    }
    if(!XInputGetState)
    {
        XInputGetState = XInputGetStateStub;
    }

    XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    if(!XInputSetState)
    {
        XInputSetState = XInputSetStateStub;
    }
    else
    {
        // TODO: Diagnostic
    }
}

internal void
Win32InitSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if(DSoundLibrary)
    {
       	// Get a DirectSound object
       	direct_sound_create *DirectSoundCreate = (direct_sound_create *)
   	    GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample)/8;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
              		DSBUFFERDESC BufferDescription = {};
              		BufferDescription.dwSize = sizeof(BufferDescription);
              		BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

              		LPDIRECTSOUNDBUFFER PrimaryBuffer;
              		if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
              		{
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
              		    if(SUCCEEDED(Error))
              		    {
                            // We have finally set the format
                            OutputDebugStringA("Primary buffer format was set.\n");
              		    }
              		    else
              		    {
                            // TODO: Diagnostic
              		    }
                }
            }
            else
            {
                // TODO: Diagnostic
            }

            // Create a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if(SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }

        }
        else
        {
            // TODO: Diagnostic
        }

    }
    else
    {
        // TODO: Diagnostic
    }
}

struct win32_window_dimension
{
    int Width;
    int Height;
};

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void
RenderCoolGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
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

            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

// DIB - Device independent bitmap
internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO(casey): Bulletproof this, huh

    if(Buffer->Memory != NULL)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width*Buffer->BytesPerPixel;

}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
    HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    // TODO(casey): Aspect ratio correction
    StretchDIBits(DeviceContext,
        /*
        X, Y, Width, Height,
        X, Y, Width, Height,
        */
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback (HWND    Window,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LRESULT Result = 0;

    switch(Msg)
    {

        case WM_DESTROY:
    {
        Running = false;
        OutputDebugStringA("WM_DESTROY\n");
    } break;

    case WM_CLOSE:
    {
        // TODO(casey): Handle this with a message to the user?
        Running = false;
        OutputDebugStringA("WM_CLOSE\n");
    } break;

    case WM_ACTIVATEAPP:
    {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:

    case WM_KEYUP:
    {
        uint32 VKCode = wParam;
        bool WasDown = ((lParam & (1 << 30)) != 0);
        bool IsDown = ((lParam & (1 << 31)) == 0);

        if (WasDown != IsDown)
        {
            if (VKCode == VK_ESCAPE)
            {
                if(IsDown)
                {
                    OutputDebugStringA("IsDown");
                }

                if(WasDown)
                {
                    OutputDebugStringA("WasDown");
                }
                OutputDebugStringA("\n");

            }
            else if(VKCode == VK_SPACE)
            {

            }
        }

        // NOTE(patryk): Okay so Casey reccommends that you
        // don't use "!= 0" because of C++ semantics and in
        // result it just wastes time.
        // bool AltKeyWasDown = ((lParam & (1 << 29)) != 0);
        bool AltKeyWasDown = (lParam & (1 << 29));
        if((VKCode == VK_F4) && AltKeyWasDown)
        {
            Running = false;
        }

    } break;


    case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        }

        default:
        {
            //      OutputDebugStringA("default\n");
            Result = DefWindowProc(Window, Msg, wParam, lParam);
        } break;
    }

    return(Result);
}

int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCode)
{
    Win32LoadXInput();
    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowEx
        (0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);

        if(Window)
        {


            // NOTE: Graphics test
            int XOffset = 0;
            int YOffset = 0;

            // NOTE: Sound test
            int SamplesPerSecond = 48000;
            int ToneHz = 256;
            uint32 RunningSampleIndex = 0;
            int SquareWavePeriod = SamplesPerSecond/ToneHz;
            int HalfSquareWavePeriod = SquareWavePeriod / 2;
            int BytesPerSample = sizeof(int16)*2;
            int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;

            // Win32InitSound(Window, SamplesPerSecond, SecondaryBufferSize);
            // if(GlobalSecondaryBuffer)
            // {
            //     GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            // }
            // else
            // {
            //     OutputDebugStringA("Warning: GlobalSecondaryBuffer is NULL — sound disabled\n");
            // }

            Running = true;
            while(Running)
            {

                MSG Message;
                while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        Running = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);

                }

                // TODO: Should we poll this more frequently
                for(DWORD ControllerIndex = 0;
                    ControllerIndex < XUSER_MAX_COUNT;
                    ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // Controller is plugged in

                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;

                        if(AButton)
                        {
                            YOffset++;
                        }

                    }
                    else
                    {
                        // Controller is not available
                    }
                }

                // If I want any vibration
                // XINPUT_VIBRATION Vibration;
                // XInputSetState(0, &Vibration);

                RenderCoolGradient(&GlobalBackBuffer, XOffset, YOffset);

                // DWORD PlayCursor;
                // DWORD WriteCursor;
                // // NOTE: I think Casey is doing lots of placeholder code with variables
                // // that don't exist, however they're named after things that should be there.
                // //
                // // *later* Okay he calls it "compression oriented programming"
                // if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                // {
                //     DWORD ByteToLock = RunningSampleIndex*BytesPerSample % SecondaryBufferSize;
                //     DWORD BytesToWrite;
                //     if(ByteToLock > PlayCursor)
                //     {
                //         BytesToWrite = SecondaryBufferSize - ByteToLock;
                //         BytesToWrite += PlayCursor;
                //     }
                //     else
                //     {
                //         BytesToWrite = PlayCursor - ByteToLock;
                //     }

                //     VOID *Region1;
                //     DWORD Region1Size;
                //     VOID *Region2;
                //     DWORD Region2Size;

                //     if(SUCCEEDED(GlobalSecondaryBuffer->Lock(
                //         ByteToLock, BytesToWrite,
                //         &Region1, &Region1Size,
                //         &Region2, &Region2Size,
                //         0)))
                //     {

                //         DWORD Region1SampleCount = Region1Size/BytesPerSample;
                //         int16 *SampleOut = (int16 *)Region1;
                //         for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
                //             SampleIndex++)
                //         {
                //             int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ?
                //             16000 : -16000;
                //             *SampleOut++ = SampleValue;
                //             *SampleOut++ = SampleValue;
                //         }

                //         DWORD Region2SampleCount = Region2Size/BytesPerSample;
                //         SampleOut = (int16 *)Region2;
                //         for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
                //             SampleIndex++)
                //         {
                //             int16 SampleValue = ((RunningSampleIndex++ > HalfSquareWavePeriod) % 2)
                //             ? 16000 : -16000;
                //             *SampleOut++ = SampleValue;
                //             *SampleOut++ = SampleValue;
                //         }
                //     }

                // }

                HDC DeviceContext = GetDC(Window);
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                    Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);

                XOffset++;
            }

        }
        else
        {
            // TODO(casey): Logging
        }

        return 0;

    }
}

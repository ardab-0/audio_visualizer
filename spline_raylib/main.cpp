/*******************************************************************************************
*
*   raylib example - procedural mesh generation
*
*   Example originally created with raylib 1.8, last time updated with raylib 4.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2023 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
#include "PerlinNoise.h"
#include <math.h>
#include "dj_fft.h"

int N = 512; // fft input size
std::vector<float> fftNrm(N);
std::vector<float> fftNrmPrev(N);

const int BUFFER_LENGTH = 200;
PerlinNoise pn(100);
static float exponent = 1.0f;                 // Audio exponentiation value
static float averageVolume[BUFFER_LENGTH] = { 0.0f };   // Average volume history
float low = 0;
static const float cutoff = 5.0f / (44100.0f / 220); 
const float k = cutoff / (cutoff + 0.1591549431f); // RC filter formula
const int screenWidth = 1280;
const int screenHeight = 720;
float lambda = 0.1;


float scale(float x) {
    return std::log(x + 1);
    return std::log(std::log(std::log(x + 1) + 1) + 1) * 4;
}

void drawAmplitudeGraph() {
    //Draw amplitude visualization
    int rectWidth = BUFFER_LENGTH;
    int rectHeight = 40;
    int startX = screenWidth / 2 - rectWidth / 2;
    int startY = screenHeight * 0.8;
    DrawRectangle(startX, startY, rectWidth, rectHeight, LIGHTGRAY);
    for (int i = 0; i < rectWidth; i++)
    {
        DrawLine(startX + i, startY + rectHeight - averageVolume[i] * rectHeight, startX + i, startY + rectHeight, MAROON);
    }
    DrawRectangleLines(startX, startY, rectWidth, rectHeight, GRAY);
}

void drawFFTGraph() {
    //Draw fft visualization
    int rectWidth = N/2;
    int rectHeight = 40;
    int startX = screenWidth / 2 - rectWidth / 2;
    int startY = screenHeight * 0.9;
    DrawRectangle(startX, startY, rectWidth, rectHeight, LIGHTGRAY);
    for (int i = 0; i < rectWidth; i++)
    {
        DrawLine(startX + i, startY + rectHeight - scale( fftNrm[i] ) * rectHeight, startX + i, startY + rectHeight, MAROON);
    }
    DrawRectangleLines(startX, startY, rectWidth, rectHeight, GRAY);
}

void fft(std::vector<float>& mono_audio) {
   
    auto myData = std::vector<std::complex<float>>(N, 0); // input data
    
    // prepare data
    for (int i = 0; i < mono_audio.size(); ++i) {
        myData[i] = mono_audio[i]; // set element (i)
    }

    auto fftData = dj::fft1d(myData, dj::fft_dir::DIR_FWD);

    for (int i = 0; i < fftData.size(); ++i) {
        fftNrmPrev = fftNrm;
        fftNrm[i] = lambda * std::abs(fftData[i]) + (1-lambda) * fftNrmPrev[i]; // set element (i)
    }
    
}


void ProcessAudio(void* buffer, unsigned int frames)
{
    float* samples = (float*)buffer;   // Samples internally stored as <float>s
    float average = 0.0f;               // Temporary average volume
    std::vector<float> mono_audio(frames);

    for (unsigned int frame = 0; frame < frames; frame++)
    {
        float* left = &samples[frame * 2 + 0], * right = &samples[frame * 2 + 1];

        float a = (*left + *right) / 2;
        mono_audio[frame] = a;

        *left = powf(fabsf(*left), exponent) * ((*left < 0.0f) ? -1.0f : 1.0f);
        *right = powf(fabsf(*right), exponent) * ((*right < 0.0f) ? -1.0f : 1.0f);

        average += fabsf(*left) / frames;   // accumulating average volume
        average += fabsf(*right) / frames;
    }
    // compute fft of block
    fft(mono_audio);


    // Moving history to the left
    for (int i = 0; i < BUFFER_LENGTH-1; i++) averageVolume[i] = averageVolume[i + 1];

    low += k * (average - low);

    averageVolume[BUFFER_LENGTH-1] = low;         

}

std::vector<float> generate_data(size_t size, int row)
{
    // We use static in order to instantiate the random engine
    // and the distribution once only.
    // It may provoke some thread-safety issues.

    std::vector<float> data(size);
    for (int i = 0; i < size; i++) {
        float flatness_amplitude = pow(1 - abs(i / float(size) - 0.5) * 2, 3.5);

        data[i] = pn.noise(i*0.2, GetTime(), row) * flatness_amplitude*4;
        data[i] += pn.noise(i , GetTime(), row) *0.1;
    }
    return data;
}

void move_camera(Camera3D& camera) {
    float dt = GetFrameTime();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        UpdateCamera(&camera, CAMERA_FREE);
    }
    if (abs(GetMouseWheelMove()) > 0) {
        UpdateCamera(&camera, CAMERA_FREE);
    }
    if (IsKeyDown('W')) {
        
        UpdateCameraPro(&camera, {  10* dt, 0, 0 }, {0, 0, 0}, {0});
    }
    if (IsKeyDown('S')) {

        UpdateCameraPro(&camera, { -10 * dt, 0, 0 }, { 0, 0, 0 }, { 0 });
    }
    if (IsKeyDown('A')) {

        UpdateCameraPro(&camera, { 0,-10*dt, 0 }, { 0, 0, 0 }, { 0 });
    }
    if (IsKeyDown('D')) {

        UpdateCameraPro(&camera, { 0, 10*dt, 0 }, { 0, 0, 0 }, { 0 });
    }
}
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Audio Visualizer");
    InitAudioDevice();              // Initialize audio device
    bool isMusicDropped = false;
    Music music;
    
    // Define the camera to look into our 3d world
    Camera camera = { { 0.0f, 5.0f, -5.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, 0 };

    // Model drawing position
    Vector3 position = { 0.0f, 0.0f, 0.0f };

    int currentModel = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        move_camera(camera); 
        if(isMusicDropped)
            UpdateMusicStream(music);   // Update music buffer with new stream data
        
        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();

            if (droppedFiles.count == 1)
            {
                if (isMusicDropped) {
                    UnloadMusicStream(music);
                    
                }
                isMusicDropped = true;
                music = LoadMusicStream(droppedFiles.paths[0]);
                AttachAudioStreamProcessor(music.stream, ProcessAudio);
                PlayMusicStream(music);
                
            }

            UnloadDroppedFiles(droppedFiles);
        }


        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(BLACK);

        
        drawAmplitudeGraph();
        drawFFTGraph();


        


        BeginMode3D(camera);

       
        //draw amplitude visualization
        for (int i = 0; i < BUFFER_LENGTH; i++) {
            auto points = generate_data(50, i);
            float step_size = 10.0 / (points.size() - 1);
            float amp = averageVolume[BUFFER_LENGTH - i -1];
            for (int j = 0; j < points.size()-1; j++) {
                

                DrawLine3D({ 15 + j*step_size, points[j]*amp, float(i)/4}, {15 + (j + 1) * step_size,  points[j+1]*amp, float(i)/4}, WHITE);
            }
            
        }

        //draw frequency visualiation
        for (int i = 0; i < int(float(N)/2); i++) {
            auto points = generate_data(50, i);
            float step_size = 10.0 / (points.size() - 1);
            float amp = scale(fftNrm[i]);
            for (int j = 0; j < points.size() - 1; j++) {


                DrawLine3D({ -25 + j * step_size, points[j] * amp, float(i)/4 }, { -25 + (j + 1) * step_size,  points[j + 1] * amp, float(i)/4 }, WHITE);
            }

        }


        

        EndMode3D();        

        DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    if (isMusicDropped) {
        UnloadMusicStream(music);
        DetachAudioStreamProcessor(music.stream, ProcessAudio);
    }    
    CloseAudioDevice();

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}


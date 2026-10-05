// This test-only translation unit observes the normal backbuffer before Present.
// Production MapDebugView and its event/render paths remain unchanged.
#include <SDL3/SDL.h>
bool openemperor_test_present(SDL_Renderer* renderer);
#define SDL_RenderPresent openemperor_test_present
#include "app/MapDebugView.cpp"
#undef SDL_RenderPresent

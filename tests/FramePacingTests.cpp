#include "app/FramePacing.h"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        using openemperor::frame_wait_nanoseconds;
        using openemperor::target_frame_nanoseconds;
        if (frame_wait_nanoseconds(0)!=target_frame_nanoseconds ||
            frame_wait_nanoseconds(target_frame_nanoseconds/2)!=
                target_frame_nanoseconds-target_frame_nanoseconds/2 ||
            frame_wait_nanoseconds(target_frame_nanoseconds)!=0 ||
            frame_wait_nanoseconds(target_frame_nanoseconds+10'000'000)!=0)
            throw std::runtime_error("frame pacing did not use only the target-frame remainder");
        std::cout<<"frame pacing remainder passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}

//
//  ParameterIds.h
//  Rackbox
//
//  Every parameter the host can see, named in one place.
//
//  These ids are the identity a saved session is restored by; once a session
//  exists none of them may change. Rename a display name instead.
//
//  A block's parameters are the same eight suffixes four times over, with the
//  block number in front: "b2_type". Two of them look the same to a host but are
//  not: `type` and `sub` are stepped choices, and `a` / `b` are the two knobs of
//  whatever effect is loaded - they are named after position because their
//  meaning changes with the effect, and an automation lane should keep pointing
//  at "the first knob" when the effect behind it is swapped.
//

#pragma once

#include <juce_core/juce_core.h>

namespace RackParams
{
    constexpr int stateVersion = 1;
    constexpr int numBlocks = 4;

    inline const juce::Identifier stateTreeType { "RACKBOX" };

    inline constexpr const char* lowpass  = "lpf";
    inline constexpr const char* highpass = "hpf";
    inline constexpr const char* dryWet   = "drywet";

    namespace Block
    {
        inline constexpr const char* type   = "type";
        inline constexpr const char* sub    = "sub";
        inline constexpr const char* bypass = "bypass";
        inline constexpr const char* sync   = "sync";
        inline constexpr const char* pre    = "pre";
        inline constexpr const char* a      = "a";
        inline constexpr const char* b      = "b";
        inline constexpr const char* mix    = "mix";
    }

    /** "b1_type" for block 0. */
    inline juce::String blockId (int block, const char* suffix)
    {
        return "b" + juce::String (block + 1) + "_" + suffix;
    }
}

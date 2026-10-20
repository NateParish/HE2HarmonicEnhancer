#pragma once

#include <JuceHeader.h>
#include "../../AppLibrary/Source/BuildTypes.h"

struct AppInfo
{
    juce::String productName = "HE2 Harmonic Enhancer";
    juce::String version = "1.0.0";
    juce::String sku = "10028";
    juce::String companyName = "Parish Audio";
    juce::String companyNameShort = "ParishAudio";
    BuildType buildType = BuildType::Authenticate; // Authenticate, Unlocked, MuseHub

    float loginWindowWidth = 400.0f;
    float loginWindowHeight = 300.0f;
    float appHeightPctOfWindow = 0.70f;
    float ratio = 900.0f / 700.0f;
    float menuBarHeightPct = 0.09f;

    float minWindowWidth = 400.0f;
    float maxWindowWidth = 1200.0f;
};

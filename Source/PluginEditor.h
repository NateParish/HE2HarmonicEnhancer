#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AppInfo.h"
#include "../../AppLibrary/Source/Library/LoginScreen/LoginScreen.h"
#include "../../AppLibrary/Source/MenuBar.h"
#include "../../AppLibrary/Source/AppData.h"
#include "../../AppLibrary/Source/Library/UserPreferences.h"
#include "GUI.h"

class HE2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit HE2AudioProcessorEditor(HE2AudioProcessor&);
    ~HE2AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void setAuthLock(AuthenticationLock* newAuthLock) { authLock = newAuthLock; }
    AuthenticationLock* getAuthLock() { return authLock; }

    void loadLoginScreen();
    void loginCompleteCallback();
    void loadAppGui();
    void logout();

private:
    void applySavedGraphicsMode();
    void applyResolutionMode(MenuBar::ResolutionMode mode);
    void applyGuiScalePreset(MenuBar::GuiScalePreset preset);
    void applyWindowSizing(bool showingLogin);
    juce::Rectangle<int> getLoginWindowBounds() const;
    juce::Rectangle<int> getAppWindowBounds() const;
    juce::Rectangle<int> getBaseAppWindowBounds() const;
    juce::Rectangle<int> getDisplayUserArea() const;

    HE2AudioProcessor& audioProcessor;
    AuthenticationLock* authLock = nullptr;

    AppInfo appInfo;
    AppData appData;

    LoginScreen loginScreen;
    HE2GUI appGui;
    MenuBar menuBar;
    UserPreferences prefs;

    int framerate = 60;
    bool applyingWindowSizing = false;
    MenuBar::GuiScalePreset currentGuiScalePreset = MenuBar::GuiScalePreset::pct100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HE2AudioProcessorEditor)
};

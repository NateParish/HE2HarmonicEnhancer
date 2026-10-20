#include "PluginProcessor.h"
#include "PluginEditor.h"

HE2AudioProcessorEditor::HE2AudioProcessorEditor(HE2AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), authLock(p.getAuthLock()), appGui(p)
{
    appData.productName = appInfo.productName;
    appData.version = appInfo.version;
    appData.sku = appInfo.sku;
    appData.companyName = appInfo.companyName;
    appData.companyNameShort = appInfo.companyNameShort;
    appData.buildType = appInfo.buildType;
    appData.loginWindowWidth = appInfo.loginWindowWidth;
    appData.loginWindowHeight = appInfo.loginWindowHeight;
    appData.appHeightPctOfWindow = appInfo.appHeightPctOfWindow;
    appData.ratio = appInfo.ratio;
    appData.menuBarHeightPct = appInfo.menuBarHeightPct;
    appData.minWindowWidth = appInfo.minWindowWidth;
    appData.maxWindowWidth = appInfo.maxWindowWidth;

    setResizable(true, true);
    menuBar.setProcessor(&p);
    menuBar.setAppData(appData);
    menuBar.setSettingsButtonVisible(true);
    currentGuiScalePreset = menuBar.getSavedDefaultGuiScalePreset();
    menuBar.setCurrentGuiScalePreset(currentGuiScalePreset);
    prefs.loadAppInfo(appData);

    loginScreen.setAuthLock(authLock);
    loginScreen.setImages({}, {}, {});
    loginScreen.setLoginProcessCompleteCallback([this] { loginCompleteCallback(); });

    menuBar.setLogoutButtonCallback([this] { logout(); });
    menuBar.setResolutionSettingCallback([this](MenuBar::ResolutionMode mode) { applyResolutionMode(mode); });
    menuBar.setGuiScaleSelectionCallback([this](MenuBar::GuiScalePreset preset) { applyGuiScalePreset(preset); });

    addChildComponent(appGui);
    addChildComponent(loginScreen);
    addChildComponent(menuBar);

    const bool locked = audioProcessor.isLocked();
    loginScreen.setVisible(locked);
    appGui.setVisible(! locked);
    menuBar.setVisible(! locked);

    if (locked)
        loadLoginScreen();

    applySavedGraphicsMode();
    startTimer(1000 / framerate);
    applyWindowSizing(locked);
}

HE2AudioProcessorEditor::~HE2AudioProcessorEditor() = default;

juce::Rectangle<int> HE2AudioProcessorEditor::getDisplayUserArea() const
{
    auto& displays = juce::Desktop::getInstance().getDisplays();

    if (auto* display = displays.getDisplayForRect(getScreenBounds()))
        return display->userArea;

    if (auto* display = displays.getPrimaryDisplay())
        return display->userArea;

    return displays.getTotalBounds(true);
}

juce::Rectangle<int> HE2AudioProcessorEditor::getLoginWindowBounds() const
{
    const auto userArea = getDisplayUserArea();
    const int width = juce::jmax(1, juce::jmin(juce::roundToInt(appData.loginWindowWidth), userArea.getWidth()));
    const int height = juce::jmax(1, juce::jmin(juce::roundToInt(appData.loginWindowHeight), userArea.getHeight()));
    return { 0, 0, width, height };
}

juce::Rectangle<int> HE2AudioProcessorEditor::getBaseAppWindowBounds() const
{
    const auto userArea = getDisplayUserArea();
    const float targetHeightPct = juce::jlimit(0.1f, 1.0f, appData.appHeightPctOfWindow);
    const double aspectRatio = juce::jmax(0.1, (double) appData.ratio);

    const int desiredHeight = juce::jmax(1, juce::roundToInt((float) userArea.getHeight() * targetHeightPct));
    const int desiredWidth = juce::jmax(1, (int) std::round((double) desiredHeight * aspectRatio));
    const int maxWidth = juce::jmax(1, juce::jmin(juce::roundToInt(appData.maxWindowWidth), userArea.getWidth()));
    const int minWidth = juce::jmax(1, juce::jmin(juce::roundToInt(appData.minWindowWidth), maxWidth));

    int width = juce::jlimit(minWidth, maxWidth, desiredWidth);
    int height = juce::jmax(1, (int) std::round((double) width / aspectRatio));

    if (height > userArea.getHeight())
    {
        height = userArea.getHeight();
        width = juce::jmax(1, juce::jmin(userArea.getWidth(), (int) std::round((double) height * aspectRatio)));
    }

    return { 0, 0, width, height };
}

juce::Rectangle<int> HE2AudioProcessorEditor::getAppWindowBounds() const
{
    const auto userArea = getDisplayUserArea();
    const auto baseBounds = getBaseAppWindowBounds();
    const double aspectRatio = juce::jmax(0.1, (double) appData.ratio);
    const int maxWidth = juce::jmax(1, juce::jmin(juce::roundToInt(appData.maxWindowWidth), userArea.getWidth()));
    const int minWidth = juce::jmax(1, juce::jmin(juce::roundToInt(appData.minWindowWidth), maxWidth));

    auto constrainToAvailableArea = [userArea, aspectRatio, minWidth, maxWidth] (int targetWidth, int targetHeight)
    {
        int width = juce::jlimit(minWidth, maxWidth, targetWidth);
        int height = juce::jmax(1, targetHeight);

        const double widthScale = width > 0 ? (double) maxWidth / (double) width : 1.0;
        const double heightScale = height > 0 ? (double) userArea.getHeight() / (double) height : 1.0;
        const double shrinkScale = juce::jmin(1.0, juce::jmin(widthScale, heightScale));

        width = juce::jmax(1, (int) std::round((double) width * shrinkScale));
        width = juce::jlimit(minWidth, maxWidth, width);
        height = juce::jmax(1, (int) std::round((double) width / aspectRatio));

        if (height > userArea.getHeight())
        {
            height = userArea.getHeight();
            width = juce::jmax(1, juce::jmin(maxWidth, (int) std::round((double) height * aspectRatio)));
        }

        return juce::Rectangle<int>(0, 0, width, height);
    };

    if (currentGuiScalePreset == MenuBar::GuiScalePreset::fullScreen)
    {
        int width = maxWidth;
        int height = juce::jmax(1, (int) std::round((double) width / aspectRatio));

        if (height > userArea.getHeight())
        {
            height = userArea.getHeight();
            width = juce::jmax(1, juce::jmin(maxWidth, (int) std::round((double) height * aspectRatio)));
        }

        return { 0, 0, width, height };
    }

    double scale = 1.0;
    switch (currentGuiScalePreset)
    {
        case MenuBar::GuiScalePreset::pct50:      scale = 0.50; break;
        case MenuBar::GuiScalePreset::pct75:      scale = 0.75; break;
        case MenuBar::GuiScalePreset::pct100:     scale = 1.00; break;
        case MenuBar::GuiScalePreset::pct125:     scale = 1.25; break;
        case MenuBar::GuiScalePreset::pct150:     scale = 1.50; break;
        case MenuBar::GuiScalePreset::fullScreen: break;
    }

    return constrainToAvailableArea(juce::jmax(1, (int) std::round((double) baseBounds.getWidth() * scale)),
                                     juce::jmax(1, (int) std::round((double) baseBounds.getHeight() * scale)));
}

void HE2AudioProcessorEditor::applyWindowSizing(bool showingLogin)
{
    struct ScopedSizingFlag
    {
        explicit ScopedSizingFlag(bool& f) : flag(f) { flag = true; }
        ~ScopedSizingFlag() { flag = false; }
        bool& flag;
    } scopedSizing(applyingWindowSizing);

    const auto targetBounds = showingLogin ? getLoginWindowBounds() : getAppWindowBounds();
    const double aspectRatio = showingLogin
        ? (double) targetBounds.getWidth() / (double) juce::jmax(1, targetBounds.getHeight())
        : juce::jmax(0.1, (double) appData.ratio);
    const auto userArea = getDisplayUserArea();
    const int maxWidth = juce::jmax(1, juce::jmin(juce::roundToInt(appData.maxWindowWidth), userArea.getWidth()));
    const int minWidth = juce::jmax(1, juce::jmin(juce::roundToInt(appData.minWindowWidth), maxWidth));
    const int minHeight = juce::jmax(1, (int) std::round((double) minWidth / aspectRatio));
    const int maxHeight = juce::jmax(minHeight, (int) std::round((double) maxWidth / aspectRatio));
    const int restoredWidth = juce::jlimit(minWidth, maxWidth, juce::roundToInt(audioProcessor.getCurWindowWidth()));
    const int restoredHeight = juce::jlimit(minHeight, maxHeight, juce::roundToInt(audioProcessor.getCurWindowHeight()));

    if (showingLogin)
    {
        setResizable(false, false);
        setResizeLimits(targetBounds.getWidth(), targetBounds.getHeight(),
                        targetBounds.getWidth(), targetBounds.getHeight());
        getConstrainer()->setFixedAspectRatio(aspectRatio);
        setSize(targetBounds.getWidth(), targetBounds.getHeight());
    }
    else if (! audioProcessor.hasCurWindowSizeBeenSet())
    {
        setResizable(true, true);
        setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
        getConstrainer()->setFixedAspectRatio(aspectRatio);
        audioProcessor.setCurWindowSize((float) targetBounds.getWidth(), (float) targetBounds.getHeight());
        setSize(targetBounds.getWidth(), targetBounds.getHeight());
    }
    else
    {
        setResizable(true, true);
        setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
        getConstrainer()->setFixedAspectRatio(aspectRatio);
        setSize(restoredWidth, restoredHeight);
    }
}

void HE2AudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void HE2AudioProcessorEditor::resized()
{
    const int menuHeight = juce::roundToInt(appData.menuBarHeightPct * (float) getHeight());
    menuBar.setBounds(0, 0, getWidth(), menuHeight);

    const double appAspectRatio = juce::jmax(0.1, (double) appData.ratio);
    const int minAppWidth = juce::roundToInt(appData.minWindowWidth);
    const int minAppHeight = juce::jmax(1, (int) std::round((double) minAppWidth / appAspectRatio));
    const bool looksLikeValidAppSize = getWidth() >= minAppWidth && getHeight() >= minAppHeight;

    if (authLock != nullptr && authLock->isLocked())
    {
        menuBar.setVisible(false);
        loginScreen.setBounds(0, 0, getWidth(), getHeight());
        appGui.setBounds(0, menuHeight, getWidth(), getHeight() - menuHeight);
    }
    else
    {
        menuBar.setVisible(true);
        appGui.setBounds(0, menuHeight, getWidth(), getHeight() - menuHeight);

        if (applyingWindowSizing)
            return;

        if (looksLikeValidAppSize)
            audioProcessor.setCurWindowSize((float) getWidth(), (float) getHeight());
    }
}

void HE2AudioProcessorEditor::timerCallback()
{
    if (! loginScreen.loginComplete())
    {
        loginScreen.frameUpdate();
        repaint();
    }
}

void HE2AudioProcessorEditor::applySavedGraphicsMode()
{
    const juce::String mode = prefs.loadGraphicsMode("auto");

    if (mode == "low")
    {
        appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::low);
        menuBar.setResolutionMode(MenuBar::ResolutionMode::low);
    }
    else if (mode == "medium")
    {
        appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::medium);
        menuBar.setResolutionMode(MenuBar::ResolutionMode::medium);
    }
    else if (mode == "high")
    {
        appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::high);
        menuBar.setResolutionMode(MenuBar::ResolutionMode::high);
    }
    else
    {
        appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::autoMode);
        menuBar.setResolutionMode(MenuBar::ResolutionMode::autoMode);
    }
}

void HE2AudioProcessorEditor::applyResolutionMode(MenuBar::ResolutionMode mode)
{
    switch (mode)
    {
        case MenuBar::ResolutionMode::autoMode: appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::autoMode); break;
        case MenuBar::ResolutionMode::low: appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::low); break;
        case MenuBar::ResolutionMode::medium: appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::medium); break;
        case MenuBar::ResolutionMode::high: appGui.setDisplayQualityMode(HE2GUI::DisplayQualityMode::high); break;
    }
}

void HE2AudioProcessorEditor::applyGuiScalePreset(MenuBar::GuiScalePreset preset)
{
    currentGuiScalePreset = preset;
    menuBar.setCurrentGuiScalePreset(preset);

    if (authLock != nullptr && authLock->isLocked())
        return;

    audioProcessor.clearCurWindowSize();
    applyWindowSizing(false);
    resized();
    repaint();
}

void HE2AudioProcessorEditor::loadLoginScreen()
{
    menuBar.setVisible(false);
    appGui.setVisible(false);
    loginScreen.setVisible(true);
    loginScreen.launchLoginScreen();
    loginScreen.logout();
    applyWindowSizing(true);
    resized();
    repaint();
}

void HE2AudioProcessorEditor::loadAppGui()
{
    loginScreen.setVisible(false);
    menuBar.setVisible(true);
    appGui.setVisible(true);
    applyWindowSizing(false);
    resized();
    repaint();
}

void HE2AudioProcessorEditor::logout()
{
    if (authLock != nullptr)
        authLock->logout();

    loadLoginScreen();
}

void HE2AudioProcessorEditor::loginCompleteCallback()
{
    loadAppGui();
}

#include "../../AppLibrary/Source/Library/Buttons/IconButton.cpp"
#include "../../AppLibrary/Source/MenuBar.cpp"
#include "../../AppLibrary/Source/Library/UserPreferences.cpp"
#include "../../AppLibrary/Source/Library/LoginScreen/Crypto.cpp"
#include "../../AppLibrary/Source/Library/LoginScreen/Authentication.cpp"

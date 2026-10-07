// Renders the plug-in editor to PNG files (used for README screenshots and layout checks).
// Usage: RecklessMuseSnapshot <output-dir>

#include "PluginEditor.h"
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    RecklessMuseProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // --process <preset name> [seq]: run the real processBlock for 40 s and print L/R RMS per 4 s
    if (argc > 2 && juce::String (argv[1]) == "--process")
    {
        for (int i = 0; i < processor.presets.getNumPresets(); ++i)
            if (processor.presets.getPreset (i).name == juce::String (argv[2]))
                processor.presets.loadPreset (i);
        if (argc > 3)
            processor.apvts.getParameter ("seqPlay")->setValueNotifyingHost (1.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        for (int w = 0; w < 10; ++w)
        {
            double l = 0, r = 0;
            for (int b = 0; b < 375; ++b)
            {
                juce::MidiBuffer midi;
                if (w == 0 && b == 0) for (int n : { 48, 55, 60 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
                if (w == 1 && b == 0) for (int n : { 48, 55, 60 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                processor.processBlock (buffer, midi);
                for (int k = 0; k < 512; ++k) { l += buffer.getSample (0, k) * buffer.getSample (0, k); r += buffer.getSample (1, k) * buffer.getSample (1, k); }
            }
            std::cout << std::sqrt (l / 192000.0) << "/" << std::sqrt (r / 192000.0) << "  ";
        }
        std::cout << std::endl;
        return 0;
    }

    // --liked: show the Favorites bank with the current preset liked (uses a temporary favourites file)
    juce::TemporaryFile tempFavorites (".xml");
    if (argc > 2 && juce::String (argv[2]) == "--liked")
    {
        processor.presets.setFavoritesFile (tempFavorites.getFile());
        processor.presets.toggleCurrentFavorite();
        processor.presets.setFavoritesMode (true);
    }

    for (float scale : { 0.5f, 1.0f, 1.5f })
    {
        processor.editorScale = scale;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = outDir.getChildFile ("ui_" + juce::String (juce::roundToInt (scale * 100)) + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << file.getFullPathName() << " " << image.getWidth() << "x" << image.getHeight() << std::endl;
    }
    return 0;
}

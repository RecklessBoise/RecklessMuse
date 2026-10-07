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

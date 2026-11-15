// Offline smoke test: runs the processor over a test signal with every stage
// enabled, checks the output is sane, and writes before/after WAV files.
//   PatinaSmokeTest [outputDir]

#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

static bool run (const char* label, PatinaProcessor& p, juce::AudioBuffer<float>& io, int blockSize)
{
    juce::MidiBuffer midi;
    for (int start = 0; start < io.getNumSamples(); start += blockSize)
    {
        const int n = juce::jmin (blockSize, io.getNumSamples() - start);
        juce::AudioBuffer<float> block (io.getArrayOfWritePointers(), io.getNumChannels(), start, n);
        p.processBlock (block, midi);
    }

    float peak = 0.0f;
    double sumSq = 0.0;
    for (int ch = 0; ch < io.getNumChannels(); ++ch)
        for (int i = 0; i < io.getNumSamples(); ++i)
        {
            const float v = io.getSample (ch, i);
            if (! std::isfinite (v))
            {
                std::cout << "FAIL " << label << ": non-finite sample\n";
                return false;
            }
            peak = juce::jmax (peak, std::abs (v));
            sumSq += (double) v * v;
        }
    const double rms = std::sqrt (sumSq / (io.getNumSamples() * io.getNumChannels()));
    std::cout << label << ": peak " << juce::Decibels::gainToDecibels (peak) << " dBFS, rms "
              << juce::Decibels::gainToDecibels ((float) rms) << " dBFS\n";
    if (peak > 4.0f) { std::cout << "FAIL " << label << ": runaway level\n"; return false; }
    return true;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int len = (int) sr * 6;
    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();

    // test signal: a little arpeggio of decaying tones
    juce::AudioBuffer<float> input (2, len);
    input.clear();
    const float notes[] = { 220.0f, 277.18f, 329.63f, 440.0f, 329.63f, 277.18f };
    for (int i = 0; i < len; ++i)
    {
        const int idx = (int) (i / (sr * 0.5)) % 6;
        const float t = (float) std::fmod (i / sr, 0.5);
        const float env = std::exp (-t * 5.0f);
        const float ph = juce::MathConstants<float>::twoPi * notes[idx] * (float) (i / sr);
        const float s = 0.4f * env * (std::sin (ph) + 0.3f * std::sin (2.0f * ph) + 0.15f * std::sin (3.0f * ph));
        input.setSample (0, i, s);
        input.setSample (1, i, s);
    }
    writeWav (outDir.getChildFile ("patina_dry.wav"), input, sr);

    bool ok = true;

    {   // defaults
        PatinaProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        auto io = input;
        ok &= run ("defaults", p, io, 512);
        writeWav (outDir.getChildFile ("patina_defaults.wav"), io, sr);
    }

    {   // everything on, every noise type, odd block sizes
        for (int type = 0; type < 3; ++type)
        {
            PatinaProcessor p;
            p.setPlayConfigDetails (2, 2, sr, 333);
            for (auto* id : { ParamID::noiseOn, ParamID::wobbleOn, ParamID::distortOn, ParamID::digitalOn,
                              ParamID::spaceOn, ParamID::magneticOn })
                p.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
            for (auto* id : { ParamID::noiseAmt, ParamID::wobbleAmt, ParamID::distortAmt, ParamID::digitalAmt,
                              ParamID::spaceAmt, ParamID::magneticAmt })
                p.apvts.getParameter (id)->setValueNotifyingHost (0.7f);
            p.apvts.getParameter (ParamID::noiseType)->setValueNotifyingHost (type / 2.0f);
            p.prepareToPlay (sr, 333);
            auto io = input;
            const juce::String label = "all-on noise=" + juce::String (type);
            ok &= run (label.toRawUTF8(), p, io, 333);
            if (type == 1)
                writeWav (outDir.getChildFile ("patina_heavy.wav"), io, sr);
        }
    }

    {   // every factory preset: sanity + loudness relative to dry (for level matching)
        double drySq = 0.0;
        for (int i = 0; i < len; ++i) drySq += (double) input.getSample (0, i) * input.getSample (0, i);
        const float dryRms = (float) std::sqrt (drySq / len);
        for (int idx = 0; idx < patina::numFactoryPresets; ++idx)
        {
            PatinaProcessor p;
            p.setPlayConfigDetails (2, 2, sr, 512);
            p.setCurrentProgram (idx);
            p.prepareToPlay (sr, 512);
            auto io = input;
            juce::MidiBuffer midi;
            for (int start = 0; start < len; start += 512)
            {
                juce::AudioBuffer<float> block (io.getArrayOfWritePointers(), 2, start, juce::jmin (512, len - start));
                p.processBlock (block, midi);
            }
            double sq = 0.0; float peak = 0.0f; bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < len; ++i)
                {
                    const float v = io.getSample (ch, i);
                    finite &= std::isfinite (v);
                    sq += (double) v * v; peak = juce::jmax (peak, std::abs (v));
                }
            const float rms = (float) std::sqrt (sq / (2.0 * len));
            const float diff = juce::Decibels::gainToDecibels (rms) - juce::Decibels::gainToDecibels (dryRms);
            std::cout << "preset " << juce::String (idx + 1).paddedLeft ('0', 2) << " "
                      << p.getProgramName (idx).paddedRight (' ', 20) << " loudness vs dry "
                      << juce::String (diff, 1).paddedLeft (' ', 6) << " dB, peak "
                      << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS\n";
            if (! finite || peak > 4.0f) { std::cout << "FAIL preset " << idx << "\n"; ok = false; }
            const auto name = p.getProgramName (idx).replaceCharacter (' ', '_').removeCharacters ("-");
            writeWav (outDir.getChildFile ("presets").getChildFile (juce::String (idx + 1).paddedLeft ('0', 2) + "_" + name + ".wav"), io, sr);
        }
    }

    {   // Magnitude 0 + mix 1: should be a delayed copy of the input
        PatinaProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 256);
        p.apvts.getParameter (ParamID::magnitude)->setValueNotifyingHost (0.0f);
        p.prepareToPlay (sr, 256);
        auto io = input;
        run ("magnitude=0", p, io, 256);
        const int lat = p.getLatencySamples();
        double err = 0.0;
        for (int i = lat; i < len; ++i)
            err = juce::jmax (err, (double) std::abs (io.getSample (0, i) - input.getSample (0, i - lat)));
        std::cout << "magnitude=0 transparency error: " << err << " (latency " << lat << " samples)\n";
        if (err > 1.0e-3) { std::cout << "FAIL transparency\n"; ok = false; }
    }

    {   // editor builds and lays out
        PatinaProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream fos (outDir.getChildFile ("patina_ui.png"));
        fos.setPosition (0); fos.truncate();
        png.writeImageToStream (img, fos);
        std::cout << "editor " << ed->getWidth() << "x" << ed->getHeight() << "\n";
    }

    std::cout << (ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}

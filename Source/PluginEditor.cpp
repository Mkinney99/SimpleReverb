/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleReverbAudioProcessorEditor::SimpleReverbAudioProcessorEditor (SimpleReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      sizeSliderAttachment  (audioProcessor.apvts, "size",    sizeSlider),
      dampSliderAttachment  (audioProcessor.apvts, "damp",    dampSlider),
      widthSliderAttachment (audioProcessor.apvts, "width",   widthSlider),
      dwSliderAttachment    (audioProcessor.apvts, "dry/wet", dwSlider),
      freezeAttachment      (audioProcessor.apvts, "freeze",  freezeButton)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&customLookAndFeel);
    setSize (500, 250);
    setWantsKeyboardFocus (true);

    sizeLabel.setText ("size", juce::NotificationType::dontSendNotification);
    sizeLabel.attachToComponent (&sizeSlider, false);

    dampLabel.setText ("damp", juce::NotificationType::dontSendNotification);
    dampLabel.attachToComponent (&dampSlider, false);

    widthLabel.setText ("width", juce::NotificationType::dontSendNotification);
    widthLabel.attachToComponent (&widthSlider, false);

    dwLabel.setText ("dw", juce::NotificationType::dontSendNotification);
    dwLabel.attachToComponent (&dwSlider, false);

    freezeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("∞")));
    freezeButton.setClickingTogglesState (true);
    freezeButton.setLookAndFeel (&customLookAndFeel);
    freezeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentWhite);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentWhite);
    freezeButton.setColour (juce::TextButton::textColourOnId, MyColours::blue);
    freezeButton.setColour (juce::TextButton::textColourOffId, MyColours::grey);

    addAndMakeVisible (sizeSlider);
    addAndMakeVisible (dampSlider);
    addAndMakeVisible (widthSlider);
    addAndMakeVisible (dwSlider);
    addAndMakeVisible (freezeButton);
}

SimpleReverbAudioProcessorEditor::~SimpleReverbAudioProcessorEditor()
{
  juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
  freezeButton.setLookAndFeel (nullptr);
}

//==============================================================================
void SimpleReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (MyColours::black);

    g.setFont (30);
    g.setColour (MyColours::offWhite);
    g.drawText ("Simple Reverb", 150, 0, 200, 75, juce::Justification::centred);
}

void SimpleReverbAudioProcessorEditor::resized()
{
    sizeSlider.setBounds   (30,  120, 60, 60);
    dampSlider.setBounds   (125, 120, 60, 60);
    widthSlider.setBounds  (315, 120, 60, 60);
    dwSlider.setBounds     (410, 120, 60, 60);
    freezeButton.setBounds (210, 120, 80, 40);
}



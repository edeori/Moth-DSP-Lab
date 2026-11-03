#pragma once
#include "LabTheme.h"
#include "measurement/GuidedWorkflow.h"
#include "measurement/MeasurementReport.h"
namespace lab
{
class CollectionPanel final : public juce::Component, private juce::ListBoxModel
{
  public:
    CollectionPanel(GuidedWorkflow &w, std::function<void()> done) : workflow(w), close(std::move(done))
    {
        for (auto *c : std::vector<juce::Component *>{&title, &list, &preview, &save, &back})
            addAndMakeVisible(c);
        title.setText(
            juce::String::fromUTF8("Egy mérési gyűjtemény — válaszd ki a hozzá tartozó sorozatokat"),
            juce::dontSendNotification);
        list.setModel(this);
        list.setMultipleSelectionEnabled(true);
        list.setRowHeight(34);
        list.setComponentID("collection-sources");
        save.setComponentID("save-collection");
        preview.setMultiLine(true);
        preview.setReadOnly(true);
        preview.setScrollbarsShown(true);
        auto files = workflow.history();
        auto links = workflow.state["collectionSources"];
        juce::SparseSet<int> selectedRows;
        for (const auto &file : files)
        {
            if (file == workflow.file)
                continue;
            candidates.add(file);
            bool selected = false;
            if (auto *ids = links.getArray())
                for (const auto &id : *ids)
                    selected |= id.toString() == file.getFileNameWithoutExtension();
            if (selected)
                selectedRows.addRange({candidates.size() - 1, candidates.size()});
        }
        list.updateContent();
        list.setSelectedRows(selectedRows, juce::dontSendNotification);
        save.onClick = [this]
        {
            try
            {
                auto state = proposed();
                collectionResults(workflow.file.getParentDirectory().getParentDirectory(), state);
                auto before = workflow.state.clone();
                workflow.state = state;
                try
                {
                    workflow.save();
                }
                catch (...)
                {
                    workflow.state = before;
                    throw;
                }
                close();
            }
            catch (const std::exception &e)
            {
                preview.setText(e.what());
            }
        };
        back.onClick = [this] { close(); };
        selectedRowsChanged(0);
    }
    ~CollectionPanel() override
    {
        list.setModel(nullptr);
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced(20);
        title.setBounds(a.removeFromTop(48));
        auto buttons = a.removeFromBottom(40);
        back.setBounds(buttons.removeFromRight(150));
        save.setBounds(buttons.removeFromRight(250));
        list.setBounds(a.removeFromTop(juce::jmin(220, a.getHeight() / 2)));
        a.removeFromTop(12);
        preview.setBounds(a);
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(LabTheme::background());
    }

  private:
    GuidedWorkflow &workflow;
    std::function<void()> close;
    juce::Label title;
    juce::ListBox list;
    juce::TextEditor preview;
    juce::TextButton save{juce::String::fromUTF8("Összevonás mentése")},
        back{juce::String::fromUTF8("Vissza")};
    juce::Array<juce::File> candidates;
    int getNumRows() override
    {
        return candidates.size();
    }
    void paintListBoxItem(int row, juce::Graphics &g, int width, int height, bool selected) override
    {
        if (selected)
            g.fillAll(juce::Colour(0xff25473e));
        g.setColour(juce::Colours::white);
        if (!juce::isPositiveAndBelow(row, candidates.size()))
            return;
        try
        {
            auto w = readJson(candidates[row]);
            g.drawText((selected ? "✓ " : "+ ") + w["name"].toString() + " | " + w["createdUtc"].toString(),
                       8, 0, width - 16, height, juce::Justification::centredLeft);
        }
        catch (...)
        {
        }
    }
    juce::var proposed()
    {
        auto state = workflow.state.clone();
        juce::Array<juce::var> links;
        for (int i = 0; i < candidates.size(); ++i)
            if (list.isRowSelected(i))
                links.add(candidates[i].getFileNameWithoutExtension());
        set(state, "collectionSources", juce::var(links));
        return state;
    }
    void selectedRowsChanged(int) override
    {
        try
        {
            auto all = collectionResults(workflow.file.getParentDirectory().getParentDirectory(), proposed());
            juce::String text =
                juce::String::fromUTF8(
                    "Az aktuális sorozat már benne van. További sorozat: Cmd/Ctrl-kattintás.\nCsak azonos "
                    "eszköz méréseit válaszd. A hibásnak jelölt felvételek kimaradnak.\n\n") +
                juce::String(all.size()) + juce::String::fromUTF8(" mérés lesz az egyetlen ZIP-ben:\n\n");
            int i = 0;
            for (const auto &f : all)
                text += juce::String(++i) + ". " +
                        SessionStore::load(f)["hardware"]["scenarioStep"]["title"].toString() + "\n";
            preview.setText(text);
            save.setEnabled(true);
        }
        catch (const std::exception &e)
        {
            preview.setText(e.what());
            save.setEnabled(false);
        }
    }
};
} // namespace lab

#pragma once

#include "Controls.h"

namespace subterra
{
struct MenuItem
{
    enum class Kind { item, header, separator };

    static MenuItem header (const juce::String& text)    { MenuItem m; m.kind = Kind::header; m.text = text; return m; }
    static MenuItem separator()                           { MenuItem m; m.kind = Kind::separator; return m; }
    static MenuItem item (const juce::String& text, std::function<void()> action, bool ticked = false)
    {
        MenuItem m;
        m.text = text;
        m.action = std::move (action);
        m.ticked = ticked;
        return m;
    }

    Kind kind = Kind::item;
    juce::String text, detail;                         // detail is drawn right-aligned in a dimmer colour
    bool ticked = false, enabled = true;
    bool keepOpen = false;                             // run the action and leave the menu up (checkbox rows)
    std::function<void()> action;
    std::function<std::vector<MenuItem>()> submenu;    // opens a page that slides in
    std::function<void()> onDelete;                    // shows a delete button on the row (user presets)
};

class Overlay;

// Base for the cards shown in the overlay: menus and dialogs.
class Card : public juce::Component
{
public:
    explicit Card (Overlay& o) : overlay (o) {}
    virtual juce::Component* getFocusTarget() { return this; }
    // Where the card grows from when it opens, in the card's coordinates.
    juce::Point<float> origin;

protected:
    void paintCardBackground (juce::Graphics&, juce::Rectangle<float>) const;
    Overlay& overlay;
};

// Drop-down menu with a sliding hover highlight, submenus that slide in from the right,
// scrolling for long lists and full keyboard control.
class MenuCard final : public Card
{
public:
    MenuCard (Overlay&, SubterraLookAndFeel&, const juce::String& title, std::vector<MenuItem> items, int width, int maxHeight);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // Runs the item's action as if it had been clicked (used by tests and keyboard).
    bool activate (const juce::String& text);
    const std::vector<MenuItem>& getCurrentItems() const { return pages.back().items; }
    void openSubmenu (int index);
    void goBack();
    void selectRow (int index);

private:
    struct Page
    {
        juce::String title;
        std::vector<MenuItem> items;
        float scroll = 0.0f;
        int selected = -1;
    };

    static float rowHeight (const MenuItem&);
    float contentHeight (const Page&) const;
    float titleHeight (const Page&) const { return title.isNotEmpty() || pages.size() > 1 ? 34.0f : 0.0f; }
    float cardHeightFor (const Page&) const;
    juce::Rectangle<float> rowBounds (const Page&, int index) const;
    juce::Rectangle<float> deleteBounds (const Page&, int index) const;
    int rowAt (const Page&, juce::Point<float>) const;
    bool isSelectable (const MenuItem& m) const { return m.kind == MenuItem::Kind::item && m.enabled; }
    void moveSelection (int direction);
    void ensureVisible (int index);
    void trigger (int index);
    void paintPage (juce::Graphics&, const Page&, float xOffset, float alpha, bool isCurrent);
    void resizeForCurrentPage (bool animate);
    float maxScroll (const Page&) const;
    float listTop (const Page& p) const { return 6.0f + titleHeight (p); }

    SubterraLookAndFeel& lnf;
    juce::String title;
    std::vector<Page> pages;
    std::unique_ptr<Page> leaving;    // the page sliding out after Back or into a submenu
    int width, maxHeight;
    int pressedRow = -1;
    bool hoverDelete = false;
    Tween slide, highlightY, highlightAlpha, height, scrollAnim;
    bool slidingForward = true;
    FrameDriver frames { *this, [this] { return tick(); } };
    bool tick();
};

// Small modal card with a title, a message and up to two buttons.
class DialogCard : public Card, private juce::TextEditor::Listener
{
public:
    DialogCard (Overlay&, SubterraLookAndFeel&, const juce::String& title, const juce::String& message,
                const juce::String& confirmText, bool destructive);

    // Adds a text field; onTextChanged lets the caller update the message (e.g. "replaces your preset").
    void addTextField (const juce::String& initialText, std::function<juce::String (const juce::String&)> describe);
    juce::String getText() const { return field != nullptr ? field->getText() : juce::String(); }

    std::function<bool()> onConfirm;    // return false to keep the dialog open
    std::function<void()> onCancel;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    juce::Component* getFocusTarget() override;
    void confirm();
    void cancel();

    class PillButton;

private:
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override { confirm(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { cancel(); }
    void updateNotice();

    SubterraLookAndFeel& lnf;
    juce::String title, message, notice;
    std::unique_ptr<juce::TextEditor> field;
    std::function<juce::String (const juce::String&)> describe;
    std::unique_ptr<PillButton> confirmButton, cancelButton;
    Tween noticeFade;
    FrameDriver frames { *this, [this] { return noticeFade.update(); } };
};

// Full-window layer for menus and dialogs. Dims the plugin behind the card,
// closes when you click outside it or press Escape, and animates in and out.
class Overlay final : public juce::Component
{
public:
    explicit Overlay (SubterraLookAndFeel&);
    ~Overlay() override;

    MenuCard& showMenu (juce::Rectangle<int> anchor, const juce::String& title, std::vector<MenuItem> items, int width = 248);
    DialogCard& showDialog (const juce::String& title, const juce::String& message, const juce::String& confirmText,
                            bool destructive = false);
    void dismiss();
    void dismissImmediately();
    bool isActive() const { return card != nullptr && ! closing; }
    Card* getCard() const { return card.get(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void present (std::unique_ptr<Card>, juce::Rectangle<int> bounds, bool dimBackground);
    bool tick();
    void applyCardTransform();

    SubterraLookAndFeel& lnf;
    std::unique_ptr<Card> card;
    Tween appear;
    bool closing = false, dim = false;
    juce::Component::SafePointer<juce::Component> previousFocus;
    FrameDriver frames { *this, [this] { return tick(); } };
};

// Short message that slides up from the bottom and fades away, e.g. "Preset saved".
class Toast final : public juce::Component
{
public:
    explicit Toast (SubterraLookAndFeel&);
    void show (const juce::String& text, bool isError = false);
    void paint (juce::Graphics&) override;

private:
    SubterraLookAndFeel& lnf;
    juce::String text;
    bool error = false;
    double hideAt = 0.0;
    Tween fade;
    FrameDriver frames { *this, [this] { return tick(); } };
    bool tick();
};
}

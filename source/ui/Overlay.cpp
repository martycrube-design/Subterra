#include "Overlay.h"

namespace subterra
{
namespace
{
const juce::Colour cardFill { 0xff202125 };
const juce::Colour cardEdge { 0x26ece6da };

void drawTick (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + r.getWidth() * 0.15f, r.getCentreY());
    p.lineTo (r.getX() + r.getWidth() * 0.42f, r.getBottom() - r.getHeight() * 0.2f);
    p.lineTo (r.getRight() - r.getWidth() * 0.1f, r.getY() + r.getHeight() * 0.2f);
    g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawChevron (juce::Graphics& g, juce::Point<float> c, bool pointsRight, float size = 4.0f)
{
    const float d = pointsRight ? 1.0f : -1.0f;
    juce::Path p;
    p.startNewSubPath (c.x - d * size * 0.5f, c.y - size);
    p.lineTo (c.x + d * size * 0.5f, c.y);
    p.lineTo (c.x - d * size * 0.5f, c.y + size);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawBin (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto c = r.getCentre();
    juce::Path p;
    p.startNewSubPath (c.x - 5.0f, c.y - 3.5f);
    p.lineTo (c.x + 5.0f, c.y - 3.5f);
    p.startNewSubPath (c.x - 1.8f, c.y - 5.5f);
    p.lineTo (c.x + 1.8f, c.y - 5.5f);
    p.addRoundedRectangle (c.x - 3.8f, c.y - 2.0f, 7.6f, 8.0f, 1.2f);
    g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Keeps preset names usable as file names on every platform.
class NameFilter final : public juce::TextEditor::InputFilter
{
public:
    juce::String filterNewText (juce::TextEditor& editor, const juce::String& newInput) override
    {
        auto text = newInput.removeCharacters ("\\/:*?\"<>|\t\r\n");
        const int room = 48 - (editor.getTotalNumChars() - editor.getHighlightedRegion().getLength());
        return text.substring (0, juce::jmax (0, room));
    }
};
}

//==============================================================================
void Card::paintCardBackground (juce::Graphics& g, juce::Rectangle<float> r) const
{
    g.setColour (cardFill);
    g.fillRoundedRectangle (r, 12.0f);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.045f), 0.0f, r.getY(),
                                juce::Colours::transparentWhite, 0.0f, r.getY() + 60.0f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (cardEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
}

//==============================================================================
MenuCard::MenuCard (Overlay& o, SubterraLookAndFeel& l, const juce::String& t, std::vector<MenuItem> items, int w, int maxH)
    : Card (o), lnf (l), title (t), width (w), maxHeight (maxH)
{
    setTitle (t.isNotEmpty() ? t : juce::String ("Menu"));
    setWantsKeyboardFocus (true);
    pages.push_back ({ t, std::move (items) });
    slide.setImmediately (1.0f);
    height.setImmediately (cardHeightFor (pages.back()));
    setSize (width, juce::roundToInt (height.get()));
}

float MenuCard::rowHeight (const MenuItem& m)
{
    switch (m.kind)
    {
        case MenuItem::Kind::header:    return 28.0f;
        case MenuItem::Kind::separator: return 11.0f;
        case MenuItem::Kind::item:      break;
    }
    return 30.0f;
}

float MenuCard::contentHeight (const Page& p) const
{
    float h = 0.0f;
    for (const auto& m : p.items)
        h += rowHeight (m);
    return h;
}

float MenuCard::cardHeightFor (const Page& p) const
{
    return juce::jmin ((float) maxHeight, listTop (p) + contentHeight (p) + 6.0f);
}

float MenuCard::maxScroll (const Page& p) const
{
    return juce::jmax (0.0f, contentHeight (p) - (cardHeightFor (p) - listTop (p) - 6.0f));
}

juce::Rectangle<float> MenuCard::rowBounds (const Page& p, int index) const
{
    float y = listTop (p) - p.scroll;
    for (int i = 0; i < index; ++i)
        y += rowHeight (p.items[(size_t) i]);
    return { 6.0f, y, (float) width - 12.0f, rowHeight (p.items[(size_t) index]) };
}

juce::Rectangle<float> MenuCard::deleteBounds (const Page& p, int index) const
{
    const auto r = rowBounds (p, index);
    return r.withLeft (r.getRight() - 30.0f).reduced (3.0f);
}

int MenuCard::rowAt (const Page& p, juce::Point<float> pos) const
{
    if (pos.y < listTop (p) || pos.y > (float) getHeight() - 6.0f)
        return -1;
    for (int i = 0; i < (int) p.items.size(); ++i)
        if (rowBounds (p, i).contains (pos))
            return isSelectable (p.items[(size_t) i]) ? i : -1;
    return -1;
}

void MenuCard::selectRow (int index)
{
    auto& p = pages.back();
    if (index == p.selected)
        return;

    if (index >= 0)
    {
        const float y = rowBounds (p, index).getY() + p.scroll;
        if (p.selected < 0 || highlightAlpha.get() < 0.05f)
            highlightY.setImmediately (y);
        else
            highlightY.animateTo (y, 110.0);
        highlightAlpha.animateTo (1.0f, 90.0);
    }
    else
    {
        highlightAlpha.animateTo (0.0f, 140.0);
    }

    p.selected = index;
    repaint();
}

void MenuCard::moveSelection (int direction)
{
    auto& p = pages.back();
    const int n = (int) p.items.size();
    int i = p.selected;
    for (int step = 0; step < n; ++step)
    {
        i = i < 0 ? (direction > 0 ? 0 : n - 1) : (i + direction + n) % n;
        if (isSelectable (p.items[(size_t) i]))
        {
            selectRow (i);
            ensureVisible (i);
            return;
        }
    }
}

void MenuCard::ensureVisible (int index)
{
    auto& p = pages.back();
    const auto r = rowBounds (p, index).translated (0.0f, p.scroll);   // content position
    const float viewTop = listTop (p), viewBottom = (float) getHeight() - 6.0f;
    float target = scrollAnim.getTarget();
    if (r.getY() - target < viewTop)
        target = r.getY() - viewTop;
    else if (r.getBottom() - target > viewBottom)
        target = r.getBottom() - viewBottom;
    target = juce::jlimit (0.0f, maxScroll (p), target);
    if (std::abs (target - scrollAnim.getTarget()) > 0.5f)
        scrollAnim.animateTo (target, 140.0);
}

void MenuCard::trigger (int index)
{
    auto& p = pages.back();
    if (! juce::isPositiveAndBelow (index, (int) p.items.size()) || ! isSelectable (p.items[(size_t) index]))
        return;

    auto& item = p.items[(size_t) index];
    if (item.submenu != nullptr)
    {
        openSubmenu (index);
        return;
    }

    if (item.keepOpen)
    {
        item.ticked = ! item.ticked;
        if (item.action != nullptr)
            item.action();
        repaint();
        return;
    }

    // Copy first: the action may replace this card with a dialog, which deletes it.
    auto action = item.action;
    overlay.dismiss();
    if (action != nullptr)
        action();
}

bool MenuCard::activate (const juce::String& text)
{
    const auto& items = pages.back().items;
    for (int i = 0; i < (int) items.size(); ++i)
        if (items[(size_t) i].text == text && isSelectable (items[(size_t) i]))
        {
            trigger (i);
            return true;
        }
    return false;
}

void MenuCard::openSubmenu (int index)
{
    auto& parent = pages.back();
    const auto& item = parent.items[(size_t) index];
    Page next { item.text, item.submenu() };

    leaving = std::make_unique<Page> (parent);
    pages.push_back (std::move (next));
    slidingForward = true;
    slide.setImmediately (0.0f);
    slide.animateTo (1.0f, 280.0, Easing::inOutCubic);
    scrollAnim.setImmediately (0.0f);
    highlightAlpha.setImmediately (0.0f);
    resizeForCurrentPage (true);

    if (hasKeyboardFocus (false))
    {
        auto& current = pages.back();
        int start = 0;
        for (int i = 0; i < (int) current.items.size(); ++i)
            if (current.items[(size_t) i].ticked)
                start = i;
        current.selected = -1;
        selectRow (start);
        ensureVisible (start);
    }
    repaint();
}

void MenuCard::goBack()
{
    if (pages.size() < 2)
        return;

    leaving = std::make_unique<Page> (pages.back());
    pages.pop_back();
    slidingForward = false;
    slide.setImmediately (0.0f);
    slide.animateTo (1.0f, 280.0, Easing::inOutCubic);
    scrollAnim.setImmediately (pages.back().scroll);

    auto& p = pages.back();
    const int keep = p.selected;
    p.selected = -1;
    highlightAlpha.setImmediately (0.0f);
    if (keep >= 0)
        selectRow (keep);
    resizeForCurrentPage (true);
    repaint();
}

void MenuCard::resizeForCurrentPage (bool animate)
{
    height.animateTo (cardHeightFor (pages.back()), animate ? 280.0 : 0.0, Easing::inOutCubic);
    if (! animate)
        setSize (width, juce::roundToInt (height.get()));
}

bool MenuCard::tick()
{
    bool changed = false;
    changed |= slide.update();
    changed |= highlightY.update();
    changed |= highlightAlpha.update();
    if (scrollAnim.update())
    {
        pages.back().scroll = scrollAnim.get();
        changed = true;
    }
    if (height.update())
    {
        setSize (width, juce::roundToInt (height.get()));
        changed = true;
    }
    if (! slide.isRunning() && leaving != nullptr)
    {
        leaving.reset();
        changed = true;
    }
    return changed;
}

void MenuCard::paintPage (juce::Graphics& g, const Page& p, float xOffset, float alpha, bool isCurrent)
{
    if (alpha <= 0.0f)
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::translation (xOffset, 0.0f));
    const auto ink = Colours::frameInk.withMultipliedAlpha (alpha);

    // Title row; on a submenu it doubles as the Back button.
    const float th = titleHeight (p);
    if (th > 0.0f)
    {
        const auto row = juce::Rectangle<float> (6.0f, 6.0f, (float) width - 12.0f, th - 6.0f);
        const bool isSub = isCurrent ? pages.size() > 1 : (! slidingForward || pages.size() > 1) && p.title != title;
        g.setColour (ink.withMultipliedAlpha (0.6f));
        if (isSub)
        {
            if (isCurrent && pressedRow == -2)
            {
                g.setColour (Colours::frameInk.withAlpha (0.08f * alpha));
                g.fillRoundedRectangle (row, 7.0f);
                g.setColour (ink.withMultipliedAlpha (0.6f));
            }
            drawChevron (g, { row.getX() + 14.0f, row.getCentreY() }, false);
            SubterraLookAndFeel::drawSpacedText (g, p.title.toUpperCase(), row.withTrimmedLeft (28.0f), lnf.inter (10.0f, 700, 1.5f),
                                                 juce::Justification::centredLeft);
        }
        else
        {
            SubterraLookAndFeel::drawSpacedText (g, p.title.toUpperCase(), row.withTrimmedLeft (10.0f), lnf.inter (10.0f, 700, 1.5f),
                                                 juce::Justification::centredLeft);
        }
        g.setColour (ink.withMultipliedAlpha (0.1f));
        g.fillRect (12.0f, th + 1.0f, (float) width - 24.0f, 1.0f);
    }

    // List, clipped to the card body so it scrolls under the title.
    g.reduceClipRegion (juce::Rectangle<float> (0.0f, listTop (p), (float) width, (float) getHeight() - listTop (p) - 4.0f).toNearestInt());

    if (isCurrent && highlightAlpha.get() > 0.0f && p.selected >= 0)
    {
        const auto r = rowBounds (p, p.selected).withY (highlightY.get() - p.scroll);
        g.setColour (Colours::frameInk.withAlpha (0.1f * highlightAlpha.get() * alpha));
        g.fillRoundedRectangle (r, 7.0f);
    }

    for (int i = 0; i < (int) p.items.size(); ++i)
    {
        const auto& m = p.items[(size_t) i];
        const auto r = rowBounds (p, i);
        if (r.getBottom() < listTop (p) || r.getY() > (float) getHeight())
            continue;

        if (m.kind == MenuItem::Kind::separator)
        {
            g.setColour (ink.withMultipliedAlpha (0.1f));
            g.fillRect (r.getX() + 6.0f, r.getCentreY(), r.getWidth() - 12.0f, 1.0f);
            continue;
        }

        if (m.kind == MenuItem::Kind::header)
        {
            g.setColour (ink.withMultipliedAlpha (0.45f));
            SubterraLookAndFeel::drawSpacedText (g, m.text.toUpperCase(), r.withTrimmedLeft (10.0f).withTrimmedTop (6.0f),
                                                 lnf.inter (9.0f, 700, 1.5f), juce::Justification::centredLeft);
            continue;
        }

        const bool selected = isCurrent && i == p.selected;
        const auto rowInk = ink.withMultipliedAlpha (m.enabled ? 1.0f : 0.4f);
        if (isCurrent && pressedRow == i)
        {
            g.setColour (Colours::frameInk.withAlpha (0.08f * alpha));
            g.fillRoundedRectangle (r, 7.0f);
        }

        g.setColour (rowInk);
        if (m.ticked)
            drawTick (g, juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ r.getX() + 16.0f, r.getCentreY() }));

        auto textArea = r.withTrimmedLeft (30.0f).withTrimmedRight (10.0f);
        if (m.submenu != nullptr)
        {
            drawChevron (g, { r.getRight() - 14.0f, r.getCentreY() }, true);
            textArea.removeFromRight (16.0f);
        }

        const bool showDelete = m.onDelete != nullptr && selected;
        if (m.onDelete != nullptr)
            textArea.removeFromRight (22.0f);

        if (m.detail.isNotEmpty())
        {
            g.setColour (rowInk.withMultipliedAlpha (0.5f));
            g.setFont (lnf.inter (12.0f, 500));
            g.drawText (m.detail, textArea, juce::Justification::centredRight, true);
        }

        g.setColour (rowInk);
        g.setFont (lnf.inter (13.0f, m.ticked ? 600 : 500));
        g.drawText (m.text, textArea, juce::Justification::centredLeft, true);

        if (showDelete)
        {
            const auto bin = deleteBounds (p, i);
            if (hoverDelete)
            {
                g.setColour (Colours::led.withAlpha (0.18f * alpha));
                g.fillRoundedRectangle (bin, 6.0f);
            }
            g.setColour ((hoverDelete ? Colours::led : Colours::frameInk.withAlpha (0.6f)).withMultipliedAlpha (alpha));
            drawBin (g, bin);
        }
    }

    // Fades at the edges when the list scrolls.
    const float max = maxScroll (p);
    if (max > 0.0f)
    {
        const float top = listTop (p), bottom = (float) getHeight() - 6.0f;
        if (p.scroll > 0.5f)
        {
            g.setGradientFill (juce::ColourGradient (cardFill.withMultipliedAlpha (alpha), 0.0f, top,
                                                     cardFill.withAlpha (0.0f), 0.0f, top + 22.0f, false));
            g.fillRect (0.0f, top, (float) width, 22.0f);
        }
        if (p.scroll < max - 0.5f)
        {
            g.setGradientFill (juce::ColourGradient (cardFill.withAlpha (0.0f), 0.0f, bottom - 26.0f,
                                                     cardFill.withMultipliedAlpha (alpha), 0.0f, bottom, false));
            g.fillRect (0.0f, bottom - 26.0f, (float) width, 26.0f);
        }

        const float view = bottom - top;
        const float barH = juce::jmax (24.0f, view * view / (view + max));
        const float barY = top + (view - barH) * (p.scroll / max);
        g.setColour (Colours::frameInk.withAlpha (0.25f * alpha));
        g.fillRoundedRectangle ((float) width - 5.0f, barY, 2.5f, barH, 1.25f);
    }
}

void MenuCard::paint (juce::Graphics& g)
{
    paintCardBackground (g, getLocalBounds().toFloat());

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
    g.reduceClipRegion (clip);

    const float t = slide.get();
    const float w = (float) width;
    if (leaving != nullptr && slide.isRunning())
    {
        if (slidingForward)
        {
            paintPage (g, *leaving, -w * 0.35f * t, 1.0f - t, false);
            paintPage (g, pages.back(), w * (1.0f - t), 1.0f, true);
        }
        else
        {
            paintPage (g, *leaving, w * t, 1.0f, false);
            paintPage (g, pages.back(), -w * 0.35f * (1.0f - t), t, true);
        }
    }
    else
    {
        paintPage (g, pages.back(), 0.0f, 1.0f, true);
    }
}

void MenuCard::mouseMove (const juce::MouseEvent& e)
{
    auto& p = pages.back();
    selectRow (rowAt (p, e.position));
    const bool overDelete = p.selected >= 0 && p.items[(size_t) p.selected].onDelete != nullptr
                            && deleteBounds (p, p.selected).contains (e.position);
    if (overDelete != hoverDelete)
    {
        hoverDelete = overDelete;
        repaint();
    }
}

void MenuCard::mouseExit (const juce::MouseEvent&)
{
    hoverDelete = false;
    selectRow (-1);
}

void MenuCard::mouseDown (const juce::MouseEvent& e)
{
    auto& p = pages.back();
    if (pages.size() > 1 && e.position.y < titleHeight (p) + 4.0f)
        pressedRow = -2;
    else
        pressedRow = rowAt (p, e.position);
    repaint();
}

void MenuCard::mouseUp (const juce::MouseEvent& e)
{
    const int pressed = std::exchange (pressedRow, -1);
    repaint();

    auto& p = pages.back();
    if (pressed == -2)
    {
        if (e.position.y < titleHeight (p) + 4.0f && e.position.y >= 0.0f)
            goBack();
        return;
    }

    if (pressed < 0 || rowAt (p, e.position) != pressed)
        return;

    auto& item = p.items[(size_t) pressed];
    if (item.onDelete != nullptr && deleteBounds (p, pressed).contains (e.position))
    {
        auto onDelete = item.onDelete;
        onDelete();
        return;
    }

    trigger (pressed);
}

void MenuCard::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto& p = pages.back();
    const float max = maxScroll (p);
    if (max <= 0.0f)
        return;

    const float delta = wheel.deltaY * (wheel.isSmooth ? 260.0f : 140.0f) * (wheel.isReversed ? -1.0f : 1.0f);
    const float target = juce::jlimit (0.0f, max, scrollAnim.getTarget() - delta);
    if (wheel.isSmooth)
    {
        scrollAnim.setImmediately (target);
        p.scroll = target;
        repaint();
    }
    else
    {
        scrollAnim.animateTo (target, 120.0);
    }

    // Keep the highlight under the pointer.
    p.selected = -1;
    highlightAlpha.setImmediately (0.0f);
    mouseMove (e);
}

bool MenuCard::keyPressed (const juce::KeyPress& key)
{
    auto& p = pages.back();
    if (key == juce::KeyPress::downKey)  { moveSelection (1); return true; }
    if (key == juce::KeyPress::upKey)    { moveSelection (-1); return true; }
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        if (p.selected >= 0)
            trigger (p.selected);
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        if (p.selected >= 0 && p.items[(size_t) p.selected].submenu != nullptr)
            openSubmenu (p.selected);
        return true;
    }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::backspaceKey)
    {
        goBack();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (pages.size() > 1)
            goBack();
        else
            overlay.dismiss();
        return true;
    }
    return false;
}

//==============================================================================
class DialogCard::PillButton final : public juce::Button
{
public:
    PillButton (SubterraLookAndFeel& l, const juce::String& label, bool isPrimary, bool isDestructive)
        : juce::Button (label), lnf (l), primary (isPrimary), destructive (isDestructive)
    {
        setButtonText (label);
        setTitle (label);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void buttonStateChanged() override { hover.animateTo (isDown() ? 2.0f : isOver() ? 1.0f : 0.0f, 110.0); }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.75f);
        const float h = hover.get();
        const float alpha = isEnabled() ? 1.0f : 0.35f;
        if (primary)
        {
            const auto fill = (destructive ? Colours::led : Colours::frameInk).withMultipliedAlpha (alpha);
            g.setColour (fill.brighter (0.08f * juce::jmin (h, 1.0f)).darker (h > 1.0f ? 0.15f : 0.0f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour ((destructive ? juce::Colours::white : Colours::frame).withMultipliedAlpha (alpha));
        }
        else
        {
            g.setColour (Colours::frameInk.withAlpha (0.08f * h));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (Colours::frameInk.withAlpha (0.35f * alpha));
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
            g.setColour (Colours::frameInk.withAlpha (alpha));
        }
        SubterraLookAndFeel::drawSpacedText (g, getButtonText().toUpperCase(), r, lnf.inter (11.0f, 700, 1.0f));
    }

private:
    SubterraLookAndFeel& lnf;
    bool primary, destructive;
    Tween hover;
    FrameDriver frames { *this, [this] { return hover.update(); } };
};

DialogCard::DialogCard (Overlay& o, SubterraLookAndFeel& l, const juce::String& t, const juce::String& m,
                        const juce::String& confirmText, bool destructive)
    : Card (o), lnf (l), title (t), message (m)
{
    setTitle (t);
    setWantsKeyboardFocus (true);
    confirmButton = std::make_unique<PillButton> (l, confirmText, true, destructive);
    cancelButton = std::make_unique<PillButton> (l, "Cancel", false, false);
    addAndMakeVisible (*confirmButton);
    addAndMakeVisible (*cancelButton);
    confirmButton->onClick = [this] { confirm(); };
    cancelButton->onClick = [this] { cancel(); };
    setSize (340, 170);
}

void DialogCard::addTextField (const juce::String& initialText, std::function<juce::String (const juce::String&)> fn)
{
    describe = std::move (fn);
    field = std::make_unique<juce::TextEditor> ("Preset name");
    field->setTitle ("Preset name");
    field->setInputFilter (new NameFilter(), true);
    field->setFont (lnf.inter (15.0f, 500));
    field->setIndents (12, 0);
    field->setJustification (juce::Justification::centredLeft);
    field->setTextToShowWhenEmpty ("Preset name", Colours::frameInk.withAlpha (0.3f));
    field->setSelectAllWhenFocused (true);
    field->addListener (this);
    field->setText (initialText, false);
    addAndMakeVisible (*field);
    setSize (340, 214);
    updateNotice();
}

juce::Component* DialogCard::getFocusTarget()
{
    return field != nullptr ? static_cast<juce::Component*> (field.get()) : this;
}

void DialogCard::resized()
{
    auto r = getLocalBounds().reduced (22, 20);
    auto buttons = r.removeFromBottom (32);
    confirmButton->setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (10);
    cancelButton->setBounds (buttons.removeFromRight (96));

    if (field != nullptr)
        field->setBounds (22, 92, getWidth() - 44, 38);
}

void DialogCard::paint (juce::Graphics& g)
{
    paintCardBackground (g, getLocalBounds().toFloat());

    g.setColour (Colours::frameInk);
    SubterraLookAndFeel::drawSpacedText (g, title.toUpperCase(), { 22.0f, 18.0f, (float) getWidth() - 44.0f, 28.0f },
                                         lnf.bebas (24.0f, 2.0f), juce::Justification::centredLeft);

    g.setColour (Colours::frameInk.withAlpha (0.7f));
    g.setFont (lnf.inter (13.0f, 500));
    g.drawFittedText (message, juce::Rectangle<int> (22, 48, getWidth() - 44, 36), juce::Justification::topLeft, 2, 1.0f);

    if (field != nullptr && noticeFade.get() > 0.0f)
    {
        g.setColour (Colours::frameInk.withAlpha (0.6f * noticeFade.get()));
        g.setFont (lnf.inter (11.5f, 500));
        g.drawText (notice, juce::Rectangle<float> (24.0f, 136.0f, (float) getWidth() - 46.0f, 16.0f), juce::Justification::centredLeft, true);
    }
}

void DialogCard::textEditorTextChanged (juce::TextEditor&) { updateNotice(); }

void DialogCard::updateNotice()
{
    if (field == nullptr)
        return;
    const auto text = field->getText();
    confirmButton->setEnabled (text.trim().isNotEmpty());
    const auto next = describe != nullptr ? describe (text) : juce::String();
    if (next.isNotEmpty())
        notice = next;
    noticeFade.animateTo (next.isNotEmpty() ? 1.0f : 0.0f, isShowing() ? 150.0 : 0.0);
    repaint();
}

void DialogCard::confirm()
{
    if (! confirmButton->isEnabled())
        return;
    if (onConfirm != nullptr && ! onConfirm())
        return;
    overlay.dismiss();
}

void DialogCard::cancel()
{
    if (onCancel != nullptr)
        onCancel();
    overlay.dismiss();
}

bool DialogCard::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey) { confirm(); return true; }
    if (key == juce::KeyPress::escapeKey) { cancel(); return true; }
    return false;
}

//==============================================================================
Overlay::Overlay (SubterraLookAndFeel& l) : lnf (l)
{
    setVisible (false);
    setWantsKeyboardFocus (true);
    setTitle ("Overlay");
}

Overlay::~Overlay()
{
    card.reset();
}

MenuCard& Overlay::showMenu (juce::Rectangle<int> anchor, const juce::String& title, std::vector<MenuItem> items, int width)
{
    const int maxHeight = juce::jmax (160, getHeight() - anchor.getBottom() - 24);
    auto menu = std::make_unique<MenuCard> (*this, lnf, title, std::move (items), width, maxHeight);
    auto& ref = *menu;

    const int x = juce::jlimit (12, getWidth() - width - 12, anchor.getCentreX() - width / 2);
    const int y = anchor.getBottom() + 8;
    ref.origin = { (float) (anchor.getCentreX() - x), 0.0f };
    present (std::move (menu), { x, y, width, ref.getHeight() }, false);
    return ref;
}

DialogCard& Overlay::showDialog (const juce::String& title, const juce::String& message, const juce::String& confirmText, bool destructive)
{
    auto dialog = std::make_unique<DialogCard> (*this, lnf, title, message, confirmText, destructive);
    auto& ref = *dialog;
    ref.origin = { (float) ref.getWidth() * 0.5f, (float) ref.getHeight() * 0.5f };
    present (std::move (dialog), juce::Rectangle<int> (ref.getWidth(), ref.getHeight()).withCentre ({ getWidth() / 2, 300 }), true);
    return ref;
}

void Overlay::present (std::unique_ptr<Card> newCard, juce::Rectangle<int> bounds, bool dimBackground)
{
    if (card != nullptr)
    {
        // Replacing a card that is still open or closing, e.g. a menu that opens a dialog.
        removeChildComponent (card.get());
        card.reset();
    }
    else
    {
        previousFocus = juce::Component::getCurrentlyFocusedComponent();
    }

    card = std::move (newCard);
    closing = false;
    dim = dimBackground;
    addAndMakeVisible (*card);
    card->setBounds (bounds);
    if (auto* dialog = dynamic_cast<DialogCard*> (card.get()))
        dialog->resized();

    setVisible (true);
    toFront (false);
    appear.setImmediately (0.0f);
    appear.animateTo (1.0f, isShowing() ? 200.0 : 0.0, Easing::outCubic);
    applyCardTransform();
    repaint();

    if (isShowing())
        card->getFocusTarget()->grabKeyboardFocus();
}

void Overlay::dismiss()
{
    if (card == nullptr || closing)
        return;
    closing = true;
    appear.animateTo (0.0f, isShowing() ? 140.0 : 0.0, Easing::outCubic);
    if (! appear.isRunning())
        dismissImmediately();
}

void Overlay::dismissImmediately()
{
    if (card == nullptr)
        return;

    const bool hadFocus = card->hasKeyboardFocus (true) || hasKeyboardFocus (false);
    removeChildComponent (card.get());
    card.reset();
    closing = false;
    setVisible (false);

    if (hadFocus)
    {
        if (previousFocus != nullptr && previousFocus->isShowing())
            previousFocus->grabKeyboardFocus();
        else
            giveAwayKeyboardFocus();
    }
}

bool Overlay::tick()
{
    if (! appear.update())
        return false;

    applyCardTransform();
    if (closing && ! appear.isRunning())
        dismissImmediately();
    return true;
}

void Overlay::applyCardTransform()
{
    if (card == nullptr)
        return;

    const float t = appear.get();
    const float scale = 0.94f + 0.06f * t;
    const auto pivot = card->getPosition().toFloat() + card->origin;
    card->setTransform (juce::AffineTransform::scale (scale, scale, pivot.x, pivot.y).translated (0.0f, -6.0f * (1.0f - t)));
    card->setAlpha (juce::jlimit (0.0f, 1.0f, t));
}

void Overlay::paint (juce::Graphics& g)
{
    const float t = appear.get();
    g.fillAll (juce::Colours::black.withAlpha ((dim ? 0.5f : 0.22f) * t));

    if (card != nullptr)
    {
        juce::Path shape;
        shape.addRoundedRectangle (card->getBounds().toFloat(), 12.0f);
        shape.applyTransform (card->getTransform());
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f * t), 30, { 0, 14 }).drawForPath (g, shape);
    }
}

void Overlay::mouseDown (const juce::MouseEvent&)
{
    // A click outside the card closes it. Dialogs ask for an explicit choice.
    if (auto* dialog = dynamic_cast<DialogCard*> (card.get()))
        dialog->cancel();
    else
        dismiss();
}

bool Overlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && card != nullptr)
    {
        if (auto* dialog = dynamic_cast<DialogCard*> (card.get()))
            dialog->cancel();
        else
            dismiss();
        return true;
    }
    return false;
}

//==============================================================================
Toast::Toast (SubterraLookAndFeel& l) : lnf (l)
{
    setInterceptsMouseClicks (false, false);
    setVisible (false);
}

void Toast::show (const juce::String& newText, bool isError)
{
    text = newText;
    error = isError;
    hideAt = juce::Time::getMillisecondCounterHiRes() + (isError ? 4000.0 : 1800.0);
    setVisible (true);
    toFront (false);
    fade.animateTo (1.0f, isShowing() ? 200.0 : 0.0);
    repaint();
}

bool Toast::tick()
{
    if (isVisible() && fade.getTarget() > 0.5f && juce::Time::getMillisecondCounterHiRes() > hideAt)
        fade.animateTo (0.0f, 320.0);

    if (! fade.update())
        return false;

    if (! fade.isRunning() && fade.get() <= 0.0f)
        setVisible (false);
    return true;
}

void Toast::paint (juce::Graphics& g)
{
    const float t = fade.get();
    if (t <= 0.0f)
        return;

    g.setFont (lnf.inter (12.5f, 600));
    const float textWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
    const float w = juce::jmin ((float) getWidth() - 4.0f, textWidth + 44.0f);
    auto pill = juce::Rectangle<float> (w, 32.0f).withCentre (getLocalBounds().toFloat().getCentre()).translated (0.0f, 8.0f * (1.0f - t));

    juce::Path shape;
    shape.addRoundedRectangle (pill, 16.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f * t), 16, { 0, 6 }).drawForPath (g, shape);
    g.setColour (juce::Colour (0xff2a2b30).withAlpha (t));
    g.fillPath (shape);
    g.setColour ((error ? Colours::led : Colours::frameInk.withAlpha (0.18f)).withMultipliedAlpha (t));
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ pill.getX() + 18.0f, pill.getCentreY() });
    g.setColour ((error ? Colours::led : juce::Colour (0xff7bd88f)).withAlpha (t));
    g.fillEllipse (dot);
    g.setColour (Colours::frameInk.withAlpha (t));
    g.drawText (text, pill.withTrimmedLeft (30.0f).withTrimmedRight (12.0f), juce::Justification::centredLeft, true);
}
}

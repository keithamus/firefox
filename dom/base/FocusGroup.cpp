/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FocusGroup.h"

#include "mozilla/ScrollContainerFrame.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WritingModes.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsFocusManager.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"

namespace mozilla::dom {

namespace {

enum class Token : uint8_t {
  None,
  Toolbar,
  Tablist,
  Radiogroup,
  Listbox,
  Menu,
  Menubar,
  Wrap,
  NoWrap,
  Inline,
  Block,
  NoMemory,
};

Maybe<Token> ParseToken(nsAtom* aAtom) {
  struct Entry {
    nsAtom* mAtom;
    const char* mName;
    Token mToken;
  };
  // nsGkAtoms members aren't constant expressions, so this can't be a static
  // table without introducing a static initializer.
  const Entry entries[] = {
      {nsGkAtoms::none, "none", Token::None},
      {nsGkAtoms::toolbar, "toolbar", Token::Toolbar},
      {nsGkAtoms::tablist, "tablist", Token::Tablist},
      {nsGkAtoms::radiogroup, "radiogroup", Token::Radiogroup},
      {nsGkAtoms::listbox, "listbox", Token::Listbox},
      {nsGkAtoms::menu, "menu", Token::Menu},
      {nsGkAtoms::menubar, "menubar", Token::Menubar},
      {nsGkAtoms::wrap, "wrap", Token::Wrap},
      {nsGkAtoms::nowrap, "nowrap", Token::NoWrap},
      {nsGkAtoms::inlinevalue, "inline", Token::Inline},
      {nsGkAtoms::block, "block", Token::Block},
      {nsGkAtoms::nomemory, "nomemory", Token::NoMemory},
  };
  for (const Entry& entry : entries) {
    if (aAtom == entry.mAtom) {
      return Some(entry.mToken);
    }
  }
  // Token matching is ASCII case-insensitive.
  const nsDependentAtomString value(aAtom);
  for (const Entry& entry : entries) {
    if (value.LowerCaseEqualsASCII(entry.mName)) {
      return Some(entry.mToken);
    }
  }
  return Nothing();
}

FocusGroupBehavior BehaviorForToken(Token aToken) {
  switch (aToken) {
    case Token::Toolbar:
      return FocusGroupBehavior::Toolbar;
    case Token::Tablist:
      return FocusGroupBehavior::Tablist;
    case Token::Radiogroup:
      return FocusGroupBehavior::Radiogroup;
    case Token::Listbox:
      return FocusGroupBehavior::Listbox;
    case Token::Menu:
      return FocusGroupBehavior::Menu;
    case Token::Menubar:
      return FocusGroupBehavior::Menubar;
    default:
      return FocusGroupBehavior::None;
  }
}

/**
 * Elements in the top layer are treated as if they had focusgroup="none" with
 * respect to the focus groups of their ancestors, see
 * https://html.spec.whatwg.org/multipage/interaction.html#focusgroup-top-layer-interaction
 */
bool IsInTopLayer(const Element& aElement) {
  const Document* document = aElement.GetComposedDoc();
  return document && document->TopLayerContains(const_cast<Element&>(aElement));
}

Element* FocusedElement() {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  return focusManager ? focusManager->GetFocusedElement() : nullptr;
}

/** True if aElement is aRoot or one of its shadow-including descendants. */
bool IsShadowIncludingInclusiveDescendantOf(const Element& aElement,
                                            const Element& aRoot) {
  for (const nsIContent* content = &aElement; content;
       content = content->GetFlattenedTreeParent()) {
    if (content == &aRoot) {
      return true;
    }
  }
  return false;
}

/**
 * True if aElement has built-in behavior for the arrow keys of aAxis, which
 * takes precedence over directional navigation. Pass Nothing() to ask about any
 * axis, and for the Home and End keys.
 * https://html.spec.whatwg.org/multipage/interaction.html#focusgroup-key-conflict-elements
 */
bool IsKeyConflictElement(const Element& aElement,
                          const Maybe<FocusGroupAxis>& aAxis) {
  // Which elements have built-in arrow key behavior, and on which axes, is up
  // to the user agent.
  if (aElement.IsAnyOfHTMLElements(nsGkAtoms::textarea, nsGkAtoms::select,
                                   nsGkAtoms::iframe, nsGkAtoms::object,
                                   nsGkAtoms::embed)) {
    return true;
  }

  if (const auto* input = HTMLInputElement::FromNode(&aElement)) {
    switch (input->ControlType()) {
      case FormControlType::InputButton:
      case FormControlType::InputCheckbox:
      case FormControlType::InputFile:
      case FormControlType::InputHidden:
      case FormControlType::InputImage:
      case FormControlType::InputReset:
      case FormControlType::InputSubmit:
        break;
      default:
        // The remaining types use the arrow keys to move the caret, to step a
        // value, or to move within a radio group.
        return true;
    }
  }

  if (aElement.IsAnyOfHTMLElements(nsGkAtoms::audio, nsGkAtoms::video) &&
      aElement.HasAttr(nsGkAtoms::controls)) {
    return true;
  }

  if (aElement.IsEditable()) {
    return true;
  }

  // A focusable scrollable region conflicts on the axes which the user can
  // scroll with the arrow keys.
  nsIFrame* frame = aElement.GetPrimaryFrame();
  ScrollContainerFrame* scrollContainer =
      frame ? frame->GetScrollTargetFrame() : nullptr;
  if (!scrollContainer) {
    return false;
  }
  const layers::ScrollDirections directions =
      scrollContainer->GetAvailableScrollingDirectionsForUserInputEvents();
  if (!aAxis) {
    return !directions.isEmpty();
  }
  const bool horizontal = (*aAxis == FocusGroupAxis::Inline) !=
                          frame->GetWritingMode().IsVertical();
  return directions.contains(horizontal ? layers::ScrollDirection::eHorizontal
                                        : layers::ScrollDirection::eVertical);
}

/**
 * True if aElement, or one of its shadow-including ancestors up to aOwner, has
 * built-in arrow key behavior. While such an element has focus, the items of
 * the focus group are tab stops so that the user can leave it.
 */
bool IsInKeyConflictElement(const Element& aElement, const Element& aOwner) {
  for (const nsIContent* content = &aElement; content && content != &aOwner;
       content = content->GetFlattenedTreeParent()) {
    const Element* element = Element::FromNode(content);
    if (element && IsKeyConflictElement(*element, Nothing())) {
      return true;
    }
  }
  return false;
}

/** True if the subtree rooted at aRoot contains a tab stop. */
bool SubtreeHasTabStop(const nsIContent& aRoot) {
  FlattenedChildIterator iterator(&aRoot);
  while (nsIContent* child = iterator.GetNextChild()) {
    Element* element = Element::FromNode(child);
    if (element && FocusGroup::IsItem(*element)) {
      return true;
    }
    if (SubtreeHasTabStop(*child)) {
      return true;
    }
  }
  return false;
}

/**
 * Collects the focus group items of a focus group scope in order-modified
 * document order, split into focus group segments, see
 * https://html.spec.whatwg.org/multipage/interaction.html#focus-group-segment
 *
 * Because Gecko doesn't reorder sequential focus navigation for the CSS order
 * and reading-flow properties yet, order-modified document order is the
 * shadow-including tree order here.
 */
struct ItemCollector {
  /** The element to locate within the scope, i.e. the focused element. */
  const Element* mProbe = nullptr;
  AutoTArray<Element*, 32> mItems;
  /** The index in mItems of the first item of each segment. */
  AutoTArray<uint32_t, 4> mSegmentStarts;
  /** The number of items which precede mProbe. */
  Maybe<uint32_t> mProbePosition;
  /** The segment which mProbe belongs to, whether it is an item or not. */
  Maybe<uint32_t> mProbeSegment;

  void Collect(const Element& aOwner) { CollectChildren(aOwner); }

  /** The half-open range of item indices of the segment which aIndex is in. */
  std::pair<uint32_t, uint32_t> SegmentOf(uint32_t aIndex) const {
    uint32_t start = 0;
    uint32_t end = mItems.Length();
    for (uint32_t i = 0; i < mSegmentStarts.Length(); ++i) {
      if (mSegmentStarts[i] > aIndex) {
        end = mSegmentStarts[i];
        break;
      }
      start = mSegmentStarts[i];
    }
    return {start, end};
  }

  uint32_t SegmentIndexOf(uint32_t aItemIndex) const {
    uint32_t segment = 0;
    for (uint32_t i = 0; i < mSegmentStarts.Length(); ++i) {
      if (mSegmentStarts[i] > aItemIndex) {
        break;
      }
      segment = i;
    }
    return segment;
  }

 private:
  bool mPendingBoundary = false;

  void AddItem(Element& aElement) {
    if (mItems.IsEmpty() || mPendingBoundary) {
      mSegmentStarts.AppendElement(mItems.Length());
      mPendingBoundary = false;
    }
    mItems.AppendElement(&aElement);
  }

  /** The segment which an element visited right now would belong to. */
  uint32_t CurrentSegment() const {
    if (mSegmentStarts.IsEmpty()) {
      return 0;
    }
    return mPendingBoundary ? mSegmentStarts.Length()
                            : mSegmentStarts.Length() - 1;
  }

  void CollectChildren(const nsIContent& aParent) {
    FlattenedChildIterator iterator(&aParent);
    while (nsIContent* child = iterator.GetNextChild()) {
      Element* element = Element::FromNode(child);
      if (!element) {
        continue;
      }
      if (element == mProbe) {
        mProbePosition = Some(mItems.Length());
        mProbeSegment = Some(CurrentSegment());
      }
      const FocusGroupState state = FocusGroup::GetState(*element);
      const bool isItem = FocusGroup::IsItem(*element);
      const bool probeInSubtree =
          mProbe && IsShadowIncludingInclusiveDescendantOf(*mProbe, *element);
      // When the probe is inside a subtree which this scope doesn't include,
      // the position of that subtree stands in for its own.
      const bool subtreeContainsProbe = probeInSubtree && element != mProbe;
      if (state.mOptOut || IsInTopLayer(*element)) {
        // A subtree which is excluded from this scope separates the segments
        // before and after it, as long as the user can reach it.
        if (isItem || probeInSubtree || SubtreeHasTabStop(*element)) {
          if (subtreeContainsProbe) {
            mProbePosition = Some(mItems.Length());
            mProbeSegment = Some(CurrentSegment());
          }
          mPendingBoundary = true;
        }
        continue;
      }
      if (isItem) {
        AddItem(*element);
      }
      if (state.IsOwner()) {
        // The items of a nested focus group scope aren't part of this scope,
        // and they separate the segments before and after them.
        if (subtreeContainsProbe) {
          mProbePosition = Some(mItems.Length());
          mProbeSegment = Some(CurrentSegment());
        }
        if (SubtreeHasTabStop(*element)) {
          mPendingBoundary = true;
        }
        continue;
      }
      CollectChildren(*element);
    }
  }
};

/**
 * The entry element of the focus group segment which spans [aStart, aEnd), or
 * null if no item of that segment takes part in sequential focus navigation.
 * https://html.spec.whatwg.org/multipage/interaction.html#focusgroup-entry-element
 */
Element* EntryElementOfSegment(const Element& aOwner,
                               const FocusGroupState& aState,
                               const ItemCollector& aCollector, uint32_t aStart,
                               uint32_t aEnd) {
  const nsTArray<Element*>& items = aCollector.mItems;
  const auto findInSegment = [&](const Element* aElement) -> Element* {
    if (!aElement) {
      return nullptr;
    }
    for (uint32_t i = aStart; i < aEnd; ++i) {
      if (items[i] == aElement) {
        return items[i];
      }
    }
    return nullptr;
  };

  // Step 1. The owner of the items of the segment is aOwner.
  Element* focused = FocusedElement();
  MOZ_ASSERT(aCollector.mProbe == focused,
             "The collector must use the focused element as its probe");

  // Step 2. If the currently focused element is a focus group item in the
  // segment, then return it.
  if (Element* item = findInSegment(focused)) {
    return item;
  }

  // The focused element is within this segment without being an item of it, for
  // example because its tabindex value is negative. The segment has already
  // been entered, so sequential focus navigation leaves the focus group instead
  // of moving to its entry element.
  if (focused && aCollector.mProbeSegment &&
      *aCollector.mProbeSegment == aCollector.SegmentIndexOf(aStart) &&
      FocusGroup::GetOwner(*focused) == &aOwner) {
    return nullptr;
  }

  // Step 3. Return the last focused item, if it is still eligible. An item
  // which left the scope of its owner, stopped being focusable or entered the
  // top layer isn't part of the collected items, and is therefore ignored.
  if (!aState.mNoMemory) {
    if (Element* remembered =
            findInSegment(aOwner.GetFocusGroupLastFocusedItem())) {
      return remembered;
    }
  }

  // Step 4. Return the first item with the focusgroupstart attribute.
  for (uint32_t i = aStart; i < aEnd; ++i) {
    if (items[i]->HasAttr(nsGkAtoms::focusgroupstart)) {
      return items[i];
    }
  }

  // Step 5 and step 6. Return the first item of the segment.
  return items[aStart];
}

}  // namespace

// https://html.spec.whatwg.org/multipage/interaction.html#determine-the-focusgroup-state
FocusGroupState FocusGroup::GetState(const Element& aElement) {
  FocusGroupState state;
  if (!StaticPrefs::dom_focusgroup_enabled()) {
    return state;
  }
  // Step 1 and step 2. The attribute is parsed into a token list already.
  const nsAttrValue* attribute = aElement.GetParsedAttr(nsGkAtoms::focusgroup);
  if (!attribute) {
    return state;
  }

  bool wrap = false;
  bool noWrap = false;
  bool inlineAxis = false;
  bool blockAxis = false;
  const int32_t count = attribute->GetAtomCount();
  for (int32_t i = 0; i < count; ++i) {
    const Maybe<Token> token = ParseToken(attribute->AtomAt(i));
    if (!token) {
      continue;
    }
    switch (*token) {
      case Token::None:
        // Step 3. The element opts out of any ancestor focus group, whatever
        // the other tokens are.
        return FocusGroupState{FocusGroupBehavior::None, /* mOptOut = */ true};
      case Token::Wrap:
        wrap = true;
        break;
      case Token::NoWrap:
        noWrap = true;
        break;
      case Token::Inline:
        inlineAxis = true;
        break;
      case Token::Block:
        blockAxis = true;
        break;
      case Token::NoMemory:
        // Step 7. Memory: the last focused item memory is disabled.
        state.mNoMemory = true;
        break;
      default:
        // Step 4. Only the first behavior token has an effect.
        if (!state.IsOwner()) {
          state.mBehavior = BehaviorForToken(*token);
        }
        break;
    }
  }

  // Step 5. Without a behavior token the attribute has no effect.
  if (!state.IsOwner()) {
    return FocusGroupState{};
  }

  // Step 6 and step 7. The default modifiers of the behavior apply where the
  // author didn't specify a modifier.
  bool defaultWrap = false;
  bool defaultInlineAxis = false;
  bool defaultBlockAxis = false;
  switch (state.mBehavior) {
    case FocusGroupBehavior::Toolbar:
      defaultInlineAxis = true;
      break;
    case FocusGroupBehavior::Tablist:
    case FocusGroupBehavior::Menubar:
      defaultInlineAxis = true;
      defaultWrap = true;
      break;
    case FocusGroupBehavior::Radiogroup:
      defaultInlineAxis = true;
      defaultBlockAxis = true;
      defaultWrap = true;
      break;
    case FocusGroupBehavior::Listbox:
      defaultBlockAxis = true;
      break;
    case FocusGroupBehavior::Menu:
      defaultBlockAxis = true;
      defaultWrap = true;
      break;
    case FocusGroupBehavior::None:
      MOZ_ASSERT_UNREACHABLE("Not a focus group owner");
      break;
  }

  // Wrap: a modifier which contradicts itself falls back to the default.
  state.mWrap = wrap == noWrap ? defaultWrap : wrap;
  // Axis: both modifiers allow both axes, one of them restricts navigation to
  // that axis, and none of them falls back to the default.
  if (inlineAxis || blockAxis) {
    state.mInlineAxis = inlineAxis;
    state.mBlockAxis = blockAxis;
  } else {
    state.mInlineAxis = defaultInlineAxis;
    state.mBlockAxis = defaultBlockAxis;
  }

  // Step 8.
  return state;
}

// https://html.spec.whatwg.org/multipage/interaction.html#focus-group-owner-of
Element* FocusGroup::GetOwner(const Element& aElement,
                              FocusGroupState* aState) {
  if (!StaticPrefs::dom_focusgroup_enabled()) {
    return nullptr;
  }
  // An element which opts out, or which is in the top layer, isn't part of the
  // focus group scope of any of its ancestors.
  if (GetState(aElement).mOptOut || IsInTopLayer(aElement)) {
    return nullptr;
  }
  // Return the nearest shadow-including ancestor which is a focus group owner,
  // as long as no element in between leaves its scope.
  for (nsIContent* ancestor = aElement.GetFlattenedTreeParent(); ancestor;
       ancestor = ancestor->GetFlattenedTreeParent()) {
    Element* element = Element::FromNode(ancestor);
    if (!element) {
      continue;
    }
    const FocusGroupState state = GetState(*element);
    if (state.IsOwner()) {
      if (aState) {
        *aState = state;
      }
      return element;
    }
    if (state.mOptOut || IsInTopLayer(*element)) {
      return nullptr;
    }
  }
  return nullptr;
}

// https://html.spec.whatwg.org/multipage/interaction.html#focus-group-item
bool FocusGroup::IsItem(const Element& aElement) {
  nsIFrame* frame = aElement.GetPrimaryFrame();
  if (!frame) {
    return false;
  }
  const auto focusable = frame->IsFocusable();
  return focusable && focusable.mTabIndex >= 0;
}

// https://html.spec.whatwg.org/multipage/interaction.html#focusgroup-event-handling
// https://html.spec.whatwg.org/multipage/interaction.html#focusgroup-directional-navigation
bool FocusGroup::HandleKeyDown(Element& aFocusedElement,
                               const WidgetKeyboardEvent& aKeyEvent) {
  if (!StaticPrefs::dom_focusgroup_enabled()) {
    return false;
  }
  // While a modifier key is held, key combinations keep their
  // platform-specific behavior.
  if (aKeyEvent.IsShift() || aKeyEvent.IsControl() || aKeyEvent.IsAlt() ||
      aKeyEvent.IsMeta()) {
    return false;
  }

  Maybe<Side> physicalSide;
  bool toLastItem = false;
  switch (aKeyEvent.mKeyCode) {
    case NS_VK_LEFT:
      physicalSide.emplace(eSideLeft);
      break;
    case NS_VK_RIGHT:
      physicalSide.emplace(eSideRight);
      break;
    case NS_VK_UP:
      physicalSide.emplace(eSideTop);
      break;
    case NS_VK_DOWN:
      physicalSide.emplace(eSideBottom);
      break;
    case NS_VK_HOME:
      break;
    case NS_VK_END:
      toLastItem = true;
      break;
    default:
      return false;
  }

  if (Document* document = aFocusedElement.GetComposedDoc()) {
    document->FlushPendingNotifications(FlushType::Frames);
  }
  if (!aFocusedElement.IsInComposedDoc()) {
    return false;
  }

  // Step 1 and step 2. The focused element and its focus group owner.
  FocusGroupState state;
  RefPtr<Element> owner = GetOwner(aFocusedElement, &state);
  if (!owner) {
    return false;
  }

  Maybe<FocusGroupAxis> axis;
  bool forward = false;
  if (physicalSide) {
    // Step 3 and step 6. The direction of the arrow key in the writing mode and
    // the directionality of the focused element.
    nsIFrame* frame = aFocusedElement.GetPrimaryFrame();
    const WritingMode writingMode =
        frame ? frame->GetWritingMode() : WritingMode();
    const LogicalSide side =
        writingMode.LogicalSideForPhysicalSide(*physicalSide);
    axis.emplace(IsInline(side) ? FocusGroupAxis::Inline
                                : FocusGroupAxis::Block);
    forward = side == LogicalSide::IEnd || side == LogicalSide::BEnd;

    // Step 5.
    if (!state.SupportsAxis(*axis)) {
      return false;
    }
  }

  // Step 4. The built-in behavior of the focused element takes precedence.
  if (IsKeyConflictElement(aFocusedElement, axis)) {
    return false;
  }

  // Step 7. The items of the scope, in order-modified document order.
  ItemCollector collector;
  collector.mProbe = &aFocusedElement;
  collector.Collect(*owner);
  if (collector.mItems.IsEmpty() || !collector.mProbePosition) {
    return false;
  }

  const int32_t count = int32_t(collector.mItems.Length());
  const uint32_t position = *collector.mProbePosition;
  int32_t index;
  if (!physicalSide) {
    // The Home and End keys move to the first and the last item of the scope.
    index = toLastItem ? count - 1 : 0;
  } else {
    // Step 8. The item which follows or precedes the focused element.
    const bool focusedIsItem = position < collector.mItems.Length() &&
                               collector.mItems[position] == &aFocusedElement;
    index = forward ? int32_t(position) + (focusedIsItem ? 1 : 0)
                    : int32_t(position) - 1;
    if (index >= count || index < 0) {
      // Step 9. Without the wrap modifier the focus stays where it is, and the
      // event isn't consumed, so that scrolling can happen instead.
      if (!state.mWrap) {
        return false;
      }
      index = index < 0 ? count - 1 : 0;
    }
  }

  RefPtr<Element> next = collector.mItems[index];
  if (next == &aFocusedElement) {
    return false;
  }
  RefPtr<nsFocusManager> focusManager = nsFocusManager::GetFocusManager();
  if (!focusManager) {
    return false;
  }
  // Step 10.
  focusManager->SetFocus(next, nsIFocusManager::FLAG_BYKEY);
  return true;
}

// https://html.spec.whatwg.org/multipage/interaction.html#excluded-from-sequential-focus-navigation
bool FocusGroup::IsExcludedFromSequentialFocusNavigation(
    const nsIContent& aContent) {
  if (!StaticPrefs::dom_focusgroup_enabled()) {
    return false;
  }
  const Element* element = Element::FromNode(&aContent);
  if (!element || !IsItem(*element)) {
    return false;
  }
  FocusGroupState state;
  Element* owner = GetOwner(*element, &state);
  if (!owner) {
    return false;
  }

  // While an element with built-in arrow key behavior has focus, every item of
  // its focus group scope takes part in sequential focus navigation, so that
  // the user can leave that element.
  Element* focused = FocusedElement();
  if (focused && focused != element && GetOwner(*focused) == owner &&
      IsInKeyConflictElement(*focused, *owner)) {
    return false;
  }

  ItemCollector collector;
  collector.mProbe = focused;
  collector.Collect(*owner);
  const size_t index = collector.mItems.IndexOf(const_cast<Element*>(element));
  if (index == decltype(collector.mItems)::NoIndex) {
    return false;
  }
  const auto [start, end] = collector.SegmentOf(uint32_t(index));
  // Only the entry element of a focus group segment takes part in sequential
  // focus navigation.
  return EntryElementOfSegment(*owner, state, collector, start, end) != element;
}

// The addition to the focusing steps of
// https://html.spec.whatwg.org/multipage/interaction.html#focusing-steps
void FocusGroup::RememberFocusedItem(Element& aElement) {
  if (!StaticPrefs::dom_focusgroup_enabled()) {
    return;
  }
  FocusGroupState state;
  Element* owner = GetOwner(aElement, &state);
  if (!owner || state.mNoMemory || !IsItem(aElement)) {
    return;
  }
  owner->SetFocusGroupLastFocusedItem(aElement);
}

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FocusGroup_h
#define mozilla_dom_FocusGroup_h

#include <cstdint>

#include "mozilla/Attributes.h"

class nsIContent;

namespace mozilla {
class WidgetKeyboardEvent;

namespace dom {

class Element;

/**
 * The behavior token of a focusgroup attribute. It selects the interaction
 * pattern of the focus group. None means that the element is not a focus group
 * owner.
 */
enum class FocusGroupBehavior : uint8_t {
  None,
  Toolbar,
  Tablist,
  Radiogroup,
  Listbox,
  Menu,
  Menubar,
};

/** The axis along which directional navigation moves focus. */
enum class FocusGroupAxis : uint8_t { Inline, Block };

/**
 * The effective state of the focusgroup attribute of an element, see
 * https://html.spec.whatwg.org/multipage/interaction.html#determine-the-focusgroup-state
 */
struct FocusGroupState {
  FocusGroupBehavior mBehavior = FocusGroupBehavior::None;
  /** True if the element opts itself and its descendants out of the focus
   * groups of its ancestors, i.e. the "none" token is present. */
  bool mOptOut = false;
  bool mWrap = false;
  bool mInlineAxis = false;
  bool mBlockAxis = false;
  bool mNoMemory = false;

  bool IsOwner() const { return mBehavior != FocusGroupBehavior::None; }

  bool SupportsAxis(FocusGroupAxis aAxis) const {
    return aAxis == FocusGroupAxis::Inline ? mInlineAxis : mBlockAxis;
  }
};

/**
 * Declarative focus navigation with the focusgroup attribute, see
 * https://html.spec.whatwg.org/multipage/interaction.html#the-focusgroup-attribute
 */
class FocusGroup final {
 public:
  /** Parses the focusgroup attribute of aElement. */
  static FocusGroupState GetState(const Element& aElement);

  /**
   * Returns the nearest shadow-including ancestor of aElement which is a focus
   * group owner and whose focus group scope contains aElement, or null.
   * If aState is given, it receives the state of that owner.
   */
  static Element* GetOwner(const Element& aElement,
                           FocusGroupState* aState = nullptr);

  /** True if aElement is a focusable area with a non-negative tabindex. */
  static bool IsItem(const Element& aElement);

  /**
   * Runs the default action of a keydown event, i.e. directional navigation for
   * an arrow key and, for a focus group item, movement to the first or the last
   * item for the Home and End keys. Returns true if focus moved, in which case
   * the caller must consume the event.
   */
  MOZ_CAN_RUN_SCRIPT static bool HandleKeyDown(
      Element& aFocusedElement, const WidgetKeyboardEvent& aKeyEvent);

  /**
   * True if aContent is a focus group item which sequential focus navigation
   * must skip, because another item of its focus group segment is the entry
   * element of that segment.
   */
  static bool IsExcludedFromSequentialFocusNavigation(
      const nsIContent& aContent);

  /**
   * Remembers aElement as the last focused item of its focus group owner, to
   * make it the entry element of its segment. Called when an element is
   * focused.
   */
  static void RememberFocusedItem(Element& aElement);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FocusGroup_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLSelectedContentElement.h"

#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLSelectedContentElementBinding.h"
#include "mozilla/dom/UnbindContext.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(SelectedContent)

namespace mozilla::dom {

HTMLSelectedContentElement::HTMLSelectedContentElement(
    already_AddRefed<class NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLSelectedContentElement::~HTMLSelectedContentElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLSelectedContentElement)

JSObject* HTMLSelectedContentElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return HTMLSelectedContentElement_Binding::Wrap(aCx, this, aGivenProto);
}

// https://html.spec.whatwg.org/#clear-a-selectedcontent
void HTMLSelectedContentElement::ClearContent() {
  // Step 1: Replace all with null within selectedcontent
  // https://dom.spec.whatwg.org/#concept-node-replace-all
  ReplaceChildren(nullptr, IgnoreErrors());
}

// https://html.spec.whatwg.org/#the-selectedcontent-element
nsresult HTMLSelectedContentElement::BindToTree(BindContext& aContext,
                                                nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "HTMLSelectedContentElement::PostConnectionSteps",
      [self = RefPtr{this}, parent = nsCOMPtr{&aParent}]()
          MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA {
            self->PostConnectionSteps(parent);
          }));

  return NS_OK;
}

void HTMLSelectedContentElement::PostConnectionSteps(nsINode* aParent) {
  if (!StaticPrefs::dom_select_customizable_select_enabled()) {
    return;
  }
  // The selectedcontent HTML element post-connection steps, given
  // selectedcontent, are:

  // 1. Let nearestSelectAncestor be null.
  RefPtr<HTMLSelectElement> nearestSelectAncestor = nullptr;

  // 3. Set selectedcontent's disabled state to false.
  SetDisabled(false);

  // 2. Let ancestor be selectedcontent's parent.
  // 4. For each ancestor of selectedcontent's ancestors, in reverse tree order:
  for (nsINode* ancestor = aParent; ancestor;
       ancestor = ancestor->GetParent()) {
    // 4.1 If ancestor is a select element:
    if (auto* select = HTMLSelectElement::FromNode(ancestor)) {
      // 4.1.1 If nearestSelectAncestor is null, then set nearestSelectAncestor
      //       to select.
      if (!nearestSelectAncestor) {
        nearestSelectAncestor = select;
      } else {
        // 4.1.2 Otherwise, set selectedcontent's disabled state to true.
        SetDisabled(true);
      }
    }

    // 4.2 If ancestor is an option element or a selectedcontent element, then
    //     set selectedcontent's disabled state to true.
    if (HTMLOptionElement::FromNode(ancestor) ||
        HTMLSelectedContentElement::FromNode(ancestor)) {
      SetDisabled(true);
    }
  }

  // 5. If nearestSelectAncestor is null or nearestSelectAncestor has the
  //    multiple attribute, then return.
  if (!nearestSelectAncestor || nearestSelectAncestor->Multiple()) {
    return;
  }

  // 6. Run update a select's selectedcontent given nearestSelectAncestor.
  nearestSelectAncestor->UpdateSelectedContent();

  // 7. Run clear a select's non-primary selectedcontent elements given
  //    nearestSelectAncestor.
  nearestSelectAncestor->ClearNonPrimarySelectedContents();
}

void HTMLSelectedContentElement::ElementRemovingSteps(nsINode* aOldParent) {
  if (!StaticPrefs::dom_select_customizable_select_enabled()) {
    return;
  }
  // The selectedcontent HTML element removing steps, given selectedcontent and
  // oldParent, are:
  // 1. For each ancestor of selectedcontent's ancestors, in reverse tree order:
  for (nsIContent* ancestor = GetParent(); ancestor;
       ancestor = ancestor->GetParent()) {
    if (HTMLSelectElement::FromNode(ancestor)) {
      // 1.1 If ancestor is a select element, then return.
      return;
    }
  }
  // 2. For each ancestor of oldParent's inclusive ancestors, in reverse tree
  //    order:
  for (nsINode* ancestor = aOldParent; ancestor;
       ancestor = ancestor->GetParent()) {
    // 2.1 If ancestor is a select element, then run update a select's
    //     selectedcontent given ancestor and return.
    if (RefPtr select = HTMLSelectElement::FromNode(ancestor)) {
      select->UpdateSelectedContent();
      return;
    }
  }
}

// https://html.spec.whatwg.org/#the-selectedcontent-element
void HTMLSelectedContentElement::UnbindFromTree(UnbindContext& aContext) {
  nsGenericHTMLElement::UnbindFromTree(aContext);
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "HTMLSelectedContentElement::ElementRemovingSteps",
      [self = RefPtr{this},
       oldParent = nsCOMPtr{aContext.GetOriginalSubtreeParent()}]()
          MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA {
            self->ElementRemovingSteps(oldParent);
          }));
}

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSelectedContentElement_h
#define mozilla_dom_HTMLSelectedContentElement_h

#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLSelectedContentElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLSelectedContentElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLSelectedContentElement, selectedcontent)

  nsresult Clone(class NodeInfo* aNodeInfo, nsINode** aResult) const override;

  // https://html.spec.whatwg.org/#selectedcontent-disabled
  void SetDisabled(bool aDisabled) { mDisabled = aDisabled; }
  bool IsDisabled() const { return mDisabled; }

  MOZ_CAN_RUN_SCRIPT void ClearContent();

  // Spec: selectedcontent post-connection steps
  // https://html.spec.whatwg.org/#the-selectedcontent-element
  nsresult BindToTree(BindContext& aContext, nsINode& aParent) override;

  // Spec: selectedcontent removing steps
  // https://html.spec.whatwg.org/#the-selectedcontent-element
  void UnbindFromTree(UnbindContext& aContext) override;

 protected:
  virtual ~HTMLSelectedContentElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 private:
  MOZ_CAN_RUN_SCRIPT void PostConnectionSteps(nsINode* aParent);
  MOZ_CAN_RUN_SCRIPT void ElementRemovingSteps(nsINode* aOldParent);

  // https://html.spec.whatwg.org/#selectedcontent-disabled
  bool mDisabled = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLSelectedContentElement_h

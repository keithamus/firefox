/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLSelectedContentElement.h"

#include "mozilla/dom/HTMLSelectedContentElementBinding.h"
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

}  // namespace mozilla::dom

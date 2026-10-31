/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PAGE = `
<!doctype html>
<fieldset>
  <input type="color" value="#000000">
  <input type="date" value="2026-10-31">
</fieldset>
`;

async function openPicker(browser, type, panelId) {
  let shown = BrowserTestUtils.waitForEvent(
    document.getElementById("mainPopupSet"),
    "popupshown",
    false,
    event => event.target.id == panelId
  );
  await SpecialPowers.spawn(browser, [type], async inputType => {
    await SpecialPowers.contentTransformsReceived(content);
    const input = content.document.querySelector(`input[type=${inputType}]`);
    input.focus();
    content.document.notifyUserGestureActivation();
    input.showPicker();
  });
  return (await shown).target;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.forms.html_color_picker.enabled", true],
      ["dom.forms.retain_focus_on_disabled.enabled", true],
    ],
  });
});

registerCleanupFunction(() => {
  for (let panelId of ["DateTimePickerPanel", "ColorPickerPanel"]) {
    document.getElementById(`${panelId}PopupFrame`)?.remove();
  }
});

for (const disabler of ["input", "fieldset"]) {
  add_task(async function test_disable_dismisses_color_picker() {
    info(`Disabling via ${disabler}`);
    await BrowserTestUtils.withNewTab(
      "data:text/html," + encodeURIComponent(PAGE),
      async browser => {
        const panel = await openPicker(browser, "color", "ColorPickerPanel");
        await SpecialPowers.spawn(browser, [], () => {
          const input = content.document.querySelector("input[type=color]");
          content.wrappedJSObject.events = [];
          for (const type of ["input", "change"]) {
            input.addEventListener(type, () =>
              content.wrappedJSObject.events.push(type)
            );
          }
        });
        panel.dispatchEvent(
          new CustomEvent("InputPickerValueChanged", {
            detail: { rgb: [1, 0, 0, 1] },
          })
        );
        await SpecialPowers.spawn(browser, [], () =>
          ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector("input[type=color]").value ==
              "#ff0000",
            "Picker selection is previewed"
          )
        );

        const hidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
        const colorDisabler =
          disabler == "input" ? "input[type=color]" : disabler;
        await SpecialPowers.spawn(browser, [colorDisabler], selector => {
          const input = content.document.querySelector("input[type=color]");
          is(content.document.activeElement, input, "Input is focused");
          content.document.querySelector(selector).disabled = true;
        });
        await hidden;

        await SpecialPowers.spawn(browser, [], () => {
          const input = content.document.querySelector("input[type=color]");
          is(content.document.activeElement, input, "Input retains focus");
          ok(!input.matches(":open"), "Input is no longer open");
          Assert.deepEqual(
            content.wrappedJSObject.events,
            ["input"],
            "Dismissing does not commit the selection"
          );
        });
      }
    );
  });

  add_task(async function test_disable_dismisses_date_picker() {
    info(`Disabling via ${disabler}`);
    await BrowserTestUtils.withNewTab(
      "data:text/html," + encodeURIComponent(PAGE),
      async browser => {
        const panel = await openPicker(browser, "date", "DateTimePickerPanel");
        const hidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
        const dateDisabler = disabler == "input" ? "input[type=date]" : disabler;
        await SpecialPowers.spawn(browser, [dateDisabler], selector => {
          content.document.querySelector(selector).disabled = true;
        });
        await hidden;

        await SpecialPowers.spawn(browser, [], () => {
          const input = content.document.querySelector("input[type=date]");
          ok(!input.matches(":open"), "Input is no longer open");
          is(input.value, "2026-10-31", "Value is unchanged");
        });
      }
    );
  });
}

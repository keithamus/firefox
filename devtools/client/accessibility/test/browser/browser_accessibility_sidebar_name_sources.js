/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the sidebar lists the sources of the computed name and
// description, and shows which source each one came from.

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Sidebar Name Sources Test</title>
  </head>
  <body>
    <button title="Search box" aria-describedby="hint"></button>
    <p id="hint">Type to search</p>
  </body>
</html>`;

addA11YPanelTask(
  "Check name and description sources in side panel",
  TEST_URI,
  async ({ doc, browser }) => {
    await toggleRow(doc, 0);
    selectRow(doc, 1);

    await TestUtils.waitForCondition(
      () => getPropertyValue(doc, "name") === `"Search box"`,
      "Wait until the sidebar is updated"
    );

    is(
      getPropertyValue(doc, "nameSources"),
      `"title"`,
      "The name came from the title attribute"
    );
    is(
      getPropertyValue(doc, "descriptionSources"),
      `"aria-describedby"`,
      "The description came from aria-describedby"
    );

    await expandProperty(doc, "nameSources");
    const nameSources = [
      ["aria-labelledby", "null"],
      ["aria-label", "null"],
      ["native", "null"],
      ["contents", "null"],
      ["title", `"Search box"`],
      ["content-alt", "null"],
    ];
    for (const [source, value] of nameSources) {
      is(
        getSourceValue(doc, "nameSources", source),
        value,
        `${source} name source has correct value`
      );
    }

    await expandProperty(doc, "descriptionSources");
    const descriptionSources = [
      ["aria-describedby", `"Type to search"`],
      ["aria-description", "null"],
      ["native", "null"],
      ["title", `"Search box"`],
    ];
    for (const [source, value] of descriptionSources) {
      is(
        getSourceValue(doc, "descriptionSources", source),
        value,
        `${source} description source has correct value`
      );
    }

    info("Name the button with aria-labelledby");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .querySelector("button")
        .setAttribute("aria-labelledby", "hint");
    });
    await TestUtils.waitForCondition(
      () => getPropertyValue(doc, "nameSources") === `"aria-labelledby"`,
      "Wait until the name comes from aria-labelledby"
    );

    // aria-labelledby takes precedence, so this fires no accessible event.
    info("Change aria-label, which the name doesn't come from");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.querySelector("button").setAttribute("aria-label", "Go");
    });
    await TestUtils.waitForCondition(
      () => getSourceValue(doc, "nameSources", "aria-label") === `"Go"`,
      "Wait until aria-label name source is updated"
    );
    is(
      getPropertyValue(doc, "nameSources"),
      `"aria-labelledby"`,
      "The name still comes from aria-labelledby"
    );
  }
);

async function expandProperty(doc, property) {
  const twisty = doc
    .getElementById(`/${property}`)
    .querySelector(".theme-twisty");
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  EventUtils.sendMouseEvent({ type: "click" }, twisty, doc.defaultView);
  AccessibilityUtils.resetEnv();
  await TestUtils.waitForCondition(
    () => twisty.classList.contains("open"),
    `${property} expanded`
  );
}

function getSourceValue(doc, property, source) {
  return doc
    .getElementById(`/${property}/${source}`)
    ?.querySelector(".object-value")?.textContent;
}

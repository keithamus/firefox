const PAGE = `
<!doctype html>
<fieldset>
  <select>
    <option>First</option>
    <option>Second</option>
  </select>
</fieldset>
`;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.retain_focus_on_disabled.enabled", true]],
  });
});

for (const disabler of ["select", "fieldset"]) {
  add_task(async function test_disable_closes_dropdown() {
    info(`Disabling via ${disabler}`);
    const url = "data:text/html," + encodeURI(PAGE);
    await BrowserTestUtils.withNewTab({ gBrowser, url }, async browser => {
      const popup = await openSelectPopup("click");
      const popupHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
      await SpecialPowers.spawn(browser, [disabler], selector => {
        content.document.querySelector(selector).disabled = true;
      });
      await popupHidden;
      await SpecialPowers.spawn(browser, [], async () => {
        const { SelectContentHelper } = ChromeUtils.importESModule(
          "moz-src:///toolkit/actors/SelectChild.sys.mjs"
        );
        await ContentTaskUtils.waitForCondition(
          () => !SelectContentHelper.open
        );
        const select = content.document.querySelector("select");
        is(content.document.activeElement, select, "Select retains focus");
        ok(select.matches(":disabled"), "Select is disabled");
        is(select.selectedIndex, 0, "Selection is unchanged");
      });
    });
  });
}

// SailingDelegate.mc
// Handles user input on the sailing display.
//
// - UP / DOWN (or swipe) cycles through display pages.
// - SELECT (middle button) is a no-op (reserved for future use).
// - BACK / ESC exits the app.

import Toybox.Lang;
import Toybox.WatchUi;

class SailingDelegate extends WatchUi.BehaviorDelegate {

    private var _model as SailingModel;

    function initialize(model as SailingModel) {
        BehaviorDelegate.initialize();
        _model = model;
    }

    // Cycle to the next page on key-up / swipe-up.
    function onNextPage() as Lang.Boolean {
        var view = WatchUi.getCurrentView()[0];
        if (view instanceof SailingView) {
            (view as SailingView).nextPage();
            WatchUi.requestUpdate();
        }
        return true;
    }

    // Cycle to the previous page on key-down / swipe-down.
    function onPreviousPage() as Lang.Boolean {
        var view = WatchUi.getCurrentView()[0];
        if (view instanceof SailingView) {
            (view as SailingView).prevPage();
            WatchUi.requestUpdate();
        }
        return true;
    }

    // Back / ESC exits the app.
    function onBack() as Lang.Boolean {
        WatchUi.popView(WatchUi.SLIDE_DOWN);
        return true;
    }
}

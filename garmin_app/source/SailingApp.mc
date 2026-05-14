// SailingApp.mc
// Entry point for the Sailing ConnectIQ watch application.
//
// Data flow:
//   OpenCPN plugin  →  UDP JSON broadcast
//     → phone companion app
//       → BLE (Communications.transmit / phone app message)
//         → this watch app
//
// The watch app registers for phone-pushed messages and updates the model
// which the view then renders.

import Toybox.Application;
import Toybox.Communications;
import Toybox.Lang;
import Toybox.WatchUi;

class SailingApp extends Application.AppBase {

    private var _model as SailingModel;

    function initialize() {
        AppBase.initialize();
        _model = new SailingModel();
    }

    // Called when the app starts.
    function onStart(state as Lang.Dictionary?) as Void {
        // Register to receive messages pushed from the companion phone app.
        if (Communications has :registerForPhoneAppMessages) {
            Communications.registerForPhoneAppMessages(method(:onPhoneMessage));
        }
    }

    function onStop(state as Lang.Dictionary?) as Void {
    }

    // Return the initial view and its input delegate.
    function getInitialView() as [WatchUi.Views] or [WatchUi.Views, WatchUi.InputDelegates] {
        return [new SailingView(_model), new SailingDelegate(_model)];
    }

    //! Handle a message pushed from the phone companion app.
    //! The message data must be a Dictionary with keys:
    //!   "sog", "cog", "depth", "aws", "awa", "tws", "twd"
    function onPhoneMessage(msg as Communications.Message) as Void {
        var data = msg.data;
        if (data instanceof Lang.Dictionary) {
            _model.updateData(data);
            WatchUi.requestUpdate();
        }
    }

    //! Allow other classes to access the shared model via Application.getApp().
    function getModel() as SailingModel {
        return _model;
    }
}

//! Convenience accessor used by other classes.
function getApp() as SailingApp {
    return Application.getApp() as SailingApp;
}

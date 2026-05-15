// SailingModel.mc
// Holds the latest sailing data received from the OpenCPN companion app.

import Toybox.Lang;
import Toybox.System;

//! Data model for the sailing display.
//! All values use SI / nautical conventions:
//!   depth     – metres
//!   sog       – knots
//!   cog       – degrees true  (0–359)
//!   aws / tws – knots
//!   awa       – degrees relative (-180 … +180, negative = port)
//!   twd       – degrees true  (0–359)
class SailingModel {

    // Speed over ground (knots).  null = no data.
    var sog as Lang.Float or Null;

    // Course over ground, degrees true.  null = no data.
    var cog as Lang.Float or Null;

    // Water depth in metres.  null = no data.
    var depth as Lang.Float or Null;

    // Apparent wind speed (knots).  null = no data.
    var aws as Lang.Float or Null;

    // Apparent wind angle, degrees (negative = port side).
    var awa as Lang.Float or Null;

    // True wind speed (knots).  null = no data.
    var tws as Lang.Float or Null;

    // True wind direction, degrees true.  null = no data.
    var twd as Lang.Float or Null;

    // Timestamp of last successful update (ms since epoch).
    var lastUpdate as Lang.Number or Null;

    function initialize() {
        sog        = null;
        cog        = null;
        depth      = null;
        aws        = null;
        awa        = null;
        tws        = null;
        twd        = null;
        lastUpdate = null;
    }

    //! Update the model from a Dictionary received via phone message.
    //! Keys mirror the JSON keys produced by the OpenCPN plugin:
    //!   "sog", "cog", "depth", "aws", "awa", "tws", "twd"
    function updateData(data as Lang.Dictionary) as Void {
        if (data.hasKey("sog"))   { sog   = (data["sog"]   as Lang.Float); }
        if (data.hasKey("cog"))   { cog   = (data["cog"]   as Lang.Float); }
        if (data.hasKey("depth")) { depth = (data["depth"] as Lang.Float); }
        if (data.hasKey("aws"))   { aws   = (data["aws"]   as Lang.Float); }
        if (data.hasKey("awa"))   { awa   = (data["awa"]   as Lang.Float); }
        if (data.hasKey("tws"))   { tws   = (data["tws"]   as Lang.Float); }
        if (data.hasKey("twd"))   { twd   = (data["twd"]   as Lang.Float); }

        lastUpdate = System.getTimer();
    }

    //! Returns true if data is considered stale (older than 30 s).
    function isStale() as Lang.Boolean {
        if (lastUpdate == null) { return true; }
        return (System.getTimer() - lastUpdate) > 30000;
    }
}

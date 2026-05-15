// SailingView.mc
// Renders sailing data on the watch face.
//
// The display is split into two pages that the user can cycle through:
//
//  Page 0 – Speed & Course
//  ┌───────────────────────┐
//  │        SAILING        │  ← title
//  │                       │
//  │   SOG          COG    │
//  │  12.3 kt      245 °   │  ← large values
//  │                       │
//  │ ◄ swipe for wind/depth│  ← hint
//  └───────────────────────┘
//
//  Page 1 – Depth & Wind
//  ┌───────────────────────┐
//  │        SAILING        │
//  │                       │
//  │   DEPTH      AWS/AWA  │
//  │  15.2 m    10 kt/135° │
//  │            (TWS/TWD)  │
//  │ ◄ swipe for SOG/COG   │
//  └───────────────────────┘
//
// All drawing is done programmatically so the layout adapts to any
// screen shape / resolution automatically.

import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.WatchUi;

class SailingView extends WatchUi.View {

    // Number of display pages.
    private static const PAGE_COUNT as Lang.Number = 2;

    private var _model      as SailingModel;
    private var _page       as Lang.Number = 0;  // 0 = SOG/COG, 1 = Depth/Wind

    // Palette – change here to restyle the whole app.
    private static const COLOR_BG       as Lang.Number = Graphics.COLOR_BLACK;
    private static const COLOR_TITLE    as Lang.Number = Graphics.COLOR_BLUE;
    private static const COLOR_LABEL    as Lang.Number = Graphics.COLOR_LT_GRAY;
    private static const COLOR_VALUE    as Lang.Number = Graphics.COLOR_WHITE;
    private static const COLOR_STALE    as Lang.Number = Graphics.COLOR_DK_GRAY;
    private static const COLOR_HINT     as Lang.Number = Graphics.COLOR_DK_GRAY;

    function initialize(model as SailingModel) {
        View.initialize();
        _model = model;
    }

    function onLayout(dc as Graphics.Dc) as Void {
        // We draw everything manually in onUpdate; nothing to inflate here.
    }

    function onUpdate(dc as Graphics.Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();

        // --- Background ---
        dc.setColor(COLOR_BG, COLOR_BG);
        dc.clear();

        // --- Title bar ---
        dc.setColor(COLOR_TITLE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2, 8, Graphics.FONT_TINY, "SAILING", Graphics.TEXT_JUSTIFY_CENTER);

        // --- Stale indicator ---
        var stale = _model.isStale();
        var valueColor = stale ? COLOR_STALE : COLOR_VALUE;

        if (_page == 0) {
            _drawSogCog(dc, w, h, valueColor);
        } else {
            _drawDepthWind(dc, w, h, valueColor);
        }

        // --- Page hint at the bottom ---
        dc.setColor(COLOR_HINT, Graphics.COLOR_TRANSPARENT);
        var hintY = h - 14;
        var hint = _page == 0 ? "v depth/wind" : "^ sog/cog";
        dc.drawText(w / 2, hintY, Graphics.FONT_TINY, hint, Graphics.TEXT_JUSTIFY_CENTER);

        // --- Page dots ---
        _drawPageDots(dc, w, h);
    }

    // -------------------------------------------------------------------------
    // Page 0 – SOG and COG
    // -------------------------------------------------------------------------
    private function _drawSogCog(dc as Graphics.Dc, w as Lang.Number, h as Lang.Number, valueColor as Lang.Number) as Void {
        var midY = h / 2;

        // SOG – left column
        _drawField(dc,
            w / 4, midY - 20,
            "SOG",
            _formatKnots(_model.sog),
            valueColor);

        // COG – right column
        _drawField(dc,
            3 * w / 4, midY - 20,
            "COG",
            _formatDegrees(_model.cog),
            valueColor);
    }

    // -------------------------------------------------------------------------
    // Page 1 – Depth and Wind
    // -------------------------------------------------------------------------
    private function _drawDepthWind(dc as Graphics.Dc, w as Lang.Number, h as Lang.Number, valueColor as Lang.Number) as Void {
        var midY = h / 2;

        // Depth – left column
        _drawField(dc,
            w / 4, midY - 20,
            "DEPTH",
            _formatMeters(_model.depth),
            valueColor);

        // Wind – right column (two value rows: speed on top, angle/dir below)
        var windLabel as Lang.String;
        var windSpeed as Lang.String;
        var windDir   as Lang.String;

        if (_model.aws != null) {
            windLabel = "AWS/AWA";
            windSpeed = _formatKnots(_model.aws);
            windDir   = _model.awa != null ? _formatRelAngle(_model.awa) : "--";
        } else if (_model.tws != null) {
            windLabel = "TWS/TWD";
            windSpeed = _formatKnots(_model.tws);
            windDir   = _formatDegrees(_model.twd);
        } else {
            windLabel = "WIND";
            windSpeed = "--";
            windDir   = "";
        }

        var cx = 3 * w / 4;
        dc.setColor(valueColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, midY - 20, Graphics.FONT_MEDIUM, windSpeed, Graphics.TEXT_JUSTIFY_CENTER);
        if (!windDir.equals("")) {
            dc.drawText(cx, midY, Graphics.FONT_SMALL, windDir, Graphics.TEXT_JUSTIFY_CENTER);
        }
        dc.setColor(COLOR_LABEL, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, midY + 22, Graphics.FONT_TINY, windLabel, Graphics.TEXT_JUSTIFY_CENTER);
    }

    // -------------------------------------------------------------------------
    // Helper: draw a labelled field centred on (cx, cy)
    // -------------------------------------------------------------------------
    private function _drawField(
            dc      as Graphics.Dc,
            cx      as Lang.Number,
            cy      as Lang.Number,
            label   as Lang.String,
            value   as Lang.String,
            valColor as Lang.Number) as Void {

        // Value
        dc.setColor(valColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, cy, Graphics.FONT_MEDIUM, value, Graphics.TEXT_JUSTIFY_CENTER);

        // Label (small, below value)
        dc.setColor(COLOR_LABEL, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, cy + 30, Graphics.FONT_TINY, label, Graphics.TEXT_JUSTIFY_CENTER);
    }

    // -------------------------------------------------------------------------
    // Helper: draw page-indicator dots
    // -------------------------------------------------------------------------
    private function _drawPageDots(dc as Graphics.Dc, w as Lang.Number, h as Lang.Number) as Void {
        var dotR  = 3;
        var gap   = 10;
        var totalW = PAGE_COUNT * (dotR * 2) + (PAGE_COUNT - 1) * gap;
        var startX = (w - totalW) / 2 + dotR;
        var dotY   = h - 26;

        for (var i = 0; i < PAGE_COUNT; i++) {
            var dotX = startX + i * (dotR * 2 + gap);
            if (i == _page) {
                dc.setColor(COLOR_VALUE, Graphics.COLOR_TRANSPARENT);
                dc.fillCircle(dotX, dotY, dotR);
            } else {
                dc.setColor(COLOR_HINT, Graphics.COLOR_TRANSPARENT);
                dc.drawCircle(dotX, dotY, dotR);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Formatters
    // -------------------------------------------------------------------------

    //! Format a speed value in knots, e.g. "12.3 kt".
    private function _formatKnots(v as Lang.Float or Null) as Lang.String {
        if (v == null) { return "--"; }
        return v.format("%.1f") + " kt";
    }

    //! Format a direction in degrees true, e.g. "245°".
    private function _formatDegrees(v as Lang.Float or Null) as Lang.String {
        if (v == null) { return "--"; }
        return v.format("%.0f") + "°";
    }

    //! Format a depth value in metres, e.g. "12.5 m".
    private function _formatMeters(v as Lang.Float or Null) as Lang.String {
        if (v == null) { return "--"; }
        return v.format("%.1f") + " m";
    }

    //! Format a relative wind angle with port/starboard prefix, e.g. "PORT 45°" or "STBD 135°".
    private function _formatRelAngle(v as Lang.Float or Null) as Lang.String {
        if (v == null) { return "--"; }
        var prefix = v >= 0.0f ? "STBD " : "PORT ";
        return prefix + v.abs().format("%.0f") + "°";
    }

    // -------------------------------------------------------------------------
    // Page navigation (called by SailingDelegate)
    // -------------------------------------------------------------------------

    function nextPage() as Void {
        _page = (_page + 1) % PAGE_COUNT;
    }

    function prevPage() as Void {
        _page = (_page - 1 + PAGE_COUNT) % PAGE_COUNT;
    }
}

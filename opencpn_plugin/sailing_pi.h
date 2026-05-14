// sailing_pi.h
// OpenCPN plugin that extracts sailing data from incoming NMEA sentences
// and broadcasts it as JSON over UDP so that a companion app (e.g. on
// an Android/iOS phone running the Garmin Connect IQ SDK companion) can
// relay the data to a Garmin watch running the SailingApp MonkeyC app.
//
// NMEA sentences parsed
// ─────────────────────
//   DPT  – depth of water
//   DBT  – depth below transducer (fallback)
//   VTG  – track made good / ground speed  → SOG & COG
//   RMC  – recommended minimum navigation  → SOG & COG (fallback)
//   MWV  – wind speed and angle (apparent & true)
//   MWD  – wind direction and speed (true, fallback)
//
// UDP broadcast
// ─────────────
//   Default address : 255.255.255.255 (configurable)
//   Default port    : 55554           (configurable)
//   Payload         : UTF-8 JSON, e.g.
//     {"sog":5.2,"cog":245.0,"depth":12.5,"aws":15.0,"awa":135.0,"tws":12.0,"twd":220.0}

#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#  include <wx/wx.h>
#endif

#include "ocpn_plugin.h"

// ──────────────────────────────────────────────────────────────────────────────
// Forward declaration
// ──────────────────────────────────────────────────────────────────────────────
class SailingPlugin;

// ──────────────────────────────────────────────────────────────────────────────
// Sailing data container
// ──────────────────────────────────────────────────────────────────────────────
// Sentinel value used for fields that have a valid range of -180…+180 degrees,
// where -1.0 would be a valid measurement.  Any value below NO_DATA_ANGLE
// means "no data".
static constexpr double NO_DATA_ANGLE = -999.0;

struct SailingData {
    double sog   = -1.0;           // knots         (-1  = not available)
    double cog   = -1.0;           // degrees true   (-1  = not available)
    double depth = -1.0;           // metres         (-1  = not available)
    double aws   = -1.0;           // knots          (-1  = not available)
    double awa   = NO_DATA_ANGLE;  // degrees rel.   (NO_DATA_ANGLE = n/a)
    double tws   = -1.0;           // knots          (-1  = not available)
    double twd   = -1.0;           // degrees true   (-1  = not available)
};

// ──────────────────────────────────────────────────────────────────────────────
// Plugin class
// ──────────────────────────────────────────────────────────────────────────────
class SailingPlugin : public opencpn_plugin_116 {
public:
    SailingPlugin(void* ppimgr);
    ~SailingPlugin() override;

    // ── OpenCPN plugin interface ─────────────────────────────────────────────
    int          Init() override;
    bool         DeInit() override;

    int          GetAPIVersionMajor() override;
    int          GetAPIVersionMinor() override;
    int          GetPlugInVersionMajor() override;
    int          GetPlugInVersionMinor() override;

    wxString     GetShortDescription() override;
    wxString     GetLongDescription() override;
    wxString     GetCommonName() override;

    // Receive all NMEA sentences from OpenCPN.
    void         SetNMEASentence(wxString& sentence) override;

    // Provide a preferences dialog.
    void         ShowPreferencesDialog(wxWindow* parent) override;

    wxBitmap*    GetPlugInBitmap() override;

private:
    // ── NMEA parsers ─────────────────────────────────────────────────────────
    void _parseDPT(const wxString& s);
    void _parseDBT(const wxString& s);
    void _parseVTG(const wxString& s);
    void _parseRMC(const wxString& s);
    void _parseMWV(const wxString& s);
    void _parseMWD(const wxString& s);

    // ── UDP broadcast ────────────────────────────────────────────────────────
    void _broadcastData();
    bool _initSocket();
    void _closeSocket();

    // ── Internal helpers ────────────────────────────────────────────────────
    static double      _fieldToDouble(const wxArrayString& fields, size_t idx);
    static wxArrayString _splitSentence(const wxString& s);
    static bool        _verifyChecksum(const wxString& s);
    wxString           _buildJson() const;

    // ── Member variables ────────────────────────────────────────────────────
    SailingData _data;

    wxString _broadcastAddr;
    int      _broadcastPort;

    // Socket handle (SOCKET on Windows, int on POSIX)
#ifdef _WIN32
    uintptr_t _sock;        // SOCKET typedef'd from UINT_PTR
#else
    int       _sock;
#endif

    wxBitmap* _pBitmap;

    // Rate-limit: only broadcast if data changed or at most once per second.
    wxLongLong _lastBroadcastMs;
};

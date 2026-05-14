// sailing_pi.cpp
// Implementation of the OpenCPN "Sailing" plugin.
// See sailing_pi.h for full description.

#include "sailing_pi.h"

#include <wx/tokenzr.h>
#include <wx/datetime.h>
#include <wx/textdlg.h>
#include <wx/log.h>

#include <cmath>
#include <cstdio>
#include <cstring>

// Platform networking headers
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
   typedef SOCKET sock_t;
#  define INVALID_SOCK INVALID_SOCKET
#  define CLOSE_SOCK(s) closesocket(s)
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
   typedef int sock_t;
#  define INVALID_SOCK (-1)
#  define CLOSE_SOCK(s) ::close(s)
#endif

// ── Plugin metadata ──────────────────────────────────────────────────────────
static const int    PLUGIN_VERSION_MAJOR = 1;
static const int    PLUGIN_VERSION_MINOR = 0;
static const wxChar PLUGIN_SHORT_DESC[]  = wxT("Sailing Data Bridge");
static const wxChar PLUGIN_LONG_DESC[]   =
    wxT("Broadcasts real-time sailing data (depth, SOG, COG, wind) as JSON ")
    wxT("over UDP so that a Garmin ConnectIQ watch app can display it.");
static const wxChar PLUGIN_NAME[]        = wxT("Sailing");

// Default UDP target
static const wxChar DEFAULT_BCAST_ADDR[] = wxT("255.255.255.255");
static const int    DEFAULT_BCAST_PORT   = 55554;

// Minimum interval between broadcasts (ms)
static const long   BROADCAST_INTERVAL_MS = 1000;

// ── Required factory function ────────────────────────────────────────────────
extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr)
{
    return new SailingPlugin(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

// ── Constructor / destructor ─────────────────────────────────────────────────
SailingPlugin::SailingPlugin(void* ppimgr)
    : opencpn_plugin_116(ppimgr)
    , _broadcastAddr(DEFAULT_BCAST_ADDR)
    , _broadcastPort(DEFAULT_BCAST_PORT)
    , _sock(INVALID_SOCK)
    , _pBitmap(nullptr)
    , _lastBroadcastMs(0)
{
}

SailingPlugin::~SailingPlugin()
{
    _closeSocket();
    delete _pBitmap;
}

// ── Init / DeInit ────────────────────────────────────────────────────────────
int SailingPlugin::Init()
{
    // Request NMEA sentence callbacks.
    return (WANTS_NMEA_SENTENCES);
}

bool SailingPlugin::DeInit()
{
    _closeSocket();
    return true;
}

// ── Version / metadata ───────────────────────────────────────────────────────
int      SailingPlugin::GetAPIVersionMajor()    { return MY_API_VERSION_MAJOR; }
int      SailingPlugin::GetAPIVersionMinor()    { return MY_API_VERSION_MINOR; }
int      SailingPlugin::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }
int      SailingPlugin::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }
wxString SailingPlugin::GetShortDescription()   { return PLUGIN_SHORT_DESC; }
wxString SailingPlugin::GetLongDescription()    { return PLUGIN_LONG_DESC; }
wxString SailingPlugin::GetCommonName()         { return PLUGIN_NAME; }

wxBitmap* SailingPlugin::GetPlugInBitmap()
{
    // Return a default bitmap; replace with a proper icon if desired.
    if (!_pBitmap) {
        _pBitmap = new wxBitmap(wxNullBitmap);
    }
    return _pBitmap;
}

// ── Preferences ──────────────────────────────────────────────────────────────
void SailingPlugin::ShowPreferencesDialog(wxWindow* parent)
{
    // Simple text-entry dialog for broadcast address:port.
    wxString current = wxString::Format(wxT("%s:%d"),
                                        _broadcastAddr, _broadcastPort);
    wxTextEntryDialog dlg(parent,
        wxT("UDP broadcast address:port\n(e.g. 255.255.255.255:55554)"),
        wxT("Sailing Plugin Preferences"),
        current);

    if (dlg.ShowModal() == wxID_OK) {
        wxString val = dlg.GetValue().Trim();
        int colonPos = val.Find(wxT(':'), true);   // last colon
        if (colonPos != wxNOT_FOUND) {
            _broadcastAddr = val.Left(colonPos);
            long port = DEFAULT_BCAST_PORT;
            val.Mid(colonPos + 1).ToLong(&port);
            _broadcastPort = static_cast<int>(port);
        } else {
            _broadcastAddr = val;
        }
        // Re-create the socket with new settings.
        _closeSocket();
        _initSocket();
    }
}

// ── NMEA sentence handler ────────────────────────────────────────────────────
void SailingPlugin::SetNMEASentence(wxString& sentence)
{
    // Trim whitespace / CRLF.
    wxString s = sentence.Trim(true).Trim(false);

    if (s.IsEmpty()) { return; }

    // Verify checksum (if present).
    if (!_verifyChecksum(s)) { return; }

    // Extract sentence type (characters 3-5 after '$' or '!').
    if (s.Len() < 6) { return; }
    wxString type = s.Mid(3, 3).Upper();   // e.g. "DPT", "VTG", "MWV"

    if      (type == wxT("DPT")) { _parseDPT(s); }
    else if (type == wxT("DBT")) { _parseDBT(s); }
    else if (type == wxT("VTG")) { _parseVTG(s); }
    else if (type == wxT("RMC")) { _parseRMC(s); }
    else if (type == wxT("MWV")) { _parseMWV(s); }
    else if (type == wxT("MWD")) { _parseMWD(s); }
    else { return; }  // not a sentence we care about

    // Rate-limited broadcast: compare wxLongLong values directly to avoid
    // truncation that would occur with GetLo() for intervals > 49 days.
    wxLongLong now = wxGetLocalTimeMillis();
    if ((now - _lastBroadcastMs) >= static_cast<wxLongLong>(BROADCAST_INTERVAL_MS)) {
        _broadcastData();
        _lastBroadcastMs = now;
    }
}

// ── NMEA parsers ─────────────────────────────────────────────────────────────

// DPT – Depth of Water
//   $xxDPT,<depth_m>,<offset_m>,<max_range_m>*hh
void SailingPlugin::_parseDPT(const wxString& s)
{
    wxArrayString f = _splitSentence(s);
    // field 1 = depth in metres
    double d = _fieldToDouble(f, 1);
    if (d >= 0.0) {
        // Add transducer offset if present (field 2).
        // _fieldToDouble returns -1.0 when absent; any non-sentinel value is valid.
        double offset = _fieldToDouble(f, 2);
        if (offset > -1.0) { d += offset; }
        _data.depth = d;
    }
}

// DBT – Depth Below Transducer (fallback if no DPT)
//   $xxDBT,<depth_ft>,f,<depth_m>,M,<depth_fathoms>,F*hh
void SailingPlugin::_parseDBT(const wxString& s)
{
    if (_data.depth >= 0.0) { return; }   // prefer DPT
    wxArrayString f = _splitSentence(s);
    double d = _fieldToDouble(f, 3);      // field 3 = metres
    if (d >= 0.0) { _data.depth = d; }
}

// VTG – Track Made Good and Ground Speed
//   $xxVTG,<cog_t>,T,<cog_m>,M,<sog_N>,N,<sog_K>,K,<mode>*hh
void SailingPlugin::_parseVTG(const wxString& s)
{
    wxArrayString f = _splitSentence(s);
    double cog = _fieldToDouble(f, 1);   // degrees true
    double sog = _fieldToDouble(f, 5);   // knots
    if (cog >= 0.0) { _data.cog = cog; }
    if (sog >= 0.0) { _data.sog = sog; }
}

// RMC – Recommended Minimum Navigation Information (fallback)
//   $xxRMC,<utc>,<status>,<lat>,N/S,<lon>,E/W,<sog>,<cog>,<date>,…*hh
void SailingPlugin::_parseRMC(const wxString& s)
{
    wxArrayString f = _splitSentence(s);
    if (f.GetCount() < 9) { return; }

    // Only update if status == 'A' (active).
    if (f[2].Upper() != wxT("A")) { return; }

    double sog = _fieldToDouble(f, 7);
    double cog = _fieldToDouble(f, 8);
    if (sog >= 0.0 && _data.sog < 0.0) { _data.sog = sog; }
    if (cog >= 0.0 && _data.cog < 0.0) { _data.cog = cog; }
}

// MWV – Wind Speed and Angle
//   $xxMWV,<angle>,<R|T>,<speed>,<unit>,<status>*hh
//   R = relative (apparent), T = true
void SailingPlugin::_parseMWV(const wxString& s)
{
    wxArrayString f = _splitSentence(s);
    if (f.GetCount() < 6) { return; }
    if (f[5].Upper() != wxT("A")) { return; }   // status must be Active

    double angle = _fieldToDouble(f, 1);
    double speed = _fieldToDouble(f, 3);
    wxString ref  = f.GetCount() > 2 ? f[2].Upper() : wxT("");
    wxString unit = f.GetCount() > 4 ? f[4].Upper() : wxT("N");

    // Convert speed to knots if necessary.
    if (unit == wxT("M")) {       // m/s → kt
        speed *= 1.94384;
    } else if (unit == wxT("K")) { // km/h → kt
        speed *= 0.539957;
    }

    if (ref == wxT("R")) {
        // MWV apparent angle convention: 0° = ahead, positive = starboard.
        // Normalise to signed: angles > 180° become negative (port side).
        if (angle > 180.0) { angle -= 360.0; }
        if (angle > NO_DATA_ANGLE) { _data.awa = angle; }
        if (speed >= 0.0)     { _data.aws = speed; }
    } else if (ref == wxT("T")) {
        if (angle >= 0.0)  { _data.twd = angle; }
        if (speed >= 0.0)  { _data.tws = speed; }
    }
}

// MWD – Wind Direction and Speed (true wind only)
//   $xxMWD,<dir_t>,T,<dir_m>,M,<speed_N>,N,<speed_ms>,M*hh
void SailingPlugin::_parseMWD(const wxString& s)
{
    wxArrayString f = _splitSentence(s);
    double dir   = _fieldToDouble(f, 1);   // true direction
    double speed = _fieldToDouble(f, 5);   // knots
    if (dir   >= 0.0) { _data.twd = dir;   }
    if (speed >= 0.0) { _data.tws = speed; }
}

// ── UDP socket helpers ────────────────────────────────────────────────────────
bool SailingPlugin::_initSocket()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { return false; }
#endif

    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCK) { return false; }

    // Enable broadcast.
    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        CLOSE_SOCK(s);
        return false;
    }

    _sock = static_cast<decltype(_sock)>(s);
    return true;
}

void SailingPlugin::_closeSocket()
{
    if (_sock != INVALID_SOCK) {
        CLOSE_SOCK(static_cast<sock_t>(_sock));
        _sock = INVALID_SOCK;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void SailingPlugin::_broadcastData()
{
    if (_sock == INVALID_SOCK) {
        if (!_initSocket()) { return; }
    }

    wxString json = _buildJson();
    std::string payload = std::string(json.mb_str(wxConvUTF8));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(_broadcastPort));

    std::string addrStr((_broadcastAddr.mb_str(wxConvUTF8)));
    addr.sin_addr.s_addr = inet_addr(addrStr.c_str());

    sendto(static_cast<sock_t>(_sock),
           payload.c_str(),
           static_cast<int>(payload.size()),
           0,
           reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr));
}

// ── JSON builder ──────────────────────────────────────────────────────────────
wxString SailingPlugin::_buildJson() const
{
    // Build a compact JSON object with only the fields that have valid data.
    wxString j = wxT("{");
    bool first = true;

    auto addField = [&](const wxChar* key, double value, bool valid) {
        if (!valid) { return; }
        if (!first) { j += wxT(","); }
        j += wxString::Format(wxT("\"%s\":%.4g"), key, value);
        first = false;
    };

    addField(wxT("sog"),   _data.sog,   _data.sog   >= 0.0);
    addField(wxT("cog"),   _data.cog,   _data.cog   >= 0.0);
    addField(wxT("depth"), _data.depth, _data.depth >= 0.0);
    addField(wxT("aws"),   _data.aws,   _data.aws   >= 0.0);
    addField(wxT("awa"),   _data.awa,   _data.awa   > NO_DATA_ANGLE);
    addField(wxT("tws"),   _data.tws,   _data.tws   >= 0.0);
    addField(wxT("twd"),   _data.twd,   _data.twd   >= 0.0);

    j += wxT("}");
    return j;
}

// ── Static helpers ────────────────────────────────────────────────────────────

//! Split an NMEA sentence into comma-separated fields,
//! stripping the leading '$'/'!' and trailing checksum.
wxArrayString SailingPlugin::_splitSentence(const wxString& s)
{
    // Strip leading '$' or '!'
    wxString body = s;
    if (!body.IsEmpty() && (body[0] == wxT('$') || body[0] == wxT('!'))) {
        body = body.Mid(1);
    }

    // Strip trailing '*XX' checksum
    int star = body.Find(wxT('*'), true);
    if (star != wxNOT_FOUND) {
        body = body.Left(star);
    }

    wxArrayString fields;
    wxStringTokenizer tok(body, wxT(","));
    while (tok.HasMoreTokens()) {
        fields.Add(tok.GetNextToken());
    }
    return fields;
}

//! Convert a field at index idx to a double; returns -1 on failure.
double SailingPlugin::_fieldToDouble(const wxArrayString& fields, size_t idx)
{
    if (idx >= fields.GetCount()) { return -1.0; }
    const wxString& f = fields[idx];
    if (f.IsEmpty()) { return -1.0; }
    double v = 0.0;
    if (!f.ToDouble(&v)) { return -1.0; }
    return v;
}

//! Verify the NMEA XOR checksum if present.  Returns true if absent or valid.
bool SailingPlugin::_verifyChecksum(const wxString& s)
{
    int star = s.Find(wxT('*'), true);
    if (star == wxNOT_FOUND) { return true; }   // no checksum to check

    // Checksum covers characters between '$' (or '!') and '*'.
    int start = (s[0] == wxT('$') || s[0] == wxT('!')) ? 1 : 0;
    unsigned char calc = 0;
    for (int i = start; i < star; ++i) {
        calc ^= static_cast<unsigned char>(s[i]);
    }

    wxString hexStr = s.Mid(star + 1, 2);
    unsigned long parsed = 0;
    if (!hexStr.ToULong(&parsed, 16)) { return false; }

    return (calc == static_cast<unsigned char>(parsed));
}

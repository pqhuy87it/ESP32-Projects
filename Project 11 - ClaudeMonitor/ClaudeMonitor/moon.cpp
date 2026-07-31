#include "config.h"
#ifdef MANGO_UI

#include "moon.h"
#include <math.h>
#include <stdlib.h>

// ── Pha trăng ───────────────────────────────────────────
static const double SYNODIC = 29.53058867;         // chu kỳ giao hội
static const double NEW_MOON_REF = 947182440.0;     // 2000-01-06 18:14 UTC

static const char* PHASE_NAMES[] = {
    "New Moon", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
    "Full Moon", "Waning Gibbous", "Last Quarter", "Waning Crescent"
};

// ── Vị trí mặt trăng (thuật toán rút gọn, đủ cho rise/set) ──
// Trả về RA (giờ) và Dec (độ) của mặt trăng tại Julian day jd.
static void moonRaDec(double jd, double& ra, double& dec) {
    double d = jd - 2451545.0;                      // ngày từ J2000
    double T = d / 36525.0;

    // Các tham số quỹ đạo (độ) — công thức Meeus rút gọn.
    double L = 218.316 + 13.176396 * d;             // kinh độ trung bình
    double M = 134.963 + 13.064993 * d;             // dị thường trung bình
    double F = 93.272  + 13.229350 * d;             // argument of latitude

    L = fmod(L, 360.0); M = fmod(M, 360.0); F = fmod(F, 360.0);
    double Lr = L * M_PI / 180.0, Mr = M * M_PI / 180.0, Fr = F * M_PI / 180.0;

    // Kinh độ hoàng đạo & vĩ độ (độ).
    double lon = L + 6.289 * sin(Mr);
    double lat = 5.128 * sin(Fr);
    double lonR = lon * M_PI / 180.0, latR = lat * M_PI / 180.0;

    // Độ nghiêng hoàng đạo.
    double eps = (23.439 - 0.0000004 * d) * M_PI / 180.0;

    // Hoàng đạo → xích đạo.
    double x = cos(latR) * cos(lonR);
    double y = cos(eps) * cos(latR) * sin(lonR) - sin(eps) * sin(latR);
    double z = sin(eps) * cos(latR) * sin(lonR) + cos(eps) * sin(latR);

    ra  = atan2(y, x) * 180.0 / M_PI / 15.0;         // giờ
    if (ra < 0) ra += 24.0;
    dec = atan2(z, sqrt(x * x + y * y)) * 180.0 / M_PI;
}

// Độ cao mặt trăng (độ) tại thời điểm jd, cho lat/lon (độ).
static double moonAltitude(double jd, double latDeg, double lonDeg) {
    double ra, dec;
    moonRaDec(jd, ra, dec);

    // Greenwich Mean Sidereal Time (giờ).
    double d = jd - 2451545.0;
    double gmst = fmod(18.697374558 + 24.06570982441908 * d, 24.0);
    if (gmst < 0) gmst += 24.0;

    // Local Sidereal Time → Hour Angle.
    double lst = fmod(gmst + lonDeg / 15.0, 24.0);
    double ha = (lst - ra) * 15.0;                   // độ
    double haR = ha * M_PI / 180.0;
    double decR = dec * M_PI / 180.0;
    double latR = latDeg * M_PI / 180.0;

    double alt = asin(sin(latR) * sin(decR) + cos(latR) * cos(decR) * cos(haR));
    return alt * 180.0 / M_PI;
}

// Chuyển epoch (giây UTC) → Julian day.
static double epochToJD(long epoch) {
    return (double)epoch / 86400.0 + 2440587.5;
}

// Tìm mọc/lặn trong ngày địa phương [dayStart, dayStart+24h). Quét mỗi 10 phút,
// bắt thời điểm altitude vượt qua -0.833° (bán kính trăng + khúc xạ ~ horizon).
static void findRiseSet(long dayStartUtc, int tzOffsetSec, double lat, double lon,
                        MoonData& out) {
    const double H0 = -0.833;
    out.riseValid = out.setValid = false;
    double prevAlt = 0;
    bool first = true;

    for (int m = 0; m <= 24 * 60; m += 10) {
        long t = dayStartUtc + (long)m * 60;
        double alt = moonAltitude(epochToJD(t), lat, lon);
        if (!first) {
            // vượt lên qua horizon → mọc
            if (prevAlt < H0 && alt >= H0 && !out.riseValid) {
                long tt = t - 300;                   // giữa khoảng 10'
                long local = tt + tzOffsetSec;
                int mins = (int)((local / 60) % (24 * 60) + 24 * 60) % (24 * 60);
                out.riseH = mins / 60; out.riseM = mins % 60; out.riseValid = true;
            }
            // hạ xuống qua horizon → lặn
            if (prevAlt >= H0 && alt < H0 && !out.setValid) {
                long tt = t - 300;
                long local = tt + tzOffsetSec;
                int mins = (int)((local / 60) % (24 * 60) + 24 * 60) % (24 * 60);
                out.setH = mins / 60; out.setM = mins % 60; out.setValid = true;
            }
        }
        prevAlt = alt; first = false;
    }
}

void computeMoon(long epoch, int tzOffsetSec, MoonData& out) {
    // ── Pha ──
    double days = ((double)epoch - NEW_MOON_REF) / 86400.0;
    double cycles = days / SYNODIC;
    double frac = cycles - floor(cycles);
    if (frac < 0) frac += 1.0;

    out.ageDays = (float)(frac * SYNODIC);

    int idx = (int)round(frac * 30.0);
    if (idx > 30) idx = 30; if (idx < 0) idx = 0;
    out.imageIndex = idx;

    double illum = (1.0 - cos(frac * 2.0 * M_PI)) / 2.0;
    out.illumPct = (int)round(illum * 100.0);

    int seg = (int)round(frac * 8.0) % 8;
    out.phaseName = PHASE_NAMES[seg];

    // ── Ngày địa phương ──
    long local = epoch + tzOffsetSec;
    long localDay = (local / 86400) * 86400;         // 00:00 local (giây UTC tương ứng)
    long dayStartUtc = localDay - tzOffsetSec;

    // Tách y/m/d từ local (dùng thuật toán civil-from-days).
    long z = local / 86400 + 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long y = (long)yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153;
    unsigned dd = doy - (153*mp+2)/5 + 1;
    unsigned mm = mp < 10 ? mp+3 : mp-9;
    out.year  = (int)(y + (mm <= 2));
    out.month = (int)mm;
    out.day   = (int)dd;

    // ── Mọc/lặn ──
    findRiseSet(dayStartUtc, tzOffsetSec, atof(OWM_LAT), atof(OWM_LON), out);
}

#endif // MANGO_UI

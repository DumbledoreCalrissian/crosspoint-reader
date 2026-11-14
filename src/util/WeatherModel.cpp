#include "WeatherModel.h"

#include <cstdio>
#include <cstring>
namespace weather {
namespace {
bool leap(unsigned y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
constexpr unsigned LENGTHS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int32_t civilDay(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = y - era * 400;
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  return era * 146097 + int(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
bool percent(uint8_t p) { return p <= 100 || p == UNKNOWN_PERCENT; }
bool temp(int16_t t) { return t >= -1000 && t <= 700; }
}  // namespace
int32_t isoDay(std::string_view text) {
  if (text.size() != 10 || text[4] != '-' || text[7] != '-') return 0;
  unsigned y = 0, m = 0, d = 0;
  for (unsigned i = 0; i < 10; ++i) {
    if (i == 4 || i == 7) continue;
    if (text[i] < '0' || text[i] > '9') return 0;
    auto& value = i < 4 ? y : i < 7 ? m : d;
    value = value * 10 + text[i] - '0';
  }
  if (y < 2020 || y > 2100 || m < 1 || m > 12 || d < 1 || d > LENGTHS[m - 1] + (m == 2 && leap(y))) return 0;
  return civilDay(y, m, d);
}
void civilDate(int32_t date, unsigned& year, unsigned& month, unsigned& day) {
  int z = date + 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = z - era * 146097;
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  unsigned y = yoe + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  year = y + (month <= 2);
}
bool validText(std::string_view text, bool empty) {
  if (text.empty()) return empty;
  for (size_t i = 0; i < text.size();) {
    uint32_t c = static_cast<unsigned char>(text[i++]);
    if (c < 32 || c == 127) return false;
    if (c < 128) continue;
    unsigned n = 0;
    uint32_t minimum = 0;
    if (c >= 0xc2 && c <= 0xdf) {
      n = 1;
      c &= 31;
      minimum = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
      n = 2;
      c &= 15;
      minimum = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
      n = 3;
      c &= 7;
      minimum = 0x10000;
    } else
      return false;
    if (n > text.size() - i) return false;
    while (n--) {
      const auto b = static_cast<unsigned char>(text[i++]);
      if ((b & 0xc0) != 0x80) return false;
      c = (c << 6) | (b & 63);
    }
    if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
  }
  return true;
}
bool validQuery(std::string_view query) {
  if (query.size() > 64 || !validText(query)) return false;
  unsigned count = 0;
  for (unsigned char c : query)
    if ((c & 0xc0) != 0x80) ++count;
  return count >= 2;
}
bool valid(const Location& p) {
  const auto n = strnlen(p.name, sizeof(p.name)), r = strnlen(p.region, sizeof(p.region));
  return n < sizeof(p.name) && r < sizeof(p.region) && validText({p.name, n}) && validText({p.region, r}, true) &&
         p.country[2] == 0 && p.country[0] >= 'A' && p.country[0] <= 'Z' && p.country[1] >= 'A' &&
         p.country[1] <= 'Z' && p.latitude >= -90000000 && p.latitude <= 90000000 && p.longitude >= -180000000 &&
         p.longitude <= 180000000;
}
bool valid(const Forecast& f) {
  if (f.isDay > 1 || f.currentDate < isoDay("2020-01-01") || f.currentDate > isoDay("2100-12-31") ||
      f.currentMinute >= 1440 || !temp(f.temperature) || !percent(f.humidity) || f.utcOffset < -64800 ||
      f.utcOffset > 64800 || (f.code > 99 && f.code != 255) || !f.sampleEpoch)
    return false;
  const int64_t sample = int64_t(f.currentDate) * 86400 + f.currentMinute * 60 - int64_t(f.utcOffset);
  if (f.sampleEpoch < uint64_t(sample) || f.sampleEpoch > uint64_t(sample + 59)) return false;
  if (f.fetchedEpoch && (f.fetchedEpoch + 86400 < f.sampleEpoch || f.fetchedEpoch > f.sampleEpoch + 86400))
    return false;
  for (unsigned i = 0; i < DAYS; ++i) {
    const auto& d = f.days[i];
    if (d.date != f.currentDate + int(i) || !temp(d.high) || !temp(d.low) || d.high < d.low || !percent(d.chance) ||
        (d.rain > 30000 && d.rain != UNKNOWN_RAIN) || (d.code > 99 && d.code != 255))
      return false;
  }
  return true;
}
bool valid(const Snapshot& s) {
  return (!s.configured || valid(s.location)) && (!s.cached || (s.configured && valid(s.forecast)));
}
int temperature(int16_t tenths, bool fahrenheit) {
  const int n = fahrenheit ? int(tenths) * 9 + 1600 : int(tenths) * 5;
  return n < 0 ? -((-n + 25) / 50) : (n + 25) / 50;
}
}  // namespace weather

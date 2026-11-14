#include "WeatherPresentation.h"

#include <I18n.h>

#include <cstdio>

#include "components/icons/weather.h"
namespace weatherui {
namespace {
static constexpr StrId WEEK[] = {StrId::STR_WEATHER_SUN, StrId::STR_WEATHER_MON, StrId::STR_WEATHER_TUE,
                                 StrId::STR_WEATHER_WED, StrId::STR_WEATHER_THU, StrId::STR_WEATHER_FRI,
                                 StrId::STR_WEATHER_SAT};
static constexpr StrId MONTH[] = {StrId::STR_WEATHER_JAN, StrId::STR_WEATHER_FEB, StrId::STR_WEATHER_MAR,
                                  StrId::STR_WEATHER_APR, StrId::STR_WEATHER_MAY, StrId::STR_WEATHER_JUN,
                                  StrId::STR_WEATHER_JUL, StrId::STR_WEATHER_AUG, StrId::STR_WEATHER_SEP,
                                  StrId::STR_WEATHER_OCT, StrId::STR_WEATHER_NOV, StrId::STR_WEATHER_DEC};
}  // namespace
const char* condition(uint8_t code) {
  switch (code) {
    case 0:
      return tr(STR_WEATHER_CLEAR);
    case 1:
      return tr(STR_WEATHER_MAINLY_CLEAR);
    case 2:
      return tr(STR_WEATHER_PARTLY);
    case 3:
      return tr(STR_WEATHER_CLOUDY);
    case 45:
    case 48:
      return tr(STR_WEATHER_FOG);
    case 51:
    case 53:
    case 55:
      return tr(STR_WEATHER_DRIZZLE);
    case 56:
    case 57:
      return tr(STR_WEATHER_FREEZING_DRIZZLE);
    case 61:
      return tr(STR_WEATHER_LIGHT_RAIN);
    case 63:
      return tr(STR_WEATHER_RAIN);
    case 65:
      return tr(STR_WEATHER_HEAVY_RAIN);
    case 66:
    case 67:
      return tr(STR_WEATHER_FREEZING_RAIN);
    case 80:
    case 81:
    case 82:
      return tr(STR_WEATHER_SHOWERS);
    case 71:
      return tr(STR_WEATHER_LIGHT_SNOW);
    case 73:
    case 77:
      return tr(STR_WEATHER_SNOW);
    case 75:
      return tr(STR_WEATHER_HEAVY_SNOW);
    case 85:
    case 86:
      return tr(STR_WEATHER_SNOW_SHOWERS);
    case 95:
    case 96:
    case 99:
      return tr(STR_WEATHER_STORM);
    default:
      return tr(STR_WEATHER_UNKNOWN);
  }
}
const uint8_t* icon(uint8_t code, bool daylight, bool large) {
  switch (code) {
    case 0:
      return daylight ? (large ? WeatherClearLargeIcon : WeatherClearIcon)
                      : (large ? WeatherNightLargeIcon : WeatherNightIcon);
    case 1:
    case 2:
      return daylight ? (large ? WeatherPartlyLargeIcon : WeatherPartlyIcon)
                      : (large ? WeatherPartlyNightLargeIcon : WeatherPartlyNightIcon);
    case 45:
    case 48:
      return (large ? WeatherFogLargeIcon : WeatherFogIcon);
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82:
      return (large ? WeatherRainLargeIcon : WeatherRainIcon);
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return (large ? WeatherSnowLargeIcon : WeatherSnowIcon);
    case 95:
    case 96:
    case 99:
      return (large ? WeatherStormLargeIcon : WeatherStormIcon);
    default:
      return (large ? WeatherCloudLargeIcon : WeatherCloudIcon);
  }
}
void temperature(char* out, size_t cap, int16_t value, bool fahrenheit, bool unit) {
  snprintf(out, cap, unit ? (fahrenheit ? tr(STR_WEATHER_TEMP_F) : tr(STR_WEATHER_TEMP_C)) : tr(STR_WEATHER_TEMP),
           weather::temperature(value, fahrenheit));
}
void range(char* out, size_t cap, const weather::Day& d, bool f) {
  snprintf(out, cap, tr(STR_WEATHER_RANGE), weather::temperature(d.high, f), weather::temperature(d.low, f));
}
void precipitation(char* out, size_t cap, uint16_t value, bool f) {
  if (value == weather::UNKNOWN_RAIN) {
    snprintf(out, cap, "%s", tr(STR_WEATHER_MISSING));
    return;
  }
  if (f) {
    const unsigned hundredths = (unsigned(value) * 100 + 127) / 254;
    snprintf(out, cap, tr(STR_WEATHER_INCHES), hundredths / 100, hundredths % 100);
  } else if (value % 10 == 0)
    snprintf(out, cap, tr(STR_WEATHER_MM_WHOLE), unsigned(value / 10));
  else
    snprintf(out, cap, tr(STR_WEATHER_MM), unsigned(value / 10), unsigned(value % 10));
}
void percent(char* out, size_t cap, uint8_t value) {
  if (value == weather::UNKNOWN_PERCENT)
    snprintf(out, cap, "%s", tr(STR_WEATHER_MISSING));
  else
    snprintf(out, cap, tr(STR_WEATHER_PERCENT), unsigned(value));
}
void day(char* out, size_t cap, int32_t date, const weather::Forecast& f, uint64_t now, bool compact) {
  if (now && int64_t(now) + f.utcOffset >= 0 && int32_t((int64_t(now) + f.utcOffset) / 86400) == date) {
    snprintf(out, cap, "%s", tr(STR_WEATHER_TODAY));
    return;
  }
  if (compact) {
    snprintf(out, cap, "%s", I18N.get(WEEK[(date + 4) % 7]));
    return;
  }
  unsigned year, month, dom;
  weather::civilDate(date, year, month, dom);
  snprintf(out, cap, tr(STR_WEATHER_DAY_LABEL), I18N.get(WEEK[(date + 4) % 7]), dom);
}
void updated(char* out, size_t cap, const weather::Forecast& f) {
  const int64_t local = int64_t(f.fetchedEpoch ? f.fetchedEpoch : f.sampleEpoch) + f.utcOffset;
  unsigned year, month, dom;
  weather::civilDate(local / 86400, year, month, dom);
  const unsigned minute = (local % 86400) / 60;
  snprintf(out, cap, f.fetchedEpoch ? tr(STR_WEATHER_UPDATED) : tr(STR_WEATHER_ASOF), dom, I18N.get(MONTH[month - 1]),
           minute / 60, minute % 60);
}
bool stale(const weather::Forecast& f, uint64_t now) {
  return !now || !f.fetchedEpoch || now < f.fetchedEpoch || now - f.fetchedEpoch > 3600;
}
}  // namespace weatherui

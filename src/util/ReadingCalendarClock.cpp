#include "ReadingCalendarClock.h"

#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>

#include "CrossPointSettings.h"
#include "ReadingCalendarStore.h"
namespace readingcalendar {
void loadGoalSetting() {
  auto ledger = makeUniqueNoThrow<Store>();
  if (!ledger) {
    LOG_ERR("CAL", "OOM: goal cache (%u bytes)", unsigned(sizeof(Store)));
    return;
  }
  ledger->load();
  if (ledger->canSave()) SETTINGS.dailyReadingGoal = ledger->goalIndex();
}
uint64_t localMilliseconds(tm* date) {
  tm local{};
  if (!SETTINGS.clockHasBeenSynced || !halClock.localTimeFresh(local) || local.tm_year < 120 || local.tm_year > 200)
    return 0;
  if (date) *date = local;
  return uint64_t(civilDay(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday)) * Model::MAX_DAY_MS +
         uint64_t(local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec) * 1000;
}
}  // namespace readingcalendar

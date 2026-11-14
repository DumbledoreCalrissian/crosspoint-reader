#pragma once
#include <functional>
#include <string_view>

#include "WeatherModel.h"
namespace weather {
enum class Fetch { Ok, Cancelled, Network, InvalidData, OutOfMemory };
Fetch search(std::string_view query, Location* places, unsigned& count, const std::function<bool()>& abort);
Fetch refresh(const Location& place, Forecast& forecast, const std::function<bool()>& abort);
}  // namespace weather

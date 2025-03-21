/* API response deserialization for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <vector>
#include <math.h>
#include <ArduinoJson.h>

#include "api_response.h"
#include "config.h"

// Haversine formula for distance calculation
float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0; // Earth radius in km
  float dLat = radians(lat2 - lat1);
  float dLon = radians(lon2 - lon1);

  float a = sin(dLat / 2) * sin(dLat / 2) +
            cos(radians(lat1)) * cos(radians(lat2)) *
            sin(dLon / 2) * sin(dLon / 2);

  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

DeserializationError deserializeOneCall(WiFiClient &json,
                                        owm_resp_onecall_t &r)
{
  int i;

  JsonDocument filter;
  filter["current"]  = true;
  filter["minutely"] = false;
  filter["hourly"]   = true;
  filter["daily"]    = true;
#if !DISPLAY_ALERTS
  filter["alerts"]   = false;
#else
  // description can be very long so they are filtered out to save on memory
  // along with sender_name
  for (int i = 0; i < OWM_NUM_ALERTS; ++i)
  {
    filter["alerts"][i]["sender_name"] = false;
    filter["alerts"][i]["event"]       = true;
    filter["alerts"][i]["start"]       = true;
    filter["alerts"][i]["end"]         = true;
    filter["alerts"][i]["description"] = false;
    filter["alerts"][i]["tags"]        = true;
  }
#endif

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json,
                                         DeserializationOption::Filter(filter));
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : "
                 + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  r.lat             = doc["lat"]            .as<float>();
  r.lon             = doc["lon"]            .as<float>();
  r.timezone        = doc["timezone"]       .as<const char *>();
  r.timezone_offset = doc["timezone_offset"].as<int>();

  JsonObject current = doc["current"];
  r.current.dt         = current["dt"]        .as<int64_t>();
  r.current.sunrise    = current["sunrise"]   .as<int64_t>();
  r.current.sunset     = current["sunset"]    .as<int64_t>();
  r.current.temp       = current["temp"]      .as<float>();
  r.current.feels_like = current["feels_like"].as<float>();
  r.current.pressure   = current["pressure"]  .as<int>();
  r.current.humidity   = current["humidity"]  .as<int>();
  r.current.dew_point  = current["dew_point"] .as<float>();
  r.current.clouds     = current["clouds"]    .as<int>();
  r.current.uvi        = current["uvi"]       .as<float>();
  r.current.visibility = current["visibility"].as<int>();
  r.current.wind_speed = current["wind_speed"].as<float>();
  r.current.wind_gust  = current["wind_gust"] .as<float>();
  r.current.wind_deg   = current["wind_deg"]  .as<int>();
  r.current.rain_1h    = current["rain"]["1h"].as<float>();
  r.current.snow_1h    = current["snow"]["1h"].as<float>();
  JsonObject current_weather = current["weather"][0];
  r.current.weather.id          = current_weather["id"]         .as<int>();
  r.current.weather.main        = current_weather["main"]       .as<const char *>();
  r.current.weather.description = current_weather["description"].as<const char *>();
  r.current.weather.icon        = current_weather["icon"]       .as<const char *>();

  // minutely forecast is currently unused
  // i = 0;
  // for (JsonObject minutely : doc["minutely"].as<JsonArray>())
  // {
  //   r.minutely[i].dt            = minutely["dt"]           .as<int64_t>();
  //   r.minutely[i].precipitation = minutely["precipitation"].as<float>();

  //   if (i == OWM_NUM_MINUTELY - 1)
  //   {
  //     break;
  //   }
  //   ++i;
  // }

  i = 0;
  for (JsonObject hourly : doc["hourly"].as<JsonArray>())
  {
    r.hourly[i].dt         = hourly["dt"]        .as<int64_t>();
    r.hourly[i].temp       = hourly["temp"]      .as<float>();
    r.hourly[i].feels_like = hourly["feels_like"].as<float>();
    r.hourly[i].pressure   = hourly["pressure"]  .as<int>();
    r.hourly[i].humidity   = hourly["humidity"]  .as<int>();
    r.hourly[i].dew_point  = hourly["dew_point"] .as<float>();
    r.hourly[i].clouds     = hourly["clouds"]    .as<int>();
    r.hourly[i].uvi        = hourly["uvi"]       .as<float>();
    r.hourly[i].visibility = hourly["visibility"].as<int>();
    r.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    r.hourly[i].wind_gust  = hourly["wind_gust"] .as<float>();
    r.hourly[i].wind_deg   = hourly["wind_deg"]  .as<int>();
    r.hourly[i].pop        = hourly["pop"]       .as<float>();
    r.hourly[i].rain_1h    = hourly["rain"]["1h"].as<float>();
    r.hourly[i].snow_1h    = hourly["snow"]["1h"].as<float>();
    JsonObject hourly_weather = hourly["weather"][0];
    r.hourly[i].weather.id          = hourly_weather["id"]         .as<int>();
    r.hourly[i].weather.main        = hourly_weather["main"]       .as<const char *>();
    r.hourly[i].weather.description = hourly_weather["description"].as<const char *>();
    r.hourly[i].weather.icon        = hourly_weather["icon"]       .as<const char *>();

    if (i == OWM_NUM_HOURLY - 1)
    {
      break;
    }
    ++i;
  }

  i = 0;
  for (JsonObject daily : doc["daily"].as<JsonArray>())
  {
    r.daily[i].dt         = daily["dt"]        .as<int64_t>();
    r.daily[i].sunrise    = daily["sunrise"]   .as<int64_t>();
    r.daily[i].sunset     = daily["sunset"]    .as<int64_t>();
    r.daily[i].moonrise   = daily["moonrise"]  .as<int64_t>();
    r.daily[i].moonset    = daily["moonset"]   .as<int64_t>();
    r.daily[i].moon_phase = daily["moon_phase"].as<float>();
    JsonObject daily_temp = daily["temp"];
    r.daily[i].temp.morn  = daily_temp["morn"] .as<float>();
    r.daily[i].temp.day   = daily_temp["day"]  .as<float>();
    r.daily[i].temp.eve   = daily_temp["eve"]  .as<float>();
    r.daily[i].temp.night = daily_temp["night"].as<float>();
    r.daily[i].temp.min   = daily_temp["min"]  .as<float>();
    r.daily[i].temp.max   = daily_temp["max"]  .as<float>();
    JsonObject daily_feels_like = daily["feels_like"];
    r.daily[i].feels_like.morn  = daily_feels_like["morn"] .as<float>();
    r.daily[i].feels_like.day   = daily_feels_like["day"]  .as<float>();
    r.daily[i].feels_like.eve   = daily_feels_like["eve"]  .as<float>();
    r.daily[i].feels_like.night = daily_feels_like["night"].as<float>();
    r.daily[i].pressure   = daily["pressure"]  .as<int>();
    r.daily[i].humidity   = daily["humidity"]  .as<int>();
    r.daily[i].dew_point  = daily["dew_point"] .as<float>();
    r.daily[i].clouds     = daily["clouds"]    .as<int>();
    r.daily[i].uvi        = daily["uvi"]       .as<float>();
    r.daily[i].visibility = daily["visibility"].as<int>();
    r.daily[i].wind_speed = daily["wind_speed"].as<float>();
    r.daily[i].wind_gust  = daily["wind_gust"] .as<float>();
    r.daily[i].wind_deg   = daily["wind_deg"]  .as<int>();
    r.daily[i].pop        = daily["pop"]       .as<float>();
    r.daily[i].rain       = daily["rain"]      .as<float>();
    r.daily[i].snow       = daily["snow"]      .as<float>();
    JsonObject daily_weather = daily["weather"][0];
    r.daily[i].weather.id          = daily_weather["id"]         .as<int>();
    r.daily[i].weather.main        = daily_weather["main"]       .as<const char *>();
    r.daily[i].weather.description = daily_weather["description"].as<const char *>();
    r.daily[i].weather.icon        = daily_weather["icon"]       .as<const char *>();

    if (i == OWM_NUM_DAILY - 1)
    {
      break;
    }
    ++i;
  }

#if DISPLAY_ALERTS
  i = 0;
  for (JsonObject alerts : doc["alerts"].as<JsonArray>())
  {
    owm_alerts_t new_alert = {};
    // new_alert.sender_name = alerts["sender_name"].as<const char *>();
    new_alert.event       = alerts["event"]      .as<const char *>();
    new_alert.start       = alerts["start"]      .as<int64_t>();
    new_alert.end         = alerts["end"]        .as<int64_t>();
    // new_alert.description = alerts["description"].as<const char *>();
    new_alert.tags        = alerts["tags"][0]    .as<const char *>();
    r.alerts.push_back(new_alert);

    if (i == OWM_NUM_ALERTS - 1)
    {
      break;
    }
    ++i;
  }
#endif

  return error;
} // end deserializeOneCall

DeserializationError deserializeAirQuality(WiFiClient &json,
                                           owm_resp_air_pollution_t &r)
{
  int i = 0;

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : "
                 + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  r.coord.lat = doc["coord"]["lat"].as<float>();
  r.coord.lon = doc["coord"]["lon"].as<float>();

  for (JsonObject list : doc["list"].as<JsonArray>())
  {

    r.main_aqi[i] = list["main"]["aqi"].as<int>();

    JsonObject list_components = list["components"];
    r.components.co[i]    = list_components["co"].as<float>();
    r.components.no[i]    = list_components["no"].as<float>();
    r.components.no2[i]   = list_components["no2"].as<float>();
    r.components.o3[i]    = list_components["o3"].as<float>();
    r.components.so2[i]   = list_components["so2"].as<float>();
    r.components.pm2_5[i] = list_components["pm2_5"].as<float>();
    r.components.pm10[i]  = list_components["pm10"].as<float>();
    r.components.nh3[i]   = list_components["nh3"].as<float>();

    r.dt[i] = list["dt"].as<int64_t>();

    if (i == OWM_NUM_AIR_POLLUTION - 1)
    {
      break;
    }
    ++i;
  }

  return error;
} // end deserializeAirQuality

DeserializationError deserializeUSGSEarthquake(WiFiClient &json, usgs_feature_t &r, 
                                               float my_lat, float my_lon){
  float min_distance;

  JsonDocument filter;
  filter["type"]                                 = false;
  filter["metadata"]                             = false;
  filter["features"]                             = true;
  filter["features"][0]["bbox"]                  = false;
  filter["features"][0]["geometry"]              = true;
  filter["features"][0]["id"]                    = false;

  JsonDocument doc;
  
  //DeserializationError error = deserializeJson(doc, json);
  
  DeserializationError error = deserializeJson(doc, json,
                                        DeserializationOption::Filter(filter));
  
  
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : "
                 + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  min_distance = __FLT_MAX__;
  float lat = 0.0;
  float lon = 0.0;

  for (JsonObject feature : doc["features"].as<JsonArray>()) {

    JsonArray coordinates = feature["geometry"]["coordinates"];
    lon = coordinates[0].as<float>();  // Longitude is first
    lat = coordinates[1].as<float>();  // Latitude is second

    float distance = calculateDistance(my_lat, my_lon, lat, lon);

    if (distance < min_distance) {
      min_distance = distance;

      JsonObject geometry = feature["geometry"];
      r.geometry.lat   = lat;
      r.geometry.lon   = lon;
      r.geometry.depth = geometry["depth"] .as<float>();

      JsonObject properties = feature["properties"];
      r.properties.mag      = properties["mag"]     .as<float>();
      r.properties.place    = properties["place"]   .as<const char *>();
      r.properties.time     = properties["time"]    .as<int64_t>();
      r.properties.updated  = properties["updated"] .as<int64_t>();
      r.properties.alert    = properties["alert"]   .as<const char *>();
      r.properties.status   = properties["status"]  .as<int64_t>();
      r.properties.tsunami  = properties["tsunami"] .as<int64_t>();
      r.properties.dmin     = properties["dmin"]    .as<float>();  
      r.properties.type     = properties["type"]    .as<const char *>();                             
    }

  }

  return error;

} // end deserializeUSGSEarthquake
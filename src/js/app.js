var Clay = require("@rebble/clay");
var clayConfig = require("./config");
var messageKeys = require("message_keys");

var clay = new Clay(
  clayConfig,
  null,
  {
    autoHandleEvents: false
  }
);

var STORAGE_EFFECT = "ppf-animation-effect";
var STORAGE_SPEED = "ppf-animation-speed";
var STORAGE_SHOW_DATE = "ppf-show-date";
var STORAGE_SHOW_TEMPERATURE = "ppf-show-temperature";
var STORAGE_SHOW_SWISS_EMBLEM = "ppf-show-swiss-emblem";

var STORAGE_TEMPERATURE = "ppf-temperature";
var STORAGE_TEMPERATURE_TIME = "ppf-temperature-time";

var WEATHER_CACHE_MS = 25 * 60 * 1000;

var weatherRequestRunning = false;


function storedValue(key, fallback) {
  var value = localStorage.getItem(key);

  if (value === null || value === undefined) {
    return fallback;
  }

  return value;
}


function storedBoolean(key, fallback) {
  var value = localStorage.getItem(key);

  if (value === null || value === undefined) {
    return fallback;
  }

  return value === "true" || value === "1";
}


function parseBoolean(value, fallback) {
  if (value === undefined || value === null) {
    return fallback;
  }

  return (
    value === true
    || value === "true"
    || value === 1
    || value === "1"
  );
}


function sendTemperature(temperature) {
  var message = {};

  message[messageKeys.Temperature] = temperature;

  Pebble.sendAppMessage(
    message,
    function() {
      console.log(
        "Temperature sent: "
        + temperature
        + " C"
      );
    },
    function(error) {
      console.log(
        "Temperature send failed: "
        + JSON.stringify(error)
      );
    }
  );
}


function useCachedTemperature() {
  var cachedTemperature = parseInt(
    localStorage.getItem(STORAGE_TEMPERATURE),
    10
  );

  var cachedTime = parseInt(
    localStorage.getItem(STORAGE_TEMPERATURE_TIME),
    10
  );

  if (
    isNaN(cachedTemperature)
    || isNaN(cachedTime)
  ) {
    return false;
  }

  if (
    Date.now() - cachedTime
        > WEATHER_CACHE_MS
  ) {
    return false;
  }

  sendTemperature(cachedTemperature);
  return true;
}


function fetchTemperature(forceRefresh) {
  if (
    !storedBoolean(
      STORAGE_SHOW_TEMPERATURE,
      true
    )
  ) {
    return;
  }

  if (weatherRequestRunning) {
    return;
  }

  if (!forceRefresh && useCachedTemperature()) {
    return;
  }

  weatherRequestRunning = true;

  navigator.geolocation.getCurrentPosition(
    function(position) {
      var latitude =
          position.coords.latitude;

      var longitude =
          position.coords.longitude;

      var url =
          "https://api.open-meteo.com/v1/forecast"
          + "?latitude="
          + encodeURIComponent(latitude)
          + "&longitude="
          + encodeURIComponent(longitude)
          + "&current=temperature_2m"
          + "&temperature_unit=celsius";

      var request = new XMLHttpRequest();

      request.onload = function() {
        weatherRequestRunning = false;

        if (
          request.status < 200
          || request.status >= 300
        ) {
          console.log(
            "Weather HTTP error: "
            + request.status
          );
          return;
        }

        try {
          var response = JSON.parse(
            request.responseText
          );

          var rawTemperature =
              response.current.temperature_2m;

          var temperature = Math.round(
            rawTemperature
          );

          localStorage.setItem(
            STORAGE_TEMPERATURE,
            String(temperature)
          );

          localStorage.setItem(
            STORAGE_TEMPERATURE_TIME,
            String(Date.now())
          );

          sendTemperature(temperature);
        } catch (error) {
          console.log(
            "Weather response error: "
            + error
          );
        }
      };

      request.onerror = function() {
        weatherRequestRunning = false;
        console.log("Weather network error.");
      };

      request.open("GET", url);
      request.send();
    },
    function(error) {
      weatherRequestRunning = false;

      console.log(
        "Location error "
        + error.code
        + ": "
        + error.message
      );
    },
    {
      enableHighAccuracy: false,
      maximumAge: 60 * 60 * 1000,
      timeout: 15000
    }
  );
}


Pebble.addEventListener("ready", function() {
  fetchTemperature(false);
});


Pebble.addEventListener("appmessage", function(event) {
  var payload = event.payload || {};

  if (
    payload[messageKeys.WeatherRequest]
        !== undefined
  ) {
    fetchTemperature(false);
  }
});


Pebble.addEventListener("showConfiguration", function() {
  clay.setSettings({
    AnimationEffect:
        storedValue(STORAGE_EFFECT, "2"),

    AnimationSpeed:
        storedValue(STORAGE_SPEED, "1"),

    ShowDate:
        storedBoolean(STORAGE_SHOW_DATE, true),

    ShowTemperature:
        storedBoolean(
          STORAGE_SHOW_TEMPERATURE,
          true
        ),

    ShowSwissEmblem:
        storedBoolean(
          STORAGE_SHOW_SWISS_EMBLEM,
          true
        )
  });

  Pebble.openURL(clay.generateUrl());
});


Pebble.addEventListener("webviewclosed", function(event) {
  if (!event || !event.response) {
    return;
  }

  var rawSettings = clay.getSettings(
    event.response,
    false
  );

  var effect = parseInt(
    rawSettings.AnimationEffect.value,
    10
  );

  var speed = parseInt(
    rawSettings.AnimationSpeed.value,
    10
  );

  var showDate = parseBoolean(
    rawSettings.ShowDate.value,
    true
  );

  var showTemperature = parseBoolean(
    rawSettings.ShowTemperature.value,
    true
  );

  var showSwissEmblem = parseBoolean(
    rawSettings.ShowSwissEmblem.value,
    true
  );

  if (isNaN(effect) || effect < 0 || effect > 2) {
    effect = 2;
  }

  if (isNaN(speed) || speed < 0 || speed > 2) {
    speed = 1;
  }

  localStorage.setItem(
    STORAGE_EFFECT,
    String(effect)
  );

  localStorage.setItem(
    STORAGE_SPEED,
    String(speed)
  );

  localStorage.setItem(
    STORAGE_SHOW_DATE,
    String(showDate)
  );

  localStorage.setItem(
    STORAGE_SHOW_TEMPERATURE,
    String(showTemperature)
  );

  localStorage.setItem(
    STORAGE_SHOW_SWISS_EMBLEM,
    String(showSwissEmblem)
  );

  var message = {};

  message[messageKeys.AnimationEffect] = effect;
  message[messageKeys.AnimationSpeed] = speed;
  message[messageKeys.ShowDate] =
      showDate ? 1 : 0;
  message[messageKeys.ShowTemperature] =
      showTemperature ? 1 : 0;

  message[messageKeys.ShowSwissEmblem] =
      showSwissEmblem ? 1 : 0;

  Pebble.sendAppMessage(
    message,
    function() {
      console.log("Settings sent.");

      if (showTemperature) {
        fetchTemperature(true);
      }
    },
    function(error) {
      console.log(
        "Settings send failed: "
        + JSON.stringify(error)
      );
    }
  );
});

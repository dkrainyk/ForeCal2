// Convert/Reduce Yahoo weather codes to our weather icons
function iconFromWeatherId(weatherId) {
	switch (weatherId) {
		case '31': //clear (night)
		case '32': //sunny
		case '33': //fair (night)
		case '34': //fair (day)
			return 1; //Sunny
		case '29': //partly cloudy (night)
		case '30': //partly cloudy (day)
		case '44': //partly cloudy
			return 2; //Partly Cloudy
		case '26': //cloudy
		case '27': //mostly cloudy (night)
		case '28': //mostly cloudy (day)
			return 3; //Cloudy
		case '23': //blustery
		case '24': //windy
			return 4; //Windy
		case '19': //dust
		case '20': //foggy
		case '21': //haze
		case '22': //smoky
			return 5; //Low Visility
		case '4':  //thunderstorms
		case '37': //isolated thunderstorms
		case '45': //thundershowers
		case '47': //isolated thundershowers
			return 6; //Isolated Thunderstorms
		case '3':  //severe thunderstorms
		case '38': //scattered thunderstorms
		case '39': //scattered thunderstorms
			return 7; //Scattered Thunderstorms
		case '9':  //drizzle
			return 8; //Drizzle
		case '11': //showers
		case '12': //showers
		case '40': //scattered showers
			return 9; //Rain
		case '8':  //freezing drizzle
		case '10': //freezing rain
		case '17': //hail
		case '35': //mixed rain and hail
			return 10; //Hail
		case '15': //blowing snow
		case '16': //snow
		case '18': //sleet
		case '41': //heavy snow
		case '43': //heavy snow
		case '46': //snow showers
			return 11; //Snow
		case '5':  //mixed rain and snow
		case '6':  //mixed rain and sleet
		case '7':  //mixed snow and sleet
			return 12; //Mixed Snow
		case '25': //cold
			return 13; //Cold
		case '0':  //tornado
			return 14; //Tornado
		case '1':  //tropical storm
			return 15; //Storm
		case '13': //snow flurries
		case '14': //light snow showers
		case '42': //scattered snow showers
			return 16; //Light Snow
		case '36': //hot
			return 17; //Hot
		case '2':  //hurricane
			return 18; //Hurricane
		default:
			return 0; // N/A
	}
}

// Parse time string into Javascript Date object
function parseTime(timeStr) {
	var time = timeStr.match(/(\d+)(?::(\d\d))?\s*(p?)/i);
	if (!time) {
		return NaN;
	}
	var hours = parseInt(time[1], 10);
	if (hours == 12 && !time[3]) {
		hours = 0;
	}
	else {
		hours += (hours < 12 && time[3]) ? 12 : 0;
	}

	var dt = new Date();
	dt.setHours(hours);
	dt.setMinutes(parseInt(time[2], 10) || 0);
	dt.setSeconds(0, 0);
	return dt;
}

// (Very) Rudementary XML parser for getting a specified attribute value of given XML tag
// (This has to be done as there is no 'document' object in this Javascript environment)
function getXmlAttrVal(xml, tag, attrName) {
	var re = new RegExp('<' + tag + '(\\s+|\\s[^>]+\\s)' + attrName + '\\s*=\\s*"([^"]+)"[^>]*\/>', 'im');
	var parts = re.exec(xml);

	if (parts && parts.length == 3)
		return parts[2];
	else
		return '';
}

// Wrapper for XMLHttpRequest.
function getHttpResponse(url, onSuccess, onError) {
	var req = new XMLHttpRequest();
	req.open('GET', url, true);
	req.onreadystatechange = function () {
		if (req.readyState == 4) {
			if (req.status == 200)
				onSuccess(req.responseText);
			else
				onError(req.status);
		}
	};
	req.send(null);
}

// Add specified number of days to a Date
function addDays(date, days) {
	var result = new Date(date);
	result.setDate(date.getDate() + days);
	return result;
}

var savedLocation = null;
var pebbleMessage = null;

function fetchLocation(latitude, longitude, onSuccess) {
	// Get the WOEID for the current location, which will trigger the weather to be fetched
	// URL for getting our WOEID from our current Lat/Long position in JSON format
	getHttpResponse('http://query.yahooapis.com/v1/public/yql?q=select%20*%20from%20geo.placefinder%20where%20text%3D%22' +
	latitude + '%2C' + longitude + '%22%20and%20gflags%3D%22R%22&format=json', function (responseText) {
		var response = JSON.parse(responseText);
		if (response && response.query && response.query.results && response.query.results.Result &&
		response.query.results.Result.woeid && response.query.results.Result.woeid !== '') {
			// Successfully found our WOEID, so now we can trigger the fetch of the weather data
			savedLocation = response.query.results.Result;
			onSuccess(savedLocation);
		} else {
			if (savedLocation !== null)
				onSuccess(savedLocation);
			else {
				// WOEID not found
				pebbleMessage = {
					"status": "Loc. N/A",
					"city": "N/A"
				};
				Pebble.sendAppMessage(pebbleMessage);
			}
		}
	}, function (err) {
		console.log("Location Error");
		if (savedLocation !== null)
			onSuccess(savedLocation);
		else {
			pebbleMessage = {
				"status": "Err: " + err,
				"city": "N/A"
			};
			Pebble.sendAppMessage(pebbleMessage);
		}
	});
}

// Fetch the weather data from Yahoo and transmit to Pebble
function fetchWeather(latitude, longitude) {
	var country, city, woeid, unit, status;
	fetchLocation(latitude, longitude, function (location) {
		country = location.countrycode;
		city = location.city;
		woeid = location.woeid;
		console.log('Country: ' + country);
		console.log('City: ' + city);
		console.log('WOEID: ' + woeid);
		
		// Determine temperature units from country code (US gets F, everyone else gets C)
		if (country == 'US')
			unit = 'F';
		else
			unit = 'C';
		console.log('Unit: ' + unit);
		
		// Fetch the weather data
		// URL for getting basic weather forecast data in XML format (RSS)
		getHttpResponse('http://weather.yahooapis.com/forecastrss?w=' + woeid + '&u=' + unit.toLowerCase(), function(weatherResponse) {
			// Successfully retrieved weather data
			var curr_temp, curr_temp_str, sunrise, sunrise_str, sunset, sunset_str;
			var curr_time, forecast_day, forecast_date, high, low, icon, condition;
			var sun_rise_set;

			curr_time = new Date();

			// Get current temperature
			curr_temp_str = getXmlAttrVal(weatherResponse, 'yweather:condition', 'temp');

			if (curr_temp_str === '')
				curr_temp = '';
			else
				curr_temp = getXmlAttrVal(weatherResponse, 'yweather:condition', 'temp') + '\u00B0' + unit;

			sun_rise_set = '';

			// Get Sunrise and Sunset times, which also dictate if Daymode is on or not
			sunrise_str = getXmlAttrVal(weatherResponse, 'yweather:astronomy', 'sunrise');
			sunset_str = getXmlAttrVal(weatherResponse, 'yweather:astronomy', 'sunset');

			if (sunrise_str !== '' && sunset_str !== '') {
				sunrise = parseTime(sunrise_str);
				sunset = parseTime(sunset_str);
			}

			if (curr_time.getHours() >= 18) {
				// Between 6pm and Midnight, show tomorrow's forecast
				forecast_day = 'Tomorrow';
				forecast_date = addDays(new Date(), 1);
			} else {
				// At all other times, show today's forecast
				forecast_day = 'Today';
				forecast_date = new Date();
			}

			var reDate = new RegExp('\\sdate\\s*=\\s*"([^"]+)"');
			var reAttr = new RegExp('([^\\s=>"]+)\\s*=\\s*"([^"]+)"');
			var forecasts = weatherResponse.match(/<yweather:forecast[^>]+>/img);

			var fd, attrs, attr, dateAttr;

			low = ''; high = ''; condition = ''; icon = 0;

			// Parse the forecast data out of the XML
			for (var i = 0; i < forecasts.length; i++) {
				dateAttr = reDate.exec(forecasts[i]);

				if (dateAttr && dateAttr.length == 2) {
					fd = new Date(dateAttr[1]);
					// Find the forecast data for today/tomorrow
					if (fd.getDate() == forecast_date.getDate()) {
						attrs = forecasts[i].match(/[^\s=>"]+\s*=\s*"[^"]+"/img);
						for (var a = 0; a < attrs.length; a++) {
							attr = reAttr.exec(attrs[a]);
							// Get all the weather forecast attribute values
							if (attr && attr.length == 3) {
								if (attr[1].toLowerCase() == 'low' && attr[2] !== '')
									low = attr[2] + '\u00B0' + unit;
								else if (attr[1].toLowerCase() == 'high' && attr[2] !== '')
									high = attr[2] + '\u00B0' + unit;
								else if (attr[1].toLowerCase() == 'text' && attr[2] !== '')
									condition = attr[2];
								else if (attr[1].toLowerCase() == 'code' && attr[2] !== '')
									icon = iconFromWeatherId(attr[2]);
							}
						}
						break;
					}
				}
			}

			// Set the status display on the Pebble to the time of the weather update
			status = 'U:' + curr_time.getHours() + ':' +
			(curr_time.getMinutes() < 10 ? '0' : '') + curr_time.getMinutes();

			console.log('Current Temp: ' + curr_temp);
			console.log('Sunrise: ' + sunrise.getHours() + ':' + sunrise.getMinutes());
			console.log('Sunset: ' + sunset.getHours() + ':' + sunset.getMinutes());
			console.log('Forecast Day: ' + forecast_day);
			console.log('Forecast Date: ' + forecast_date);
			console.log('Low: ' + low);
			console.log('High: ' + high);
			console.log('Condition: ' + condition);
			console.log('Icon: ' + icon);

			// Send the data to the Pebble
			pebbleMessage = {
				"status": status,
				"curr_temp": curr_temp,
				"forecast_day": forecast_day,
				"high_temp": high,
				"low_temp": low,
				"icon": icon,
				"condition": condition.slice(0, 50),
				"city": city.slice(0, 50),
				"sun_rise_hour": sunrise.getHours(),
				"sun_rise_min": sunrise.getMinutes(),
				"sun_set_hour": sunset.getHours(),
				"sun_set_min": sunset.getMinutes()
			};
			Pebble.sendAppMessage(pebbleMessage);
		}, function(err) {
			console.log("Error");
			pebbleMessage = {
				"status": "Err: " + err,
				"city": city.slice(0, 50)
			};
			Pebble.sendAppMessage(pebbleMessage);
		});
	});
}

var savedLongitude = null;
var savedLatitude = null;

function locationSuccess(pos) {
	// Got our Lat/Long so now fetch the weather data
	var coordinates = pos.coords;
	savedLongitude = coordinates.longitude;
	savedLatitude = coordinates.latitude;
	
	fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
	console.warn('Location error (' + err.code + '): ' + err.message);
	if (savedLongitude !== null && savedLatitude !== null) {
			fetchWeather(savedLatitude, savedLongitude);
		}
	else {
		pebbleMessage = {
			"status": "GPS N/A",
			"city": "N/A"
		};
		Pebble.sendAppMessage(pebbleMessage);
	}
}

var locationOptions = { "timeout": 15000, "maximumAge": 600000 };

Pebble.addEventListener("ready",
function (e) {
	console.log("JS Ready");
	// Trigger location and weather fetch on load
	navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
});

Pebble.addEventListener("appmessage",
function (e) {
	console.log("Pebble App Message!");
	if (pebbleMessage !== null)
		Pebble.sendAppMessage(pebbleMessage);
	// Trigger location and weather fetch on command from Pebble
	navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
});

Pebble.addEventListener("webviewclosed",
function (e) {
	console.log("Webview closed");
});

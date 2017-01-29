#include "weather.h"
#include "http.h"
#include "term.h"
#include "util.h"
#include "jsmn.h"

struct get_mapclick_url_ctx {
  char * url;
  size_t url_size;
};

struct get_mapclick_data_ctx {
  char * data;
  size_t data_size;

  // Current index where next byte should be written
  char * data_i;
  // Last byte of writable data
  char * data_last;
  size_t data_bytes_read;
};

struct day_forecast {
  short high_temperature;
  char weather[20];
  char text[100];
};

struct weather {
  char timestamp[32];
  char area[24];

  // Current conditions
  char description[20];
  int temperature;
  int dewpoint;
  int relative_humidity;
  int wind_speed;
  int wind_direction;
  int gust;
  
  // Future 
  struct day_forecast future[13];
};

void get_mapclick_url_header_cb(struct http_request * req, const char * header, const char * value) {
  struct get_mapclick_url_ctx * ctx = (struct get_mapclick_url_ctx *) req->caller_ctx;
  
  if (strcmp(header, "Location") == 0) {
    scopy(ctx->url, value, ctx->url_size);
  }
}

boolean get_mapclick_url(const char * zip, char * mapclick_url, size_t mapclick_url_size) {  
  const char base_path_and_query[] = "/zipcity.php?inputstring=";
  
  // Append the zip to the query string; 5 extra bytes for the zip
  char path_and_query[sizeof(base_path_and_query) + 5];
  scopy(path_and_query, base_path_and_query, sizeof(base_path_and_query));
  // strlen doesn't include the terminator; copy 6 from zip to copy its terminator
  scopy(path_and_query + strlen(path_and_query), zip, 6);

  struct get_mapclick_url_ctx ctx;
  ctx.url = mapclick_url;
  ctx.url_size = mapclick_url_size;
  
  struct http_request req;
  http_request_init(&req);
  req.host = "forecast.weather.gov";
  req.path_and_query = path_and_query;
  req.header_cb = get_mapclick_url_header_cb;
  req.body_cb = NULL;
  req.caller_ctx = &ctx;

  memset(mapclick_url, '\0', mapclick_url_size);
  http_get(&req);

  if (req.status != 302) {
    term_write("HTTP error getting MapClick URL: ");
    term_println(req.status, DEC);
    return false;
  }

  if (strlen(mapclick_url) == 0) { 
    term_writeln("Got an empty MapClick URL from the redirect.");
    return false;
  }
  
  return true;
}

void get_mapclick_data_body_cb(struct http_request * req) {
  struct get_mapclick_data_ctx * ctx = (struct get_mapclick_data_ctx *) req->caller_ctx;

  // Read until the request is over or until we reach the penultimate byte
  while (req->client->connected() && ctx->data_i < ctx->data_last) {
    int c = req->client->read();
    if (c != -1) {
      *ctx->data_i++ = c;
      ctx->data_bytes_read++;
    }
  }
}

bool get_mapclick_json(const char * mapclick_url, char * mapclick_json, size_t mapclick_json_size) {
  // Parse the mapclick URL so we can add a query param and query it
  struct url_parts parts;
  if (!parse_url(&parts, mapclick_url)) {
    term_writeln("Could not parse the MapClick URL that was returned: ");
    term_writeln(mapclick_url);
    return false;
  }

  // There are already some query args, so add one more
  char json_path_and_query[256];
  scopy(json_path_and_query, parts.path_and_query, sizeof(json_path_and_query));
  scopy(json_path_and_query + strlen(json_path_and_query), "&FcstType=json", sizeof(json_path_and_query) - strlen(parts.path_and_query));

  struct get_mapclick_data_ctx ctx;
  ctx.data = mapclick_json;
  ctx.data_size = mapclick_json_size;
  ctx.data_i = ctx.data;
  ctx.data_last = ctx.data + ctx.data_size - 1;
  ctx.data_bytes_read = 0;
  
  struct http_request req;
  http_request_init(&req);
  req.host = parts.host;
  if (parts.port != 0) {
    req.port = parts.port;
  }
  req.path_and_query = json_path_and_query;
  req.header_cb = NULL;
  req.body_cb = get_mapclick_data_body_cb;
  req.caller_ctx = &ctx;

  memset(mapclick_json, '\0', mapclick_json_size);
  http_get(&req);

  if (req.status != 200) {
    term_write("HTTP error getting MapClick data: ");
    term_println(req.status, DEC);
    return false;
  }

  if (strlen(mapclick_json) == 0) { 
    term_writeln("Got no MapClick data.");
    return false;
  }

  return true;
}


char * scopy_json(char * dest, size_t dest_size, const char * json, jsmntok_t * src) {
  // Copy the lesser of the length of the string plus one for the term, or the dest size
  size_t max_chars_to_copy = min(dest_size, (src->end - src->start) +1);
  return scopy(dest, json + src->start, max_chars_to_copy);
}

int atoi_json(const char * json, jsmntok_t * tok) {
  char num_buf[16];
  scopy_json(num_buf, sizeof(num_buf), json, tok);
  return atoi(num_buf);
}

bool parse_mapclick_json(const char * mapclick_json, struct weather * weather) {
  const int tokens_size = 500;
  jsmntok_t tokens[tokens_size];
  jsmn_parser parser;
  
  jsmn_init(&parser);
  int num_tokens = jsmn_parse(&parser, mapclick_json, strlen(mapclick_json), tokens, tokens_size);
  if (num_tokens < 0) {
    term_writeln("Failed to parse the MapClick JSON");
    return false;
  }

  if (num_tokens < 1 || tokens[0].type != JSMN_OBJECT) {
    term_writeln("Top level MapClick item was not an object.");
    return false;
  }

  char num_buf[8];
  
  int timestamp_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "creationDateLocal");
  if (timestamp_i == -1) {
    term_writeln("JSON missing creationDateLocal");
    return false;
  }
  scopy_json(weather->timestamp, sizeof(weather->timestamp), mapclick_json, &tokens[timestamp_i]);

  int location_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "location");
  if (location_i == -1) {
    term_writeln("JSON missing location");
    return false;
  }

  int area_i = find_json_prop(mapclick_json, tokens, num_tokens, location_i, "areaDescription");
  if (area_i == -1) {
    term_writeln("JSON missing location.areaDescription");
    return false;
  }
  scopy_json(weather->area, sizeof(weather->area), mapclick_json, &tokens[area_i]);

  int data_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "data");
  if (data_i == -1) {
    term_writeln("JSON missing data");
    return false;
  }

  int current_observation_i = find_json_prop(mapclick_json, tokens, num_tokens, 0, "currentobservation");
  if (current_observation_i == -1) {
    term_writeln("JSON missing currentobservation");
    return false;
  }

  int description_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Weather");
  if (description_i == -1) {
    term_writeln("JSON missing data.currentobservation.Weather");
    return false;
  }
  scopy_json(weather->description, sizeof(weather->description), mapclick_json, &tokens[description_i]);

  int temp_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Temp");
  if (temp_i == -1) {
    term_writeln("JSON missing data.currentobservation.Temp");
    return false;
  }
  weather->temperature = atoi_json(mapclick_json, &tokens[temp_i]);

  int dewpoint_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Dewp");
  if (dewpoint_i == -1) {
    term_writeln("JSON missing data.currentobservation.Dewp");
    return false;
  }
  weather->dewpoint = atoi_json(mapclick_json, &tokens[dewpoint_i]);

  int relative_humidity_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Relh");
  if (relative_humidity_i == -1) {
    term_writeln("JSON missing data.currentobservation.Relh");
    return false;
  }
  weather->relative_humidity = atoi_json(mapclick_json, &tokens[relative_humidity_i]);
  
  int wind_speed_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Winds");
  if (wind_speed_i == -1) {
    term_writeln("JSON missing data.currentobservation.Winds");
    return false;
  }
  weather->wind_speed = atoi_json(mapclick_json, &tokens[wind_speed_i]);

  int wind_direction_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Windd");
  if (wind_direction_i == -1) {
    term_writeln("JSON missing data.currentobservation.Windd");
    return false;
  }
  weather->wind_direction = atoi_json(mapclick_json, &tokens[wind_direction_i]);

  int gust_i = find_json_prop(mapclick_json, tokens, num_tokens, current_observation_i, "Gust");
  if (gust_i == -1) {
    term_writeln("JSON missing data.currentobservation.Gust");
    return false;
  }
  weather->gust = atoi_json(mapclick_json, &tokens[gust_i]);

  return true;
}

const char * wind_direction(int angle) {
  const char* dirs[]   = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "N" };
  const short angles[] = { 0,   45,   90,  135,  180, 225,  270, 315,  360 };
  
  short min_diff;
  const char * dir_for_min_diff;
  
  // Quick and dirty "find closest direction"
  for (short i = 0; i < sizeof(dirs); i++) {
    short diff = abs(angle - angles[i]);
    if (diff < min_diff) {
      min_diff = diff;
      dir_for_min_diff = dirs[i];
    }
  }

  return dir_for_min_diff;
}

void print_weather(struct weather * weather) {
  term_write(weather->area);
  term_write(" (");
  term_write(weather->timestamp);
  term_writeln(")");

  term_write(" Weather:           ");
  term_writeln(weather->description);

  term_write(" Temperature:       ");
  term_print(weather->temperature, DEC);
  term_writeln(" F");

  term_write(" Relative Humidity: ");
  term_print(weather->relative_humidity, DEC);
  term_writeln(" %");

  term_write(" Dewpoint:          ");
  term_print(weather->dewpoint, DEC);
  term_writeln(" F");

  term_write(" Wind:              ");
  term_print(weather->wind_speed, DEC);
  term_write(" mph (gusts ");
  term_print(weather->gust, DEC);
  term_write(" mph) from the ");
  term_writeln(wind_direction(weather->wind_direction));
}

void weather(const char * zip) {
  char mapclick_url[200];
  if (!get_mapclick_url(zip, mapclick_url, sizeof(mapclick_url))) {
    term_writeln("Could not resolve city and state to a location.");
    term_writeln("Was that a valid ZIP code?");
    return;
  }

  char mapclick_json[6000];
  if (!get_mapclick_json(mapclick_url, mapclick_json, sizeof(mapclick_json))) {
    term_writeln("Could not read the forecast data.  This might be a temporary problem.");
    return;
  }

  struct weather weather;
  if (!parse_mapclick_json(mapclick_json, &weather)) {
    term_writeln("Could not parse the forecast JSON.");
    return;
  }

  print_weather(&weather);
}


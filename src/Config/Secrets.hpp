#pragma once

// Copy LocalSecrets.example.hpp to LocalSecrets.hpp and fill it locally.
// LocalSecrets.hpp is intentionally ignored by Git.
#if __has_include("Config/LocalSecrets.hpp")
#include "Config/LocalSecrets.hpp"
#endif

#ifndef OPENWEATHER_API_KEY
#define OPENWEATHER_API_KEY ""
#endif

#ifndef NIXIE_HTTP_USERNAME
#define NIXIE_HTTP_USERNAME ""
#endif

#ifndef NIXIE_HTTP_PASSWORD
#define NIXIE_HTTP_PASSWORD ""
#endif

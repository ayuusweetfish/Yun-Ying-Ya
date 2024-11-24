#pragma once

// User-provided
const char *net_tsinghua_request(const char *url, const char *cookies);

// Implemented
int net_tsinghua_perform_login(const char *user, const char *pwd);

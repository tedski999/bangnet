#ifndef BNET_NTP_HPP
#define BNET_NTP_HPP

#include <cstdint>

#define MSECS_TO_SEC (1000ll)
#define USECS_TO_SEC (MSECS_TO_SEC * 1000ll)

void bnet_ntp_start();
int64_t bnet_ntp_time_usec();

#endif // BNET_NTP_HPP

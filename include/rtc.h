#pragma once
struct rtc_time { int sec, min, hour, day, month, year; };
void rtc_read(struct rtc_time* t);

/**
 * Copyright (c) 2016 harlequin
 * https://github.com/harlequin/samygo-plugin-dvbapi
 *
 * This file is part of samygo-plugin-dvbapi.
 *
 * samygo-plugin-dvbapi is free software: you can redistribute it and/or modify
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LOG_H_
#define LOG_H_

#define LIB_NAME "dvbapi"
#define LIB_VERSION "v0.1"
#define LIB_TV_MODELS "H (T-MST)"
#define INFO_VERSION "samygo-plugin-dvbapi " LIB_VERSION " / MODEL " LIB_TV_MODELS
#define LOG_FILE "/dtv/"LIB_NAME".log"
void LOG(const char *fmt, ...);
#define log(...) LOG("["LIB_NAME"] "__VA_ARGS__);
#define logh(fmt,...) LOG("["LIB_NAME"] %s, "fmt,__func__+2,__VA_ARGS__)
#define logf(fmt,...) LOG("["LIB_NAME"] %s, "fmt,__func__,__VA_ARGS__)

#endif /* LOG_H_ */

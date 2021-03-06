/*
 *  tvheadend, CWC interface
 *  Copyright (C) 2007 Andreas �man
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CWC_H_
#define CWC_H_

struct mpegts_mux;

void cwc_init(void);

void cwc_done(void);

void cwc_service_start(struct service *t);

void cwc_caid_update(struct mpegts_mux *mux,
                     uint16_t caid, uint16_t pid, int valid);

#endif /* CWC_H_ */

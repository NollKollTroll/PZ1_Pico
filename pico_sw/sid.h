/*
 *  sid.h - 6581 SID emulation
 *
 *  SIDPlayer (C) Copyright 1996-2004 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef SID_H
#define SID_H

extern "C" {
  // Reset SID emulation
  void sid_reset();
  
  //return next sample from SID
  int16_t sid_calc();

  //SID-register access
  uint8_t sid_read(uint8_t addr);
  void sid_write(uint8_t addr, uint8_t data);  
}

#endif //SID_H

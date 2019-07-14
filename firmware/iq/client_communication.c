/*
  Copyright 2019 IQinetics Technologies, Inc support@iq-control.com

  This file is part of the IQ C++ API.

  IQ C++ API is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  IQ C++ API is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*
  Name: client_communication.cpp
  Last update: 4/12/2019 by Matthew Piccoli
  Author: Matthew Piccoli
  Contributors: Raphael Van Hoffelen
*/

#include <stdint.h>
#include "multi_turn_angle_control_client.h"
#include "generic_interface.h"

extern mta_storage entry_array[];

// lookup entry_length based on the mta->type field
int8_t ParseMsg(struct mta_object *mta, uint8_t* rx_data, uint8_t rx_length)
{
  uint8_t type_idn = rx_data[0];
  uint8_t sub_idn = rx_data[1];
  uint8_t obj_idn = rx_data[2] >> 2; // high 6 bits are obj_idn
  enum Access dir = (enum Access) (rx_data[2] & 0b00000011); // low two bits

  // if we have a reply (we only parse replies here)
  if(dir == kReply)
  {
    // if sub_idn is within array range (safe to access array at this location)
    if(sub_idn < kEntryLength)
      {
	    // if the type and obj identifiers match
	    if(entry_array[sub_idn].command == type_idn &&
		 mta->obj_idn == obj_idn)
		{
		  // ... then we have a valid message
		  mta_Reply(mta, &rx_data[3], rx_length-3, sub_idn);
		  return 1; // I parsed something
		}
	    }
    }
  return 0; // I didn't parse anything
}


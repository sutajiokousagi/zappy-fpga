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
  Name: generic_interface.hpp
  Last update: 3/7/2019 by Raphael Van Hoffelen
  Author: Matthew Piccoli
  Contributors: Raphael Van Hoffelen
*/

#ifndef GENERIC_INTERFACE_H
#define GENERIC_INTERFACE_H

#include "packet_finder.h"
#include "byte_queue.h"
#include "bipbuffer.h"
#include "multi_turn_angle_control_client.h"

#define GENERIC_PF_INDEX_DATA_SIZE 20   // size of index buffer in packet_finder

#ifndef GENERIC_TX_BUFFER_SIZE
  #define GENERIC_TX_BUFFER_SIZE 64
#endif

// Member Variables
typedef struct CommInterface_storage {
  struct PacketFinder pf;        // packet_finder instance
  struct ByteQueue index_queue;              // needed by pf for storing indices
  uint8_t pf_index_data[GENERIC_PF_INDEX_DATA_SIZE]; // data for index_queue used by pf
  struct BipBuffer tx_bipbuf;   // bipbuffer for transmissions 
  uint8_t tx_buffer[GENERIC_TX_BUFFER_SIZE];   // raw buffer for transmissions
} CommInterface_storage;

#include "communication_interface.h"

    /// Gets all outbound bytes 
    /// The data is copied into the user supplied data_out buffer.
    /// The length of data transferred is copied into length_out.
    /// Returns: 1 for data transferred
    ///          0 for no data transferred (buffer empty)
int8_t CommInterface_SetRxBytes(struct CommInterface_storage *self, uint8_t* data_in, uint16_t length_in);
int8_t CommInterface_GetTxBytes(struct CommInterface_storage *self, uint8_t* data_out, uint8_t *length_out);
void CommInterface_init(struct CommInterface_storage *iq_com);
int8_t ParseMsg(struct mta_object *mta, uint8_t* rx_data, uint8_t rx_length);
    
#endif // GENERIC_INTERFACE_H

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
  Name: communication_interface.hpp
  Last update: 3/7/2019 by Raphael Van Hoffelen
  Author: Matthew Piccoli
  Contributors: Raphael Van Hoffelen
*/

#ifndef COMMUNICATION_INTERFACE_H
#define	COMMUNICATION_INTERFACE_H

#include <stdint.h>

    /*******************************************************************************
     * Receive
     ******************************************************************************/

    /// Poll the hardware for new byte data.
    ///   Returns: 1 packet ready
    ///            0 normal operation
    ///           -1 failure
    ///
int8_t CommInterface_GetBytes(struct CommInterface_storage *self);

    /// Peek at the next available incoming packet. If a packet is ready, pointer 
    /// 'packet' will point to the first byte of type+data and 'length' will give 
    /// the length of packet type+data. Arguments 'packet' and 'length' are ignored 
    /// if no packet is ready.  Repeated calls to Peek will return pointers to the 
    /// same packet data until Drop is used.
    ///   Returns: 1 packet peek available
    ///            0 no packet peek available
    ///           -1 failure
    ///
int8_t CommInterface_PeekPacket(struct CommInterface_storage *self, uint8_t **packet, uint8_t *length);

    /// Drop the next available packet from queue. Usually called after Peek.
    ///   Returns: 1 packet removed
    ///            0 no packet ready to remove
    ///           -1 failure
    ///
int8_t CommInterface_DropPacket(struct CommInterface_storage *self);


    /*******************************************************************************
     * Send
     ******************************************************************************/

    /// Add a packet to the outgoing USB queue with automatically generated header 
    /// and CRC. A hardware transmission is not immediately initiated unless the 
    /// endpoint is filled. To force a transmission, follow with SendNow(). This 
    /// operation is nonblocking. If the buffer fills, the most recent data is lost.
int8_t CommInterface_SendPacket(struct CommInterface_storage *self, uint8_t msg_type, uint8_t *data, uint16_t length);

    /// Add bytes to the outgoing USB queue. A hardware transmission is not 
    /// immediately initiated unless the endpoint is filled. To force a 
    /// transmission, follow with SendUsbNow(). This operation is 
    /// nonblocking. If the buffer fills, the most recent data is lost.
int8_t CommInterface_SendBytes(struct CommInterface_storage *self, uint8_t *bytes, uint16_t length);

    /// Initiate a hardware transmission, which will chain transmissions through 
    /// the endpoint IN interrupt until the buffer empties completely.
void CommInterface_SendNow(struct CommInterface_storage *self);
    
    /*******************************************************************************
     * Parsing
     ******************************************************************************/
     
    /// Read a given message and act appropriately.
void CommInterface_ReadMsg(struct CommInterface_storage *com, uint8_t* data, uint8_t length);
#endif // COMMUNICATION_INTERFACE_H

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
  Name: generic_interface.cpp
  Last update: 3/7/2019 by Raphael Van Hoffelen
  Author: Matthew Piccoli
  Contributors: Raphael Van Hoffelen
*/

#include "generic_interface.h"
#include "crc_helper.h"
#include "string.h" // for memcpy
#include "bipbuffer.h"

struct BipBuffer tx_bipbuf;

void CommInterface_init(struct CommInterface_storage *self)
{
  InitBQ(&self->index_queue, self->pf_index_data, GENERIC_PF_INDEX_DATA_SIZE);
  InitPacketFinder(&self->pf, &self->index_queue);
  BipBuffer_init(&tx_bipbuf, self->tx_buffer, GENERIC_TX_BUFFER_SIZE); // if we need more than one of these, malloc the tx_bifbuf for now now static to keep it easy
}

int8_t CommInterface_GetBytes(struct CommInterface_storage *self)
{
  // I can't do anything on my own since I don't have hardware
  // Use SetRxBytes(uint8_t* data_in, uint16_t length_in)
  return 0;
}

int8_t CommInterface_SetRxBytes(struct CommInterface_storage *self, uint8_t* data_in, uint16_t length_in)
{
  if(data_in == NULL)
    return -1;
  
  if(length_in)
  {
    //copy it over
    PutBytes(&self->pf, data_in, length_in); 
    return 1;
  }
  else
    return 0;
}

int8_t CommInterface_PeekPacket(struct CommInterface_storage *self, uint8_t **packet, uint8_t *length)
{
  return(PeekPacket(&self->pf, packet, length));
}

int8_t CommInterface_DropPacket(struct CommInterface_storage *self)
{
  return(DropPacket(&self->pf));
}

int8_t CommInterface_SendPacket(struct CommInterface_storage *self, uint8_t msg_type, uint8_t *data, uint16_t length)
{
  // This function must not be interrupted by another call to SendBytes or 
  // SendPacket, or else the packet it builds will be spliced/corrupted.

  uint8_t header[3];
  header[0] = kStartByte;                   // const defined by packet_finder.c
  header[1] = length;
  header[2] = msg_type;
  CommInterface_SendBytes(self, header, 3);
  
  CommInterface_SendBytes(self, data, length);
  
  uint8_t footer[2];
  uint16_t crc;
  crc = MakeCrc(&(header[1]), 2);
  crc = ArrayUpdateCrc(crc, data, length);
  footer[0] = crc & 0x00FF;
  footer[1] = crc >> 8;
  CommInterface_SendBytes(self, footer, 2);
  
  return(1);
}

int8_t CommInterface_SendBytes(struct CommInterface_storage *self, uint8_t *bytes, uint16_t length)
{
  uint16_t length_temp = 0;
  uint8_t* location_temp;
  int8_t ret = 0;
    
  // Reserve space in the buffer
  location_temp = self->tx_bipbuf.Reserve(&self->tx_bipbuf, length, &length_temp);
  
  // If there's room, do the copy
  if(length == length_temp)
  {
    memcpy(location_temp, bytes, length_temp);   // do copy
    self->tx_bipbuf.Commit(&self->tx_bipbuf, length_temp);
    ret = 1;
  }
  else
  {
    self->tx_bipbuf.Commit(&self->tx_bipbuf, 0); // Call the restaurant, cancel the reservations
  }
    
  return ret;
}

int8_t CommInterface_GetTxBytes(struct CommInterface_storage *self, uint8_t* data_out, uint8_t *length_out)
{
  uint16_t length_temp;
  uint8_t* location_temp;
  
  location_temp = self->tx_bipbuf.GetContiguousBlock(&self->tx_bipbuf, length_temp);
  if(length_temp)
  {
    memcpy(data_out, location_temp, length_temp);
    *length_out = length_temp;
    self->tx_bipbuf.DecommitBlock(&self->tx_bipbuf, length_temp);
    
    location_temp = self->tx_bipbuf.GetContiguousBlock(&self->tx_bipbuf, length_temp);
    memcpy(&data_out[*length_out], location_temp, length_temp);
    *length_out = *length_out + length_temp;
    self->tx_bipbuf.DecommitBlock(&self->tx_bipbuf, length_temp);
    return 1;
  }
  return 0;
}

void CommInterface_SendNow(struct CommInterface_storage *self)
{
  // I'm useless.
}

void CommInterface_ReadMsg(struct CommInterface_storage *com, uint8_t* data, uint8_t length)
{
  // I currently don't support being talked to
  (void)com;
  (void)data;
  (void)length;
}

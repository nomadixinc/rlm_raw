/*
 *   This program is is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * $Id:$
 * @file rlm_raw.c
 * @brief Return a value of attribute from raw packet
 *
 * @copyright 2000,2006  The FreeRADIUS server project
 * @copyright 2017  Neutron Soutmun <neo.neutron@gmail.com>
 */

RCSID("$Id:$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>

typedef struct rlm_raw_t RAW_INST;

typedef struct rlm_raw_t {
  char const *xlat_name;
} rlm_raw_t;

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};

char *base64_encode(RADIUS_PACKET *data,
                    size_t input_length,
                    size_t *output_length)
{

  *output_length = 4 * ((input_length + 2) / 3);

  char *encoded_data = malloc(*output_length);
  if (encoded_data == NULL)
    return NULL;

  for (int i = 0, j = 0; i < input_length;)
  {

    uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
    uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
    uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
  }

  for (int i = 0; i < mod_table[input_length % 3]; i++)
    encoded_data[*output_length - 1 - i] = '=';

  return encoded_data;
}

static void copy_packet_free(RADIUS_PACKET **packet) {
  free((*packet)->data);
  (*packet)->data = NULL;
  rad_free(packet);
}

static RADIUS_PACKET * copy_packet(RADIUS_PACKET *in) {
  RADIUS_PACKET *out = NULL;

  out = rad_alloc(NULL, false);
  if (!out)
    return NULL;

  memcpy(out, in, sizeof(*out));
  out->vps = NULL;

  out->data = rad_malloc(out->data_len * sizeof(uint8_t));
  if (!out->data) {
    copy_packet_free(&out);
    return NULL;
  }

  memcpy(out->data, in->data, out->data_len);

  return out;
}

static ssize_t raw_xlat(void *instance, REQUEST *request, char const *attr,
  char *out, size_t freespace)
{
  RADIUS_PACKET *dup_packet = NULL;
  DICT_ATTR const *da = NULL;
  VALUE_PAIR *vp = NULL;
  int decode_result = 0;

  if (strcmp(attr, "pkt") != 0) {
    da = dict_attrbyname(attr);
    if (!da)
      return 0;
  }

  dup_packet = copy_packet(request->packet);
  if (!dup_packet)
    return 0;

  if (strcmp(attr, "pkt") == 0)
    strncpy(out, base64_encode(dup_packet), freespace);
    return 0;
  }

  decode_result = rad_decode(dup_packet, NULL, "");
  if (decode_result == 0 && dup_packet->vps) {
    vp = fr_pair_find_by_da(dup_packet->vps, da, TAG_ANY);
    if (vp) {
      strncpy(out, vp->vp_strvalue, freespace);
      RDEBUG2("rlm_raw: Found, %s = %s", attr, vp->vp_strvalue);
    } else {
      RDEBUG2("rlm_raw: Not found, %s", attr);
    }
  } else {
    RDEBUG2("rlm_raw: Could not decode packet or no VPS data");
  }

  copy_packet_free(&dup_packet);

  return 0;
}

static int mod_bootstrap(CONF_SECTION *conf, void *instance)
{
  RAW_INST *inst = instance;

  inst->xlat_name = cf_section_name2(conf);
  if (!inst->xlat_name) {
    inst->xlat_name = cf_section_name1(conf);
  }

  xlat_register(inst->xlat_name, raw_xlat, NULL, inst);

  return 0;
}

extern module_t rlm_raw;
module_t rlm_raw = {
  .magic     = RLM_MODULE_INIT,
  .name      = "raw",
  .type      = RLM_TYPE_THREAD_SAFE,
  .inst_size = sizeof(RAW_INST),
  .bootstrap = mod_bootstrap
};

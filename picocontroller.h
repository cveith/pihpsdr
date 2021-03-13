/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#define MAX_PICOENCODERS 5
#define MAX_PICOSWITCHES 21

typedef struct _picocontroller{
  gchar *controller_device;
  gchar *baudrate;
} PICOCONTROLLER;

typedef struct _picoencoder {
  gboolean bottom_encoder_enabled;
  gint bottom_encoder_address;
  gint bottom_encoder_function;

  gint top_encoder_enabled;
  gint top_encoder_address;
  gint top_encoder_function;

  gboolean switch_enabled;

  gint switch_address;
  gint switch_function;
} PICOENCODER;

typedef struct _picoswitch {
  gboolean switch_enabled;
  gint switch_address;
  gint switch_function;
} PICOSWITCH;

extern PICOCONTROLLER picocontroller;
extern PICOENCODER picoencoders[MAX_PICOENCODERS]; 
extern PICOSWITCH picoswitches[MAX_PICOSWITCHES];

extern void pico_configure(GtkWidget *notebook, GtkWidget *grid);
extern void pico_configure_encoders();
extern void pico_configure_switches();


extern void pico_set_defaults(int ctrlr);
extern void pico_restore_actions();
extern void pico_restore_state();
extern void pico_save_state();
extern void pico_save_actions();
extern int pico_init();
extern void pico_close();
